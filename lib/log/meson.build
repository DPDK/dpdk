# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 Intel Corporation

includes += global_inc
sources = files(
        'log.c',
        'log_color.c',
        'log_timestamp.c',
)

if not is_windows
    sources += files('log_syslog.c')
endif

if is_linux
    sources += files('log_journal.c')
endif

headers = files('rte_log.h')
