# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2021 NXP

if not is_linux
    build = false
    reason = 'only supported on Linux'
endif

deps += ['bus_dpaa', 'mempool_dpaa', 'security']
sources = files('dpaa_sec.c', 'dpaa_sec_raw_dp.c')

includes += include_directories('../../bus/dpaa/include')
includes += include_directories('../../common/dpaax')
includes += include_directories('../../common/dpaax/caamflib/')
