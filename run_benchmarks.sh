#!/bin/bash

sudo cpupower frequency-set --governor performance 2>&1 > /dev/null
sudo taskset -c 3 chrt -f 99 ./cmake-build-release/OrderBookBenchmark
sudo cpupower frequency-set --governor powersave 2>&1 > /dev/null
