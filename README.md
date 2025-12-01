# Garden E-Cutters Firmware (BLE-v1)

This repository contains the ESP32 firmware for the **shears** and the **base station** used in the Garden E-Cutters system. Both devices run ESP-IDF with the NimBLE stack and follow a modular design that separates BLE, LED indication, and (on the shears) GPS logging and file-transfer behavior.

BLE-v1 establishes a stable, reproducible communication link that supports:

- automatic scanning and reconnection  
- a custom BLE service (`0xFFF0`)  
- a complete log file transfer protocol between devices  
- clean subsystem boundaries  

The immediate next step is to hook this firmware into the existing web interface so the base can serve the collected data over HTTP, in parallel with continuing work on telemetry packets, CRC, and queueing.

---

# High-Level Architecture

The firmware is split into two independent ESP32 applications:

- **shears-fw** – operates as a BLE peripheral  
- **base-fw** – operates as a BLE central  

Each lives in its own folder, builds independently, and exposes its own README.

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

This is the bridge between “BLE world” and “filesystem world” on the base. The web server will read from the same data that this module writes.

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

# Next Steps

The BLE log-transfer pipeline is in place. The next work is to wire this into the web server and UI so the data on the base can be viewed and interacted with, while in parallel evolving the data transmission protocol itself.

### 1. Connect base firmware to the web server

- Decide on the data source:
  - read directly from `/spiffs/gps_points.csv`, or
  - maintain an in-memory representation (parsed from CSV) that the web server can expose.
- Define a clean interface between:
  - the log transfer client (which writes the file), and
  - the HTTP/web server layer (which serves it).

At a high level, the base should:

1. Pull the log from the shears over BLE (already implemented).  
2. Store or parse it on the base (SPIFFS file or RAM model).  
3. Expose the data over HTTP to the existing web frontend.

### 2. Define HTTP endpoints for log data

Add one or more endpoints to the existing base web server, for example:

- `GET /api/gps/log/raw`  
  returns the raw CSV file

- `GET /api/gps/log`  
  returns parsed data as JSON

- `GET /api/gps/latest`  
  returns only the latest point or a small window

### 3. Hook the web UI to these endpoints

In the existing web interface:

- call the new API endpoints to fetch log data  
- display:
  - a basic table of points, or
  - a minimal map/plot-style view
- add a “Refresh log” or “Pull from shears” action that:
  - triggers a new log transfer, or
  - at least reloads the data the firmware has already pulled

### 4. Optional: trigger log pulls from the web UI

Once the basic data path is working:

- add a HTTP endpoint that instructs the firmware to:
  - request a fresh log from the shears
  - wait until the transfer completes
  - then return success/failure to the frontend
- integrate that into the UI as a “Sync from shears” button

This closes the loop: browser → web server → BLE → shears → BLE → base → browser.

### 5. Evolve the data protocol (CRC + queueing + telemetry)

In parallel with web work, the underlying transmission protocol can be made more robust:

- add per-entry or per-chunk CRCs for integrity
- implement a queue of entries on the shears and delete entries only after ACK from the base
- define a compact telemetry packet and characteristic for real-time streaming
- give the base tools to distinguish “bulk log transfer” from “live telemetry”

These changes build on the same structure but can be layered in as the web path stabilizes.

---

# Summary

BLE-v1 now provides:

- a stable BLE central/peripheral pair  
- a custom GATT service for file transfer  
- a GPS logging subsystem on the shears  
- a file-transfer client on the base  
- SPIFFS integration and status LEDs on both devices  

The next concrete milestone is to surface that data through the base web server and wire it into the existing web interface, while incrementally improving the transfer protocol with CRC, queueing, and later telemetry.
