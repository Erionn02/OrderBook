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


sudo cpupower frequency-set --governor performance 2>&1 > /dev/null
sudo env LD_PRELOAD=${LD_PRELOAD} MALLOC_CONF=${MALLOC_CONF} taskset -c 3 chrt -f 99 ./cmake-build-release/RealWorldDataOrderBookBenchmark ./cache/${test_ticker}.itch "${@:2}"
sudo cpupower frequency-set --governor powersave 2>&1 > /dev/null
