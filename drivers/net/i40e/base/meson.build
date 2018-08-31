# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

sources = [
	'i40e_adminq.c',
	'i40e_common.c',
	'i40e_dcb.c',
	'i40e_diag.c',
	'i40e_hmc.c',
	'i40e_lan_hmc.c',
	'i40e_nvm.c'
]

error_cflags = ['-Wno-sign-compare', '-Wno-unused-value',
		'-Wno-format', '-Wno-error=format-security',
		'-Wno-strict-aliasing', '-Wno-unused-but-set-variable'
]
c_args = cflags
if allow_experimental_apis
	c_args += '-DALLOW_EXPERIMENTAL_API'
endif
foreach flag: error_cflags
	if cc.has_argument(flag)
		c_args += flag
	endif
endforeach

base_lib = static_library('i40e_base', sources,
	dependencies: static_rte_eal,
	c_args: c_args)
base_objs = base_lib.extract_all_objects()
