# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Intel Corporation

# This does not build a driver, but instead holds common files for
# the crypto and compression drivers.
build = false
qat_deps = ['bus_pci']
qat_sources = files('qat_common.c',
		'qat_qp.c',
		'qat_device.c',
		'qat_logs.c')
qat_includes = [include_directories('.', 'qat_adf')]
qat_ext_deps = []
qat_cflags = []
