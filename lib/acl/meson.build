# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

cflags += no_wvla_cflag

sources = files('acl_bld.c', 'acl_gen.c', 'acl_run_scalar.c',
        'rte_acl.c', 'tb_mem.c')
headers = files('rte_acl.h', 'rte_acl_osdep.h')

if dpdk_conf.has('RTE_ARCH_X86')
    sources += files('acl_run_sse.c')
    sources_avx2 += files('acl_run_avx2.c')
    # AVX512 is only supported on 64-bit builds
    if dpdk_conf.has('RTE_ARCH_X86_64')
        sources_avx512 += files('acl_run_avx512.c')
    endif
elif dpdk_conf.has('RTE_ARCH_ARM')
    cflags += '-flax-vector-conversions'
    sources += files('acl_run_neon.c')
elif dpdk_conf.has('RTE_ARCH_PPC_64')
    sources += files('acl_run_altivec.c')
endif
