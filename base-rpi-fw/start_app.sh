#!/bin/bash
set -e

repoDir="/home/pi/Garden-E-Cutters/base-rpi-fw"
branchName="main"

echo "[boot] entering repo directory..."
cd "$repoDir"

echo "[boot] waiting for network..."
sleep 5

echo "[boot] checking internet connectivity..."
if ping -c 1 -W 3 8.8.8.8 > /dev/null 2>&1; then
	echo "[boot] internet available, pulling latest code from GitHub..."
	git pull origin "$branchName"
else
	echo "[boot] no internet connection, skipping git pull..."
fi

echo "[boot] starting app.py..."
exec /usr/bin/python3 "$repoDir/app.py"