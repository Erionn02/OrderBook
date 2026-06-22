#!/bin/bash
set -e

if [ ! -f ./cache/12302019.NASDAQ_ITCH50 ]; then
  echo "NASDAQ itch file not detected, downloading..."
  ./download_nasdaq_itch.sh
fi


sudo cpupower frequency-set --governor performance 2>&1 > /dev/null
sudo taskset -c 3 chrt -f 99 ./cmake-build-release/ITCHParserBenchmark ./cache/12302019.NASDAQ_ITCH50
sudo cpupower frequency-set --governor powersave 2>&1 > /dev/null
