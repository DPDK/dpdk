# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2016-2018 Solarflare Communications Inc.
# All rights reserved.
#
# This software was jointly developed between OKTET Labs (under contract
# for Solarflare) and Solarflare Communications, Inc.

if arch_subdir != 'x86'
	build = false
endif

allow_experimental_apis = true

extra_flags = []

# Strict-aliasing rules are violated by rte_eth_link to uint64_t casts
extra_flags += '-Wno-strict-aliasing'

# Enable more warnings
extra_flags += [
	'-Wextra',
	'-Wdisabled-optimization'
]

# Compiler and version dependent flags
extra_flags += [
	'-Waggregate-return',
	'-Wnested-externs',
	'-Wbad-function-cast'
]

foreach flag: extra_flags
	if cc.has_argument(flag)
		cflags += flag
	endif
endforeach

subdir('base')
objs = [base_objs]

sources = files(
	'sfc_ethdev.c',
	'sfc_kvargs.c',
	'sfc.c',
	'sfc_mcdi.c',
	'sfc_intr.c',
	'sfc_ev.c',
	'sfc_port.c',
	'sfc_rx.c',
	'sfc_tx.c',
	'sfc_tso.c',
	'sfc_filter.c',
	'sfc_flow.c',
	'sfc_dp.c',
	'sfc_ef10_rx.c',
	'sfc_ef10_essb_rx.c',
	'sfc_ef10_tx.c'
)

includes += include_directories('base')
