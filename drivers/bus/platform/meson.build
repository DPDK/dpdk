# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2023 Marvell.
#

if not is_linux
    build = false
    reason = 'only supported on Linux'
    subdir_done()
endif

require_iova_in_mbuf = false

deps += ['kvargs']
sources = files(
        'platform_params.c',
        'platform.c',
)
driver_sdk_headers += files('bus_platform_driver.h')
