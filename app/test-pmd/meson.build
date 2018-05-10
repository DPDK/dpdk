# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# override default name to drop the hyphen
name = 'testpmd'
allow_experimental_apis = true
sources = files('cmdline.c',
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
	'parameters.c',
	'rxonly.c',
	'testpmd.c',
	'txonly.c')

deps = ['ethdev', 'gro', 'gso', 'cmdline', 'metrics', 'meter', 'bus_pci']
if dpdk_conf.has('RTE_LIBRTE_PDUMP')
	deps += 'pdump'
endif
if dpdk_conf.has('RTE_LIBRTE_I40E_PMD')
	deps += 'pmd_i40e'
endif
if dpdk_conf.has('RTE_LIBRTE_IXGBE_PMD')
	deps += 'pmd_ixgbe'
endif
if dpdk_conf.has('RTE_LIBRTE_SOFTNIC_PMD')
	sources += files('tm.c')
	deps += 'pmd_softnic'
endif
if dpdk_conf.has('RTE_LIBRTE_DPAA_PMD')
	deps += ['bus_dpaa', 'mempool_dpaa', 'pmd_dpaa']
endif
if dpdk_conf.has('RTE_LIBRTE_BPF')
	sources += files('bpf_cmd.c')
	deps += 'bpf'
endif
