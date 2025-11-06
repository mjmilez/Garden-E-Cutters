# Shears Firmware (Peripheral)

ESP32 firmware for the handheld shears.

This module acts as a **BLE Peripheral (Server)** that:
- Advertises itself as "E-Cutters Shears"
- Provides a single GATT Service with one Telemetry Characteristic
- Periodically samples sensors and sends telemetry frames with CRC validation

Version: v1 — one-way BLE data stream only (no ACKs yet)
