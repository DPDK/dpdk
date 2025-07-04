# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

cflags += no_wvla_cflag

headers = files('rte_member.h')

sources = files(
        'rte_member.c',
        'rte_member_ht.c',
        'rte_member_sketch.c',
        'rte_member_vbf.c',
)

deps += ['hash', 'ring']

# compile AVX512 version if we have avx512 on MSVC or the 'ifma' flag on GCC/Clang
if dpdk_conf.has('RTE_ARCH_X86_64')
    if is_ms_compiler
        sources_avx512 += files('rte_member_sketch_avx512.c')
    elif cc.has_argument('-mavx512ifma')
        sources_avx512 += files('rte_member_sketch_avx512.c')
        cflags_avx512 += '-mavx512ifma'
    endif
endif
