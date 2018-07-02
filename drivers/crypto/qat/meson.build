# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Intel Corporation

dep = dependency('libcrypto', required: false)

build = false
sources = files('qat_common.c',
		'qat_qp.c',
		'qat_device.c',
		'qat_logs.c')

if dep.found()
	sources += files('qat_sym_pmd.c', 'qat_sym.c', 'qat_sym_session.c')
	pkgconfig_extra_libs += '-lcrypto'
	build = true
endif

includes += include_directories('qat_adf')
deps += ['bus_pci']
ext_deps += dep
