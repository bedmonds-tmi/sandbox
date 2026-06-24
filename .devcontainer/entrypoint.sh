#!/usr/bin/env bash
set -euo pipefail

cd /workspace

if [ -d dev ]; then
    cd /workspace/dev
    exec "$@"
fi

# make a dev directory and move all files except dev into it
mkdir dev
find . -mindepth 1 -maxdepth 1 ! -name dev -exec mv {} dev/ \;

west init -l dev
west update
west config zephyr.base deps/zephyr
west zephyr-export

cd /workspace/dev

exec "$@"