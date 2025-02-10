# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 Intel Corporation

sources += files(
        'idpf_controlq.c',
        'idpf_controlq_setup.c',
)

if is_ms_compiler
    error_cflags = [
            '/wd4101', # unreferenced local variable
    ]
else
    error_cflags = [
            '-Wno-unused-variable',
    ]
endif

foreach flag: error_cflags
    if cc.has_argument(flag)
        cflags += flag
    endif
endforeach
