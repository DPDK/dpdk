# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 Intel Corporation

sources += files(
        'idpf_controlq.c',
        'idpf_controlq_setup.c',
)

error_cflags = [
        '-Wno-unused-variable',
]
foreach flag: error_cflags
    if cc.has_argument(flag)
        cflags += flag
    endif
endforeach
