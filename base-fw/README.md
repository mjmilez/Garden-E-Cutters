# Base Firmware

The base firmware runs on an ESP32 using ESP-IDF and acts as the **BLE central (client)** for the Garden E-Cutters system. It scans for the shears device, connects when it is found, and drives a status LED to indicate link state.

---

## Features

### 1. BLE Central (NimBLE)

Implemented in `base_ble.c`.

- Uses ESP-IDF’s NimBLE stack as a BLE **central**.
- Scans for devices advertising the name **`WM-SHEARS`**.
- Filters advertisements by device name to avoid unnecessary connections.
- Initiates a BLE connection when the shears device is discovered.
- Automatically restarts scanning after:
  - disconnects
  - failed connection attempts
  - completed scan windows
- Reports connection state to the application layer via a callback.

---

### 2. Status LED (GPIO 33)

Implemented in `base_led.c`.

LED behavior:

| State                                      | Behavior   |
|-------------------------------------------|-----------|
| Scanning / Attempting to Connect          | Fast blink |
| Connected to Shears                       | Solid ON   |
| Disconnected / Link Lost                  | Blink again |

The LED logic runs in its own FreeRTOS task and is controlled through a small API (`baseLedSetBlinking`, `baseLedSetSolidOn`, etc.) so it stays decoupled from BLE internals.

---

## Project Structure

The base firmware is organized into small, focused modules:

```text
base-fw/
├── main/
│   ├── main.c          # High-level wiring: LED + BLE init and callbacks
│   ├── base_led.c/.h   # Status LED subsystem (GPIO33 + blink task)
│   ├── base_ble.c/.h   # BLE central logic (scan + connect to WM-SHEARS)
│   └── CMakeLists.txt
└── CMakeLists.txt      # Top-level ESP-IDF project configuration
```

`main.c` is intentionally minimal and only wires modules together:
- Initializes the LED subsystem
- Initializes BLE central logic
- Updates LED state when the BLE connection state changes

---

## Build & Flash

Make sure ESP-IDF is installed and the environment is activated.

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

Use the appropriate serial port for your system instead of `COM6`.

---

## Hardware Notes

### Status LED

- GPIO: **33**
- LED type: **active-high**
- Recommended series resistor: **220–330 Ω**
- Behavior:
  - Blinks while scanning / attempting connection
  - Solid ON when connected
- All timing and behavior are managed by `base_led.c`.

### BLE Link

- Designed to pair with the shears firmware advertising as **`WM-SHEARS`**.
- The base must remain powered to maintain the BLE connection.
- Uses NimBLE in central mode; no GATT services are exposed yet by the base.

---

## Next Steps

Possible future extensions:

- Perform GATT service / characteristic discovery on the shears.
- Subscribe to telemetry characteristics (force, position, battery, GPS triggers, etc.).
- Implement packet handling, parsing, and validation (CRC / sequence counters).
- Add connection timeout/backoff logic and more sophisticated reconnection policies.
- Integrate with higher-level application logic once telemetry and command formats are defined.
