/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2022 Advanced Micro Devices, Inc.
 */

#include <rte_malloc.h>
#include <ethdev_driver.h>

#include "ionic.h"
#include "ionic_logs.h"
#include "ionic_lif.h"
#include "ionic_ethdev.h"
#include "ionic_rx_filter.h"
#include "ionic_rxtx.h"

/* queuetype support level */
static const uint8_t ionic_qtype_vers[IONIC_QTYPE_MAX] = {
	[IONIC_QTYPE_ADMINQ]  = 0,   /* 0 = Base version with CQ support */
	[IONIC_QTYPE_NOTIFYQ] = 0,   /* 0 = Base version */
	[IONIC_QTYPE_RXQ]     = 2,   /* 0 = Base version with CQ+SG support
				      * 1 =       ... with EQ
				      * 2 =       ... with CMB
				      */
	[IONIC_QTYPE_TXQ]     = 3,   /* 0 = Base version with CQ+SG support
				      * 1 =   ... with Tx SG version 1
				      * 2 =       ... with EQ
				      * 3 =       ... with CMB
				      */
};

static int ionic_lif_addr_add(struct ionic_lif *lif, const uint8_t *addr);
static int ionic_lif_addr_del(struct ionic_lif *lif, const uint8_t *addr);

static int
ionic_qcq_disable(struct ionic_qcq *qcq)
{
	struct ionic_queue *q = &qcq->q;
	struct ionic_lif *lif = qcq->lif;
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.q_control = {
			.opcode = IONIC_CMD_Q_CONTROL,
			.type = q->type,
			.index = rte_cpu_to_le_32(q->index),
			.oper = IONIC_Q_DISABLE,
		},
	};

	return ionic_adminq_post_wait(lif, &ctx);
}

void
ionic_lif_stop(struct ionic_lif *lif)
{
	uint32_t i;

	IONIC_PRINT_CALL();

	lif->state &= ~IONIC_LIF_F_UP;

	for (i = 0; i < lif->nrxqcqs; i++) {
		struct ionic_rx_qcq *rxq = lif->rxqcqs[i];
		if (rxq->flags & IONIC_QCQ_F_INITED)
			(void)ionic_dev_rx_queue_stop(lif->eth_dev, i);
	}

	for (i = 0; i < lif->ntxqcqs; i++) {
		struct ionic_tx_qcq *txq = lif->txqcqs[i];
		if (txq->flags & IONIC_QCQ_F_INITED)
			(void)ionic_dev_tx_queue_stop(lif->eth_dev, i);
	}
}

void
ionic_lif_reset(struct ionic_lif *lif)
{
	struct ionic_dev *idev = &lif->adapter->idev;
	int err;

	IONIC_PRINT_CALL();

	ionic_dev_cmd_lif_reset(idev);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	if (err)
		IONIC_PRINT(WARNING, "Failed to reset %s", lif->name);
}

static void
ionic_lif_get_abs_stats(const struct ionic_lif *lif, struct rte_eth_stats *stats)
{
	struct ionic_lif_stats *ls = &lif->info->stats;
	uint32_t i;
	uint32_t num_rx_q_counters = RTE_MIN(lif->nrxqcqs, (uint32_t)
			RTE_ETHDEV_QUEUE_STAT_CNTRS);
	uint32_t num_tx_q_counters = RTE_MIN(lif->ntxqcqs, (uint32_t)
			RTE_ETHDEV_QUEUE_STAT_CNTRS);

	memset(stats, 0, sizeof(*stats));

	if (ls == NULL) {
		IONIC_PRINT(DEBUG, "Stats on port %u not yet initialized",
			lif->port_id);
		return;
	}

	/* RX */

	stats->ipackets = ls->rx_ucast_packets +
		ls->rx_mcast_packets +
		ls->rx_bcast_packets;

	stats->ibytes = ls->rx_ucast_bytes +
		ls->rx_mcast_bytes +
		ls->rx_bcast_bytes;

	for (i = 0; i < lif->nrxqcqs; i++) {
		struct ionic_rx_stats *rx_stats = &lif->rxqcqs[i]->stats;
		stats->ierrors +=
			rx_stats->bad_cq_status +
			rx_stats->bad_len;
	}

	stats->imissed +=
		ls->rx_ucast_drop_packets +
		ls->rx_mcast_drop_packets +
		ls->rx_bcast_drop_packets;

	stats->ierrors +=
		ls->rx_dma_error +
		ls->rx_desc_fetch_error +
		ls->rx_desc_data_error;

	for (i = 0; i < num_rx_q_counters; i++) {
		struct ionic_rx_stats *rx_stats = &lif->rxqcqs[i]->stats;
		stats->q_ipackets[i] = rx_stats->packets;
		stats->q_ibytes[i] = rx_stats->bytes;
		stats->q_errors[i] =
			rx_stats->bad_cq_status +
			rx_stats->bad_len;
	}

	/* TX */

	stats->opackets = ls->tx_ucast_packets +
		ls->tx_mcast_packets +
		ls->tx_bcast_packets;

	stats->obytes = ls->tx_ucast_bytes +
		ls->tx_mcast_bytes +
		ls->tx_bcast_bytes;

	for (i = 0; i < lif->ntxqcqs; i++) {
		struct ionic_tx_stats *tx_stats = &lif->txqcqs[i]->stats;
		stats->oerrors += tx_stats->drop;
	}

	stats->oerrors +=
		ls->tx_ucast_drop_packets +
		ls->tx_mcast_drop_packets +
		ls->tx_bcast_drop_packets;

	stats->oerrors +=
		ls->tx_dma_error +
		ls->tx_queue_disabled +
		ls->tx_desc_fetch_error +
		ls->tx_desc_data_error;

	for (i = 0; i < num_tx_q_counters; i++) {
		struct ionic_tx_stats *tx_stats = &lif->txqcqs[i]->stats;
		stats->q_opackets[i] = tx_stats->packets;
		stats->q_obytes[i] = tx_stats->bytes;
	}
}

void
ionic_lif_get_stats(const struct ionic_lif *lif,
		struct rte_eth_stats *stats)
{
	ionic_lif_get_abs_stats(lif, stats);

	stats->ipackets  -= lif->stats_base.ipackets;
	stats->opackets  -= lif->stats_base.opackets;
	stats->ibytes    -= lif->stats_base.ibytes;
	stats->obytes    -= lif->stats_base.obytes;
	stats->imissed   -= lif->stats_base.imissed;
	stats->ierrors   -= lif->stats_base.ierrors;
	stats->oerrors   -= lif->stats_base.oerrors;
	stats->rx_nombuf -= lif->stats_base.rx_nombuf;
}

void
ionic_lif_reset_stats(struct ionic_lif *lif)
{
	uint32_t i;

	for (i = 0; i < lif->nrxqcqs; i++) {
		memset(&lif->rxqcqs[i]->stats, 0,
			sizeof(struct ionic_rx_stats));
		memset(&lif->txqcqs[i]->stats, 0,
			sizeof(struct ionic_tx_stats));
	}

	ionic_lif_get_abs_stats(lif, &lif->stats_base);
}

void
ionic_lif_get_hw_stats(struct ionic_lif *lif, struct ionic_lif_stats *stats)
{
	uint16_t i, count = sizeof(struct ionic_lif_stats) / sizeof(uint64_t);
	uint64_t *stats64 = (uint64_t *)stats;
	uint64_t *lif_stats64 = (uint64_t *)&lif->info->stats;
	uint64_t *lif_stats64_base = (uint64_t *)&lif->lif_stats_base;

	for (i = 0; i < count; i++)
		stats64[i] = lif_stats64[i] - lif_stats64_base[i];
}

void
ionic_lif_reset_hw_stats(struct ionic_lif *lif)
{
	uint16_t i, count = sizeof(struct ionic_lif_stats) / sizeof(uint64_t);
	uint64_t *lif_stats64 = (uint64_t *)&lif->info->stats;
	uint64_t *lif_stats64_base = (uint64_t *)&lif->lif_stats_base;

	for (i = 0; i < count; i++)
		lif_stats64_base[i] = lif_stats64[i];
}

static int
ionic_lif_addr_add(struct ionic_lif *lif, const uint8_t *addr)
{
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.rx_filter_add = {
			.opcode = IONIC_CMD_RX_FILTER_ADD,
			.match = rte_cpu_to_le_16(IONIC_RX_FILTER_MATCH_MAC),
		},
	};
	int err;

	memcpy(ctx.cmd.rx_filter_add.mac.addr, addr, RTE_ETHER_ADDR_LEN);

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	IONIC_PRINT(INFO, "rx_filter add (id %d)",
		rte_le_to_cpu_32(ctx.comp.rx_filter_add.filter_id));

	return ionic_rx_filter_save(lif, 0, IONIC_RXQ_INDEX_ANY, &ctx);
}

