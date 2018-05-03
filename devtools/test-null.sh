#! /bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015 6WIND S.A.

# Run a quick testpmd forwarding with null PMD without hugepage

build=${1:-build}
coremask=${2:-3} # default using cores 0 and 1

if grep -q SHARED_LIB=y $build/.config; then
	pmd='-d librte_pmd_null.so'
fi

(sleep 1 && echo stop) |
$build/app/testpmd -c $coremask -n 1 --no-huge \
	$pmd --vdev net_null1 --vdev net_null2 -- \
	--total-num-mbufs=2048 -ia
