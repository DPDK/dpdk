# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

if not is_linux
	build = false
endif
allow_experimental_apis = true
deps += ['power', 'timer', 'lpm', 'hash']
sources = files(
	'main.c', 'perf_core.c'
)
