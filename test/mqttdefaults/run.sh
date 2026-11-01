#!/bin/sh
# Builds the MQTT pre-fill rule on the host and checks it.   sh test/mqttdefaults/run.sh
set -e
here=$(cd "$(dirname "$0")" && pwd); root=$(cd "$here/../.." && pwd)
out=${TMPDIR:-/tmp}/mqttdefaults_test.$$
c++ -std=c++17 -Wall -Wextra -I"$root/include" -o "$out" "$here/test.cpp"
"$out"; rm -f "$out"
