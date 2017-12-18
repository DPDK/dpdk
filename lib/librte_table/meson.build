# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

version = 2
sources = files('rte_table_lpm.c', 'rte_table_lpm_ipv6.c',
		'rte_table_hash_cuckoo.c', 'rte_table_hash_key8.c',
		'rte_table_hash_key16.c', 'rte_table_hash_key32.c',
		'rte_table_hash_ext.c', 'rte_table_hash_lru.c',
		'rte_table_array.c', 'rte_table_stub.c')
headers = files('rte_table.h', 'rte_table_lpm.h',
		'rte_table_lpm_ipv6.h', 'rte_table_hash.h',
		'rte_lru.h', 'rte_table_array.h',
		'rte_table_stub.h')
deps += ['mbuf', 'port', 'lpm', 'hash']

if arch_subdir == 'x86'
	headers += files('rte_lru_x86.h')
endif

if dpdk_conf.has('RTE_LIBRTE_ACL')
	sources += files('rte_table_acl.c')
	headers += files('rte_table_acl.h')
	deps += ['acl']
endif
