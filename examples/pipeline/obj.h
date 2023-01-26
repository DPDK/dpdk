/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _INCLUDE_OBJ_H_
#define _INCLUDE_OBJ_H_

#include <stdint.h>

/*
 * ethdev
 */
#ifndef ETHDEV_RXQ_RSS_MAX
#define ETHDEV_RXQ_RSS_MAX 16
#endif

struct ethdev_params_rss {
	uint32_t queue_id[ETHDEV_RXQ_RSS_MAX];
	uint32_t n_queues;
};

struct ethdev_params {
	struct {
		uint32_t n_queues;
		uint32_t queue_size;
		const char *mempool_name;
		struct ethdev_params_rss *rss;
	} rx;

	struct {
		uint32_t n_queues;
		uint32_t queue_size;
	} tx;

	int promiscuous;
};

int
ethdev_config(const char *name, struct ethdev_params *params);

/*
 * cryptodev
 */
struct cryptodev_params {
	uint32_t n_queue_pairs;
	uint32_t queue_size;
};

int
cryptodev_config(const char *name, struct cryptodev_params *params);

#endif /* _INCLUDE_OBJ_H_ */
