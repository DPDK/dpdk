# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

cflags += '-std=gnu99'
cflags += '-D_DEFAULT_SOURCE'
cflags += '-D_XOPEN_SOURCE=700'
cflags += '-pedantic'
if is_linux
    cflags += '-DLINUX'
else
    cflags += '-DBSD'
endif

sources = files(
        'failsafe.c',
        'failsafe_args.c',
        'failsafe_eal.c',
        'failsafe_ether.c',
        'failsafe_flow.c',
        'failsafe_intr.c',
        'failsafe_ops.c',
        'failsafe_rxtx.c',
)

require_iova_in_mbuf = false

if is_freebsd
    annotate_locks = false
endif
