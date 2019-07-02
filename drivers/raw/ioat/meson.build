# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019 Intel Corporation

build = dpdk_conf.has('RTE_ARCH_X86')
sources = files('ioat_rawdev.c')
deps += ['rawdev', 'bus_pci']

install_headers('rte_ioat_rawdev.h',
		'rte_ioat_spec.h')
