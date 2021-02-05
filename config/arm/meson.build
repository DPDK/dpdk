# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation.
# Copyright(c) 2017 Cavium, Inc

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

## Part numbers are specific to Arm implementers
# implementer specific aarch64 flags have middle priority
#     (will overwrite common flags)
# part number specific aarch64 flags have the highest priority
#     (will overwrite both common and implementer specific flags)
implementer_generic = {
	'description': 'Generic armv8',
	'flags': [
		['RTE_MACHINE', '"armv8a"'],
		['RTE_USE_C11_MEM_MODEL', true],
		['RTE_MAX_LCORE', 256],
		['RTE_MAX_NUMA_NODES', 4]
	],
	'part_number_config': {
		'generic': {'machine_args': ['-march=armv8-a+crc',
					     '-moutline-atomics']}
	}
}

part_number_config_arm = {
	'0xd03': {'machine_args':  ['-mcpu=cortex-a53']},
	'0xd04': {'machine_args':  ['-mcpu=cortex-a35']},
	'0xd07': {'machine_args':  ['-mcpu=cortex-a57']},
	'0xd08': {'machine_args':  ['-mcpu=cortex-a72']},
	'0xd09': {'machine_args':  ['-mcpu=cortex-a73']},
	'0xd0a': {'machine_args':  ['-mcpu=cortex-a75']},
	'0xd0b': {'machine_args':  ['-mcpu=cortex-a76']},
	'0xd0c': {
		'machine_args':  ['-march=armv8.2-a+crypto',
				  '-mcpu=neoverse-n1'],
		'flags': [
			['RTE_MACHINE', '"neoverse-n1"'],
			['RTE_ARM_FEATURE_ATOMICS', true],
			['RTE_MAX_MEM_MB', 1048576],
			['RTE_MAX_LCORE', 80]
		]
	},
	'0xd49': {
		'machine_args':  ['-march=armv8.5-a+crypto+sve2'],
		'flags': [
			['RTE_MACHINE', '"neoverse-n2"'],
			['RTE_ARM_FEATURE_ATOMICS', true],
			['RTE_MAX_LCORE', 64]
		]
	}
}
implementer_arm = {
	'description': 'Arm',
	'flags': [
		['RTE_MACHINE', '"armv8a"'],
		['RTE_USE_C11_MEM_MODEL', true],
		['RTE_CACHE_LINE_SIZE', 64],
		['RTE_MAX_LCORE', 16],
		['RTE_MAX_NUMA_NODES', 1]
	],
	'part_number_config': part_number_config_arm
}

flags_part_number_thunderx = [
	['RTE_MACHINE', '"thunderx"'],
	['RTE_USE_C11_MEM_MODEL', false]
]
implementer_cavium = {
	'description': 'Cavium',
	'flags': [
		['RTE_MAX_VFIO_GROUPS', 128],
		['RTE_MAX_LCORE', 96],
		['RTE_MAX_NUMA_NODES', 2]
	],
	'part_number_config': {
		'0xa1': {
			'machine_args': ['-mcpu=thunderxt88'],
			'flags': flags_part_number_thunderx
		},
		'0xa2': {
			'machine_args': ['-mcpu=thunderxt81'],
			'flags': flags_part_number_thunderx
		},
		'0xa3': {
			'machine_args': ['-mcpu=thunderxt83'],
			'flags': flags_part_number_thunderx
		},
		'0xaf': {
			'machine_args': ['-march=armv8.1-a+crc+crypto',
					 '-mcpu=thunderx2t99'],
			'flags': [
				['RTE_MACHINE', '"thunderx2"'],
				['RTE_ARM_FEATURE_ATOMICS', true],
				['RTE_USE_C11_MEM_MODEL', true],
				['RTE_CACHE_LINE_SIZE', 64],
				['RTE_MAX_LCORE', 256]
			]
		},
		'0xb2': {
			'machine_args': ['-march=armv8.2-a+crc+crypto+lse',
					 '-mcpu=octeontx2'],
			'flags': [
				['RTE_MACHINE', '"octeontx2"'],
				['RTE_ARM_FEATURE_ATOMICS', true],
				['RTE_USE_C11_MEM_MODEL', true],
				['RTE_EAL_IGB_UIO', false],
				['RTE_MAX_LCORE', 36],
				['RTE_MAX_NUMA_NODES', 1]
			]
		}
	}
}

