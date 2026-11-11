FROM ubuntu:24.04

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential \
        util-linux \
        python3 \
        python3-pip \
        python3-venv \
    && rm -rf /var/lib/apt/lists/*
# CONTEXT (RUN apt-get update ...):
# This RUN command updates the package lists for the Ubuntu base image and installs essential build tools and utilities. The `--no-install-recommends` option ensures that only the essential packages are installed, reducing the image size. After installation, it cleans up the package lists to save space in the Docker image.



WORKDIR /workspace
# CONTEXT (/workspace): WORKDIR sets `/workspace` as the working directory for any subsequent commands in the Dockerfile. If the directory does not exist, it will be created.

COPY . /workspace
# CONTEXT (COPY . /workspace): COPY copies the contents of the current directory (where the Dockerfile is located) into the `/workspace` directory in the Docker image.

RUN python3 -m venv /opt/venv \
    && /opt/venv/bin/pip install --no-cache-dir -r requirements.txt
ENV PATH="/opt/venv/bin:$PATH"

RUN chmod +x /workspace/scripts/run_all_variants.sh 
# chmod "change mode", +x "add execute permission for all users" /workspace/scripts/run_all_variants.sh

ENTRYPOINT ["/workspace/scripts/run_all_variants.sh"]
# CONTEXT (ENTRYPOINT):
# The ENTRYPOINT instruction in a Dockerfile specifies the command that will be executed when a container is run from the image.