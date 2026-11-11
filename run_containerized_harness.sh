#!/usr/bin/env bash
set -euo pipefail

if ! command -v docker >/dev/null 2>&1; then
  echo "docker is required but was not found" >&2
  exit 1
fi


NUM_RUNS="${1:-1}"
OUTPUT_DIR="${2:-./results}"
SAMPLE_INTERVAL="${3:-4096}"
NUM_ELEMENTS="${4:-8388608}" 
CPU_SET="${CPU_SET:-0,2,4}"

# CONTEXT (Syntax for variables):
# ${VAR:-default} means "use the value of VAR if it is set, otherwise use default"


# QUESTION: Is the order of variable declaration here exactly the order of arguments that we must pass?
# Yes, the order of variable declaration corresponds to the order of command-line arguments passed to the script. The first argument ($1) is assigned to OUTPUT_DIR, the second argument ($2) to SAMPLE_INTERVAL, the third argument ($3) to NUM_ELEMENTS, and the fourth argument ($4) to NUM_RUNS. If any of these arguments are not provided when running the script, default values will be used instead.

# CONTEXT (Syntax for command-line arguments):
# $1, $2, $3, $4 are the first four command-line arguments


# CPU_SET is "producer,consumer,harness" in HOST numbering (--cpuset-cpus keeps host ids).
# Pick three different physical cores: check
#   cat /sys/devices/system/cpu/cpu{0,2,4}/topology/thread_siblings_list
# and make sure none of them list each other.


IFS=',' read -r -a CPUS <<< "${CPU_SET}"
# CONTEXT (Syntax for arrays): 
# IFS=',' read -r -a CPUS <<< "${CPU_SET}" means "split the string in CPU_SET on commas and store the resulting values in an array called CPUS"

if [[ "${#CPUS[@]}" != "3" ]]; then
# CONTEXT (Syntax of arrays): ${#CPUS[@]} gives the number of elements in the array CPUS
  echo "CPU_SET must contain exactly 3 CPU ids (producer,consumer,harness). Got: ${CPU_SET}" >&2
  # CONTEXT (echo <string> >&2): "send this output to standard error instead of standard output"
  exit 1
fi

PRODUCER_CORE="${CPUS[0]}"git
CONSUMER_CORE="${CPUS[1]}"
HARNESS_CORE="${CPUS[2]}"

if (( SAMPLE_INTERVAL < 1 || (SAMPLE_INTERVAL & (SAMPLE_INTERVAL - 1)) != 0 )); then
  echo "SAMPLE_INTERVAL must be a power of two. Got: ${SAMPLE_INTERVAL}" >&2
  exit 1
fi

mkdir -p "${OUTPUT_DIR}"
# CONTEXT (-p in mkdir): create parent directories as needed, and don't error if the directory already exists.
# By default, OUTPUT_DIR is set to ./results, so mkdir -p "${OUTPUT_DIR}" will create a directory called "results" in the current working directory if it doesn't already exist. If OUTPUT_DIR is set to a different path, it will create that directory instead.

OUTPUT_DIR="$(cd "${OUTPUT_DIR}" && pwd)"
# CONTEXT: overwrites OUTPUT_DIR with the absolute path of the directory, ensuring that any relative paths are resolved to their full form. This is useful for consistency and avoiding issues with relative paths later in the script.

docker build -t lockfree-spsc-harness .
# CONTEXT (docker build -t <name> .): build a Docker image from the Dockerfile in the current directory (.) and tag it with the name "lockfree-spsc-harness"

docker run --rm \
  --cpuset-cpus="${CPU_SET}" \
  -e OUTPUT_DIR=/results \
  -e SAMPLE_INTERVAL="${SAMPLE_INTERVAL}" \
  -e NUM_ELEMENTS="${NUM_ELEMENTS}" \
  -e PRODUCER_CORE="${PRODUCER_CORE}" \
  -e CONSUMER_CORE="${CONSUMER_CORE}" \
  -e HARNESS_CORE="${HARNESS_CORE}" \
  -e NUM_RUNS="${NUM_RUNS}" \
  -v "${OUTPUT_DIR}:/results" \
  --user "$(id -u):$(id -g)" \
  lockfree-spsc-harness
  
  # CONTEXT: The last line is the name of the image to run. It must match the name given in `docker build -t ...` above.


# QUESTION: How do you run commands inside the container from here?
# You can use `docker exec` to run commands in a running container, but since this script uses `--rm`, the container will be removed after it exits. 

# CONTEXT (docker run arguments):
# --rm: automatically remove the container when it exits
# --cpuset-cpus: restrict the container to specific CPUs
# -e: set environment variables inside the container
# -v: mount a volume (host directory to container directory)
#     -v "${OUTPUT_DIR}:/results": mounts the host's OUTPUT_DIR to /results in the container, allowing the container to write results to the host filesystem.


# QUESTION: What does "mounting" mean?
# Mounting in the context of Docker means making a directory from the host machine available inside the container. In this case, the host's OUTPUT_DIR is mounted to /results in the container, allowing the container to read from and write to that directory as if it were part of its own filesystem. This is useful for sharing data between the host and the container.
# FOLLOW-UP: Does it automatically create the directory if it doesn't exist? Yes, Docker will create the host directory if it does not already exist when you use the -v option to mount a volume. However, the container will not create the directory from inside itself; it will only have access to the host's directory.
# FOLLOW-UP: Does it automatically unmount the directory when the container exits? Yes, when the container exits, the mount is automatically removed, but the host directory remains intact. The data written to the host directory by the container will persist after the container has stopped or been removed.