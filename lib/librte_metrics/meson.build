# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

sources = files('rte_metrics.c')
headers = files('rte_metrics.h')

jansson = dependency('jansson', required: false, method: 'pkg-config')
if jansson.found()
	ext_deps += jansson
	sources += files('rte_metrics_telemetry.c')
	headers += files('rte_metrics_telemetry.h')
	deps += ['ethdev', 'telemetry']
	includes += include_directories('../librte_telemetry')
endif
