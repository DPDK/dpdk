# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
	build = false
	reason = 'only supported on 64-bit linux'
endif
sources = files('rte_kni.c')
headers = files('rte_kni.h')
deps += ['ethdev', 'pci']
