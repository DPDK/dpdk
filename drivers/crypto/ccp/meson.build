# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.

if host_machine.system() != 'linux'
        build = false
endif
dep = dependency('libcrypto', required: false)
if not dep.found()
	build = false
endif
deps += 'bus_vdev'
deps += 'bus_pci'

sources = files('rte_ccp_pmd.c',
		'ccp_crypto.c',
		'ccp_dev.c',
		'ccp_pci.c',
		'ccp_pmd_ops.c')

ext_deps += dep
pkgconfig_extra_libs += '-lcrypto'
