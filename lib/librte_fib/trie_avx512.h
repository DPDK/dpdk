/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _TRIE_AVX512_H_
#define _TRIE_AVX512_H_

void
rte_trie_vec_lookup_bulk_2b(void *p, uint8_t ips[][RTE_FIB6_IPV6_ADDR_SIZE],
	uint64_t *next_hops, const unsigned int n);

void
rte_trie_vec_lookup_bulk_4b(void *p, uint8_t ips[][RTE_FIB6_IPV6_ADDR_SIZE],
	uint64_t *next_hops, const unsigned int n);

void
rte_trie_vec_lookup_bulk_8b(void *p, uint8_t ips[][RTE_FIB6_IPV6_ADDR_SIZE],
	uint64_t *next_hops, const unsigned int n);

#endif /* _TRIE_AVX512_H_ */
