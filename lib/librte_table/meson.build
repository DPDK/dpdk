# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

sources = files('rte_table_acl.c',
		'rte_table_lpm.c',
		'rte_table_lpm_ipv6.c',
		'rte_table_hash_cuckoo.c',
		'rte_table_hash_key8.c',
		'rte_table_hash_key16.c',
		'rte_table_hash_key32.c',
		'rte_table_hash_ext.c',
		'rte_table_hash_lru.c',
		'rte_table_array.c',
		'rte_table_stub.c',
		'rte_swx_table_em.c',)
headers = files('rte_table.h',
		'rte_table_acl.h',
		'rte_table_lpm.h',
		'rte_table_lpm_ipv6.h',
		'rte_table_hash.h',
		'rte_table_hash_cuckoo.h',
		'rte_table_hash_func.h',
		'rte_table_hash_func_arm64.h',
		'rte_lru.h',
		'rte_table_array.h',
		'rte_table_stub.h',
		'rte_swx_table.h',
		'rte_swx_table_em.h',)
deps += ['mbuf', 'port', 'lpm', 'hash', 'acl']

if arch_subdir == 'x86'
	headers += files('rte_lru_x86.h')
elif arch_subdir == 'arm' and dpdk_conf.has('RTE_ARCH_ARM64')
	headers += files('rte_lru_arm64.h')
endif
