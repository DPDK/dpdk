# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

allow_experimental_apis = true
if not is_linux
	build = false
endif
deps += 'rawdev'
cflags += ['-D_FILE_OFFSET_BITS=64']
sources = files(
	'ntb_fwd.c'
)
if dpdk_conf.has('RTE_LIBRTE_PMD_NTB_RAWDEV')
	deps += 'rawdev_ntb'
endif