static int
ionic_lif_addr_del(struct ionic_lif *lif, const uint8_t *addr)
{
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.rx_filter_del = {
			.opcode = IONIC_CMD_RX_FILTER_DEL,
		},
	};
	struct ionic_rx_filter *f;
	int err;

	IONIC_PRINT_CALL();

	rte_spinlock_lock(&lif->rx_filters.lock);

	f = ionic_rx_filter_by_addr(lif, addr);
	if (!f) {
		rte_spinlock_unlock(&lif->rx_filters.lock);
		return -ENOENT;
	}

	ctx.cmd.rx_filter_del.filter_id = rte_cpu_to_le_32(f->filter_id);
	ionic_rx_filter_free(f);

	rte_spinlock_unlock(&lif->rx_filters.lock);

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	IONIC_PRINT(INFO, "rx_filter del (id %d)",
		rte_le_to_cpu_32(ctx.cmd.rx_filter_del.filter_id));

	return 0;
}

int
ionic_dev_add_mac(struct rte_eth_dev *eth_dev,
		struct rte_ether_addr *mac_addr,
		uint32_t index __rte_unused, uint32_t pool __rte_unused)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);

	IONIC_PRINT_CALL();

	return ionic_lif_addr_add(lif, (const uint8_t *)mac_addr);
}

void
ionic_dev_remove_mac(struct rte_eth_dev *eth_dev, uint32_t index)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	struct ionic_adapter *adapter = lif->adapter;
	struct rte_ether_addr *mac_addr;

	IONIC_PRINT_CALL();

	if (index >= adapter->max_mac_addrs) {
		IONIC_PRINT(WARNING,
			"Index %u is above MAC filter limit %u",
			index, adapter->max_mac_addrs);
		return;
	}

	mac_addr = &eth_dev->data->mac_addrs[index];

	if (!rte_is_valid_assigned_ether_addr(mac_addr))
		return;

	ionic_lif_addr_del(lif, (const uint8_t *)mac_addr);
}

int
ionic_dev_set_mac(struct rte_eth_dev *eth_dev, struct rte_ether_addr *mac_addr)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);

	IONIC_PRINT_CALL();

	if (mac_addr == NULL) {
		IONIC_PRINT(NOTICE, "New mac is null");
		return -1;
	}

	if (!rte_is_zero_ether_addr((struct rte_ether_addr *)lif->mac_addr)) {
		IONIC_PRINT(INFO, "Deleting mac addr %pM",
			lif->mac_addr);
		ionic_lif_addr_del(lif, lif->mac_addr);
		memset(lif->mac_addr, 0, RTE_ETHER_ADDR_LEN);
	}

	IONIC_PRINT(INFO, "Updating mac addr");

	rte_ether_addr_copy(mac_addr, (struct rte_ether_addr *)lif->mac_addr);

	return ionic_lif_addr_add(lif, (const uint8_t *)mac_addr);
}

static int
ionic_vlan_rx_add_vid(struct ionic_lif *lif, uint16_t vid)
{
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.rx_filter_add = {
			.opcode = IONIC_CMD_RX_FILTER_ADD,
			.match = rte_cpu_to_le_16(IONIC_RX_FILTER_MATCH_VLAN),
			.vlan.vlan = rte_cpu_to_le_16(vid),
		},
	};
	int err;

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	IONIC_PRINT(INFO, "rx_filter add VLAN %d (id %d)", vid,
		rte_le_to_cpu_32(ctx.comp.rx_filter_add.filter_id));

	return ionic_rx_filter_save(lif, 0, IONIC_RXQ_INDEX_ANY, &ctx);
}

static int
ionic_vlan_rx_kill_vid(struct ionic_lif *lif, uint16_t vid)
{
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.rx_filter_del = {
			.opcode = IONIC_CMD_RX_FILTER_DEL,
		},
	};
	struct ionic_rx_filter *f;
	int err;

	IONIC_PRINT_CALL();

	rte_spinlock_lock(&lif->rx_filters.lock);

	f = ionic_rx_filter_by_vlan(lif, vid);
	if (!f) {
		rte_spinlock_unlock(&lif->rx_filters.lock);
		return -ENOENT;
	}

	ctx.cmd.rx_filter_del.filter_id = rte_cpu_to_le_32(f->filter_id);
	ionic_rx_filter_free(f);
	rte_spinlock_unlock(&lif->rx_filters.lock);

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	IONIC_PRINT(INFO, "rx_filter del VLAN %d (id %d)", vid,
		rte_le_to_cpu_32(ctx.cmd.rx_filter_del.filter_id));

	return 0;
}

int
ionic_dev_vlan_filter_set(struct rte_eth_dev *eth_dev, uint16_t vlan_id,
		int on)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	int err;

	if (on)
		err = ionic_vlan_rx_add_vid(lif, vlan_id);
	else
		err = ionic_vlan_rx_kill_vid(lif, vlan_id);

	return err;
}

static void
ionic_lif_rx_mode(struct ionic_lif *lif, uint32_t rx_mode)
{
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.rx_mode_set = {
			.opcode = IONIC_CMD_RX_MODE_SET,
			.rx_mode = rte_cpu_to_le_16(rx_mode),
		},
	};
	int err;

	if (rx_mode & IONIC_RX_MODE_F_UNICAST)
		IONIC_PRINT(DEBUG, "rx_mode IONIC_RX_MODE_F_UNICAST");
	if (rx_mode & IONIC_RX_MODE_F_MULTICAST)
		IONIC_PRINT(DEBUG, "rx_mode IONIC_RX_MODE_F_MULTICAST");
	if (rx_mode & IONIC_RX_MODE_F_BROADCAST)
		IONIC_PRINT(DEBUG, "rx_mode IONIC_RX_MODE_F_BROADCAST");
	if (rx_mode & IONIC_RX_MODE_F_PROMISC)
		IONIC_PRINT(DEBUG, "rx_mode IONIC_RX_MODE_F_PROMISC");
	if (rx_mode & IONIC_RX_MODE_F_ALLMULTI)
		IONIC_PRINT(DEBUG, "rx_mode IONIC_RX_MODE_F_ALLMULTI");

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		IONIC_PRINT(ERR, "Failure setting RX mode");
}

static void
ionic_set_rx_mode(struct ionic_lif *lif, uint32_t rx_mode)
{
	if (lif->rx_mode != rx_mode) {
		lif->rx_mode = rx_mode;
		ionic_lif_rx_mode(lif, rx_mode);
	}
}

int
ionic_dev_promiscuous_enable(struct rte_eth_dev *eth_dev)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	uint32_t rx_mode = lif->rx_mode;

	IONIC_PRINT_CALL();

	rx_mode |= IONIC_RX_MODE_F_PROMISC;

	ionic_set_rx_mode(lif, rx_mode);

	return 0;
}

int
ionic_dev_promiscuous_disable(struct rte_eth_dev *eth_dev)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	uint32_t rx_mode = lif->rx_mode;

	rx_mode &= ~IONIC_RX_MODE_F_PROMISC;

	ionic_set_rx_mode(lif, rx_mode);

	return 0;
}

int
ionic_dev_allmulticast_enable(struct rte_eth_dev *eth_dev)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	uint32_t rx_mode = lif->rx_mode;

	rx_mode |= IONIC_RX_MODE_F_ALLMULTI;

	ionic_set_rx_mode(lif, rx_mode);

	return 0;
}

int
ionic_dev_allmulticast_disable(struct rte_eth_dev *eth_dev)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	uint32_t rx_mode = lif->rx_mode;

	rx_mode &= ~IONIC_RX_MODE_F_ALLMULTI;

	ionic_set_rx_mode(lif, rx_mode);

	return 0;
}

int
ionic_lif_change_mtu(struct ionic_lif *lif, uint32_t new_mtu)
{
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.lif_setattr = {
			.opcode = IONIC_CMD_LIF_SETATTR,
			.attr = IONIC_LIF_ATTR_MTU,
			.mtu = rte_cpu_to_le_32(new_mtu),
		},
	};

	return ionic_adminq_post_wait(lif, &ctx);
}

int
ionic_intr_alloc(struct ionic_lif *lif, struct ionic_intr_info *intr)
{
	struct ionic_adapter *adapter = lif->adapter;
	struct ionic_dev *idev = &adapter->idev;
	unsigned long index;

	/*
	 * Note: interrupt handler is called for index = 0 only
	 * (we use interrupts for the notifyq only anyway,
	 * which has index = 0)
	 */

	for (index = 0; index < adapter->nintrs; index++)
		if (!adapter->intrs[index])
			break;

	if (index == adapter->nintrs)
		return -ENOSPC;

	adapter->intrs[index] = true;

	ionic_intr_init(idev, intr, index);

	return 0;
}

static int
ionic_qcq_alloc(struct ionic_lif *lif,
		uint8_t type,
		size_t struct_size,
		uint32_t socket_id,
		uint32_t index,
		const char *type_name,
		uint16_t flags,
		uint16_t num_descs,
		uint16_t num_segs,
		uint16_t desc_size,
		uint16_t cq_desc_size,
		uint16_t sg_desc_size,
		struct ionic_qcq **qcq)
{
	struct ionic_qcq *new;
	uint32_t q_size, cq_size, sg_size, total_size;
	void *q_base, *cq_base, *sg_base;
	rte_iova_t q_base_pa = 0;
	rte_iova_t cq_base_pa = 0;
	rte_iova_t sg_base_pa = 0;
	size_t page_size = rte_mem_page_size();
	int err;

