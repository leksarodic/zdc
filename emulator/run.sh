#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

if [ ! -x build/strada-ui ]; then
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
fi

exec ./build/strada-ui "$@"
