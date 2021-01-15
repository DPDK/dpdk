# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation.
# Copyright(c) 2017 Cavium, Inc

arm_force_native_march = false

# common flags to all aarch64 builds, with lowest priority
flags_common = [
	# Accelerate rte_memcpy. Be sure to run unit test (memcpy_perf_autotest)
	# to determine the best threshold in code. Refer to notes in source file
	# (lib/librte_eal/arm/include/rte_memcpy_64.h) for more info.
	['RTE_ARCH_ARM64_MEMCPY', false],
	#	['RTE_ARM64_MEMCPY_ALIGNED_THRESHOLD', 2048],
	#	['RTE_ARM64_MEMCPY_UNALIGNED_THRESHOLD', 512],
	# Leave below RTE_ARM64_MEMCPY_xxx options commented out,
	# unless there are strong reasons.
	#	['RTE_ARM64_MEMCPY_SKIP_GCC_VER_CHECK', false],
	#	['RTE_ARM64_MEMCPY_ALIGN_MASK', 0xF],
	#	['RTE_ARM64_MEMCPY_STRICT_ALIGN', false],

	['RTE_NET_FM10K', false],
	['RTE_NET_AVP', false],

	['RTE_SCHED_VECTOR', false],
	['RTE_ARM_USE_WFE', false],
	['RTE_ARCH_ARM64', true],
	['RTE_CACHE_LINE_SIZE', 128]
]

# implementer specific aarch64 flags, with middle priority
# (will overwrite common flags)
flags_implementer_generic = [
	['RTE_MACHINE', '"armv8a"'],
	['RTE_USE_C11_MEM_MODEL', true],
	['RTE_MAX_LCORE', 256]
]
flags_implementer_arm = [
	['RTE_MACHINE', '"armv8a"'],
	['RTE_USE_C11_MEM_MODEL', true],
	['RTE_CACHE_LINE_SIZE', 64],
	['RTE_MAX_LCORE', 16]
]
flags_implementer_cavium = [
	['RTE_MAX_VFIO_GROUPS', 128],
	['RTE_MAX_LCORE', 96],
	['RTE_MAX_NUMA_NODES', 2]
]
flags_implementer_dpaa = [
	['RTE_MACHINE', '"dpaa"'],
	['RTE_LIBRTE_DPAA2_USE_PHYS_IOVA', false],
	['RTE_USE_C11_MEM_MODEL', true],
	['RTE_CACHE_LINE_SIZE', 64],
	['RTE_MAX_LCORE', 16],
	['RTE_MAX_NUMA_NODES', 1]
]
flags_implementer_emag = [
	['RTE_MACHINE', '"emag"'],
	['RTE_CACHE_LINE_SIZE', 64],
	['RTE_MAX_LCORE', 32],
	['RTE_MAX_NUMA_NODES', 1]
]
flags_implementer_armada = [
	['RTE_MACHINE', '"armv8a"'],
	['RTE_CACHE_LINE_SIZE', 64],
	['RTE_MAX_LCORE', 16],
	['RTE_MAX_NUMA_NODES', 1]
]

# part number specific aarch64 flags, with highest priority
# (will overwrite both common and implementer specific flags)
flags_part_number_thunderx = [
	['RTE_MACHINE', '"thunderx"'],
	['RTE_USE_C11_MEM_MODEL', false]
]
flags_part_number_thunderx2 = [
	['RTE_MACHINE', '"thunderx2"'],
	['RTE_ARM_FEATURE_ATOMICS', true],
	['RTE_USE_C11_MEM_MODEL', true],
	['RTE_CACHE_LINE_SIZE', 64],
	['RTE_MAX_LCORE', 256],
	['RTE_MAX_NUMA_NODES', 2]
]
flags_part_number_octeontx2 = [
	['RTE_MACHINE', '"octeontx2"'],
	['RTE_ARM_FEATURE_ATOMICS', true],
	['RTE_USE_C11_MEM_MODEL', true],
	['RTE_EAL_IGB_UIO', false],
	['RTE_MAX_LCORE', 36],
	['RTE_MAX_NUMA_NODES', 1]
]
flags_part_number_n1generic = [
	['RTE_MACHINE', '"neoverse-n1"'],
	['RTE_ARM_FEATURE_ATOMICS', true],
	['RTE_USE_C11_MEM_MODEL', true],
	['RTE_EAL_NUMA_AWARE_HUGEPAGES', false],
	['RTE_LIBRTE_VHOST_NUMA', false],
	['RTE_MAX_MEM_MB', 1048576],
	['RTE_CACHE_LINE_SIZE', 64],
	['RTE_MAX_LCORE', 64],
	['RTE_MAX_NUMA_NODES', 1]
]
flags_part_number_n2generic = [
	['RTE_MACHINE', '"neoverse-n2"'],
	['RTE_ARM_FEATURE_ATOMICS', true],
	['RTE_USE_C11_MEM_MODEL', true],
	['RTE_EAL_NUMA_AWARE_HUGEPAGES', false],
	['RTE_LIBRTE_VHOST_NUMA', false],
	['RTE_MAX_LCORE', 64],
	['RTE_CACHE_LINE_SIZE', 64]
]

