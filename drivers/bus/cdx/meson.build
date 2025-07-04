# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2022-2023, Advanced Micro Devices, Inc.

if not is_linux
    build = false
    reason = 'only supported on Linux'
endif

driver_sdk_headers = files('bus_cdx_driver.h')
sources = files(
        'cdx.c',
        'cdx_vfio.c',
)
