/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _STREAM_BINARY_FLOW_API_H_
#define _STREAM_BINARY_FLOW_API_H_

/*
 * Flow frontend for binary programming interface
 */

#define FLOW_MAX_QUEUES 128

struct flow_queue_id_s {
	int id;
	int hw_id;
};

struct flow_eth_dev;             /* port device */

#endif  /* _STREAM_BINARY_FLOW_API_H_ */
