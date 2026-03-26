#!/bin/bash
# start_network.sh
# If a known WiFi network is available, connect to it (dev mode).
# Otherwise, start the AP (field mode).

HOTSPOT_SSID="Josh's phone"
AP_IP="192.168.4.1"
MAX_RETRIES=5
RETRY_DELAY=3

echo "[net] Stopping AP services..."
sudo systemctl stop hostapd 2>/dev/null || true
sudo systemctl stop ap-static-ip 2>/dev/null || true
sudo ip addr flush dev wlan0 2>/dev/null || true

# Wait for wlan0 to be managed by NetworkManager
echo "[net] Waiting for WiFi radio..."
for i in $(seq 1 $MAX_RETRIES); do
    STATE=$(nmcli -t -f DEVICE,STATE device 2>/dev/null | grep "^wlan0:" | cut -d: -f2)
    if [ "$STATE" != "" ] && [ "$STATE" != "unmanaged" ] && [ "$STATE" != "unavailable" ]; then
        echo "[net] wlan0 is ready (state: $STATE)"
        break
    fi
    echo "[net] wlan0 not ready yet (attempt $i/$MAX_RETRIES, state: $STATE)..."
    sleep $RETRY_DELAY
done

# Scan with retries — radio may need a moment to see nearby SSIDs
echo "[net] Scanning for known networks..."
FOUND=0
for i in $(seq 1 $MAX_RETRIES); do
    SCAN=$(nmcli -t -f SSID device wifi list --rescan yes 2>/dev/null)
    if echo "$SCAN" | grep -qF "$HOTSPOT_SSID"; then
        FOUND=1
        echo "[net] Found '$HOTSPOT_SSID' on attempt $i"
        break
    fi
    echo "[net] Scan $i/$MAX_RETRIES — not found yet..."
    sleep $RETRY_DELAY
done

if [ $FOUND -eq 1 ]; then
    echo "[net] Connecting to '$HOTSPOT_SSID'..."
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