# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 Marvell.

# override default name to drop the hyphen
name = 'graph'
build = cc.has_header('sys/epoll.h')
if not build
    reason = 'only supported on Linux'
    subdir_done()
endif

deps += ['graph', 'eal', 'lpm', 'ethdev', 'node', 'cmdline', 'net']
sources = files(
        'cli.c',
        'conn.c',
        'ethdev_rx.c',
        'ethdev.c',
        'feature.c',
        'graph.c',
        'ip4_route.c',
        'ip4_output_hook.c',
        'ip6_route.c',
        'l2fwd.c',
        'l3fwd.c',
        'main.c',
        'mempool.c',
        'neigh.c',
        'utils.c',
)

cmd_h = custom_target('commands_hdr',
        output: 'commands.h',
        input: files('commands.list'),
        capture: true,
        command: [cmdline_gen_cmd, '--context-name=modules_ctx', '@INPUT@']
)
sources += cmd_h
