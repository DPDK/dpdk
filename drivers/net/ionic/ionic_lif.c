/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#include <rte_malloc.h>
#include <rte_ethdev_driver.h>

#include "ionic.h"
#include "ionic_logs.h"
#include "ionic_lif.h"
#include "ionic_ethdev.h"
#include "ionic_rx_filter.h"
#include "ionic_rxtx.h"

static int ionic_lif_addr_add(struct ionic_lif *lif, const uint8_t *addr);
static int ionic_lif_addr_del(struct ionic_lif *lif, const uint8_t *addr);

int
ionic_qcq_enable(struct ionic_qcq *qcq)
{
	struct ionic_queue *q = &qcq->q;
	struct ionic_lif *lif = q->lif;
	struct ionic_dev *idev = &lif->adapter->idev;
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.q_control = {
			.opcode = IONIC_CMD_Q_CONTROL,
			.lif_index = lif->index,
			.type = q->type,
			.index = q->index,
			.oper = IONIC_Q_ENABLE,
		},
	};

	if (qcq->flags & IONIC_QCQ_F_INTR) {
		ionic_intr_mask(idev->intr_ctrl, qcq->intr.index,
			IONIC_INTR_MASK_CLEAR);
	}

	return ionic_adminq_post_wait(lif, &ctx);
}

int
ionic_qcq_disable(struct ionic_qcq *qcq)
{
	struct ionic_queue *q = &qcq->q;
	struct ionic_lif *lif = q->lif;
	struct ionic_dev *idev = &lif->adapter->idev;
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.q_control = {
			.opcode = IONIC_CMD_Q_CONTROL,
			.lif_index = lif->index,
			.type = q->type,
			.index = q->index,
			.oper = IONIC_Q_DISABLE,
		},
	};

	if (qcq->flags & IONIC_QCQ_F_INTR) {
		ionic_intr_mask(idev->intr_ctrl, qcq->intr.index,
			IONIC_INTR_MASK_SET);
	}

	return ionic_adminq_post_wait(lif, &ctx);
}

int
ionic_lif_stop(struct ionic_lif *lif __rte_unused)
{
	/* Carrier OFF here */

	return 0;
}

void
ionic_lif_reset(struct ionic_lif *lif)
{
	struct ionic_dev *idev = &lif->adapter->idev;

	IONIC_PRINT_CALL();

	ionic_dev_cmd_lif_reset(idev, lif->index);
	ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
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
		struct ionic_rx_stats *rx_stats = &lif->rxqcqs[i]->stats.rx;
		stats->imissed +=
			rx_stats->no_cb_arg +
			rx_stats->bad_cq_status +
			rx_stats->no_room +
			rx_stats->bad_len;
	}

	stats->imissed +=
		ls->rx_ucast_drop_packets +
		ls->rx_mcast_drop_packets +
		ls->rx_bcast_drop_packets;

	stats->imissed +=
		ls->rx_queue_empty +
		ls->rx_dma_error +
		ls->rx_queue_disabled +
		ls->rx_desc_fetch_error +
		ls->rx_desc_data_error;

	for (i = 0; i < num_rx_q_counters; i++) {
		struct ionic_rx_stats *rx_stats = &lif->rxqcqs[i]->stats.rx;
		stats->q_ipackets[i] = rx_stats->packets;
		stats->q_ibytes[i] = rx_stats->bytes;
		stats->q_errors[i] =
			rx_stats->no_cb_arg +
			rx_stats->bad_cq_status +
			rx_stats->no_room +
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
		struct ionic_tx_stats *tx_stats = &lif->txqcqs[i]->stats.tx;
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
		struct ionic_tx_stats *tx_stats = &lif->txqcqs[i]->stats.tx;
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
		memset(&lif->rxqcqs[i]->stats.rx, 0,
			sizeof(struct ionic_rx_stats));
		memset(&lif->txqcqs[i]->stats.tx, 0,
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
			.match = IONIC_RX_FILTER_MATCH_MAC,
		},
	};
	int err;

	memcpy(ctx.cmd.rx_filter_add.mac.addr, addr, RTE_ETHER_ADDR_LEN);

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	IONIC_PRINT(INFO, "rx_filter add (id %d)",
		ctx.comp.rx_filter_add.filter_id);

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

	ctx.cmd.rx_filter_del.filter_id = f->filter_id;
	ionic_rx_filter_free(f);

	rte_spinlock_unlock(&lif->rx_filters.lock);

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	IONIC_PRINT(INFO, "rx_filter del (id %d)",
		ctx.cmd.rx_filter_del.filter_id);

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
ionic_dev_remove_mac(struct rte_eth_dev *eth_dev, uint32_t index __rte_unused)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	struct ionic_adapter *adapter = lif->adapter;

	IONIC_PRINT_CALL();

	if (index >= adapter->max_mac_addrs) {
		IONIC_PRINT(WARNING,
			"Index %u is above MAC filter limit %u",
			index, adapter->max_mac_addrs);
		return;
	}

	if (!rte_is_valid_assigned_ether_addr(&eth_dev->data->mac_addrs[index]))
		return;

	ionic_lif_addr_del(lif, (const uint8_t *)
		&eth_dev->data->mac_addrs[index]);
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
			.match = IONIC_RX_FILTER_MATCH_VLAN,
			.vlan.vlan = vid,
		},
	};
	int err;

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	IONIC_PRINT(INFO, "rx_filter add VLAN %d (id %d)", vid,
		ctx.comp.rx_filter_add.filter_id);

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

	ctx.cmd.rx_filter_del.filter_id = f->filter_id;
	ionic_rx_filter_free(f);
	rte_spinlock_unlock(&lif->rx_filters.lock);

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	IONIC_PRINT(INFO, "rx_filter del VLAN %d (id %d)", vid,
		ctx.cmd.rx_filter_del.filter_id);

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
			.lif_index = lif->index,
			.rx_mode = rx_mode,
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
ionic_lif_change_mtu(struct ionic_lif *lif, int new_mtu)
{
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.lif_setattr = {
			.opcode = IONIC_CMD_LIF_SETATTR,
			.index = lif->index,
			.attr = IONIC_LIF_ATTR_MTU,
			.mtu = new_mtu,
		},
	};
	int err;

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	lif->mtu = new_mtu;

	return 0;
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

