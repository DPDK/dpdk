FROM ubuntu:22.04

# 基本ツールと依存関係のインストール
RUN apt update && apt install -y \
    git \
    build-essential \
    pciutils \
    iproute2 \
    kmod \
    python3 \
    python3-pip \
    python3-pyelftools \
    libnuma-dev \
    meson \
    ninja-build \
    && rm -rf /var/lib/apt/lists/*

# 作業ディレクトリ作成
WORKDIR /opt

# Mesonでビルド
WORKDIR /opt/dpdk
COPY . /opt/dpdk

# # コンパイル時にexamplesも含める
RUN meson configure build -Dexamples=all && ninja -C build

RUN meson build && ninja -C build

CMD ["/bin/bash"]