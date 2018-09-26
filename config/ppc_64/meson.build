# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Luca Boccassi <bluca@debian.org>

if not dpdk_conf.get('RTE_ARCH_64')
	error('Only 64-bit compiles are supported for this platform type')
endif
dpdk_conf.set('RTE_ARCH', 'ppc_64')
dpdk_conf.set('RTE_ARCH_PPC_64', 1)

# overrides specific to ppc64
dpdk_conf.set('RTE_MAX_LCORE', 256)
dpdk_conf.set('RTE_MAX_NUMA_NODES', 32)
dpdk_conf.set('RTE_CACHE_LINE_SIZE', 128)
