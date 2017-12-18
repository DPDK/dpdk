# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if host_machine.system() != 'linux'
	build = false
endif
version = 2
sources = files('rte_kni.c')
headers = files('rte_kni.h')
deps += ['ethdev', 'pci']
