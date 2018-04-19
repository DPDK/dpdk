#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation


# Load config options:
# - DPDK_GETMAINTAINER_PATH
. $(dirname $(readlink -e $0))/load-devel-config

options="--no-git-fallback"
options="$options --no-rolestats"

print_usage () {
	cat <<- END_OF_HELP
	usage: $(basename $0) <patch>

	The DPDK_GETMAINTAINER_PATH variable should be set to the full path to
	the get_maintainer.pl script located in Linux kernel sources. Example:
	DPDK_GETMAINTAINER_PATH=~/linux/scripts/get_maintainer.pl

	Also refer to devtools/load-devel-config to store your configuration.
	END_OF_HELP
}

# Requires DPDK_GETMAINTAINER_PATH devel config option set
if [ ! -f "$DPDK_GETMAINTAINER_PATH" ] ||
   [ ! -x "$DPDK_GETMAINTAINER_PATH" ] ; then
	print_usage >&2
	echo
	echo 'Cannot execute DPDK_GETMAINTAINER_PATH' >&2
	exit 1
fi

FILES="COPYING CREDITS Kbuild"
FOLDERS="Documentation arch include fs init ipc scripts"

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
