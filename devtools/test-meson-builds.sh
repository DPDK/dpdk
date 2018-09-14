#! /bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

# Run meson to auto-configure the various builds.
# * all builds get put in a directory whose name starts with "build-"
# * if a build-directory already exists we assume it was properly configured
# Run ninja after configuration is done.

srcdir=$(dirname $(readlink -m $0))/..
MESON=${MESON:-meson}
use_shared="--default-library=shared"

if command -v ninja >/dev/null 2>&1 ; then
	ninja_cmd=ninja
elif command -v ninja-build >/dev/null 2>&1 ; then
	ninja_cmd=ninja-build
else
	echo "ERROR: ninja is not found" >&2
	exit 1
fi

build () # <directory> <meson options>
{
	builddir=$1
	shift
	if [ ! -d "$builddir" ] ; then
		options="--werror -Dexamples=all $*"
		echo "$MESON $options $srcdir $builddir"
		$MESON $options $srcdir $builddir
		unset CC
	fi
	echo "$ninja_cmd -C $builddir"
	$ninja_cmd -C $builddir
}

# shared and static linked builds with gcc and clang
for c in gcc clang ; do
	for s in static shared ; do
		export CC="ccache $c"
		build build-$c-$s --default-library=$s
	done
done

# test compilation with minimal x86 instruction set
build build-x86-default -Dmachine=nehalem $use_shared

# enable cross compilation if gcc cross-compiler is found
c=aarch64-linux-gnu-gcc
if command -v $c >/dev/null 2>&1 ; then
	# compile the general v8a also for clang to increase coverage
	export CC="ccache clang"
	build build-arm64-host-clang $use_shared \
		--cross-file config/arm/arm64_armv8_linuxapp_gcc

	for f in config/arm/arm*gcc ; do
		export CC="ccache gcc"
		build build-$(basename $f | tr '_' '-' | cut -d'-' -f-2) \
			$use_shared --cross-file $f
	done
fi
