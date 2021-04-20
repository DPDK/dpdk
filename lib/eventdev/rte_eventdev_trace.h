/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef _RTE_EVENTDEV_TRACE_H_
#define _RTE_EVENTDEV_TRACE_H_

/**
 * @file
 *
 * API for ethdev trace support
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_trace_point.h>

#include "rte_eventdev.h"
#include "rte_event_eth_rx_adapter.h"
#include "rte_event_timer_adapter.h"

RTE_TRACE_POINT(
	rte_eventdev_trace_configure,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id,
		const struct rte_event_dev_config *dev_conf, int rc),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u32(dev_conf->dequeue_timeout_ns);
	rte_trace_point_emit_i32(dev_conf->nb_events_limit);
	rte_trace_point_emit_u8(dev_conf->nb_event_queues);
	rte_trace_point_emit_u8(dev_conf->nb_event_ports);
	rte_trace_point_emit_u32(dev_conf->nb_event_queue_flows);
	rte_trace_point_emit_u32(dev_conf->nb_event_port_dequeue_depth);
	rte_trace_point_emit_u32(dev_conf->nb_event_port_enqueue_depth);
	rte_trace_point_emit_u32(dev_conf->event_dev_cfg);
	rte_trace_point_emit_u8(dev_conf->nb_single_link_event_port_queues);
	rte_trace_point_emit_int(rc);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_queue_setup,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint8_t queue_id,
		const struct rte_event_queue_conf *queue_conf),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u8(queue_id);
	rte_trace_point_emit_u32(queue_conf->nb_atomic_flows);
	rte_trace_point_emit_u32(queue_conf->nb_atomic_order_sequences);
	rte_trace_point_emit_u32(queue_conf->event_queue_cfg);
	rte_trace_point_emit_u8(queue_conf->schedule_type);
	rte_trace_point_emit_u8(queue_conf->priority);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_port_setup,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint8_t port_id,
		const struct rte_event_port_conf *port_conf, int rc),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u8(port_id);
	rte_trace_point_emit_i32(port_conf->new_event_threshold);
	rte_trace_point_emit_u16(port_conf->dequeue_depth);
	rte_trace_point_emit_u16(port_conf->enqueue_depth);
	rte_trace_point_emit_u32(port_conf->event_port_cfg);
	rte_trace_point_emit_int(rc);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_port_link,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint8_t port_id,
		uint16_t nb_links, int rc),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u8(port_id);
	rte_trace_point_emit_u16(nb_links);
	rte_trace_point_emit_int(rc);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_port_unlink,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint8_t port_id,
		uint16_t nb_unlinks, int rc),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u8(port_id);
	rte_trace_point_emit_u16(nb_unlinks);
	rte_trace_point_emit_int(rc);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_start,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, int rc),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_int(rc);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_stop,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id),
	rte_trace_point_emit_u8(dev_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_close,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id),
	rte_trace_point_emit_u8(dev_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_rx_adapter_create,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, uint8_t dev_id, void *conf_cb,
		void *conf_arg),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(conf_cb);
	rte_trace_point_emit_ptr(conf_arg);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_rx_adapter_free,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id),
	rte_trace_point_emit_u8(adptr_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_rx_adapter_queue_add,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, uint16_t eth_dev_id,
		int32_t rx_queue_id,
		const struct rte_event_eth_rx_adapter_queue_conf *queue_conf,
		int rc),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_u16(eth_dev_id);
	rte_trace_point_emit_i32(rx_queue_id);
	rte_trace_point_emit_u32(queue_conf->rx_queue_flags);
	rte_trace_point_emit_u16(queue_conf->servicing_weight);
	rte_trace_point_emit_u8(queue_conf->ev.queue_id);
	rte_trace_point_emit_u8(queue_conf->ev.priority);
	rte_trace_point_emit_int(rc);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_rx_adapter_queue_del,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, uint16_t eth_dev_id,
		int32_t rx_queue_id, int rc),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_u16(eth_dev_id);
	rte_trace_point_emit_i32(rx_queue_id);
	rte_trace_point_emit_int(rc);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_rx_adapter_start,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id),
	rte_trace_point_emit_u8(adptr_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_rx_adapter_stop,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id),
	rte_trace_point_emit_u8(adptr_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_tx_adapter_create,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, uint8_t dev_id, void *conf_cb,
		struct rte_event_port_conf *port_conf, int rc),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_i32(port_conf->new_event_threshold);
	rte_trace_point_emit_u16(port_conf->dequeue_depth);
	rte_trace_point_emit_u16(port_conf->enqueue_depth);
	rte_trace_point_emit_u32(port_conf->event_port_cfg);
	rte_trace_point_emit_ptr(conf_cb);
	rte_trace_point_emit_int(rc);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_tx_adapter_free,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, int rc),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_int(rc);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_tx_adapter_queue_add,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, uint16_t eth_dev_id,
		int32_t queue, int rc),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_u16(eth_dev_id);
	rte_trace_point_emit_i32(queue);
	rte_trace_point_emit_int(rc);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_tx_adapter_queue_del,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, uint16_t eth_dev_id,
		int32_t queue, int rc),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_u16(eth_dev_id);
	rte_trace_point_emit_i32(queue);
	rte_trace_point_emit_int(rc);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_tx_adapter_start,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, int rc),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_int(rc);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_tx_adapter_stop,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, int rc),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_int(rc);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_timer_adapter_create,
	RTE_TRACE_POINT_ARGS(uint16_t adapter_id, void *adapter,
		const struct rte_event_timer_adapter_conf *conf,
		void *conf_cb),
	rte_trace_point_emit_u16(adapter_id);
	rte_trace_point_emit_ptr(adapter);
	rte_trace_point_emit_ptr(conf);
	rte_trace_point_emit_u8(conf->event_dev_id);
	rte_trace_point_emit_u16(conf->timer_adapter_id);
	rte_trace_point_emit_u64(conf->timer_tick_ns);
	rte_trace_point_emit_u64(conf->max_tmo_ns);
	rte_trace_point_emit_u64(conf->nb_timers);
	rte_trace_point_emit_u64(conf->flags);
	rte_trace_point_emit_ptr(conf_cb);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_timer_adapter_start,
	RTE_TRACE_POINT_ARGS(const void *adapter),
	rte_trace_point_emit_ptr(adapter);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_timer_adapter_stop,
	RTE_TRACE_POINT_ARGS(const void *adapter),
	rte_trace_point_emit_ptr(adapter);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_timer_adapter_free,
	RTE_TRACE_POINT_ARGS(void *adapter),
	rte_trace_point_emit_ptr(adapter);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_crypto_adapter_create,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, uint8_t dev_id, void *adapter,
		struct rte_event_port_conf *port_conf, uint8_t mode),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(adapter);
	rte_trace_point_emit_u8(mode);
	rte_trace_point_emit_i32(port_conf->new_event_threshold);
	rte_trace_point_emit_u16(port_conf->dequeue_depth);
	rte_trace_point_emit_u16(port_conf->enqueue_depth);
	rte_trace_point_emit_u32(port_conf->event_port_cfg);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_crypto_adapter_free,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, void *adapter),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_ptr(adapter);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_crypto_adapter_queue_pair_add,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, uint8_t cdev_id,
		const void *event, int32_t queue_pair_id),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_u8(cdev_id);
	rte_trace_point_emit_i32(queue_pair_id);
	rte_trace_point_emit_ptr(event);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_crypto_adapter_queue_pair_del,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, uint8_t cdev_id,
		int32_t queue_pair_id, int rc),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_u8(cdev_id);
	rte_trace_point_emit_i32(queue_pair_id);
	rte_trace_point_emit_int(rc);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_crypto_adapter_start,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, void *adapter),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_ptr(adapter);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_crypto_adapter_stop,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id),
	rte_trace_point_emit_u8(adptr_id);
)

#ifdef __cplusplus
}
#endif

#endif /* _RTE_EVENTDEV_TRACE_H_ */
