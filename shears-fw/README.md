# Shears Firmware

The shears firmware runs on an ESP32 and provides three core functions for the Garden E-Cutters system:

1. **BLE Peripheral** – advertises as `WM-SHEARS` and exposes the custom log-transfer service.  
2. **GPS Logger** – receives NMEA sentences over UART2, parses `$GPGGA`, and appends rows to a CSV log stored in SPIFFS.  
3. **Status LED** – visually indicates BLE link state.

All continuous work (GPS reads, save requests, BLE I/O, file streaming) runs inside module-specific FreeRTOS tasks. `main.c` simply initializes the modules and ties them together.

---

## Features

### 1. BLE Peripheral (NimBLE)

Implemented in `shears_ble.c`.

- Advertises the name **`WM-SHEARS`**.
- Exposes a custom 16-bit service: **0xFFF0**.
- Provides two characteristics:
  - **Control (0xFFF1)**  
    Accepts `START_TRANSFER` and `ABORT` from the base. Sends STATUS events back.
  - **Data (0xFFF2)**  
    Streams file chunks to the base using notifications.
- Restarts advertising automatically after disconnects.
- Reports connection state to the application via callback (`bleConnChanged` in `main.c`).

---

### 2. Status LED (GPIO 33)

Implemented in `shears_led.c`.

| State                         | Behavior   |
|------------------------------|------------|
| Advertising / Waiting         | Fast blink |
| Connected to Base            | Solid on   |
| Disconnected / Reconnecting  | Blink      |

The LED runs on its own FreeRTOS task and stays independent of BLE and GPS workloads.

---

### 3. GPS Logger (UART2 + SPIFFS + Button)

Implemented in `gps_logger.c`.

Capabilities:

- Reads raw NMEA sentences from a GPS module on **UART2**  
  (RX = GPIO 16, TX = GPIO 17, 9600 baud).
- Reassembles complete NMEA lines in a reader task.
- Tracks the latest full sentence in `latestNmea`.
- Logs `$GPGGA` data into `/spiffs/gps_points.csv`.
- CSV file is automatically created with a header row on first boot.
- Supports two ways to trigger a save:
  - **Physical button** on GPIO 23 (falling-edge interrupt).
  - **Software call:** `gpsLoggerRequestSave()` (used later for BLE-driven saves).

CSV fields:

```
utc_time,latitude,longitude,fix_quality,num_satellites,
hdop,altitude,geoid_height
```

---

## Log Transfer Service (Shears → Base)

Implemented in `log_transfer_server.c`.

- The base requests a file by basename, e.g. `"gps_points.csv"`.
- The shears resolves this to `/spiffs/<name>`.
- The transfer server:
  - validates the request
  - opens the file
  - sends `STATUS_OK` with the size
  - streams indexed data chunks until EOF
  - sends `STATUS_TRANSFER_DONE` when finished
- Chunk order is strictly increasing (`chunkIndex`), enabling the base to detect gaps.

This service is used to offload the GPS log to the base over BLE.

---

## Project Structure

```
shears-fw/
├── main/
│   ├── main.c                 # Top-level wiring
│   ├── shears_led.c/.h        # LED subsystem
│   ├── shears_ble.c/.h        # NimBLE advertising + callbacks
│   ├── gps_logger.c/.h        # UART + SPIFFS + button + CSV logging
│   ├── log_transfer_server.c/.h
│   └── CMakeLists.txt
└── partitions.csv
```

---

## Partition Table

```
nvs,        data, nvs,     0x9000,   0x5000
phy_init,   data, phy,     0xE000,   0x1000
factory,    app,  factory, 0x10000,  0x140000
storage,    data, spiffs,            0x64000
```

SPIFFS is used for the GPS log and any future on-device files.

---

## Build & Flash

Set target:
```
idf.py set-target esp32
```

Build:
```
idf.py build
```

Flash (replace with your serial device):
```
idf.py -p COM5 flash
```

Monitor:
```
idf.py -p COM5 monitor
```

---

## Hardware Notes

### Status LED
- GPIO 33  
- Active-high  
- Series resistor: 220–330 Ω  

### GPS UART (NMEA input)
- RX → GPIO 16  
- TX → GPIO 17  
- 9600 baud  

### Save Button
- GPIO 23  
- Internal pull-up  
- Falling-edge interrupt  

### SPIFFS
- Mounted at `/spiffs`  
- Stores `/spiffs/gps_points.csv`  

---

## Next Steps

- Add CRC to improve file integrity.
- Add a queueing mechanism on the shears to delete log entries once acknowledged by the base.
