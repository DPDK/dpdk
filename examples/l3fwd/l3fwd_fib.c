/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <rte_fib.h>
#include <rte_fib6.h>

#include "l3fwd.h"
#if defined RTE_ARCH_X86
#include "l3fwd_sse.h"
#elif defined __ARM_NEON
#include "l3fwd_neon.h"
#elif defined RTE_ARCH_PPC_64
#include "l3fwd_altivec.h"
#else
#include "l3fwd_common.h"
#endif
#include "l3fwd_event.h"
#include "l3fwd_route.h"

/* Configure how many packets ahead to prefetch for fib. */
#define FIB_PREFETCH_OFFSET 4

/* A non-existent portid is needed to denote a default hop for fib. */
#define FIB_DEFAULT_HOP 999

/*
 * If the machine has SSE, NEON or PPC 64 then multiple packets
 * can be sent at once if not only single packets will be sent
 */
#if defined RTE_ARCH_X86 || defined __ARM_NEON \
		|| defined RTE_ARCH_PPC_64
#define FIB_SEND_MULTI
#endif

static struct rte_fib *ipv4_l3fwd_fib_lookup_struct[NB_SOCKETS];
static struct rte_fib6 *ipv6_l3fwd_fib_lookup_struct[NB_SOCKETS];

/* Parse packet type and ip address. */
static inline void
fib_parse_packet(struct rte_mbuf *mbuf,
		uint32_t *ipv4, uint32_t *ipv4_cnt,
		uint8_t ipv6[RTE_FIB6_IPV6_ADDR_SIZE],
		uint32_t *ipv6_cnt, uint8_t *ip_type)
{
	struct rte_ether_hdr *eth_hdr;
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_ipv6_hdr *ipv6_hdr;

	eth_hdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
	/* IPv4 */
	if (mbuf->packet_type & RTE_PTYPE_L3_IPV4) {
		ipv4_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
		*ipv4 = rte_be_to_cpu_32(ipv4_hdr->dst_addr);
		/* Store type of packet in type_arr (IPv4=1, IPv6=0). */
		*ip_type = 1;
		(*ipv4_cnt)++;
	}
	/* IPv6 */
	else {
		ipv6_hdr = (struct rte_ipv6_hdr *)(eth_hdr + 1);
		rte_mov16(ipv6, (const uint8_t *)ipv6_hdr->dst_addr);
		*ip_type = 0;
		(*ipv6_cnt)++;
	}
}

/*
 * If the machine does not have SSE, NEON or PPC 64 then the packets
 * are sent one at a time using send_single_packet()
 */
#if !defined FIB_SEND_MULTI
static inline void
process_packet(struct rte_mbuf *pkt, uint16_t *hop)
{
	struct rte_ether_hdr *eth_hdr;

	/* Run rfc1812 if packet is ipv4 and checks enabled. */
#if defined DO_RFC_1812_CHECKS
	rfc1812_process(
		(struct rte_ipv4_hdr *)(rte_pktmbuf_mtod(
						pkt, struct rte_ether_hdr *) +
					1),
		hop, pkt->packet_type);
#endif

	/* Set MAC addresses. */
	eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	*(uint64_t *)&eth_hdr->dst_addr = dest_eth_addr[*hop];
	rte_ether_addr_copy(&ports_eth_addr[*hop], &eth_hdr->src_addr);
}

static inline void
fib_send_single(int nb_tx, struct lcore_conf *qconf,
		struct rte_mbuf **pkts_burst, uint16_t hops[nb_tx])
{
	int32_t j;

	for (j = 0; j < nb_tx; j++) {
		process_packet(pkts_burst[j], &hops[j]);
		if (hops[j] == BAD_PORT) {
			rte_pktmbuf_free(pkts_burst[j]);
			continue;
		}
		/* Send single packet. */
		send_single_packet(qconf, pkts_burst[j], hops[j]);
	}
}
#endif

