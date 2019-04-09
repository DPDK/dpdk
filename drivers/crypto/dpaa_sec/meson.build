# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

if not is_linux
        build = false
endif

deps += ['bus_dpaa', 'security']
sources = files('dpaa_sec.c')

allow_experimental_apis = true

includes += include_directories('../dpaa2_sec/')
