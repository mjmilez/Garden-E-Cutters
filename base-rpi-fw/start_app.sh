#!/bin/bash
set -e

repoDir="/home/pi/garden-e-cutters/base-rpi-fw"
branchName="main"

echo "[boot] entering repo directory..."
cd "$repoDir"

maxAttempts=5
attempt=1
connected=false

while [ $attempt -le $maxAttempts ]; do
	echo "[boot] checking internet connection (attempt $attempt/$maxAttempts)..."
	if ping -c 1 -W 3 8.8.8.8 > /dev/null 2>&1; then
		connected=true
		break
	fi

	attempt=$((attempt + 1))
	sleep 3
done

if [ "$connected" = true ]; then
	echo "[boot] internet available, syncing to origin/$branchName..."
	git fetch origin
	git checkout "$branchName"
	git reset --hard "origin/$branchName"
else
	echo "[boot] internet not available, skipping git update..."
fi

echo "[boot] starting app.py..."
exec /usr/bin/python3 "$repoDir/app.py"