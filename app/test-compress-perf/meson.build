# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files(
        'comp_perf_options_parse.c',
        'comp_perf_test_common.c',
        'comp_perf_test_cyclecount.c',
        'comp_perf_test_throughput.c',
        'comp_perf_test_verify.c',
        'main.c',
)
deps = ['compressdev']
