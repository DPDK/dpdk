# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Microsoft Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files('rte_pcapng.c')
headers = files('rte_pcapng.h')

deps += ['ethdev']
