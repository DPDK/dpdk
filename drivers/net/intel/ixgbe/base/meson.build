# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2024 Intel Corporation

sources = [
        'ixgbe_82598.c',
        'ixgbe_82599.c',
        'ixgbe_api.c',
        'ixgbe_common.c',
        'ixgbe_dcb_82598.c',
        'ixgbe_dcb_82599.c',
        'ixgbe_dcb.c',
        'ixgbe_e610.c',
        'ixgbe_hv_vf.c',
        'ixgbe_mbx.c',
        'ixgbe_phy.c',
        'ixgbe_vf.c',
        'ixgbe_x540.c',
        'ixgbe_x550.c',
]

base_lib = static_library('ixgbe_base', sources,
    dependencies: [static_rte_eal, static_rte_net],
    c_args: c_args)
base_objs = base_lib.extract_all_objects(recursive: true)
