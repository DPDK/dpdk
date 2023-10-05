/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifdef RTE_LIB_EVENTDEV
#include <stdbool.h>
#include <getopt.h>

#include <rte_malloc.h>

#include "l3fwd.h"
#include "l3fwd_event.h"

static void
print_ethaddr(const char *name, const struct rte_ether_addr *eth_addr)
{
	char buf[RTE_ETHER_ADDR_FMT_SIZE];
	rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, eth_addr);
	printf("%s%s", name, buf);
}

struct l3fwd_event_resources *
l3fwd_get_eventdev_rsrc(void)
{
	static struct l3fwd_event_resources *rsrc;

	if (rsrc != NULL)
		return rsrc;

	rsrc = rte_zmalloc("l3fwd", sizeof(struct l3fwd_event_resources), 0);
	if (rsrc != NULL) {
		rsrc->sched_type = RTE_SCHED_TYPE_ATOMIC;
		rsrc->eth_rx_queues = 1;
		return rsrc;
	}

	rte_exit(EXIT_FAILURE, "Unable to allocate memory for eventdev cfg\n");

	return NULL;
}

static void
l3fwd_eth_dev_port_setup(struct rte_eth_conf *port_conf)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();
	uint16_t nb_ports = rte_eth_dev_count_avail();
	unsigned int nb_lcores = rte_lcore_count();
	struct rte_eth_conf local_port_conf;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf txconf;
	struct rte_eth_rxconf rxconf;
	unsigned int nb_mbuf;
	uint16_t port_id;
	uint8_t eth_qid;
	int32_t ret;

	/* initialize all ports */
	RTE_ETH_FOREACH_DEV(port_id) {
		local_port_conf = *port_conf;
		/* skip ports that are not enabled */
		if ((evt_rsrc->port_mask & (1 << port_id)) == 0) {
			printf("\nSkipping disabled port %d\n", port_id);
			continue;
		}

		/* init port */
		printf("Initializing port %d ... ", port_id);
		fflush(stdout);
		printf("Creating queues: nb_rxq=%d nb_txq=1...\n",
		       evt_rsrc->eth_rx_queues);

		ret = rte_eth_dev_info_get(port_id, &dev_info);
		if (ret != 0)
			rte_panic("Error during getting device (port %u) info:"
				  "%s\n", port_id, strerror(-ret));

		ret = config_port_max_pkt_len(&local_port_conf, &dev_info);
		if (ret != 0)
			rte_exit(EXIT_FAILURE,
				"Invalid max packet length: %u (port %u)\n",
				max_pkt_len, port_id);

		if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
			local_port_conf.txmode.offloads |=
						RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

		local_port_conf.rx_adv_conf.rss_conf.rss_hf &=
						dev_info.flow_type_rss_offloads;
		if (local_port_conf.rx_adv_conf.rss_conf.rss_hf !=
				port_conf->rx_adv_conf.rss_conf.rss_hf) {
			printf("Port %u modified RSS hash function "
			       "based on hardware support,"
			       "requested:%#"PRIx64" configured:%#"PRIx64"\n",
			       port_id,
			       port_conf->rx_adv_conf.rss_conf.rss_hf,
			       local_port_conf.rx_adv_conf.rss_conf.rss_hf);
		}

		ret = rte_eth_dev_configure(port_id, evt_rsrc->eth_rx_queues,
					    1, &local_port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Cannot configure device: err=%d, port=%d\n",
				 ret, port_id);

		ret = rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &nb_rxd,
						       &nb_txd);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Cannot adjust number of descriptors: err=%d, "
				 "port=%d\n", ret, port_id);

		rte_eth_macaddr_get(port_id, &ports_eth_addr[port_id]);
		print_ethaddr(" Address:", &ports_eth_addr[port_id]);
		printf(", ");
		print_ethaddr("Destination:",
			(const struct rte_ether_addr *)&dest_eth_addr[port_id]);
		printf(", ");

		/* prepare source MAC for each port. */
		rte_ether_addr_copy(&ports_eth_addr[port_id],
			(struct rte_ether_addr *)(val_eth + port_id) + 1);

		/* init memory */
		if (!evt_rsrc->per_port_pool) {
			/* port_id = 0; this is *not* signifying the first port,
			 * rather, it signifies that port_id is ignored.
			 */
			nb_mbuf = RTE_MAX(nb_ports * nb_rxd +
					  nb_ports * nb_txd +
					  nb_ports * nb_lcores *
							MAX_PKT_BURST +
					  nb_lcores * MEMPOOL_CACHE_SIZE,
					  8192u);
			ret = init_mem(0, nb_mbuf);
		} else {
			nb_mbuf = RTE_MAX(nb_rxd + nb_rxd +
					  nb_lcores * MAX_PKT_BURST +
					  nb_lcores * MEMPOOL_CACHE_SIZE,
					  8192u);
			ret = init_mem(port_id, nb_mbuf);
		}
		/* init Rx queues per port */
		rxconf = dev_info.default_rxconf;
		rxconf.offloads = local_port_conf.rxmode.offloads;

		for (eth_qid = 0; eth_qid < evt_rsrc->eth_rx_queues;
		     eth_qid++) {
			if (!evt_rsrc->per_port_pool)
				ret = rte_eth_rx_queue_setup(port_id, eth_qid,
					nb_rxd, 0, &rxconf,
					evt_rsrc->pkt_pool[0][0]);
			else
				ret = rte_eth_rx_queue_setup(port_id, eth_qid,
					nb_rxd, 0, &rxconf,
					evt_rsrc->pkt_pool[port_id][0]);
			if (ret < 0)
				rte_exit(EXIT_FAILURE,
					 "rte_eth_rx_queue_setup: err=%d, "
					 "port=%d, eth_qid: %d\n",
					 ret, port_id, eth_qid);
		}

		/* init one Tx queue per port */
		txconf = dev_info.default_txconf;
		txconf.offloads = local_port_conf.txmode.offloads;
		ret = rte_eth_tx_queue_setup(port_id, 0, nb_txd, 0, &txconf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "rte_eth_tx_queue_setup: err=%d, "
				 "port=%d\n", ret, port_id);
	}
}

