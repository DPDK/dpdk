# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2021 Marvell International Ltd.

error_cflags = ['-Wno-uninitialized']
foreach flag: error_cflags
    if cc.has_argument(flag)
        cflags += flag
    endif
endforeach

driver_sdk_headers = files(
        'cnxk_dma_event_dp.h',
)

deps += ['bus_pci', 'common_cnxk', 'dmadev', 'eventdev']

includes += include_directories('../../event/cnxk')

sources = files('cnxk_dmadev.c', 'cnxk_dmadev_fp.c')
require_iova_in_mbuf = false
