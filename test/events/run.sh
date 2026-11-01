#!/bin/sh
# Builds the formatter on the host and checks its output with the collector's own parser.
# usage: sh test/events/run.sh [path-to-lora-aprs-collector]
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
collector=${1:-$HOME/lora-compress/lora-aprs-collector}
out=${TMPDIR:-/tmp}/events_test.$$
c++ -std=c++17 -Wall -Wextra -I"$root/include" -o "$out" "$here/events_test.cpp" "$root/src/events_format.cpp"
"$out" private || { echo "privatePath test failed"; exit 1; }
"$out" | python3 -I "$here/check.py" "$collector"
rm -f "$out"
