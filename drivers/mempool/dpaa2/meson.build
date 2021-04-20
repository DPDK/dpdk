# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

if not is_linux
    build = false
    reason = 'only supported on Linux'
endif

deps += ['bus_fslmc']
sources = files('dpaa2_hw_mempool.c')

headers = files('rte_dpaa2_mempool.h')
