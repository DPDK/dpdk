# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 Marvell.

# override default name to drop the hyphen
name = 'graph'
build = cc.has_header('sys/epoll.h')
if not build
    subdir_done()
endif

deps += ['graph', 'eal', 'lpm', 'ethdev', 'node', 'cmdline']
sources = files(
        'cli.c',
        'conn.c',
        'ethdev_rx.c',
        'ethdev.c',
        'graph.c',
        'ip4_route.c',
        'ip6_route.c',
        'l3fwd.c',
        'main.c',
        'mempool.c',
        'neigh.c',
        'utils.c',
)
