/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _INCLUDE_LINK_H_
#define _INCLUDE_LINK_H_

#include <stdint.h>
#include <sys/queue.h>

#include "common.h"

#ifndef LINK_RXQ_RSS_MAX
#define LINK_RXQ_RSS_MAX                                   16
#endif

struct link {
	TAILQ_ENTRY(link) node;
	char name[NAME_SIZE];
	uint16_t port_id;
	uint32_t n_rxq;
	uint32_t n_txq;
};

TAILQ_HEAD(link_list, link);

int
link_init(void);

struct link *
link_find(const char *name);

struct link_params_rss {
	uint32_t queue_id[LINK_RXQ_RSS_MAX];
	uint32_t n_queues;
};

struct link_params {
	const char *dev_name;
	uint16_t port_id; /**< Valid only when *dev_name* is NULL. */

	struct {
		uint32_t n_queues;
		uint32_t queue_size;
		const char *mempool_name;
		struct link_params_rss *rss;
	} rx;

	struct {
		uint32_t n_queues;
		uint32_t queue_size;
	} tx;

	int promiscuous;
};

struct link *
link_create(const char *name, struct link_params *params);

int
link_is_up(const char *name);

#endif /* _INCLUDE_LINK_H_ */
