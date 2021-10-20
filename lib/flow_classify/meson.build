# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files('rte_flow_classify.c', 'rte_flow_classify_parse.c')
headers = files('rte_flow_classify.h')
deps += ['net', 'table']
