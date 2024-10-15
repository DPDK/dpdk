# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, 2024 NXP

if not is_linux
    build = false
    reason = 'only supported on Linux'
endif

sources = files('dpaax_iova_table.c', 'dpaa_of.c', 'caamflib.c')

includes += include_directories('caamflib')

if cc.has_argument('-Wno-cast-qual')
    cflags += '-Wno-cast-qual'
endif
if cc.has_argument('-Wno-pointer-arith')
    cflags += '-Wno-pointer-arith'
endif
headers = files('rte_pmd_dpaax_qdma.h')
