# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018,2021 NXP

if not is_linux
    build = false
    reason = 'only supported on Linux'
endif

deps += ['security', 'mempool_dpaa2']
sources = files(
        'dpaa2_sec_dpseci.c',
        'dpaa2_sec_raw_dp.c',
        'mc/dpseci.c',
)

includes += include_directories('mc', '../../common/dpaax', '../../common/dpaax/caamflib')
