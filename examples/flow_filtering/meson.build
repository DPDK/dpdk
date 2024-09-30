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
        'snippets/snippet_match_ipv4.c',
        'snippets/snippet_match_gre.c',
        'snippets/snippet_match_mpls.c',
)

# The code snippets are not utilized.
cflags += '-Wno-unused-function'
