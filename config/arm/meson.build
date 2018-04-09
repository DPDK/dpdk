# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation.
# Copyright(c) 2017 Cavium, Inc

# for checking defines we need to use the correct compiler flags
march_opt = '-march=@0@'.format(machine)

arm_force_native_march = false

machine_args_generic = [
	['default', ['-march=armv8-a+crc+crypto']],
	['native', ['-march=native']],
	['0xd03', ['-mcpu=cortex-a53']],
	['0xd04', ['-mcpu=cortex-a35']],
	['0xd07', ['-mcpu=cortex-a57']],
	['0xd08', ['-mcpu=cortex-a72']],
	['0xd09', ['-mcpu=cortex-a73']],
	['0xd0a', ['-mcpu=cortex-a75']],
]
machine_args_cavium = [
	['default', ['-march=armv8-a+crc+crypto','-mcpu=thunderx']],
	['native', ['-march=native']],
	['0xa1', ['-mcpu=thunderxt88']],
	['0xa2', ['-mcpu=thunderxt81']],
	['0xa3', ['-mcpu=thunderxt83']]]

flags_common_default = [
	# Accelarate rte_memcpy. Be sure to run unit test (memcpy_perf_autotest)
	# to determine the best threshold in code. Refer to notes in source file
	# (lib/librte_eal/common/include/arch/arm/rte_memcpy_64.h) for more info.
	['RTE_ARCH_ARM64_MEMCPY', false],
	#	['RTE_ARM64_MEMCPY_ALIGNED_THRESHOLD', 2048],
	#	['RTE_ARM64_MEMCPY_UNALIGNED_THRESHOLD', 512],
	# Leave below RTE_ARM64_MEMCPY_xxx options commented out, unless there're
	# strong reasons.
	#	['RTE_ARM64_MEMCPY_SKIP_GCC_VER_CHECK', false],
	#	['RTE_ARM64_MEMCPY_ALIGN_MASK', 0xF],
	#	['RTE_ARM64_MEMCPY_STRICT_ALIGN', false],

	['RTE_LIBRTE_FM10K_PMD', false],
	['RTE_LIBRTE_SFC_EFX_PMD', false],
	['RTE_LIBRTE_AVP_PMD', false],

	['RTE_SCHED_VECTOR', false],
]

flags_generic = [
	['RTE_MACHINE', '"armv8a"'],
	['RTE_CACHE_LINE_SIZE', 128]]
flags_cavium = [
	['RTE_MACHINE', '"thunderx"'],
	['RTE_CACHE_LINE_SIZE', 128],
	['RTE_MAX_NUMA_NODES', 2],
	['RTE_MAX_LCORE', 96],
	['RTE_MAX_VFIO_GROUPS', 128],
	['RTE_RING_USE_C11_MEM_MODEL', false]]
flags_dpaa = [
	['RTE_MACHINE', '"dpaa"'],
	['RTE_CACHE_LINE_SIZE', 64],
	['RTE_MAX_NUMA_NODES', 1],
	['RTE_MAX_LCORE', 16]]
flags_dpaa2 = [
	['RTE_MACHINE', '"dpaa2"'],
	['RTE_CACHE_LINE_SIZE', 64],
	['RTE_MAX_NUMA_NODES', 1],
	['RTE_MAX_LCORE', 16],
	['RTE_LIBRTE_DPAA2_USE_PHYS_IOVA', 'n']]

## Arm implementer ID (ARM DDI 0487C.a, Section G7.2.106, Page G7-5321)
impl_generic = ['Generic armv8', flags_generic, machine_args_generic]
impl_0x41 = ['Arm', flags_generic, machine_args_generic]
impl_0x42 = ['Broadcom', flags_generic, machine_args_generic]
impl_0x43 = ['Cavium', flags_cavium, machine_args_cavium]
impl_0x44 = ['DEC', flags_generic, machine_args_generic]
impl_0x49 = ['Infineon', flags_generic, machine_args_generic]
impl_0x4d = ['Motorola', flags_generic, machine_args_generic]
impl_0x4e = ['NVIDIA', flags_generic, machine_args_generic]
impl_0x50 = ['AppliedMicro', flags_generic, machine_args_generic]
impl_0x51 = ['Qualcomm', flags_generic, machine_args_generic]
impl_0x53 = ['Samsung', flags_generic, machine_args_generic]
impl_0x56 = ['Marvell', flags_generic, machine_args_generic]
impl_0x69 = ['Intel', flags_generic, machine_args_generic]
impl_dpaa = ['NXP DPAA', flags_dpaa, machine_args_generic]
impl_dpaa2 = ['NXP DPAA2', flags_dpaa2, machine_args_generic]

