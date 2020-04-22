# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

allow_experimental_apis = true
deps += ['sched', 'cfgfile']
sources = files(
	'app_thread.c', 'args.c', 'cfg_file.c', 'cmdline.c',
	'init.c', 'main.c', 'stats.c'
)
