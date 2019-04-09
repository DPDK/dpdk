# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

if not is_linux
        build = false
endif

deps += ['bus_dpaa']
sources = files('dpaa_mempool.c')

# depends on dpaa bus which uses experimental API
allow_experimental_apis = true
