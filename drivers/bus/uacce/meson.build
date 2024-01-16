# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 HiSilicon Limited.

if not is_linux
    build = false
    reason = 'only supported on Linux'
endif

sources = files('uacce.c')
driver_sdk_headers += files('bus_uacce_driver.h')

deps += ['kvargs']
