# Device Locator - Complete Program Flow Guide for Beginners

---

## HOW THE PROGRAM WORKS - THE BIG PICTURE

Imagine this device as a **"phone finder"**:

1. **Setup Phase:** Initialize all the hardware (LCD, WiFi, Bluetooth, MQTT)
2. **Pairing Phase:** Wait for user to pair their phone via Bluetooth
3. **Connection Phase:** Connect to the paired phone
4. **Measurement Phase:** Continuously measure how strong the Bluetooth signal is
5. **Display Phase:** Show the signal strength and distance on the LCD screen
6. **Cloud Phase:** Send the data to the internet via MQTT every 10 seconds

The program cycles through these phases using a state machine in the `loop()` function.

---

## FILE 1: config.h - THE DATABASE OF INFORMATION

**What this file does:**
- Stores all the **settings and variables** the program needs to remember
- It's like a filing cabinet where you keep all the important data

**What's in it:**

```
LCD Object → controls the 16x2 display
WiFi Client → manages WiFi connection
MQTT Client → manages cloud communication
BLE Client → manages Bluetooth communication
```

**The Constants (unchanging values):**
- `RSSI_AT_1M = -55` → Reference signal strength at 1 meter
- `PATH_LOSS = 2.0` → How quickly signal weakens over distance
- `RSSI_SAMPLES = 5` → Average last 5 signal readings
- `OUTLIER_THRESHOLD = 0.5` → Ignore distance jumps larger than 0.5 meters
- `DISPLAY_INTERVAL = 1000` → Update LCD every 1 second
- `MQTT_SEND_INTERVAL = 10000` → Send data every 10 seconds

**The State Variables (values that change):**
- `connected` → Is phone currently connected? (true/false)
- `isPaired` → Has phone been paired? (true/false)
- `wasConnected` → Was connected before? (tracks state changes)
- `pairedBDAddr[6]` → The phone's MAC address (6 bytes)
- `lastValidDistance` → Last accepted distance (for outlier filtering)
- `targetRSSI` → Current signal strength reading

**How it's used:**
- Every other file includes config.h
- All files can access these constants and variables
- It's the "shared memory" of the program

---

## FILE 2: display.h - THE LCD SCREEN CONTROLLER

**What this file does:**
- Controls the 16x2 LCD screen mounted on the device
- Shows status messages and measurements

**Functions in this file:**

### 1. setupDisplay()
**What it does:** Initialize the LCD when the device powers on

**Line by line:**
- `Wire.begin(21, 22)` → Start I2C communication on GPIO pins 21 & 22
  - I2C is a communication protocol for the LCD
- `lcd.init()` → Wake up the LCD and prepare it
- `lcd.backlight()` → Turn on the LCD backlight (so you can see it)
- `lcd.print("BLE Locator")` → Display the welcome message
- `delay(2000)` → Show it for 2 seconds so user sees it
- `lcd.clear()` → Erase the screen and prepare for main program

**When it's called:** Once in setup()

**After this runs:** LCD is ready to display information

---

### 2. updateDisplay(rssi, distance)
**What it does:** Update the LCD with current signal strength and distance

**Called from:** loop() - every 1 second

**What happens:**
```
Line 1: "RSSI:-55 dBm"
Line 2: "Dist:2.3m"
```

**Line by line:**
- `lcd.clear()` → Erase old data
- `lcd.setCursor(0, 0)` → Move cursor to top-left (row 0, column 0)
- `lcd.print("RSSI:")` → Print the label
- `lcd.print(rssi)` → Print the actual signal strength (e.g., -55)
- `lcd.print(" dBm")` → Print the unit
- `lcd.setCursor(0, 1)` → Move to bottom row
- Similar process for distance

**Practical use:** User looks at LCD and sees real-time measurements

---

### 3. showConnected()
**What it does:** Display "Connected!" message when phone connects

**When called:** First time entering the connected state

**What happens:**
- Clears LCD
- Shows "Connected!" for 2 seconds
- Returns to normal operation

**Why needed:** Visual feedback that pairing worked

---

### 4. showDisconnected()
**What it does:** Display "Disconnected!" message when phone disconnects

**When called:** Connection is lost

**Practical effect:** User knows phone was found but signal was lost

---

## FILE 3: wifi.h - THE INTERNET CONNECTION

**What this file does:**
- Connects the ESP32 to the home/office WiFi network
- Required to use MQTT (cloud connection)

