/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _STREAM_BINARY_FLOW_API_H_
#define _STREAM_BINARY_FLOW_API_H_

#include "rte_flow.h"
#include "rte_flow_driver.h"
/*
 * Flow frontend for binary programming interface
 */

#define FLOW_MAX_QUEUES 128

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

struct flow_eth_dev;             /* port device */
struct flow_handle;

#endif  /* _STREAM_BINARY_FLOW_API_H_ */
