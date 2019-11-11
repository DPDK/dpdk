/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Vladimir Medvedkin <medvedkinv@gmail.com>
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _DIR24_8_H_
#define _DIR24_8_H_

/**
 * @file
 * DIR24_8 algorithm
 */

#ifdef __cplusplus
extern "C" {
#endif

void *
dir24_8_create(const char *name, int socket_id, struct rte_fib_conf *conf);

void
dir24_8_free(void *p);

rte_fib_lookup_fn_t
dir24_8_get_lookup_fn(struct rte_fib_conf *conf);

int
dir24_8_modify(struct rte_fib *fib, uint32_t ip, uint8_t depth,
	uint64_t next_hop, int op);

#ifdef __cplusplus
}
#endif

#endif /* _DIR24_8_H_ */