# arm config (implementer 0x41) is the default config
part_number_config_arm = [
	['generic', ['-march=armv8-a+crc', '-moutline-atomics']],
	['native', ['-march=native']],
	['0xd03', ['-mcpu=cortex-a53']],
	['0xd04', ['-mcpu=cortex-a35']],
	['0xd07', ['-mcpu=cortex-a57']],
	['0xd08', ['-mcpu=cortex-a72']],
	['0xd09', ['-mcpu=cortex-a73']],
	['0xd0a', ['-mcpu=cortex-a75']],
	['0xd0b', ['-mcpu=cortex-a76']],
	['0xd0c', ['-march=armv8.2-a+crypto', '-mcpu=neoverse-n1'], flags_part_number_n1generic],
	['0xd49', ['-march=armv8.5-a+crypto+sve2'], flags_part_number_n2generic]
]

part_number_config_cavium = [
	['generic', ['-march=armv8-a+crc+crypto','-mcpu=thunderx']],
	['native', ['-march=native']],
	['0xa1', ['-mcpu=thunderxt88'], flags_part_number_thunderx],
	['0xa2', ['-mcpu=thunderxt81'], flags_part_number_thunderx],
	['0xa3', ['-mcpu=thunderxt83'], flags_part_number_thunderx],
	['0xaf', ['-march=armv8.1-a+crc+crypto','-mcpu=thunderx2t99'], flags_part_number_thunderx2],
	['0xb2', ['-march=armv8.2-a+crc+crypto+lse','-mcpu=octeontx2'], flags_part_number_octeontx2]
]
part_number_config_emag = [
	['generic', ['-march=armv8-a+crc+crypto', '-mtune=emag']],
	['native', ['-march=native']]
]

## Arm implementer ID (MIDR in Arm Architecture Reference Manual)
implementer_generic = ['Generic armv8', flags_implementer_generic, part_number_config_arm]
implementer_0x41 = ['Arm', flags_implementer_arm, part_number_config_arm]
implementer_0x43 = ['Cavium', flags_implementer_cavium, part_number_config_cavium]
implementer_0x50 = ['Ampere Computing', flags_implementer_emag, part_number_config_emag]
implementer_0x56 = ['Marvell ARMADA', flags_implementer_armada, part_number_config_arm]
implementer_dpaa = ['NXP DPAA', flags_implementer_dpaa, part_number_config_arm]

dpdk_conf.set('RTE_ARCH_ARM', 1)
dpdk_conf.set('RTE_FORCE_INTRINSICS', 1)

if dpdk_conf.get('RTE_ARCH_32')
	# armv7 build
	dpdk_conf.set('RTE_CACHE_LINE_SIZE', 64)
	dpdk_conf.set('RTE_ARCH_ARMv7', 1)
	# the minimum architecture supported, armv7-a, needs the following,
	machine_args += '-mfpu=neon'
else
	# aarch64 build
	implementer_id = 'generic'
	machine_args = [] # Clear previous machine args
	if machine == 'default' and not meson.is_cross_build()
		# default build
		implementer_config = implementer_generic
		part_number = 'generic'
	elif not meson.is_cross_build()
		# native build
		# The script returns ['Implementer', 'Variant', 'Architecture',
		# 'Primary Part number', 'Revision']
		detect_vendor = find_program(join_paths(
				meson.current_source_dir(), 'armv8_machine.py'))
		cmd = run_command(detect_vendor.path())
		if cmd.returncode() == 0
			cmd_output = cmd.stdout().to_lower().strip().split(' ')
			implementer_id = cmd_output[0]
			part_number = cmd_output[3]
		endif
		# Set to generic if variable is not found
		implementer_config = get_variable('implementer_' + implementer_id, ['generic'])
		if implementer_config[0] == 'generic'
			implementer_config = implementer_generic
			part_number = 'generic'
		endif
		if arm_force_native_march == true
			part_number = 'native'
		endif
	else
		# cross build
		implementer_id = meson.get_cross_property('implementer_id', 'generic')
		part_number = meson.get_cross_property('part_number', 'generic')
		implementer_config = get_variable('implementer_' + implementer_id)
	endif

	# Apply Common Defaults. These settings may be overwritten by machine
	# settings later.
	foreach flag: flags_common
		if flag.length() > 0
			dpdk_conf.set(flag[0], flag[1])
		endif
	endforeach

	message('Implementer : ' + implementer_config[0])
	foreach flag: implementer_config[1]
		if flag.length() > 0
			dpdk_conf.set(flag[0], flag[1])
		endif
	endforeach

	foreach marg: implementer_config[2]
		if marg[0] == part_number
			foreach flag: marg[1]
				if cc.has_argument(flag)
					machine_args += flag
				endif
			endforeach
			# Apply any extra machine specific flags.
			foreach flag: marg.get(2, [])
				if flag.length() > 0
					dpdk_conf.set(flag[0], flag[1])
				endif
			endforeach
		endif
	endforeach
endif
message('Using machine args: @0@'.format(machine_args))

if (cc.get_define('__ARM_NEON', args: machine_args) != '' or
    cc.get_define('__aarch64__', args: machine_args) != '')
	compile_time_cpuflags += ['RTE_CPUFLAG_NEON']
endif

if cc.get_define('__ARM_FEATURE_CRC32', args: machine_args) != ''
	compile_time_cpuflags += ['RTE_CPUFLAG_CRC32']
endif

if cc.get_define('__ARM_FEATURE_CRYPTO', args: machine_args) != ''
	compile_time_cpuflags += ['RTE_CPUFLAG_AES', 'RTE_CPUFLAG_PMULL',
	'RTE_CPUFLAG_SHA1', 'RTE_CPUFLAG_SHA2']
endif