	*qcq = NULL;

	q_size  = num_descs * desc_size;
	cq_size = num_descs * cq_desc_size;
	sg_size = num_descs * sg_desc_size;

	total_size = RTE_ALIGN(q_size, page_size) +
			RTE_ALIGN(cq_size, page_size);
	/*
	 * Note: aligning q_size/cq_size is not enough due to cq_base address
	 * aligning as q_base could be not aligned to the page.
	 * Adding page_size.
	 */
	total_size += page_size;

	if (flags & IONIC_QCQ_F_SG) {
		total_size += RTE_ALIGN(sg_size, page_size);
		total_size += page_size;
	}

	new = rte_zmalloc_socket("ionic", struct_size,
				RTE_CACHE_LINE_SIZE, socket_id);
	if (!new) {
		IONIC_PRINT(ERR, "Cannot allocate queue structure");
		return -ENOMEM;
	}

	new->lif = lif;

	/* Most queue types will store 1 ptr per descriptor */
	new->q.info = rte_calloc_socket("ionic",
				(uint64_t)num_descs * num_segs,
				sizeof(void *), page_size, socket_id);
	if (!new->q.info) {
		IONIC_PRINT(ERR, "Cannot allocate queue info");
		err = -ENOMEM;
		goto err_out_free_qcq;
	}

	new->q.num_segs = num_segs;
	new->q.type = type;

	err = ionic_q_init(&new->q, index, num_descs);
	if (err) {
		IONIC_PRINT(ERR, "Queue initialization failed");
		goto err_out_free_info;
	}

	err = ionic_cq_init(&new->cq, num_descs);
	if (err) {
		IONIC_PRINT(ERR, "Completion queue initialization failed");
		goto err_out_free_info;
	}

	new->base_z = rte_eth_dma_zone_reserve(lif->eth_dev,
		type_name, index /* queue_idx */,
		total_size, IONIC_ALIGN, socket_id);

	if (!new->base_z) {
		IONIC_PRINT(ERR, "Cannot reserve queue DMA memory");
		err = -ENOMEM;
		goto err_out_free_info;
	}

	new->base = new->base_z->addr;
	new->base_pa = new->base_z->iova;

	q_base = new->base;
	q_base_pa = new->base_pa;

	cq_base = (void *)RTE_ALIGN((uintptr_t)q_base + q_size, page_size);
	cq_base_pa = RTE_ALIGN(q_base_pa + q_size, page_size);

	if (flags & IONIC_QCQ_F_SG) {
		sg_base = (void *)RTE_ALIGN((uintptr_t)cq_base + cq_size,
				page_size);
		sg_base_pa = RTE_ALIGN(cq_base_pa + cq_size, page_size);
		ionic_q_sg_map(&new->q, sg_base, sg_base_pa);
	}

	if (flags & IONIC_QCQ_F_CMB) {
		/* alloc descriptor ring from nic memory */
		if (lif->adapter->cmb_offset + q_size >
				lif->adapter->bars.bar[2].len) {
			IONIC_PRINT(ERR, "Cannot reserve queue from NIC mem");
			return -ENOMEM;
		}
		q_base = (void *)
			((uintptr_t)lif->adapter->bars.bar[2].vaddr +
			 (uintptr_t)lif->adapter->cmb_offset);
		/* CMB PA is a relative address */
		q_base_pa = lif->adapter->cmb_offset;
		lif->adapter->cmb_offset += q_size;
	}

	IONIC_PRINT(DEBUG, "Q-Base-PA = %#jx CQ-Base-PA = %#jx "
		"SG-base-PA = %#jx",
		q_base_pa, cq_base_pa, sg_base_pa);

	ionic_q_map(&new->q, q_base, q_base_pa);
	ionic_cq_map(&new->cq, cq_base, cq_base_pa);

	*qcq = new;

	return 0;

err_out_free_info:
	rte_free(new->q.info);
err_out_free_qcq:
	rte_free(new);

	return err;
}

void
ionic_qcq_free(struct ionic_qcq *qcq)
{
	if (qcq->base_z) {
		qcq->base = NULL;
		qcq->base_pa = 0;
		rte_memzone_free(qcq->base_z);
		qcq->base_z = NULL;
	}

	if (qcq->q.info) {
		rte_free(qcq->q.info);
		qcq->q.info = NULL;
	}

	rte_free(qcq);
}

static uint64_t
ionic_rx_rearm_data(struct ionic_lif *lif)
{
	struct rte_mbuf rxm;

	memset(&rxm, 0, sizeof(rxm));

	rte_mbuf_refcnt_set(&rxm, 1);
	rxm.data_off = RTE_PKTMBUF_HEADROOM;
	rxm.nb_segs = 1;
	rxm.port = lif->port_id;

	rte_compiler_barrier();

	RTE_BUILD_BUG_ON(sizeof(rxm.rearm_data[0]) != sizeof(uint64_t));
	return rxm.rearm_data[0];
}

static uint64_t
ionic_rx_seg_rearm_data(struct ionic_lif *lif)
{
	struct rte_mbuf rxm;

	memset(&rxm, 0, sizeof(rxm));

	rte_mbuf_refcnt_set(&rxm, 1);
	rxm.data_off = 0;  /* no headroom */
	rxm.nb_segs = 1;
	rxm.port = lif->port_id;

	rte_compiler_barrier();

	RTE_BUILD_BUG_ON(sizeof(rxm.rearm_data[0]) != sizeof(uint64_t));
	return rxm.rearm_data[0];
}

int
ionic_rx_qcq_alloc(struct ionic_lif *lif, uint32_t socket_id, uint32_t index,
		uint16_t nrxq_descs, struct rte_mempool *mb_pool,
		struct ionic_rx_qcq **rxq_out)
{
	struct ionic_rx_qcq *rxq;
	uint16_t flags = 0, seg_size, hdr_seg_size, max_segs, max_segs_fw = 1;
	uint32_t max_mtu;
	int err;

	if (lif->state & IONIC_LIF_F_Q_IN_CMB)
		flags |= IONIC_QCQ_F_CMB;

	seg_size = rte_pktmbuf_data_room_size(mb_pool);

	/* The first mbuf needs to leave headroom */
	hdr_seg_size = seg_size - RTE_PKTMBUF_HEADROOM;

	max_mtu = rte_le_to_cpu_32(lif->adapter->ident.lif.eth.max_mtu);

	/* If mbufs are too small to hold received packets, enable SG */
	if (max_mtu > hdr_seg_size) {
		IONIC_PRINT(NOTICE, "Enabling RX_OFFLOAD_SCATTER");
		lif->eth_dev->data->dev_conf.rxmode.offloads |=
			RTE_ETH_RX_OFFLOAD_SCATTER;
		ionic_lif_configure_rx_sg_offload(lif);
	}

	if (lif->features & IONIC_ETH_HW_RX_SG) {
		flags |= IONIC_QCQ_F_SG;
		max_segs_fw = IONIC_RX_MAX_SG_ELEMS + 1;
	}

	/*
	 * Calculate how many fragment pointers might be stored in queue.
	 * This is the worst-case number, so that there's enough room in
	 * the info array.
	 */
	max_segs = 1 + (max_mtu + RTE_PKTMBUF_HEADROOM - 1) / seg_size;

	IONIC_PRINT(DEBUG, "rxq %u max_mtu %u seg_size %u max_segs %u",
		index, max_mtu, seg_size, max_segs);
	if (max_segs > max_segs_fw) {
		IONIC_PRINT(ERR, "Rx mbuf size insufficient (%d > %d avail)",
			max_segs, max_segs_fw);
		return -EINVAL;
	}

	err = ionic_qcq_alloc(lif,
		IONIC_QTYPE_RXQ,
		sizeof(struct ionic_rx_qcq),
		socket_id,
		index,
		"rx",
		flags,
		nrxq_descs,
		max_segs,
		sizeof(struct ionic_rxq_desc),
		sizeof(struct ionic_rxq_comp),
		sizeof(struct ionic_rxq_sg_desc),
		(struct ionic_qcq **)&rxq);
	if (err)
		return err;

	rxq->flags = flags;
	rxq->seg_size = seg_size;
	rxq->hdr_seg_size = hdr_seg_size;
	rxq->rearm_data = ionic_rx_rearm_data(lif);
	rxq->rearm_seg_data = ionic_rx_seg_rearm_data(lif);

	lif->rxqcqs[index] = rxq;
	*rxq_out = rxq;

	return 0;
}

