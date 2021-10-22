# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020 Mellanox Technologies, Ltd

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files(
        'actions_gen.c',
        'flow_gen.c',
        'items_gen.c',
        'main.c',
)

deps += ['ethdev']
