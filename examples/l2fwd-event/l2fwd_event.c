/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <stdbool.h>
#include <getopt.h>

#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_eventdev.h>
#include <rte_event_eth_rx_adapter.h>
#include <rte_event_eth_tx_adapter.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_spinlock.h>

#include "l2fwd_event.h"

#define L2FWD_EVENT_SINGLE	0x1
#define L2FWD_EVENT_BURST	0x2
#define L2FWD_EVENT_TX_DIRECT	0x4
#define L2FWD_EVENT_TX_ENQ	0x8
#define L2FWD_EVENT_UPDT_MAC	0x10

static inline int
l2fwd_event_service_enable(uint32_t service_id)
{
	uint8_t min_service_count = UINT8_MAX;
	uint32_t slcore_array[RTE_MAX_LCORE];
	unsigned int slcore = 0;
	uint8_t service_count;
	int32_t slcore_count;

	if (!rte_service_lcore_count())
		return -ENOENT;

	slcore_count = rte_service_lcore_list(slcore_array, RTE_MAX_LCORE);
	if (slcore_count < 0)
		return -ENOENT;
	/* Get the core which has least number of services running. */
	while (slcore_count--) {
		/* Reset default mapping */
		if (rte_service_map_lcore_set(service_id,
					slcore_array[slcore_count], 0) != 0)
			return -ENOENT;
		service_count = rte_service_lcore_count_services(
				slcore_array[slcore_count]);
		if (service_count < min_service_count) {
			slcore = slcore_array[slcore_count];
			min_service_count = service_count;
		}
	}
	if (rte_service_map_lcore_set(service_id, slcore, 1) != 0)
		return -ENOENT;
	rte_service_lcore_start(slcore);

	return 0;
}

void
l2fwd_event_service_setup(struct l2fwd_resources *rsrc)
{
	struct l2fwd_event_resources *evt_rsrc = rsrc->evt_rsrc;
	struct rte_event_dev_info evdev_info;
	uint32_t service_id, caps;
	int ret, i;

	rte_event_dev_info_get(evt_rsrc->event_d_id, &evdev_info);
	if (!(evdev_info.event_dev_cap & RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED)) {
		ret = rte_event_dev_service_id_get(evt_rsrc->event_d_id,
				&service_id);
		if (ret != -ESRCH && ret != 0)
			rte_panic("Error in starting eventdev service\n");
		l2fwd_event_service_enable(service_id);
	}

	for (i = 0; i < evt_rsrc->rx_adptr.nb_rx_adptr; i++) {
		ret = rte_event_eth_rx_adapter_caps_get(evt_rsrc->event_d_id,
				evt_rsrc->rx_adptr.rx_adptr[i], &caps);
		if (ret < 0)
			rte_panic("Failed to get Rx adapter[%d] caps\n",
				  evt_rsrc->rx_adptr.rx_adptr[i]);
		ret = rte_event_eth_rx_adapter_service_id_get(
				evt_rsrc->event_d_id,
				&service_id);
		if (ret != -ESRCH && ret != 0)
			rte_panic("Error in starting Rx adapter[%d] service\n",
				  evt_rsrc->rx_adptr.rx_adptr[i]);
		l2fwd_event_service_enable(service_id);
	}

	for (i = 0; i < evt_rsrc->tx_adptr.nb_tx_adptr; i++) {
		ret = rte_event_eth_tx_adapter_caps_get(evt_rsrc->event_d_id,
				evt_rsrc->tx_adptr.tx_adptr[i], &caps);
		if (ret < 0)
			rte_panic("Failed to get Rx adapter[%d] caps\n",
				  evt_rsrc->tx_adptr.tx_adptr[i]);
		ret = rte_event_eth_tx_adapter_service_id_get(
				evt_rsrc->event_d_id,
				&service_id);
		if (ret != -ESRCH && ret != 0)
			rte_panic("Error in starting Rx adapter[%d] service\n",
				  evt_rsrc->tx_adptr.tx_adptr[i]);
		l2fwd_event_service_enable(service_id);
	}
}

static void
l2fwd_event_capability_setup(struct l2fwd_event_resources *evt_rsrc)
{
	uint32_t caps = 0;
	uint16_t i;
	int ret;

	RTE_ETH_FOREACH_DEV(i) {
		ret = rte_event_eth_tx_adapter_caps_get(0, i, &caps);
		if (ret)
			rte_panic("Invalid capability for Tx adptr port %d\n",
				  i);

		evt_rsrc->tx_mode_q |= !(caps &
				   RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT);
	}

	if (evt_rsrc->tx_mode_q)
		l2fwd_event_set_generic_ops(&evt_rsrc->ops);
	else
		l2fwd_event_set_internal_port_ops(&evt_rsrc->ops);
}

static __rte_noinline int
l2fwd_get_free_event_port(struct l2fwd_event_resources *evt_rsrc)
{
	static int index;
	int port_id;

	rte_spinlock_lock(&evt_rsrc->evp.lock);
	if (index >= evt_rsrc->evp.nb_ports) {
		printf("No free event port is available\n");
		return -1;
	}

	port_id = evt_rsrc->evp.event_p_id[index];
	index++;
	rte_spinlock_unlock(&evt_rsrc->evp.lock);

	return port_id;
}

