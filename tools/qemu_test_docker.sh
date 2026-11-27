#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

# Docker Desktop on macOS ships the credential helper outside the default PATH.
if [ -d /Applications/Docker.app/Contents/Resources/bin ]; then
  export PATH="/Applications/Docker.app/Contents/Resources/bin:$PATH"
fi

mkdir -p build

docker run --rm --platform linux/amd64 \
  -v "$PWD":/work -w /work debian:bookworm-slim bash -lc '
    set -e
    apt-get update >/dev/null
    apt-get install -y --no-install-recommends \
      build-essential gcc-arm-linux-gnueabi libc6-dev-armel-cross qemu-user file >/dev/null
    arm-linux-gnueabi-gcc -static -march=armv5te -mtune=arm926ej-s -mfloat-abi=soft \
      -O2 -Iapp tests/test_bt.c app/bt_sim.c app/bt_uart.c -o build/test_bt_arm
    file build/test_bt_arm
    qemu-arm ./build/test_bt_arm
  '
