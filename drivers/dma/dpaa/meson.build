# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021 NXP

if not is_linux
    build = false
    reason = 'only supported on linux'
endif

deps += ['dmadev', 'bus_dpaa']
sources = files('dpaa_qdma.c')

if cc.has_argument('-Wno-pointer-arith')
    cflags += '-Wno-pointer-arith'
endif
