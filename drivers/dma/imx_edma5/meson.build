# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP

if not is_linux
    build = false
    reason = 'only supported on linux'
endif

deps += ['bus_platform', 'dmadev']
sources = files('imx_edma5_dmadev.c')
