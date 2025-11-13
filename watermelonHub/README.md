# Garden E-Cutters: Watermelon Harvest Tracking System

Senior Design Project - Computer Engineering

## Project Overview
Automated watermelon harvesting tracker using ESP32 with RTK GPS positioning and BLE communication.

## Important: Directory Structure Requirements

⚠️ **CRITICAL**: The ESP-IDF framework expects projects to be in a specific location for easiest development.

### Recommended Setup (Easiest)
Place this project in your `~/esp` directory alongside ESP-IDF:
```
~/esp/
├── esp-idf/           # ESP-IDF framework (install first)
├── Garden-E-Cutters/  # This repository
│   └── watermelonHub/ # The actual ESP32 project
```

## Setup Instructions

### Step 1: Install ESP-IDF (One-time setup)
```bash
# Create esp directory in your home folder
mkdir -p ~/esp
cd ~/esp

# Clone and install ESP-IDF
git clone -b v5.1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh
```

### Step 2: Clone This Repository
```bash
# Go back to esp directory
cd ~/esp

# Clone our project
git clone https://github.com/mjmilez/Garden-E-Cutters.git
cd Garden-E-Cutters
```

### Step 3: Build the Hub
```bash
# Navigate to the hub project
cd ~/esp/Garden-E-Cutters/watermelonHub

# IMPORTANT: Set up ESP-IDF environment (needed for every new terminal session)
. ~/esp/esp-idf/export.sh

# Configure for ESP32
idf.py set-target esp32

# Build the project
idf.py build

# Flash to ESP32 (adjust port as needed)
idf.py -p /dev/ttyUSB0 flash monitor
```

## For WSL Users
If using Windows Subsystem for Linux, you need to attach the USB device:
```powershell
# In Windows PowerShell (Admin)
usbipd list
usbipd attach --wsl --busid <your-device-busid>
```

## Project Structure
```
watermelonHub/
├── main/                      # Core hub logic
│   ├── main.cpp              # Entry point
│   ├── hub_controller.cpp    # Main orchestrator
│   └── interfaces.h          # Component interfaces
├── components/
│   ├── ble_handler/          # BLE communication (YOUR WORK AREA if doing BLE)
│   ├── data_manager/         # CSV data storage
│   └── web_server/           # Web dashboard (YOUR WORK AREA if doing web)
├── CMakeLists.txt
├── partitions.csv            # Flash memory layout
└── sdkconfig.defaults        # Default ESP32 configuration
```

## Team Development Workflow

### Working on Your Component
1. **Create your own branch**:
```bash
   git checkout -b your-feature-name
```

2. **Find your work area**:
   - **BLE Integration**: Edit `components/ble_handler/ble_stub.cpp`
   - **Web Dashboard**: Edit `components/web_server/web_stub.cpp`
   - **Hub Logic**: Edit `main/hub_controller.cpp`

3. **Test your changes**:
```bash
   cd ~/esp/Garden-E-Cutters/watermelonHub
   . ~/esp/esp-idf/export.sh
   idf.py build
   idf.py -p /dev/ttyUSB0 flash monitor
```

4. **Push your branch**:
```bash
   git add .
   git commit -m "Description of your changes"
   git push origin your-branch-name
```

## Current Implementation Status

- ✅ Hub controller architecture
- ✅ Data manager (CSV storage on SPIFFS)
- 🔄 BLE handler (stub implementation - needs real BLE code)
- 🔄 Web server (stub implementation - needs real HTTP server)
- ⏳ WiFi AP mode (for offline dashboard access)
- ⏳ RTK GPS integration on shears

## Hardware Requirements
- ESP32 DevKit (2x minimum)
  - One for hub (receives and stores data)
  - One for shears (with GPS and force sensor)
- RTK GPS module (ZED-F9P or similar)
- Force sensor/strain gauge
- USB cables for programming

## Troubleshooting

### "Command not found: idf.py"
You forgot to source the export script:
```bash
. ~/esp/esp-idf/export.sh
```

### Permission denied on /dev/ttyUSB0
```bash
sudo chmod 666 /dev/ttyUSB0
```

### Can't find ESP32 (WSL users)
Make sure you attached it from Windows using usbipd

## Team Members
- Alessandro - Hub implementation & system integration
- Matthew - Project lead
- Josh - [Component]
- Christian - [Component]
- Alejandro - [Component]

## Questions?
Check the Pre-Alpha Build PDF in the repo for system architecture details.

