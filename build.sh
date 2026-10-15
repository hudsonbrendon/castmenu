#!/usr/bin/env bash
# Build the menu with the kazade/dreamcast-sdk container (KallistiOS + sh-elf-gcc).
set -euo pipefail
cd "$(dirname "$0")"
python3 tools/cast_card/mkassets.py && python3 tools/cast_card/mklang.py
docker run --rm -v "$PWD":/src -w /src kazade/dreamcast-sdk:latest \
  bash -lc 'source /etc/bash.bashrc >/dev/null 2>&1 || true; source /opt/toolchains/dc/kos/environ.sh && make "$@"' _ "$@"
