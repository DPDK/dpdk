/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */
#ifndef _CXGBE_FLOW_H_
#define _CXGBE_FLOW_H_

#include <rte_flow_driver.h>
#include "cxgbe_filter.h"
#include "mps_tcam.h"
#include "cxgbe.h"

/* Max poll time is 100 * 100msec = 10 sec */
#define CXGBE_FLOW_POLL_MS  100 /* 100 milliseconds */
#define CXGBE_FLOW_POLL_CNT 100 /* Max number of times to poll */

struct chrte_fparse {
	int (*fptr)(const void *mask, /* currently supported mask */
		    const struct rte_flow_item *item, /* user input */
		    struct ch_filter_specification *fs, /* where to parse */
		    struct rte_flow_error *e);
	const void *dmask; /* Specify what is supported by chelsio by default*/
};

struct rte_flow {
	struct filter_entry *f;
	struct ch_filter_specification fs; /* temp, to create filter */
	struct chrte_fparse *item_parser;
	/*
	 * filter_entry doesn't store user priority.
	 * Post creation of filter this will indicate the
	 * flow index (fidx) for both hash and tcam filters
	 */
	unsigned int fidx;
	struct rte_eth_dev *dev;
};

int cxgbe_dev_flow_ops_get(struct rte_eth_dev *dev,
			   const struct rte_flow_ops **ops);

#endif /* _CXGBE_FLOW_H_ */
