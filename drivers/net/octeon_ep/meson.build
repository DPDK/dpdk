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
    if cc.get_define('__AVX2__', args: machine_args) != ''
        cflags += ['-DCC_AVX2_SUPPORT']
        sources += files('cnxk_ep_rx_avx.c')
    elif cc.has_argument('-mavx2')
        cflags += ['-DCC_AVX2_SUPPORT']
        otx_ep_avx2_lib = static_library('otx_ep_avx2_lib',
                        'cnxk_ep_rx_avx.c',
                        dependencies: [static_rte_ethdev, static_rte_pci, static_rte_bus_pci],
                        include_directories: includes,
                        c_args: [cflags, '-mavx2'])
        objs += otx_ep_avx2_lib.extract_objects('cnxk_ep_rx_avx.c')
    endif
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
