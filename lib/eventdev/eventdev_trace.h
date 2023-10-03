/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef EVENTDEV_TRACE_H
#define EVENTDEV_TRACE_H

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
#include "rte_event_crypto_adapter.h"
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
	rte_trace_point_emit_ptr(port_conf);
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
	rte_eventdev_trace_port_profile_links_set,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint8_t port_id,
		uint16_t nb_links, uint8_t profile_id, int rc),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u8(port_id);
	rte_trace_point_emit_u16(nb_links);
	rte_trace_point_emit_u8(profile_id);
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
	rte_eventdev_trace_port_profile_unlink,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint8_t port_id,
		uint16_t nb_unlinks, uint8_t profile_id, int rc),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u8(port_id);
	rte_trace_point_emit_u16(nb_unlinks);
	rte_trace_point_emit_u8(profile_id);
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
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, uint8_t dev_id,
		struct rte_event_port_conf *port_config, int mode, int ret),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(port_config);
	rte_trace_point_emit_i32(port_config->new_event_threshold);
	rte_trace_point_emit_u16(port_config->dequeue_depth);
	rte_trace_point_emit_u16(port_config->enqueue_depth);
	rte_trace_point_emit_u32(port_config->event_port_cfg);
	rte_trace_point_emit_int(mode);
	rte_trace_point_emit_int(ret);
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
		int32_t queue_pair_id,
		const struct rte_event_crypto_adapter_queue_conf *conf),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_u8(cdev_id);
	rte_trace_point_emit_i32(queue_pair_id);
	rte_trace_point_emit_ptr(conf);
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

