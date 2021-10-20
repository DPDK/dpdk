# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files('rte_rawdev.c')
headers = files('rte_rawdev.h', 'rte_rawdev_pmd.h')

deps += ['telemetry']