**Only function: connectToWiFi()**

**What it does step by step:**

1. **Print startup message:**
   - `Serial.print("Connecting to WiFi: ")` → Shows progress on serial monitor
   - `Serial.println(SSID)` → Shows which network it's trying (from credentials.h)

2. **Start connection:**
   - `WiFi.begin(SSID, PASSWORD)` → Begin connecting to WiFi
   - Uses SSID and password from credentials.h

3. **Retry loop:**
   ```
   while (WiFi.status() != WL_CONNECTED && attempts < 20)
   ```
   - Keep trying to connect up to 20 times
   - Wait 500ms between attempts
   - Print dots (.....) to show progress
   - This gives it up to 10 seconds to connect

4. **After trying:**
   - If successful: Print IP address (e.g., "192.168.1.100")
   - If failed: Print "WiFi failed!"

**When called:** Once in setup()

**Why needed:** Can't reach MQTT broker without internet

---

## FILE 4: mqtt.h - THE CLOUD COMMUNICATOR & SIGNAL PROCESSING

**What this file does:**
- Sends sensor data to the cloud
- Receives commands from the cloud (like "find device")
- Processes the raw Bluetooth signal into distance measurements

**This is the most complex file. Let's go through it:**

---

### 1. calculateDistance(rssi)
**What it does:** Convert raw signal strength (RSSI) into distance in meters

**Why needed:** Bluetooth signal strength alone doesn't tell you distance - you need math

**The formula:**
```
distance = 10^((RSSI_AT_1M - RSSI) / (10 * PATH_LOSS))
```

**Example:**
- Phone at 1 meter away → RSSI = -55 dBm → distance = 1.0m
- Phone at 3 meters away → RSSI = -65 dBm → distance = 3.16m
- Phone at 10 meters away → RSSI = -75 dBm → distance = 10.0m

**How it works:**
- `RSSI_AT_1M (-55)` = our reference signal strength
- The more negative the RSSI, the farther the phone
- The formula converts this into understandable meters

**Line by line:**
- `if (rssi == 0) return 0` → If no valid reading, return 0
- `float distance = pow(10.0, ...)` → Calculate using logarithmic formula

**When called:** Every loop iteration when connected

**Practical use:** Raw -65 dBm → becomes 3.16 meters on display

---

### 2. getSmoothedRSSI(newRssi)
**What it does:** Smooth out noisy signal readings

**Why needed:** Raw RSSI jumps around wildly (±10-20 dBm per reading)
- Without smoothing: distance jumps 1m → 5m → 2m → 3m (jittery)
- With smoothing: distance gradually changes (stable)

**How it works:**
Uses a **circular buffer** to average the last 5 readings

```
Reading 1: -48 dBm
Reading 2: -50 dBm
Reading 3: -49 dBm
Reading 4: -51 dBm
Reading 5: -48 dBm
Average: (-48 + -50 + -49 + -51 + -48) / 5 = -49.2 dBm (smooth!)
```

**Line by line:**
1. `rssiBuffer[rssiIndex] = newRssi` → Store new reading at current position
2. `rssiIndex = (rssiIndex + 1) % RSSI_SAMPLES` → Move to next position (0→1→2→3→4→0→1...)
   - The `% 5` makes it wrap around (this is the "circular" part)
3. `for loop` → Add up all 5 values
4. `return sum / RSSI_SAMPLES` → Return the average

**Visual example:**
```
Position 0 (oldest):  -48
Position 1:           -50
Position 2:           -49
Position 3:           -51
Position 4 (newest):  -48
↑
After next reading, position 0 gets replaced with the new value
```

**When called:** Every loop iteration when connected

**Practical use:** Stabilizes the distance display

---

### 3. filterOutlier(newDistance)
**What it does:** Reject distance readings that seem unrealistic

**Why needed:** Sometimes signal glitches and gives crazy readings
- Example: Smoothed distance is 2.5m, but suddenly reads as 5.0m
- That's a 2.5m jump in one reading - probably a glitch!

**How it works:**
- Compare new distance to last accepted distance
- If difference > 0.5m, reject it (ignore the spike)
- If difference <= 0.5m, accept it (realistic change)

**Line by line:**
1. `if (lastValidDistance == 0)` → First reading? Accept it as baseline
2. `float difference = abs(newDistance - lastValidDistance)` → How much changed?
3. `if (difference > OUTLIER_THRESHOLD)` → Is the change too big (> 0.5m)?
   - If YES: Print warning to serial, return old distance (ignore new one)
   - If NO: Accept new distance, update lastValidDistance