int
ionic_tx_qcq_alloc(struct ionic_lif *lif, uint32_t socket_id, uint32_t index,
		uint16_t ntxq_descs, struct ionic_tx_qcq **txq_out)
{
	struct ionic_tx_qcq *txq;
	uint16_t flags = 0, num_segs_fw = 1;
	int err;

	if (lif->features & IONIC_ETH_HW_TX_SG) {
		flags |= IONIC_QCQ_F_SG;
		num_segs_fw = IONIC_TX_MAX_SG_ELEMS_V1 + 1;
	}
	if (lif->state & IONIC_LIF_F_Q_IN_CMB)
		flags |= IONIC_QCQ_F_CMB;

	IONIC_PRINT(DEBUG, "txq %u num_segs %u", index, num_segs_fw);

	err = ionic_qcq_alloc(lif,
		IONIC_QTYPE_TXQ,
		sizeof(struct ionic_tx_qcq),
		socket_id,
		index,
		"tx",
		flags,
		ntxq_descs,
		num_segs_fw,
		sizeof(struct ionic_txq_desc),
		sizeof(struct ionic_txq_comp),
		sizeof(struct ionic_txq_sg_desc_v1),
		(struct ionic_qcq **)&txq);
	if (err)
		return err;

	txq->flags = flags;
	txq->num_segs_fw = num_segs_fw;

	lif->txqcqs[index] = txq;
	*txq_out = txq;

	return 0;
}

static int
ionic_admin_qcq_alloc(struct ionic_lif *lif)
{
	uint16_t flags = 0;
	int err;

	err = ionic_qcq_alloc(lif,
		IONIC_QTYPE_ADMINQ,
		sizeof(struct ionic_admin_qcq),
		rte_socket_id(),
		0,
		"admin",
		flags,
		IONIC_ADMINQ_LENGTH,
		1,
		sizeof(struct ionic_admin_cmd),
		sizeof(struct ionic_admin_comp),
		0,
		(struct ionic_qcq **)&lif->adminqcq);
	if (err)
		return err;

	return 0;
}

static int
ionic_notify_qcq_alloc(struct ionic_lif *lif)
{
	struct ionic_notify_qcq *nqcq;
	struct ionic_dev *idev = &lif->adapter->idev;
	uint16_t flags = 0;
	int err;

	err = ionic_qcq_alloc(lif,
		IONIC_QTYPE_NOTIFYQ,
		sizeof(struct ionic_notify_qcq),
		rte_socket_id(),
		0,
		"notify",
		flags,
		IONIC_NOTIFYQ_LENGTH,
		1,
		sizeof(struct ionic_notifyq_cmd),
		sizeof(union ionic_notifyq_comp),
		0,
		(struct ionic_qcq **)&nqcq);
	if (err)
		return err;

	err = ionic_intr_alloc(lif, &nqcq->intr);
	if (err) {
		ionic_qcq_free(&nqcq->qcq);
		return err;
	}

	ionic_intr_mask_assert(idev->intr_ctrl, nqcq->intr.index,
		IONIC_INTR_MASK_SET);

	lif->notifyqcq = nqcq;

	return 0;
}

static void
ionic_lif_queue_identify(struct ionic_lif *lif)
{
	struct ionic_adapter *adapter = lif->adapter;
	struct ionic_dev *idev = &adapter->idev;
	union ionic_q_identity *q_ident = &adapter->ident.txq;
	uint32_t q_words = RTE_DIM(q_ident->words);
	uint32_t cmd_words = RTE_DIM(idev->dev_cmd->data);
	uint32_t i, nwords, qtype;
	int err;

	for (qtype = 0; qtype < RTE_DIM(ionic_qtype_vers); qtype++) {
		struct ionic_qtype_info *qti = &lif->qtype_info[qtype];

		/* Filter out the types this driver knows about */
		switch (qtype) {
		case IONIC_QTYPE_ADMINQ:
		case IONIC_QTYPE_NOTIFYQ:
		case IONIC_QTYPE_RXQ:
		case IONIC_QTYPE_TXQ:
			break;
		default:
			continue;
		}

		memset(qti, 0, sizeof(*qti));

		ionic_dev_cmd_queue_identify(idev, IONIC_LIF_TYPE_CLASSIC,
			qtype, ionic_qtype_vers[qtype]);
		err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
		if (err == -EINVAL) {
			IONIC_PRINT(ERR, "qtype %d not supported\n", qtype);
			continue;
		} else if (err == -EIO) {
			IONIC_PRINT(ERR, "q_ident failed, older FW\n");
			return;
		} else if (err) {
			IONIC_PRINT(ERR, "q_ident failed, qtype %d: %d\n",
				qtype, err);
			return;
		}

		nwords = RTE_MIN(q_words, cmd_words);
		for (i = 0; i < nwords; i++)
			q_ident->words[i] = ioread32(&idev->dev_cmd->data[i]);

		qti->version   = q_ident->version;
		qti->supported = q_ident->supported;
		qti->features  = rte_le_to_cpu_64(q_ident->features);
		qti->desc_sz   = rte_le_to_cpu_16(q_ident->desc_sz);
		qti->comp_sz   = rte_le_to_cpu_16(q_ident->comp_sz);
		qti->sg_desc_sz   = rte_le_to_cpu_16(q_ident->sg_desc_sz);
		qti->max_sg_elems = rte_le_to_cpu_16(q_ident->max_sg_elems);
		qti->sg_desc_stride =
			rte_le_to_cpu_16(q_ident->sg_desc_stride);

		IONIC_PRINT(DEBUG, " qtype[%d].version = %d",
			qtype, qti->version);
		IONIC_PRINT(DEBUG, " qtype[%d].supported = %#x",
			qtype, qti->supported);
		IONIC_PRINT(DEBUG, " qtype[%d].features = %#jx",
			qtype, qti->features);
		IONIC_PRINT(DEBUG, " qtype[%d].desc_sz = %d",
			qtype, qti->desc_sz);
		IONIC_PRINT(DEBUG, " qtype[%d].comp_sz = %d",
			qtype, qti->comp_sz);
		IONIC_PRINT(DEBUG, " qtype[%d].sg_desc_sz = %d",
			qtype, qti->sg_desc_sz);
		IONIC_PRINT(DEBUG, " qtype[%d].max_sg_elems = %d",
			qtype, qti->max_sg_elems);
		IONIC_PRINT(DEBUG, " qtype[%d].sg_desc_stride = %d",
			qtype, qti->sg_desc_stride);
	}
}

int
ionic_lif_alloc(struct ionic_lif *lif)
{
	struct ionic_adapter *adapter = lif->adapter;
	uint32_t socket_id = rte_socket_id();
	int err;

	/*
	 * lif->name was zeroed on allocation.
	 * Copy (sizeof() - 1) bytes to ensure that it is NULL terminated.
	 */
	memcpy(lif->name, lif->eth_dev->data->name, sizeof(lif->name) - 1);

	IONIC_PRINT(DEBUG, "LIF: %s", lif->name);

	ionic_lif_queue_identify(lif);

	if (lif->qtype_info[IONIC_QTYPE_TXQ].version < 1) {
		IONIC_PRINT(ERR, "FW too old, please upgrade");
		return -ENXIO;
	}

	if (adapter->q_in_cmb) {
		if (adapter->bars.num_bars >= 3 &&
		    lif->qtype_info[IONIC_QTYPE_RXQ].version >= 2 &&
		    lif->qtype_info[IONIC_QTYPE_TXQ].version >= 3) {
			IONIC_PRINT(INFO, "%s enabled on %s",
				PMD_IONIC_CMB_KVARG, lif->name);
			lif->state |= IONIC_LIF_F_Q_IN_CMB;
		} else {
			IONIC_PRINT(ERR, "%s not supported on %s, disabled",
				PMD_IONIC_CMB_KVARG, lif->name);
		}
	}

	IONIC_PRINT(DEBUG, "Allocating Lif Info");

	rte_spinlock_init(&lif->adminq_lock);
	rte_spinlock_init(&lif->adminq_service_lock);

	lif->kern_dbpage = adapter->idev.db_pages;
	if (!lif->kern_dbpage) {
		IONIC_PRINT(ERR, "Cannot map dbpage, aborting");
		return -ENOMEM;
	}

	lif->txqcqs = rte_calloc_socket("ionic",
				adapter->max_ntxqs_per_lif,
				sizeof(*lif->txqcqs),
				RTE_CACHE_LINE_SIZE, socket_id);
	if (!lif->txqcqs) {
		IONIC_PRINT(ERR, "Cannot allocate tx queues array");
		return -ENOMEM;
	}

	lif->rxqcqs = rte_calloc_socket("ionic",
				adapter->max_nrxqs_per_lif,
				sizeof(*lif->rxqcqs),
				RTE_CACHE_LINE_SIZE, socket_id);
	if (!lif->rxqcqs) {
		IONIC_PRINT(ERR, "Cannot allocate rx queues array");
		return -ENOMEM;
	}

	IONIC_PRINT(DEBUG, "Allocating Notify Queue");

	err = ionic_notify_qcq_alloc(lif);
	if (err) {
		IONIC_PRINT(ERR, "Cannot allocate notify queue");
		return err;
	}

	IONIC_PRINT(DEBUG, "Allocating Admin Queue");

	err = ionic_admin_qcq_alloc(lif);
	if (err) {
		IONIC_PRINT(ERR, "Cannot allocate admin queue");
		return err;
	}

	IONIC_PRINT(DEBUG, "Allocating Lif Info");

	lif->info_sz = RTE_ALIGN(sizeof(*lif->info), rte_mem_page_size());

	lif->info_z = rte_eth_dma_zone_reserve(lif->eth_dev,
		"lif_info", 0 /* queue_idx*/,
		lif->info_sz, IONIC_ALIGN, socket_id);
	if (!lif->info_z) {
		IONIC_PRINT(ERR, "Cannot allocate lif info memory");
		return -ENOMEM;
	}

	lif->info = lif->info_z->addr;
	lif->info_pa = lif->info_z->iova;

	return 0;
}

