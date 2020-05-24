#! /bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

# Run meson to auto-configure the various builds.
# * all builds get put in a directory whose name starts with "build-"
# * if a build-directory already exists we assume it was properly configured
# Run ninja after configuration is done.

# set pipefail option if possible
PIPEFAIL=""
set -o | grep -q pipefail && set -o pipefail && PIPEFAIL=1

srcdir=$(dirname $(readlink -f $0))/..
. $srcdir/devtools/load-devel-config

MESON=${MESON:-meson}
use_shared="--default-library=shared"
builds_dir=${DPDK_BUILD_TEST_DIR:-.}

if command -v gmake >/dev/null 2>&1 ; then
	MAKE=gmake
else
	MAKE=make
fi
if command -v ninja >/dev/null 2>&1 ; then
	ninja_cmd=ninja
elif command -v ninja-build >/dev/null 2>&1 ; then
	ninja_cmd=ninja-build
else
	echo "ERROR: ninja is not found" >&2
	exit 1
fi
if command -v ccache >/dev/null 2>&1 ; then
	CCACHE=ccache
else
	CCACHE=
fi

default_path=$PATH
default_pkgpath=$PKG_CONFIG_PATH
default_cppflags=$CPPFLAGS
default_cflags=$CFLAGS
default_ldflags=$LDFLAGS

load_env () # <target compiler>
{
	targetcc=$1
	export PATH=$default_path
	export PKG_CONFIG_PATH=$default_pkgpath
	export CPPFLAGS=$default_cppflags
	export CFLAGS=$default_cflags
	export LDFLAGS=$default_ldflags
	unset DPDK_MESON_OPTIONS
	if command -v $targetcc >/dev/null 2>&1 ; then
		DPDK_TARGET=$($targetcc -v 2>&1 | sed -n 's,^Target: ,,p')
	else # toolchain not yet in PATH: its name should be enough
		DPDK_TARGET=$targetcc
	fi
	# config input: $DPDK_TARGET
	. $srcdir/devtools/load-devel-config
	# config output: $DPDK_MESON_OPTIONS, $PATH, $PKG_CONFIG_PATH, etc
	command -v $targetcc >/dev/null 2>&1 || return 1
}

config () # <dir> <builddir> <meson options>
{
	dir=$1
	shift
	builddir=$1
	shift
	if [ -f "$builddir/build.ninja" ] ; then
		# for existing environments, switch to debugoptimized if unset
		# so that ABI checks can run
		if ! $MESON configure $builddir |
				awk '$1=="buildtype" {print $2}' |
				grep -qw debugoptimized; then
			$MESON configure --buildtype=debugoptimized $builddir
		fi
		return
	fi
	options=
	if echo $* | grep -qw -- '--default-library=shared' ; then
		options="$options -Dexamples=all"
	else
		options="$options -Dexamples=l3fwd" # save disk space
	fi
	options="$options --buildtype=debugoptimized"
	for option in $DPDK_MESON_OPTIONS ; do
		options="$options -D$option"
	done
	options="$options $*"
	echo "$MESON $options $dir $builddir"
	$MESON $options $dir $builddir
}

compile () # <builddir>
{
	builddir=$1
	if [ -n "$TEST_MESON_BUILD_VERY_VERBOSE" ] ; then
		# for full output from ninja use "-v"
		echo "$ninja_cmd -v -C $builddir"
		$ninja_cmd -v -C $builddir
	elif [ -n "$TEST_MESON_BUILD_VERBOSE" ] ; then
		# for keeping the history of short cmds, pipe through cat
		echo "$ninja_cmd -C $builddir | cat"
		$ninja_cmd -C $builddir | cat
	else
		echo "$ninja_cmd -C $builddir"
		$ninja_cmd -C $builddir
	fi
}

install_target () # <builddir> <installdir>
{
	rm -rf $2
	if [ -n "$TEST_MESON_BUILD_VERY_VERBOSE$TEST_MESON_BUILD_VERBOSE" ]; then
		echo "DESTDIR=$2 $ninja_cmd -C $1 install"
		DESTDIR=$2 $ninja_cmd -C $1 install
	else
		echo "DESTDIR=$2 $ninja_cmd -C $1 install >/dev/null"
		DESTDIR=$2 $ninja_cmd -C $1 install >/dev/null
	fi
}

