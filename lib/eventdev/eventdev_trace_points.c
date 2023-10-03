/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <rte_trace_point_register.h>

#include "eventdev_trace.h"

/* Eventdev trace points */
RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_configure,
	lib.eventdev.configure)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_queue_setup,
	lib.eventdev.queue.setup)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_port_setup,
	lib.eventdev.port.setup)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_port_link,
	lib.eventdev.port.link)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_port_profile_links_set,
	lib.eventdev.port.profile.links.set)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_port_unlink,
	lib.eventdev.port.unlink)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_port_profile_unlink,
	lib.eventdev.port.profile.unlink)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_start,
	lib.eventdev.start)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_stop,
	lib.eventdev.stop)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_close,
	lib.eventdev.close)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_enq_burst,
	lib.eventdev.enq.burst)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_deq_burst,
	lib.eventdev.deq.burst)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_maintain,
	lib.eventdev.maintain)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_port_profile_switch,
	lib.eventdev.port.profile.switch)

/* Eventdev Rx adapter trace points */
RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_create,
	lib.eventdev.rx.adapter.create)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_free,
	lib.eventdev.rx.adapter.free)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_queue_add,
	lib.eventdev.rx.adapter.queue.add)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_queue_del,
	lib.eventdev.rx.adapter.queue.del)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_start,
	lib.eventdev.rx.adapter.start)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_stop,
	lib.eventdev.rx.adapter.stop)

/* Eventdev Tx adapter trace points */
RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_create,
	lib.eventdev.tx.adapter.create)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_free,
	lib.eventdev.tx.adapter.free)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_queue_add,
	lib.eventdev.tx.adapter.queue.add)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_queue_del,
	lib.eventdev.tx.adapter.queue.del)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_start,
	lib.eventdev.tx.adapter.start)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_stop,
	lib.eventdev.tx.adapter.stop)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_enqueue,
	lib.eventdev.tx.adapter.enq)

/* Eventdev Timer adapter trace points */
RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_timer_adapter_create,
	lib.eventdev.timer.create)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_timer_adapter_start,
	lib.eventdev.timer.start)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_timer_adapter_stop,
	lib.eventdev.timer.stop)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_timer_adapter_free,
	lib.eventdev.timer.free)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_timer_arm_burst,
	lib.eventdev.timer.burst)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_timer_arm_tmo_tick_burst,
	lib.eventdev.timer.tick.burst)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_timer_cancel_burst,
	lib.eventdev.timer.cancel)

/* Eventdev Crypto adapter trace points */
RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_crypto_adapter_create,
	lib.eventdev.crypto.create)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_crypto_adapter_free,
	lib.eventdev.crypto.free)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_crypto_adapter_queue_pair_add,
	lib.eventdev.crypto.queue.add)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_crypto_adapter_queue_pair_del,
	lib.eventdev.crypto.queue.del)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_crypto_adapter_start,
	lib.eventdev.crypto.start)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_crypto_adapter_stop,
	lib.eventdev.crypto.stop)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_crypto_adapter_enqueue,
	lib.eventdev.crypto.enq)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_crypto_adapter_event_port_get,
	lib.eventdev.crypto.adapter_event_port_get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_crypto_adapter_service_id_get,
	lib.eventdev.crypto.adapter_service_id_get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_crypto_adapter_vector_limits_get,
	lib.eventdev.crypto.adapter_vector_limits_get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_attr_get,
	lib.eventdev.attr_get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_get_dev_id,
	lib.eventdev.get_dev_id)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_info_get,
	lib.eventdev.info_get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_service_id_get,
	lib.eventdev.service_id_get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_socket_id,
	lib.eventdev.socket.id)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_stop_flush_callback_register,
	lib.eventdev.stop.flush.callback.register)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_caps_get,
	lib.eventdev.eth.rx.adapter.caps.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_cb_register,
	lib.eventdev.eth.rx.adapter.cb.register)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_service_id_get,
	lib.eventdev.eth.rx.adapter.service.id.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_event_port_get,
	lib.eventdev.eth.rx.adapter.event.port.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_vector_limits_get,
	lib.eventdev.eth.rx.adapter.vector.limits.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_queue_stats_get,
	lib.eventdev.eth.rx.adapter.queue.stats.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_stats_reset,
	lib.eventdev.eth.rx.adapter.stats.reset)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_queue_stats_reset,
	lib.eventdev.eth.rx.adapter.queue.stats.reset)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_instance_get,
	lib.eventdev.eth.rx.adapter.instance.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_caps_get,
	lib.eventdev.eth.tx.adapter.caps.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_event_port_get,
	lib.eventdev.eth.tx.adapter.event.port.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_service_id_get,
	lib.eventdev.eth.tx.adapter.service.id.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_stats_get,
	lib.eventdev.eth.tx.adapter.stats.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_stats_reset,
	lib.eventdev.eth.tx.adapter.stats.reset)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_instance_get,
	lib.eventdev.eth.tx.adapter.instance.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_port_attr_get,
	lib.eventdev.port.attr.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_port_default_conf_get,
	lib.eventdev.port.default.conf.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_port_links_get,
	lib.eventdev.port.links.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_port_profile_links_get,
	lib.eventdev.port.profile.links.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_port_unlinks_in_progress,
	lib.eventdev.port.unlinks.in.progress)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_queue_attr_get,
	lib.eventdev.queue.attr.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_queue_default_conf_get,
	lib.eventdev.queue.default.conf.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_ring_create,
	lib.eventdev.ring.create)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_ring_free,
	lib.eventdev.ring.free)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_ring_init,
	lib.eventdev.ring.init)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_ring_lookup,
	lib.eventdev.ring.lookup)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_timer_adapter_caps_get,
	lib.eventdev.timer.adapter.caps.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_timer_adapter_get_info,
	lib.eventdev.timer.adapter.get.info)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_timer_adapter_lookup,
	lib.eventdev.timer.adapter.lookup)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_timer_adapter_service_id_get,
	lib.eventdev.timer.adapter.service.id.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_vector_pool_create,
	lib.eventdev.vector.pool.create)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_create_with_params,
	lib.eventdev.eth.rx.adapter.create.with.params)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_queue_conf_get,
	lib.eventdev.eth.rx.adapter.queue.conf.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_queue_attr_set,
	lib.eventdev.queue.attr.set)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_port_quiesce,
	lib.eventdev.port.quiesce)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_crypto_adapter_caps_get,
	lib.eventdev.crypto.adapter.caps.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_dequeue_timeout_ticks,
	lib.eventdev.dequeue.timeout.ticks)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_crypto_adapter_stats_get,
	lib.eventdev.crypto.adapter.stats.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_crypto_adapter_stats_reset,
	lib.eventdev.crypto.adapter.stats.reset)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_rx_adapter_stats_get,
	lib.eventdev.rx.adapter.stats.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_queue_start,
	lib.eventdev.tx.adapter.queue.start)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_eth_tx_adapter_queue_stop,
	lib.eventdev.tx.adapter.queue.stop)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_timer_adapter_stats_get,
	lib.eventdev.timer.adapter.stats.get)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_timer_adapter_stats_reset,
	lib.eventdev.timer.adapter.stats.reset)

RTE_TRACE_POINT_REGISTER(rte_eventdev_trace_timer_remaining_ticks_get,
	lib.eventdev.timer.remaining.ticks.get)
