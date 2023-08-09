#! /bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 Mellanox Technologies, Ltd

cd $(dirname $0)/..

# speed up by ignoring Unicode details
export LC_ALL=C

if [ $# = 0 ] ; then
    set -- $(find lib drivers -name '*.map' -a ! -path drivers/version.map)
fi

ret=0

find_orphan_symbols ()
{
    for map in $@ ; do
        for sym in $(sed -rn 's,^([^}]*_.*);.*$,\1,p' $map) ; do
            if echo $sym | grep -q '^per_lcore_' ; then
                symsrc=${sym#per_lcore_}
            elif echo $sym | grep -q '^__rte_.*_trace_' ; then
                symsrc=${sym#__}
            else
                symsrc=$sym
            fi
            if [ -z "$(grep -rlw $symsrc $(dirname $map) | grep -v $map)" ] ; then
                echo "$map: $sym"
            fi
        done
    done
}

orphan_symbols=$(find_orphan_symbols $@)
if [ -n "$orphan_symbols" ] ; then
    echo "Found only in symbol map file:"
    echo "$orphan_symbols" | sed 's,^,\t,'
    ret=1
fi

find_duplicate_symbols ()
{
    for map in $@ ; do
        buildtools/map-list-symbol.sh $map | \
            sort | uniq -c | grep -v " 1 $map" || true
    done
}

duplicate_symbols=$(find_duplicate_symbols $@)
if [ -n "$duplicate_symbols" ] ; then
    echo "Found duplicates in symbol map file:"
    echo "$duplicate_symbols"
    ret=1
fi

local_miss_maps=$(grep -L 'local: \*;' $@ || true)
if [ -n "$local_miss_maps" ] ; then
    echo "Found maps without local catch-all:"
    echo "$local_miss_maps"
    ret=1
fi

find_empty_maps ()
{
    for map in $@ ; do
        [ $(buildtools/map-list-symbol.sh $map | wc -l) != '0' ] || echo $map
    done
}

empty_maps=$(find_empty_maps $@)
if [ -n "$empty_maps" ] ; then
    echo "Found empty maps:"
    echo "$empty_maps"
    ret=1
fi

find_bad_format_maps ()
{
    abi_version=$(cut -d'.' -f 1 ABI_VERSION)
    next_abi_version=$((abi_version + 1))
    for map in $@ ; do
        cat $map | awk '
            /^(DPDK_('$abi_version'|'$next_abi_version')|EXPERIMENTAL|INTERNAL) \{$/ { next; } # start of a section
            /^}( DPDK_'$abi_version')?;$/ { next; } # end of a section
            /^$/ { next; } # empty line
            /^\t(global:|local: \*;)$/ { next; } # qualifiers
            /^\t[a-zA-Z_0-9]*;( # WINDOWS_NO_EXPORT)?$/ { next; } # symbols
            /^\t# added in [0-9]*\.[0-9]*$/ { next; } # version comments
            { print $0; }' || echo $map
    done
}

bad_format_maps=$(find_bad_format_maps $@)
if [ -n "$bad_format_maps" ] ; then
    echo "Found badly formatted maps:"
    echo "$bad_format_maps"
    ret=1
fi

exit $ret
