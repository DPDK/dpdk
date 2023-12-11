# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2021 Marvell.
#

sources = files(
        'otx_ep_ethdev.c',
        'otx_ep_rxtx.c',
        'otx_ep_vf.c',
        'otx2_ep_vf.c',
        'cnxk_ep_vf.c',
        'otx_ep_mbox.c',
        'cnxk_ep_rx.c',
        'cnxk_ep_tx.c',
)

if arch_subdir == 'x86'
    sources += files('cnxk_ep_rx_sse.c')
endif

extra_flags = ['-Wno-strict-aliasing']
foreach flag: extra_flags
    if cc.has_argument(flag)
        cflags += flag
    endif
endforeach
