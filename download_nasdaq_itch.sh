#!/bin/bash
set -euo pipefail

FILENAME="12302019.NASDAQ_ITCH50"
GZ_FILE="${FILENAME}.gz"
BASE_URL="https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH"
OUTPUT_DIR="cache"

mkdir -p "$OUTPUT_DIR"
cd "$OUTPUT_DIR"

echo "Downloading ${GZ_FILE}..."
wget \
    --continue \
    --show-progress \
    --progress=bar:force \
    -O "${GZ_FILE}" \
    "${BASE_URL}/${GZ_FILE}"

if [ ! -s "${GZ_FILE}" ]; then
    echo "Downloaded file is empty"
    exit 1
fi

if ! gzip -t "${GZ_FILE}" 2>/dev/null; then
    echo "gzip integrity check failed"
    exit 1
fi

echo "Decompressing..."
gunzip -k -v "${GZ_FILE}"

echo "Removing .gz file"
rm ${GZ_FILE}
