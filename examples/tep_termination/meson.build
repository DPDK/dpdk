# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

if not is_linux
	build = false
endif
deps += ['hash', 'vhost']
cflags += '-Wno-deprecated-declarations'
sources = files(
	'main.c', 'vxlan.c', 'vxlan_setup.c'
)
