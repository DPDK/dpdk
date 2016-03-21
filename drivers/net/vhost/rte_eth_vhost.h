/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 IGEL Co., Ltd.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of IGEL Co., Ltd. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RTE_ETH_VHOST_H_
#define _RTE_ETH_VHOST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include <rte_virtio_net.h>

/**
 * Disable features in feature_mask.
 *
 * @param feature_mask
 *  Vhost features defined in "linux/virtio_net.h".
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
int rte_eth_vhost_feature_disable(uint64_t feature_mask);

/**
 * Enable features in feature_mask.
 *
 * @param feature_mask
 *  Vhost features defined in "linux/virtio_net.h".
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
int rte_eth_vhost_feature_enable(uint64_t feature_mask);

/**
 * Returns currently supported vhost features.
 *
 * @return
 *  Vhost features defined in "linux/virtio_net.h".
 */
uint64_t rte_eth_vhost_feature_get(void);

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
int rte_eth_vhost_get_queue_event(uint8_t port_id,
		struct rte_eth_vhost_queue_event *event);

#ifdef __cplusplus
}
#endif

#endif
