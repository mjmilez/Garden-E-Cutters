# Garden E-Cutters: Watermelon Harvest Tracking System
## Senior Design Project - Computer Engineering

## 🚀 Quick Start for Team Members

### Get Alessandro's BLE Integration Branch
```bash
git clone https://github.com/mjmilez/Garden-E-Cutters.git
cd Garden-E-Cutters
git checkout hub-ble-integration  # Alessandro's branch with Josh's BLE integrated
```

---

## Project Overview
Automated watermelon harvesting tracker using dual ESP32 system:
- **Hub ESP32**: Creates WiFi AP, runs web dashboard, receives harvest data via BLE
- **Shears ESP32**: Collects GPS coordinates and force sensor data, transmits via BLE

## System Architecture
```
┌─────────────┐  BLE Transfer    ┌─────────────┐  WiFi (192.168.4.1)   ┌────────────┐
│  Shears     │ ───────────────> │  Hub        │ <──────────────────>  │  Phone/PC  │
│  (ESP32)    │                  │  (ESP32)    │                       │  Browser   │
│  GPS+Force  │                  │  Storage    │                       │  Dashboard │
└─────────────┘                  └─────────────┘                       └────────────┘
```

---

## 📋 Prerequisites

### Required Software
1. **ESP-IDF v5.1.2** (MUST be this version for NimBLE support)
2. **Git**
3. **Python 3.8+**
4. **USB drivers for your ESP32 board**

### Hardware Requirements
- 2x ESP32 DevKit boards (with 4MB flash)
- USB cables for programming
- (For shears) ZED-F9P GPS module + force sensor

---

## 🛠️ Complete Setup Instructions

### Step 1: Install ESP-IDF (if not already installed)

#### For Linux/WSL:
```bash
# Install prerequisites
sudo apt-get update
sudo apt-get install git wget flex bison gperf python3 python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

# Create esp directory
mkdir -p ~/esp
cd ~/esp

# Clone ESP-IDF v5.1.2 specifically (IMPORTANT: Must be v5.1.2)
git clone -b v5.1.2 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32
```

#### For Windows (using ESP-IDF Tools Installer):
1. Download from: https://dl.espressif.com/dl/esp-idf-tools-setup-online-5.1.2.exe
2. Run installer, select ESP32 target
3. Use ESP-IDF PowerShell or Command Prompt

#### For macOS:
```bash
# Install prerequisites
brew install cmake ninja dfu-util

# Follow Linux instructions above
```

#### For VS Code Users:
1. Install "Espressif IDF" extension
2. Use Command Palette → "ESP-IDF: Configure ESP-IDF Extension"
3. Select ESP-IDF v5.1.2
4. Set target to ESP32

---

### Step 2: Clone and Checkout the BLE Integration Branch

```bash
cd ~/esp  # Or your preferred workspace
git clone https://github.com/mjmilez/Garden-E-Cutters.git
cd Garden-E-Cutters
git fetch origin
git checkout hub-ble-integration  # Alessandro's branch with BLE
```

---

### Step 3: Configure ESP32 Settings (IMPORTANT!)

```bash
# Navigate to hub project
cd watermelonHub

# Set up ESP-IDF environment (REQUIRED for each new terminal)
# Linux/macOS/WSL:
. ~/esp/esp-idf/export.sh

# Windows ESP-IDF PowerShell:
# (environment is already set up)

# Clean any previous builds
rm -rf build sdkconfig

# Open menuconfig to verify settings
idf.py menuconfig
```

#### In menuconfig, verify these settings:
1. **Serial flasher config** → 
   - Flash size: **4MB**
   - Flash mode: **DIO**
   - Flash frequency: **40MHz**
   
2. **Component config** → **Bluetooth** → **Host** → 
   - Select **NimBLE - BLE only** (should already be enabled)
   
3. **Component config** → **Serial flasher config** →
   - Default baud rate: **115200** (IMPORTANT: Use this for flashing)

Press **S** to Save, **Q** to Quit

---

### Step 4: Build the Project

```bash
# Build the project
idf.py build
```

If the build succeeds, you'll see:
```
Project build complete. To flash, run this command:
```

---

### Step 5: Flash to ESP32

```bash
# Find your serial port
# Linux/WSL: /dev/ttyUSB0 or /dev/ttyACM0
# macOS: /dev/cu.usbserial-* or /dev/cu.SLAB_USBtoUART
# Windows: COM3, COM4, etc.

# Flash with 115200 baud rate (IMPORTANT: Use -b 115200)
idf.py -p YOUR_PORT -b 115200 flash monitor

# Examples:
idf.py -p /dev/ttyUSB0 -b 115200 flash monitor  # Linux/WSL
idf.py -p COM3 -b 115200 flash monitor          # Windows
idf.py -p /dev/cu.usbserial-0001 -b 115200 flash monitor  # macOS
```

