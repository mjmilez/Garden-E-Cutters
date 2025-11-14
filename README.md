# Garden E‑Cutters Firmware (BLE-v1)

This branch contains the first working BLE link between the **shears** and the **base station**.  
Both devices run ESP-IDF with the NimBLE stack and implement the foundational communication layer for the E‑Cutters system.  
The goal of BLE‑v1 is to establish a stable connection, define the structure for future data exchange, and prepare for telemetry transport.

---

## High‑Level Overview

### Shears Firmware (Peripheral)
The shears firmware runs as a **BLE peripheral** that:
- Advertises as `WM-SHEARS`
- Publishes a primary BLE service (`0xFFF0`)
- Accepts incoming connections from the base station
- Restarts advertising automatically if the connection is lost

A status LED is connected to **GPIO33**:
- Fast blink while advertising / waiting for a connection  
- Solid on when connected  
- Returns to blinking after disconnect  

This LED gives immediate visual feedback on the BLE link state.

The shears currently do **not** send telemetry yet, but the structure is ready for the next step:
- A fixed-size telemetry struct
- A characteristic for data publishing
- A periodic task to generate and push packets through notify once implemented

---

### Base Firmware (Central)
The base station firmware runs as a **BLE central** that:
- Scans continuously for devices named `WM-SHEARS`
- Filters advertisement packets by device name
- Initiates a BLE connection when the shears are discovered
- Restarts scanning automatically if the link fails or drops

The base also uses **GPIO33** for a status LED:
- Fast blink while scanning / connecting  
- Solid on when connected to the shears  

This matches the shears’ LED behavior and helps confirm link status.

The base currently performs:
- Active scanning
- Name-based filtering
- Manual connect attempts
- Logging of discovered and connected devices

Telemetry discovery and notification handling will be added in the next milestone.

---

## Next Steps for BLE-v1 → BLE-v2

The next development step is adding **real, structured communication**.  
The plan is already defined:

### 1. Define a telemetry record
Example structure:

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

For BLE‑v1 → BLE‑v2 transition, a simplified version will be used for bring-up.

### 2. Add a GATT characteristic on the shears
- Service UUID: `0xFFF0` (already advertising)
- Telemetry Characteristic UUID: `0xFFF1`
- Properties: `READ | NOTIFY`

### 3. Shears: periodic telemetry task
- Update struct fields (e.g., increment `seq`)
- Send notify if connected + notifications enabled

### 4. Base: service + characteristic discovery
- Discover `0xFFF0` service and `0xFFF1` characteristic
- Enable notifications through CCCD
- Log incoming packets

This forms the **shears → base** telemetry channel.

After this is stable, a command/control characteristic (`0xFFF2`) can be added for base → shears messages.

---

## Current Project Structure

```
BLE-v1/
├── shears-fw/
│   ├── main/
│   │   └── main.c
│   └── README.md
├── base-fw/
│   ├── main/
│   │   └── main.c
│   └── README.md
└── README.md   <-- (this file)
```

---

## Build & Flash (Both Firmware Targets)

Activate ESP-IDF first.

### Build
```bash
idf.py build
```

### Flash (example)
```bash
idf.py -p COM5 flash
```

### Monitor
```bash
idf.py -p COM5 monitor
```

---

## Summary

BLE-v1 establishes:
- Reliable BLE discovery
- Central ↔ Peripheral connection flow
- Automatic reconnection logic
- Status LEDs on both devices
- Advertising + scanning foundation
- Initial layout for future GATT services
- Clear roadmap for telemetry communication

This branch now forms the baseline for the full BLE data link used in the Garden E‑Cutters system.

