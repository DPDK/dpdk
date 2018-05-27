#! /bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 Mellanox Technologies, Ltd

cd $(dirname $0)/..

# speed up by ignoring Unicode details
export LC_ALL=C

find_orphan_symbols ()
{
    for map in $(find lib drivers -name '*.map') ; do
        for sym in $(sed -rn 's,^([^}]*_.*);,\1,p' $map) ; do
            if echo $sym | grep -q '^per_lcore_' ; then
                continue
            fi
            if ! grep -q -r --exclude=$(basename $map) \
                    -w $sym $(dirname $map) ; then
                echo "$map: $sym"
            fi
        done
    done
}

orphan_symbols=$(find_orphan_symbols)
if [ -n "$orphan_symbols" ] ; then
    echo "Found only in symbol map file:"
    echo "$orphan_symbols" | sed 's,^,\t,'
    exit 1
fi
