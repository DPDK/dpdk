# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Luca Boccassi <bluca@debian.org>

if not dpdk_conf.get('RTE_ARCH_64')
	error('Only 64-bit compiles are supported for this platform type')
endif
dpdk_conf.set('RTE_ARCH', 'ppc_64')
dpdk_conf.set('RTE_ARCH_PPC_64', 1)

# RHEL 7.x uses gcc 4.8.X which doesn't generate code for Power 9 CPUs,
# though it will detect a Power 9 CPU when the "-mcpu=native" argument
# is used, resulting in a build failure.
power9_supported = cc.has_argument('-mcpu=power9')
if not power9_supported
	machine = 'power8'
	machine_args = ['-mcpu=power8', '-mtune=power8']
	dpdk_conf.set('RTE_MACHINE','power8')
endif

# overrides specific to ppc64
dpdk_conf.set('RTE_MAX_LCORE', 1536)
dpdk_conf.set('RTE_MAX_NUMA_NODES', 32)
dpdk_conf.set('RTE_CACHE_LINE_SIZE', 128)
dpdk_conf.set('RTE_MACHINE_CPUFLAG_ALTIVEC', 1)
dpdk_conf.set('RTE_MACHINE_CPUFLAG_VSX', 1)