/* Bulk parse, fib lookup and send. */
static inline void
fib_send_packets(int nb_rx, struct rte_mbuf **pkts_burst,
		uint16_t portid, struct lcore_conf *qconf)
{
	uint32_t ipv4_arr[nb_rx];
	uint8_t ipv6_arr[nb_rx][RTE_FIB6_IPV6_ADDR_SIZE];
	uint16_t hops[nb_rx];
	uint64_t hopsv4[nb_rx], hopsv6[nb_rx];
	uint8_t type_arr[nb_rx];
	uint32_t ipv4_cnt = 0, ipv6_cnt = 0;
	uint32_t ipv4_arr_assem = 0, ipv6_arr_assem = 0;
	uint16_t nh;
	int32_t i;

	/* Prefetch first packets. */
	for (i = 0; i < FIB_PREFETCH_OFFSET && i < nb_rx; i++)
		rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[i], void *));

	/* Parse packet info and prefetch. */
	for (i = 0; i < (nb_rx - FIB_PREFETCH_OFFSET); i++) {
		/* Prefetch packet. */
		rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[
				i + FIB_PREFETCH_OFFSET], void *));
		fib_parse_packet(pkts_burst[i],
				&ipv4_arr[ipv4_cnt], &ipv4_cnt,
				ipv6_arr[ipv6_cnt], &ipv6_cnt,
				&type_arr[i]);
	}

	/* Parse remaining packet info. */
	for (; i < nb_rx; i++)
		fib_parse_packet(pkts_burst[i],
				&ipv4_arr[ipv4_cnt], &ipv4_cnt,
				ipv6_arr[ipv6_cnt], &ipv6_cnt,
				&type_arr[i]);

	/* Lookup IPv4 hops if IPv4 packets are present. */
	if (likely(ipv4_cnt > 0))
		rte_fib_lookup_bulk(qconf->ipv4_lookup_struct,
				ipv4_arr, hopsv4, ipv4_cnt);

	/* Lookup IPv6 hops if IPv6 packets are present. */
	if (ipv6_cnt > 0)
		rte_fib6_lookup_bulk(qconf->ipv6_lookup_struct,
				ipv6_arr, hopsv6, ipv6_cnt);

	/* Add IPv4 and IPv6 hops to one array depending on type. */
	for (i = 0; i < nb_rx; i++) {
		if (type_arr[i])
			nh = (uint16_t)hopsv4[ipv4_arr_assem++];
		else
			nh = (uint16_t)hopsv6[ipv6_arr_assem++];
		hops[i] = nh != FIB_DEFAULT_HOP ? nh : portid;
	}

#if defined FIB_SEND_MULTI
	send_packets_multi(qconf, pkts_burst, hops, nb_rx);
#else
	fib_send_single(nb_rx, qconf, pkts_burst, hops);
#endif
}

/* Main fib processing loop. */
int
fib_main_loop(__rte_unused void *dummy)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	unsigned int lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc;
	int i, nb_rx;
	uint16_t portid;
	uint8_t queueid;
	struct lcore_conf *qconf;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) /
			US_PER_S * BURST_TX_DRAIN_US;

	lcore_id = rte_lcore_id();
	qconf = &lcore_conf[lcore_id];

	const uint16_t n_rx_q = qconf->n_rx_queue;
	const uint16_t n_tx_p = qconf->n_tx_port;
	if (n_rx_q == 0) {
		RTE_LOG(INFO, L3FWD, "lcore %u has nothing to do\n", lcore_id);
		return 0;
	}

	RTE_LOG(INFO, L3FWD, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < n_rx_q; i++) {

		portid = qconf->rx_queue_list[i].port_id;
		queueid = qconf->rx_queue_list[i].queue_id;
		RTE_LOG(INFO, L3FWD,
				" -- lcoreid=%u portid=%u rxqueueid=%hhu\n",
				lcore_id, portid, queueid);
	}

	cur_tsc = rte_rdtsc();
	prev_tsc = cur_tsc;

	while (!force_quit) {

		/* TX burst queue drain. */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {

			for (i = 0; i < n_tx_p; ++i) {
				portid = qconf->tx_port_id[i];
				if (qconf->tx_mbufs[portid].len == 0)
					continue;
				send_burst(qconf,
					qconf->tx_mbufs[portid].len,
					portid);
				qconf->tx_mbufs[portid].len = 0;
			}

			prev_tsc = cur_tsc;
		}

		/* Read packet from RX queues. */
		for (i = 0; i < n_rx_q; ++i) {
			portid = qconf->rx_queue_list[i].port_id;
			queueid = qconf->rx_queue_list[i].queue_id;
			nb_rx = rte_eth_rx_burst(portid, queueid, pkts_burst,
					MAX_PKT_BURST);
			if (nb_rx == 0)
				continue;

			/* Use fib to lookup port IDs and transmit them. */
			fib_send_packets(nb_rx, pkts_burst,	portid, qconf);
		}

		cur_tsc = rte_rdtsc();
	}

	return 0;
}

