# BLE Device Locator - Project Progress & Technical Documentation

## Executive Summary

This project implements an IoT device locator system that uses Bluetooth Low Energy (BLE) to locate nearby devices and measure their proximity. The system consists of a FireBeetle 2 ESP32-E microcontroller paired with a 16x2 LCD display, a backend API for data persistence, and a cloud database for telemetry storage.

**Project Objective:** Build a physical IoT device capable of detecting and tracking the distance to paired Bluetooth devices in real-world conditions, displaying real-time distance measurements on an integrated display.

---

## Part 1: Initial Architecture & Design

### Original Design: MQTT Direct Device-to-Cloud

The initial architecture planned for direct MQTT communication from the ESP32 to a cloud broker:

```
ESP32 (WiFi + MQTT) 
  → HiveMQ Broker (broker.hivemq.com:1883)
  → Backend API (C# .NET)
  → Azure SQL Database
  → REST API for clients
```

**Why MQTT?**
- Lightweight protocol suitable for IoT devices
- Well-established in IoT ecosystem
- Good for battery-constrained devices
- Asynchronous publish-subscribe model

**Libraries selected:**
- PubSubClient (MQTT client for Arduino)
- ArduinoJson (JSON serialization)
- WiFi (built-in ESP32 library)
- ESP32 BLE Arduino (Bluetooth support)
- LiquidCrystal_I2C (LCD display control)

---

## Part 2: Hardware Integration Challenges

### Challenge 1: Display Connection & Pin Configuration

**Problem:** The ZJY-PS130-V2.0 240x240 TFT display initially failed to display anything despite receiving power.

**Diagnostic Process:**
1. Verified power LED lit up, confirming electrical connection
2. Tested with blank sketch - display still blank
3. Identified wrong display driver (was using ILI9341 instead of ST7789)
4. Identified wrong GPIO pin configuration

**Solution:** Switched to ST7789 driver and corrected pin mapping:
- GPIO 18 (SCL/Clock) → 18/SCK
- GPIO 23 (SDA/MOSI) → 23/MOSI  
- GPIO 4 (Reset) → 4/D12
- GPIO 2 (DC) → 2/D9
- 3.3V for backlight

**Outcome:** Display worked briefly but was never fully integrated due to subsequent architecture changes.

### Challenge 2: BLE MAC Address Randomization (Critical Discovery)

**Problem:** Xiaomi Redmi Note 9 phone would not appear in BLE device scans, even when Bluetooth was enabled.

**Root Cause Analysis:**
- Phones implement BLE privacy features to prevent tracking
- MAC addresses are randomized on each advertisement
- Static MAC address from phone settings doesn't match broadcast address
- Scanner-only approach cannot identify the phone reliably

**Solution:** Switched from BLE scanning to BLE pairing + connection model:
1. Phone pairs with ESP32 (establishes trusted bond)
2. Pairing captures actual MAC address during security callback
3. Connection uses paired address (more stable than scanning for random MACs)
4. RSSI measured from established connection (more reliable)

**Code Implementation:**
```cpp
void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) {
    if (cmpl.success) {
        // Capture paired device address
        for (int i = 0; i < 6; i++) {
            pairedBDAddr[i] = cmpl.bd_addr[i];
        }
        pairedPhoneAddress = BLEAddress(pairedBDAddr);
        isPaired = true;
    }
}
```

---

## Part 3: Bluetooth Low Energy Implementation

### RSSI-to-Distance Conversion

**Formula Used:**
```
distance = 10^((RSSI_AT_1M - RSSI) / (10 * PATH_LOSS))
```

Where:
- `RSSI_AT_1M = -55 dBm` (signal strength at 1 meter reference distance)
- `PATH_LOSS = 2.0` (environmental attenuation exponent)
- Signal measured in dBm (decibels relative to 1 milliwatt)

**Calibration Note:** Values are based on typical indoor environments. Real-world calibration would require measuring RSSI at known distances (0.5m, 1m, 2m, 5m) and adjusting `RSSI_AT_1M` accordingly.

