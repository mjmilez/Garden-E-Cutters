# Base Station Firmware (Central)

ESP32 firmware for the stationary base hub.

This module acts as a **BLE Central (Client)** that:
- Scans for the E-Cutters Shears device
- Connects and subscribes to the Telemetry Characteristic
- Receives and verifies telemetry frames using CRC

Version: v1 — one-way BLE data reception only
