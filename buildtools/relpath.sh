#!/bin/sh

#   BSD LICENSE
#
#   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
