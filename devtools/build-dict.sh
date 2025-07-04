#! /bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020 Mellanox Technologies, Ltd

# Build a spelling dictionary suitable for DPDK_CHECKPATCH_CODESPELL

# path to local clone of https://github.com/codespell-project/codespell.git
codespell_path=$1
dic_path=$codespell_path/codespell_lib/data
if [ ! -d "$dic_path" ]; then
	echo "Usage: $0 <path_to_codespell_project>" >&2
	exit 1
fi

# concatenate codespell dictionaries, except GB/US one
for suffix in .txt _code.txt _informal.txt _names.txt _rare.txt _usage.txt ; do
	cat $dic_path/dictionary$suffix
done |

# remove too short or wrong checks
sed '/^..->/d' |
sed '/^uint->/d' |
sed "/^doesn'->/d" |
sed '/^wasn->/d' |
sed '/^stdio->/d' |
sed '/^soring->/d' |

# print to stdout
cat
