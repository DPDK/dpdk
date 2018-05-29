#! /bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

# Run meson to auto-configure the various builds.
# * all builds get put in a directory whose name starts with "build-"
# * if a build-directory already exists we assume it was properly configured
# Run ninja after configuration is done.

srcdir=$(dirname $(readlink -m $0))/..
MESON=${MESON:-meson}

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
	echo "ninja -C $builddir"
	ninja -C $builddir
}

# shared and static linked builds with gcc and clang
for c in gcc clang ; do
	for s in static shared ; do
		export CC="ccache $c"
		build build-$c-$s --default-library=$s
	done
done

# test compilation with minimal x86 instruction set
build build-x86-default -Dmachine=nehalem

# enable cross compilation if gcc cross-compiler is found
for f in config/arm/arm*gcc ; do
	c=aarch64-linux-gnu-gcc
	if ! command -v $c >/dev/null 2>&1 ; then
		continue
	fi
	export CC="ccache $c"
	build build-$(basename $f | tr '_' '-' | cut -d'-' -f-2) --cross-file $f
done