**Practical example:**
- Last distance: 2.5m
- New distance: 3.0m (difference: 0.5m) → ACCEPT (realistic movement)
- New distance: 5.0m (difference: 2.5m) → REJECT (glitch, stay at 2.5m)

**When called:** Every loop iteration when connected

**Practical use:** Prevents the display from jumping around

---

### 4. mqttCallback(topic, payload, length)
**What it does:** Receive and process messages from the cloud

**When called:** Automatically when MQTT message arrives

**How it works:**
1. **Receive:** Message arrives from MQTT broker
2. **Convert:** Turn byte array into readable string
3. **Process:** Check what the message says
4. **React:** Do something based on the message

**Line by line:**
1. Loop converts bytes to string:
   ```cpp
   for (int i = 0; i < length; i++) {
     message += (char)payload[i];
   }
   ```
   - Example: bytes `[102, 105, 110, 100]` → string "find"

2. Print for debugging:
   ```cpp
   Serial.print("MQTT message on ");
   Serial.print(topic);  // e.g., "fsiot/devicelocator/command"
   Serial.println(message);  // e.g., "find"
   ```

3. Check if command:
   ```cpp
   if (String(topic) == mqtt_command_topic)  // Is this a command?
   ```

4. Execute command:
   ```cpp
   if (message == "find") {
     flashLED();  // Flash the LED!
   }
   ```

**Practical flow:**
- User clicks "Find Device" in web dashboard
- Backend publishes "find" to MQTT
- ESP32 receives it in this function
- LED flashes to help user find the device

---

### 5. reconnectMQTT()
**What it does:** Connect (or reconnect) to the MQTT broker

**When called:** 
- First time in setup()
- Whenever connection drops

**How it works:**

1. **Loop until connected:**
   ```cpp
   while (!client.connected() && WiFi.status() == WL_CONNECTED)
   ```
   - Keep trying as long as: NOT connected AND WiFi IS connected

2. **Try to connect:**
   ```cpp
   if (client.connect("DeviceLocator-ESP32"))
   ```
   - Client name: "DeviceLocator-ESP32" (broker sees this name)

3. **If successful:**
   ```cpp
   client.setCallback(mqttCallback);  // Register the callback function
   client.subscribe(mqtt_command_topic);  // Listen for commands
   return;  // Exit the function
   ```

4. **If failed:**
   - Wait 500ms and retry

**Practical effect:** Device is always connected to MQTT (or trying to be)

---

### 6. sendMQTTData(deviceName, rssi, distance)
**What it does:** Publish sensor measurements to the cloud

**Called from:** loop() - every 10 seconds

**How it works:**

1. **Check connection:**
   ```cpp
   if (!client.connected()) {
     reconnectMQTT();  // Try to reconnect if disconnected
   }
   ```

2. **Build JSON message:**
   ```cpp
   String payload = "{\"DeviceName\":\"5e:8e:98:27:e5:66\",\"Rssi\":-55,\"Distance\":2.3,\"Status\":\"Connected\",\"Timestamp\":\"1234567890\"}";
   ```
   - Creates a structured message the backend can understand
   - DeviceName: MAC address of the phone
   - Rssi: Signal strength
   - Distance: Calculated distance in meters
   - Status: Always "Connected" when this runs
   - Timestamp: When this measurement was taken

3. **Publish:**
   ```cpp
   client.publish(mqtt_topic, payload.c_str());
   ```
   - Send to topic: `fsiot/devicelocator/telemetry`
   - Backend listens on this topic and stores the data

4. **Debug print:**
   ```cpp
   Serial.print("MQTT: ");
   Serial.println(payload);  // Show what we sent
   ```

**Practical flow:**
- Device measures RSSI: -55 dBm
- Calculates distance: 2.3 meters
- Every 10 seconds: publishes to cloud
- Backend stores in database
- Web dashboard shows the data

---

## FILE 5: ble.h - THE BLUETOOTH CONTROLLER

**What this file does:**
- Handle Bluetooth pairing with the phone
- Maintain connection to paired phone
- Read signal strength from the connection

**Functions in this file:**

---

### 1. clearAllBonds()
**What it does:** Forget all previously paired phones

**When called:** Only if `#define CLEAR_BONDED_DEVICES` is uncommented

