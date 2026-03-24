#!/bin/bash
# start_network.sh
# If a known WiFi network is available, connect to it (dev mode).
# Otherwise, start the AP (field mode).

HOTSPOT_SSID="Josh's phone"
AP_IP="192.168.4.1"

echo "[net] Stopping AP services..."
sudo systemctl stop hostapd 2>/dev/null || true
sudo systemctl stop ap-static-ip 2>/dev/null || true
sudo ip addr flush dev wlan0 2>/dev/null || true

sleep 2

echo "[net] Scanning for known networks..."
SCAN=$(nmcli -t -f SSID device wifi list --rescan yes 2>/dev/null)

if echo "$SCAN" | grep -qF "$HOTSPOT_SSID"; then
    echo "[net] Found '$HOTSPOT_SSID' — connecting..."
    nmcli device wifi connect "$HOTSPOT_SSID"
    if [ $? -eq 0 ]; then
        echo "[net] Connected to hotspot. Dev mode active."
        exit 0
    fi
    echo "[net] Connection failed, falling back to AP mode..."
fi

echo "[net] Starting AP mode..."
sudo systemctl start hostapd
sleep 1
sudo ip addr add $AP_IP/24 dev wlan0 2>/dev/null || true
sudo systemctl restart dnsmasq
echo "[net] AP mode active: WatermelonHub @ $AP_IP"