void
ionic_intr_free(struct ionic_lif *lif, struct ionic_intr_info *intr)
{
	if (intr->index != IONIC_INTR_INDEX_NOT_ASSIGNED)
		lif->adapter->intrs[intr->index] = false;
}

static int
ionic_qcq_alloc(struct ionic_lif *lif, uint8_t type,
		uint32_t index,
		const char *base, uint32_t flags,
		uint32_t num_descs,
		uint32_t desc_size,
		uint32_t cq_desc_size,
		uint32_t sg_desc_size,
		uint32_t pid, struct ionic_qcq **qcq)
{
	struct ionic_dev *idev = &lif->adapter->idev;
	struct ionic_qcq *new;
	uint32_t q_size, cq_size, sg_size, total_size;
	void *q_base, *cq_base, *sg_base;
	rte_iova_t q_base_pa = 0;
	rte_iova_t cq_base_pa = 0;
	rte_iova_t sg_base_pa = 0;
	uint32_t socket_id = rte_socket_id();
	int err;

	*qcq = NULL;

	q_size  = num_descs * desc_size;
	cq_size = num_descs * cq_desc_size;
	sg_size = num_descs * sg_desc_size;

	total_size = RTE_ALIGN(q_size, PAGE_SIZE) +
		RTE_ALIGN(cq_size, PAGE_SIZE);
	/*
	 * Note: aligning q_size/cq_size is not enough due to cq_base address
	 * aligning as q_base could be not aligned to the page.
	 * Adding PAGE_SIZE.
	 */
	total_size += PAGE_SIZE;

	if (flags & IONIC_QCQ_F_SG) {
		total_size += RTE_ALIGN(sg_size, PAGE_SIZE);
		total_size += PAGE_SIZE;
	}

	new = rte_zmalloc("ionic", sizeof(*new), 0);
	if (!new) {
		IONIC_PRINT(ERR, "Cannot allocate queue structure");
		return -ENOMEM;
	}

	new->lif = lif;
	new->flags = flags;

	new->q.info = rte_zmalloc("ionic", sizeof(*new->q.info) * num_descs, 0);
	if (!new->q.info) {
		IONIC_PRINT(ERR, "Cannot allocate queue info");
		return -ENOMEM;
	}

	new->q.type = type;

	err = ionic_q_init(lif, idev, &new->q, index, num_descs,
		desc_size, sg_desc_size, pid);
	if (err) {
		IONIC_PRINT(ERR, "Queue initialization failed");
		return err;
	}

	if (flags & IONIC_QCQ_F_INTR) {
		err = ionic_intr_alloc(lif, &new->intr);
		if (err)
			return err;

		ionic_intr_mask_assert(idev->intr_ctrl, new->intr.index,
			IONIC_INTR_MASK_SET);
	} else {
		new->intr.index = IONIC_INTR_INDEX_NOT_ASSIGNED;
	}

	err = ionic_cq_init(lif, &new->cq, &new->intr,
		num_descs, cq_desc_size);
	if (err) {
		IONIC_PRINT(ERR, "Completion queue initialization failed");
		goto err_out_free_intr;
	}

	new->base_z = rte_eth_dma_zone_reserve(lif->eth_dev,
		base /* name */, index /* queue_idx */,
		total_size, IONIC_ALIGN, socket_id);

	if (!new->base_z) {
		IONIC_PRINT(ERR, "Cannot reserve queue DMA memory");
		err = -ENOMEM;
		goto err_out_free_intr;
	}

	new->base = new->base_z->addr;
	new->base_pa = new->base_z->iova;
	new->total_size = total_size;

	q_base = new->base;
	q_base_pa = new->base_pa;

	cq_base = (void *)RTE_ALIGN((uintptr_t)q_base + q_size, PAGE_SIZE);
	cq_base_pa = RTE_ALIGN(q_base_pa + q_size, PAGE_SIZE);

	if (flags & IONIC_QCQ_F_SG) {
		sg_base = (void *)RTE_ALIGN((uintptr_t)cq_base + cq_size,
			PAGE_SIZE);
		sg_base_pa = RTE_ALIGN(cq_base_pa + cq_size, PAGE_SIZE);
		ionic_q_sg_map(&new->q, sg_base, sg_base_pa);
	}

	IONIC_PRINT(DEBUG, "Q-Base-PA = %#jx CQ-Base-PA = %#jx "
		"SG-base-PA = %#jx",
		q_base_pa, cq_base_pa, sg_base_pa);

	ionic_q_map(&new->q, q_base, q_base_pa);
	ionic_cq_map(&new->cq, cq_base, cq_base_pa);
	ionic_cq_bind(&new->cq, &new->q);

	*qcq = new;

	return 0;

err_out_free_intr:
	if (flags & IONIC_QCQ_F_INTR)
		ionic_intr_free(lif, &new->intr);

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

int
ionic_rx_qcq_alloc(struct ionic_lif *lif, uint32_t index, uint16_t nrxq_descs,
		struct ionic_qcq **qcq)
{
	uint32_t flags;
	int err = -ENOMEM;

	flags = IONIC_QCQ_F_SG;
	err = ionic_qcq_alloc(lif, IONIC_QTYPE_RXQ, index, "rx", flags,
		nrxq_descs,
		sizeof(struct ionic_rxq_desc),
		sizeof(struct ionic_rxq_comp),
		sizeof(struct ionic_rxq_sg_desc),
		lif->kern_pid, &lif->rxqcqs[index]);
	if (err)
		return err;

	*qcq = lif->rxqcqs[index];

	return 0;
}

int
ionic_tx_qcq_alloc(struct ionic_lif *lif, uint32_t index, uint16_t ntxq_descs,
		struct ionic_qcq **qcq)
{
	uint32_t flags;
	int err = -ENOMEM;

	flags = IONIC_QCQ_F_SG;
	err = ionic_qcq_alloc(lif, IONIC_QTYPE_TXQ, index, "tx", flags,
		ntxq_descs,
		sizeof(struct ionic_txq_desc),
		sizeof(struct ionic_txq_comp),
		sizeof(struct ionic_txq_sg_desc),
		lif->kern_pid, &lif->txqcqs[index]);
	if (err)
		return err;

	*qcq = lif->txqcqs[index];

	return 0;
}

static int
ionic_admin_qcq_alloc(struct ionic_lif *lif)
{
	uint32_t flags;
	int err = -ENOMEM;

	flags = 0;
	err = ionic_qcq_alloc(lif, IONIC_QTYPE_ADMINQ, 0, "admin", flags,
		IONIC_ADMINQ_LENGTH,
		sizeof(struct ionic_admin_cmd),
		sizeof(struct ionic_admin_comp),
		0,
		lif->kern_pid, &lif->adminqcq);
	if (err)
		return err;

	return 0;
}

static int
ionic_notify_qcq_alloc(struct ionic_lif *lif)
{
	uint32_t flags;
	int err = -ENOMEM;

	flags = IONIC_QCQ_F_NOTIFYQ | IONIC_QCQ_F_INTR;

	err = ionic_qcq_alloc(lif, IONIC_QTYPE_NOTIFYQ, 0, "notify",
		flags,
		IONIC_NOTIFYQ_LENGTH,
		sizeof(struct ionic_notifyq_cmd),
		sizeof(union ionic_notifyq_comp),
		0,
		lif->kern_pid, &lif->notifyqcq);
	if (err)
		return err;

	return 0;
}

static void *
ionic_bus_map_dbpage(struct ionic_adapter *adapter, int page_num)
{
	char *vaddr = adapter->bars[IONIC_PCI_BAR_DBELL].vaddr;

	if (adapter->num_bars <= IONIC_PCI_BAR_DBELL)
		return NULL;

	return (void *)&vaddr[page_num << PAGE_SHIFT];
}

int
ionic_lif_alloc(struct ionic_lif *lif)
{
	struct ionic_adapter *adapter = lif->adapter;
	uint32_t socket_id = rte_socket_id();
	int dbpage_num;
	int err;

	/*
	 * lif->name was zeroed on allocation.
	 * Copy (sizeof() - 1) bytes to ensure that it is NULL terminated.
	 */
	memcpy(lif->name, lif->eth_dev->data->name, sizeof(lif->name) - 1);

	IONIC_PRINT(DEBUG, "LIF: %s", lif->name);

	IONIC_PRINT(DEBUG, "Allocating Lif Info");

	rte_spinlock_init(&lif->adminq_lock);
	rte_spinlock_init(&lif->adminq_service_lock);

	lif->kern_pid = 0;

	dbpage_num = ionic_db_page_num(lif, 0);

	lif->kern_dbpage = ionic_bus_map_dbpage(adapter, dbpage_num);
	if (!lif->kern_dbpage) {
		IONIC_PRINT(ERR, "Cannot map dbpage, aborting");
		return -ENOMEM;
	}

	lif->txqcqs = rte_zmalloc("ionic", sizeof(*lif->txqcqs) *
		adapter->max_ntxqs_per_lif, 0);

	if (!lif->txqcqs) {
		IONIC_PRINT(ERR, "Cannot allocate tx queues array");
		return -ENOMEM;
	}

	lif->rxqcqs = rte_zmalloc("ionic", sizeof(*lif->rxqcqs) *
		adapter->max_nrxqs_per_lif, 0);

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

	lif->info_sz = RTE_ALIGN(sizeof(*lif->info), PAGE_SIZE);

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
		ionic_qcq_free(lif->notifyqcq);
		lif->notifyqcq = NULL;
	}

	if (lif->adminqcq) {
		ionic_qcq_free(lif->adminqcq);
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

int
ionic_lif_rss_config(struct ionic_lif *lif,
		const uint16_t types, const uint8_t *key, const uint32_t *indir)
{
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.lif_setattr = {
			.opcode = IONIC_CMD_LIF_SETATTR,
			.attr = IONIC_LIF_ATTR_RSS,
			.rss.types = types,
			.rss.addr = lif->rss_ind_tbl_pa,
		},
	};
	unsigned int i;

	IONIC_PRINT_CALL();

	lif->rss_types = types;

	if (key)
		memcpy(lif->rss_hash_key, key, IONIC_RSS_HASH_KEY_SIZE);

	if (indir)
		for (i = 0; i < lif->adapter->ident.lif.eth.rss_ind_tbl_sz; i++)
			lif->rss_ind_tbl[i] = indir[i];

	memcpy(ctx.cmd.lif_setattr.rss.key, lif->rss_hash_key,
	       IONIC_RSS_HASH_KEY_SIZE);

	return ionic_adminq_post_wait(lif, &ctx);
}

static int
ionic_lif_rss_setup(struct ionic_lif *lif)
{
	size_t tbl_size = sizeof(*lif->rss_ind_tbl) *
		lif->adapter->ident.lif.eth.rss_ind_tbl_sz;
	static const uint8_t toeplitz_symmetric_key[] = {
		0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
		0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
		0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
		0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
		0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
	};
	uint32_t socket_id = rte_socket_id();
	uint32_t i;
	int err;

	IONIC_PRINT_CALL();

	lif->rss_ind_tbl_z = rte_eth_dma_zone_reserve(lif->eth_dev,
		"rss_ind_tbl",
		0 /* queue_idx*/, tbl_size, IONIC_ALIGN, socket_id);

	if (!lif->rss_ind_tbl_z) {
		IONIC_PRINT(ERR, "OOM");
		return -ENOMEM;
	}

	lif->rss_ind_tbl = lif->rss_ind_tbl_z->addr;
	lif->rss_ind_tbl_pa = lif->rss_ind_tbl_z->iova;

	/* Fill indirection table with 'default' values */
	for (i = 0; i < lif->adapter->ident.lif.eth.rss_ind_tbl_sz; i++)
		lif->rss_ind_tbl[i] = i % lif->nrxqcqs;

	err = ionic_lif_rss_config(lif, IONIC_RSS_OFFLOAD_ALL,
		toeplitz_symmetric_key, NULL);
	if (err)
		return err;

	return 0;
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

static void
ionic_lif_qcq_deinit(struct ionic_lif *lif, struct ionic_qcq *qcq)
{
	struct ionic_dev *idev = &lif->adapter->idev;

	if (!(qcq->flags & IONIC_QCQ_F_INITED))
		return;

	if (qcq->flags & IONIC_QCQ_F_INTR)
		ionic_intr_mask(idev->intr_ctrl, qcq->intr.index,
			IONIC_INTR_MASK_SET);

	qcq->flags &= ~IONIC_QCQ_F_INITED;
}

void
ionic_lif_txq_deinit(struct ionic_qcq *qcq)
{
	ionic_lif_qcq_deinit(qcq->lif, qcq);
}

void
ionic_lif_rxq_deinit(struct ionic_qcq *qcq)
{
	ionic_lif_qcq_deinit(qcq->lif, qcq);
}

bool
ionic_adminq_service(struct ionic_cq *cq, uint32_t cq_desc_index,
		void *cb_arg __rte_unused)
{
	struct ionic_admin_comp *cq_desc_base = cq->base;
	struct ionic_admin_comp *cq_desc = &cq_desc_base[cq_desc_index];

	if (!color_match(cq_desc->color, cq->done_color))
		return false;

	ionic_q_service(cq->bound_q, cq_desc_index, cq_desc->comp_index, NULL);

	return true;
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
		IONIC_PRINT(DEBUG, "Link up - %d Gbps",
			lif->info->status.link_speed);
		adapter->link_speed = lif->info->status.link_speed;
	} else {
		IONIC_PRINT(DEBUG, "Link down");
	}

	adapter->link_up = link_up;
}

static bool
ionic_notifyq_cb(struct ionic_cq *cq, uint32_t cq_desc_index, void *cb_arg)
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
			"Notifyq IONIC_EVENT_LINK_CHANGE eid=%jd link_status=%d link_speed=%d",
			cq_desc->event.eid,
			cq_desc->link_change.link_status,
			cq_desc->link_change.link_speed);

		lif->state |= IONIC_LIF_F_LINK_CHECK_NEEDED;

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
	struct ionic_qcq *qcq = lif->notifyqcq;
	uint32_t work_done;

	if (!(qcq->flags & IONIC_QCQ_F_INITED)) {
		IONIC_PRINT(DEBUG, "Notifyq not yet initialized");
		return -1;
	}

	ionic_intr_mask(idev->intr_ctrl, qcq->intr.index,
		IONIC_INTR_MASK_SET);

	work_done = ionic_qcq_service(qcq, budget, ionic_notifyq_cb, lif);

	if (lif->state & IONIC_LIF_F_LINK_CHECK_NEEDED)
		ionic_link_status_check(lif);

	ionic_intr_credits(idev->intr_ctrl, qcq->intr.index,
		work_done, IONIC_INTR_CRED_RESET_COALESCE);

	ionic_intr_mask(idev->intr_ctrl, qcq->intr.index,
		IONIC_INTR_MASK_CLEAR);

	return 0;
}