**Practical use:** Testing - forget old pairings to start fresh

**How it works:**
1. Get count of bonded devices
2. Create a list of all bonded devices
3. Loop through and delete each one
4. Free the memory

---

### 2. MySecurityCallbacks class
**What it does:** Handle the Bluetooth pairing handshake with the phone

**When called:** Automatically during pairing process

**What happens:**

**onPassKeyRequest():**
- Phone asks: "What PIN should I use?"
- ESP32 responds: "000000"

**onConfirmPIN():**
- User confirms PIN on phone screen
- ESP32 allows pairing

**onAuthenticationComplete():**
- **Most important function!**
- Called when pairing finishes (success or fail)
- **If successful:**
  - Captures the REAL MAC address: `5e:8e:98:27:e5:66`
  - Stores it in `pairedBDAddr[6]`
  - Sets `isPaired = true`
  - Prints the address to serial

**Why this matters:**
- Phones randomize their Bluetooth address for privacy
- During pairing, the REAL address is revealed in this callback
- We capture it so we can reliably find the phone later

**Practical flow:**
1. User opens phone's Bluetooth settings
2. User taps "DeviceLocator" to pair
3. This callback runs automatically
4. MAC address is captured
5. ESP32 now knows the phone's real address forever

---

### 3. setupBLE()
**What it does:** Configure Bluetooth and advertise the device

**Called from:** setup() - once at startup

**Line by line:**

1. `BLEDevice::init("DeviceLocator")` → Name the device "DeviceLocator"
   - This is what appears in phone's Bluetooth list

2. Print ESP32's own MAC address for debugging

3. `BLEDevice::setSecurityCallbacks(new MySecurityCallbacks())` → Register the pairing handler
   - When pairing starts, use MySecurityCallbacks

4. `BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_NO_MITM)` → Security level
   - Encrypts the connection

5. `BLEDevice::startAdvertising()` → Start broadcasting
   - Phone can now find "DeviceLocator" in Bluetooth list

**Practical effect:** After this runs, phone can see and pair with the device

---

### 4. connectToPhone()
**What it does:** Establish active connection to the paired phone

**Called from:** loop() - when `isPaired` but `!connected`

**How it works:**

1. `if (!isPaired) return false` → Can't connect if not paired
   
2. `if (pClient == nullptr) { pClient = BLEDevice::createClient(); }`
   - Create a BLE client object (used to connect to the phone)
   - Only create once

3. `if (pClient->connect(pairedPhoneAddress, BLE_ADDR_TYPE_RANDOM))`
   - Try to connect to the phone using its MAC address
   - If successful:
     - Set `connected = true`
     - Set `targetFound = true`
     - Return true

4. If fails, return false and try again next loop

**Practical flow:**
- loop() → not connected yet?
- Loop repeatedly calls connectToPhone()
- Eventually phone gets in range and connects
- Now we can read signal strength

---

### 5. readConnectionRSSI()
**What it does:** Read the current signal strength from the connection

**Called from:** loop() - every iteration when connected

**How it works:**
1. Check if client exists and is connected
2. If yes: `return pClient->getRssi()` → Get signal strength (e.g., -55 dBm)
3. If no: return 0 (invalid)

**Practical effect:** Gets the raw measurement that other functions use

---

## FILE 6: main.cpp - THE CONDUCTOR

**What this file does:**
- Includes all other files
- Runs setup() once
- Runs loop() forever
- Coordinates everything

**Functions in this file:**

---

### 1. flashLED()
**What it does:** Blink the LED 5 times

**Called from:** mqttCallback() - when "find" command received

**How it works:**
- Loop 5 times:
  - Turn LED ON (200ms)
  - Turn LED OFF (200ms)

**Practical use:** Help user physically locate the device

---

### 2. setup() - THE INITIALIZATION
**Called once when device powers on**

**Purpose:** Initialize all hardware and connections

**Step by step:**

1. **Serial monitor:**
   - `Serial.begin(115200)` → Start serial communication for debugging
   - Print welcome message

2. **LED setup:**
   - `pinMode(LED_PIN, OUTPUT)` → Configure LED pin as output
   - `digitalWrite(LED_PIN, LOW)` → Turn LED off (force it twice for safety)

3. **Optional: Clear old pairings:**
   - `#ifdef CLEAR_BONDED_DEVICES`
   - Only if uncommented at top

