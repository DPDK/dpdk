# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.

sources = [
        'txgbe_dcb_hw.c',
        'txgbe_dcb.c',
        'txgbe_eeprom.c',
        'txgbe_hw.c',
        'txgbe_mbx.c',
        'txgbe_mng.c',
        'txgbe_phy.c',
        'txgbe_vf.c',
]

error_cflags = []

c_args = cflags
foreach flag: error_cflags
    if cc.has_argument(flag)
        c_args += flag
    endif
endforeach

base_lib = static_library('txgbe_base', sources,
    dependencies: [static_rte_eal, static_rte_net],
    c_args: c_args)
base_objs = base_lib.extract_all_objects(recursive: true)
