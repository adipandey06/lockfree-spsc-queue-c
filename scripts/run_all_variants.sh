#!/usr/bin/env bash
set -euo pipefail

OUTPUT_DIR="${OUTPUT_DIR:-/results}"
SAMPLE_INTERVAL="${SAMPLE_INTERVAL:-1000}"
MIN_BUFFER_SIZE="${MIN_BUFFER_SIZE:-2}"
NUM_ELEMENTS="${NUM_ELEMENTS:-1048576}"
HARNESS_CORE="${HARNESS_CORE:-2}"

VARIANTS=(
  "0-naive"
  "1-cached_indices_and_padding"
  "2-bitwise_modulus"
  "0-naive-seq_cst"
  "1-cached_indices_and_padding-seq_cst"
  "2-bitwise_modulus-seq_cst"
)

mkdir -p "${OUTPUT_DIR}"

for variant in "${VARIANTS[@]}"; do
  binary_path="/tmp/harness-${variant}"
  run_dir="$(mktemp -d)"

  gcc -O3 -pthread \
    -I"/workspace/src/${variant}" \
    /workspace/test/harness.c \
    "/workspace/src/${variant}/core.c" \
    -o "${binary_path}"

  (
    cd "${run_dir}"
    taskset -c "${HARNESS_CORE}" "${binary_path}" "${SAMPLE_INTERVAL}" "${MIN_BUFFER_SIZE}" "${NUM_ELEMENTS}"
    mv latency_throughput.csv "${OUTPUT_DIR}/${variant}.csv"
  )

  rm -rf "${run_dir}" "${binary_path}"
  echo "Saved ${OUTPUT_DIR}/${variant}.csv"
done