4. **Initialize everything:**
   - `setupDisplay()` → Initialize LCD
   - `connectToWiFi()` → Connect to internet
   - `client.setServer(mqtt_server, mqtt_port)` → Configure MQTT broker
   - `setupBLE()` → Start Bluetooth advertising

5. **Print ready message:**
   - "Ready" indicates all systems initialized

**After setup():** Device is running and waiting for pairing

---

### 3. loop() - THE MAIN STATE MACHINE
**Called repeatedly forever (thousands of times per second)**

**Purpose:** Main program logic - continuously measure and update

**This is where the magic happens!**

**The loop has 3 states:**

---

## STATE 1: WAITING FOR PAIRING

```cpp
if (!isPaired) {
  lcd.print("Waiting...");
  delay(1000);
}
```

**What's happening:**
- Phone hasn't paired yet
- Display shows "Waiting..."
- Loop every 1 second

**Practical effect:** User sees "Waiting..." while they pair their phone via Bluetooth settings

**Transition:** When user pairs phone, MySecurityCallbacks captures the MAC address and sets `isPaired = true`. Next loop iteration moves to STATE 2.

---

## STATE 2: CONNECTING TO PHONE

```cpp
else if (!connected) {
  lcd.print("Connecting...");
  connectToPhone();  // Try to connect
  delay(2000);
}
```

**What's happening:**
- Phone is paired (MAC address known)
- But not yet connected (signal not strong enough or out of range)
- Display shows "Connecting..."
- Tries to connect every 2 seconds

**Why separate states?**
- Pairing = one-time trust relationship
- Connection = active signal link
- Can be paired but disconnected (phone in another room)

**Transition:** When `connectToPhone()` succeeds, sets `connected = true`. Next loop moves to STATE 3.

---

## STATE 3: MEASURING & TRANSMITTING (THE ACTIVE STATE)

```cpp
else {
  // Connected and measuring!
}
```

**This is the main work. Here's what happens:**

### Part A: Show "Connected!" once

```cpp
if (!wasConnected) {
  showConnected();
  wasConnected = true;
}
```

- First time entering this state: display "Connected!" for 2 seconds
- Set `wasConnected = true` so we don't show it again
- Next time we enter this state: skip this block

---

### Part B: Read signal strength

```cpp
targetRSSI = readConnectionRSSI();  // Get current signal (e.g., -55 dBm)
```

- Reads raw Bluetooth signal strength
- Example: -55 dBm means strong signal (close to phone)

---

### Part C: Validate the signal

```cpp
if (targetRSSI != 0 && targetRSSI != 127) {
  // Valid reading, process it
}
```

- `!= 0` → Ignore if no reading
- `!= 127` → Ignore if error code

If valid, proceed. If invalid, handle disconnection (see Part E below).

---

### Part D: Process the signal (THIS IS THE PIPELINE)

```cpp
int32_t smoothedRssi = getSmoothedRSSI(targetRSSI);
float filteredDistance = filterOutlier(calculateDistance(smoothedRssi));
```

**This is the data pipeline:**

1. **Raw RSSI** → `-55` (noisy Bluetooth reading)
2. **Smooth it** → `getSmoothedRSSI()` → `-49` (average of last 5)
3. **Calculate distance** → `calculateDistance()` → `2.3 meters`
4. **Filter outliers** → `filterOutlier()` → `2.3 meters` (reject spikes)

**Final result:** Cleaned up, reliable distance measurement

---

### Part D-1: Update LCD every 1 second

```cpp
if (millis() - lastDisplayTime >= DISPLAY_INTERVAL) {
  updateDisplay(smoothedRssi, filteredDistance);
  lastDisplayTime = millis();
}
```

- Track time since last update
- Only update if 1000ms has passed (prevents flickering)
- Display shows: `RSSI:-49 dBm` and `Dist:2.3m`

**Example timeline:**
- Time 0ms: Update LCD, set `lastDisplayTime = 0`
- Time 500ms: Skip update (only 500ms passed)
- Time 1000ms: Update LCD, set `lastDisplayTime = 1000`
- Time 1500ms: Skip update (only 500ms passed)
- etc...

---

### Part D-2: Send MQTT every 10 seconds

```cpp
if (millis() - lastMqttSend >= MQTT_SEND_INTERVAL) {
  sendMQTTData(DEVICE_FRIENDLY_NAME, smoothedRssi, filteredDistance);
  lastMqttSend = millis();
}
```

