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

ret=0
for SYM in `$LIST_SYMBOL -S EXPERIMENTAL $MAPFILE`
do
	objdump -t $OBJFILE | grep -q "\.text.*$SYM$"
	IN_TEXT=$?
	objdump -t $OBJFILE | grep -q "\.text\.experimental.*$SYM$"
	IN_EXP=$?
	if [ $IN_TEXT -eq 0 -a $IN_EXP -ne 0 ]
	then
		cat >&2 <<- END_OF_MESSAGE
		$SYM is not flagged as experimental
		but is listed in version map
		Please add __rte_experimental to the definition of $SYM
		END_OF_MESSAGE
		ret=1
	fi
done

for SYM in `objdump -t $OBJFILE |awk '{
	if ($2 != "l" && $4 == ".text.experimental") {
		print $NF
	}
}'`
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
