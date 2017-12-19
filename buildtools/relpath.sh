#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

#
# print the relative path of $1 from $2 directory
# $1 and $2 MUST be absolute paths
#

if [ $# -ne 2 ]; then
	echo "Bad arguments"
	echo "Usage:"
	echo "  $0 path1 path2"
	exit 1
fi

# get the real absolute path, derefencing symlinks
ABS1="$(dirname $(readlink -f $1))/$(basename $1)"
ABS2=$(readlink -f $2)

# remove leading slash
REL1=${ABS1#/}
REL2=${ABS2#/}

left1=${REL1%%/*}
right1=${REL1#*/}
prev_right1=$REL1
prev_left1=

left2=${REL2%%/*}
right2=${REL2#*/}
prev_right2=$REL2
prev_left2=

prefix=

while [ "${right1}" != "" -a "${right2}" != "" ]; do

	if [ "$left1" != "$left2" ]; then
		break
	fi

	prev_left1=$left1
	left1=$left1/${right1%%/*}
	prev_right1=$right1
	right1=${prev_right1#*/}
	if [ "$right1" = "$prev_right1" ]; then
		right1=""
	fi

	prev_left2=$left2
	left2=$left2/${right2%%/*}
	prev_right2=$right2
	right2=${prev_right2#*/}
	if [ "$right2" = "$prev_right2" ]; then
		right2=""
	fi
done

if [ "${left1}" != "${left2}" ]; then
	right2=${prev_right2}
	right1=${prev_right1}
fi

while [ "${right2}" != "" ]; do
	prefix=${prefix}../
	prev_right2=$right2
	right2=${right2#*/}
	if [ "$right2" = "$prev_right2" ]; then
		right2=""
	fi
done

echo ${prefix}${right1}

exit 0
