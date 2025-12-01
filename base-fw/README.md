# Base Firmware

The base firmware runs on an ESP32 (ESP-IDF) and acts as the BLE central for the Garden E-Cutters system. It scans for the shears device, connects when it is found, drives a status LED, and requests log data using the custom log-transfer service.

## What This Firmware Does

### 1. BLE Central for WM-SHEARS
Files: base_ble.c, base_ble.h

The base uses NimBLE in central mode:

- Scans for devices advertising "WM-SHEARS"
- Filters advertisements by name
- Connects automatically when the shears are discovered
- Restarts scanning on disconnect, failed connection attempts, or scan completion
- Reports link state through a callback
- Performs full GATT discovery to locate the log-transfer service and characteristics

### 2. Log Transfer Client
Files: log_transfer_client.c, log_transfer_client.h

The base implements the client side of a simple file transfer protocol:

1. Sends START_TRANSFER with the requested filename
2. Receives STATUS_OK with file size
3. Streams incoming chunks into SPIFFS or a RAM buffer
4. Handles STATUS_TRANSFER_DONE and finalizes storage
5. Dumps the first several lines for debugging

The client is entirely self-contained. BLE only forwards notifications into it.

### 3. Status LED (GPIO 33)
File: base_led.c

LED behavior:
- Blink while scanning or attempting to reconnect
- Solid on when connected

Runs in its own FreeRTOS task.

### 4. SPIFFS Initialization
File: main.c

Mounts the "storage" partition at /spiffs so CSV logs can be written locally.

## Project Structure

```
base-fw/
├── main/
│   ├── main.c
│   ├── base_led.c
│   ├── base_led.h
│   ├── base_ble.c
│   ├── base_ble.h
│   ├── log_transfer_client.c
│   ├── log_transfer_client.h
│   ├── log_transfer_protocol.h
│   ├── log_paths.h
│   └── CMakeLists.txt
└── CMakeLists.txt
```

main.c wires modules together:

- Initialize SPIFFS
- Initialize and start LED system
- Initialize BLE central
- Request logs when connected

## Build and Flash

```
idf.py set-target esp32
idf.py build
idf.py -p <port> flash
idf.py -p <port> monitor
```

Replace <port> with your serial device.

## Hardware Notes

### LED
- GPIO: 33
- Active-high
- Series resistor: 220 to 330 ohms

### BLE Link
- Pairs with the shears advertising as "WM-SHEARS"
- Base works only as central
- Uses NimBLE for low resource usage

## Next Steps

### 1. Per-entry CRC Validation
Future transfers will treat each log entry as a discrete record with:
- entry index
- payload
- CRC

The base will verify each entry before committing it.

### 2. Queue-Based Deletion Pipeline
Once the base receives and validates an entry:
- Write to SPIFFS
- Respond with acknowledgement
- Shears deletes the acknowledged entry
- Shears sends the next entry

This prevents re-sending entire CSV files and provides ordered, reliable delivery.

### 3. Shears-Side Bookkeeping
The shears will maintain:
- A queue of unsent log entries
- A flag for waiting on an acknowledgement
- Logic for deleting entries on acknowledgement
- Ability to resume after disconnects

### 4. Optional Future Telemetry
The base may later subscribe to telemetry characteristics such as:
- force readings
- blade position
- cut events
- GPS triggers
- battery status

### 5. Improved Reconnection Logic
Add backoff or timing control to avoid constant reconnect attempts when the shears are unavailable.
