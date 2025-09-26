# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

subdir('base')

sources = files(
        'txgbe_ethdev.c',
        'txgbe_ethdev_vf.c',
        'txgbe_fdir.c',
        'txgbe_flow.c',
        'txgbe_ipsec.c',
        'txgbe_ptypes.c',
        'txgbe_pf.c',
        'txgbe_rxtx.c',
        'txgbe_tm.c',
)

deps += ['hash', 'security']

cflags += no_wvla_cflag

if arch_subdir == 'x86'
    sources += files('txgbe_rxtx_vec_sse.c')
elif arch_subdir == 'arm'
    sources += files('txgbe_rxtx_vec_neon.c')
endif

headers = files('rte_pmd_txgbe.h')
