/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020-2021 HiSilicon Limited.
 */

#include <rte_io.h>
#include <ethdev_driver.h>

#include "hns3_ethdev.h"
#include "hns3_rxtx.h"
#include "hns3_rxtx_vec.h"

#if defined RTE_ARCH_ARM64
#include "hns3_rxtx_vec_neon.h"
#endif

int
hns3_tx_check_vec_support(struct rte_eth_dev *dev)
{
	struct rte_eth_txmode *txmode = &dev->data->dev_conf.txmode;
	struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;

	/* Only support RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE */
	if (txmode->offloads != RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
		return -ENOTSUP;

	/*
	 * PTP function requires the cooperation of Rx and Tx.
	 * Tx vector isn't supported if RTE_ETH_RX_OFFLOAD_TIMESTAMP is set
	 * in Rx offloads.
	 */
	if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP)
		return -ENOTSUP;

	return 0;
}

uint16_t
hns3_xmit_pkts_vec(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct hns3_tx_queue *txq = (struct hns3_tx_queue *)tx_queue;
	uint16_t nb_tx = 0;

	while (nb_pkts) {
		uint16_t ret, new_burst;

		new_burst = RTE_MIN(nb_pkts, txq->tx_rs_thresh);
		ret = hns3_xmit_fixed_burst_vec(tx_queue, &tx_pkts[nb_tx],
						new_burst);
		nb_tx += ret;
		nb_pkts -= ret;
		if (ret < new_burst)
			break;
	}

	return nb_tx;
}

uint16_t
hns3_recv_pkts_vec(void *__restrict rx_queue,
		   struct rte_mbuf **__restrict rx_pkts,
		   uint16_t nb_pkts)
{
	struct hns3_rx_queue *rxq = rx_queue;
	struct hns3_desc *rxdp = &rxq->rx_ring[rxq->next_to_use];
	uint64_t pkt_err_mask;  /* bit mask indicate whick pkts is error */
	uint16_t nb_rx;

	rte_prefetch_non_temporal(rxdp);

	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, HNS3_DEFAULT_DESCS_PER_LOOP);

	if (rxq->rx_rearm_nb > HNS3_DEFAULT_RXQ_REARM_THRESH)
		hns3_rxq_rearm_mbuf(rxq);

	if (unlikely(!(rxdp->rx.bd_base_info &
			rte_cpu_to_le_32(1u << HNS3_RXD_VLD_B))))
		return 0;

	rte_prefetch0(rxq->sw_ring[rxq->next_to_use + 0].mbuf);
	rte_prefetch0(rxq->sw_ring[rxq->next_to_use + 1].mbuf);
	rte_prefetch0(rxq->sw_ring[rxq->next_to_use + 2].mbuf);
	rte_prefetch0(rxq->sw_ring[rxq->next_to_use + 3].mbuf);

	if (likely(nb_pkts <= HNS3_DEFAULT_RX_BURST)) {
		pkt_err_mask = 0;
		nb_rx = hns3_recv_burst_vec(rxq, rx_pkts, nb_pkts,
					    &pkt_err_mask);
		nb_rx = hns3_rx_reassemble_pkts(rx_pkts, nb_rx, pkt_err_mask);
		return nb_rx;
	}

	nb_rx = 0;
	while (nb_pkts > 0) {
		uint16_t ret, n;

		n = RTE_MIN(nb_pkts, HNS3_DEFAULT_RX_BURST);
		pkt_err_mask = 0;
		ret = hns3_recv_burst_vec(rxq, &rx_pkts[nb_rx], n,
					  &pkt_err_mask);
		nb_pkts -= ret;
		nb_rx += hns3_rx_reassemble_pkts(&rx_pkts[nb_rx], ret,
						 pkt_err_mask);
		if (ret < n)
			break;

		if (rxq->rx_rearm_nb > HNS3_DEFAULT_RXQ_REARM_THRESH)
			hns3_rxq_rearm_mbuf(rxq);
	}

	return nb_rx;
}

static void
hns3_rxq_vec_setup_rearm_data(struct hns3_rx_queue *rxq)
{
	uintptr_t p;
	struct rte_mbuf mb_def = { .buf_addr = 0 }; /* zeroed mbuf */

	mb_def.nb_segs = 1;
	mb_def.data_off = RTE_PKTMBUF_HEADROOM;
	mb_def.port = rxq->port_id;
	rte_mbuf_refcnt_set(&mb_def, 1);

	/* compile-time verifies the rearm_data first 8bytes */
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_off) <
			 offsetof(struct rte_mbuf, rearm_data));
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, refcnt) <
			 offsetof(struct rte_mbuf, rearm_data));
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, nb_segs) <
			 offsetof(struct rte_mbuf, rearm_data));
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, port) <
			 offsetof(struct rte_mbuf, rearm_data));
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_off) -
			 offsetof(struct rte_mbuf, rearm_data) > 6);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, refcnt) -
			 offsetof(struct rte_mbuf, rearm_data) > 6);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, nb_segs) -
			 offsetof(struct rte_mbuf, rearm_data) > 6);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, port) -
			 offsetof(struct rte_mbuf, rearm_data) > 6);

	/* prevent compiler reordering: rearm_data covers previous fields */
	rte_compiler_barrier();
	p = (uintptr_t)&mb_def.rearm_data;
	rxq->mbuf_initializer = *(uint64_t *)p;
}

void
hns3_rxq_vec_setup(struct hns3_rx_queue *rxq)
{
	struct hns3_entry *sw_ring = &rxq->sw_ring[rxq->nb_rx_desc];
	unsigned int i;

	memset(&rxq->rx_ring[rxq->nb_rx_desc], 0,
		sizeof(struct hns3_desc) * HNS3_DEFAULT_RX_BURST);

	memset(&rxq->fake_mbuf, 0, sizeof(rxq->fake_mbuf));
	for (i = 0; i < HNS3_DEFAULT_RX_BURST; i++)
		sw_ring[i].mbuf = &rxq->fake_mbuf;

	hns3_rxq_vec_setup_rearm_data(rxq);

	memset(rxq->offset_table, 0, sizeof(rxq->offset_table));
}

static int
hns3_rxq_vec_check(struct hns3_rx_queue *rxq, void *arg)
{
	uint32_t min_vec_bds = HNS3_DEFAULT_RXQ_REARM_THRESH +
				HNS3_DEFAULT_RX_BURST;

	if (rxq->nb_rx_desc < min_vec_bds)
		return -ENOTSUP;

	if (rxq->nb_rx_desc % HNS3_DEFAULT_RXQ_REARM_THRESH)
		return -ENOTSUP;

	RTE_SET_USED(arg);
	return 0;
}

int
hns3_rx_check_vec_support(struct rte_eth_dev *dev)
{
	struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
	uint64_t offloads_mask = RTE_ETH_RX_OFFLOAD_TCP_LRO |
				 RTE_ETH_RX_OFFLOAD_VLAN |
				 RTE_ETH_RX_OFFLOAD_TIMESTAMP;

	if (dev->data->scattered_rx)
		return -ENOTSUP;

	if (rxmode->offloads & offloads_mask)
		return -ENOTSUP;

	if (hns3_rxq_iterate(dev, hns3_rxq_vec_check, NULL) != 0)
		return -ENOTSUP;

	return 0;
}
