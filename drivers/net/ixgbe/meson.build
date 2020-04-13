# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

cflags += ['-DRTE_LIBRTE_IXGBE_BYPASS']

subdir('base')
objs = [base_objs]

sources = files(
	'ixgbe_82599_bypass.c',
	'ixgbe_bypass.c',
	'ixgbe_ethdev.c',
	'ixgbe_fdir.c',
	'ixgbe_flow.c',
	'ixgbe_ipsec.c',
	'ixgbe_pf.c',
	'ixgbe_rxtx.c',
	'ixgbe_tm.c',
	'ixgbe_vf_representor.c',
	'rte_pmd_ixgbe.c'
)

deps += ['hash', 'security']

if arch_subdir == 'x86'
	sources += files('ixgbe_rxtx_vec_sse.c')
elif arch_subdir == 'arm'
	sources += files('ixgbe_rxtx_vec_neon.c')
endif

includes += include_directories('base')

install_headers('rte_pmd_ixgbe.h')
