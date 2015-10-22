#! /bin/sh

# BSD LICENSE
#
# Copyright 2015 6WIND S.A.
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
#   * Neither the name of 6WIND S.A. nor the names of its
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

# Load config options:
# - DPDK_CHECKPATCH_PATH
# - DPDK_CHECKPATCH_LINE_LENGTH
. scripts/load-devel-config.sh
if [ ! -x "$DPDK_CHECKPATCH_PATH" ] ; then
	echo 'Cannot execute DPDK_CHECKPATCH_PATH' >&2
	exit 1
fi

length=${DPDK_CHECKPATCH_LINE_LENGTH:-80}

# override default Linux options
options="--no-tree"
options="$options --max-line-length=$length"
options="$options --show-types"
options="$options --ignore=LINUX_VERSION_CODE,FILE_PATH_CHANGES,\
VOLATILE,PREFER_PACKED,PREFER_ALIGNED,PREFER_PRINTF,PREFER_KERNEL_TYPES,\
SPLIT_STRING,LINE_SPACING,PARENTHESIS_ALIGNMENT,NETWORKING_BLOCK_COMMENT_STYLE,\
NEW_TYPEDEFS,COMPARISON_TO_NULL"

print_usage () {
	echo "usage: $(basename $0) [-q] [-v] [patch1 [patch2] ...]]"
}

quiet=false
verbose=false
while getopts hqv ARG ; do
	case $ARG in
		q ) quiet=true ;;
		v ) verbose=true ;;
		h ) print_usage ; exit 0 ;;
		? ) print_usage ; exit 1 ;;
	esac
done
shift $(($OPTIND - 1))

status=0
for p in "$@" ; do
	! $verbose || printf '\n### %s\n\n' "$p"
	report=$($DPDK_CHECKPATCH_PATH $options "$p" 2>/dev/null)
	[ $? -ne 0 ] || continue
	$verbose || printf '\n### %s\n\n' "$p"
	printf '%s\n' "$report" | head -n -6
	status=$(($status + 1))
done
pass=$(($# - $status))
$quiet || printf '%d/%d valid patch' $pass $#
$quiet || [ $pass -le 1 ] || printf 'es'
$quiet || printf '\n'
exit $status
