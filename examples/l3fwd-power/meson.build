# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

if host_machine.system() != 'linux'
	build = false
endif
deps += ['power', 'timer', 'lpm', 'hash']
sources = files(
	'main.c'
)
