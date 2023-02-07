# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2022 Marvell.

sources = files(
        'rte_mldev_pmd.c',
        'rte_mldev.c',
)

headers = files(
        'rte_mldev.h',
)

indirect_headers += files(
        'rte_mldev_core.h',
)

driver_sdk_headers += files(
        'rte_mldev_pmd.h',
)

deps += ['mempool']

if get_option('buildtype').contains('debug')
        cflags += [ '-DRTE_LIBRTE_ML_DEV_DEBUG' ]
else
        cflags += [ '-URTE_LIBRTE_ML_DEV_DEBUG' ]
endif
