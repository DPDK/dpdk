# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 Intel Corporation

includes += global_inc
sources = files(
        'log.c',
        'log_timestamp.c',
)

if not is_windows
    sources += files('log_syslog.c')
endif

headers = files('rte_log.h')