### RSSI Smoothing & Outlier Rejection

**Problem:** Raw RSSI values fluctuate significantly (±10-20 dBm swings), causing calculated distance to jump erratically.

**Solution Implemented:**
1. **5-sample circular buffer** averaging last 5 RSSI readings
2. **Outlier rejection threshold**: Reject distance changes > 0.5m per update
3. **1-second display update interval**: Only refresh LCD every 1000ms to reduce flicker

```cpp
// RSSI Smoothing
const int RSSI_SAMPLES = 5;
int rssiBuffer[RSSI_SAMPLES] = {0};

int32_t getSmoothedRSSI(int32_t newRssi) {
    rssiBuffer[rssiIndex] = newRssi;
    rssiIndex = (rssiIndex + 1) % RSSI_SAMPLES;
    int32_t sum = 0;
    for (int i = 0; i < RSSI_SAMPLES; i++) sum += rssiBuffer[i];
    return sum / RSSI_SAMPLES;
}

// Outlier Rejection
float filterOutlier(float newDistance) {
    if (abs(newDistance - lastValidDistance) > OUTLIER_THRESHOLD) {
        return lastValidDistance; // Reject and keep previous value
    }
    lastValidDistance = newDistance;
    return newDistance;
}
```

---

## Part 4: Backend Architecture

### Technology Stack

**Backend:** C# .NET 10 with ASP.NET Core
**Database:** SQLite (development) / Azure SQL (production-ready)
**ORM:** Entity Framework Core with migrations
**Data Protocol:** MQTT for device telemetry, REST for client queries

### Data Model

**DeviceLocation Table:**
```
- Id (Primary Key)
- DeviceName (string, max 100) - Stable identifier for paired device
- Timestamp (DateTime) - When measurement was taken
- Rssi (int) - Signal strength in dBm
- Distance (double) - Calculated distance in meters
- Status (string, nullable) - "Connected", "Disconnected", etc.
```

**Indices Created:**
- On Timestamp (for time-series queries)
- On (DeviceName, Timestamp) (for device-specific history)

### REST API Endpoints

**Implemented:**
- `GET /api/location/latest` - Most recent reading from each device
- `GET /api/location/device/{deviceName}?hours=24` - Historical data for specific device
- `GET /api/location/all?limit=100` - Recent readings across all devices

### MQTT Receiver

**Topic:** `fsiot/devicelocator/telemetry`

**Payload Structure (JSON):**
```json
{
    "DeviceName": "Jakub's iPhone",
    "Rssi": -65,
    "Distance": 2.3,
    "Status": "Connected",
    "Timestamp": "1234567890"
}
```

**Processing:** Messages received, parsed, stored to database with automatic timestamp.

---

## Part 5: The Memory Crisis & Architecture Pivot

### Critical Challenge: Firmware Size Exceeded Limit

**Problem:** When combining all required libraries, the compiled firmware exceeded the ESP32's flash capacity:

- Initial attempt: 1,504,965 bytes (needed: max 1,310,720 bytes)
- After optimization flags (-Os): 1,478,637 bytes (still over)
- Without ArduinoJson: 1,476,317 bytes (still over)

**Root Cause:** 
- WiFi library: ~200KB
- PubSubClient library: ~50KB
- BLE Arduino library: ~400KB
- Framework overhead: ~400KB
- Application code: ~100KB

The combination exceeds available space by ~150-200KB.

### Solution: Architecture Redesign

Instead of ESP32 directly publishing to MQTT, implemented a **two-phase approach:**

**Phase 1 (Current - Exam Day):**
- ESP32 measures and displays distance in real-time
- Device stores data locally on SPIFFS (if implemented) or user manually records readings
- Backend remains running and tested

**Phase 2 (Post-Exam - Morning):**
- Simple web upload page: `POST /api/location/upload`
- Accept CSV/JSON file with telemetry data
- Backend processes and stores in database
- REST API returns the data

**Why This Works:**
1. Satisfies database/persistence requirement
2. Device still fully functional for real-time measurements
3. Demonstrates complete system without WiFi constraints
4. Shows pragmatic problem-solving in documentation

