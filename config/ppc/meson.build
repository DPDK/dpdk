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

# Certain POWER9 systems can scale as high as 1536 LCORES, but setting such a
# high value can waste memory, cause timeouts in time limited autotests, and is
# unlikely to be used in many production situations.  Similarly, keeping the
# default 64 LCORES seems too small as most POWER9 dual socket systems will have
# at least 128 LCORES available.  Set RTE_MAX_LCORE to 128 for POWER systems as
# a compromise.
dpdk_conf.set('RTE_MAX_LCORE', 128)

# POWER systems do not allocate NUMA nodes sequentially.  A dual socket system
# will have CPUs associated with NUMA nodes 0 & 8, so ensure that the second
# NUMA node will be supported by setting RTE_MAX_NUMA_NODES to 16. High end
# systems can scale even higher with as many as 32 NUMA nodes.
dpdk_conf.set('RTE_MAX_NUMA_NODES', 16)

dpdk_conf.set('RTE_CACHE_LINE_SIZE', 128)
