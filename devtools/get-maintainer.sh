#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation


# Load config options:
# - DPDK_GETMAINTAINER_PATH
. $(dirname $(readlink -f $0))/load-devel-config

options="--no-tree --no-git-fallback"
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

$DPDK_GETMAINTAINER_PATH $options $@
