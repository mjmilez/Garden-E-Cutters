# Shears Firmware

The shears firmware runs on an ESP32 using ESP-IDF and provides the BLE peripheral (server) side of the Garden E-Cutters wireless link. It advertises a connectable BLE service, accepts incoming connections from the base station, and exposes a simple connection status indicator through a GPIO-driven LED.

## Features

### BLE Peripheral
- Uses ESP-IDF’s NimBLE stack.
- Advertises as **`WM-SHEARS`**.
- Broadcasts a custom 16-bit service UUID: **`0xFFF0`**.
- Accepts connections from the base ESP32.
- Automatically restarts advertising after disconnects or failed connection attempts.

### Status LED (GPIO33)
- LED connected to **GPIO 33**.
- **Fast blink** while advertising or waiting for a connection.
- **Solid on** when a BLE connection is active.
- Returns to blinking if the connection drops.

## Project Structure

```
shears-fw/
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
idf.py -p COM5 flash
```

### Monitor:
```bash
idf.py -p COM5 monitor
```

## Hardware Notes

### LED
- GPIO: **33**
- LED type: **active-high**
- Series resistor recommended (≈220–330 Ω).
- LED behavior is controlled by the LED task in `main.c`.

### BLE
- Works with ESP32 modules supporting the NimBLE stack.
- Must be powered for the base unit to connect.

## Next Steps

- Add a custom GATT service for telemetry packets (force, position, battery, etc.).
- Implement packet buffering for intermittent connections.
- Add power management (sleep/wake).
- Define packet structure and CRC fields.
