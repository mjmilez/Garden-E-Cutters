# Base Firmware

The base firmware runs on an ESP32 using ESP-IDF and acts as the BLE central (client) for the Garden E-Cutters system. It scans for the shears device, connects when it is found, and drives a status LED to indicate link state.

## Features

### BLE Central
- Uses ESP-IDF’s NimBLE stack.
- Scans for devices advertising the name **`WM-SHEARS`**.
- Filters advertisements by device name to avoid unnecessary connections.
- Initiates a BLE connection when the shears device is discovered.
- Automatically restarts scanning after disconnects or failed connection attempts.

### Status LED (GPIO33)
- LED connected to **GPIO 33**.
- **Fast blink** while scanning or attempting to connect.
- **Solid on** when a BLE connection to the shears is active.
- Returns to blinking if the link is lost.

## Project Structure

```
base-fw/
├── main/
│   ├── main.c
│   └── CMakeLists.txt
└── CMakeLists.txt
```

## Build & Flash

Make sure ESP-IDF is installed and activated.

### Set target (if not already done):
```bash
idf.py set-target esp32
```

### Build:
```bash
idf.py build
```

### Flash (example port):
```bash
idf.py -p COM6 flash
```

### Monitor:
```bash
idf.py -p COM6 monitor
```

## Hardware Notes

### LED
- GPIO: **33**
- LED type: **active-high**
- Recommended series resistor: **220–330 Ω**
- LED behavior is handled in the internal LED task in `main.c`.

### BLE
- Designed to pair with the shears firmware advertising as **WM-SHEARS**.
- The base must remain powered to maintain the link.

## Next Steps

- Add GATT discovery and subscription to telemetry characteristics.
- Implement packet handling and parsing.
- Add connection timeout/backoff logic.
- Integrate with higher-level system logic once telemetry is defined.
