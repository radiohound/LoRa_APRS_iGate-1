#!/bin/sh
# Host test of the safe settings file logic (include/config_store.h).   sh test/configstore/run.sh
set -e
here=$(cd "$(dirname "$0")" && pwd); root=$(cd "$here/../.." && pwd)
aj=$(ls -d "$root"/.pio/libdeps/*/ArduinoJson/src | head -1)
out=${TMPDIR:-/tmp}/configstore_test.$$
c++ -std=c++17 -Wall -Wextra -I"$root/include" -I"$aj" -o "$out" "$here/test.cpp"
"$out"; rm -f "$out"
