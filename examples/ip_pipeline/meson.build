# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

deps += ['pipeline', 'bus_pci']
allow_experimental_apis = true
sources = files(
	'action.c',
	'cli.c',
	'conn.c',
	'kni.c',
	'link.c',
	'main.c',
	'mempool.c',
	'parser.c',
	'pipeline.c',
	'swq.c',
	'tap.c',
	'thread.c',
	'tmgr.c'
)
