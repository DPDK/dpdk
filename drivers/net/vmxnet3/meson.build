# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Luca Boccassi <bluca@debian.org>

sources += files(
        'vmxnet3_ethdev.c',
        'vmxnet3_rxtx.c',
)

error_cflags = [
        '-Wno-unused-parameter',
        '-Wno-unused-value',
        '-Wno-strict-aliasing',
        '-Wno-format-extra-args',
]
foreach flag: error_cflags
        if cc.has_argument(flag)
                cflags += flag
        endif
endforeach