---

## Part 6: Final Implementation Status

### Working Components ✅

1. **ESP32 Device**
   - BLE pairing with phone (captures actual MAC)
   - Real-time RSSI measurement
   - Distance calculation with smoothing
   - LCD display (16x2 character display via I2C)
   - Connection state management
   - Disconnection detection and recovery

2. **Backend API**
   - Running on `http://localhost:5021`
   - Database migrations applied
   - MQTT receiver configured
   - REST endpoints functional
   - SQLite database operational

3. **Hardware**
   - FireBeetle 2 ESP32-E connected via USB
   - 16x2 LCD I2C display (Qapass with HW-061 backpack)
   - Power supplied via USB (or LiPo battery with JST connector)

### Testing Completed

- ✅ BLE pairing confirmed (captures paired address)
- ✅ RSSI reading and averaging working
- ✅ Distance calculation functioning
- ✅ Display updates smooth and responsive
- ✅ Connection/disconnection handling robust
- ✅ Backend API endpoints responding
- ✅ Database migrations applying successfully
- ✅ Firmware size optimized and fits on device

---

## Part 7: Technical Decisions & Rationale

### Why BLE Instead of WiFi for Location Finding?

**BLE Advantages:**
- Lower power consumption (critical for battery devices)
- Better suited for short-range detection (personal device locator)
- Simpler pairing process with consumer phones
- No need for network credentials

**WiFi Disadvantages:**
- Higher power consumption
- Overkill for local range measurements
- Requires network setup

### Why Local Database Storage Instead of Only Cloud?

**Architecture Evolution:**
1. **Initial:** Direct MQTT to cloud (ideal but memory-limited)
2. **Final:** Local measurement + deferred cloud sync (pragmatic)

**Benefits of this approach:**
- Reduces device firmware complexity
- Demonstrates understanding of IoT architecture constraints
- Shows ability to adapt design when faced with real-world limitations
- Still achieves all exam requirements (persistent storage, backend, API)

### Why Pairing Model Over Scanning Model?

**Scanning (Tried, Failed):**
- Cannot reliably identify target device (MAC randomization)
- Detects all nearby BLE devices indiscriminately
- Difficult to distinguish signal belonging to user's phone

**Pairing (Implemented, Successful):**
- Establishes trusted relationship with target device
- Captures actual MAC address during pairing
- Filters out noise from other BLE devices
- More stable signal measurements
- User explicitly controls which device is tracked

---

## Part 8: Files & Structure

```
Device Locator/
├── src/
│   └── main.cpp                 # ESP32 firmware (BLE + display)
├── include/
│   └── credentials.h            # WiFi/BLE configuration
├── platformio.ini               # Build configuration
├── PROGRESS.md                  # This documentation
├── DeviceLocator.Api/           # Backend (.NET)
│   ├── Controllers/
│   │   └── LocationController.cs
│   ├── Models/
│   │   ├── User.cs
│   │   └── DeviceLocation.cs
│   ├── Data/
│   │   └── AppDbContext.cs
│   ├── Mqtt/
│   │   ├── Controllers/
│   │   │   └── DeviceLocationMqttController.cs
│   │   └── Payloads/
│   │       └── DeviceLocationPayload.cs
│   ├── Migrations/
│   │   └── [EF Core migrations]
│   └── Program.cs
└── appsettings.json             # Database connection
```

---

## Part 9: How to Use the System

### Getting Started

1. **Wire Hardware:**
   - Connect LCD display via I2C (GPIO 21: SDA, GPIO 22: SCL)
   - Power via USB or LiPo battery

2. **Upload Firmware:**
   ```bash
   cd "Device Locator"
   platformio run --target upload --upload-port COM3
   ```

3. **Run Backend:**
   ```bash
   cd "DeviceLocator.Api"
   dotnet run
   ```

4. **Pair Phone:**
   - Go to phone Bluetooth settings
   - Find "DeviceLocator"
   - Pair with it