build () # <directory> <target compiler | cross file> <meson options>
{
	targetdir=$1
	shift
	crossfile=
	[ -r $1 ] && crossfile=$1 || targetcc=$1
	shift
	# skip build if compiler not available
	command -v ${CC##* } >/dev/null 2>&1 || return 0
	if [ -n "$crossfile" ] ; then
		cross="--cross-file $crossfile"
		targetcc=$(sed -n 's,^c[[:space:]]*=[[:space:]]*,,p' \
			$crossfile | tr -d "'" | tr -d '"')
	else
		cross=
	fi
	load_env $targetcc || return 0
	config $srcdir $builds_dir/$targetdir $cross --werror $*
	compile $builds_dir/$targetdir
	if [ -n "$DPDK_ABI_REF_VERSION" ]; then
		abirefdir=${DPDK_ABI_REF_DIR:-reference}/$DPDK_ABI_REF_VERSION
		if [ ! -d $abirefdir/$targetdir ]; then
			# clone current sources
			if [ ! -d $abirefdir/src ]; then
				git clone --local --no-hardlinks \
					--single-branch \
					-b $DPDK_ABI_REF_VERSION \
					$srcdir $abirefdir/src
			fi

			rm -rf $abirefdir/build
			config $abirefdir/src $abirefdir/build $cross \
				-Dexamples= $*
			compile $abirefdir/build
			install_target $abirefdir/build $abirefdir/$targetdir
			$srcdir/devtools/gen-abi.sh $abirefdir/$targetdir

			# save disk space by removing static libs and apps
			find $abirefdir/$targetdir/usr/local -name '*.a' -delete
			rm -rf $abirefdir/$targetdir/usr/local/bin
			rm -rf $abirefdir/$targetdir/usr/local/share
		fi

		install_target $builds_dir/$targetdir \
			$(readlink -f $builds_dir/$targetdir/install)
		$srcdir/devtools/gen-abi.sh \
			$(readlink -f $builds_dir/$targetdir/install)
		$srcdir/devtools/check-abi.sh $abirefdir/$targetdir \
			$(readlink -f $builds_dir/$targetdir/install)
	fi
}

if [ "$1" = "-vv" ] ; then
	TEST_MESON_BUILD_VERY_VERBOSE=1
elif [ "$1" = "-v" ] ; then
	TEST_MESON_BUILD_VERBOSE=1
fi
# we can't use plain verbose when we don't have pipefail option so up-level
if [ -z "$PIPEFAIL" -a -n "$TEST_MESON_BUILD_VERBOSE" ] ; then
	echo "# Missing pipefail shell option, changing VERBOSE to VERY_VERBOSE"
	TEST_MESON_BUILD_VERY_VERBOSE=1
fi

# shared and static linked builds with gcc and clang
for c in gcc clang ; do
	command -v $c >/dev/null 2>&1 || continue
	for s in static shared ; do
		export CC="$CCACHE $c"
		build build-$c-$s $c --default-library=$s
		unset CC
	done
done

# test compilation with minimal x86 instruction set
# Set the install path for libraries to "lib" explicitly to prevent problems
# with pkg-config prefixes if installed in "lib/x86_64-linux-gnu" later.
default_machine='nehalem'
ok=$(cc -march=$default_machine -E - < /dev/null > /dev/null 2>&1 || echo false)
if [ "$ok" = "false" ] ; then
	default_machine='corei7'
fi
build build-x86-default cc -Dlibdir=lib -Dmachine=$default_machine $use_shared

# x86 MinGW
build build-x86-mingw $srcdir/config/x86/cross-mingw -Dexamples=helloworld

# generic armv8a with clang as host compiler
f=$srcdir/config/arm/arm64_armv8_linux_gcc
export CC="clang"
build build-arm64-host-clang $f $use_shared
unset CC
# some gcc/arm configurations
for f in $srcdir/config/arm/arm64_[bdo]*gcc ; do
	export CC="$CCACHE gcc"
	build build-$(basename $f | tr '_' '-' | cut -d'-' -f-2) $f $use_shared
	unset CC
done

# ppc configurations
for f in $srcdir/config/ppc/ppc* ; do
	build build-$(basename $f | cut -d'-' -f-2) $f $use_shared
done

# Test installation of the x86-default target, to be used for checking
# the sample apps build using the pkg-config file for cflags and libs
build_path=$(readlink -f $builds_dir/build-x86-default)
export DESTDIR=$build_path/install
# No need to reinstall if ABI checks are enabled
if [ -z "$DPDK_ABI_REF_VERSION" ]; then
	install_target $build_path $DESTDIR
fi

load_env cc
pc_file=$(find $DESTDIR -name libdpdk.pc)
export PKG_CONFIG_PATH=$(dirname $pc_file):$PKG_CONFIG_PATH

# if pkg-config defines the necessary flags, test building some examples
if pkg-config --define-prefix libdpdk >/dev/null 2>&1; then
	export PKGCONF="pkg-config --define-prefix"
	for example in cmdline helloworld l2fwd l3fwd skeleton timer; do
		echo "## Building $example"
		$MAKE -C $DESTDIR/usr/local/share/dpdk/examples/$example clean shared static
	done
fi
