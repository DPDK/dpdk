# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Intel Corporation

# this does not build the QAT driver, instead that is done in the compression
# driver which comes later. Here we just add our sources files to the list
build = false
dep = dependency('libcrypto', required: false)
qat_includes += include_directories('.')
qat_deps += 'cryptodev'
if dep.found()
	# Add our sources files to the list
	qat_sources += files('qat_sym_pmd.c',
			     'qat_sym.c',
			     'qat_sym_session.c')
	qat_ext_deps += dep
	pkgconfig_extra_libs += '-lcrypto'
	qat_cflags += '-DBUILD_QAT_SYM'
endif
