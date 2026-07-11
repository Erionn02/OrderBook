#!/bin/bash
set -e

if [[ -z $(dpkg -l | grep libjemalloc2) ]]; then
  echo "libjemalloc2 not detected, installing"
  sudo apt update
  sudo apt install -y libjemalloc2
else
  echo "libjemalloc2 detected"
fi


./build.sh Release -DPGO_MODE=GENERATE
LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libjemalloc.so.2 MALLOC_CONF="thp:always,metadata_thp:always" ./run_synthetic_benchmarks.sh
./build.sh Release -DPGO_MODE=USE
LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libjemalloc.so.2 MALLOC_CONF="thp:always,metadata_thp:always" ./run_synthetic_benchmarks.sh