implementer_ampere = {
	'description': 'Ampere Computing',
	'flags': [
		['RTE_MACHINE', '"emag"'],
		['RTE_CACHE_LINE_SIZE', 64],
		['RTE_MAX_LCORE', 32],
		['RTE_MAX_NUMA_NODES', 1]
	],
	'part_number_config': {
		'0x0': {'machine_args':  ['-march=armv8-a+crc+crypto',
					  '-mtune=emag']}
	}
}

implementer_qualcomm = {
	'description': 'Qualcomm',
	'flags': [
		['RTE_MACHINE', '"armv8a"'],
		['RTE_USE_C11_MEM_MODEL', true],
		['RTE_CACHE_LINE_SIZE', 64],
		['RTE_MAX_LCORE', 64],
		['RTE_MAX_NUMA_NODES', 1]
	],
	'part_number_config': {
		'0xc00': {'machine_args':  ['-march=armv8-a+crc']}
	}
}

implementer_marvell = {
	'description': 'Marvell ARMADA',
	'flags': [
		['RTE_MACHINE', '"armv8a"'],
		['RTE_CACHE_LINE_SIZE', 64],
		['RTE_MAX_LCORE', 16],
		['RTE_MAX_NUMA_NODES', 1]
	],
	'part_number_config': part_number_config_arm
}

implementer_dpaa = {
	'description': 'NXP DPAA',
	'flags': [
		['RTE_MACHINE', '"dpaa"'],
		['RTE_LIBRTE_DPAA2_USE_PHYS_IOVA', false],
		['RTE_USE_C11_MEM_MODEL', true],
		['RTE_CACHE_LINE_SIZE', 64],
		['RTE_MAX_LCORE', 16],
		['RTE_MAX_NUMA_NODES', 1]
	],
	'part_number_config': part_number_config_arm
}

## Arm implementers (ID from MIDR in Arm Architecture Reference Manual)
implementers = {
	'generic': implementer_generic,
	'0x41': implementer_arm,
	'0x43': implementer_cavium,
	'0x50': implementer_ampere,
	'0x51': implementer_qualcomm,
	'0x56': implementer_marvell,
	'dpaa': implementer_dpaa
}

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
	if not meson.is_cross_build()
		if machine == 'default'
			# default build
			implementer_id = 'generic'
			part_number = 'generic'
		else
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
			else
				error('Error when getting Arm Implementer ID and part number.')
			endif
		endif
	else
		# cross build
		implementer_id = meson.get_cross_property('implementer_id')
		part_number = meson.get_cross_property('part_number')
	endif

	if implementers.has_key(implementer_id)
		implementer_config = implementers[implementer_id]
	else
		error('Unsupported Arm implementer: @0@. '.format(implementer_id) +
		      'Please add support for it or use the generic ' +
		      '(-Dmachine=generic) build.')
	endif

	message('Arm implementer: ' + implementer_config['description'])
	message('Arm part number: ' + part_number)

	part_number_config = implementer_config['part_number_config']
	if part_number_config.has_key(part_number)
		# use the specified part_number machine args if found
		part_number_config = part_number_config[part_number]
	else
		# unknown part number
		error('Unsupported part number @0@ of implementer @1@. '
		      .format(part_number, implementer_id) +
		      'Please add support for it or use the generic ' +
		      '(-Dmachine=generic) build.')
	endif

	# use default flags with implementer flags
	dpdk_flags = flags_common + implementer_config['flags'] + part_number_config.get('flags', [])

	# apply supported machine args
	machine_args = [] # Clear previous machine args
	foreach flag: part_number_config['machine_args']
		if cc.has_argument(flag)
			machine_args += flag
		endif
	endforeach

	# apply flags
	foreach flag: dpdk_flags
		if flag.length() > 0
			dpdk_conf.set(flag[0], flag[1])
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
