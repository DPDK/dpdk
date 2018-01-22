# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation.
# Copyright(c) 2017 Cavium, Inc

# for checking defines we need to use the correct compiler flags
march_opt = '-march=@0@'.format(machine)

machine_args_generic = [
	['default', ['-march=armv8-a+crc+crypto']]]
machine_args_cavium = [
	['default', ['-march=armv8-a+crc+crypto','-mcpu=thunderx']],
	['0xa1', ['-mcpu=thunderxt88']],
	['0xa2', ['-mcpu=thunderxt81']],
	['0xa3', ['-mcpu=thunderxt83']]]

flags_generic = [[]]
flags_cavium = [
	['RTE_MACHINE', '"thunderx"'],
	['RTE_CACHE_LINE_SIZE', 128],
	['RTE_MAX_NUMA_NODES', 2],
	['RTE_MAX_LCORE', 96],
	['RTE_MAX_VFIO_GROUPS', 128],
	['RTE_RING_USE_C11_MEM_MODEL', false]]

impl_generic = ['Generic armv8', flags_generic, machine_args_generic]
impl_0x43 = ['Cavium', flags_cavium, machine_args_cavium]

if cc.get_define('__clang__') != ''
	dpdk_conf.set_quoted('RTE_TOOLCHAIN', 'clang')
	dpdk_conf.set('RTE_TOOLCHAIN_CLANG', 1)
else
	dpdk_conf.set_quoted('RTE_TOOLCHAIN', 'gcc')
	dpdk_conf.set('RTE_TOOLCHAIN_GCC', 1)
endif

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
			cmd_output = cmd.stdout().strip().split(' ')
		endif
		# Set to generic if variable is not found
		machine = get_variable('impl_' + cmd_output[0], 'generic')
	else
		impl_id = meson.get_cross_property('implementor_id', 'generic')
		machine = get_variable('impl_' + impl_id)
	endif

	if machine == 'generic'
		machine = impl_generic
		cmd_output = cmd_generic
	endif

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
		foreach marg: machine[2]
			if marg[0] == 'default'
				foreach f: marg[1]
					machine_args += f
				endforeach
			endif
		endforeach
	else
		foreach marg: machine[2]
			if marg[0] == cmd_output[3]
				foreach f: marg[1]
					machine_args += f
				endforeach
			endif
		endforeach
	endif
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
