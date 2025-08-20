# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 Phytium Technology Co., Ltd.

if not is_linux
    build = false
    reason = 'only supported on Linux'
    subdir_done()
endif

subdir('base')
objs = [base_objs]

sources = files(
        'macb_ethdev.c',
        'macb_rxtx.c',
)

includes += include_directories('base')