dpdk_conf.set('RTE_FORCE_INTRINSICS', 1)

if cc.sizeof('void *') != 8
	dpdk_conf.set('RTE_CACHE_LINE_SIZE', 64)
	dpdk_conf.set('RTE_ARCH_ARM', 1)
	dpdk_conf.set('RTE_ARCH_ARMv7', 1)
else
	dpdk_conf.set('RTE_CACHE_LINE_SIZE', 128)
	dpdk_conf.set('RTE_ARCH_ARM64', 1)
	dpdk_conf.set('RTE_ARCH_64', 1)

	machine = []
	cmd_generic = ['generic', '', '', 'default', '']
	cmd_output = cmd_generic # Set generic by default
	machine_args = [] # Clear previous machine args
	if not meson.is_cross_build()
		# The script returns ['Implementer', 'Variant', 'Architecture',
		# 'Primary Part number', 'Revision']
		detect_vendor = find_program(join_paths(
				meson.current_source_dir(), 'armv8_machine.py'))
		cmd = run_command(detect_vendor.path())
		if cmd.returncode() == 0
			cmd_output = cmd.stdout().to_lower().strip().split(' ')
		endif
		# Set to generic if variable is not found
		machine = get_variable('impl_' + cmd_output[0], 'generic')
		if machine == 'generic'
			machine = impl_generic
			cmd_output = cmd_generic
		endif
		impl_pn = cmd_output[3]
		if arm_force_native_march == true
			impl_pn = 'native'
		endif
	else
		impl_id = meson.get_cross_property('implementor_id', 'generic')
		impl_pn = meson.get_cross_property('implementor_pn', 'default')
		machine = get_variable('impl_' + impl_id)
	endif

	# Apply Common Defaults. These settings may be overwritten by machine
	# settings later.
	foreach flag: flags_common_default
		if flag.length() > 0
			dpdk_conf.set(flag[0], flag[1])
		endif
	endforeach

	message('Implementer : ' + machine[0])
	foreach flag: machine[1]
		if flag.length() > 0
			dpdk_conf.set(flag[0], flag[1])
		endif
	endforeach
	# Primary part number based mcpu flags are supported
	# for gcc versions > 7
	if cc.version().version_compare(
			'<7.0') or cmd_output.length() == 0
		if not meson.is_cross_build() and arm_force_native_march == true
			impl_pn = 'native'
		else
			impl_pn = 'default'
		endif
	endif
	foreach marg: machine[2]
		if marg[0] == impl_pn
			foreach f: marg[1]
				machine_args += f
			endforeach
		endif
	endforeach
endif
message(machine_args)

if cc.get_define('__ARM_NEON', args: machine_args) != ''
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_NEON', 1)
	compile_time_cpuflags += ['RTE_CPUFLAG_NEON']
endif

if cc.get_define('__ARM_FEATURE_CRC32', args: machine_args) != ''
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_CRC32', 1)
	compile_time_cpuflags += ['RTE_CPUFLAG_CRC32']
endif

if cc.get_define('__ARM_FEATURE_CRYPTO', args: machine_args) != ''
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_AES', 1)
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_PMULL', 1)
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_SHA1', 1)
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_SHA2', 1)
	compile_time_cpuflags += ['RTE_CPUFLAG_AES', 'RTE_CPUFLAG_PMULL',
	'RTE_CPUFLAG_SHA1', 'RTE_CPUFLAG_SHA2']
endif
