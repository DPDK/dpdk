# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2022 Marvell.

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
    subdir_done()
endif

driver_sdk_headers = files(
        'cn10k_ml_dev.h',
        'cn10k_ml_ops.h',
        'cn10k_ml_model.h',
        'cn10k_ml_ocm.h',
)

sources = files(
        'cn10k_ml_dev.c',
        'cn10k_ml_ops.c',
        'cn10k_ml_model.c',
        'cn10k_ml_ocm.c',
)

deps += ['mldev', 'common_cnxk', 'kvargs', 'hash']

require_iova_in_mbuf = false

if get_option('buildtype').contains('debug')
        cflags += [ '-DCNXK_ML_DEV_DEBUG' ]
else
        cflags += [ '-UCNXK_ML_DEV_DEBUG' ]
endif
