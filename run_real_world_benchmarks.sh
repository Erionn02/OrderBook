#!/bin/bash
set -e

test_ticker="AAPL"

if [ $# -ne 0 ]; then
  test_ticker=$1
fi

echo "Performing benchmark for $test_ticker"


if [ ! -f ./cache/${test_ticker}.itch ]; then
  if [ ! -f ./cache/12302019.NASDAQ_ITCH50 ]; then
    echo "NASDAQ itch file not detected, downloading..."
    ./download_nasdaq_itch.sh
  fi
  echo "ITCH file for ${test_ticker} not detected, extracting"
  ./cmake-build-release/apps/ExtractITCHPerTicker ./cache/12302019.NASDAQ_ITCH50 ${test_ticker} ./cache/${test_ticker}.itch
fi

sudo sysctl kernel.perf_event_paranoid=1
echo "performance" | sudo tee /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor
sleep 1
sudo env LD_PRELOAD=${LD_PRELOAD} MALLOC_CONF=${MALLOC_CONF} taskset -c 3 chrt -f 99 ./cmake-build-release/RealWorldDataOrderBookBenchmark ./cache/${test_ticker}.itch "${@:2}"
echo "powersave" | sudo tee /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor
