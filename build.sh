#!/usr/bin/env bash
# Cross-build the album app for the RK1808 board (aarch64).
# Run this INSIDE a Linux machine that has the cross toolchain + sysroot,
# and that has the vendor DRMwrap files in place:
#     inc/DRMwrap.h        (from your vendor / course SDK)
#     lib/libDRMwrap.so    (from your vendor / course SDK)
# Both are intentionally NOT part of this repository (they are vendor code).
set -e
cd "$(dirname "$0")"

if [ ! -f inc/DRMwrap.h ]; then
    echo "[FAIL] inc/DRMwrap.h missing."
    echo "       Copy DRMwrap.h and libDRMwrap.so from your vendor SDK into inc/ and lib/ first."
    echo "       (You do not need them for the PC tests: cd test && make unit)"
    exit 1
fi

echo "==> make clean"
make clean || true

echo "==> make"
make

echo
echo "==> file main"
file main
if file main | grep -q 'ARM aarch64'; then
    echo "[OK] aarch64 binary produced: ./main"
else
    echo "[FAIL] not an aarch64 binary - check CC / SYSROOT in the Makefile"
    exit 1
fi
