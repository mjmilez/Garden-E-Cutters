# Shears Firmware

The shears firmware runs on an ESP32 and implements the BLE peripheral, GPS logger, and status LED functionality for the Garden E-Cutters project. It advertises as a BLE device, accepts a connection from the base station, logs GPS points from a UART-connected GPS module, and exposes connection / logging state through a status LED.

## Features

### 1. BLE Peripheral (NimBLE)
- Advertises as `WM-SHEARS`.
- Broadcasts a custom 16-bit service UUID: `0xFFF0`.
- Accepts connections from the base station (central).
- Automatically restarts advertising after disconnects or failed connection attempts.
- Reports connection state to the application layer via callback.

### 2. Status LED (GPIO 33)
Implemented in `shears_led.c`.

| State | Behavior |
|-------|----------|
| Advertising / Waiting for Connection | Fast blink |
| Connected to Base | Solid ON |
| Disconnected | Blink again |

The LED subsystem runs its own FreeRTOS task and does not block BLE or GPS logic.

### 3. GPS Logger (UART2 + SPIFFS + Button)
Fully modularized in `gps_logger.c`.

Functionality:
- Reads NMEA sentences from GPS over UART2 (GPIO16/17 at 9600 baud).
- Builds full NMEA lines in a background task.
- Maintains the most recent NMEA sentence.
- On command, parses `$GPGGA` and stores CSV data to SPIFFS:
  `/spiffs/gps_points.csv`
- Hardware “Save Point” button on GPIO23 (falling-edge interrupt).
- Software save also available via:
  `gpsLoggerRequestSave();`

CSV fields:
```
utc_time, latitude, longitude, fix_quality, num_satellites,
hdop, altitude, geoid_height
```

## Project Structure
```
shears-fw/
├── main/
│   ├── main.c
│   ├── shears_led.c/.h
│   ├── shears_ble.c/.h
│   ├── gps_logger.c/.h
│   └── CMakeLists.txt
└── partitions.csv
```

## Partition Table
```
nvs,        data, nvs,     0x9000,   0x5000
phy_init,   data, phy,     0xE000,   0x1000
factory,    app,  factory, 0x10000,  0x140000
storage,    data, spiffs,            0x64000
```

## Build & Flash

Set target:
```
idf.py set-target esp32
```

Build:
```
idf.py build
```

Flash:
```
idf.py -p COM5 flash
```

Monitor:
```
idf.py -p COM5 monitor
```

## Hardware Notes

### Status LED
- GPIO: 33
- Active-high
- 220–330 Ω resistor required

### GPS UART
- RX → GPIO 16
- TX → GPIO 17
- 9600 baud

### Button
- GPIO 23
- Internal pull-up
- Falling-edge interrupt

### SPIFFS
- Mounted at `/spiffs`
- Stores `/spiffs/gps_points.csv`

## Next Steps
- Add GATT services for telemetry
- Add remote GPS snapshot commands
- Implement low-power sleep
- Add packet buffering and CRC

