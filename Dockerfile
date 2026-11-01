FROM ubuntu:24.04

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential \
        util-linux \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY . /workspace

RUN chmod +x /workspace/scripts/run_all_variants.sh

ENTRYPOINT ["/workspace/scripts/run_all_variants.sh"]
