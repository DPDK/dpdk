# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Vladimir Medvedkin <medvedkinv@gmail.com>
# Copyright(c) 2019 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files('rte_rib.c', 'rte_rib6.c')
headers = files('rte_rib.h', 'rte_rib6.h')
deps += ['mempool']
