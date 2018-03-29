/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _INCLUDE_MEMPOOL_H_
#define _INCLUDE_MEMPOOL_H_

#include <stdint.h>
#include <sys/queue.h>

#include <rte_mempool.h>

#include "common.h"

struct mempool {
	TAILQ_ENTRY(mempool) node;
	char name[NAME_SIZE];
	struct rte_mempool *m;
	uint32_t buffer_size;
};

TAILQ_HEAD(mempool_list, mempool);

int
mempool_init(void);

struct mempool *
mempool_find(const char *name);

struct mempool_params {
	uint32_t buffer_size;
	uint32_t pool_size;
	uint32_t cache_size;
	uint32_t cpu_id;
};

struct mempool *
mempool_create(const char *name, struct mempool_params *params);

#endif /* _INCLUDE_MEMPOOL_H_ */
