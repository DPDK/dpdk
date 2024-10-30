/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _STREAM_BINARY_FLOW_API_H_
#define _STREAM_BINARY_FLOW_API_H_

#include "rte_flow.h"
#include "rte_flow_driver.h"

/* Max RSS hash key length in bytes */
#define MAX_RSS_KEY_LEN 40

/*
 * Flow frontend for binary programming interface
 */

#define FLOW_MAX_QUEUES 128

#define RAW_ENCAP_DECAP_ELEMS_MAX 16
/*
 * Flow eth dev profile determines how the FPGA module resources are
 * managed and what features are available
 */
enum flow_eth_dev_profile {
	FLOW_ETH_DEV_PROFILE_INLINE = 0,
};

struct flow_queue_id_s {
	int id;
	int hw_id;
};

/*
 * RTE_FLOW_ACTION_TYPE_RAW_ENCAP
 */
struct flow_action_raw_encap {
	uint8_t *data;
	uint8_t *preserve;
	size_t size;
	struct rte_flow_item items[RAW_ENCAP_DECAP_ELEMS_MAX];
	int item_count;
};

/*
 * RTE_FLOW_ACTION_TYPE_RAW_DECAP
 */
struct flow_action_raw_decap {
	uint8_t *data;
	size_t size;
	struct rte_flow_item items[RAW_ENCAP_DECAP_ELEMS_MAX];
	int item_count;
};

struct flow_eth_dev;             /* port device */
struct flow_handle;

#endif  /* _STREAM_BINARY_FLOW_API_H_ */
