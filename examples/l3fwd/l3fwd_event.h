/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __L3FWD_EVENTDEV_H__
#define __L3FWD_EVENTDEV_H__

#include <rte_common.h>
#include <rte_service.h>
#include <rte_spinlock.h>

#ifdef RTE_LIB_EVENTDEV
#include <rte_eventdev.h>
#include <rte_event_eth_rx_adapter.h>
#include <rte_event_eth_tx_adapter.h>

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
	event_loop_cb fib_event_loop;
};

struct l3fwd_event_resources {
	struct rte_event_port_conf def_p_conf;
	struct l3fwd_event_rx_adptr rx_adptr;
	struct l3fwd_event_tx_adptr tx_adptr;
	uint8_t disable_implicit_release;
	struct l3fwd_event_setup_ops ops;
	struct rte_mempool * (*pkt_pool)[NB_SOCKETS];
	struct rte_mempool **vec_pool;
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
	uint8_t vector_enabled;
	uint16_t vector_size;
	uint64_t vector_tmo_ns;
};

#if defined(RTE_ARCH_X86)
#include "l3fwd_sse.h"
#elif defined __ARM_NEON
#include "l3fwd_neon.h"
#elif defined(RTE_ARCH_PPC_64)
#include "l3fwd_altivec.h"
#else
static inline uint16_t
process_dst_port(uint16_t *dst_ports, uint16_t nb_elem)
{
	int i;

	for (i = 0; i < nb_elem; i++) {
		if (dst_ports[i] != dst_ports[0])
			return BAD_PORT;
	}

	return dst_ports[0];
}
#endif

static inline uint16_t
filter_bad_packets(struct rte_mbuf **mbufs, uint16_t *dst_port,
		   uint16_t nb_pkts)
{
	uint16_t *des_pos, free = 0;
	struct rte_mbuf **pos;
	int i;

	/* Filter out and free bad packets */
	for (i = 0; i < nb_pkts; i++) {
		if (dst_port[i] == BAD_PORT) {
			rte_pktmbuf_free(mbufs[i]);
			if (!free) {
				pos = &mbufs[i];
				des_pos = &dst_port[i];
			}
			free++;
			continue;
		}

		if (free) {
			*pos = mbufs[i];
			pos++;
			*des_pos = dst_port[i];
			des_pos++;
		}
	}

	return nb_pkts - free;
}

static inline void
process_event_vector(struct rte_event_vector *vec, uint16_t *dst_port)
{
	uint16_t port, i;

	vec->nb_elem = filter_bad_packets(vec->mbufs, dst_port, vec->nb_elem);
	/* Verify destination array */
	port = process_dst_port(dst_port, vec->nb_elem);
	if (port == BAD_PORT) {
		vec->attr_valid = 0;
		for (i = 0; i < vec->nb_elem; i++) {
			vec->mbufs[i]->port = dst_port[i];
			rte_event_eth_tx_adapter_txq_set(vec->mbufs[i], 0);
		}
	} else {
		vec->attr_valid = 1;
		vec->port = port;
		vec->queue = 0;
	}
}

struct l3fwd_event_resources *l3fwd_get_eventdev_rsrc(void);
void l3fwd_event_resource_setup(struct rte_eth_conf *port_conf);
int l3fwd_get_free_event_port(struct l3fwd_event_resources *eventdev_rsrc);
void l3fwd_event_set_generic_ops(struct l3fwd_event_setup_ops *ops);
void l3fwd_event_set_internal_port_ops(struct l3fwd_event_setup_ops *ops);
void l3fwd_event_worker_cleanup(uint8_t event_d_id, uint8_t event_p_id,
				struct rte_event events[], uint16_t nb_enq,
				uint16_t nb_deq, uint8_t is_vector);

#endif /* RTE_LIB_EVENTDEV */
#endif /* __L3FWD_EVENTDEV_H__ */
