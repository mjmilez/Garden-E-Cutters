# Garden E‑Cutters Firmware (BLE‑v1)

This branch contains the first fully modular BLE communication link between the **shears** and the **base station**.  
Both sides run ESP‑IDF using the NimBLE stack and now include clean subsystem architecture for BLE, LED status indication, and (on the shears) GPS logging with SPIFFS.

BLE‑v1 establishes a stable connection flow, separates responsibilities into modules, and lays the groundwork for structured telemetry exchange in BLE‑v2.

---

# High‑Level Overview

## Shears Firmware (Peripheral Role)

The shears run as a **BLE Peripheral** and now include **three independent subsystems**:

### **1. BLE Peripheral (shears_ble.c/.h)**
- Advertises as **`WM-SHEARS`**
- Broadcasts custom service UUID **`0xFFF0`**
- Accepts incoming connections from the base
- Automatically restarts advertising on:
  - disconnect
  - connection failure
- Reports connection state to the application layer via callback

### **2. Status LED (shears_led.c/.h)**
- GPIO: **33**
- Fast blink → advertising / searching for base  
- Solid ON   → connected  
- Blink again → disconnected  
- Dedicated FreeRTOS task ensures non‑blocking behavior

### **3. GPS Logger (gps_logger.c/.h)**
The shears now include a fully integrated GPS logging subsystem:

- Reads NMEA sentences via **UART2** (GPIO16/17 @ 9600 baud)
- Buffers full NMEA lines in a background task
- Extracts and parses **`$GPGGA`** sentences
- Stores GPS fixes to **SPIFFS** at:
  `/spiffs/gps_points.csv`
- Ensures CSV file + headers exist on boot
- Physical “Save Point” button on **GPIO 23** triggers a save
- Software save is also available through:
  ```c
  gpsLoggerRequestSave();
  ```

This module is fully self‑contained and ready to integrate with BLE commands in BLE‑v2.

---

## Base Firmware (Central Role)

The base firmware runs as a **BLE Central**, handling scanning and link management:

### **1. BLE Central (base_ble.c/.h)**
- Scans continuously for advertisements named **`WM-SHEARS`**
- Filters by device name for fast pairing
- Attempts connection when the shears are detected
- Automatically resumes scanning on:
  - disconnect
  - connection failure
  - scan completion event

### **2. Status LED (base_led.c/.h)**
- GPIO: **33**
- Fast blink → scanning / connecting  
- Solid ON   → connected  
- Returns to blinking on disconnect  
- Runs in its own FreeRTOS task

The base is now cleanly structured and ready to add:
- GATT discovery  
- Notification subscription  
- Telemetry decoding  

in BLE‑v2.

---

# Updated Architecture (BLE‑v1 Modular Layout)

```
BLE-v1/
├── shears-fw/
│   ├── main/
│   │   ├── main.c
│   │   ├── shears_ble.c/.h
│   │   ├── shears_led.c/.h
│   │   ├── gps_logger.c/.h
│   │   └── CMakeLists.txt
│   └── README.md
│
├── base-fw/
│   ├── main/
│   │   ├── main.c
│   │   ├── base_ble.c/.h
│   │   ├── base_led.c/.h
│   │   └── CMakeLists.txt
│   └── README.md
│
└── README.md   <-- (this file)
```

Each subsystem is fully separated, testable, and easy to expand.

---

# BLE‑v1 → BLE‑v2 Roadmap

BLE‑v1 establishes a stable and modular BLE foundation. BLE‑v2 will introduce *actual data exchange*.

## **1. Telemetry Record Definition**

Planned structure:

```c
typedef struct {
    uint8_t  version;
    uint16_t seq;
    uint32_t timestamp_ms;
    int16_t  force;
    int16_t  position;
    uint8_t  battery_pct;
    uint8_t  flags;
} __attribute__((packed)) shears_telemetry_t;
```

This will be transported over BLE in BLE‑v2.

---

## **2. Shears: Add Telemetry GATT Characteristic**
Service: `0xFFF0`  
Characteristic: `0xFFF1`

Properties:
- READ
- NOTIFY

The shears will:
- update telemetry fields periodically
- send notifications when connected

---

## **3. Base: Discovery + Notification Pipeline**
The base will:
- Discover the shears’ GATT service
- Find characteristic `0xFFF1`
- Enable notifications via CCCD
- Log incoming telemetry packets

This establishes the one‑way (shears → base) telemetry stream.

---

## **4. Optional: Add Command/Control Characteristic (`0xFFF2`)**
Allows base → shears messaging, e.g.:

- Trigger GPS save  
- Request configuration  
- Set sampling rates  

---

# Build & Flash Instructions (Both Targets)

Activate ESP-IDF:

```bash
. $IDF_PATH/export.sh
```

Build:

```bash
idf.py build
```

Flash (example):

```bash
idf.py -p COM5 flash
```

Monitor:

```bash
idf.py -p COM5 monitor
```

---

# Summary

BLE‑v1 now includes:

✓ Fully modular subsystems  
✓ BLE connection logic (central ↔ peripheral)  
✓ Automatic scanning/advertising recovery  
✓ Status LEDs on both devices  
✓ SPIFFS filesystem integration  
✓ GPS UART + CSV logging on shears  
✓ Clean project structure ready for telemetry  

BLE‑v1 delivers the stable communication foundation needed to build the full wireless data link for the Garden E‑Cutters system.

BLE‑v2 will layer telemetry, command/control, and reliability mechanisms on top of this solid base.