static int
ionic_lif_adminq_init(struct ionic_lif *lif)
{
	struct ionic_dev *idev = &lif->adapter->idev;
	struct ionic_qcq *qcq = lif->adminqcq;
	struct ionic_queue *q = &qcq->q;
	struct ionic_q_init_comp comp;
	int err;

	ionic_dev_cmd_adminq_init(idev, qcq, lif->index, qcq->intr.index);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	if (err)
		return err;

	ionic_dev_cmd_comp(idev, &comp);

	q->hw_type = comp.hw_type;
	q->hw_index = comp.hw_index;
	q->db = ionic_db_map(lif, q);

	IONIC_PRINT(DEBUG, "adminq->hw_type %d", q->hw_type);
	IONIC_PRINT(DEBUG, "adminq->hw_index %d", q->hw_index);
	IONIC_PRINT(DEBUG, "adminq->db %p", q->db);

	if (qcq->flags & IONIC_QCQ_F_INTR)
		ionic_intr_mask(idev->intr_ctrl, qcq->intr.index,
			IONIC_INTR_MASK_CLEAR);

	qcq->flags |= IONIC_QCQ_F_INITED;

	return 0;
}

static int
ionic_lif_notifyq_init(struct ionic_lif *lif)
{
	struct ionic_dev *idev = &lif->adapter->idev;
	struct ionic_qcq *qcq = lif->notifyqcq;
	struct ionic_queue *q = &qcq->q;
	int err;

	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.q_init = {
			.opcode = IONIC_CMD_Q_INIT,
			.lif_index = lif->index,
			.type = q->type,
			.index = q->index,
			.flags = (IONIC_QINIT_F_IRQ | IONIC_QINIT_F_ENA),
			.intr_index = qcq->intr.index,
			.pid = q->pid,
			.ring_size = rte_log2_u32(q->num_descs),
			.ring_base = q->base_pa,
		}
	};

	IONIC_PRINT(DEBUG, "notifyq_init.pid %d", ctx.cmd.q_init.pid);
	IONIC_PRINT(DEBUG, "notifyq_init.index %d",
		ctx.cmd.q_init.index);
	IONIC_PRINT(DEBUG, "notifyq_init.ring_base 0x%" PRIx64 "",
		ctx.cmd.q_init.ring_base);
	IONIC_PRINT(DEBUG, "notifyq_init.ring_size %d",
		ctx.cmd.q_init.ring_size);
	IONIC_PRINT(DEBUG, "notifyq_init.ver %u", ctx.cmd.q_init.ver);

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	q->hw_type = ctx.comp.q_init.hw_type;
	q->hw_index = ctx.comp.q_init.hw_index;
	q->db = NULL;

	IONIC_PRINT(DEBUG, "notifyq->hw_type %d", q->hw_type);
	IONIC_PRINT(DEBUG, "notifyq->hw_index %d", q->hw_index);
	IONIC_PRINT(DEBUG, "notifyq->db %p", q->db);

	if (qcq->flags & IONIC_QCQ_F_INTR)
		ionic_intr_mask(idev->intr_ctrl, qcq->intr.index,
			IONIC_INTR_MASK_CLEAR);

	qcq->flags |= IONIC_QCQ_F_INITED;

	return 0;
}