#ifdef RTE_LIB_EVENTDEV
/* One eventdev loop for single and burst using fib. */
static __rte_always_inline void
fib_event_loop(struct l3fwd_event_resources *evt_rsrc,
		const uint8_t flags)
{
	const int event_p_id = l3fwd_get_free_event_port(evt_rsrc);
	const uint8_t tx_q_id = evt_rsrc->evq.event_q_id[
			evt_rsrc->evq.nb_queues - 1];
	const uint8_t event_d_id = evt_rsrc->event_d_id;
	const uint16_t deq_len = evt_rsrc->deq_depth;
	struct rte_event events[MAX_PKT_BURST];
	int i, nb_enq = 0, nb_deq = 0;
	struct lcore_conf *lconf;
	unsigned int lcore_id;

	uint32_t ipv4_arr[MAX_PKT_BURST];
	uint8_t ipv6_arr[MAX_PKT_BURST][RTE_FIB6_IPV6_ADDR_SIZE];
	uint64_t hopsv4[MAX_PKT_BURST], hopsv6[MAX_PKT_BURST];
	uint16_t nh, hops[MAX_PKT_BURST];
	uint8_t type_arr[MAX_PKT_BURST];
	uint32_t ipv4_cnt, ipv6_cnt;
	uint32_t ipv4_arr_assem, ipv6_arr_assem;

	if (event_p_id < 0)
		return;

	lcore_id = rte_lcore_id();

	lconf = &lcore_conf[lcore_id];

	RTE_LOG(INFO, L3FWD, "entering %s on lcore %u\n", __func__, lcore_id);

	while (!force_quit) {
		/* Read events from RX queues. */
		nb_deq = rte_event_dequeue_burst(event_d_id, event_p_id,
				events, deq_len, 0);
		if (nb_deq == 0) {
			rte_pause();
			continue;
		}

		/* Reset counters. */
		ipv4_cnt = 0;
		ipv6_cnt = 0;
		ipv4_arr_assem = 0;
		ipv6_arr_assem = 0;

		/* Prefetch first packets. */
		for (i = 0; i < FIB_PREFETCH_OFFSET && i < nb_deq; i++)
			rte_prefetch0(rte_pktmbuf_mtod(events[i].mbuf, void *));

		/* Parse packet info and prefetch. */
		for (i = 0; i < (nb_deq - FIB_PREFETCH_OFFSET); i++) {
			if (flags & L3FWD_EVENT_TX_ENQ) {
				events[i].queue_id = tx_q_id;
				events[i].op = RTE_EVENT_OP_FORWARD;
			}

			if (flags & L3FWD_EVENT_TX_DIRECT)
				rte_event_eth_tx_adapter_txq_set(events[i].mbuf,
						0);

			/* Prefetch packet. */
			rte_prefetch0(rte_pktmbuf_mtod(events[
					i + FIB_PREFETCH_OFFSET].mbuf,
					void *));

			fib_parse_packet(events[i].mbuf,
					&ipv4_arr[ipv4_cnt], &ipv4_cnt,
					ipv6_arr[ipv6_cnt], &ipv6_cnt,
					&type_arr[i]);
		}

		/* Parse remaining packet info. */
		for (; i < nb_deq; i++) {
			if (flags & L3FWD_EVENT_TX_ENQ) {
				events[i].queue_id = tx_q_id;
				events[i].op = RTE_EVENT_OP_FORWARD;
			}

			if (flags & L3FWD_EVENT_TX_DIRECT)
				rte_event_eth_tx_adapter_txq_set(events[i].mbuf,
						0);

			fib_parse_packet(events[i].mbuf,
					&ipv4_arr[ipv4_cnt], &ipv4_cnt,
					ipv6_arr[ipv6_cnt], &ipv6_cnt,
					&type_arr[i]);
		}

		/* Lookup IPv4 hops if IPv4 packets are present. */
		if (likely(ipv4_cnt > 0))
			rte_fib_lookup_bulk(lconf->ipv4_lookup_struct,
					ipv4_arr, hopsv4, ipv4_cnt);

		/* Lookup IPv6 hops if IPv6 packets are present. */
		if (ipv6_cnt > 0)
			rte_fib6_lookup_bulk(lconf->ipv6_lookup_struct,
					ipv6_arr, hopsv6, ipv6_cnt);

		/* Assign ports looked up in fib depending on IPv4 or IPv6 */
		for (i = 0; i < nb_deq; i++) {
			if (type_arr[i])
				nh = (uint16_t)hopsv4[ipv4_arr_assem++];
			else
				nh = (uint16_t)hopsv6[ipv6_arr_assem++];

			hops[i] = nh != FIB_DEFAULT_HOP ?
				  nh :
				  events[i].mbuf->port;
			process_packet(events[i].mbuf, &hops[i]);
			events[i].mbuf->port = hops[i] != BAD_PORT ?
						       hops[i] :
						       events[i].mbuf->port;
		}

		if (flags & L3FWD_EVENT_TX_ENQ) {
			nb_enq = rte_event_enqueue_burst(event_d_id, event_p_id,
					events, nb_deq);
			while (nb_enq < nb_deq && !force_quit)
				nb_enq += rte_event_enqueue_burst(event_d_id,
						event_p_id, events + nb_enq,
						nb_deq - nb_enq);
		}

		if (flags & L3FWD_EVENT_TX_DIRECT) {
			nb_enq = rte_event_eth_tx_adapter_enqueue(event_d_id,
					event_p_id, events, nb_deq, 0);
			while (nb_enq < nb_deq && !force_quit)
				nb_enq += rte_event_eth_tx_adapter_enqueue(
						event_d_id, event_p_id,
						events + nb_enq,
						nb_deq - nb_enq, 0);
		}
	}

	l3fwd_event_worker_cleanup(event_d_id, event_p_id, events, nb_enq,
				   nb_deq, 0);
}

