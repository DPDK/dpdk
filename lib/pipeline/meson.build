# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

cflags += no_wvla_cflag

sources = files(
        'rte_swx_ipsec.c',
        'rte_swx_pipeline.c',
        'rte_swx_pipeline_spec.c',
        'rte_swx_ctl.c',
)
headers = files(
        'rte_swx_ipsec.h',
        'rte_swx_pipeline.h',
        'rte_swx_extern.h',
        'rte_swx_ctl.h',
)
deps += ['port', 'table', 'meter', 'cryptodev', 'ipsec']
