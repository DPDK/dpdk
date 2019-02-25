# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

build = dpdk_conf.has('RTE_LIBRTE_VHOST')
allow_experimental_apis = true
deps += ['vhost', 'cryptodev']
cflags += ['-D_FILE_OFFSET_BITS=64']
sources = files(
	'main.c'
)
