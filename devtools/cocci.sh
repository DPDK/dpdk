#! /bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2020 Mellanox Technologies, Ltd

# Apply coccinelle transforms.

SRCTREE=$(readlink -f $(dirname $0)/..)
COCCI=$SRCTREE/devtools/cocci
[ -n "$SPATCH" ] || SPATCH=$(which spatch)

PATCH_LIST="$@"
[ -n "$PATCH_LIST" ] || PATCH_LIST=$(echo $COCCI/*.cocci)

[ -x "$SPATCH" ] || (
	echo "Coccinelle tools not installed."
	exit 1
)

tmp=$(mktemp -t dpdk.cocci.XXX)

for c in $PATCH_LIST; do
	while true; do
		echo -n "Applying $c..."
		$SPATCH --sp-file $c -c --linux-spacing --very-quiet	\
			--include-headers --preprocess			\
			--in-place --dir $SRCTREE > $tmp
		if [ -s $tmp ]; then
			echo " changes applied, retrying."
		else
			echo " no change."
			break;
		fi
	done
done

rm -f $tmp
