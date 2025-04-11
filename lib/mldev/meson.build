# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2022 Marvell.

sources = files(
        'rte_mldev_pmd.c',
        'rte_mldev.c',
        'mldev_utils.c',
)

if (dpdk_conf.has('RTE_ARCH_ARM64') and
    cc.get_define('__ARM_NEON', args: machine_args) != '')
    sources += files('mldev_utils_neon.c')
else
    sources += files('mldev_utils_scalar.c')
endif

if (dpdk_conf.has('RTE_ARCH_ARM64') and
    cc.get_define('__ARM_NEON', args: machine_args) != '' and
    cc.get_define('__ARM_FEATURE_BF16', args: machine_args) != '')
    sources += files('mldev_utils_neon_bfloat16.c')
else
    sources += files('mldev_utils_scalar_bfloat16.c')
endif

headers = files(
        'rte_mldev.h',
)

indirect_headers += files(
        'rte_mldev_core.h',
)

driver_sdk_headers += files(
        'rte_mldev_pmd.h',
        'mldev_utils.h',
)

deps += ['mempool', 'mbuf']

if get_option('buildtype').contains('debug')
        cflags += [ '-DRTE_LIBRTE_ML_DEV_DEBUG' ]
else
        cflags += [ '-URTE_LIBRTE_ML_DEV_DEBUG' ]
endif
