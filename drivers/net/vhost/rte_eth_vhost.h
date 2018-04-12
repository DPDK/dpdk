/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 IGEL Co., Ltd.
 * Copyright(c) 2016-2018 Intel Corporation
 */
#ifndef _RTE_ETH_VHOST_H_
#define _RTE_ETH_VHOST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include <rte_vhost.h>

/*
 * Event description.
 */
struct rte_eth_vhost_queue_event {
	uint16_t queue_id;
	bool rx;
	bool enable;
};

/**
 * Get queue events from specified port.
 * If a callback for below event is registered by
 * rte_eth_dev_callback_register(), this function will describe what was
 * changed.
 *  - RTE_ETH_EVENT_QUEUE_STATE
 * Multiple events may cause only one callback kicking, so call this function
 * while returning 0.
 *
 * @param port_id
 *  Port id.
 * @param event
 *  Pointer to a rte_eth_vhost_queue_event structure.
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
int rte_eth_vhost_get_queue_event(uint16_t port_id,
		struct rte_eth_vhost_queue_event *event);

/**
 * Get the 'vid' value associated with the specified port.
 *
 * @return
 *  - On success, the 'vid' associated with 'port_id'.
 *  - On failure, a negative value.
 */
int rte_eth_vhost_get_vid_from_port_id(uint16_t port_id);

#ifdef __cplusplus
}
#endif

#endif