static void
l3fwd_event_capability_setup(void)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();
	uint32_t caps = 0;
	uint16_t i;
	int ret;

	RTE_ETH_FOREACH_DEV(i) {
		ret = rte_event_eth_tx_adapter_caps_get(0, i, &caps);
		if (ret)
			rte_exit(EXIT_FAILURE,
				 "Invalid capability for Tx adptr port %d\n",
				 i);

		evt_rsrc->tx_mode_q |= !(caps &
				   RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT);
	}

	if (evt_rsrc->tx_mode_q)
		l3fwd_event_set_generic_ops(&evt_rsrc->ops);
	else
		l3fwd_event_set_internal_port_ops(&evt_rsrc->ops);
}

int
l3fwd_get_free_event_port(struct l3fwd_event_resources *evt_rsrc)
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

void
l3fwd_event_resource_setup(struct rte_eth_conf *port_conf)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();
	const event_loop_cb lpm_event_loop[2][2][2] = {
		[0][0][0] = lpm_event_main_loop_tx_d,
		[0][0][1] = lpm_event_main_loop_tx_d_burst,
		[0][1][0] = lpm_event_main_loop_tx_q,
		[0][1][1] = lpm_event_main_loop_tx_q_burst,
		[1][0][0] = lpm_event_main_loop_tx_d_vector,
		[1][0][1] = lpm_event_main_loop_tx_d_burst_vector,
		[1][1][0] = lpm_event_main_loop_tx_q_vector,
		[1][1][1] = lpm_event_main_loop_tx_q_burst_vector,
	};
	const event_loop_cb em_event_loop[2][2][2] = {
		[0][0][0] = em_event_main_loop_tx_d,
		[0][0][1] = em_event_main_loop_tx_d_burst,
		[0][1][0] = em_event_main_loop_tx_q,
		[0][1][1] = em_event_main_loop_tx_q_burst,
		[1][0][0] = em_event_main_loop_tx_d_vector,
		[1][0][1] = em_event_main_loop_tx_d_burst_vector,
		[1][1][0] = em_event_main_loop_tx_q_vector,
		[1][1][1] = em_event_main_loop_tx_q_burst_vector,
	};
	const event_loop_cb fib_event_loop[2][2][2] = {
		[0][0][0] = fib_event_main_loop_tx_d,
		[0][0][1] = fib_event_main_loop_tx_d_burst,
		[0][1][0] = fib_event_main_loop_tx_q,
		[0][1][1] = fib_event_main_loop_tx_q_burst,
		[1][0][0] = fib_event_main_loop_tx_d_vector,
		[1][0][1] = fib_event_main_loop_tx_d_burst_vector,
		[1][1][0] = fib_event_main_loop_tx_q_vector,
		[1][1][1] = fib_event_main_loop_tx_q_burst_vector,
	};
	uint32_t event_queue_cfg;
	int ret;

	if (!evt_rsrc->enabled)
		return;

	if (!rte_event_dev_count())
		rte_exit(EXIT_FAILURE, "No Eventdev found");

	/* Setup eventdev capability callbacks */
	l3fwd_event_capability_setup();

	/* Ethernet device configuration */
	l3fwd_eth_dev_port_setup(port_conf);

	/* Event device configuration */
	event_queue_cfg = evt_rsrc->ops.event_device_setup();

	/* Event queue configuration */
	evt_rsrc->ops.event_queue_setup(event_queue_cfg);

	/* Event port configuration */
	evt_rsrc->ops.event_port_setup();

	/* Rx/Tx adapters configuration */
	evt_rsrc->ops.adapter_setup();

	/* Start event device */
	ret = rte_event_dev_start(evt_rsrc->event_d_id);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error in starting eventdev");

	evt_rsrc->ops.lpm_event_loop =
		lpm_event_loop[evt_rsrc->vector_enabled][evt_rsrc->tx_mode_q]
			      [evt_rsrc->has_burst];

	evt_rsrc->ops.em_event_loop =
		em_event_loop[evt_rsrc->vector_enabled][evt_rsrc->tx_mode_q]
			     [evt_rsrc->has_burst];

	evt_rsrc->ops.fib_event_loop =
		fib_event_loop[evt_rsrc->vector_enabled][evt_rsrc->tx_mode_q]
			      [evt_rsrc->has_burst];
}