void
ionic_lif_free(struct ionic_lif *lif)
{
	if (lif->notifyqcq) {
		ionic_qcq_free(&lif->notifyqcq->qcq);
		lif->notifyqcq = NULL;
	}

	if (lif->adminqcq) {
		ionic_qcq_free(&lif->adminqcq->qcq);
		lif->adminqcq = NULL;
	}

	if (lif->txqcqs) {
		rte_free(lif->txqcqs);
		lif->txqcqs = NULL;
	}

	if (lif->rxqcqs) {
		rte_free(lif->rxqcqs);
		lif->rxqcqs = NULL;
	}

	if (lif->info) {
		rte_memzone_free(lif->info_z);
		lif->info = NULL;
	}
}

void
ionic_lif_free_queues(struct ionic_lif *lif)
{
	uint32_t i;

	for (i = 0; i < lif->ntxqcqs; i++) {
		ionic_dev_tx_queue_release(lif->eth_dev, i);
		lif->eth_dev->data->tx_queues[i] = NULL;
	}
	for (i = 0; i < lif->nrxqcqs; i++) {
		ionic_dev_rx_queue_release(lif->eth_dev, i);
		lif->eth_dev->data->rx_queues[i] = NULL;
	}
}

int
ionic_lif_rss_config(struct ionic_lif *lif,
		const uint16_t types, const uint8_t *key, const uint32_t *indir)
{
	struct ionic_adapter *adapter = lif->adapter;
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.lif_setattr = {
			.opcode = IONIC_CMD_LIF_SETATTR,
			.attr = IONIC_LIF_ATTR_RSS,
			.rss.types = rte_cpu_to_le_16(types),
			.rss.addr = rte_cpu_to_le_64(lif->rss_ind_tbl_pa),
		},
	};
	unsigned int i;
	uint16_t tbl_sz =
		rte_le_to_cpu_16(adapter->ident.lif.eth.rss_ind_tbl_sz);

	IONIC_PRINT_CALL();

	lif->rss_types = types;

	if (key)
		memcpy(lif->rss_hash_key, key, IONIC_RSS_HASH_KEY_SIZE);

	if (indir)
		for (i = 0; i < tbl_sz; i++)
			lif->rss_ind_tbl[i] = indir[i];

	memcpy(ctx.cmd.lif_setattr.rss.key, lif->rss_hash_key,
	       IONIC_RSS_HASH_KEY_SIZE);

	return ionic_adminq_post_wait(lif, &ctx);
}

static int
ionic_lif_rss_setup(struct ionic_lif *lif)
{
	struct ionic_adapter *adapter = lif->adapter;
	static const uint8_t toeplitz_symmetric_key[] = {
		0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
		0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
		0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
		0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
		0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
	};
	uint32_t i;
	uint16_t tbl_sz =
		rte_le_to_cpu_16(adapter->ident.lif.eth.rss_ind_tbl_sz);

	IONIC_PRINT_CALL();

	if (!lif->rss_ind_tbl_z) {
		lif->rss_ind_tbl_z = rte_eth_dma_zone_reserve(lif->eth_dev,
					"rss_ind_tbl", 0 /* queue_idx */,
					sizeof(*lif->rss_ind_tbl) * tbl_sz,
					IONIC_ALIGN, rte_socket_id());
		if (!lif->rss_ind_tbl_z) {
			IONIC_PRINT(ERR, "OOM");
			return -ENOMEM;
		}

		lif->rss_ind_tbl = lif->rss_ind_tbl_z->addr;
		lif->rss_ind_tbl_pa = lif->rss_ind_tbl_z->iova;
	}

	if (lif->rss_ind_tbl_nrxqcqs != lif->nrxqcqs) {
		lif->rss_ind_tbl_nrxqcqs = lif->nrxqcqs;

		/* Fill indirection table with 'default' values */
		for (i = 0; i < tbl_sz; i++)
			lif->rss_ind_tbl[i] = i % lif->nrxqcqs;
	}

	return ionic_lif_rss_config(lif, IONIC_RSS_OFFLOAD_ALL,
			toeplitz_symmetric_key, NULL);
}

static void
ionic_lif_rss_teardown(struct ionic_lif *lif)
{
	if (!lif->rss_ind_tbl)
		return;

	if (lif->rss_ind_tbl_z) {
		/* Disable RSS on the NIC */
		ionic_lif_rss_config(lif, 0x0, NULL, NULL);

		lif->rss_ind_tbl = NULL;
		lif->rss_ind_tbl_pa = 0;
		rte_memzone_free(lif->rss_ind_tbl_z);
		lif->rss_ind_tbl_z = NULL;
	}
}

void
ionic_lif_txq_deinit(struct ionic_tx_qcq *txq)
{
	ionic_qcq_disable(&txq->qcq);

	txq->flags &= ~IONIC_QCQ_F_INITED;
}

void
ionic_lif_rxq_deinit(struct ionic_rx_qcq *rxq)
{
	ionic_qcq_disable(&rxq->qcq);

	rxq->flags &= ~IONIC_QCQ_F_INITED;
}

static void
ionic_lif_adminq_deinit(struct ionic_lif *lif)
{
	lif->adminqcq->flags &= ~IONIC_QCQ_F_INITED;
}

static void
ionic_lif_notifyq_deinit(struct ionic_lif *lif)
{
	struct ionic_notify_qcq *nqcq = lif->notifyqcq;
	struct ionic_dev *idev = &lif->adapter->idev;

	if (!(nqcq->flags & IONIC_QCQ_F_INITED))
		return;

	ionic_intr_mask(idev->intr_ctrl, nqcq->intr.index,
		IONIC_INTR_MASK_SET);

	nqcq->flags &= ~IONIC_QCQ_F_INITED;
}

/* This acts like ionic_napi */
int
ionic_qcq_service(struct ionic_qcq *qcq, int budget, ionic_cq_cb cb,
		void *cb_arg)
{
	struct ionic_cq *cq = &qcq->cq;
	uint32_t work_done;

	work_done = ionic_cq_service(cq, budget, cb, cb_arg);

	return work_done;
}

static void
ionic_link_status_check(struct ionic_lif *lif)
{
	struct ionic_adapter *adapter = lif->adapter;
	bool link_up;

	lif->state &= ~IONIC_LIF_F_LINK_CHECK_NEEDED;

	if (!lif->info)
		return;

	link_up = (lif->info->status.link_status == IONIC_PORT_OPER_STATUS_UP);

	if ((link_up  && adapter->link_up) ||
	    (!link_up && !adapter->link_up))
		return;

	if (link_up) {
		adapter->link_speed =
			rte_le_to_cpu_32(lif->info->status.link_speed);
		IONIC_PRINT(DEBUG, "Link up - %d Gbps",
			adapter->link_speed);
	} else {
		IONIC_PRINT(DEBUG, "Link down");
	}

	adapter->link_up = link_up;
	ionic_dev_link_update(lif->eth_dev, 0);
}

static void
ionic_lif_handle_fw_down(struct ionic_lif *lif)
{
	if (lif->state & IONIC_LIF_F_FW_RESET)
		return;

	lif->state |= IONIC_LIF_F_FW_RESET;

	if (lif->state & IONIC_LIF_F_UP) {
		IONIC_PRINT(NOTICE,
			"Surprise FW stop, stopping %s\n", lif->name);
		ionic_lif_stop(lif);
	}

	IONIC_PRINT(NOTICE, "FW down, %s stopped", lif->name);
}

static bool
ionic_notifyq_cb(struct ionic_cq *cq, uint16_t cq_desc_index, void *cb_arg)
{
	union ionic_notifyq_comp *cq_desc_base = cq->base;
	union ionic_notifyq_comp *cq_desc = &cq_desc_base[cq_desc_index];
	struct ionic_lif *lif = cb_arg;

	IONIC_PRINT(DEBUG, "Notifyq callback eid = %jd ecode = %d",
		cq_desc->event.eid, cq_desc->event.ecode);

	/* Have we run out of new completions to process? */
	if (!(cq_desc->event.eid > lif->last_eid))
		return false;

	lif->last_eid = cq_desc->event.eid;

	switch (cq_desc->event.ecode) {
	case IONIC_EVENT_LINK_CHANGE:
		IONIC_PRINT(DEBUG,
			"Notifyq IONIC_EVENT_LINK_CHANGE %s "
			"eid=%jd link_status=%d link_speed=%d",
			lif->name,
			cq_desc->event.eid,
			cq_desc->link_change.link_status,
			cq_desc->link_change.link_speed);

		lif->state |= IONIC_LIF_F_LINK_CHECK_NEEDED;
		break;

	case IONIC_EVENT_RESET:
		IONIC_PRINT(NOTICE,
			"Notifyq IONIC_EVENT_RESET %s "
			"eid=%jd, reset_code=%d state=%d",
			lif->name,
			cq_desc->event.eid,
			cq_desc->reset.reset_code,
			cq_desc->reset.state);
		ionic_lif_handle_fw_down(lif);
		break;

	default:
		IONIC_PRINT(WARNING, "Notifyq bad event ecode=%d eid=%jd",
			cq_desc->event.ecode, cq_desc->event.eid);
		break;
	}

	return true;
}

