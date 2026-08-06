#!/bin/bash
set -e

if [ ! -f ./cache/12302019.NASDAQ_ITCH50 ]; then
  echo "NASDAQ itch file not detected, downloading..."
  ./download_nasdaq_itch.sh
fi


echo "performance" | sudo tee /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor
sudo taskset -c 3 chrt -f 99 ./cmake-build-release/ITCHParserBenchmark ./cache/12302019.NASDAQ_ITCH50
echo "powersave" | sudo tee /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor
