#!/bin/sh

# SPDX-License-Identifier: BSD-3-Clause

MAPFILE=$1
OBJFILE=$2

# added check for "make -C test/" usage
if [ ! -e $MAPFILE ] || [ ! -f $OBJFILE ]
then
	exit 0
fi

if [ -d $MAPFILE ]
then
	exit 0
fi

for i in `awk 'BEGIN {found=0}
		/.*EXPERIMENTAL.*/ {found=1}
		/.*}.*;/ {found=0}
		/.*;/ {if (found == 1) print $1}' $MAPFILE`
do
	SYM=`echo $i | sed -e"s/;//"`
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
		exit 1
	fi
done
exit 0

