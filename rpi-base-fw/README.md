# base-fw-rpi

Raspberry Pi runtime for the Base Firmware.

Runs as a systemd-managed service and behaves like firmware (auto-start,
auto-restart, journald logging).

------------------------------------------------------------------------

## Install Build Tools

``` bash
sudo apt update
sudo apt install -y build-essential cmake
```

------------------------------------------------------------------------

## Build

``` bash
mkdir -p build
cd build
cmake ..
make -j
```

Binary output:

    build/base-fw-rpi

------------------------------------------------------------------------

## Run Manually (Development)

``` bash
./build/base-fw-rpi
```

Stop with:

    Ctrl + C

------------------------------------------------------------------------

## Install as Service (Firmware Mode)

### Install Binary

``` bash
sudo install -m 0755 ./build/base-fw-rpi /usr/local/bin/base-fw-rpi
```

------------------------------------------------------------------------

### Create Service File

Create:

    /etc/systemd/system/base-fw-rpi.service

Contents:

``` ini
[Unit]
Description=Base FW (Raspberry Pi)
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/base-fw-rpi
Restart=always
RestartSec=2
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

------------------------------------------------------------------------

### Enable + Start

``` bash
sudo systemctl daemon-reload
sudo systemctl enable base-fw-rpi.service
sudo systemctl start base-fw-rpi.service
```

------------------------------------------------------------------------

## View Logs (Serial Monitor Equivalent)

``` bash
sudo journalctl -u base-fw-rpi.service -f
```
