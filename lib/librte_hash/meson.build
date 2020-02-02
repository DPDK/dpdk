# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

headers = files('rte_crc_arm64.h',
	'rte_fbk_hash.h',
	'rte_hash_crc.h',
	'rte_hash.h',
	'rte_jhash.h',
	'rte_thash.h')

sources = files('rte_cuckoo_hash.c', 'rte_fbk_hash.c')
deps += ['ring']

# rte ring reset is not yet part of stable API
allow_experimental_apis = true
