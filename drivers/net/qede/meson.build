# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Luca Boccassi <bluca@debian.org>

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

subdir('base')
objs = [base_objs]

sources = files(
        'qede_debug.c',
        'qede_ethdev.c',
        'qede_filter.c',
        'qede_main.c',
        'qede_regs.c',
        'qede_rxtx.c',
        'qede_sriov.c',
)

if cc.has_argument('-Wno-format-nonliteral')
    cflags += '-Wno-format-nonliteral'
endif
