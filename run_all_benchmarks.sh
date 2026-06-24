#!/bin/bash

echo "Running ITCH parser benchmark"
./run_itch_parser_benchmarks.sh

echo "Running OrderBook with synthetic data benchmark"
./run_synthetic_benchmarks.sh

echo "Running OrderBook with real data benchmark"
./run_real_world_benchmarks.sh