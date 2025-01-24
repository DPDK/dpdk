# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

subdir('base')
objs = [base_objs]

sources = files(
        'fm10k_ethdev.c',
        'fm10k_rxtx.c',
)
if arch_subdir == 'x86'
    sources += files('fm10k_rxtx_vec.c')
endif

includes += include_directories('base')
