# Garden E-Cutters – Instrumented Shears

## Overview
The Garden E-Cutters shears are a smart harvesting tool that records each cut event along with high-accuracy GPS data. When a cut is detected, the system captures an RTK-corrected GNSS position, logs the event locally, and transmits it to a base station for visualization on a live map dashboard.

## Key Features
- RTK GNSS positioning (u-blox ZED-F9P) for centimeter-level accuracy  
- Hall effect sensor-based cut detection  
- ESP32-based embedded control system  
- Local CSV logging for offline reliability  
- Bluetooth Low Energy (BLE) data transfer to base station  
- Audible and LED feedback for user interaction  
- Debounced trigger logic to prevent duplicate cut events  

## How It Works
1. A cut is detected using a Hall effect sensor on the shears.  
2. The ESP32 captures the next valid GNSS (GNGGA) message from the ZED-F9P.  
3. The event (timestamp + GPS + fix data) is stored locally in a CSV buffer.  
4. When connected, data is transmitted over BLE to the base station.  
5. The base system visualizes cut locations on a map dashboard.

## Hardware
- ESP32 microcontroller  
- u-blox ZED-F9P RTK GNSS receiver  
- Hall effect sensor (cut detection)  
- Piezo buzzer (status feedback)  
- Status LEDs  
- Battery-powered field enclosure  

## Design Focus
- Low-latency cut-to-log workflow  
- Reliable operation under intermittent connectivity  
- Minimal power and compute load on the field device  
- Robust data capture even during GNSS or BLE interruptions  

## Status
Prototype validated in field-like testing with reliable event detection, RTK-level positioning, and stable end-to-end data transfer to the base system.
