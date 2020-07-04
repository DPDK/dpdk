# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

# get binutils version for the workaround of Bug 97
if not is_windows
	binutils_ok = run_command(binutils_avx512_check)
	if binutils_ok.returncode() != 0 and cc.has_argument('-mno-avx512f')
		machine_args += '-mno-avx512f'
		warning('Binutils error with AVX512 assembly, disabling AVX512 support')
	endif
endif

# we require SSE4.2 for DPDK
if cc.get_define('__SSE4_2__', args: machine_args) == ''
	message('SSE 4.2 not enabled by default, explicitly enabling')
	machine_args += '-msse4'
endif

base_flags = ['SSE', 'SSE2', 'SSE3','SSSE3', 'SSE4_1', 'SSE4_2']
foreach f:base_flags
	dpdk_conf.set('RTE_MACHINE_CPUFLAG_' + f, 1)
	compile_time_cpuflags += ['RTE_CPUFLAG_' + f]
endforeach

optional_flags = ['AES', 'PCLMUL',
		'AVX', 'AVX2', 'AVX512F',
		'RDRND', 'RDSEED']
foreach f:optional_flags
	if cc.get_define('__@0@__'.format(f), args: machine_args) == '1'
		if f == 'PCLMUL' # special case flags with different defines
			f = 'PCLMULQDQ'
		elif f == 'RDRND'
			f = 'RDRAND'
		endif
		dpdk_conf.set('RTE_MACHINE_CPUFLAG_' + f, 1)
		compile_time_cpuflags += ['RTE_CPUFLAG_' + f]
	endif
endforeach


dpdk_conf.set('RTE_ARCH_X86', 1)
if dpdk_conf.get('RTE_ARCH_64')
	dpdk_conf.set('RTE_ARCH_X86_64', 1)
	dpdk_conf.set('RTE_ARCH', 'x86_64')
else
	dpdk_conf.set('RTE_ARCH_I686', 1)
	dpdk_conf.set('RTE_ARCH', 'i686')
endif

dpdk_conf.set('RTE_CACHE_LINE_SIZE', 64)
