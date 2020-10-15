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
if dpdk_conf.has('RTE_LIB_BITRATESTATS')
	deps += 'bitratestats'
endif
if dpdk_conf.has('RTE_LIB_PDUMP')
	deps += 'pdump'
endif
if dpdk_conf.has('RTE_LIB_BITRATESTATS')
	deps += 'bitratestats'
endif
if dpdk_conf.has('RTE_LIB_LATENCYSTATS')
	deps += 'latencystats'
endif
if dpdk_conf.has('RTE_CRYPTO_SCHEDULER')
	deps += 'crypto_scheduler'
endif
if dpdk_conf.has('RTE_NET_BOND')
	deps += 'net_bond'
endif
if dpdk_conf.has('RTE_NET_BNXT')
	deps += 'net_bnxt'
endif
if dpdk_conf.has('RTE_NET_I40E')
	deps += 'net_i40e'
endif
if dpdk_conf.has('RTE_NET_IXGBE')
	deps += 'net_ixgbe'
endif
if dpdk_conf.has('RTE_NET_DPAA')
	deps += ['bus_dpaa', 'mempool_dpaa', 'net_dpaa']
endif
if dpdk_conf.has('RTE_LIB_BPF')
	sources += files('bpf_cmd.c')
	deps += 'bpf'
endif
