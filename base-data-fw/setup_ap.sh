#!/bin/bash
# setup_ap.sh
#
# One-time WiFi Access Point setup for Raspberry Pi 4.
# Creates a "WatermelonHub" WiFi network that field devices
# (phones, laptops) connect to for accessing the dashboard.
#
# Run once with: sudo bash setup_ap.sh
# After reboot, the Pi will broadcast the WiFi network automatically.

set -e

SSID="WatermelonHub"
PASSWORD="harvest123"
IP_ADDR="192.168.4.1"
DHCP_RANGE_START="192.168.4.10"
DHCP_RANGE_END="192.168.4.50"

echo "=== Watermelon Hub WiFi AP Setup ==="
echo "SSID:     $SSID"
echo "Password: $PASSWORD"
echo "IP:       $IP_ADDR"
echo ""

# Install required packages
echo "[1/4] Installing hostapd and dnsmasq..."
apt-get update
apt-get install -y hostapd dnsmasq

# Stop services while configuring
systemctl stop hostapd 2>/dev/null || true
systemctl stop dnsmasq 2>/dev/null || true

# Configure static IP for wlan0
echo "[2/4] Configuring static IP..."
cat >> /etc/dhcpcd.conf <<EOF

# Watermelon Hub AP - static IP for wlan0
interface wlan0
    static ip_address=${IP_ADDR}/24
    nohook wpa_supplicant
EOF

# Configure hostapd
echo "[3/4] Configuring hostapd..."
cat > /etc/hostapd/hostapd.conf <<EOF
interface=wlan0
driver=nl80211
ssid=${SSID}
hw_mode=g
channel=7
wmm_enabled=0
macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_passphrase=${PASSWORD}
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
EOF

# Point hostapd to its config
sed -i 's|^#DAEMON_CONF=.*|DAEMON_CONF="/etc/hostapd/hostapd.conf"|' /etc/default/hostapd

# Configure dnsmasq (DHCP server)
echo "[4/4] Configuring dnsmasq..."
mv /etc/dnsmasq.conf /etc/dnsmasq.conf.bak 2>/dev/null || true
cat > /etc/dnsmasq.conf <<EOF
interface=wlan0
dhcp-range=${DHCP_RANGE_START},${DHCP_RANGE_END},255.255.255.0,24h
domain=local
address=/watermelonhub.local/${IP_ADDR}
EOF

# Enable and start services
systemctl unmask hostapd
systemctl enable hostapd
systemctl enable dnsmasq

echo ""
echo "=== Setup complete! ==="
echo "Reboot the Pi to start the WiFi AP:"
echo "  sudo reboot"
echo ""
echo "After reboot:"
echo "  1. Connect to WiFi: ${SSID} (password: ${PASSWORD})"
echo "  2. Open browser: http://${IP_ADDR}"
echo "  3. Dashboard should load automatically"