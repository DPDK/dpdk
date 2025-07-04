# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019 Mellanox Technologies, Ltd

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

allow_experimental_apis = true

cmd_h = custom_target('commands_hdr',
        output: 'commands.h',
        input: files('commands.list'),
        capture: true,
        command: [cmdline_gen_cmd, '--context-name=simple_mp_ctx', '@INPUT@']
)

sources = files(
        'mp_commands.c',
        'main.c',
)
sources += cmd_h
