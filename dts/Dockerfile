# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 University of New Hampshire

# There are two Docker images defined in this Dockerfile.
# One is to be used in CI for automated testing.
# The other provides a DTS development environment, simplifying Python dependency management.

FROM ubuntu:24.04 AS base

RUN apt-get -y update && apt-get -y upgrade && \
    apt-get -y install --no-install-recommends \
        python3 \
        python3-pip \
        pipx \
        python3-cachecontrol \
        git \
        xz-utils \
        openssh-client && \
    pipx install poetry>=1.8.2 && pipx ensurepath && \
    git config --global --add safe.directory /dpdk
WORKDIR /dpdk/dts


FROM base AS runner

# This image is intended to be used as the base for automated systems.
# It bakes DTS into the image during the build.

COPY . /dpdk/dts
# pipx installs packages in ~/.local/bin, which is not in PATH by default. The `pipx ensurepath`
# command used in the previous step adds said directory to PATH, but the docker build process does
# not preserve environment variables between steps. Therefore, ~/.local/bin must be manually added
# into PATH in order to use the poetry command below.
ENV PATH="$PATH:/root/.local/bin"
RUN poetry install --only main

ENTRYPOINT ["poetry", "run", "python", "main.py"]

FROM base AS dev

# This image is intended to be used as DTS development environment. It doesn't need C compilation
# capabilities, only Python dependencies. Once a container mounting DTS using this image is running,
# the dependencies should be installed using Poetry.

RUN apt-get -y install --no-install-recommends \
        vim emacs
