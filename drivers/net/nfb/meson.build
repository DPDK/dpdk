# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Cesnet
# Copyright(c) 2019 Netcope Technologies, a.s. <info@netcope.com>
# All rights reserved.

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

dep = dependency('netcope-common', required: false, method: 'pkg-config')
reason = 'missing dependency, "libnfb"'
build = dep.found()
ext_deps += dep

sources = files(
        'nfb_ethdev.c',
        'nfb_rx.c',
        'nfb_rxmode.c',
        'nfb_stats.c',
        'nfb_tx.c',
)
