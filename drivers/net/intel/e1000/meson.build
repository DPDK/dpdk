# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

base_sources = files(
        'base/e1000_base.c',
        'base/e1000_80003es2lan.c',
        'base/e1000_82540.c',
        'base/e1000_82541.c',
        'base/e1000_82542.c',
        'base/e1000_82543.c',
        'base/e1000_82571.c',
        'base/e1000_82575.c',
        'base/e1000_api.c',
        'base/e1000_i210.c',
        'base/e1000_i225.c',
        'base/e1000_ich8lan.c',
        'base/e1000_mac.c',
        'base/e1000_manage.c',
        'base/e1000_mbx.c',
        'base/e1000_nvm.c',
        'base/e1000_phy.c',
        'base/e1000_vf.c',
)

sources += files(
        'e1000_logs.c',
        'e1000_osdep.c',
        'em_ethdev.c',
        'em_rxtx.c',
        'igb_ethdev.c',
        'igb_flow.c',
        'igb_pf.c',
        'igb_rxtx.c',
)

# do not build IGC on Windows
if not is_windows
        sources += files(
                'igc_ethdev.c',
                'igc_logs.c',
                'igc_filter.c',
                'igc_flow.c',
                'igc_txrx.c',
        )
endif
