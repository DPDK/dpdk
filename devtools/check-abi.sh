#!/bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2019 Red Hat, Inc.

if [ $# != 2 ] && [ $# != 3 ]; then
	echo "Usage: $0 refdir newdir [warnonly]"
	exit 1
fi

refdir=$1
newdir=$2
warnonly=${3:-}
ABIDIFF_OPTIONS="--suppr $(dirname $0)/libabigail.abignore --no-added-syms"

if [ ! -d $refdir ]; then
	echo "Error: reference directory '$refdir' does not exist."
	exit 1
fi
incdir=$(find $refdir -type d -a -name include)
if [ -z "$incdir" ] || [ ! -e "$incdir" ]; then
	echo "WARNING: could not identify a include directory for $refdir, expect false positives..."
else
	ABIDIFF_OPTIONS="$ABIDIFF_OPTIONS --headers-dir1 $incdir"
fi

if [ ! -d $newdir ]; then
	echo "Error: directory to check '$newdir' does not exist."
	exit 1
fi
incdir2=$(find $newdir -type d -a -name include)
if [ -z "$incdir2" ] || [ ! -e "$incdir2" ]; then
	echo "WARNING: could not identify a include directory for $newdir, expect false positives..."
else
	ABIDIFF_OPTIONS="$ABIDIFF_OPTIONS --headers-dir2 $incdir2"
fi

error=
for dump in $(find $refdir -name "*.dump"); do
	name=$(basename $dump)
	# skip glue drivers, example librte_pmd_mlx5_glue.dump
	# We can't rely on a suppression rule for now:
	# https://sourceware.org/bugzilla/show_bug.cgi?id=25480
	if grep -qE "\<soname='[^']*_glue\.so\.[^']*'" $dump; then
		echo "Skipped glue library $name."
		continue
	fi
	dump2=$(find $newdir -name $name)
	if [ -z "$dump2" ] || [ ! -e "$dump2" ]; then
		echo "Error: can't find $name in $newdir"
		error=1
		continue
	fi
	abidiff $ABIDIFF_OPTIONS $dump $dump2 || {
		abiret=$?
		echo "Error: ABI issue reported for 'abidiff $ABIDIFF_OPTIONS $dump $dump2'"
		error=1
		echo
		if [ $(($abiret & 3)) -ne 0 ]; then
			echo "ABIDIFF_ERROR|ABIDIFF_USAGE_ERROR, this could be a script or environment issue."
		fi
		if [ $(($abiret & 4)) -ne 0 ]; then
			echo "ABIDIFF_ABI_CHANGE, this change requires a review (abidiff flagged this as a potential issue)."
		fi
		if [ $(($abiret & 8)) -ne 0 ]; then
			echo "ABIDIFF_ABI_INCOMPATIBLE_CHANGE, this change breaks the ABI."
		fi
		echo
	}
done

[ -z "$error" ] || [ -n "$warnonly" ]