- Similar to LCD timing, but 10 seconds instead of 1 second
- Publishes JSON to cloud: `{"DeviceName":"...", "Rssi":-49, "Distance":2.3, ...}`
- Backend receives and stores in database
- Web dashboard displays the data

**Why different intervals?**
- LCD: Fast updates (1 sec) so user sees real-time on device
- MQTT: Slower updates (10 sec) to not flood the cloud with data

---

### Part E: Handle disconnection

```cpp
else {
  // RSSI was 0 or 127 = connection lost
  connected = false;
  wasConnected = false;
  isPaired = false;
  lastValidDistance = 0;
  showDisconnected();
  if (pClient) pClient->disconnect();
}
```

**What happens when signal is lost:**
- Display "Disconnected!" for 2 seconds
- Reset all state variables
- Disconnect the BLE client
- Loop goes back to STATE 1 (waiting for re-pairing)
- Can then go to STATE 2 (reconnecting)

---

### Part F: Small delay

```cpp
delay(100);
```

- Wait 100ms before next loop iteration
- Prevents CPU from running too fast
- Allows other processes to run

---

## THE COMPLETE CYCLE - REAL EXAMPLE

Here's what happens in real time:

**Second 0:** Power on
- setup() runs → Initialize everything
- Device advertises "DeviceLocator" on Bluetooth
- LCD shows splash screen "BLE Locator"

**Second 2:** User pairs phone
- Phone appears in Bluetooth list
- User taps "DeviceLocator"
- MySecurityCallbacks captures MAC: `5e:8e:98:27:e5:66`
- `isPaired = true`
- LCD shows "Waiting..."

**Second 5:** Phone in range
- loop() calls `connectToPhone()`
- Successfully connects to the phone's MAC address
- `connected = true`
- LCD shows "Connecting..." then "Connected!"

**Second 6+:** Continuous measurement
```
Loop iteration 1:
  Read RSSI: -48 dBm
  Smooth: -49 dBm
  Calculate: 2.1m
  Filter: 2.1m (accepted)
  Update LCD: "RSSI:-49 dBm" "Dist:2.1m"

Loop iteration 2:
  Read RSSI: -50 dBm
  Smooth: -49 dBm
  Calculate: 2.2m
  Filter: 2.2m (accepted)
  Skip LCD update (only 100ms passed)

... (many more iterations) ...

Loop iteration ~100 (after 10 seconds):
  Send MQTT: {"DeviceName":"5e:8e:98:27:e5:66","Rssi":-49,"Distance":2.1,...}
```

**Second 30:** User walks away
- Phone goes out of range
- RSSI becomes invalid
- loop() detects disconnection
- LCD shows "Disconnected!"
- Resets to waiting state
- Goes back to STATE 1

**Second 35:** User brings phone back
- Phone comes back in range
- Reconnects automatically
- Goes through STATE 2 (connecting) → STATE 3 (measuring)
- Back to continuous measurement

---

## HOW THE FILES TALK TO EACH OTHER

```
main.cpp (setup & loop)
├── Includes: config.h (settings)
├── Calls: display.h (LCD functions)
├── Calls: wifi.h (WiFi function)
├── Calls: mqtt.h (MQTT & signal processing)
├── Calls: ble.h (Bluetooth functions)
└── Calls: flashLED() (LED in main.cpp)

mqtt.h listens for commands that call flashLED()
ble.h handles pairing that sets isPaired (used by loop)
display.h uses variables from config.h
All files read/write to variables in config.h
```

---

## SUMMARY FOR PRESENTATION

**1. config.h** = Database (variables and constants)
**2. display.h** = LCD screen (4 functions for display)
**3. wifi.h** = Internet connection (1 function)
**4. mqtt.h** = Cloud communication + signal processing (6 functions)
**5. ble.h** = Bluetooth pairing and connection (5 functions)
**6. main.cpp** = Conductor (setup once, loop forever)

**The loop is a state machine with 3 states:**
1. **Waiting** - Waiting for phone to pair
2. **Connecting** - Paired but not connected
3. **Measuring** - Connected and continuously sending data

**Data flow in STATE 3:**
```
Bluetooth RSSI
    ↓
Smooth (5-sample average)
    ↓
Calculate distance (logarithmic formula)
    ↓
Filter outliers (reject > 0.5m changes)
    ↓
Update LCD (every 1 second)
Update MQTT (every 10 seconds)
```

That's the entire program!
