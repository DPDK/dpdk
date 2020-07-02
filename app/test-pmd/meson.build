# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# override default name to drop the hyphen
name = 'testpmd'
cflags += '-Wno-deprecated-declarations'
sources = files('5tswap.c',
	'cmdline.c',
	'cmdline_flow.c',
	'cmdline_mtr.c',
	'cmdline_tm.c',
	'config.c',
	'csumonly.c',
	'flowgen.c',
	'icmpecho.c',
	'ieee1588fwd.c',
	'iofwd.c',
	'macfwd.c',
	'macswap.c',
	'noisy_vnf.c',
	'parameters.c',
	'rxonly.c',
	'testpmd.c',
	'txonly.c',
	'util.c')

deps += ['ethdev', 'gro', 'gso', 'cmdline', 'metrics', 'meter', 'bus_pci']
if dpdk_conf.has('RTE_LIBRTE_PDUMP')
	deps += 'pdump'
endif
if dpdk_conf.has('RTE_LIBRTE_BNXT_PMD')
	deps += 'pmd_bnxt'
endif
if dpdk_conf.has('RTE_LIBRTE_I40E_PMD')
	deps += 'pmd_i40e'
endif
if dpdk_conf.has('RTE_LIBRTE_IXGBE_PMD')
	deps += 'pmd_ixgbe'
endif
if dpdk_conf.has('RTE_LIBRTE_DPAA_PMD')
	deps += ['bus_dpaa', 'mempool_dpaa', 'pmd_dpaa']
endif
if dpdk_conf.has('RTE_LIBRTE_BPF')
	sources += files('bpf_cmd.c')
	deps += 'bpf'
endif
