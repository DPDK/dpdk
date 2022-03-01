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
ABIDIFF_OPTIONS="--suppr $(dirname $0)/libabigail.abignore --no-added-syms"

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

error=
for dump in $(find $refdir -name "*.dump"); do
	name=$(basename $dump)
	if grep -qE "\<librte_*.*_octeontx2" $dump; then
		echo "Skipped removed driver $name."
		continue
	fi
	dump2=$(find $newdir -name $name)
	if [ -z "$dump2" ] || [ ! -e "$dump2" ]; then
		echo "Error: cannot find $name in $newdir" >&2
		error=1
		continue
	fi
	abidiff $ABIDIFF_OPTIONS $dump $dump2 || {
		abiret=$?
		echo "Error: ABI issue reported for 'abidiff $ABIDIFF_OPTIONS $dump $dump2'" >&2
		error=1
		echo
		if [ $(($abiret & 3)) -ne 0 ]; then
			echo "ABIDIFF_ERROR|ABIDIFF_USAGE_ERROR, this could be a script or environment issue." >&2
		fi
		if [ $(($abiret & 4)) -ne 0 ]; then
			echo "ABIDIFF_ABI_CHANGE, this change requires a review (abidiff flagged this as a potential issue)." >&2
		fi
		if [ $(($abiret & 8)) -ne 0 ]; then
			echo "ABIDIFF_ABI_INCOMPATIBLE_CHANGE, this change breaks the ABI." >&2
		fi
		echo
	}
done

[ -z "$error" ] || [ -n "$warnonly" ]
