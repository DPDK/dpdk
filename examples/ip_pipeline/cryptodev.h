/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _INCLUDE_SYM_C_H_
#define _INCLUDE_SYM_C_H_

#include <stdint.h>
#include <sys/queue.h>

#include <rte_cryptodev.h>

#include "common.h"

struct cryptodev {
	TAILQ_ENTRY(cryptodev) node;
	char name[NAME_SIZE];
	uint16_t dev_id;
	uint32_t n_queues;
	struct rte_mempool *mp_create;
	struct rte_mempool *mp_init;
};

TAILQ_HEAD(cryptodev_list, cryptodev);

int
cryptodev_init(void);

struct cryptodev *
cryptodev_find(const char *name);

struct cryptodev *
cryptodev_next(struct cryptodev *cryptodev);

struct cryptodev_params {
	const char *dev_name;
	uint32_t dev_id; /**< Valid only when *dev_name* is NULL. */
	uint32_t n_queues;
	uint32_t queue_size;
	uint32_t session_pool_size;
};

struct cryptodev *
cryptodev_create(const char *name, struct cryptodev_params *params);

#endif