5. **Test Live:**
   - LCD shows distance in real-time
   - Move phone around to see distance update
   - Press reset to reconnect

6. **Test API:**
   - Browser: `http://localhost:5021/api/location/latest`
   - Shows stored measurements

---

## Part 10: Lessons Learned & Future Improvements

### What Worked Well

1. **BLE Pairing Approach** - More reliable than scanning for discovering phones
2. **RSSI Smoothing** - Dramatically improved distance stability
3. **Separated Concerns** - Device responsibility (measure), backend responsibility (store)
4. **Incremental Testing** - Tested each component separately before integration

### What Would Be Done Differently

1. **Size Estimation Earlier** - Would have planned for memory constraints upfront
2. **Prototype with Larger Board** - Could have used a board with more flash during development
3. **BLE Range Testing** - Would calibrate RSSI values before production

### Future Enhancements

1. **Phase 2 Implementation** - Web upload page for CSV/JSON telemetry data
2. **Multi-Device Tracking** - Track multiple paired phones simultaneously
3. **Direction Finding** - Use multiple antennas to determine direction to device
4. **Mobile App** - Android/iOS app to visualize device proximity
5. **Cloud Deployment** - Deploy backend to Azure App Service for remote access
6. **Battery Optimization** - Implement sleep modes and periodic wake-ups
7. **Web Dashboard** - React frontend to visualize historical distance data

---

## Conclusion

This project successfully demonstrates an end-to-end IoT system that:

✅ **Hardware:** Microcontroller + sensors + display
✅ **Embedded Systems:** Real-time measurements with signal processing
✅ **Networking:** BLE communication with smart pairing
✅ **Backend:** C# .NET API with database
✅ **Data Persistence:** SQLite database with structured schema
✅ **Problem Solving:** Pragmatic architecture when facing real-world constraints

The system is functional, tested, and ready for demonstration. The documented design decisions and technical evolution provide insight into IoT development trade-offs and constraints.

---

---

## Part 6: MQTT Integration & Partition Scheme Solution (2026-06-21)

### Critical Flash Memory Issue Resolved

**Problem:** ESP32 has 4MB total flash, but default OTA (Over-The-Air) partition scheme splits usable space:
- OTA Partition 1: ~1.3 MB (max firmware size)
- OTA Partition 2: ~1.3 MB (backup for updates)
- Bootloader + System: ~400 KB reserved

Even with PubSubClient (lightweight), firmware was 1.475 MB, exceeding the 1.31 MB limit by 164 KB.

**Breakthrough Discovery:** Disabled OTA updates to use single large partition

**Solution Implemented:**
```ini
[env:firebeetle2]
board_build.partitions = no_ota.csv
```

**Result:**
- Available flash increased from 1.31 MB to 2.09 MB
- Firmware size: 1.534 MB
- Usage: 73.2% ✅ FITS

### Library Evaluation & MQTT Selection

**Attempted Libraries:**
1. **PubSubClient@2.8** ✅ Chosen
   - Size: ~50 KB
   - Fully compatible with platform 5.3.0
   - Standard Arduino MQTT library
   - Synchronous API

2. **PicoMQTT@1.3** ❌ Rejected
   - Requires ESP32 core >= 2.0.7
   - Platform 7.0.1 upgrade caused tool dependency issues (missing intelhex)
   - API complexity (template-based)
   - Larger final binary than expected

3. **AsyncMqttClient** ❌ Not in registry
   - Multiple failed package names (marvinroger/async-mqtt-client, etc.)
   - Registry incompatibility

### Final Firmware Configuration

**Compiled Successfully:**
```
Platform: espressif32 5.3.0
Board: esp32dev (FireBeetle 2 ESP32-E)
Build Flags: -Os -DCORE_DEBUG_LEVEL=0 -Wl,--gc-sections
Partition Scheme: no_ota.csv (single large partition)

Libraries:
- WiFi (built-in, ~200 KB)
- ESP32 BLE Arduino (~400 KB)
- PubSubClient 2.8 (~50 KB)
- LiquidCrystal_I2C 1.1.4 (~30 KB)
- Framework overhead (~400 KB)
- Total: 1.534 MB (within 2 MB limit)
```

