#!/usr/bin/env bash
set -euo pipefail

OUTPUT_DIR="${OUTPUT_DIR:-/results}"
SAMPLE_INTERVAL="${SAMPLE_INTERVAL:-4096}"
NUM_ELEMENTS="${NUM_ELEMENTS:-16777216}" # 2^24  
NUM_RUNS="${NUM_RUNS:-10}"
PRODUCER_CORE="${PRODUCER_CORE:-0}"
CONSUMER_CORE="${CONSUMER_CORE:-1}"
HARNESS_CORE="${HARNESS_CORE:-2}"

VARIANTS=(
  "0-naive"
  "1-padding_without_cached_indices"
  "2-cached_indices_without_padding"
  "1-cached_indices_and_padding"
  "2-bitwise_modulus"
  "0-naive-seq_cst"
  "1-padding_without_cached_indices-seq_cst"
  "2-cached_indices_without_padding-seq_cst"
  "1-cached_indices_and_padding-seq_cst"
  "2-bitwise_modulus-seq_cst"
)

mkdir -p "${OUTPUT_DIR}"

# Run every variant NUM_RUNS times, randomizing each run's order to avoid bias
# from CPU thermal throttling or other effects.

for ((run = 1; run <= NUM_RUNS; run++)); do
  mapfile -t run_variants < <(printf '%s\n' "${VARIANTS[@]}" | shuf)
  run_label="$(printf '%02d' "${run}")"

  for variant in "${run_variants[@]}"; do
    binary_path="/tmp/harness-${variant}"
    run_dir="$(mktemp -d)"

    gcc -O3 -pthread \
      -I"/workspace/src/${variant}" \
      /workspace/test/harness.c \
      "/workspace/src/${variant}/core.c" \
      -o "${binary_path}"

    (
      cd "${run_dir}"
      # The harness process starts on HARNESS_CORE; the producer and consumer threads
      # re-pin themselves to their own cores (all three must be in the container's cpuset).
      taskset -c "${HARNESS_CORE}" "${binary_path}" \
        "${SAMPLE_INTERVAL}" "${NUM_ELEMENTS}" "${PRODUCER_CORE}" "${CONSUMER_CORE}"
      mkdir -p "${OUTPUT_DIR}/${variant}/run-${run_label}"
      mv latency_throughput_buffer_*.csv "${OUTPUT_DIR}/${variant}/run-${run_label}/"
    )

    rm -rf "${run_dir}" "${binary_path}"
    echo "Saved ${OUTPUT_DIR}/${variant}/run-${run_label}/latency_throughput_buffer_*.csv"
  done
done