int __rte_noinline
fib_event_main_loop_tx_d(__rte_unused void *dummy)
{
	struct l3fwd_event_resources *evt_rsrc =
			l3fwd_get_eventdev_rsrc();

	fib_event_loop(evt_rsrc, L3FWD_EVENT_TX_DIRECT);
	return 0;
}

int __rte_noinline
fib_event_main_loop_tx_d_burst(__rte_unused void *dummy)
{
	struct l3fwd_event_resources *evt_rsrc =
			l3fwd_get_eventdev_rsrc();

	fib_event_loop(evt_rsrc, L3FWD_EVENT_TX_DIRECT);
	return 0;
}

int __rte_noinline
fib_event_main_loop_tx_q(__rte_unused void *dummy)
{
	struct l3fwd_event_resources *evt_rsrc =
			l3fwd_get_eventdev_rsrc();

	fib_event_loop(evt_rsrc, L3FWD_EVENT_TX_ENQ);
	return 0;
}

int __rte_noinline
fib_event_main_loop_tx_q_burst(__rte_unused void *dummy)
{
	struct l3fwd_event_resources *evt_rsrc =
			l3fwd_get_eventdev_rsrc();

	fib_event_loop(evt_rsrc, L3FWD_EVENT_TX_ENQ);
	return 0;
}

