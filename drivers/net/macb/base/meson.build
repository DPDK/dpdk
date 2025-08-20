# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 Phytium Technology Co., Ltd.

sources = [
        'macb_common.c',
        'macb_uio.c',
        'generic_phy.c',
]

error_cflags = ['-Wno-unused-value',
        '-Wno-unused-but-set-variable',
        '-Wno-unused-variable',
        '-Wno-unused-parameter',
        ]
c_args = cflags
foreach flag: error_cflags
    if cc.has_argument(flag)
        c_args += flag
    endif
endforeach

base_lib = static_library('macb_base', sources,
    dependencies: static_rte_eal,
    c_args: c_args)
base_objs = base_lib.extract_all_objects()