static void
l3fwd_event_vector_array_free(struct rte_event events[], uint16_t num)
{
	uint16_t i;

	for (i = 0; i < num; i++) {
		rte_pktmbuf_free_bulk(
			&events[i].vec->mbufs[events[i].vec->elem_offset],
			events[i].vec->nb_elem);
		rte_mempool_put(rte_mempool_from_obj(events[i].vec),
				events[i].vec);
	}
}

static void
l3fwd_event_port_flush(uint8_t event_d_id __rte_unused, struct rte_event ev,
		       void *args __rte_unused)
{
	if (ev.event_type & RTE_EVENT_TYPE_VECTOR)
		l3fwd_event_vector_array_free(&ev, 1);
	else
		rte_pktmbuf_free(ev.mbuf);
}

void
l3fwd_event_worker_cleanup(uint8_t event_d_id, uint8_t event_p_id,
			   struct rte_event events[], uint16_t nb_enq,
			   uint16_t nb_deq, uint8_t is_vector)
{
	int i;

	if (nb_deq) {
		if (is_vector)
			l3fwd_event_vector_array_free(events + nb_enq,
						      nb_deq - nb_enq);
		else
			for (i = nb_enq; i < nb_deq; i++)
				rte_pktmbuf_free(events[i].mbuf);

		for (i = 0; i < nb_deq; i++)
			events[i].op = RTE_EVENT_OP_RELEASE;
		rte_event_enqueue_burst(event_d_id, event_p_id, events, nb_deq);
	}

	rte_event_port_quiesce(event_d_id, event_p_id, l3fwd_event_port_flush,
			       NULL);
}
#endif /* RTE_LIB_EVENTDEV */
