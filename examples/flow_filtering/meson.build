# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

allow_experimental_apis = true
deps += ['argparse']

sources = files(
        'main.c',
        'flow_skeleton.c',
        'jump_flow.c',
        'snippets/snippet_match_ipv4.c',
        'snippets/snippet_match_gre.c',
        'snippets/snippet_match_integrity_flags.c',
        'snippets/snippet_match_mpls.c',
        'snippets/snippet_match_nsh.c',
        'snippets/snippet_match_nvgre.c',
        'snippets/snippet_match_packet_type.c',
        'snippets/snippet_match_port_affinity.c',
        'snippets/snippet_match_roce_ib_bth.c',
        'snippets/snippet_match_vxlan_gbp.c',
        'snippets/snippet_match_vxlan_gpe.c',
        'snippets/snippet_modify_ecn.c',
        'snippets/snippet_nat64.c',
        'snippets/snippet_random_match.c',
        'snippets/snippet_re_route_to_kernel.c',
        'snippets/snippet_switch_granularity.c',
)

# The code snippets are not utilized.
cflags += '-Wno-unused-function'
cflags += no_shadow_cflag
