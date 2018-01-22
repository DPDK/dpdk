# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation.
# Copyright(c) 2017 Cavium, Inc

# for checking defines we need to use the correct compiler flags
march_opt = '-march=@0@'.format(machine)

dpdk_conf.set('RTE_FORCE_INTRINSICS', 1)
if cc.sizeof('void *') == 8
	dpdk_conf.set('RTE_CACHE_LINE_SIZE', 128)
	dpdk_conf.set('RTE_ARCH_ARM64', 1)
	dpdk_conf.set('RTE_ARCH_64', 1)
else
	dpdk_conf.set('RTE_CACHE_LINE_SIZE', 64)
	dpdk_conf.set('RTE_ARCH_ARM', 1)
	dpdk_conf.set('RTE_ARCH_ARMv7', 1)
endif

if cc.get_define('__ARM_NEON', args: march_opt) != ''
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_NEON', 1)
	compile_time_cpuflags += ['RTE_CPUFLAG_NEON']
endif

if cc.get_define('__ARM_FEATURE_CRC32', args: march_opt) != ''
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_CRC32', 1)
	compile_time_cpuflags += ['RTE_CPUFLAG_CRC32']
endif

if cc.get_define('__ARM_FEATURE_CRYPTO', args: march_opt) != ''
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_AES', 1)
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_PMULL', 1)
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_SHA1', 1)
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_SHA2', 1)
	compile_time_cpuflags += ['RTE_CPUFLAG_AES', 'RTE_CPUFLAG_PMULL',
	'RTE_CPUFLAG_SHA1', 'RTE_CPUFLAG_SHA2']
endif
