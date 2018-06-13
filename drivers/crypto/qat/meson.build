# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Intel Corporation

dep = dependency('libcrypto', required: false)
if not dep.found()
	build = false
endif
sources = files('qat_crypto.c', 'qat_qp.c',
		'qat_sym_session.c',
		'rte_qat_cryptodev.c',
		'qat_device.c')
includes += include_directories('qat_adf')
deps += ['bus_pci']
ext_deps += dep
pkgconfig_extra_libs += '-lcrypto'
