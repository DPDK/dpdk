# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

# this app can be built if-and-only-if KNI library is buildable
build = dpdk_conf.has('RTE_LIB_KNI')
if not build
	subdir_done()
endif

deps += ['kni', 'bus_pci']
sources = files(
	'main.c'
)
allow_experimental_apis = true
