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
# This (obscure) bash script finds the smallest different path between
# path1 and path2 given as command line argument. The given paths MUST
# be relative paths, the script is not designed to work with absolute
# paths.
#
# The script will then generate Makefile code that can be saved in a
# file and included in build system.
#
# For instance:
#   depdirs-rule.sh a/b/c/d a/b/e/f
# Will print:
#   FULL_DEPDIRS-a/b/c/d += a/b/e/f
#   LOCAL_DEPDIRS-a/b/c += a/b/e
#
# The script returns 0 except if invalid arguments are given.
#

if [ $# -ne 2 ]; then
	echo "Bad arguments"
	echo "Usage:"
	echo "  $0 path1 path2"
	exit 1
fi

left1=${1%%/*}
right1=${1#*/}
prev_right1=$1
prev_left1=

left2=${2%%/*}
right2=${2#*/}
prev_right2=$2
prev_left2=

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

echo FULL_DEPDIRS-$1 += $2
echo LOCAL_DEPDIRS-$left1 += $left2

exit 0
