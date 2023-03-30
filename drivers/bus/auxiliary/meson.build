# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2021 NVIDIA Corporation & Affiliates

driver_sdk_headers += files('bus_auxiliary_driver.h')
sources = files(
        'auxiliary_common.c',
        'auxiliary_params.c',
)
if is_linux
    sources += files(
            'linux/auxiliary.c',
    )
endif

require_iova_in_mbuf = false

deps += ['kvargs']
