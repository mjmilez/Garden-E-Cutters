#!/bin/bash
set -e
repoDir="/home/pi/Garden-E-Cutters/base-rpi-fw"
branchName="main"
echo "[boot] entering repo directory..."
cd "$repoDir"

# Handle network mode (hotspot or AP)
bash "$repoDir/start_network.sh"

sleep 5

# If we have internet, sync git
if ping -c 1 -W 3 8.8.8.8 > /dev/null 2>&1; then
    echo "[boot] Syncing to origin/$branchName..."
    git fetch origin
    git checkout "$branchName"
    git reset --hard "origin/$branchName"
else
    echo "[boot] No internet, skipping git update..."
fi

echo "[boot] starting app.py..."
exec /usr/bin/python3 "$repoDir/app.py"