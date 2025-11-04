# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files('rte_pdump.c')
headers = files('rte_pdump.h')
deps += ['ethdev', 'bpf', 'pcapng']
