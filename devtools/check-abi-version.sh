#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

# Check whether library symbols have correct
# version (provided ABI number or provided ABI
# number + 1 or EXPERIMENTAL or INTERNAL).
# Args:
#   $1: path of the library .so file
#   $2: ABI major version number to check
#       (defaults to ABI_VERSION file value)

if [ -z "$1" ]; then
    echo "Script checks whether library symbols have"
    echo "correct version (ABI_VER/ABI_VER+1/EXPERIMENTAL/INTERNAL)"
    echo "Usage:"
    echo "  $0 SO_FILE_PATH [ABI_VER]"
    exit 1
fi

LIB="$1"
DEFAULT_ABI=$(cat "$(dirname \
            $(readlink -f $0))/../ABI_VERSION" | \
            cut -d'.' -f 1)
ABIVER="DPDK_${2-$DEFAULT_ABI}"
NEXT_ABIVER="DPDK_$((${2-$DEFAULT_ABI}+1))"

ret=0

# get output of objdump
OBJ_DUMP_OUTPUT=`objdump -TC --section=.text ${LIB} 2>&1 | grep ".text"`

# there may not be any .text sections in the .so file, in which case exit early
echo "${OBJ_DUMP_OUTPUT}" | grep "not found in any input file" -q
if [ "$?" -eq 0 ]; then
    exit 0
fi

# we have symbols, so let's see if the versions are correct
for SYM in $(echo "${OBJ_DUMP_OUTPUT}" | awk '{print $(NF-1) "-" $NF}')
do
    version=$(echo $SYM | cut -d'-' -f 1)
    symbol=$(echo $SYM | cut -d'-' -f 2)
    case $version in (*"$ABIVER"*|*"$NEXT_ABIVER"*|"EXPERIMENTAL"|"INTERNAL")
        ;;
    (*)
        echo "Warning: symbol $symbol ($version) should be annotated " \
             "as ABI version $ABIVER / $NEXT_ABIVER, EXPERIMENTAL, or INTERNAL."
        ret=1
    ;;
    esac
done

exit $ret
