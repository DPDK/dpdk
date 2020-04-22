# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

pqos = cc.find_library('pqos', required: false)
build = pqos.found()
ext_deps += pqos
allow_experimental_apis = true
cflags += '-I/usr/local/include' # assume pqos lib installed in /usr/local
sources = files(
	'cat.c', 'l2fwd-cat.c'
)
