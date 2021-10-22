# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Cavium, Inc

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files(
        'evt_main.c',
        'evt_options.c',
        'evt_test.c',
        'parser.c',
        'test_order_atq.c',
        'test_order_common.c',
        'test_order_queue.c',
        'test_perf_atq.c',
        'test_perf_common.c',
        'test_perf_queue.c',
        'test_pipeline_atq.c',
        'test_pipeline_common.c',
        'test_pipeline_queue.c',
)
deps += 'eventdev'