static  __rte_always_inline void
l2fwd_event_fwd(struct l2fwd_resources *rsrc, struct rte_event *ev,
		const uint8_t tx_q_id, const uint64_t timer_period,
		const uint32_t flags)
{
	struct rte_mbuf *mbuf = ev->mbuf;
	uint16_t dst_port;

	rte_prefetch0(rte_pktmbuf_mtod(mbuf, void *));
	dst_port = rsrc->dst_ports[mbuf->port];

	if (timer_period > 0)
		__atomic_fetch_add(&rsrc->port_stats[mbuf->port].rx,
				1, __ATOMIC_RELAXED);
	mbuf->port = dst_port;

	if (flags & L2FWD_EVENT_UPDT_MAC)
		l2fwd_mac_updating(mbuf, dst_port, &rsrc->eth_addr[dst_port]);

	if (flags & L2FWD_EVENT_TX_ENQ) {
		ev->queue_id = tx_q_id;
		ev->op = RTE_EVENT_OP_FORWARD;
	}

	if (flags & L2FWD_EVENT_TX_DIRECT)
		rte_event_eth_tx_adapter_txq_set(mbuf, 0);

	if (timer_period > 0)
		__atomic_fetch_add(&rsrc->port_stats[mbuf->port].tx,
				1, __ATOMIC_RELAXED);
}

static __rte_always_inline void
l2fwd_event_loop_single(struct l2fwd_resources *rsrc,
			const uint32_t flags)
{
	struct l2fwd_event_resources *evt_rsrc = rsrc->evt_rsrc;
	const int port_id = l2fwd_get_free_event_port(evt_rsrc);
	const uint8_t tx_q_id = evt_rsrc->evq.event_q_id[
					evt_rsrc->evq.nb_queues - 1];
	const uint64_t timer_period = rsrc->timer_period;
	const uint8_t event_d_id = evt_rsrc->event_d_id;
	struct rte_event ev;

	if (port_id < 0)
		return;

	printf("%s(): entering eventdev main loop on lcore %u\n", __func__,
		rte_lcore_id());

	while (!rsrc->force_quit) {
		/* Read packet from eventdev */
		if (!rte_event_dequeue_burst(event_d_id, port_id, &ev, 1, 0))
			continue;

		l2fwd_event_fwd(rsrc, &ev, tx_q_id, timer_period, flags);

		if (flags & L2FWD_EVENT_TX_ENQ) {
			while (rte_event_enqueue_burst(event_d_id, port_id,
						       &ev, 1) &&
					!rsrc->force_quit)
				;
		}

		if (flags & L2FWD_EVENT_TX_DIRECT) {
			while (!rte_event_eth_tx_adapter_enqueue(event_d_id,
								port_id,
								&ev, 1, 0) &&
					!rsrc->force_quit)
				;
		}
	}
}

static __rte_always_inline void
l2fwd_event_loop_burst(struct l2fwd_resources *rsrc,
		       const uint32_t flags)
{
	struct l2fwd_event_resources *evt_rsrc = rsrc->evt_rsrc;
	const int port_id = l2fwd_get_free_event_port(evt_rsrc);
	const uint8_t tx_q_id = evt_rsrc->evq.event_q_id[
					evt_rsrc->evq.nb_queues - 1];
	const uint64_t timer_period = rsrc->timer_period;
	const uint8_t event_d_id = evt_rsrc->event_d_id;
	const uint8_t deq_len = evt_rsrc->deq_depth;
	struct rte_event ev[MAX_PKT_BURST];
	uint16_t nb_rx, nb_tx;
	uint8_t i;

	if (port_id < 0)
		return;

	printf("%s(): entering eventdev main loop on lcore %u\n", __func__,
		rte_lcore_id());

	while (!rsrc->force_quit) {
		/* Read packet from eventdev */
		nb_rx = rte_event_dequeue_burst(event_d_id, port_id, ev,
						deq_len, 0);
		if (nb_rx == 0)
			continue;

		for (i = 0; i < nb_rx; i++) {
			l2fwd_event_fwd(rsrc, &ev[i], tx_q_id, timer_period,
					flags);
		}

		if (flags & L2FWD_EVENT_TX_ENQ) {
			nb_tx = rte_event_enqueue_burst(event_d_id, port_id,
							ev, nb_rx);
			while (nb_tx < nb_rx && !rsrc->force_quit)
				nb_tx += rte_event_enqueue_burst(event_d_id,
						port_id, ev + nb_tx,
						nb_rx - nb_tx);
		}

		if (flags & L2FWD_EVENT_TX_DIRECT) {
			nb_tx = rte_event_eth_tx_adapter_enqueue(event_d_id,
								 port_id, ev,
								 nb_rx, 0);
			while (nb_tx < nb_rx && !rsrc->force_quit)
				nb_tx += rte_event_eth_tx_adapter_enqueue(
						event_d_id, port_id,
						ev + nb_tx, nb_rx - nb_tx, 0);
		}
	}
}

