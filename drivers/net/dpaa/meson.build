# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2020,2026 NXP

if not is_linux
    build = false
    reason = 'only supported on Linux'
endif
deps += ['mempool_dpaa']

sources = files(
        'dpaa_ethdev.c',
        'dpaa_flow.c',
        'dpaa_fmc.c',
        'dpaa_ptp.c',
        'dpaa_oldev.c',
        'dpaa_rxtx.c',
        'fmlib/fm_lib.c',
        'fmlib/fm_vsp.c',
)

cflags += no_wvla_cflag
cflags += no_shadow_cflag

if cc.has_argument('-Wno-pointer-arith')
    cflags += '-Wno-pointer-arith'
endif

headers = files('rte_pmd_dpaa.h', 'rte_pmd_dpaa_oldev.h')
