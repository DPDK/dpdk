# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Intel Corporation


# Add our sources files to the list
qat_sources += files('qat_comp_pmd.c',
		     'qat_comp.c')
qat_includes += include_directories('.')
qat_deps += 'compressdev'
qat_ext_deps += dep

# build the whole driver
sources += qat_sources
cflags += qat_cflags
deps += qat_deps
ext_deps += qat_ext_deps
includes += qat_includes
