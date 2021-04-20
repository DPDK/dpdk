# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

if not is_linux
    build = false
    reason = 'only supported on Linux'
endif

deps += ['bus_dpaa']
sources = files('dpaa_mempool.c')
