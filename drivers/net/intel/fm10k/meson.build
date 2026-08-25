# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

base_sources = files(
        'base/fm10k_api.c',
        'base/fm10k_common.c',
        'base/fm10k_mbx.c',
        'base/fm10k_pf.c',
        'base/fm10k_tlv.c',
        'base/fm10k_vf.c',
)

sources += files(
        'fm10k_ethdev.c',
        'fm10k_rxtx.c',
)
if arch_subdir == 'x86'
    sources += files('fm10k_rxtx_vec.c')
endif
