# BLE Device Locator - Exam Attempt 1 Summary

**Date:** June 22, 2026  
**Status:** Did not pass  
**Next Attempt:** ~2 months

## What Was Completed ✅

### Hardware & Firmware
- ✅ ESP32 FireBeetle 2 with BLE pairing (solved MAC randomization issue)
- ✅ RSSI signal processing with smoothing and outlier rejection
- ✅ LCD display integration (16x2 I2C)
- ✅ LED control via GPIO 5 with MQTT command triggering
- ✅ WiFi and MQTT connectivity
- ✅ Firmware size optimization (changed partition scheme from OTA to no_ota)
- ✅ Successfully uploaded and tested

### Backend API (.NET)
- ✅ C# .NET 10 ASP.NET Core
- ✅ SQLite database with EF Core migrations
- ✅ MQTT receiver (HiveMQtt) for device telemetry
- ✅ REST API endpoints:
  - GET /api/location/latest
  - GET /api/location/device/{deviceName}
  - GET /api/location/all
  - GET /api/location/devices
  - POST /api/location/command/find
- ✅ JWT authentication with BCrypt password hashing

### Frontend (React)
- ✅ Dashboard with device tiles (Redmi X card showing RSSI/Distance)
- ✅ Device detail page with historical graphs (Recharts)
- ✅ Time range selector (1h, 6h, 24h, 7d, 30d, 90d)
- ✅ "Find Device" button triggering MQTT LED command
- ✅ Login page with JWT token management

### Project Deployed
- ✅ GitHub repository: https://github.com/JacobWolf2912/BLE-Device-Locator
- ✅ .gitignore excludes sensitive files (credentials.h)

## Technical Achievements

1. **BLE Pairing Model** - Solved phone MAC randomization by switching from scanning to pairing
2. **Signal Processing** - Three-layer filtering (smoothing + distance calculation + outlier rejection)
3. **Memory Optimization** - Changed partition scheme to fit 1.5MB firmware on 2MB flash
4. **Full-Stack IoT** - Device → MQTT → Backend → Database → Frontend → MQTT Command → Device
5. **Bidirectional Communication** - Not just sensors, device responds to commands

## Potential Exam Feedback

- Code organization: All ESP32 code in one main.cpp (could split into modules)
- LED testing: Changed GPIO pins multiple times (GPIO 7 → GPIO 2 → GPIO 5)
- WiFi credentials: Had to change mid-exam when moving networks
- Architecture: Two-phase approach for MQTT (Phase 1: device measures, Phase 2: web upload planned)

## For Next Attempt (Improvements)

1. **Code Structure** - Separate ESP32 code into header files (ble.h, mqtt.h, signal_processing.h)
2. **Documentation** - Add inline comments explaining signal processing logic
3. **Error Handling** - Add more robust error recovery for WiFi/MQTT disconnections
4. **Testing** - Pre-test all hardware scenarios before exam
5. **Partition Scheme** - Decide early whether to use OTA or fixed partitions

## Key Files for Reference

- **ESP32 Firmware:** `src/main.cpp` (main logic), `include/credentials.h` (sensitive config)
- **Backend:** `DeviceLocator.Api/Controllers/LocationController.cs` (API endpoints)
- **Frontend:** `DeviceLocator.React/src/pages/DeviceDetail.tsx` (dashboard)
- **Database:** `DeviceLocator.Api/Models/DeviceLocation.cs` (data schema)

## Running the System

```bash
# Build firmware
cd "Device Locator"
python -m platformio run --target upload --upload-port COM3

# Run backend
cd DeviceLocator.Api
dotnet run

# Open dashboard
http://localhost:5021
```

---

**Next Focus:** DEI Point Calculator project
