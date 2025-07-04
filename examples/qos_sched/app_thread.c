/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdint.h>

#include <rte_log.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_memcpy.h>
#include <rte_byteorder.h>
#include <rte_branch_prediction.h>
#include <rte_sched.h>

#include "main.h"

/*
 * QoS parameters are encoded as follows:
 *		Outer VLAN ID defines subport
 *		Inner VLAN ID defines pipe
 *		Destination IP host (0.0.0.XXX) defines queue
 * Values below define offset to each field from start of frame
 */
#define SUBPORT_OFFSET	7
#define PIPE_OFFSET		9
#define QUEUE_OFFSET	20
#define COLOR_OFFSET	19

static inline int
get_pkt_sched(struct rte_mbuf *m, uint32_t *subport, uint32_t *pipe,
			uint32_t *traffic_class, uint32_t *queue, uint32_t *color)
{
	uint16_t *pdata = rte_pktmbuf_mtod(m, uint16_t *);
	uint16_t pipe_queue;

	/* Outer VLAN ID*/
	*subport = (rte_be_to_cpu_16(pdata[SUBPORT_OFFSET]) & 0x0FFF) &
		(port_params.n_subports_per_port - 1);

	/* Inner VLAN ID */
	*pipe = (rte_be_to_cpu_16(pdata[PIPE_OFFSET]) & 0x0FFF) &
		(subport_params[*subport].n_pipes_per_subport_enabled - 1);

	pipe_queue = active_queues[(pdata[QUEUE_OFFSET] >> 8) % n_active_queues];

	/* Traffic class (Destination IP) */
	*traffic_class = pipe_queue > RTE_SCHED_TRAFFIC_CLASS_BE ?
			RTE_SCHED_TRAFFIC_CLASS_BE : pipe_queue;

	/* Traffic class queue (Destination IP) */
	*queue = pipe_queue - *traffic_class;

	/* Color (Destination IP) */
	*color = pdata[COLOR_OFFSET] & 0x03;

	return 0;
}

void
app_rx_thread(struct thread_conf **confs)
{
	uint32_t i, nb_rx;
	alignas(RTE_CACHE_LINE_SIZE) struct rte_mbuf *rx_mbufs[burst_conf.rx_burst];
	struct thread_conf *conf;
	int conf_idx = 0;

	uint32_t subport;
	uint32_t pipe;
	uint32_t traffic_class;
	uint32_t queue;
	uint32_t color;

	while ((conf = confs[conf_idx])) {
		nb_rx = rte_eth_rx_burst(conf->rx_port, conf->rx_queue, rx_mbufs,
				burst_conf.rx_burst);

		if (likely(nb_rx != 0)) {
			APP_STATS_ADD(conf->stat.nb_rx, nb_rx);

			for(i = 0; i < nb_rx; i++) {
				get_pkt_sched(rx_mbufs[i],
						&subport, &pipe, &traffic_class, &queue, &color);
				rte_sched_port_pkt_write(conf->sched_port,
						rx_mbufs[i],
						subport, pipe,
						traffic_class, queue,
						(enum rte_color) color);
			}

			if (unlikely(rte_ring_sp_enqueue_bulk(conf->rx_ring,
					(void **)rx_mbufs, nb_rx, NULL) == 0)) {
				for(i = 0; i < nb_rx; i++) {
					rte_pktmbuf_free(rx_mbufs[i]);

					APP_STATS_ADD(conf->stat.nb_drop, 1);
				}
			}
		}
		conf_idx++;
		if (confs[conf_idx] == NULL)
			conf_idx = 0;
	}
}

void
app_tx_thread(struct thread_conf **confs)
{
	struct rte_mbuf *mbufs[burst_conf.qos_dequeue];
	struct thread_conf *conf;
	int conf_idx = 0;
	int nb_pkts;

	while ((conf = confs[conf_idx])) {
		nb_pkts = rte_ring_sc_dequeue_burst(conf->tx_ring, (void **)mbufs,
					burst_conf.qos_dequeue, NULL);
		if (likely(nb_pkts != 0)) {
			uint16_t nb_tx = rte_eth_tx_burst(conf->tx_port, 0, mbufs, nb_pkts);
			if (nb_pkts != nb_tx)
				rte_pktmbuf_free_bulk(&mbufs[nb_tx], nb_pkts - nb_tx);
		}

		conf_idx++;
		if (confs[conf_idx] == NULL)
			conf_idx = 0;
	}
}


void
app_worker_thread(struct thread_conf **confs)
{
	struct rte_mbuf *mbufs[burst_conf.ring_burst];
	struct thread_conf *conf;
	int conf_idx = 0;

	while ((conf = confs[conf_idx])) {
		uint32_t nb_pkt;

		/* Read packet from the ring */
		nb_pkt = rte_ring_sc_dequeue_burst(conf->rx_ring, (void **)mbufs,
					burst_conf.ring_burst, NULL);
		if (likely(nb_pkt)) {
			int nb_sent = rte_sched_port_enqueue(conf->sched_port, mbufs,
					nb_pkt);

			APP_STATS_ADD(conf->stat.nb_drop, nb_pkt - nb_sent);
			APP_STATS_ADD(conf->stat.nb_rx, nb_pkt);
		}

		nb_pkt = rte_sched_port_dequeue(conf->sched_port, mbufs,
					burst_conf.qos_dequeue);
		if (likely(nb_pkt > 0))
			while (rte_ring_sp_enqueue_bulk(conf->tx_ring,
					(void **)mbufs, nb_pkt, NULL) == 0)
				; /* empty body */

		conf_idx++;
		if (confs[conf_idx] == NULL)
			conf_idx = 0;
	}
}


void
app_mixed_thread(struct thread_conf **confs)
{
	struct rte_mbuf *mbufs[burst_conf.ring_burst];
	struct thread_conf *conf;
	int conf_idx = 0;

	while ((conf = confs[conf_idx])) {
		uint32_t nb_pkt;

		/* Read packet from the ring */
		nb_pkt = rte_ring_sc_dequeue_burst(conf->rx_ring, (void **)mbufs,
					burst_conf.ring_burst, NULL);
		if (likely(nb_pkt)) {
			int nb_sent = rte_sched_port_enqueue(conf->sched_port, mbufs,
					nb_pkt);

			APP_STATS_ADD(conf->stat.nb_drop, nb_pkt - nb_sent);
			APP_STATS_ADD(conf->stat.nb_rx, nb_pkt);
		}


		nb_pkt = rte_sched_port_dequeue(conf->sched_port, mbufs,
					burst_conf.qos_dequeue);
		if (likely(nb_pkt > 0)) {
			uint16_t nb_tx = rte_eth_tx_burst(conf->tx_port, 0, mbufs, nb_pkt);
			if (nb_tx != nb_pkt)
				rte_pktmbuf_free_bulk(&mbufs[nb_tx], nb_pkt - nb_tx);
		}

		conf_idx++;
		if (confs[conf_idx] == NULL)
			conf_idx = 0;
	}
}