static __rte_always_inline void
fib_process_event_vector(struct rte_event_vector *vec, uint8_t *type_arr,
			 uint8_t **ipv6_arr, uint64_t *hopsv4, uint64_t *hopsv6,
			 uint32_t *ipv4_arr, uint16_t *hops)
{
	uint32_t ipv4_arr_assem, ipv6_arr_assem;
	struct rte_mbuf **mbufs = vec->mbufs;
	uint32_t ipv4_cnt, ipv6_cnt;
	struct lcore_conf *lconf;
	uint16_t nh;
	int i;

	lconf = &lcore_conf[rte_lcore_id()];

	/* Reset counters. */
	ipv4_cnt = 0;
	ipv6_cnt = 0;
	ipv4_arr_assem = 0;
	ipv6_arr_assem = 0;

	/* Prefetch first packets. */
	for (i = 0; i < FIB_PREFETCH_OFFSET && i < vec->nb_elem; i++)
		rte_prefetch0(rte_pktmbuf_mtod(mbufs[i], void *));

	/* Parse packet info and prefetch. */
	for (i = 0; i < (vec->nb_elem - FIB_PREFETCH_OFFSET); i++) {
		rte_prefetch0(rte_pktmbuf_mtod(mbufs[i + FIB_PREFETCH_OFFSET],
					       void *));
		fib_parse_packet(mbufs[i], &ipv4_arr[ipv4_cnt], &ipv4_cnt,
				 ipv6_arr[ipv6_cnt], &ipv6_cnt, &type_arr[i]);
	}

	/* Parse remaining packet info. */
	for (; i < vec->nb_elem; i++)
		fib_parse_packet(mbufs[i], &ipv4_arr[ipv4_cnt], &ipv4_cnt,
				 ipv6_arr[ipv6_cnt], &ipv6_cnt, &type_arr[i]);

	/* Lookup IPv4 hops if IPv4 packets are present. */
	if (likely(ipv4_cnt > 0))
		rte_fib_lookup_bulk(lconf->ipv4_lookup_struct, ipv4_arr, hopsv4,
				    ipv4_cnt);

	/* Lookup IPv6 hops if IPv6 packets are present. */
	if (ipv6_cnt > 0)
		rte_fib6_lookup_bulk(
			lconf->ipv6_lookup_struct,
			(uint8_t(*)[RTE_FIB6_IPV6_ADDR_SIZE])ipv6_arr, hopsv6,
			ipv6_cnt);

	/* Assign ports looked up in fib depending on IPv4 or IPv6 */
	for (i = 0; i < vec->nb_elem; i++) {
		if (type_arr[i])
			nh = (uint16_t)hopsv4[ipv4_arr_assem++];
		else
			nh = (uint16_t)hopsv6[ipv6_arr_assem++];
		if (nh != FIB_DEFAULT_HOP)
			hops[i] = nh;
		else
			hops[i] = vec->attr_valid ? vec->port :
						    vec->mbufs[i]->port;
	}

#if defined FIB_SEND_MULTI
	uint16_t k;
	k = RTE_ALIGN_FLOOR(vec->nb_elem, FWDSTEP);

	for (i = 0; i != k; i += FWDSTEP)
		processx4_step3(&vec->mbufs[i], &hops[i]);
	for (; i < vec->nb_elem; i++)
		process_packet(vec->mbufs[i], &hops[i]);
#else
	for (i = 0; i < vec->nb_elem; i++)
		process_packet(vec->mbufs[i], &hops[i]);
#endif

	process_event_vector(vec, hops);
}

