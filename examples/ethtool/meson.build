# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

build = is_linux
if not build
    subdir_done()
endif

sources = files(
        'lib/rte_ethtool.c',
        'ethtool-app/ethapp.c',
        'ethtool-app/main.c',
)
includes = include_directories('lib', 'ethtool-app')

deps += 'bus_pci'
if dpdk_conf.has('RTE_NET_IXGBE')
    deps += 'net_ixgbe'
endif

allow_experimental_apis = true