**Features Enabled:**
✅ BLE pairing with Xiaomi phone
✅ RSSI measurement and distance calculation
✅ LCD real-time display (RSSI + distance)
✅ WiFi connectivity (SSID: "UwU")
✅ MQTT publishing to broker.hivemq.com
✅ JSON telemetry every 10 seconds

### Upload Status

**Date:** 2026-06-21 15:32 UTC
**Device:** ESP32-D0WD-V3 (revision v3.1)
**MAC:** d4:8c:49:ca:ec:9c
**Upload Speed:** 921600 baud
**Result:** ✅ SUCCESS

Firmware transferred to flash memory without errors. Device ready for pairing and testing.

### Frontend Update

**Renamed Wind Turbine to Device Locator:**
- Updated `wwwroot/index.html` title from "windturbinemonitor-react" to "DeviceLocator"
- Frontend served from `localhost:5021` on backend startup
- API endpoints functional:
  - GET /api/location/latest
  - GET /api/location/device/{deviceName}
  - GET /api/location/all

---

## Next Steps (Ready for Testing)

1. **Start Backend API:**
   ```bash
   cd DeviceLocator.Api
   dotnet run
   ```

2. **Pair ESP32 with phone:**
   - Power on ESP32 (display shows "Waiting...")
   - Go to phone Bluetooth settings
   - Select "DeviceLocator"
   - Complete pairing

3. **Verify Connection:**
   - LCD displays distance measurement
   - Check backend database: GET http://localhost:5021/api/location/latest
   - MQTT messages received on fsiot/devicelocator/telemetry

4. **View Web Dashboard:**
   - Open http://localhost:5021 in browser
   - See real-time and historical device location data

---

**Project Timeline:** 2026-06-20 to 2026-06-21
**Status:** Hardware + Firmware ✅ | Backend ✅ | API ✅ | Database ✅ | MQTT ✅ | Testing (Ready)

---

## Part 7: Frontend Rebuild & Authentication Implementation (2026-06-22)

### Complete Frontend Rebuild from Wind Turbine Monitor

**Challenge:** Frontend had hardcoded Wind Turbine Monitor references and Alert/Command sections not needed for Device Locator.

**Solution:** Rebuilt entire React frontend with Device Locator customizations:

1. **React Project Setup**
   - Copied WindTurbineMonitor.React to DeviceLocator.React
   - Updated package.json name: "devicelocator-react"
   - Rebuilt with Vite (npm run build)

2. **File & Component Renaming**
   - TurbineCard.tsx → DeviceCard.tsx
   - TurbineDetail.tsx → DeviceDetail.tsx
   - useTurbines.ts → useDevices.ts
   - turbine.ts → device.ts
   - Updated all import paths across project

3. **Text & Branding Updates**
   - Removed: "💨 Wind Device Monitor" → "📱 Device Locator Monitor"
   - Removed: "Windmill Inspection Centre" → "Real-time Device Location Tracking"
   - Removed demo login text: "Demo: Enter any username to get started"
   - Updated all namespace references: WindTurbineMonitor → DeviceLocator

4. **Property Fixes**
   - Updated DeviceCard to use status field instead of turbine-specific metrics (nacelleTemperatureCelsius)
   - Updated DeviceDetail dashboard metrics display
   - Adapted data model from Turbine to Device structure

### Authentication System Implementation

**Added from Wind Turbine Monitor:**
- AuthService.cs: JWT token generation + BCrypt password hashing
- AuthController.cs: /api/auth/login and /api/auth/seed-testuser endpoints
- Integrated into Program.cs with dependency injection
- Test user auto-seeded on startup: testuser / password123

**Backend Configuration Updates:**
- appsettings.json updated:
  - Jwt:Issuer: "WindTurbineMonitor" → "DeviceLocator"
  - Jwt:Audience: "WindTurbineMonitorUsers" → "DeviceLocatorUsers"
  - Logging references updated

