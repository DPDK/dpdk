# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Marvell International Ltd.
# Copyright(c) 2018 Semihalf.
# All rights reserved.

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

dep = dependency('libmusdk', required: false, method: 'pkg-config')
if not dep.found()
    build = false
    reason = 'missing dependency, "libmusdk"'
    subdir_done()
endif

ext_deps += dep

sources = files(
        'mrvl_ethdev.c',
        'mrvl_flow.c',
        'mrvl_qos.c',
        'mrvl_mtr.c',
        'mrvl_tm.c',
)

deps += ['cfgfile', 'common_mvep']
