# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 University of New Hampshire

# There are two Docker images defined in this Dockerfile.
# One is to be used in CI for automated testing.
# The other provides a DTS development environment, simplifying Python dependency management.

FROM ubuntu:22.04 AS base

RUN apt-get -y update && apt-get -y upgrade && \
    apt-get -y install --no-install-recommends \
        python3 \
        python3-pip \
        pipx \
        python3-cachecontrol \
        openssh-client && \
    pipx install poetry>=1.8.2 && pipx ensurepath
WORKDIR /dpdk/dts


FROM base AS runner

# This image is intended to be used as the base for automated systems.
# It bakes DTS into the image during the build.

COPY . /dpdk/dts
RUN poetry install --no-dev

CMD ["poetry", "run", "python", "main.py"]

FROM base AS dev

# This image is intended to be used as DTS development environment. It doesn't need C compilation
# capabilities, only Python dependencies. Once a container mounting DTS using this image is running,
# the dependencies should be installed using Poetry.

RUN apt-get -y install --no-install-recommends \
        vim emacs git
