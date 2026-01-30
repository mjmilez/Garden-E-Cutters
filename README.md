# Garden E-Cutters Firmware (BLE-v1)

This repository contains the ESP32 firmware for the **shears** and the **base station** used in the Garden E-Cutters system.

---

# High-Level Architecture

The firmware is split into two independent ESP32 applications:

- **shears-fw** – operates as a BLE peripheral  
- **base-fw** – operates as a BLE central  

Each lives in its own folder, builds independently, and has its own README.

---

## Shears Firmware (Peripheral)

The shears act as the BLE advertising device. It exposes the log-transfer GATT service and handles GPS logging on-device.

### 1. BLE Peripheral (`shears_ble.c/.h`)
- Advertises as `WM-SHEARS`  
- Publishes custom service: `0xFFF0`  
- Implements:
  - **Control characteristic (0xFFF1)**  
    Accepts `START_TRANSFER` and `ABORT`  
    Sends status events back (`STATUS_OK`, `STATUS_DONE`, error codes)
  - **Data characteristic (0xFFF2)**  
    Streams indexed file chunks to the base
- Automatically resumes advertising on disconnect  
- Provides connection-state callback to the application

### 2. Status LED (`shears_led.c/.h`)
- GPIO 33  
- Fast blink → waiting for a connection  
- Solid → connected  
- Blink again → disconnected  
- Runs in its own FreeRTOS task

### 3. GPS Logger (`gps_logger.c/.h`)
- Reads NMEA sentences from a GPS module on UART2 (9600 baud on GPIO16/17)
- Reassembles full NMEA lines in a reader task
- Parses `$GPGGA` sentences
- Writes entries to `/spiffs/gps_points.csv`
- Ensures the CSV file and header exist on boot
- Save triggers:
  - physical button on GPIO 23 (interrupt-driven)  
  - software save via `gpsLoggerRequestSave()`  

CSV fields:

```text
utc_time,latitude,longitude,fix_quality,num_satellites,
hdop,altitude,geoid_height
```

### 4. Log Transfer Server (`log_transfer_server.c/.h`)
- Accepts filename from the base (`"gps_points.csv"`)
- Resolves to `/spiffs/<filename>`
- Sends:
  - `STATUS_OK` with file size
  - sequential file chunks (indexed to detect gaps)
  - `STATUS_TRANSFER_DONE` when finished
- Supports `CTRL_CMD_ABORT`

---

## Base Firmware (Central)

The base acts as the BLE central device. It handles scanning, discovery, connection, and pulling the log from the shears so that it can later be exposed via the web interface.

### 1. BLE Central (`base_ble.c/.h`)
- Scans for advertisements named `WM-SHEARS`
- Connects automatically when the shears are discovered
- Resumes scanning after:
  - disconnect
  - failed connection attempts
  - scan completion events
- Performs GATT discovery of:
  - the log-transfer service (`0xFFF0`)
  - the control characteristic (`0xFFF1`)
  - the data characteristic (`0xFFF2`)
- Subscribes to notifications on both characteristics
- Exposes connection-state callback to the application (used by `main.c` and the LED)

### 2. Status LED (`base_led.c/.h`)
- GPIO 33  
- Blinking → scanning / connecting  
- Solid → connected  
- Runs in its own FreeRTOS task

### 3. Log Transfer Client (`log_transfer_client.c/.h`)
- Sends `START_TRANSFER` with the requested basename (for now, `gps_points.csv`)
- Receives:
  - status events (`STATUS_OK`, `STATUS_TRANSFER_DONE`, errors)
  - indexed file chunks via DATA notifications
- Writes received data to:
  - a local SPIFFS file (default: `/spiffs/gps_points.csv`), or
  - an in-RAM buffer if the filesystem is not available
- Handles chunk ordering and aggregation

---

# System Architecture Diagram

```text
               +----------------------+
               |      Base ESP32      |
               |   (BLE Central)      |
               +----------+-----------+
                          |
                Scan → Connect
                          |
         +----------------v----------------+
         |   Log Transfer Client Module    |
         |  - START_TRANSFER               |
         |  - receive STATUS + chunks      |
         +----------------+----------------+
                          |
                      SPIFFS / RAM
                          |
                   Web server / HTTP
                          |
                     Browser UI

                          ^
                          |
                          |
               +----------+-----------+
               |     Shears ESP32     |
               |    (BLE Peripheral)  |
               +----------+-----------+
                          |
             Advertise → Accept conn
                          |
     +--------------------v--------------------+
     |   Log Transfer Server Module            |
     |  - validate filename                    |
     |  - send STATUS_OK + size                |
     |  - notify sequential data chunks        |
     |  - send STATUS_DONE                     |
     +--------------------+--------------------+
                          |
                    GPS Logger
                (UART → $GPGGA → CSV)
                          |
                     status LED
```

---

# Repository Structure

```text
BLE-v1/
├── base-fw/
│   ├── main/
│   │   ├── main.c
│   │   ├── base_led.c/.h
│   │   ├── base_ble.c/.h
│   │   ├── log_transfer_client.c/.h
│   │   ├── log_transfer_protocol.h
│   │   ├── log_paths.h
│   │   └── CMakeLists.txt
│   └── README.md
│
├── shears-fw/
│   ├── main/
│   │   ├── main.c
│   │   ├── shears_led.c/.h
│   │   ├── shears_ble.c/.h
│   │   ├── gps_logger.c/.h
│   │   ├── log_transfer_server.c/.h
│   │   ├── log_transfer_protocol.h
│   │   ├── log_paths.h
│   │   └── CMakeLists.txt
│   └── partitions.csv
│   └── README.md
│
└── README.md   ← this file
```

Both subprojects share `log_transfer_protocol.h` and `log_paths.h` to keep the BLE protocol and file naming conventions aligned.

---

# Build & Flash

Activate ESP-IDF:

```bash
. $IDF_PATH/export.sh
```

For either device:

```bash
idf.py set-target esp32
idf.py build
idf.py -p <port> flash monitor
```

Replace `<port>` with your actual serial device (for example `COM5` or `/dev/ttyUSB0`).

---

# Next steps

BLE log transfer works end-to-end. The next phase is wiring it into the base’s web server so the data can be viewed and interacted with.

- Hook the base web server up to the downloaded GPS log  

- Update the web UI to fetch and display the data  
  - basic table or simple map/plot  
  - button to refresh or re-pull the log

Longer-term to-dos:
- add CRCs / better error handling to the transfer protocol  
- ACK + delete entries on the shears side

# Bugs
- no known bugs at this moment
