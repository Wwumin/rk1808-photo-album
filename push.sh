#!/usr/bin/env bash
# Copy everything the board needs: main + bmp/ + lib/libDRMwrap.so
# Usage: ./push.sh <board-ip> [user] [remote-dir]
#   ./push.sh 192.168.1.100
#   ./push.sh 192.168.1.100 root /root/album
set -e
IP="${1:?usage: ./push.sh <board-ip> [user] [remote-dir]}"
USER_NAME="${2:-root}"
DEST="${3:-/root/album}"
HERE="$(cd "$(dirname "$0")" && pwd)"

if [ ! -f "$HERE/main" ]; then
    echo "[FAIL] ./main not found - run ./build.sh first"
    exit 1
fi
if [ ! -d "$HERE/bmp" ]; then
    echo "[FAIL] bmp/ not found - run 'make assets' first (needs python3 + Pillow)"
    exit 1
fi

echo "==> mkdir $DEST and $DEST/lib on $IP"
ssh "$USER_NAME@$IP" "mkdir -p $DEST/lib"

echo "==> copy main"
scp "$HERE/main" "$USER_NAME@$IP:$DEST/"

echo "==> copy bmp/ (about 60 MB, compressed transfer)"
scp -C -r "$HERE/bmp" "$USER_NAME@$IP:$DEST/"

# rpath is ./lib, so dropping the .so into <DEST>/lib always resolves.
if [ -f "$HERE/lib/libDRMwrap.so" ]; then
    echo "==> copy lib/libDRMwrap.so"
    scp "$HERE/lib/libDRMwrap.so" "$USER_NAME@$IP:$DEST/lib/"
fi

echo
echo "[OK] done. On the board run:"
echo "     cd $DEST && ./main"
