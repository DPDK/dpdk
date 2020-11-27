# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019-2020 Intel Corporation

sources = [
	'igc_api.c',
	'igc_base.c',
	'igc_i225.c',
	'igc_mac.c',
	'igc_manage.c',
	'igc_nvm.c',
	'igc_osdep.c',
	'igc_phy.c',
]

base_lib = static_library('igc_base', sources,
	dependencies: static_rte_eal,
	c_args: cflags)

base_objs = base_lib.extract_all_objects()
