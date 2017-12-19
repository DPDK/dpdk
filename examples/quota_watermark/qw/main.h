/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _MAIN_H_
#define _MAIN_H_

#include "../include/conf.h"

enum ring_state {
	RING_READY,
	RING_OVERLOADED,
};

extern int *quota;
extern unsigned int *low_watermark;
extern unsigned int *high_watermark;

extern uint16_t port_pairs[RTE_MAX_ETHPORTS];

extern struct rte_ring *rings[RTE_MAX_LCORE][RTE_MAX_ETHPORTS];
extern struct rte_mempool *mbuf_pool;


static inline int
is_bit_set(int i, unsigned int mask)
{
	return (1 << i) & mask;
}

#endif /* _MAIN_H_ */
