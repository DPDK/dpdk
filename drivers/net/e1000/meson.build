# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

allow_experimental_apis = true

subdir('base')
objs = [base_objs]

sources = files(
	'e1000_logs.c',
	'em_ethdev.c',
	'em_rxtx.c',
	'igb_ethdev.c',
	'igb_flow.c',
	'igb_pf.c',
	'igb_rxtx.c'
)

includes += include_directories('base')
