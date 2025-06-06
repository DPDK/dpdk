FROM ubuntu:22.04

# 開発に必要なツールをすべてインストール
# 対話モードを無効化し、不要なキャッシュは削除する
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential \
    pciutils \
    iproute2 \
    kmod \
    python3 \
    python3-pyelftools \
    libnuma-dev \
    meson \
    ninja-build \
    git \
    && rm -rf /var/lib/apt/lists/*

# コンテナが終了しないようにするためのダミーコマンド
# VS Codeがこの設定を上書きするので、実際には実行されない
CMD ["sleep", "infinity"]