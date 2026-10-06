# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

sources = files('rte_metrics.c')
headers = files('rte_metrics.h')

# Legacy JSON telemetry commands are only available with jansson.
if dpdk_conf.has('RTE_HAS_JANSSON')
    sources += files('rte_metrics_telemetry.c')
    ext_deps += jansson_dep
endif

deps += ['ethdev', 'telemetry']

cflags += no_wvla_cflag
