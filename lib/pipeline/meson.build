# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files(
        'rte_pipeline.c',
        'rte_port_in_action.c',
        'rte_table_action.c',
        'rte_swx_pipeline.c',
        'rte_swx_pipeline_spec.c',
        'rte_swx_ctl.c',
)
headers = files(
        'rte_pipeline.h',
        'rte_port_in_action.h',
        'rte_table_action.h',
        'rte_swx_pipeline.h',
        'rte_swx_extern.h',
        'rte_swx_ctl.h',
)
deps += ['port', 'table', 'meter', 'sched', 'cryptodev']
