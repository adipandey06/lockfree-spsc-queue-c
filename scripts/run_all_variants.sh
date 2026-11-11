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
  # CONTEXT (mapfile -t run_variants < <(printf '%s\n' "${VARIANTS[@]}" | shuf)):
  # This command reads the shuffled list of variants into an array called run_variants. The `shuf` command randomizes the order of the variants, and `mapfile -t` reads each line of the shuffled output into the run_variants array without including the newline characters.

  run_label="$(printf '%02d' "${run}")"
  # CONTEXT (run_label="$(printf '%02d' "${run}")"): This command formats the current run number as a two-digit string with leading zeros (e.g., "01", "02", ..., "10") and assigns it to the variable run_label. This is useful for creating consistent directory names for each run's results.

  for variant in "${run_variants[@]}"; do
    binary_path="/tmp/harness-${variant}"
    run_dir="$(mktemp -d)"
    # CONTEXT (run_dir="$(mktemp -d)"): This command creates a temporary directory and assigns its path to the variable run_dir. The `mktemp -d` command generates a unique directory name in the system's temporary directory, which is useful for isolating each run's files and avoiding conflicts.

    gcc -O3 -pthread \
      -I"/workspace/src/${variant}" \
      /workspace/test/harness.c \
      "/workspace/src/${variant}/core.c" \
      -o "${binary_path}"

    # CONTEXT (gcc -O3 -pthread ...): This command compiles the harness.c file along with the core.c file from the specified variant directory. The `-O3` flag enables optimization level 3 for better performance, and `-pthread` links the pthread library for multithreading support. The `-I` flag specifies the include directory for the variant's header files. The output binary is saved to the path specified by binary_path.
    # -I "/workspace/src/${variant}": This option tells the compiler to look for header files in the specified directory, which is where the variant's core.c and any associated headers are located. This allows the harness to include the correct implementation of the lock-free SPSC queue for each variant.
    # /workspace/test/harness.c: This is the main test harness source file that contains the logic for running the producer-consumer benchmark. It includes the necessary headers and defines the main function that sets up the test environment, creates threads, and measures performance metrics such as latency and throughput.
    # "/workspace/src/${variant}/core.c": This is the source file for the specific variant of the lock-free SPSC queue implementation. It contains the core logic for the producer-consumer queue, including enqueue and dequeue operations, memory management, and any optimizations specific to that variant.

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

python3 -m scripts.run_analysis_notebook