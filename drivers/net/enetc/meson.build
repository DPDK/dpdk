# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018,2024 NXP

if not is_linux
    build = false
    reason = 'only supported on Linux'
endif

deps += ['common_dpaax']
sources = files(
        'enetc_ethdev.c',
        'enetc_rxtx.c',
        'enetc4_ethdev.c',
        'enetc4_vf.c',
)

includes += include_directories('base')