static __rte_always_inline void
fib_event_loop_vector(struct l3fwd_event_resources *evt_rsrc,
		      const uint8_t flags)
{
	const int event_p_id = l3fwd_get_free_event_port(evt_rsrc);
	const uint8_t tx_q_id =
		evt_rsrc->evq.event_q_id[evt_rsrc->evq.nb_queues - 1];
	const uint8_t event_d_id = evt_rsrc->event_d_id;
	const uint16_t deq_len = evt_rsrc->deq_depth;
	struct rte_event events[MAX_PKT_BURST];
	uint8_t *type_arr, **ipv6_arr, *ptr;
	int nb_enq = 0, nb_deq = 0, i;
	uint64_t *hopsv4, *hopsv6;
	uint32_t *ipv4_arr;
	uint16_t *hops;
	uintptr_t mem;

	mem = (uintptr_t)rte_zmalloc(
		"vector_fib",
		(sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint64_t) +
		 sizeof(uint64_t) + sizeof(uint16_t) + sizeof(uint8_t *) +
		 (sizeof(uint8_t) * RTE_FIB6_IPV6_ADDR_SIZE)) *
			evt_rsrc->vector_size,
		RTE_CACHE_LINE_SIZE);
	if (mem == 0)
		return;
	ipv4_arr = (uint32_t *)mem;
	type_arr = (uint8_t *)&ipv4_arr[evt_rsrc->vector_size];
	hopsv4 = (uint64_t *)&type_arr[evt_rsrc->vector_size];
	hopsv6 = (uint64_t *)&hopsv4[evt_rsrc->vector_size];
	hops = (uint16_t *)&hopsv6[evt_rsrc->vector_size];
	ipv6_arr = (uint8_t **)&hops[evt_rsrc->vector_size];

	ptr = (uint8_t *)&ipv6_arr[evt_rsrc->vector_size];
	for (i = 0; i < evt_rsrc->vector_size; i++)
		ipv6_arr[i] = &ptr[RTE_FIB6_IPV6_ADDR_SIZE + i];

	if (event_p_id < 0) {
		rte_free((void *)mem);
		return;
	}

	RTE_LOG(INFO, L3FWD, "entering %s on lcore %u\n", __func__,
		rte_lcore_id());

	while (!force_quit) {
		/* Read events from RX queues. */
		nb_deq = rte_event_dequeue_burst(event_d_id, event_p_id, events,
						 deq_len, 0);
		if (nb_deq == 0) {
			rte_pause();
			continue;
		}

		for (i = 0; i < nb_deq; i++) {
			if (flags & L3FWD_EVENT_TX_ENQ) {
				events[i].queue_id = tx_q_id;
				events[i].op = RTE_EVENT_OP_FORWARD;
			}

			fib_process_event_vector(events[i].vec, type_arr,
						 ipv6_arr, hopsv4, hopsv6,
						 ipv4_arr, hops);
		}

		if (flags & L3FWD_EVENT_TX_ENQ) {
			nb_enq = rte_event_enqueue_burst(event_d_id, event_p_id,
							 events, nb_deq);
			while (nb_enq < nb_deq && !force_quit)
				nb_enq += rte_event_enqueue_burst(
					event_d_id, event_p_id, events + nb_enq,
					nb_deq - nb_enq);
		}

		if (flags & L3FWD_EVENT_TX_DIRECT) {
			nb_enq = rte_event_eth_tx_adapter_enqueue(
				event_d_id, event_p_id, events, nb_deq, 0);
			while (nb_enq < nb_deq && !force_quit)
				nb_enq += rte_event_eth_tx_adapter_enqueue(
					event_d_id, event_p_id, events + nb_enq,
					nb_deq - nb_enq, 0);
		}
	}

	l3fwd_event_worker_cleanup(event_d_id, event_p_id, events, nb_enq,
				   nb_deq, 1);
	rte_free((void *)mem);
}

int __rte_noinline
fib_event_main_loop_tx_d_vector(__rte_unused void *dummy)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();

	fib_event_loop_vector(evt_rsrc, L3FWD_EVENT_TX_DIRECT);
	return 0;
}

int __rte_noinline
fib_event_main_loop_tx_d_burst_vector(__rte_unused void *dummy)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();

	fib_event_loop_vector(evt_rsrc, L3FWD_EVENT_TX_DIRECT);
	return 0;
}

int __rte_noinline
fib_event_main_loop_tx_q_vector(__rte_unused void *dummy)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();

	fib_event_loop_vector(evt_rsrc, L3FWD_EVENT_TX_ENQ);
	return 0;
}

int __rte_noinline
fib_event_main_loop_tx_q_burst_vector(__rte_unused void *dummy)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();

	fib_event_loop_vector(evt_rsrc, L3FWD_EVENT_TX_ENQ);
	return 0;
}
#endif

