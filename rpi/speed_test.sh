#!/usr/bin/env bash
# Compare SD vs USB stick read/write throughput.
set -euo pipefail

SIZE_MB=256
USB_MNT=$(findmnt -n -o TARGET /dev/sda1 || echo "")

echo "=== READ test (256 MiB, bypassing OS cache) ==="

echo "--- SD card (/dev/mmcblk0) ---"
sudo -n dd if=/dev/mmcblk0 of=/dev/null bs=1M count=$SIZE_MB iflag=direct status=none 2>&1
sudo -n dd if=/dev/mmcblk0 of=/dev/null bs=1M count=$SIZE_MB iflag=direct 2>&1 | tail -1

echo "--- USB stick (/dev/sda) ---"
sudo -n dd if=/dev/sda of=/dev/null bs=1M count=$SIZE_MB iflag=direct status=none 2>&1
sudo -n dd if=/dev/sda of=/dev/null bs=1M count=$SIZE_MB iflag=direct 2>&1 | tail -1

echo
echo "=== WRITE test (256 MiB, fsync at end) ==="

echo "--- SD card (writing to /tmp on rootfs) ---"
dd if=/dev/zero of=/tmp/sdtest.bin bs=1M count=$SIZE_MB oflag=direct conv=fdatasync 2>&1 | tail -1
rm -f /tmp/sdtest.bin

if [ -n "$USB_MNT" ]; then
    echo "--- USB stick (writing to $USB_MNT) ---"
    dd if=/dev/zero of="$USB_MNT/usbtest.bin" bs=1M count=$SIZE_MB oflag=direct conv=fdatasync 2>&1 | tail -1
    rm -f "$USB_MNT/usbtest.bin"
else
    echo "USB not mounted; skipping write test"
fi

echo
echo "=== summary verdict ==="
echo "OS 부팅 디스크로 USB가 더 빠른지 확인하세요."