static __rte_always_inline void
l2fwd_event_loop(struct l2fwd_resources *rsrc,
			const uint32_t flags)
{
	if (flags & L2FWD_EVENT_SINGLE)
		l2fwd_event_loop_single(rsrc, flags);
	if (flags & L2FWD_EVENT_BURST)
		l2fwd_event_loop_burst(rsrc, flags);
}

static void __rte_noinline
l2fwd_event_main_loop_tx_d(struct l2fwd_resources *rsrc)
{
	l2fwd_event_loop(rsrc,
			 L2FWD_EVENT_TX_DIRECT | L2FWD_EVENT_SINGLE);
}

static void __rte_noinline
l2fwd_event_main_loop_tx_d_brst(struct l2fwd_resources *rsrc)
{
	l2fwd_event_loop(rsrc, L2FWD_EVENT_TX_DIRECT | L2FWD_EVENT_BURST);
}

static void __rte_noinline
l2fwd_event_main_loop_tx_q(struct l2fwd_resources *rsrc)
{
	l2fwd_event_loop(rsrc, L2FWD_EVENT_TX_ENQ | L2FWD_EVENT_SINGLE);
}

static void __rte_noinline
l2fwd_event_main_loop_tx_q_brst(struct l2fwd_resources *rsrc)
{
	l2fwd_event_loop(rsrc, L2FWD_EVENT_TX_ENQ | L2FWD_EVENT_BURST);
}

static void __rte_noinline
l2fwd_event_main_loop_tx_d_mac(struct l2fwd_resources *rsrc)
{
	l2fwd_event_loop(rsrc, L2FWD_EVENT_UPDT_MAC |
			L2FWD_EVENT_TX_DIRECT | L2FWD_EVENT_SINGLE);
}

static void __rte_noinline
l2fwd_event_main_loop_tx_d_brst_mac(struct l2fwd_resources *rsrc)
{
	l2fwd_event_loop(rsrc, L2FWD_EVENT_UPDT_MAC |
			L2FWD_EVENT_TX_DIRECT | L2FWD_EVENT_BURST);
}

static void __rte_noinline
l2fwd_event_main_loop_tx_q_mac(struct l2fwd_resources *rsrc)
{
	l2fwd_event_loop(rsrc, L2FWD_EVENT_UPDT_MAC |
			L2FWD_EVENT_TX_ENQ | L2FWD_EVENT_SINGLE);
}

static void __rte_noinline
l2fwd_event_main_loop_tx_q_brst_mac(struct l2fwd_resources *rsrc)
{
	l2fwd_event_loop(rsrc, L2FWD_EVENT_UPDT_MAC |
			L2FWD_EVENT_TX_ENQ | L2FWD_EVENT_BURST);
}

void
l2fwd_event_resource_setup(struct l2fwd_resources *rsrc)
{
	/* [MAC_UPDT][TX_MODE][BURST] */
	const event_loop_cb event_loop[2][2][2] = {
		[0][0][0] = l2fwd_event_main_loop_tx_d,
		[0][0][1] = l2fwd_event_main_loop_tx_d_brst,
		[0][1][0] = l2fwd_event_main_loop_tx_q,
		[0][1][1] = l2fwd_event_main_loop_tx_q_brst,
		[1][0][0] = l2fwd_event_main_loop_tx_d_mac,
		[1][0][1] = l2fwd_event_main_loop_tx_d_brst_mac,
		[1][1][0] = l2fwd_event_main_loop_tx_q_mac,
		[1][1][1] = l2fwd_event_main_loop_tx_q_brst_mac,
	};
	struct l2fwd_event_resources *evt_rsrc;
	uint32_t event_queue_cfg;
	int ret;

	if (!rte_event_dev_count())
		rte_panic("No Eventdev found\n");

	evt_rsrc = rte_zmalloc("l2fwd_event",
				 sizeof(struct l2fwd_event_resources), 0);
	if (evt_rsrc == NULL)
		rte_panic("Failed to allocate memory\n");

	rsrc->evt_rsrc = evt_rsrc;

	/* Setup eventdev capability callbacks */
	l2fwd_event_capability_setup(evt_rsrc);

	/* Event device configuration */
	event_queue_cfg = evt_rsrc->ops.event_device_setup(rsrc);

	/* Event queue configuration */
	evt_rsrc->ops.event_queue_setup(rsrc, event_queue_cfg);

	/* Event port configuration */
	evt_rsrc->ops.event_port_setup(rsrc);

	/* Rx/Tx adapters configuration */
	evt_rsrc->ops.adapter_setup(rsrc);

	/* Start event device */
	ret = rte_event_dev_start(evt_rsrc->event_d_id);
	if (ret < 0)
		rte_panic("Error in starting eventdev\n");

	evt_rsrc->ops.l2fwd_event_loop = event_loop
					[rsrc->mac_updating]
					[evt_rsrc->tx_mode_q]
					[evt_rsrc->has_burst];
}
