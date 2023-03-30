#!/bin/sh -xe

if [ -z "${DEF_LIB:-}" ]; then
    DEF_LIB=static ABI_CHECKS= BUILD_DOCS= RUN_TESTS= $0
    DEF_LIB=shared $0
    exit
fi

# Builds are run as root in containers, no need for sudo
[ "$(id -u)" != '0' ] || alias sudo=

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

cross_file=

if [ "$AARCH64" = "true" ]; then
    if [ "${CC%%clang}" != "$CC" ]; then
        cross_file=config/arm/arm64_armv8_linux_clang_ubuntu
    else
        cross_file=config/arm/arm64_armv8_linux_gcc
    fi
fi

if [ "$MINGW" = "true" ]; then
    cross_file=config/x86/cross-mingw
fi

if [ "$PPC64LE" = "true" ]; then
    cross_file=config/ppc/ppc64le-power8-linux-gcc-ubuntu
fi

if [ "$RISCV64" = "true" ]; then
    cross_file=config/riscv/riscv64_linux_gcc
fi

buildtype=debugoptimized

if [ "$BUILD_DEBUG" = "true" ]; then
    buildtype=debug
fi

if [ "$BUILD_DOCS" = "true" ]; then
    OPTS="$OPTS -Denable_docs=true"
fi

if [ "$BUILD_32BIT" = "true" ]; then
    OPTS="$OPTS -Dc_args=-m32 -Dc_link_args=-m32"
    OPTS="$OPTS -Dcpp_args=-m32 -Dcpp_link_args=-m32"
    export PKG_CONFIG_LIBDIR="/usr/lib32/pkgconfig"
fi

if [ "$MINGW" = "true" ]; then
    OPTS="$OPTS -Dexamples=helloworld"
elif [ "$DEF_LIB" = "static" ]; then
    OPTS="$OPTS -Dexamples=l2fwd,l3fwd"
else
    OPTS="$OPTS -Dexamples=all"
fi

OPTS="$OPTS -Dplatform=generic"
OPTS="$OPTS -Ddefault_library=$DEF_LIB"
OPTS="$OPTS -Dbuildtype=$buildtype"
OPTS="$OPTS -Dcheck_includes=true"
if [ "$MINI" = "true" ]; then
    OPTS="$OPTS -Denable_drivers=net/null"
    OPTS="$OPTS -Ddisable_libs=*"
else
    OPTS="$OPTS -Ddisable_libs="
fi

if [ "$ASAN" = "true" ]; then
    OPTS="$OPTS -Db_sanitize=address"
    if [ "${CC%%clang}" != "$CC" ]; then
        OPTS="$OPTS -Db_lundef=false"
    fi
fi

OPTS="$OPTS -Dwerror=true"

if [ -d build ]; then
    meson configure build $OPTS
else
    if [ -n "$cross_file" ]; then
        OPTS="$OPTS --cross-file $cross_file"
    fi
    meson setup build $OPTS
fi
ninja -C build

if [ -z "$cross_file" ]; then
    failed=
    configure_coredump
    devtools/test-null.sh || failed="true"
    catch_coredump
    [ "$failed" != "true" ]
fi

if [ "$ABI_CHECKS" = "true" ]; then
    if [ "$(cat libabigail/VERSION 2>/dev/null)" != "$LIBABIGAIL_VERSION" ]; then
        rm -rf libabigail
    fi

    if [ ! -d libabigail ]; then
        install_libabigail "$LIBABIGAIL_VERSION" $(pwd)/libabigail
        echo $LIBABIGAIL_VERSION > libabigail/VERSION
    fi

    export PATH=$(pwd)/libabigail/bin:$PATH

    REF_GIT_REPO=${REF_GIT_REPO:-https://dpdk.org/git/dpdk}

    if [ "$(cat reference/VERSION 2>/dev/null)" != "$REF_GIT_TAG" ]; then
        rm -rf reference
    fi

    if [ ! -d reference ]; then
        refsrcdir=$(readlink -f $(pwd)/../dpdk-$REF_GIT_TAG)
        git clone --single-branch -b "$REF_GIT_TAG" $REF_GIT_REPO $refsrcdir
        meson setup $OPTS -Dexamples= $refsrcdir $refsrcdir/build
        ninja -C $refsrcdir/build
        DESTDIR=$(pwd)/reference ninja -C $refsrcdir/build install
        find reference/usr/local -name '*.a' -delete
        rm -rf reference/usr/local/bin
        rm -rf reference/usr/local/share
        echo $REF_GIT_TAG > reference/VERSION
    fi

    DESTDIR=$(pwd)/install ninja -C build install
    devtools/check-abi.sh reference install ${ABI_CHECKS_WARN_ONLY:-}
fi

if [ "$RUN_TESTS" = "true" ]; then
    failed=
    configure_coredump
    sudo meson test -C build --suite fast-tests -t 3 || failed="true"
    catch_coredump
    [ "$failed" != "true" ]
fi
