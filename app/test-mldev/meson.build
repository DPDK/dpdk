# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2022 Marvell.

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files(
        'ml_main.c',
        'ml_options.c',
        'ml_test.c',
        'parser.c',
        'test_common.c',
        'test_device_ops.c',
        'test_model_common.c',
        'test_model_ops.c',
        'test_inference_common.c',
        'test_inference_ordered.c',
        'test_inference_interleave.c',
)

deps += ['mldev', 'hash']