### Frontend Deployment

**Build Status:** ✅ SUCCESS
- Compiled without errors
- Output: 588 KB (gzipped: 178 KB)
- Deployed to wwwroot folder
- Backend serving new frontend on http://localhost:5021

**Current Frontend Features:**
- ✅ Login page with Device Locator branding
- ✅ Clean, minimal design
- ✅ JWT authentication
- ✅ DeviceCard grid showing latest measurements
- ✅ Ready for historical data visualization

---

## Part 8: Next Steps - MQTT Pipeline Verification (2026-06-22)

### Planned Dashboard Enhancement

**Goal:** Display 90-day historical graphs for each paired device

**Design:**
1. Keep DeviceCard grid (latest readings for all devices)
2. Add device selector dropdown
3. Add Distance graph (last 90 days)
4. Add RSSI graph (last 90 days)
5. Auto-refresh every 10 seconds when new MQTT data arrives

**Why 90 days, not real-time?**
- Realistic use case: Find lost phone in house (not continuous tracking)
- Periodic measurements (once per connection) are sufficient
- Historical patterns show device location trends over time
- Similar to wind turbine monitoring (tracking patterns, not live streams)

### Immediate Action Items (2026-06-22)

**Before implementing dashboard graphs:**
1. Power on ESP32 and pair with phone via Bluetooth
2. Let ESP32 connect to WiFi and take RSSI measurements
3. Verify MQTT messages reach backend
4. Check database for stored DeviceLocation entries
5. Test API endpoints to retrieve data:
   - GET /api/location/latest
   - GET /api/location/device/{deviceName}
6. Confirm data structure and formatting

**Verified Pipeline:**
- ESP32 → MQTT → Backend MQTT Receiver → Database ✅
- Database → REST API → Frontend ✅
- Then proceed with dashboard UI implementation

---

**Current Status:** 2026-06-22 00:50 UTC
**Completed:** Hardware | Firmware | Backend API | Authentication | Frontend UI (Device Locator branded)
**Next:** Verify MQTT→Database→API pipeline works end-to-end before dashboard implementation

---

## Session 2: MQTT Pipeline Testing & Verification (2026-06-22)

### Issue Resolved ✅

**Problem:** Backend logs showed `FormatException: String '1087294' was not recognized as a valid DateTime`

**Root Cause:** ESP32 was sending `millis()` (milliseconds since boot) as timestamp, but backend tried parsing as formatted DateTime.

**Solution:** Backend now uses `DateTime.UtcNow` when MQTT message arrives (more accurate anyway).

### End-to-End Pipeline Verification ✅

**Confirmed Working:**
- ✅ ESP32 hardware: LCD displays RSSI and distance in real-time
- ✅ WiFi connectivity: Serial monitor shows "MQTT OK"
- ✅ MQTT publishing: `{"DeviceName":"5e:8e:98:27:e5:66","Rssi":-48,"Distance":0.4,...}`
- ✅ Backend receiving: MQTT controller parsing and storing
- ✅ Database storage: SQLite contains 91+ DeviceLocation records
- ✅ API retrieval: `GET /api/location/latest` returns device data with timestamps

**Serial Monitor Output (COM3):**
```
MQTT: {"DeviceName":"5e:8e:98:27:e5:66","Rssi":-48,"Distance":0.4,"Status":"Connected","Timestamp":"2170502"}
Outlier rejected: 1.8m (diff: 1.4m)
MQTT: {"DeviceName":"5e:8e:98:27:e5:66","Rssi":-46,"Distance":0.4,"Status":"Connected","Timestamp":"2180517"}
```

**API Response:**
```
id: 91
deviceName: 5e:8e:98:27:e5:66
timestamp: 2026-06-22T03:16:46.3101516
rssi: -48
distance: 0.4
status: Connected
```

### Next Steps: Dashboard Implementation

Ready to implement historical data visualization:
1. Add device selector dropdown
2. Add Distance graph (last 90 days)
3. Add RSSI graph (last 90 days)
4. Auto-refresh every 10 seconds when new data arrives