int
ionic_notifyq_handler(struct ionic_lif *lif, int budget)
{
	struct ionic_dev *idev = &lif->adapter->idev;
	struct ionic_notify_qcq *nqcq = lif->notifyqcq;
	uint32_t work_done;

	if (!(nqcq->flags & IONIC_QCQ_F_INITED)) {
		IONIC_PRINT(DEBUG, "Notifyq not yet initialized");
		return -1;
	}

	ionic_intr_mask(idev->intr_ctrl, nqcq->intr.index,
		IONIC_INTR_MASK_SET);

	work_done = ionic_qcq_service(&nqcq->qcq, budget,
				ionic_notifyq_cb, lif);

	if (lif->state & IONIC_LIF_F_LINK_CHECK_NEEDED)
		ionic_link_status_check(lif);

	ionic_intr_credits(idev->intr_ctrl, nqcq->intr.index,
		work_done, IONIC_INTR_CRED_RESET_COALESCE);

	ionic_intr_mask(idev->intr_ctrl, nqcq->intr.index,
		IONIC_INTR_MASK_CLEAR);

	return 0;
}

static int
ionic_lif_adminq_init(struct ionic_lif *lif)
{
	struct ionic_dev *idev = &lif->adapter->idev;
	struct ionic_admin_qcq *aqcq = lif->adminqcq;
	struct ionic_queue *q = &aqcq->qcq.q;
	struct ionic_q_init_comp comp;
	uint32_t retries = 5;
	int err;

retry_adminq_init:
	ionic_dev_cmd_adminq_init(idev, &aqcq->qcq);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	if (err == -EAGAIN && retries > 0) {
		retries--;
		rte_delay_us_block(IONIC_DEVCMD_RETRY_WAIT_US);
		goto retry_adminq_init;
	}
	if (err)
		return err;

	ionic_dev_cmd_comp(idev, &comp);

	q->hw_type = comp.hw_type;
	q->hw_index = rte_le_to_cpu_32(comp.hw_index);
	q->db = ionic_db_map(lif, q);

	IONIC_PRINT(DEBUG, "adminq->hw_type %d", q->hw_type);
	IONIC_PRINT(DEBUG, "adminq->hw_index %d", q->hw_index);
	IONIC_PRINT(DEBUG, "adminq->db %p", q->db);

	aqcq->flags |= IONIC_QCQ_F_INITED;

	return 0;
}

static int
ionic_lif_notifyq_init(struct ionic_lif *lif)
{
	struct ionic_dev *idev = &lif->adapter->idev;
	struct ionic_notify_qcq *nqcq = lif->notifyqcq;
	struct ionic_queue *q = &nqcq->qcq.q;
	uint16_t flags = IONIC_QINIT_F_ENA;
	int err;

	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.q_init = {
			.opcode = IONIC_CMD_Q_INIT,
			.type = q->type,
			.ver = lif->qtype_info[q->type].version,
			.index = rte_cpu_to_le_32(q->index),
			.intr_index = rte_cpu_to_le_16(IONIC_INTR_NONE),
			.ring_size = rte_log2_u32(q->num_descs),
			.ring_base = rte_cpu_to_le_64(q->base_pa),
		}
	};

	/* Only enable an interrupt if the device supports them */
	if (lif->adapter->intf->configure_intr != NULL) {
		flags |= IONIC_QINIT_F_IRQ;
		ctx.cmd.q_init.intr_index = rte_cpu_to_le_16(nqcq->intr.index);
	}
	ctx.cmd.q_init.flags = rte_cpu_to_le_16(flags);

	IONIC_PRINT(DEBUG, "notifyq_init.index %d", q->index);
	IONIC_PRINT(DEBUG, "notifyq_init.ring_base 0x%" PRIx64 "", q->base_pa);
	IONIC_PRINT(DEBUG, "notifyq_init.ring_size %d",
		ctx.cmd.q_init.ring_size);
	IONIC_PRINT(DEBUG, "notifyq_init.ver %u", ctx.cmd.q_init.ver);

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	q->hw_type = ctx.comp.q_init.hw_type;
	q->hw_index = rte_le_to_cpu_32(ctx.comp.q_init.hw_index);
	q->db = NULL;

	IONIC_PRINT(DEBUG, "notifyq->hw_type %d", q->hw_type);
	IONIC_PRINT(DEBUG, "notifyq->hw_index %d", q->hw_index);
	IONIC_PRINT(DEBUG, "notifyq->db %p", q->db);

	ionic_intr_mask(idev->intr_ctrl, nqcq->intr.index,
		IONIC_INTR_MASK_CLEAR);

	nqcq->flags |= IONIC_QCQ_F_INITED;

	return 0;
}

int
ionic_lif_set_features(struct ionic_lif *lif)
{
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.lif_setattr = {
			.opcode = IONIC_CMD_LIF_SETATTR,
			.attr = IONIC_LIF_ATTR_FEATURES,
			.features = rte_cpu_to_le_64(lif->features),
		},
	};
	int err;

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	lif->hw_features = rte_le_to_cpu_64(ctx.cmd.lif_setattr.features &
						ctx.comp.lif_setattr.features);

	if (lif->hw_features & IONIC_ETH_HW_VLAN_TX_TAG)
		IONIC_PRINT(DEBUG, "feature IONIC_ETH_HW_VLAN_TX_TAG");
	if (lif->hw_features & IONIC_ETH_HW_VLAN_RX_STRIP)
		IONIC_PRINT(DEBUG, "feature IONIC_ETH_HW_VLAN_RX_STRIP");
	if (lif->hw_features & IONIC_ETH_HW_VLAN_RX_FILTER)
		IONIC_PRINT(DEBUG, "feature IONIC_ETH_HW_VLAN_RX_FILTER");
	if (lif->hw_features & IONIC_ETH_HW_RX_HASH)
		IONIC_PRINT(DEBUG, "feature IONIC_ETH_HW_RX_HASH");
	if (lif->hw_features & IONIC_ETH_HW_TX_SG)
		IONIC_PRINT(DEBUG, "feature IONIC_ETH_HW_TX_SG");
	if (lif->hw_features & IONIC_ETH_HW_RX_SG)
		IONIC_PRINT(DEBUG, "feature IONIC_ETH_HW_RX_SG");
	if (lif->hw_features & IONIC_ETH_HW_TX_CSUM)
		IONIC_PRINT(DEBUG, "feature IONIC_ETH_HW_TX_CSUM");
	if (lif->hw_features & IONIC_ETH_HW_RX_CSUM)
		IONIC_PRINT(DEBUG, "feature IONIC_ETH_HW_RX_CSUM");
	if (lif->hw_features & IONIC_ETH_HW_TSO)
		IONIC_PRINT(DEBUG, "feature IONIC_ETH_HW_TSO");
	if (lif->hw_features & IONIC_ETH_HW_TSO_IPV6)
		IONIC_PRINT(DEBUG, "feature IONIC_ETH_HW_TSO_IPV6");
	if (lif->hw_features & IONIC_ETH_HW_TSO_ECN)
		IONIC_PRINT(DEBUG, "feature IONIC_ETH_HW_TSO_ECN");
	if (lif->hw_features & IONIC_ETH_HW_TSO_GRE)
		IONIC_PRINT(DEBUG, "feature IONIC_ETH_HW_TSO_GRE");
	if (lif->hw_features & IONIC_ETH_HW_TSO_GRE_CSUM)
		IONIC_PRINT(DEBUG, "feature IONIC_ETH_HW_TSO_GRE_CSUM");
	if (lif->hw_features & IONIC_ETH_HW_TSO_IPXIP4)
		IONIC_PRINT(DEBUG, "feature IONIC_ETH_HW_TSO_IPXIP4");
	if (lif->hw_features & IONIC_ETH_HW_TSO_IPXIP6)
		IONIC_PRINT(DEBUG, "feature IONIC_ETH_HW_TSO_IPXIP6");
	if (lif->hw_features & IONIC_ETH_HW_TSO_UDP)
		IONIC_PRINT(DEBUG, "feature IONIC_ETH_HW_TSO_UDP");
	if (lif->hw_features & IONIC_ETH_HW_TSO_UDP_CSUM)
		IONIC_PRINT(DEBUG, "feature IONIC_ETH_HW_TSO_UDP_CSUM");

	return 0;
}

