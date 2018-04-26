# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

version = 2

cflags += ['-DRTE_LIBRTE_IXGBE_BYPASS']

subdir('base')
objs = [base_objs]

allow_experimental_apis = true
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
	dpdk_conf.set('RTE_IXGBE_INC_VECTOR', 1)
	sources += files('ixgbe_rxtx_vec_sse.c')
endif

includes += include_directories('base')

install_headers('rte_pmd_ixgbe.h')
