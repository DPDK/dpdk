# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

sources = files('rte_telemetry.c', 'rte_telemetry_parser.c', 'rte_telemetry_parser_test.c')
headers = files('rte_telemetry.h', 'rte_telemetry_internal.h', 'rte_telemetry_parser.h', 'rte_telemetry_parser_test.h')
deps += ['metrics', 'ethdev']
cflags += '-DALLOW_EXPERIMENTAL_API'

jansson = dependency('jansson', required: false)
if jansson.found()
	ext_deps += jansson
	dpdk_app_link_libraries += ['telemetry']
else
	build = false
endif
