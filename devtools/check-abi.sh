#!/bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2019 Red Hat, Inc.

if [ $# != 2 ] && [ $# != 3 ]; then
	echo "Usage: $0 refdir newdir [warnonly]" >&2
	exit 1
fi

refdir=$1
newdir=$2
warnonly=${3:-}
ABIDIFF_SUPPRESSIONS=$(dirname $(readlink -f $0))/libabigail.abignore
ABIDIFF_OPTIONS="--suppr $ABIDIFF_SUPPRESSIONS --no-added-syms"

if [ ! -d $refdir ]; then
	echo "Error: reference directory '$refdir' does not exist." >&2
	exit 1
fi
incdir=$(find $refdir -type d -a -name include)
if [ -z "$incdir" ] || [ ! -e "$incdir" ]; then
	echo "WARNING: could not identify an include directory for $refdir, expect false positives..." >&2
else
	ABIDIFF_OPTIONS="$ABIDIFF_OPTIONS --headers-dir1 $incdir"
fi

if [ ! -d $newdir ]; then
	echo "Error: directory to check '$newdir' does not exist." >&2
	exit 1
fi
incdir2=$(find $newdir -type d -a -name include)
if [ -z "$incdir2" ] || [ ! -e "$incdir2" ]; then
	echo "WARNING: could not identify an include directory for $newdir, expect false positives..." >&2
else
	ABIDIFF_OPTIONS="$ABIDIFF_OPTIONS --headers-dir2 $incdir2"
fi

export newdir ABIDIFF_OPTIONS ABIDIFF_SUPPRESSIONS
export diff_func='run_diff() {
	lib=$1
	name=$(basename $lib)
	if grep -q "; SKIP_LIBRARY=${name%.so.*}\>" $ABIDIFF_SUPPRESSIONS; then
		echo "Skipped $name" >&2
		return 0
	fi
	# Look for a library with the same major ABI version
	lib2=$(find $newdir -name "${name%.*}.*" -a ! -type l)
	if [ -z "$lib2" ] || [ ! -e "$lib2" ]; then
		echo "Error: cannot find $name in $newdir" >&2
		return 1
	fi
	abidiff $ABIDIFF_OPTIONS $lib $lib2 || {
		abiret=$?
		echo "Error: ABI issue reported for abidiff $ABIDIFF_OPTIONS $lib $lib2" >&2
		if [ $(($abiret & 3)) -ne 0 ]; then
			echo "ABIDIFF_ERROR|ABIDIFF_USAGE_ERROR, this could be a script or environment issue." >&2
		fi
		if [ $(($abiret & 4)) -ne 0 ]; then
			echo "ABIDIFF_ABI_CHANGE, this change requires a review (abidiff flagged this as a potential issue)." >&2
		fi
		if [ $(($abiret & 8)) -ne 0 ]; then
			echo "ABIDIFF_ABI_INCOMPATIBLE_CHANGE, this change breaks the ABI." >&2
		fi
		return 1
	}
}'

error=
find $refdir -name "*.so.*" -a ! -type l |
xargs -n1 -P0 sh -c 'eval "$diff_func"; run_diff $0' ||
error=1

[ -z "$error" ] || [ -n "$warnonly" ]
