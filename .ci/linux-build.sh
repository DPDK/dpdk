#!/bin/sh -xe

on_error() {
    if [ $? = 0 ]; then
        exit
    fi
    FILES_TO_PRINT="build/meson-logs/testlog.txt build/.ninja_log build/meson-logs/meson-log.txt"

    for pr_file in $FILES_TO_PRINT; do
        if [ -e "$pr_file" ]; then
            cat "$pr_file"
        fi
    done
}
trap on_error EXIT

if [ "$AARCH64" = "1" ]; then
    # convert the arch specifier
    OPTS="$OPTS --cross-file config/arm/arm64_armv8_linux_gcc"
fi

if [ "$BUILD_DOCS" = "1" ]; then
    OPTS="$OPTS -Denable_docs=true"
fi

if [ "$BUILD_32BIT" = "1" ]; then
    OPTS="$OPTS -Dc_args=-m32 -Dc_link_args=-m32"
    export PKG_CONFIG_LIBDIR="/usr/lib32/pkgconfig"
fi

OPTS="$OPTS --default-library=$DEF_LIB"
OPTS="$OPTS --buildtype=debugoptimized"
meson build --werror -Dexamples=all $OPTS
ninja -C build

if [ "$AARCH64" != "1" ]; then
    devtools/test-null.sh
fi

if [ "$ABI_CHECKS" = "1" ]; then
    REF_GIT_REPO=${REF_GIT_REPO:-https://dpdk.org/git/dpdk}
    REF_GIT_TAG=${REF_GIT_TAG:-v19.11}

    if [ "$(cat reference/VERSION 2>/dev/null)" != "$REF_GIT_TAG" ]; then
        rm -rf reference
    fi

    if [ ! -d reference ]; then
        refsrcdir=$(readlink -f $(pwd)/../dpdk-$REF_GIT_TAG)
        git clone --single-branch -b $REF_GIT_TAG $REF_GIT_REPO $refsrcdir
        meson --werror $OPTS $refsrcdir $refsrcdir/build
        ninja -C $refsrcdir/build
        DESTDIR=$(pwd)/reference ninja -C $refsrcdir/build install
        devtools/gen-abi.sh reference
        echo $REF_GIT_TAG > reference/VERSION
    fi

    DESTDIR=$(pwd)/install ninja -C build install
    devtools/gen-abi.sh install
    devtools/check-abi.sh reference install ${ABI_CHECKS_WARN_ONLY:-}
fi

if [ "$RUN_TESTS" = "1" ]; then
    sudo meson test -C build --suite fast-tests -t 3
fi
