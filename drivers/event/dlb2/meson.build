# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019-2020 Intel Corporation

if not is_linux or not dpdk_conf.has('RTE_ARCH_X86_64')
        build = false
        reason = 'only supported on x86_64 Linux'
        subdir_done()
endif

if dpdk_conf.get('RTE_IOVA_IN_MBUF') == 0
    subdir_done()
endif

sources = files(
        'dlb2.c',
        'dlb2_iface.c',
        'dlb2_xstats.c',
        'pf/dlb2_main.c',
        'pf/dlb2_pf.c',
        'pf/base/dlb2_resource.c',
        'rte_pmd_dlb2.c',
        'dlb2_selftest.c',
        'dlb2_sse.c',
)

sources_avx512 += files('dlb2_avx512.c')

headers = files('rte_pmd_dlb2.h')

deps += ['mbuf', 'mempool', 'ring', 'pci', 'bus_pci']

dlb_pmd_defines = ['-DDLB2_BYPASS_FENCE_ON_PP', '-DDLB_HW_CREDITS_CHECKS', '-DDLB_SW_CREDITS_CHECKS', '-DDLB_TYPE_CHECK']
dlb_pmd_default = ['0','1','1','1']

c_args=get_option('c_args')
index = 0
foreach opt: dlb_pmd_defines
    opt_true = opt + '=1'
    opt_false = opt + '=0'
    if not (c_args.contains(opt_true) or c_args.contains(opt_false))
        cflags += opt + '=' + dlb_pmd_default[index]
    endif

    index += 1
endforeach
