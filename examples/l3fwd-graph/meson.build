# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2020 Marvell International Ltd.

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

deps += ['graph', 'eal', 'lpm', 'ethdev', 'node' ]
sources = files(
        'main.c',
)
allow_experimental_apis = true
