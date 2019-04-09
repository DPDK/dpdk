# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

# require the power library
build = dpdk_conf.has('RTE_LIBRTE_POWER')

deps += ['distributor', 'power']
sources = files(
	'main.c'
)
