#! /bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015 6WIND S.A.
# Copyright 2019 Mellanox Technologies, Ltd

# Run a quick testpmd forwarding with null PMD without hugepage

build=${1:-build} # first argument can be the build directory
testpmd=$1 # or first argument can be the testpmd path
corelist=${2:-0-1} # default using cores 0 and 1
eal_options=$3
testpmd_options=$4

[ -f "$testpmd" ] && build=$(dirname $(dirname $testpmd))
[ -f "$testpmd" ] || testpmd=$build/app/dpdk-testpmd
[ -f "$testpmd" ] || testpmd=$build/app/testpmd
if [ ! -f "$testpmd" ] ; then
	echo 'ERROR: testpmd cannot be found' >&2
	exit 1
fi

if ldd $testpmd | grep -q librte_ ; then
	export LD_LIBRARY_PATH=$build/lib:$LD_LIBRARY_PATH
	libs="-d $build/drivers"
else
	libs=
fi

(sleep 1 && echo stop) |
# testpmd only needs 20M, make it x2 (default number of cores) for NUMA systems
$testpmd -l $corelist --no-huge -m 40 \
	$libs -a 0:0.0 --vdev net_null1 --vdev net_null2 $eal_options -- \
	--no-mlockall --total-num-mbufs=2048 $testpmd_options -ia
