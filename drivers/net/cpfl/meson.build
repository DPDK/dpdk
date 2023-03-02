# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

deps += ['common_idpf']

sources = files(
        'cpfl_ethdev.c',
        'cpfl_rxtx.c',
)
