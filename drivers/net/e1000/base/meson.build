# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

sources = [
	'e1000_base.c',
	'e1000_80003es2lan.c',
	'e1000_82540.c',
	'e1000_82541.c',
	'e1000_82542.c',
	'e1000_82543.c',
	'e1000_82571.c',
	'e1000_82575.c',
	'e1000_api.c',
	'e1000_i210.c',
	'e1000_ich8lan.c',
	'e1000_mac.c',
	'e1000_manage.c',
	'e1000_mbx.c',
	'e1000_nvm.c',
	'e1000_osdep.c',
	'e1000_phy.c',
	'e1000_vf.c'
]

error_cflags = ['-Wno-uninitialized', '-Wno-unused-parameter',
	'-Wno-unused-variable', '-Wno-misleading-indentation',
	'-Wno-implicit-fallthrough']
c_args = cflags
foreach flag: error_cflags
	if cc.has_argument(flag)
		c_args += flag
	endif
endforeach

base_lib = static_library('e1000_base', sources,
	dependencies: static_rte_eal,
	c_args: c_args)
base_objs = base_lib.extract_all_objects()
