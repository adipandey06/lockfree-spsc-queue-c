#!/usr/bin/env bash
set -euo pipefail

if ! command -v docker >/dev/null 2>&1; then
  echo "docker is required but was not found" >&2
  exit 1
fi

OUTPUT_DIR="${1:-./results}"
SAMPLE_INTERVAL="${2:-1000}"
MIN_BUFFER_SIZE="${3:-2}"
NUM_ELEMENTS="${4:-1048576}"
CPU_SET="${CPU_SET:-0,1,2}"

if [[ "$(echo "${CPU_SET}" | tr ',' '\n' | sed '/^$/d' | wc -l | xargs)" != "3" ]]; then
  echo "CPU_SET must contain exactly 3 CPU ids. Got: ${CPU_SET}" >&2
  exit 1
fi

if [[ "$(echo "${CPU_SET}" | tr ',' '\n' | sed '/^$/d' | sort -n | paste -sd, -)" != "0,1,2" ]]; then
  echo "CPU_SET must be 0,1,2 (in any order) so producer and consumer can pin to cores 0 and 1." >&2
  exit 1
fi

mkdir -p "${OUTPUT_DIR}"
OUTPUT_DIR="$(cd "${OUTPUT_DIR}" && pwd)"

docker build -t lockfree-spsc-harness .
docker run --rm \
  --cpuset-cpus="${CPU_SET}" \
  -e OUTPUT_DIR=/results \
  -e SAMPLE_INTERVAL="${SAMPLE_INTERVAL}" \
  -e MIN_BUFFER_SIZE="${MIN_BUFFER_SIZE}" \
  -e NUM_ELEMENTS="${NUM_ELEMENTS}" \
  -e HARNESS_CORE=2 \
  -v "${OUTPUT_DIR}:/results" \
  lockfree-spsc-harness