int
ionic_lif_txq_init(struct ionic_tx_qcq *txq)
{
	struct ionic_qcq *qcq = &txq->qcq;
	struct ionic_queue *q = &qcq->q;
	struct ionic_lif *lif = qcq->lif;
	struct ionic_cq *cq = &qcq->cq;
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.q_init = {
			.opcode = IONIC_CMD_Q_INIT,
			.type = q->type,
			.ver = lif->qtype_info[q->type].version,
			.index = rte_cpu_to_le_32(q->index),
			.flags = rte_cpu_to_le_16(IONIC_QINIT_F_ENA),
			.intr_index = rte_cpu_to_le_16(IONIC_INTR_NONE),
			.ring_size = rte_log2_u32(q->num_descs),
			.ring_base = rte_cpu_to_le_64(q->base_pa),
			.cq_ring_base = rte_cpu_to_le_64(cq->base_pa),
			.sg_ring_base = rte_cpu_to_le_64(q->sg_base_pa),
		},
	};
	int err;

	if (txq->flags & IONIC_QCQ_F_SG)
		ctx.cmd.q_init.flags |= rte_cpu_to_le_16(IONIC_QINIT_F_SG);
	if (txq->flags & IONIC_QCQ_F_CMB)
		ctx.cmd.q_init.flags |= rte_cpu_to_le_16(IONIC_QINIT_F_CMB);

	IONIC_PRINT(DEBUG, "txq_init.index %d", q->index);
	IONIC_PRINT(DEBUG, "txq_init.ring_base 0x%" PRIx64 "", q->base_pa);
	IONIC_PRINT(DEBUG, "txq_init.ring_size %d",
		ctx.cmd.q_init.ring_size);
	IONIC_PRINT(DEBUG, "txq_init.ver %u", ctx.cmd.q_init.ver);

	ionic_q_reset(q);
	ionic_cq_reset(cq);

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	q->hw_type = ctx.comp.q_init.hw_type;
	q->hw_index = rte_le_to_cpu_32(ctx.comp.q_init.hw_index);
	q->db = ionic_db_map(lif, q);

	IONIC_PRINT(DEBUG, "txq->hw_type %d", q->hw_type);
	IONIC_PRINT(DEBUG, "txq->hw_index %d", q->hw_index);
	IONIC_PRINT(DEBUG, "txq->db %p", q->db);

	txq->flags |= IONIC_QCQ_F_INITED;

	return 0;
}

int
ionic_lif_rxq_init(struct ionic_rx_qcq *rxq)
{
	struct ionic_qcq *qcq = &rxq->qcq;
	struct ionic_queue *q = &qcq->q;
	struct ionic_lif *lif = qcq->lif;
	struct ionic_cq *cq = &qcq->cq;
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.q_init = {
			.opcode = IONIC_CMD_Q_INIT,
			.type = q->type,
			.ver = lif->qtype_info[q->type].version,
			.index = rte_cpu_to_le_32(q->index),
			.flags = rte_cpu_to_le_16(IONIC_QINIT_F_ENA),
			.intr_index = rte_cpu_to_le_16(IONIC_INTR_NONE),
			.ring_size = rte_log2_u32(q->num_descs),
			.ring_base = rte_cpu_to_le_64(q->base_pa),
			.cq_ring_base = rte_cpu_to_le_64(cq->base_pa),
			.sg_ring_base = rte_cpu_to_le_64(q->sg_base_pa),
		},
	};
	int err;

	if (rxq->flags & IONIC_QCQ_F_SG)
		ctx.cmd.q_init.flags |= rte_cpu_to_le_16(IONIC_QINIT_F_SG);
	if (rxq->flags & IONIC_QCQ_F_CMB)
		ctx.cmd.q_init.flags |= rte_cpu_to_le_16(IONIC_QINIT_F_CMB);

	IONIC_PRINT(DEBUG, "rxq_init.index %d", q->index);
	IONIC_PRINT(DEBUG, "rxq_init.ring_base 0x%" PRIx64 "", q->base_pa);
	IONIC_PRINT(DEBUG, "rxq_init.ring_size %d",
		ctx.cmd.q_init.ring_size);
	IONIC_PRINT(DEBUG, "rxq_init.ver %u", ctx.cmd.q_init.ver);

	ionic_q_reset(q);
	ionic_cq_reset(cq);

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	q->hw_type = ctx.comp.q_init.hw_type;
	q->hw_index = rte_le_to_cpu_32(ctx.comp.q_init.hw_index);
	q->db = ionic_db_map(lif, q);

	rxq->flags |= IONIC_QCQ_F_INITED;

	IONIC_PRINT(DEBUG, "rxq->hw_type %d", q->hw_type);
	IONIC_PRINT(DEBUG, "rxq->hw_index %d", q->hw_index);
	IONIC_PRINT(DEBUG, "rxq->db %p", q->db);

	return 0;
}

static int
ionic_station_set(struct ionic_lif *lif)
{
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.lif_getattr = {
			.opcode = IONIC_CMD_LIF_GETATTR,
			.attr = IONIC_LIF_ATTR_MAC,
		},
	};
	int err;

	IONIC_PRINT_CALL();

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	memcpy(lif->mac_addr, ctx.comp.lif_getattr.mac, RTE_ETHER_ADDR_LEN);

	return 0;
}

static void
ionic_lif_set_name(struct ionic_lif *lif)
{
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.lif_setattr = {
			.opcode = IONIC_CMD_LIF_SETATTR,
			.attr = IONIC_LIF_ATTR_NAME,
		},
	};

	memcpy(ctx.cmd.lif_setattr.name, lif->name,
		sizeof(ctx.cmd.lif_setattr.name) - 1);

	ionic_adminq_post_wait(lif, &ctx);
}

int
ionic_lif_init(struct ionic_lif *lif)
{
	struct ionic_dev *idev = &lif->adapter->idev;
	struct ionic_lif_init_comp comp;
	uint32_t retries = 5;
	int err;

	memset(&lif->stats_base, 0, sizeof(lif->stats_base));

retry_lif_init:
	ionic_dev_cmd_lif_init(idev, lif->info_pa);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	if (err == -EAGAIN && retries > 0) {
		retries--;
		rte_delay_us_block(IONIC_DEVCMD_RETRY_WAIT_US);
		goto retry_lif_init;
	}
	if (err)
		return err;

	ionic_dev_cmd_comp(idev, &comp);

	lif->hw_index = rte_cpu_to_le_16(comp.hw_index);

	err = ionic_lif_adminq_init(lif);
	if (err)
		return err;

	err = ionic_lif_notifyq_init(lif);
	if (err)
		goto err_out_adminq_deinit;

	/*
	 * Configure initial feature set
	 * This will be updated later by the dev_configure() step
	 */
	lif->features = IONIC_ETH_HW_RX_HASH | IONIC_ETH_HW_VLAN_RX_FILTER;

	err = ionic_lif_set_features(lif);
	if (err)
		goto err_out_notifyq_deinit;

	err = ionic_rx_filters_init(lif);
	if (err)
		goto err_out_notifyq_deinit;

	err = ionic_station_set(lif);
	if (err)
		goto err_out_rx_filter_deinit;

	ionic_lif_set_name(lif);

	lif->state |= IONIC_LIF_F_INITED;

	return 0;

err_out_rx_filter_deinit:
	ionic_rx_filters_deinit(lif);

err_out_notifyq_deinit:
	ionic_lif_notifyq_deinit(lif);

err_out_adminq_deinit:
	ionic_lif_adminq_deinit(lif);

	return err;
}

void
ionic_lif_deinit(struct ionic_lif *lif)
{
	if (!(lif->state & IONIC_LIF_F_INITED))
		return;

	ionic_rx_filters_deinit(lif);
	ionic_lif_rss_teardown(lif);
	ionic_lif_notifyq_deinit(lif);
	ionic_lif_adminq_deinit(lif);

	lif->state &= ~IONIC_LIF_F_INITED;
}

void
ionic_lif_configure_vlan_offload(struct ionic_lif *lif, int mask)
{
	struct rte_eth_dev *eth_dev = lif->eth_dev;
	struct rte_eth_rxmode *rxmode = &eth_dev->data->dev_conf.rxmode;

	/*
	 * IONIC_ETH_HW_VLAN_RX_FILTER cannot be turned off, so
	 * set RTE_ETH_RX_OFFLOAD_VLAN_FILTER and ignore RTE_ETH_VLAN_FILTER_MASK
	 */
	rxmode->offloads |= RTE_ETH_RX_OFFLOAD_VLAN_FILTER;

	if (mask & RTE_ETH_VLAN_STRIP_MASK) {
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
			lif->features |= IONIC_ETH_HW_VLAN_RX_STRIP;
		else
			lif->features &= ~IONIC_ETH_HW_VLAN_RX_STRIP;
	}
}

void
ionic_lif_configure_rx_sg_offload(struct ionic_lif *lif)
{
	struct rte_eth_rxmode *rxmode = &lif->eth_dev->data->dev_conf.rxmode;

	if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_SCATTER) {
		lif->features |= IONIC_ETH_HW_RX_SG;
		lif->eth_dev->data->scattered_rx = 1;
	} else {
		lif->features &= ~IONIC_ETH_HW_RX_SG;
		lif->eth_dev->data->scattered_rx = 0;
	}
}

