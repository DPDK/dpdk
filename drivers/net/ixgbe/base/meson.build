# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2020 Intel Corporation

sources = [
	'ixgbe_82598.c',
	'ixgbe_82599.c',
	'ixgbe_api.c',
	'ixgbe_common.c',
	'ixgbe_dcb_82598.c',
	'ixgbe_dcb_82599.c',
	'ixgbe_dcb.c',
	'ixgbe_hv_vf.c',
	'ixgbe_mbx.c',
	'ixgbe_phy.c',
	'ixgbe_vf.c',
	'ixgbe_x540.c',
	'ixgbe_x550.c'
]

error_cflags = ['-Wno-unused-value',
		'-Wno-unused-but-set-variable',
		'-Wno-unused-parameter',
		]
c_args = cflags
foreach flag: error_cflags
	if cc.has_argument(flag)
		c_args += flag
	endif
endforeach

base_lib = static_library('ixgbe_base', sources,
	dependencies: static_rte_eal,
	c_args: c_args)
base_objs = base_lib.extract_all_objects()
