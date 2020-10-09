# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2020 Intel Corporation

headers = files('rte_ip.h',
	'rte_tcp.h',
	'rte_udp.h',
	'rte_esp.h',
	'rte_sctp.h',
	'rte_icmp.h',
	'rte_arp.h',
	'rte_ether.h',
	'rte_vxlan.h',
	'rte_gre.h',
	'rte_gtp.h',
	'rte_net.h',
	'rte_net_crc.h',
	'rte_mpls.h',
	'rte_higig.h',
	'rte_ecpri.h')

sources = files('rte_arp.c', 'rte_ether.c', 'rte_net.c', 'rte_net_crc.c')
deps += ['mbuf']

if dpdk_conf.has('RTE_ARCH_X86_64')
	net_crc_sse42_cpu_support = (
		cc.get_define('__PCLMUL__', args: machine_args) != '')
	net_crc_sse42_cc_support = (
		cc.has_argument('-mpclmul') and cc.has_argument('-maes'))

	build_static_net_crc_sse42_lib = 0

	if net_crc_sse42_cpu_support == true
		sources += files('net_crc_sse.c')
		cflags += ['-DCC_X86_64_SSE42_PCLMULQDQ_SUPPORT']
	elif net_crc_sse42_cc_support == true
		build_static_net_crc_sse42_lib = 1
		net_crc_sse42_lib_cflags = ['-mpclmul', '-maes']
		cflags += ['-DCC_X86_64_SSE42_PCLMULQDQ_SUPPORT']
	endif

	if build_static_net_crc_sse42_lib == 1
		net_crc_sse42_lib = static_library(
					'net_crc_sse42_lib',
					'net_crc_sse.c',
					dependencies: static_rte_eal,
					c_args: [cflags,
						net_crc_sse42_lib_cflags])
		objs += net_crc_sse42_lib.extract_objects('net_crc_sse.c')
	endif
elif (dpdk_conf.has('RTE_ARCH_ARM64') and
		cc.get_define('__ARM_FEATURE_CRYPTO', args: machine_args) != '')
	sources += files('net_crc_neon.c')
	cflags += ['-DCC_ARM64_NEON_PMULL_SUPPORT']
endif
