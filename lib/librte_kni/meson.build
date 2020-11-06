# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
	build = false
	reason = 'only supported on 64-bit Linux'
endif
sources = files('rte_kni.c')
headers = files('rte_kni.h', 'rte_kni_common.h')
deps += ['ethdev', 'pci']
