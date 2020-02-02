#!/bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2019 Red Hat, Inc.

if [ $# != 1 ]; then
	echo "Usage: $0 installdir"
	exit 1
fi

installdir=$1
if [ ! -d $installdir ]; then
	echo "Error: install directory '$installdir' does not exist."
	exit 1
fi

dumpdir=$installdir/dump
rm -rf $dumpdir
mkdir -p $dumpdir
for f in $(find $installdir -name "*.so.*"); do
	if test -L $f; then
		continue
	fi

	libname=$(basename $f)
	abidw --out-file $dumpdir/${libname%.so*}.dump $f
done