**Note**: We use 115200 baud rate to avoid flashing issues. Higher rates may cause errors.

To exit monitor: `Ctrl+]`

---

## 🔧 Troubleshooting Guide

### Common Build Errors

#### 1. "format '%d' expects argument of type 'int', but argument has type 'uint32_t'"
**Already fixed in hub-ble-integration branch!** But if you see this elsewhere:
```c
// Change from:
ESP_LOGI(TAG, "Value: %d", some_uint32);
// To:
ESP_LOGI(TAG, "Value: %lu", (unsigned long)some_uint32);
```

#### 2. "Failed to resolve component 'nimble'" or "host/ble_hs.h: No such file or directory"
NimBLE needs to be enabled in menuconfig:
```bash
idf.py menuconfig
# Navigate: Component config → Bluetooth → Host → NimBLE - BLE only
# Enable it, Save (S), Quit (Q)
idf.py build
```

#### 3. "app partition is too small for binary"
The partition table is already fixed in the branch, but if you encounter this:
```bash
# Check partition table
cat partitions.csv

# Should show:
# factory,  app,  factory, 0x10000,  2M,
# storage,  data, spiffs,  0x210000, 1M,
```

#### 4. Flash size detection issues
If you get flash size errors:
```bash
idf.py menuconfig
# Serial flasher config → Flash size → 4MB
# Save and rebuild
```

#### 5. "Command not found: idf.py"
```bash
# You forgot to source the environment
. ~/esp/esp-idf/export.sh
```

#### 6. "Permission denied on /dev/ttyUSB0" (Linux/WSL)
```bash
sudo chmod 666 /dev/ttyUSB0
# OR add yourself to dialout group (permanent fix):
sudo usermod -a -G dialout $USER
# Then logout and login again
```

#### 7. Flashing fails or corrupted flash
Always use the 115200 baud rate:
```bash
idf.py -p YOUR_PORT -b 115200 flash
```

#### 8. "CONFIG_BLE_SMP_ENABLE was replaced with CONFIG_BT_BLE_SMP_ENABLE"
This warning can be ignored - it's already handled by the build system.

---

### WSL-Specific Setup

#### Installing USB support in WSL:
```powershell
# In Windows PowerShell (Admin)
# Install usbipd if not already installed
winget install usbipd

# List USB devices
usbipd list

# Attach ESP32 to WSL (find your device in the list)
usbipd attach --wsl --busid <BUSID>

# Example:
usbipd attach --wsl --busid 2-4
```

Then in WSL:
```bash
# Verify device is available
ls /dev/ttyUSB*

# If not visible, you may need to:
sudo apt install linux-tools-generic hwdata
sudo update-alternatives --install /usr/local/bin/usbip usbip /usr/lib/linux-tools/*-generic/usbip 20
```

---

## 📁 Project Structure

```
watermelonHub/
├── main/
│   ├── main.cpp                 # Entry point - initializes WiFi, BLE, components
│   ├── hub_controller.cpp       # Orchestrates data flow between components
│   ├── base_ble.c              # Josh's BLE central implementation
│   ├── base_led.c              # Status LED control
│   ├── log_transfer_client.c   # BLE file transfer client
│   └── log_transfer_protocol.h # BLE protocol definitions
├── components/
│   ├── ble_handler/            # BLE interface (stub for demo)
│   ├── data_manager/           # CSV storage on SPIFFS
│   └── web_server/             # HTTP dashboard server
├── partitions.csv              # Flash layout (2MB app, 1MB storage)
├── sdkconfig.defaults          # NimBLE and project configuration
└── CMakeLists.txt             # Build configuration
```

---

## 🔄 System Operation

### Hub Device (This Code)
1. **Creates WiFi Access Point**: SSID: "WatermelonHub", IP: 192.168.4.1
2. **Scans for BLE Device**: Looking for "WM-SHEARS"
3. **Downloads GPS Log**: Automatically retrieves `/spiffs/gps_points.csv`
4. **Serves Web Dashboard**: http://192.168.4.1 shows harvest data

### Shears Device (Josh's Code - branch: `feature/ble-v1-oneway`)
1. **Logs GPS Data**: Reads from ZED-F9P, saves to SPIFFS
2. **Advertises via BLE**: Device name "WM-SHEARS"
3. **Transfers Files**: Sends GPS log when hub requests

### LED Status Indicators
- **Blinking**: Scanning for shears (hub) or advertising (shears)
- **Solid**: Connected via BLE
- **Off**: Idle/Error