int
ionic_lif_set_features(struct ionic_lif *lif)
{
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.lif_setattr = {
			.opcode = IONIC_CMD_LIF_SETATTR,
			.index = lif->index,
			.attr = IONIC_LIF_ATTR_FEATURES,
			.features = lif->features,
		},
	};
	int err;

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	lif->hw_features = (ctx.cmd.lif_setattr.features &
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
ionic_lif_txq_init(struct ionic_qcq *qcq)
{
	struct ionic_queue *q = &qcq->q;
	struct ionic_lif *lif = qcq->lif;
	struct ionic_cq *cq = &qcq->cq;
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.q_init = {
			.opcode = IONIC_CMD_Q_INIT,
			.lif_index = lif->index,
			.type = q->type,
			.index = q->index,
			.flags = IONIC_QINIT_F_SG,
			.intr_index = cq->bound_intr->index,
			.pid = q->pid,
			.ring_size = rte_log2_u32(q->num_descs),
			.ring_base = q->base_pa,
			.cq_ring_base = cq->base_pa,
			.sg_ring_base = q->sg_base_pa,
		},
	};
	int err;

	IONIC_PRINT(DEBUG, "txq_init.pid %d", ctx.cmd.q_init.pid);
	IONIC_PRINT(DEBUG, "txq_init.index %d", ctx.cmd.q_init.index);
	IONIC_PRINT(DEBUG, "txq_init.ring_base 0x%" PRIx64 "",
		ctx.cmd.q_init.ring_base);
	IONIC_PRINT(DEBUG, "txq_init.ring_size %d",
		ctx.cmd.q_init.ring_size);
	IONIC_PRINT(DEBUG, "txq_init.ver %u", ctx.cmd.q_init.ver);

	err = ionic_adminq_post_wait(qcq->lif, &ctx);
	if (err)
		return err;

	q->hw_type = ctx.comp.q_init.hw_type;
	q->hw_index = ctx.comp.q_init.hw_index;
	q->db = ionic_db_map(lif, q);

	IONIC_PRINT(DEBUG, "txq->hw_type %d", q->hw_type);
	IONIC_PRINT(DEBUG, "txq->hw_index %d", q->hw_index);
	IONIC_PRINT(DEBUG, "txq->db %p", q->db);

	qcq->flags |= IONIC_QCQ_F_INITED;

	return 0;
}

