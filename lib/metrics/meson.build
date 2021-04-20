# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

sources = files('rte_metrics.c', 'rte_metrics_telemetry.c')
headers = files('rte_metrics.h', 'rte_metrics_telemetry.h')

jansson = dependency('jansson', required: false, method: 'pkg-config')
if jansson.found()
    dpdk_conf.set('RTE_HAS_JANSSON', 1)
    ext_deps += jansson
endif

deps += ['ethdev', 'telemetry']