---

## 🌐 Web Dashboard Access

After flashing the hub:
1. Connect to WiFi network: **"WatermelonHub"** (no password)
2. Open browser, navigate to: **http://192.168.4.1**
3. Dashboard shows:
   - Harvest events list
   - Map with GPS markers
   - CSV download option
   - Real-time updates (simulated data for now)

---

## 👥 Development Workflow

### Making Changes
```bash
# Create your feature branch
git checkout -b feature/your-feature

# Make changes
# ...edit files...

# Test locally
idf.py build
idf.py -p YOUR_PORT -b 115200 flash monitor

# Commit and push
git add .
git commit -m "Description of changes"
git push origin feature/your-feature
```

### Merging BLE Integration
When ready to merge Alessandro's BLE work:
```bash
git checkout main
git pull origin main
git merge hub-ble-integration
# Resolve any conflicts
git push origin main
```

---

## ⚡ Quick Commands Reference

```bash
# Environment setup (every new terminal)
. ~/esp/esp-idf/export.sh

# Full clean, build, flash, monitor sequence
idf.py fullclean && idf.py build && idf.py -p /dev/ttyUSB0 -b 115200 flash monitor

# Individual commands
idf.py build                           # Build only
idf.py -p /dev/ttyUSB0 -b 115200 flash # Flash only
idf.py -p /dev/ttyUSB0 monitor        # Monitor only
idf.py fullclean                      # Clean everything
idf.py menuconfig                     # Configuration menu
idf.py size                           # Analyze binary size
```

---

## 📊 Memory Configuration
- **Flash Size**: 4MB (set in menuconfig)
- **App Partition**: 2MB (47% used, ~1.06MB binary)
- **SPIFFS Storage**: 1MB for CSV logs
- **RAM Usage**: ~50KB for BLE + WiFi

---

## 🐛 Debug Tips

### Enable Verbose Logging
```bash
idf.py menuconfig
# Component config → Log output → Default log verbosity → Debug
```

### Monitor BLE Connection
Look for these key log messages:
```
I (xxxx) base_ble: Scanning for WM-SHEARS...
I (xxxx) base_ble: Connected to shears!
I (xxxx) log_transfer: Transfer completed. Bytes received: xxxx
```

### Check SPIFFS Files
The hub stores data at `/spiffs/harvest_data.csv`

### Serial Monitor Not Working?
- Make sure no other program is using the port
- Try a different baud rate for monitoring: `idf.py -p YOUR_PORT monitor -b 115200`

---

## 📝 Implementation Status
- ✅ WiFi Access Point ("WatermelonHub")
- ✅ Web Dashboard Server (http://192.168.4.1)
- ✅ BLE Central with NimBLE
- ✅ File Transfer Protocol
- ✅ CSV Data Storage (SPIFFS)
- ✅ LED Status Indicators
- ✅ Format specifier fixes for ESP32
- ✅ Partition table (2MB app + 1MB storage)
- 🔄 Real GPS data integration (using simulated data for now)
- 🔄 Force sensor integration
- ⏳ Real-time dashboard updates via WebSocket

---

## Known Issues & Fixes

1. **Josh's BLE files require NimBLE** - Already enabled in sdkconfig
2. **Format specifiers for uint32_t** - All fixed in this branch
3. **Binary size > 1MB** - Partition table adjusted to 2MB
4. **Flashing at high baud rates fails** - Always use `-b 115200`
5. **ESP-IDF version compatibility** - Must use v5.1.2 for NimBLE

---

## Team Members
- **Alessandro** - Hub implementation, BLE integration, system architecture
- **Josh** - BLE firmware, GPS logging, file transfer protocol  
- **Matthew** - Project lead, Shears dev
- **Christian** - Web UI
- **Alejandro** - Fake ZED

---

## 📚 Additional Resources
- [ESP-IDF v5.1.2 Documentation](https://docs.espressif.com/projects/esp-idf/en/v5.1.2/)
- [NimBLE Documentation](https://mynewt.apache.org/latest/network/ble_hs/)
- [Project Repository](https://github.com/mjmilez/Garden-E-Cutters)

---

## Need Help?
1. Check the troubleshooting section above
2. Make sure you're using ESP-IDF v5.1.2
3. Verify you're using `-b 115200` for flashing
4. Check menuconfig settings (flash size, NimBLE enabled)
5. Contact Alessandro for hub/integration issues
6. Contact Josh for BLE/shears firmware issues

---

**Last Updated**: December 2025  
**Branch**: `hub-ble-integration`  
**ESP-IDF Version**: v5.1.2 (REQUIRED)
**Flash Baud Rate**: 115200 (REQUIRED)
