# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2025 Marvell International Ltd.

if not is_linux
    build = false
    reason = 'only supported on Linux'
    subdir_done()
endif

headers = files('rte_pmu.h')
sources = files('pmu.c')

deps += ['log']
