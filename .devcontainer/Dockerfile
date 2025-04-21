# x86_64 based Ubuntu 18.04
FROM --platform=linux/amd64 ubuntu:22.04

# install packages
RUN apt-get update && apt-get install -y \
    git \
    curl \
    build-essential \
    cmake \
    gdb \
    valgrind \
    && apt-get clean \
    # additional packages
    && rm -rf /var/lib/apt/lists/*

# set working directory
WORKDIR /workspaces
