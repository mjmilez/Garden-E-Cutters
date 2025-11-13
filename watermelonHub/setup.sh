#!/bin/bash
echo "Setting up ESP-IDF environment..."
. ~/esp/esp-idf/export.sh

# Check if USB device is present
if [ -e /dev/ttyUSB0 ]; then
    echo "ESP32 found at /dev/ttyUSB0"
else
    echo "ESP32 not found! Please attach using:"
    echo "  Windows: usbipd attach --wsl --busid <busid>"
    echo "  Then check: ls /dev/ttyUSB*"
fi

echo "Ready! You can now use:"
echo "  idf.py build"
echo "  idf.py -p /dev/ttyUSB0 flash monitor"
