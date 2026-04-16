FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        valgrind gcc g++ make libc6-dbg && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /work
CMD ["/bin/bash"]
