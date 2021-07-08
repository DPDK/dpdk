# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.

sources = [
        'ngbe_hw.c',
]

error_cflags = []

c_args = cflags

base_lib = static_library('ngbe_base', sources,
    dependencies: [static_rte_eal, static_rte_ethdev, static_rte_bus_pci],
    c_args: c_args)
base_objs = base_lib.extract_all_objects()
