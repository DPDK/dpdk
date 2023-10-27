# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

if not is_linux
    build = false
    subdir_done()
endif

deps += 'vhost'
allow_experimental_apis = true
sources = files(
        'main.c',
)
cmd_h = custom_target('commands_hdr',
        output: 'commands.h',
        input: files('commands.list'),
        capture: true,
        command: [cmdline_gen_cmd, '--context-name=main_ctx', '@INPUT@']
)
sources += cmd_h
