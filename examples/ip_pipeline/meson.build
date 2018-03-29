# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

deps += ['cfgfile', 'bus_pci']
sources = files(
	'config_check.c',
	'config_parse.c',
	'config_parse_tm.c',
	'cpu_core_map.c',
	'init.c',
	'main.c',
	'parser.c',
	'thread.c',
)
