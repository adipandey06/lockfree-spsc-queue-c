#!/usr/bin/env bash
set -euo pipefail

if ! command -v docker >/dev/null 2>&1; then
  echo "docker is required but was not found" >&2
  exit 1
fi

# Usage: [CPU_SET=p,c,h] ./run.sh [output_dir] [sample_interval] [num_elements]
OUTPUT_DIR="${1:-./results}"
SAMPLE_INTERVAL="${2:-4096}" # 2^12 elements
NUM_ELEMENTS="${3:-8388608}" # 2^23 elements
# 2^11 samples
CPU_SET="${CPU_SET:-0,2,4}"

# CPU_SET is "producer,consumer,harness" in HOST numbering (--cpuset-cpus keeps host ids).
# Pick three different physical cores: check
#   cat /sys/devices/system/cpu/cpu{0,2,4}/topology/thread_siblings_list
# and make sure none of them list each other.
IFS=',' read -r -a CPUS <<< "${CPU_SET}"
if [[ "${#CPUS[@]}" != "3" ]]; then
  echo "CPU_SET must contain exactly 3 CPU ids (producer,consumer,harness). Got: ${CPU_SET}" >&2
  exit 1
fi
PRODUCER_CORE="${CPUS[0]}"
CONSUMER_CORE="${CPUS[1]}"
HARNESS_CORE="${CPUS[2]}"

if (( SAMPLE_INTERVAL < 1 || (SAMPLE_INTERVAL & (SAMPLE_INTERVAL - 1)) != 0 )); then
  echo "SAMPLE_INTERVAL must be a power of two. Got: ${SAMPLE_INTERVAL}" >&2
  exit 1
fi

mkdir -p "${OUTPUT_DIR}"
OUTPUT_DIR="$(cd "${OUTPUT_DIR}" && pwd)"

docker build -t lockfree-spsc-harness .
docker run --rm \
  --cpuset-cpus="${CPU_SET}" \
  -e OUTPUT_DIR=/results \
  -e SAMPLE_INTERVAL="${SAMPLE_INTERVAL}" \
  -e NUM_ELEMENTS="${NUM_ELEMENTS}" \
  -e PRODUCER_CORE="${PRODUCER_CORE}" \
  -e CONSUMER_CORE="${CONSUMER_CORE}" \
  -e HARNESS_CORE="${HARNESS_CORE}" \
  -v "${OUTPUT_DIR}:/results" \
  lockfree-spsc-harness