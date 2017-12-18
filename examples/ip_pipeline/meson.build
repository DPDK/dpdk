# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

deps += ['cfgfile', 'pipeline', 'bus_pci']
includes += include_directories('pipeline')
sources = files(
	'config_check.c',
	'config_parse.c',
	'config_parse_tm.c',
	'cpu_core_map.c',
	'init.c',
	'main.c',
	'parser.c',
	'thread.c',
	'thread_fe.c',
	'pipeline/pipeline_common_be.c',
	'pipeline/pipeline_common_fe.c',
	'pipeline/pipeline_firewall_be.c',
	'pipeline/pipeline_firewall.c',
	'pipeline/pipeline_flow_actions_be.c',
	'pipeline/pipeline_flow_actions.c',
	'pipeline/pipeline_flow_classification_be.c',
	'pipeline/pipeline_flow_classification.c',
	'pipeline/pipeline_master_be.c',
	'pipeline/pipeline_master.c',
	'pipeline/pipeline_passthrough_be.c',
	'pipeline/pipeline_passthrough.c',
	'pipeline/pipeline_routing_be.c',
	'pipeline/pipeline_routing.c',
)