/* Function to setup fib. 8< */
void
setup_fib(const int socketid)
{
	struct rte_eth_dev_info dev_info;
	struct rte_fib6_conf config;
	struct rte_fib_conf config_ipv4;
	int i;
	int ret;
	char s[64];
	char abuf[INET6_ADDRSTRLEN];

	/* Create the fib IPv4 table. */
	config_ipv4.type = RTE_FIB_DIR24_8;
	config_ipv4.max_routes = (1 << 16);
	config_ipv4.rib_ext_sz = 0;
	config_ipv4.default_nh = FIB_DEFAULT_HOP;
	config_ipv4.dir24_8.nh_sz = RTE_FIB_DIR24_8_4B;
	config_ipv4.dir24_8.num_tbl8 = (1 << 15);
	snprintf(s, sizeof(s), "IPV4_L3FWD_FIB_%d", socketid);
	ipv4_l3fwd_fib_lookup_struct[socketid] =
			rte_fib_create(s, socketid, &config_ipv4);
	if (ipv4_l3fwd_fib_lookup_struct[socketid] == NULL)
		rte_exit(EXIT_FAILURE,
			"Unable to create the l3fwd FIB table on socket %d\n",
			socketid);


	/* Populate the fib ipv4 table. */
	for (i = 0; i < route_num_v4; i++) {
		struct in_addr in;

		/* Skip unused ports. */
		if ((1 << route_base_v4[i].if_out &
				enabled_port_mask) == 0)
			continue;

		rte_eth_dev_info_get(route_base_v4[i].if_out,
				     &dev_info);
		ret = rte_fib_add(ipv4_l3fwd_fib_lookup_struct[socketid],
			route_base_v4[i].ip,
			route_base_v4[i].depth,
			route_base_v4[i].if_out);

		if (ret < 0) {
			free(route_base_v4);
			rte_exit(EXIT_FAILURE,
					"Unable to add entry %u to the l3fwd FIB table on socket %d\n",
					i, socketid);
		}

		in.s_addr = htonl(route_base_v4[i].ip);
		if (inet_ntop(AF_INET, &in, abuf, sizeof(abuf)) != NULL) {
			printf("FIB: Adding route %s / %d (%d) [%s]\n", abuf,
			       route_base_v4[i].depth,
			       route_base_v4[i].if_out,
			       rte_dev_name(dev_info.device));
		} else {
			printf("FIB: IPv4 route added to port %d [%s]\n",
			       route_base_v4[i].if_out,
			       rte_dev_name(dev_info.device));
		}
	}
	/* >8 End of setup fib. */

	/* Create the fib IPv6 table. */
	snprintf(s, sizeof(s), "IPV6_L3FWD_FIB_%d", socketid);

	config.type = RTE_FIB6_TRIE;
	config.max_routes = (1 << 16) - 1;
	config.rib_ext_sz = 0;
	config.default_nh = FIB_DEFAULT_HOP;
	config.trie.nh_sz = RTE_FIB6_TRIE_4B;
	config.trie.num_tbl8 = (1 << 15);
	ipv6_l3fwd_fib_lookup_struct[socketid] = rte_fib6_create(s, socketid,
			&config);
	if (ipv6_l3fwd_fib_lookup_struct[socketid] == NULL) {
		free(route_base_v4);
		rte_exit(EXIT_FAILURE,
				"Unable to create the l3fwd FIB table on socket %d\n",
				socketid);
	}

	/* Populate the fib IPv6 table. */
	for (i = 0; i < route_num_v6; i++) {

		/* Skip unused ports. */
		if ((1 << route_base_v6[i].if_out &
				enabled_port_mask) == 0)
			continue;

		rte_eth_dev_info_get(route_base_v6[i].if_out,
				     &dev_info);
		ret = rte_fib6_add(ipv6_l3fwd_fib_lookup_struct[socketid],
			route_base_v6[i].ip_8,
			route_base_v6[i].depth,
			route_base_v6[i].if_out);

		if (ret < 0) {
			free(route_base_v4);
			free(route_base_v6);
			rte_exit(EXIT_FAILURE,
					"Unable to add entry %u to the l3fwd FIB table on socket %d\n",
					i, socketid);
		}

		if (inet_ntop(AF_INET6, route_base_v6[i].ip_8,
				abuf, sizeof(abuf)) != NULL) {
			printf("FIB: Adding route %s / %d (%d) [%s]\n", abuf,
			       route_base_v6[i].depth,
			       route_base_v6[i].if_out,
			       rte_dev_name(dev_info.device));
		} else {
			printf("FIB: IPv6 route added to port %d [%s]\n",
			       route_base_v6[i].if_out,
			       rte_dev_name(dev_info.device));
		}
	}
}

/* Return ipv4 fib lookup struct. */
void *
fib_get_ipv4_l3fwd_lookup_struct(const int socketid)
{
	return ipv4_l3fwd_fib_lookup_struct[socketid];
}

/* Return ipv6 fib lookup struct. */
void *
fib_get_ipv6_l3fwd_lookup_struct(const int socketid)
{
	return ipv6_l3fwd_fib_lookup_struct[socketid];
}
