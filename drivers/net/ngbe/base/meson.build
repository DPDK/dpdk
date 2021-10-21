# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.

sources = [
        'ngbe_eeprom.c',
        'ngbe_hw.c',
        'ngbe_mbx.c',
        'ngbe_mng.c',
        'ngbe_phy.c',
        'ngbe_phy_rtl.c',
        'ngbe_phy_mvl.c',
        'ngbe_phy_yt.c',
]

error_cflags = []

c_args = cflags

base_lib = static_library('ngbe_base', sources,
    dependencies: [static_rte_eal, static_rte_ethdev, static_rte_bus_pci],
    c_args: c_args)
base_objs = base_lib.extract_all_objects(recursive: true)