RTE_TRACE_POINT(
	rte_eventdev_trace_crypto_adapter_event_port_get,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, uint8_t event_port_id),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_u8(event_port_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_crypto_adapter_service_id_get,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, uint32_t service_id),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_u32(service_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_attr_get,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, const void *dev, uint32_t attr_id,
		uint32_t attr_value),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(dev);
	rte_trace_point_emit_u32(attr_id);
	rte_trace_point_emit_u32(attr_value);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_get_dev_id,
	RTE_TRACE_POINT_ARGS(const char *name, int dev_id),
	rte_trace_point_emit_string(name);
	rte_trace_point_emit_int(dev_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_info_get,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, const void *dev_info, const void *dev),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(dev_info);
	rte_trace_point_emit_ptr(dev);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_service_id_get,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint32_t service_id),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u32(service_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_socket_id,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, const void *dev, int socket_id),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(dev);
	rte_trace_point_emit_int(socket_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_stop_flush_callback_register,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, const void *callback, const void *userdata),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(callback);
	rte_trace_point_emit_ptr(userdata);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_rx_adapter_caps_get,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t eth_port_id),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(eth_port_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_rx_adapter_cb_register,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, uint16_t eth_dev_id, const void *cb_fn,
		const void *cb_arg),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_u16(eth_dev_id);
	rte_trace_point_emit_ptr(cb_fn);
	rte_trace_point_emit_ptr(cb_arg);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_rx_adapter_create_with_params,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, uint8_t dev_id,
		const struct rte_event_port_conf *port_config,
		const struct rte_event_eth_rx_adapter_params *rxa_params,
		int ret),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(port_config);
	rte_trace_point_emit_i32(port_config->new_event_threshold);
	rte_trace_point_emit_u16(port_config->dequeue_depth);
	rte_trace_point_emit_u16(port_config->enqueue_depth);
	rte_trace_point_emit_u32(port_config->event_port_cfg);
	rte_trace_point_emit_ptr(rxa_params);
	rte_trace_point_emit_u16(rxa_params->event_buf_size);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_rx_adapter_service_id_get,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, uint32_t service_id),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_u32(service_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_rx_adapter_event_port_get,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, uint8_t event_port_id),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_u8(event_port_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_rx_adapter_vector_limits_get,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t eth_port_id,
		uint16_t min_sz, uint16_t max_sz, uint8_t log2_sz,
		uint64_t min_timeout_ns, uint64_t max_timeout_ns, int ret),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(eth_port_id);
	rte_trace_point_emit_u16(min_sz);
	rte_trace_point_emit_u16(max_sz);
	rte_trace_point_emit_u8(log2_sz);
	rte_trace_point_emit_u64(min_timeout_ns);
	rte_trace_point_emit_u64(max_timeout_ns);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_tx_adapter_caps_get,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, const void *dev, uint16_t eth_port_id,
		const void *eth_dev),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(dev);
	rte_trace_point_emit_u16(eth_port_id);
	rte_trace_point_emit_ptr(eth_dev);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_tx_adapter_event_port_get,
	RTE_TRACE_POINT_ARGS(uint8_t id),
	rte_trace_point_emit_u8(id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_tx_adapter_service_id_get,
	RTE_TRACE_POINT_ARGS(uint8_t id, uint32_t service_id),
	rte_trace_point_emit_u8(id);
	rte_trace_point_emit_u32(service_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_port_attr_get,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, const void *dev, uint8_t port_id,
		uint32_t attr_id, uint32_t attr_value),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(dev);
	rte_trace_point_emit_u8(port_id);
	rte_trace_point_emit_u32(attr_id);
	rte_trace_point_emit_u32(attr_value);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_port_default_conf_get,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, const void *dev, uint8_t port_id,
		const struct rte_event_port_conf *port_conf),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(dev);
	rte_trace_point_emit_u8(port_id);
	rte_trace_point_emit_ptr(port_conf);
	rte_trace_point_emit_i32(port_conf->new_event_threshold);
	rte_trace_point_emit_u16(port_conf->dequeue_depth);
	rte_trace_point_emit_u16(port_conf->enqueue_depth);
	rte_trace_point_emit_u32(port_conf->event_port_cfg);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_port_links_get,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint8_t port_id, int count),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u8(port_id);
	rte_trace_point_emit_int(count);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_port_profile_links_get,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint8_t port_id, uint8_t profile_id,
		int count),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u8(port_id);
	rte_trace_point_emit_u8(profile_id);
	rte_trace_point_emit_int(count);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_port_unlinks_in_progress,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint8_t port_id),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u8(port_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_queue_attr_get,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, const void *dev, uint8_t queue_id,
		uint32_t attr_id, uint32_t attr_value),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(dev);
	rte_trace_point_emit_u8(queue_id);
	rte_trace_point_emit_u32(attr_id);
	rte_trace_point_emit_u32(attr_value);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_queue_default_conf_get,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, const void *dev, uint8_t queue_id,
		const struct rte_event_queue_conf *queue_conf),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(dev);
	rte_trace_point_emit_u8(queue_id);
	rte_trace_point_emit_ptr(queue_conf);
	rte_trace_point_emit_u32(queue_conf->nb_atomic_flows);
	rte_trace_point_emit_u32(queue_conf->nb_atomic_order_sequences);
	rte_trace_point_emit_u32(queue_conf->event_queue_cfg);
	rte_trace_point_emit_u8(queue_conf->schedule_type);
	rte_trace_point_emit_u8(queue_conf->priority);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_ring_create,
	RTE_TRACE_POINT_ARGS(const char *name, unsigned int count,
		int socket_id, unsigned int flags),
	rte_trace_point_emit_string(name);
	rte_trace_point_emit_u32(count);
	rte_trace_point_emit_int(socket_id);
	rte_trace_point_emit_u32(flags);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_ring_free,
	RTE_TRACE_POINT_ARGS(const char *name),
	rte_trace_point_emit_string(name);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_ring_init,
	RTE_TRACE_POINT_ARGS(const void *r, const char *name,
		unsigned int count, unsigned int flags),
	rte_trace_point_emit_ptr(r);
	rte_trace_point_emit_string(name);
	rte_trace_point_emit_u32(count);
	rte_trace_point_emit_u32(flags);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_ring_lookup,
	RTE_TRACE_POINT_ARGS(const char *name),
	rte_trace_point_emit_string(name);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_timer_adapter_caps_get,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id),
	rte_trace_point_emit_u8(dev_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_timer_adapter_get_info,
	RTE_TRACE_POINT_ARGS(const void *adapter,
		const struct rte_event_timer_adapter_info *adapter_info),
	rte_trace_point_emit_ptr(adapter);
	rte_trace_point_emit_ptr(adapter_info);
	rte_trace_point_emit_u64(adapter_info->min_resolution_ns);
	rte_trace_point_emit_u64(adapter_info->max_tmo_ns);
	rte_trace_point_emit_u32(adapter_info->caps);
	rte_trace_point_emit_u16(adapter_info->event_dev_port_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_timer_adapter_lookup,
	RTE_TRACE_POINT_ARGS(uint16_t adapter_id, const void *adapter),
	rte_trace_point_emit_u16(adapter_id);
	rte_trace_point_emit_ptr(adapter);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_timer_adapter_service_id_get,
	RTE_TRACE_POINT_ARGS(const struct rte_event_timer_adapter *adapter,
		uint32_t service_id),
	rte_trace_point_emit_ptr(adapter);
	rte_trace_point_emit_u32(service_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_vector_pool_create,
	RTE_TRACE_POINT_ARGS(const void *mp, const char *name, int socket_id,
		uint32_t size, uint32_t cache_size, uint32_t elt_size),
	rte_trace_point_emit_ptr(mp);
	rte_trace_point_emit_string(name);
	rte_trace_point_emit_int(socket_id);
	rte_trace_point_emit_u32(size);
	rte_trace_point_emit_u32(cache_size);
	rte_trace_point_emit_u32(elt_size);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_rx_adapter_queue_conf_get,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, uint16_t eth_dev_id,
		uint16_t rx_queue_id, const void *queue_conf),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_u16(eth_dev_id);
	rte_trace_point_emit_u16(rx_queue_id);
	rte_trace_point_emit_ptr(queue_conf);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_queue_attr_set,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint8_t queue_id, uint32_t attr_id,
		uint64_t attr_value),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u8(queue_id);
	rte_trace_point_emit_u32(attr_id);
	rte_trace_point_emit_u64(attr_value);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_port_quiesce,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, const void *dev, uint8_t port_id, const void *args),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(dev);
	rte_trace_point_emit_u8(port_id);
	rte_trace_point_emit_ptr(args);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_crypto_adapter_caps_get,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, const void *dev, uint8_t cdev_id,
		const void *cdev),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(dev);
	rte_trace_point_emit_u8(cdev_id);
	rte_trace_point_emit_ptr(cdev);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_dequeue_timeout_ticks,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint64_t ns,
		const void *timeout_ticks),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u64(ns);
	rte_trace_point_emit_ptr(timeout_ticks);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_crypto_adapter_stats_get,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, const void *stats,
		uint64_t event_poll_count, uint64_t event_deq_count,
		uint64_t crypto_enq_count, uint64_t crypto_enq_fail,
		uint64_t crypto_deq_count, uint64_t event_enq_count,
		uint64_t event_enq_retry_count, uint64_t event_enq_fail_count),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_ptr(stats);
	rte_trace_point_emit_u64(event_poll_count);
	rte_trace_point_emit_u64(event_deq_count);
	rte_trace_point_emit_u64(crypto_enq_count);
	rte_trace_point_emit_u64(crypto_enq_fail);
	rte_trace_point_emit_u64(crypto_deq_count);
	rte_trace_point_emit_u64(event_enq_count);
	rte_trace_point_emit_u64(event_enq_retry_count);
	rte_trace_point_emit_u64(event_enq_fail_count);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_crypto_adapter_stats_reset,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id),
	rte_trace_point_emit_u8(adptr_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_rx_adapter_stats_get,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, const void *stats),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_ptr(stats);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_crypto_adapter_vector_limits_get,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t cdev_id,
		const void *limits),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(cdev_id);
	rte_trace_point_emit_ptr(limits);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_rx_adapter_queue_stats_get,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, uint16_t eth_dev_id,
		uint16_t rx_queue_id, const void *stats),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_u16(eth_dev_id);
	rte_trace_point_emit_u16(rx_queue_id);
	rte_trace_point_emit_ptr(stats);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_rx_adapter_stats_reset,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id),
	rte_trace_point_emit_u8(adptr_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_rx_adapter_queue_stats_reset,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, uint16_t eth_dev_id,
		uint16_t rx_queue_id),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_u16(eth_dev_id);
	rte_trace_point_emit_u16(rx_queue_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_rx_adapter_instance_get,
	RTE_TRACE_POINT_ARGS(uint16_t eth_dev_id, uint16_t rx_queue_id,
		uint8_t rxa_inst_id),
	rte_trace_point_emit_u16(eth_dev_id);
	rte_trace_point_emit_u16(rx_queue_id);
	rte_trace_point_emit_u8(rxa_inst_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_tx_adapter_stats_get,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, uint64_t tx_retry,
		uint64_t tx_packets, uint64_t tx_dropped, int ret),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_u64(tx_retry);
	rte_trace_point_emit_u64(tx_packets);
	rte_trace_point_emit_u64(tx_dropped);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_tx_adapter_stats_reset,
	RTE_TRACE_POINT_ARGS(uint8_t adptr_id, int ret),
	rte_trace_point_emit_u8(adptr_id);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_tx_adapter_instance_get,
	RTE_TRACE_POINT_ARGS(uint16_t eth_dev_id, uint16_t tx_queue_id,
		uint8_t txa_inst_id),
	rte_trace_point_emit_u16(eth_dev_id);
	rte_trace_point_emit_u16(tx_queue_id);
	rte_trace_point_emit_u8(txa_inst_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_tx_adapter_queue_start,
	RTE_TRACE_POINT_ARGS(uint16_t eth_dev_id, uint16_t tx_queue_id),
	rte_trace_point_emit_u16(eth_dev_id);
	rte_trace_point_emit_u16(tx_queue_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_eth_tx_adapter_queue_stop,
	RTE_TRACE_POINT_ARGS(uint16_t eth_dev_id, uint16_t tx_queue_id),
	rte_trace_point_emit_u16(eth_dev_id);
	rte_trace_point_emit_u16(tx_queue_id);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_timer_adapter_stats_get,
	RTE_TRACE_POINT_ARGS(const void *adapter, const void *stats),
	rte_trace_point_emit_ptr(adapter);
	rte_trace_point_emit_ptr(stats);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_timer_adapter_stats_reset,
	RTE_TRACE_POINT_ARGS(const void *adapter),
	rte_trace_point_emit_ptr(adapter);
)

RTE_TRACE_POINT(
	rte_eventdev_trace_timer_remaining_ticks_get,
	RTE_TRACE_POINT_ARGS(const void *adapter, const void *evtim, const void *ticks_remaining),
	rte_trace_point_emit_ptr(adapter);
	rte_trace_point_emit_ptr(evtim);
	rte_trace_point_emit_ptr(ticks_remaining);
)

#ifdef __cplusplus
}
#endif

#endif /* EVENTDEV_TRACE_H */