int
ionic_lif_rxq_init(struct ionic_qcq *qcq)
{
	struct ionic_queue *q = &qcq->q;
	struct ionic_lif *lif = qcq->lif;
	struct ionic_cq *cq = &qcq->cq;
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.q_init = {
			.opcode = IONIC_CMD_Q_INIT,
			.lif_index = lif->index,
			.type = q->type,
			.index = q->index,
			.flags = IONIC_QINIT_F_SG,
			.intr_index = cq->bound_intr->index,
			.pid = q->pid,
			.ring_size = rte_log2_u32(q->num_descs),
			.ring_base = q->base_pa,
			.cq_ring_base = cq->base_pa,
			.sg_ring_base = q->sg_base_pa,
		},
	};
	int err;

	IONIC_PRINT(DEBUG, "rxq_init.pid %d", ctx.cmd.q_init.pid);
	IONIC_PRINT(DEBUG, "rxq_init.index %d", ctx.cmd.q_init.index);
	IONIC_PRINT(DEBUG, "rxq_init.ring_base 0x%" PRIx64 "",
		ctx.cmd.q_init.ring_base);
	IONIC_PRINT(DEBUG, "rxq_init.ring_size %d",
		ctx.cmd.q_init.ring_size);
	IONIC_PRINT(DEBUG, "rxq_init.ver %u", ctx.cmd.q_init.ver);

	err = ionic_adminq_post_wait(qcq->lif, &ctx);
	if (err)
		return err;

	q->hw_type = ctx.comp.q_init.hw_type;
	q->hw_index = ctx.comp.q_init.hw_index;
	q->db = ionic_db_map(lif, q);

	qcq->flags |= IONIC_QCQ_F_INITED;

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
			.index = lif->index,
			.attr = IONIC_LIF_ATTR_MAC,
		},
	};
	int err;

	IONIC_PRINT_CALL();

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	if (!rte_is_zero_ether_addr((struct rte_ether_addr *)
			lif->mac_addr)) {
		IONIC_PRINT(INFO, "deleting station MAC addr");

		ionic_lif_addr_del(lif, lif->mac_addr);
	}

	memcpy(lif->mac_addr, ctx.comp.lif_getattr.mac, RTE_ETHER_ADDR_LEN);

	if (rte_is_zero_ether_addr((struct rte_ether_addr *)lif->mac_addr)) {
		IONIC_PRINT(NOTICE, "empty MAC addr (VF?)");
		return 0;
	}

	IONIC_PRINT(DEBUG, "adding station MAC addr");

	ionic_lif_addr_add(lif, lif->mac_addr);

	return 0;
}

