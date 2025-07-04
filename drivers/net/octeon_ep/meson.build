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
    sources_avx2 += files('cnxk_ep_rx_avx.c')
endif

if arch_subdir == 'arm'
    sources += files('cnxk_ep_rx_neon.c')
endif

extra_flags = ['-Wno-strict-aliasing', '-flax-vector-conversions']
foreach flag: extra_flags
    if cc.has_argument(flag)
        cflags += flag
    endif
endforeach
