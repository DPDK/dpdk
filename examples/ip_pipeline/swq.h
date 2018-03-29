/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _INCLUDE_SWQ_H_
#define _INCLUDE_SWQ_H_

#include <stdint.h>
#include <sys/queue.h>

#include <rte_ring.h>

#include "common.h"

struct swq {
	TAILQ_ENTRY(swq) node;
	char name[NAME_SIZE];
	struct rte_ring *r;
};

TAILQ_HEAD(swq_list, swq);

int
swq_init(void);

struct swq *
swq_find(const char *name);

struct swq_params {
	uint32_t size;
	uint32_t cpu_id;
};

struct swq *
swq_create(const char *name, struct swq_params *params);

#endif /* _INCLUDE_SWQ_H_ */
