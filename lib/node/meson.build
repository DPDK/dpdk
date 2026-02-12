# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2020 Marvell International Ltd.

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files(
        'ethdev_ctrl.c',
        'ethdev_rx.c',
        'ethdev_tx.c',
        'interface_tx_feature.c',
        'ip4_local.c',
        'ip4_lookup.c',
        'ip4_lookup_fib.c',
        'ip4_reassembly.c',
        'ip4_rewrite.c',
        'ip6_lookup.c',
        'ip6_lookup_fib.c',
        'ip6_rewrite.c',
        'kernel_rx.c',
        'kernel_tx.c',
        'log.c',
        'node_mbuf_dynfield.c',
        'null.c',
        'pkt_cls.c',
        'pkt_drop.c',
        'udp4_input.c',
)
headers = files(
        'rte_node_eth_api.h',
        'rte_node_ip4_api.h',
        'rte_node_ip6_api.h',
        'rte_node_mbuf_dynfield.h',
        'rte_node_pkt_cls_api.h',
        'rte_node_udp4_input_api.h',
)

# Strict-aliasing rules are violated by uint8_t[] to context size casts.
cflags += '-fno-strict-aliasing'
deps += ['graph', 'mbuf', 'lpm', 'ethdev', 'mempool', 'cryptodev', 'ip_frag', 'fib']
