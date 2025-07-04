# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

subdir('base')

sources = files(
        'ngbe_ethdev.c',
        'ngbe_ptypes.c',
        'ngbe_pf.c',
        'ngbe_rxtx.c',
        'ngbe_ethdev_vf.c',
)

deps += ['hash']

if arch_subdir == 'x86'
    sources += files('ngbe_rxtx_vec_sse.c')
elif arch_subdir == 'arm'
    sources += files('ngbe_rxtx_vec_neon.c')
endif
