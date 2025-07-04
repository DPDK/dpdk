#!/bin/sh

# SPDX-License-Identifier: BSD-3-Clause

MAPFILE=$1
OBJFILE=$2

ROOTDIR=$(readlink -f $(dirname $(readlink -f $0))/..)
LIST_SYMBOL=$ROOTDIR/buildtools/map-list-symbol.sh
DUMPFILE=$(mktemp -t dpdk.${0##*/}.objdump.XXXXXX)
trap 'rm -f "$DUMPFILE"' EXIT
objdump -t $OBJFILE >$DUMPFILE

ret=0

for SYM in `$LIST_SYMBOL -S EXPERIMENTAL $MAPFILE |cut -d ' ' -f 3`
do
	if grep -q "\.text.*[[:space:]]$SYM$" $DUMPFILE &&
		! grep -q "\.text\.experimental.*[[:space:]]$SYM$" $DUMPFILE &&
		$LIST_SYMBOL -s $SYM $MAPFILE | grep -q EXPERIMENTAL
	then
		cat >&2 <<- END_OF_MESSAGE
		$SYM is not flagged as experimental but is exported as an experimental symbol
		Please add __rte_experimental to the definition of $SYM
		END_OF_MESSAGE
		ret=1
	fi
done

for SYM in `awk '{
	if ($2 != "l" && $4 == ".text.experimental") {
		print $NF
	}
}' $DUMPFILE`
do
	$LIST_SYMBOL -S EXPERIMENTAL -s $SYM -q $MAPFILE || {
		cat >&2 <<- END_OF_MESSAGE
		$SYM is flagged as experimental but is not exported as an experimental symbol
		Please add RTE_EXPORT_EXPERIMENTAL_SYMBOL to the definition of $SYM
		END_OF_MESSAGE
		ret=1
	}
done

for SYM in `$LIST_SYMBOL -S INTERNAL $MAPFILE |cut -d ' ' -f 3`
do
	if grep -q "\.text.*[[:space:]]$SYM$" $DUMPFILE &&
		! grep -q "\.text\.internal.*[[:space:]]$SYM$" $DUMPFILE
	then
		cat >&2 <<- END_OF_MESSAGE
		$SYM is not flagged as internal but is exported as an internal symbol
		Please add __rte_internal to the definition of $SYM
		END_OF_MESSAGE
		ret=1
	fi
done

for SYM in `awk '{
	if ($2 != "l" && $4 == ".text.internal") {
		print $NF
	}
}' $DUMPFILE`
do
	$LIST_SYMBOL -S INTERNAL -s $SYM -q $MAPFILE || {
		cat >&2 <<- END_OF_MESSAGE
		$SYM is flagged as internal but is not exported as an internal symbol
		Please add RTE_EXPORT_INTERNAL_SYMBOL to the definition of $SYM
		END_OF_MESSAGE
		ret=1
	}
done

exit $ret