static void
ionic_lif_set_name(struct ionic_lif *lif)
{
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.lif_setattr = {
			.opcode = IONIC_CMD_LIF_SETATTR,
			.index = lif->index,
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
	int err;

	memset(&lif->stats_base, 0, sizeof(lif->stats_base));

	ionic_dev_cmd_lif_init(idev, lif->index, lif->info_pa);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	if (err)
		return err;

	ionic_dev_cmd_comp(idev, &comp);

	lif->hw_index = comp.hw_index;

	err = ionic_lif_adminq_init(lif);
	if (err)
		return err;

	err = ionic_lif_notifyq_init(lif);
	if (err)
		goto err_out_adminq_deinit;

	lif->features =
		  IONIC_ETH_HW_VLAN_TX_TAG
		| IONIC_ETH_HW_VLAN_RX_STRIP
		| IONIC_ETH_HW_VLAN_RX_FILTER
		| IONIC_ETH_HW_RX_HASH
		| IONIC_ETH_HW_TX_SG
		| IONIC_ETH_HW_RX_SG
		| IONIC_ETH_HW_TX_CSUM
		| IONIC_ETH_HW_RX_CSUM
		| IONIC_ETH_HW_TSO
		| IONIC_ETH_HW_TSO_IPV6
		| IONIC_ETH_HW_TSO_ECN;

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
	ionic_lif_qcq_deinit(lif, lif->notifyqcq);

err_out_adminq_deinit:
	ionic_lif_qcq_deinit(lif, lif->adminqcq);

	return err;
}

void
ionic_lif_deinit(struct ionic_lif *lif)
{
	if (!(lif->state & IONIC_LIF_F_INITED))
		return;

	ionic_rx_filters_deinit(lif);
	ionic_lif_rss_teardown(lif);
	ionic_lif_qcq_deinit(lif, lif->notifyqcq);
	ionic_lif_qcq_deinit(lif, lif->adminqcq);

	lif->state &= ~IONIC_LIF_F_INITED;
}

int
ionic_lif_configure(struct ionic_lif *lif)
{
	struct ionic_identity *ident = &lif->adapter->ident;
	uint32_t ntxqs_per_lif =
		ident->lif.eth.config.queue_count[IONIC_QTYPE_TXQ];
	uint32_t nrxqs_per_lif =
		ident->lif.eth.config.queue_count[IONIC_QTYPE_RXQ];
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

	return 0;
}

int
ionic_lif_start(struct ionic_lif *lif)
{
	uint32_t rx_mode = 0;
	uint32_t i;
	int err;

	IONIC_PRINT(DEBUG, "Setting RSS configuration on port %u",
		lif->port_id);

	err = ionic_lif_rss_setup(lif);
	if (err)
		return err;

	IONIC_PRINT(DEBUG, "Setting RX mode on port %u",
		lif->port_id);

	rx_mode |= IONIC_RX_MODE_F_UNICAST;
	rx_mode |= IONIC_RX_MODE_F_MULTICAST;
	rx_mode |= IONIC_RX_MODE_F_BROADCAST;

	lif->rx_mode = 0; /* set by ionic_set_rx_mode */

	ionic_set_rx_mode(lif, rx_mode);

	IONIC_PRINT(DEBUG, "Starting %u RX queues and %u TX queues "
		"on port %u",
		lif->nrxqcqs, lif->ntxqcqs, lif->port_id);

	for (i = 0; i < lif->nrxqcqs; i++) {
		struct ionic_qcq *rxq = lif->rxqcqs[i];
		if (!rxq->deferred_start) {
			err = ionic_dev_rx_queue_start(lif->eth_dev, i);

			if (err)
				return err;
		}
	}

	for (i = 0; i < lif->ntxqcqs; i++) {
		struct ionic_qcq *txq = lif->txqcqs[i];
		if (!txq->deferred_start) {
			err = ionic_dev_tx_queue_start(lif->eth_dev, i);

			if (err)
				return err;
		}
	}

	ionic_link_status_check(lif);

	/* Carrier ON here */

	return 0;
}

int
ionic_lif_identify(struct ionic_adapter *adapter)
{
	struct ionic_dev *idev = &adapter->idev;
	struct ionic_identity *ident = &adapter->ident;
	int err;
	unsigned int i;
	unsigned int lif_words = sizeof(ident->lif.words) /
		sizeof(ident->lif.words[0]);
	unsigned int cmd_words = sizeof(idev->dev_cmd->data) /
		sizeof(idev->dev_cmd->data[0]);
	unsigned int nwords;

	ionic_dev_cmd_lif_identify(idev, IONIC_LIF_TYPE_CLASSIC,
		IONIC_IDENTITY_VERSION_1);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	if (err)
		return (err);

	nwords = RTE_MIN(lif_words, cmd_words);
	for (i = 0; i < nwords; i++)
		ident->lif.words[i] = ioread32(&idev->dev_cmd->data[i]);

	IONIC_PRINT(INFO, "capabilities 0x%" PRIx64 " ",
		ident->lif.capabilities);

	IONIC_PRINT(INFO, "eth.max_ucast_filters 0x%" PRIx32 " ",
		ident->lif.eth.max_ucast_filters);
	IONIC_PRINT(INFO, "eth.max_mcast_filters 0x%" PRIx32 " ",
		ident->lif.eth.max_mcast_filters);

	IONIC_PRINT(INFO, "eth.features 0x%" PRIx64 " ",
		ident->lif.eth.config.features);
	IONIC_PRINT(INFO, "eth.queue_count[IONIC_QTYPE_ADMINQ] 0x%" PRIx32 " ",
		ident->lif.eth.config.queue_count[IONIC_QTYPE_ADMINQ]);
	IONIC_PRINT(INFO, "eth.queue_count[IONIC_QTYPE_NOTIFYQ] 0x%" PRIx32 " ",
		ident->lif.eth.config.queue_count[IONIC_QTYPE_NOTIFYQ]);
	IONIC_PRINT(INFO, "eth.queue_count[IONIC_QTYPE_RXQ] 0x%" PRIx32 " ",
		ident->lif.eth.config.queue_count[IONIC_QTYPE_RXQ]);
	IONIC_PRINT(INFO, "eth.queue_count[IONIC_QTYPE_TXQ] 0x%" PRIx32 " ",
		ident->lif.eth.config.queue_count[IONIC_QTYPE_TXQ]);

	return 0;
}

int
ionic_lifs_size(struct ionic_adapter *adapter)
{
	struct ionic_identity *ident = &adapter->ident;
	uint32_t nlifs = ident->dev.nlifs;
	uint32_t nintrs, dev_nintrs = ident->dev.nintrs;

	adapter->max_ntxqs_per_lif =
		ident->lif.eth.config.queue_count[IONIC_QTYPE_TXQ];
	adapter->max_nrxqs_per_lif =
		ident->lif.eth.config.queue_count[IONIC_QTYPE_RXQ];

	nintrs = nlifs * 1 /* notifyq */;

	if (nintrs > dev_nintrs) {
		IONIC_PRINT(ERR,
			"At most %d intr supported, minimum req'd is %u",
			dev_nintrs, nintrs);
		return -ENOSPC;
	}

	adapter->nintrs = nintrs;

	return 0;
}
