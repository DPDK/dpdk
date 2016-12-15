#! /bin/sh

# BSD LICENSE
#
# Copyright 2015 EZchip Semiconductor Ltd.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of EZchip Semiconductor nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Apply coccinelle transforms.

SRCTREE=$(readlink -f $(dirname $0)/..)
COCCI=$SRCTREE/devtools/cocci
[ -n "$SPATCH" ] || SPATCH=$(which spatch)

PATCH_LIST="$@"
[ -n "$PATCH_LIST" ] || PATCH_LIST=$(echo $COCCI/*.cocci)

[ -x "$SPATCH" ] || (
	echo "Coccinelle tools not installed."
	exit 1
)

tmp=$(mktemp)

for c in $PATCH_LIST; do
	while true; do
		echo -n "Applying $c..."
		$SPATCH --sp-file $c -c --linux-spacing --very-quiet	\
			--include-headers --preprocess			\
			--in-place --dir $SRCTREE > $tmp
		if [ -s $tmp ]; then
			echo " changes applied, retrying."
		else
			echo " no change."
			break;
		fi
	done
done

rm -f $tmp
