# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

cflags += ['-DRTE_LIBRTE_IXGBE_BYPASS']

base_sources = files(
        'base/ixgbe_82598.c',
        'base/ixgbe_82599.c',
        'base/ixgbe_api.c',
        'base/ixgbe_common.c',
        'base/ixgbe_dcb_82598.c',
        'base/ixgbe_dcb_82599.c',
        'base/ixgbe_dcb.c',
        'base/ixgbe_e610.c',
        'base/ixgbe_hv_vf.c',
        'base/ixgbe_mbx.c',
        'base/ixgbe_phy.c',
        'base/ixgbe_vf.c',
        'base/ixgbe_x540.c',
        'base/ixgbe_x550.c',
)

sources += files(
        'ixgbe_82599_bypass.c',
        'ixgbe_bypass.c',
        'ixgbe_ethdev.c',
        'ixgbe_fdir.c',
        'ixgbe_flow.c',
        'ixgbe_ipsec.c',
        'ixgbe_pf.c',
        'ixgbe_rxtx.c',
        'ixgbe_tm.c',
        'ixgbe_vf_representor.c',
        'rte_pmd_ixgbe.c',
)

testpmd_sources = files('ixgbe_testpmd.c')

deps += ['hash', 'security']

if arch_subdir == 'x86'
    sources += files('ixgbe_rxtx_vec_common.c')
    sources += files('ixgbe_rxtx_vec_sse.c')
    cflags += ['-DIXGBE_VPMD_SUPPORTED']
elif arch_subdir == 'arm'
    sources += files('ixgbe_rxtx_vec_common.c')
    sources += files('ixgbe_rxtx_vec_neon.c')
    cflags += ['-DIXGBE_VPMD_SUPPORTED']
endif

headers = files('rte_pmd_ixgbe.h')
