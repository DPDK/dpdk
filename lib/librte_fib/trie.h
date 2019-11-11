/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Vladimir Medvedkin <medvedkinv@gmail.com>
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _TRIE_H_
#define _TRIE_H_

/**
 * @file
 * RTE IPv6 Longest Prefix Match (LPM)
 */

#ifdef __cplusplus
extern "C" {
#endif

void *
trie_create(const char *name, int socket_id, struct rte_fib6_conf *conf);

void
trie_free(void *p);

rte_fib6_lookup_fn_t
rte_trie_get_lookup_fn(struct rte_fib6_conf *fib_conf);

int
trie_modify(struct rte_fib6 *fib, const uint8_t ip[RTE_FIB6_IPV6_ADDR_SIZE],
	uint8_t depth, uint64_t next_hop, int op);


#ifdef __cplusplus
}
#endif

#endif /* _TRIE_H_ */
