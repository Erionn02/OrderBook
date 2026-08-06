#!/bin/bash

echo "performance" | sudo tee /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor
sudo env LD_PRELOAD=${LD_PRELOAD} MALLOC_CONF=${MALLOC_CONF} taskset -c 3 chrt -f 99 ./cmake-build-release/OrderBookBenchmark
echo "powersave" | sudo tee /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor
