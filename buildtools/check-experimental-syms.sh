#!/bin/sh

# SPDX-License-Identifier: BSD-3-Clause

MAPFILE=$1
OBJFILE=$2

LIST_SYMBOL=$RTE_SDK/buildtools/map-list-symbol.sh

# added check for "make -C test/" usage
if [ ! -e $MAPFILE ] || [ ! -f $OBJFILE ]
then
	exit 0
fi

if [ -d $MAPFILE ]
then
	exit 0
fi

DUMPFILE=$(mktemp -t dpdk.${0##*/}.XXX.objdump)
trap 'rm -f "$DUMPFILE"' EXIT
objdump -t $OBJFILE >$DUMPFILE

ret=0
for SYM in `$LIST_SYMBOL -S EXPERIMENTAL $MAPFILE`
do
	if grep -q "\.text.*$SYM$" $DUMPFILE &&
		! grep -q "\.text\.experimental.*$SYM$" $DUMPFILE
	then
		cat >&2 <<- END_OF_MESSAGE
		$SYM is not flagged as experimental
		but is listed in version map
		Please add __rte_experimental to the definition of $SYM
		END_OF_MESSAGE
		ret=1
	fi
done

# Filter out symbols suffixed with a . for icc
for SYM in `awk '{
	if ($2 != "l" && $4 == ".text.experimental" && !($NF ~ /\.$/)) {
		print $NF
	}
}' $DUMPFILE`
do
	$LIST_SYMBOL -S EXPERIMENTAL -s $SYM -q $MAPFILE || {
		cat >&2 <<- END_OF_MESSAGE
		$SYM is flagged as experimental
		but is not listed in version map
		Please add $SYM to the version map
		END_OF_MESSAGE
		ret=1
	}
done

exit $ret
