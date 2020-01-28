/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __L3FWD_EVENTDEV_H__
#define __L3FWD_EVENTDEV_H__

#include <rte_common.h>
#include <rte_eventdev.h>
#include <rte_event_eth_rx_adapter.h>
#include <rte_event_eth_tx_adapter.h>
#include <rte_service.h>
#include <rte_spinlock.h>

#include "l3fwd.h"

#define L3FWD_EVENT_SINGLE     0x1
#define L3FWD_EVENT_BURST      0x2
#define L3FWD_EVENT_TX_DIRECT  0x4
#define L3FWD_EVENT_TX_ENQ     0x8

typedef uint32_t (*event_device_setup_cb)(void);
typedef void (*event_queue_setup_cb)(uint32_t event_queue_cfg);
typedef void (*event_port_setup_cb)(void);
typedef void (*adapter_setup_cb)(void);
typedef int (*event_loop_cb)(void *);

struct l3fwd_event_queues {
	uint8_t *event_q_id;
	uint8_t	nb_queues;
};

struct l3fwd_event_ports {
	uint8_t *event_p_id;
	uint8_t	nb_ports;
	rte_spinlock_t lock;
};

struct l3fwd_event_rx_adptr {
	uint32_t service_id;
	uint8_t	nb_rx_adptr;
	uint8_t *rx_adptr;
};

struct l3fwd_event_tx_adptr {
	uint32_t service_id;
	uint8_t	nb_tx_adptr;
	uint8_t *tx_adptr;
};

struct l3fwd_event_setup_ops {
	event_device_setup_cb event_device_setup;
	event_queue_setup_cb event_queue_setup;
	event_port_setup_cb event_port_setup;
	adapter_setup_cb adapter_setup;
	event_loop_cb lpm_event_loop;
	event_loop_cb em_event_loop;
};

struct l3fwd_event_resources {
	struct rte_event_port_conf def_p_conf;
	struct l3fwd_event_rx_adptr rx_adptr;
	struct l3fwd_event_tx_adptr tx_adptr;
	uint8_t disable_implicit_release;
	struct l3fwd_event_setup_ops ops;
	struct rte_mempool * (*pkt_pool)[NB_SOCKETS];
	struct l3fwd_event_queues evq;
	struct l3fwd_event_ports evp;
	uint32_t port_mask;
	uint8_t per_port_pool;
	uint8_t event_d_id;
	uint8_t sched_type;
	uint8_t tx_mode_q;
	uint8_t deq_depth;
	uint8_t has_burst;
	uint8_t enabled;
	uint8_t eth_rx_queues;
};

struct l3fwd_event_resources *l3fwd_get_eventdev_rsrc(void);
void l3fwd_event_resource_setup(struct rte_eth_conf *port_conf);
int l3fwd_get_free_event_port(struct l3fwd_event_resources *eventdev_rsrc);
void l3fwd_event_set_generic_ops(struct l3fwd_event_setup_ops *ops);
void l3fwd_event_set_internal_port_ops(struct l3fwd_event_setup_ops *ops);

#endif /* __L3FWD_EVENTDEV_H__ */
