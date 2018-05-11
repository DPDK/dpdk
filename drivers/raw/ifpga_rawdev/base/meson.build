# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

sources = [
	'ifpga_api.c',
	'ifpga_enumerate.c',
	'ifpga_feature_dev.c',
	'ifpga_fme.c',
	'ifpga_fme_iperf.c',
	'ifpga_fme_dperf.c',
	'ifpga_fme_error.c',
	'ifpga_port.c',
	'ifpga_port_error.c',
	'ifpga_fme_pr.c',
	'opae_hw_api.c',
	'opae_ifpga_hw_api.c',
	'opae_debug.c'
]

error_cflags = ['-Wno-sign-compare', '-Wno-unused-value',
		'-Wno-format', '-Wno-unused-but-set-variable',
		'-Wno-strict-aliasing'
]
c_args = cflags
foreach flag: error_cflags
	if cc.has_argument(flag)
		c_args += flag
	endif
endforeach

base_lib = static_library('ifpga_rawdev_base', sources,
	dependencies: static_rte_eal,
	c_args: c_args)
base_objs = base_lib.extract_all_objects()
