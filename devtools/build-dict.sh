#! /bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020 Mellanox Technologies, Ltd

# Build a spelling dictionary suitable for DPDK_CHECKPATCH_CODESPELL

# path to local clone of https://github.com/codespell-project/codespell.git
codespell_path=$1

# concatenate codespell dictionaries, except GB/US one
for suffix in .txt _code.txt _informal.txt _names.txt _rare.txt _usage.txt ; do
	cat $codespell_path/codespell_lib/data/dictionary$suffix
done |

# remove too short or wrong checks
sed '/^..->/d' |
sed '/^uint->/d' |
sed "/^doesn'->/d" |
sed '/^wasn->/d' |

# print to stdout
cat
