#!/bin/sh -xe

on_error() {
    if [ $? = 0 ]; then
        exit
    fi
    FILES_TO_PRINT="build/meson-logs/testlog.txt"
    FILES_TO_PRINT="$FILES_TO_PRINT build/.ninja_log"
    FILES_TO_PRINT="$FILES_TO_PRINT build/meson-logs/meson-log.txt"
    FILES_TO_PRINT="$FILES_TO_PRINT build/gdb.log"

    for pr_file in $FILES_TO_PRINT; do
        if [ -e "$pr_file" ]; then
            cat "$pr_file"
        fi
    done
}
# We capture the error logs as artifacts in Github Actions, no need to dump
# them via a EXIT handler.
[ -n "$GITHUB_WORKFLOW" ] || trap on_error EXIT

install_libabigail() {
    version=$1
    instdir=$2

    wget -q "http://mirrors.kernel.org/sourceware/libabigail/${version}.tar.gz"
    tar -xf ${version}.tar.gz
    cd $version && autoreconf -vfi && cd -
    mkdir $version/build
    cd $version/build && ../configure --prefix=$instdir && cd -
    make -C $version/build all install
    rm -rf $version
    rm ${version}.tar.gz
}

configure_coredump() {
    # No point in configuring coredump without gdb
    which gdb >/dev/null || return 0
    ulimit -c unlimited
    sudo sysctl -w kernel.core_pattern=/tmp/dpdk-core.%e.%p
}

catch_coredump() {
    ls /tmp/dpdk-core.*.* 2>/dev/null || return 0
    for core in /tmp/dpdk-core.*.*; do
        binary=$(sudo readelf -n $core |grep $(pwd)/build/ 2>/dev/null |head -n1)
        [ -x $binary ] || binary=
        sudo gdb $binary -c $core \
            -ex 'info threads' \
            -ex 'thread apply all bt full' \
            -ex 'quit'
    done |tee -a build/gdb.log
    return 1
}

if [ "$AARCH64" = "true" ]; then
    # convert the arch specifier
    if [ "$CC_FOR_BUILD" = "gcc" ]; then
    	OPTS="$OPTS --cross-file config/arm/arm64_armv8_linux_gcc"
    elif [ "$CC_FOR_BUILD" = "clang" ]; then
    	OPTS="$OPTS --cross-file config/arm/arm64_armv8_linux_clang_ubuntu1804"
    fi
fi

if [ "$BUILD_DOCS" = "true" ]; then
    OPTS="$OPTS -Denable_docs=true"
fi

if [ "$BUILD_32BIT" = "true" ]; then
    OPTS="$OPTS -Dc_args=-m32 -Dc_link_args=-m32"
    export PKG_CONFIG_LIBDIR="/usr/lib32/pkgconfig"
fi

if [ "$DEF_LIB" = "static" ]; then
    OPTS="$OPTS -Dexamples=l2fwd,l3fwd"
else
    OPTS="$OPTS -Dexamples=all"
fi

OPTS="$OPTS -Dplatform=generic"
OPTS="$OPTS --default-library=$DEF_LIB"
OPTS="$OPTS --buildtype=debugoptimized"
OPTS="$OPTS -Dcheck_includes=true"
meson build --werror $OPTS
ninja -C build

if [ "$AARCH64" != "true" ]; then
    failed=
    configure_coredump
    devtools/test-null.sh || failed="true"
    catch_coredump
    [ "$failed" != "true" ]
fi

if [ "$ABI_CHECKS" = "true" ]; then
    LIBABIGAIL_VERSION=${LIBABIGAIL_VERSION:-libabigail-1.6}

    if [ "$(cat libabigail/VERSION 2>/dev/null)" != "$LIBABIGAIL_VERSION" ]; then
        rm -rf libabigail
        # if we change libabigail, invalidate existing abi cache
        rm -rf reference
    fi

    if [ ! -d libabigail ]; then
        install_libabigail $LIBABIGAIL_VERSION $(pwd)/libabigail
        echo $LIBABIGAIL_VERSION > libabigail/VERSION
    fi

    export PATH=$(pwd)/libabigail/bin:$PATH

    REF_GIT_REPO=${REF_GIT_REPO:-https://dpdk.org/git/dpdk}
    REF_GIT_TAG=${REF_GIT_TAG:-v19.11}

    if [ "$(cat reference/VERSION 2>/dev/null)" != "$REF_GIT_TAG" ]; then
        rm -rf reference
    fi

    if [ ! -d reference ]; then
        refsrcdir=$(readlink -f $(pwd)/../dpdk-$REF_GIT_TAG)
        git clone --single-branch -b $REF_GIT_TAG $REF_GIT_REPO $refsrcdir
        meson $OPTS -Dexamples= $refsrcdir $refsrcdir/build
        ninja -C $refsrcdir/build
        DESTDIR=$(pwd)/reference ninja -C $refsrcdir/build install
        devtools/gen-abi.sh reference
        find reference/usr/local -name '*.a' -delete
        rm -rf reference/usr/local/bin
        rm -rf reference/usr/local/share
        echo $REF_GIT_TAG > reference/VERSION
    fi

    DESTDIR=$(pwd)/install ninja -C build install
    devtools/gen-abi.sh install
    devtools/check-abi.sh reference install ${ABI_CHECKS_WARN_ONLY:-}
fi

if [ "$RUN_TESTS" = "true" ]; then
    failed=
    configure_coredump
    sudo meson test -C build --suite fast-tests -t 3 || failed="true"
    catch_coredump
    [ "$failed" != "true" ]
fi
