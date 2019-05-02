# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

# get binutils version for the workaround of Bug 97
if not is_windows
	ldver = run_command('ld', '-v').stdout().strip()
	if ldver.contains('2.30') and cc.has_argument('-mno-avx512f')
		machine_args += '-mno-avx512f'
		message('Binutils 2.30 detected, disabling AVX512 support as workaround for bug #97')
	endif
	if ldver.contains('2.31') and cc.has_argument('-mno-avx512f')
		machine_args += '-mno-avx512f'
		message('Binutils 2.31 detected, disabling AVX512 support as workaround for bug #249')
	endif
endif

# we require SSE4.2 for DPDK
sse_errormsg = '''SSE4.2 instruction set is required for DPDK.
Please set the machine type to "nehalem" or "corei7" or higher value'''

if cc.get_define('__SSE4_2__', args: machine_args) == ''
	error(sse_errormsg)
endif

base_flags = ['SSE', 'SSE2', 'SSE3','SSSE3', 'SSE4_1', 'SSE4_2']
foreach f:base_flags
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_' + f, 1)
	compile_time_cpuflags += ['RTE_CPUFLAG_' + f]
endforeach

dpdk_conf.set('RTE_ARCH_X86', 1)
if dpdk_conf.get('RTE_ARCH_64')
	dpdk_conf.set('RTE_ARCH_X86_64', 1)
	dpdk_conf.set('RTE_ARCH', 'x86_64')
else
	dpdk_conf.set('RTE_ARCH_I686', 1)
	dpdk_conf.set('RTE_ARCH', 'i686')
endif

if cc.get_define('__AES__', args: machine_args) != ''
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_AES', 1)
	compile_time_cpuflags += ['RTE_CPUFLAG_AES']
endif
if cc.get_define('__PCLMUL__', args: machine_args) != ''
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_PCLMULQDQ', 1)
	compile_time_cpuflags += ['RTE_CPUFLAG_PCLMULQDQ']
endif
if cc.get_define('__AVX__', args: machine_args) != ''
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_AVX', 1)
	compile_time_cpuflags += ['RTE_CPUFLAG_AVX']
endif
if cc.get_define('__AVX2__', args: machine_args) != ''
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_AVX2', 1)
	compile_time_cpuflags += ['RTE_CPUFLAG_AVX2']
endif
if cc.get_define('__AVX512F__', args: machine_args) != ''
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_AVX512F', 1)
	compile_time_cpuflags += ['RTE_CPUFLAG_AVX512F']
endif

dpdk_conf.set('RTE_CACHE_LINE_SIZE', 64)