void
ionic_lif_configure(struct ionic_lif *lif)
{
	struct rte_eth_rxmode *rxmode = &lif->eth_dev->data->dev_conf.rxmode;
	struct rte_eth_txmode *txmode = &lif->eth_dev->data->dev_conf.txmode;
	struct ionic_identity *ident = &lif->adapter->ident;
	union ionic_lif_config *cfg = &ident->lif.eth.config;
	uint32_t ntxqs_per_lif =
		rte_le_to_cpu_32(cfg->queue_count[IONIC_QTYPE_TXQ]);
	uint32_t nrxqs_per_lif =
		rte_le_to_cpu_32(cfg->queue_count[IONIC_QTYPE_RXQ]);
	uint32_t nrxqs = lif->eth_dev->data->nb_rx_queues;
	uint32_t ntxqs = lif->eth_dev->data->nb_tx_queues;

	lif->port_id = lif->eth_dev->data->port_id;

	IONIC_PRINT(DEBUG, "Configuring LIF on port %u",
		lif->port_id);

	if (nrxqs > 0)
		nrxqs_per_lif = RTE_MIN(nrxqs_per_lif, nrxqs);

	if (ntxqs > 0)
		ntxqs_per_lif = RTE_MIN(ntxqs_per_lif, ntxqs);

	lif->nrxqcqs = nrxqs_per_lif;
	lif->ntxqcqs = ntxqs_per_lif;

	/* Update the LIF configuration based on the eth_dev */

	/*
	 * NB: While it is true that RSS_HASH is always enabled on ionic,
	 *     setting this flag unconditionally causes problems in DTS.
	 * rxmode->offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;
	 */

	/* RX per-port */

	if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_IPV4_CKSUM ||
	    rxmode->offloads & RTE_ETH_RX_OFFLOAD_UDP_CKSUM ||
	    rxmode->offloads & RTE_ETH_RX_OFFLOAD_TCP_CKSUM)
		lif->features |= IONIC_ETH_HW_RX_CSUM;
	else
		lif->features &= ~IONIC_ETH_HW_RX_CSUM;

	/*
	 * NB: RX_SG may be enabled later during rx_queue_setup() if
	 * required by the mbuf/mtu configuration
	 */
	ionic_lif_configure_rx_sg_offload(lif);

	/* Covers VLAN_STRIP */
	ionic_lif_configure_vlan_offload(lif, RTE_ETH_VLAN_STRIP_MASK);

	/* TX per-port */

	if (txmode->offloads & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM ||
	    txmode->offloads & RTE_ETH_TX_OFFLOAD_UDP_CKSUM ||
	    txmode->offloads & RTE_ETH_TX_OFFLOAD_TCP_CKSUM ||
	    txmode->offloads & RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM ||
	    txmode->offloads & RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM)
		lif->features |= IONIC_ETH_HW_TX_CSUM;
	else
		lif->features &= ~IONIC_ETH_HW_TX_CSUM;

	if (txmode->offloads & RTE_ETH_TX_OFFLOAD_VLAN_INSERT)
		lif->features |= IONIC_ETH_HW_VLAN_TX_TAG;
	else
		lif->features &= ~IONIC_ETH_HW_VLAN_TX_TAG;

	if (txmode->offloads & RTE_ETH_TX_OFFLOAD_MULTI_SEGS)
		lif->features |= IONIC_ETH_HW_TX_SG;
	else
		lif->features &= ~IONIC_ETH_HW_TX_SG;

	if (txmode->offloads & RTE_ETH_TX_OFFLOAD_TCP_TSO) {
		lif->features |= IONIC_ETH_HW_TSO;
		lif->features |= IONIC_ETH_HW_TSO_IPV6;
		lif->features |= IONIC_ETH_HW_TSO_ECN;
	} else {
		lif->features &= ~IONIC_ETH_HW_TSO;
		lif->features &= ~IONIC_ETH_HW_TSO_IPV6;
		lif->features &= ~IONIC_ETH_HW_TSO_ECN;
	}
}

int
ionic_lif_start(struct ionic_lif *lif)
{
	uint32_t rx_mode;
	uint32_t i;
	int err;

	err = ionic_lif_rss_setup(lif);
	if (err)
		return err;

	if (!lif->rx_mode) {
		IONIC_PRINT(DEBUG, "Setting RX mode on %s",
			lif->name);

		rx_mode  = IONIC_RX_MODE_F_UNICAST;
		rx_mode |= IONIC_RX_MODE_F_MULTICAST;
		rx_mode |= IONIC_RX_MODE_F_BROADCAST;

		ionic_set_rx_mode(lif, rx_mode);
	}

	IONIC_PRINT(DEBUG, "Starting %u RX queues and %u TX queues "
		"on port %u",
		lif->nrxqcqs, lif->ntxqcqs, lif->port_id);

	for (i = 0; i < lif->nrxqcqs; i++) {
		struct ionic_rx_qcq *rxq = lif->rxqcqs[i];
		if (!(rxq->flags & IONIC_QCQ_F_DEFERRED)) {
			err = ionic_dev_rx_queue_start(lif->eth_dev, i);

			if (err)
				return err;
		}
	}

	for (i = 0; i < lif->ntxqcqs; i++) {
		struct ionic_tx_qcq *txq = lif->txqcqs[i];
		if (!(txq->flags & IONIC_QCQ_F_DEFERRED)) {
			err = ionic_dev_tx_queue_start(lif->eth_dev, i);

			if (err)
				return err;
		}
	}

	/* Carrier ON here */
	lif->state |= IONIC_LIF_F_UP;

	ionic_link_status_check(lif);

	return 0;
}

int
ionic_lif_identify(struct ionic_adapter *adapter)
{
	struct ionic_dev *idev = &adapter->idev;
	struct ionic_identity *ident = &adapter->ident;
	union ionic_lif_config *cfg = &ident->lif.eth.config;
	uint32_t lif_words = RTE_DIM(ident->lif.words);
	uint32_t cmd_words = RTE_DIM(idev->dev_cmd->data);
	uint32_t i, nwords;
	int err;

	ionic_dev_cmd_lif_identify(idev, IONIC_LIF_TYPE_CLASSIC,
		IONIC_IDENTITY_VERSION_1);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	if (err)
		return (err);

	nwords = RTE_MIN(lif_words, cmd_words);
	for (i = 0; i < nwords; i++)
		ident->lif.words[i] = ioread32(&idev->dev_cmd->data[i]);

	IONIC_PRINT(INFO, "capabilities 0x%" PRIx64 " ",
		rte_le_to_cpu_64(ident->lif.capabilities));

	IONIC_PRINT(INFO, "eth.max_ucast_filters 0x%" PRIx32 " ",
		rte_le_to_cpu_32(ident->lif.eth.max_ucast_filters));
	IONIC_PRINT(INFO, "eth.max_mcast_filters 0x%" PRIx32 " ",
		rte_le_to_cpu_32(ident->lif.eth.max_mcast_filters));

	IONIC_PRINT(INFO, "eth.features 0x%" PRIx64 " ",
		rte_le_to_cpu_64(cfg->features));
	IONIC_PRINT(INFO, "eth.queue_count[IONIC_QTYPE_ADMINQ] 0x%" PRIx32 " ",
		rte_le_to_cpu_32(cfg->queue_count[IONIC_QTYPE_ADMINQ]));
	IONIC_PRINT(INFO, "eth.queue_count[IONIC_QTYPE_NOTIFYQ] 0x%" PRIx32 " ",
		rte_le_to_cpu_32(cfg->queue_count[IONIC_QTYPE_NOTIFYQ]));
	IONIC_PRINT(INFO, "eth.queue_count[IONIC_QTYPE_RXQ] 0x%" PRIx32 " ",
		rte_le_to_cpu_32(cfg->queue_count[IONIC_QTYPE_RXQ]));
	IONIC_PRINT(INFO, "eth.queue_count[IONIC_QTYPE_TXQ] 0x%" PRIx32 " ",
		rte_le_to_cpu_32(cfg->queue_count[IONIC_QTYPE_TXQ]));

	return 0;
}

int
ionic_lifs_size(struct ionic_adapter *adapter)
{
	struct ionic_identity *ident = &adapter->ident;
	union ionic_lif_config *cfg = &ident->lif.eth.config;
	uint32_t nintrs, dev_nintrs = rte_le_to_cpu_32(ident->dev.nintrs);

	adapter->max_ntxqs_per_lif =
		rte_le_to_cpu_32(cfg->queue_count[IONIC_QTYPE_TXQ]);
	adapter->max_nrxqs_per_lif =
		rte_le_to_cpu_32(cfg->queue_count[IONIC_QTYPE_RXQ]);

	nintrs = 1 /* notifyq */;

	if (nintrs > dev_nintrs) {
		IONIC_PRINT(ERR,
			"At most %d intr supported, minimum req'd is %u",
			dev_nintrs, nintrs);
		return -ENOSPC;
	}

	adapter->nintrs = nintrs;

	return 0;
}
