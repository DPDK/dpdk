# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

sources = [
	'fm10k_api.c',
	'fm10k_common.c',
	'fm10k_mbx.c',
	'fm10k_pf.c',
	'fm10k_tlv.c',
	'fm10k_vf.c'
]

error_cflags = ['-Wno-unused-parameter', '-Wno-unused-value',
	'-Wno-strict-aliasing', '-Wno-format-extra-args',
	'-Wno-unused-variable', '-Wno-missing-field-initializers'
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

base_lib = static_library('fm10k_base', sources,
	dependencies: static_rte_eal,
	c_args: c_args)
base_objs = base_lib.extract_all_objects()
