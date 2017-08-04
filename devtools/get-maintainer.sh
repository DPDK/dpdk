#!/bin/sh

#   BSD LICENSE
#
#   Copyright(c) 2017 Intel Corporation. All rights reserved.
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


# Load config options:
# - DPDK_GETMAINTAINER_PATH
. $(dirname $(readlink -e $0))/load-devel-config

options="--no-git-fallback"
options="$options --no-rolestats"

print_usage () {
	cat <<- END_OF_HELP
	usage: $(basename $0) <patch>
	END_OF_HELP
}

# Requires DPDK_GETMAINTAINER_PATH devel config option set,
# please check devtools/load-devel-config.
# DPDK_GETMAINTAINER_PATH should be full path to the get_maintainer.pl script,
# like:
#   DPDK_GETMAINTAINER_PATH=~/linux/scripts/get_maintainer.pl

if [ ! -x "$DPDK_GETMAINTAINER_PATH" ] ; then
	print_usage >&2
	echo
	echo 'Cannot execute DPDK_GETMAINTAINER_PATH' >&2
	exit 1
fi

FILES="COPYING CREDITS Kbuild"
FOLDERS="Documentation arch include fs init ipc kernel scripts"

# Kernel script checks for some files and folders to run
workaround () {
	for f in $FILES; do
		if [ ! -f $f ]; then touch $f; fi
	done

	for d in $FOLDERS; do
		if [ ! -d $d ]; then mkdir $d; fi
	done
}

fix_workaround () {
	for f in $FILES; do if [ -f $f ]; then rm -f $f; fi; done
	for d in $FOLDERS; do if [ -d $d ]; then rmdir $d; fi; done
}

# clean workaround on exit
trap fix_workaround EXIT

workaround
$DPDK_GETMAINTAINER_PATH $options $@
# fix_workaround called on exit by trap
