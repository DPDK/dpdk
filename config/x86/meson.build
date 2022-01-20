# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2020 Intel Corporation

# get binutils version for the workaround of Bug 97
binutils_ok = true
if is_linux or cc.get_id() == 'gcc'
    binutils_ok = run_command(binutils_avx512_check, check: false).returncode() == 0
    if not binutils_ok and cc.has_argument('-mno-avx512f')
        machine_args += '-mno-avx512f'
        warning('Binutils error with AVX512 assembly, disabling AVX512 support')
    endif
endif

# check if compiler is working with _mm512_extracti64x4_epi64
# Ref: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=82887
if cc.has_argument('-mavx512f')
    code = '''#include <immintrin.h>
    void test(__m512i zmm){
        __m256i ymm = _mm512_extracti64x4_epi64(zmm, 0);}'''
    result = cc.compiles(code, args : '-mavx512f', name : 'AVX512 checking')
    if result == false
        machine_args += '-mno-avx512f'
        warning('Broken _mm512_extracti64x4_epi64, disabling AVX512 support')
    endif
endif

# we require SSE4.2 for DPDK
if cc.get_define('__SSE4_2__', args: machine_args) == ''
    message('SSE 4.2 not enabled by default, explicitly enabling')
    machine_args += '-msse4'
endif

base_flags = ['SSE', 'SSE2', 'SSE3','SSSE3', 'SSE4_1', 'SSE4_2']
foreach f:base_flags
    compile_time_cpuflags += ['RTE_CPUFLAG_' + f]
endforeach

optional_flags = [
        'AES',
        'AVX',
        'AVX2',
        'AVX512BW',
        'AVX512CD',
        'AVX512DQ',
        'AVX512F',
        'AVX512VL',
        'PCLMUL',
        'RDRND',
        'RDSEED',
        'VPCLMULQDQ',
]
foreach f:optional_flags
    if cc.get_define('__@0@__'.format(f), args: machine_args) == '1'
        if f == 'PCLMUL' # special case flags with different defines
            f = 'PCLMULQDQ'
        elif f == 'RDRND'
            f = 'RDRAND'
        endif
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
dpdk_conf.set('RTE_MAX_LCORE', 128)
dpdk_conf.set('RTE_MAX_NUMA_NODES', 32)
