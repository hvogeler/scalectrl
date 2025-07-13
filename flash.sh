#!/bin/bash
PORT=${1:-/dev/cu.usbmodem21301}
echo "Flashing to $PORT..."
esptool.py --chip esp32s3 --port $PORT --baud 460800 \
  --before default_reset --after hard_reset write_flash -z \
  --flash_mode dio --flash_freq 80m --flash_size 4MB \
  0x0 bootloader.bin \
  0x10000 scalectrl.bin

echo "Flashing complete!"