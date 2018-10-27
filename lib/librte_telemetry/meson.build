# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

sources = files('rte_telemetry.c')
headers = files('rte_telemetry.h', 'rte_telemetry_internal.h')
deps += ['metrics', 'ethdev']
cflags += '-DALLOW_EXPERIMENTAL_API'

jansson = cc.find_library('jansson', required: false)
if jansson.found()
	ext_deps += jansson
else
	build = false
endif
