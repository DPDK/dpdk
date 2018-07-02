# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Intel Corporation

build = false
dep = dependency('libcrypto', required: false)
if dep.found()
	# Add our sources files to the list
	qat_sources += files('qat_sym_pmd.c',
			     'qat_sym.c',
			     'qat_sym_session.c')
	qat_includes += include_directories('.')
	qat_deps += 'cryptodev'
	qat_ext_deps += dep
	pkgconfig_extra_libs += '-lcrypto'
	qat_cflags += '-DBUILD_QAT_SYM'

	# build the whole driver
	sources += qat_sources
	cflags += qat_cflags
	deps += qat_deps
	ext_deps += qat_ext_deps
	includes += qat_includes
	build = true
endif
