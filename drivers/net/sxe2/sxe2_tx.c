/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include <rte_common.h>
#include <rte_net.h>
#include <rte_vect.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <ethdev_driver.h>
#include "sxe2_tx.h"
#include "sxe2_ethdev.h"
#include "sxe2_common_log.h"
#include "sxe2_cmd_chnl.h"

static void *sxe2_tx_doorbell_addr_get(struct sxe2_adapter *adapter, uint16_t queue_id)
{
	return sxe2_pci_map_addr_get(adapter, SXE2_PCI_MAP_RES_DOORBELL_TX,
				     queue_id);
}

static void sxe2_tx_tail_init(struct sxe2_adapter *adapter, struct sxe2_tx_queue *txq)
{
	txq->tdt_reg_addr = sxe2_tx_doorbell_addr_get(adapter, txq->queue_id);
	SXE2_PCI_REG_WRITE_WC(txq->tdt_reg_addr, 0);
}

void __rte_cold sxe2_tx_queue_reset(struct sxe2_tx_queue *txq)
{
	uint16_t prev, i;
	volatile union sxe2_tx_data_desc *txd;
	static const union sxe2_tx_data_desc zeroed_desc = {{0}};
	struct sxe2_tx_buffer *tx_buffer = txq->buffer_ring;

	for (i = 0; i < txq->ring_depth; i++)
		txq->desc_ring[i] = zeroed_desc;

	prev = txq->ring_depth - 1;
	for (i = 0; i < txq->ring_depth; i++) {
		txd = &txq->desc_ring[i];
		if (txd == NULL)
			continue;

		txd->wb.dd = rte_cpu_to_le_64(SXE2_TX_DESC_DTYPE_DESC_DONE);
		tx_buffer[i].mbuf       = NULL;
		tx_buffer[i].last_id    = i;
		tx_buffer[prev].next_id = i;
		prev = i;
	}

	txq->desc_used_num = 0;
	txq->desc_free_num = txq->ring_depth - 1;
	txq->next_use      = 0;
	txq->next_clean    = txq->ring_depth - 1;
	txq->next_dd       = txq->rs_thresh  - 1;
	txq->next_rs       = txq->rs_thresh  - 1;
}

void __rte_cold sxe2_tx_queue_mbufs_release(struct sxe2_tx_queue *txq)
{
	uint32_t i;

	if (txq != NULL && txq->buffer_ring != NULL) {
		for (i = 0; i < txq->ring_depth; i++) {
			if (txq->buffer_ring[i].mbuf != NULL) {
				rte_pktmbuf_free_seg(txq->buffer_ring[i].mbuf);
				txq->buffer_ring[i].mbuf = NULL;
			}
		}
	}
}

static void sxe2_tx_buffer_ring_free(struct sxe2_tx_queue *txq)
{
	if (txq != NULL && txq->buffer_ring != NULL)
		rte_free(txq->buffer_ring);
}

const struct sxe2_txq_ops sxe2_default_txq_ops = {
	.queue_reset      = sxe2_tx_queue_reset,
	.mbufs_release    = sxe2_tx_queue_mbufs_release,
	.buffer_ring_free = sxe2_tx_buffer_ring_free,
};

static struct sxe2_txq_ops sxe2_tx_default_ops_get(void)
{
	return sxe2_default_txq_ops;
}

static int32_t sxe2_txq_arg_validate(struct rte_eth_dev *dev, uint16_t ring_depth,
		uint16_t *rs_thresh, uint16_t *free_thresh, const struct rte_eth_txconf *tx_conf)
{
	int32_t ret = 0;

	if ((ring_depth % SXE2_TX_DESC_RING_ALIGN) != 0 ||
		ring_depth > SXE2_MAX_RING_DESC ||
		ring_depth < SXE2_MIN_RING_DESC) {
		PMD_LOG_ERR(TX, "number:%u of receive descriptors is invalid", ring_depth);
		ret = -EINVAL;
		goto l_end;
	}

	*free_thresh = (uint16_t)((tx_conf->tx_free_thresh) ?
			tx_conf->tx_free_thresh : DEFAULT_TX_FREE_THRESH);
	*rs_thresh   = (uint16_t)((tx_conf->tx_rs_thresh) ?
			tx_conf->tx_rs_thresh : DEFAULT_TX_RS_THRESH);

	if (*rs_thresh >= (ring_depth - 2)) {
		PMD_LOG_ERR(TX, "tx_rs_thresh must be less than the number "
			"of tx descriptors minus 2. (tx_rs_thresh:%u port:%u)",
			*rs_thresh, dev->data->port_id);
		ret = -EINVAL;
		goto l_end;
	}

	if (*free_thresh >= (ring_depth - 3)) {
		PMD_LOG_ERR(TX, "tx_free_thresh must be less than the number "
			"of tx descriptors minus 3. (tx_free_thresh:%u port:%u)",
			*free_thresh, dev->data->port_id);
		ret = -EINVAL;
		goto l_end;
	}

	if (*rs_thresh > *free_thresh) {
		PMD_LOG_ERR(TX, "tx_rs_thresh must be less than or equal to "
			"tx_free_thresh. (tx_free_thresh:%u tx_rs_thresh:%u port:%u)",
			*free_thresh, *rs_thresh, dev->data->port_id);
		ret = -EINVAL;
		goto l_end;
	}

	if ((ring_depth % *rs_thresh) != 0) {
		PMD_LOG_ERR(TX, "tx_rs_thresh must be a divisor of the "
			"number of tx descriptors. (tx_rs_thresh:%u port:%d ring_depth:%u)",
			*rs_thresh, dev->data->port_id, ring_depth);
		ret = -EINVAL;
		goto l_end;
	}

	ret = 0;

l_end:
	return ret;
}

void __rte_cold sxe2_tx_queue_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		struct rte_eth_txq_info *qinfo)
{
	struct sxe2_tx_queue *txq = NULL;

	txq = dev->data->tx_queues[queue_id];
	if (txq == NULL) {
		PMD_LOG_WARN(TX, "tx queue:%u is NULL", queue_id);
		goto end;
	}

	qinfo->nb_desc                = txq->ring_depth;

	qinfo->conf.tx_thresh.pthresh = txq->pthresh;
	qinfo->conf.tx_thresh.hthresh = txq->hthresh;
	qinfo->conf.tx_thresh.wthresh = txq->wthresh;
	qinfo->conf.tx_free_thresh    = txq->free_thresh;
	qinfo->conf.tx_rs_thresh      = txq->rs_thresh;
	qinfo->conf.offloads          = txq->offloads;
	qinfo->conf.tx_deferred_start = txq->tx_deferred_start;

end:
	return;
}

int32_t __rte_cold sxe2_tx_queue_stop(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_tx_queue *txq;
	int32_t ret;
	PMD_INIT_FUNC_TRACE();

	if (dev->data->tx_queue_state[queue_id] ==
			RTE_ETH_QUEUE_STATE_STOPPED) {
		ret = 0;
		goto l_end;
	}

	txq = dev->data->tx_queues[queue_id];
	if (txq == NULL) {
		ret = 0;
		goto l_end;
	}

	ret = sxe2_drv_txq_switch(adapter, txq, false);
	if (ret) {
		PMD_LOG_ERR(TX, "Failed to switch tx queue %u off",
				queue_id);
		goto l_end;
	}

	txq->ops.mbufs_release(txq);
	txq->ops.queue_reset(txq);
	dev->data->tx_queue_state[queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;
	ret = 0;

l_end:
	return ret;
}

static void __rte_cold sxe2_tx_queue_free(struct sxe2_tx_queue *txq)
{
	if (txq != NULL) {
		txq->ops.mbufs_release(txq);
		txq->ops.buffer_ring_free(txq);

		rte_memzone_free(txq->mz);
		rte_free(txq);
	}
}

void __rte_cold sxe2_tx_queue_release(struct rte_eth_dev *dev, uint16_t queue_idx)
{
	(void)sxe2_tx_queue_stop(dev, queue_idx);
	sxe2_tx_queue_free(dev->data->tx_queues[queue_idx]);
	dev->data->tx_queues[queue_idx] = NULL;
}

void __rte_cold sxe2_all_txqs_release(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	uint16_t nb_txq;

	for (nb_txq = 0; nb_txq < data->nb_tx_queues; nb_txq++) {
		if (data->tx_queues[nb_txq] == NULL)
			continue;

		sxe2_tx_queue_release(dev, nb_txq);
		data->tx_queues[nb_txq] = NULL;
	}
	data->nb_tx_queues = 0;
}

static struct sxe2_tx_queue
*sxe2_tx_queue_alloc(struct rte_eth_dev *dev, uint16_t queue_idx,
		uint16_t ring_depth, uint32_t socket_id)
{
	struct sxe2_tx_queue *txq;
	const struct rte_memzone *tz;

	if (dev->data->tx_queues[queue_idx]) {
		sxe2_tx_queue_release(dev, queue_idx);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	txq = rte_zmalloc_socket("tx_queue", sizeof(struct sxe2_tx_queue),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (txq == NULL) {
		PMD_LOG_ERR(TX, "tx queue:%d alloc failed", queue_idx);
		goto l_end;
	}

	tz = rte_eth_dma_zone_reserve(dev, "tx_dma", queue_idx,
			sizeof(union sxe2_tx_data_desc) * SXE2_MAX_RING_DESC,
			SXE2_DESC_ADDR_ALIGN, socket_id);
	if (tz == NULL) {
		PMD_LOG_ERR(TX, "tx desc ring alloc failed, queue_id:%d", queue_idx);
		rte_free(txq);
		txq = NULL;
		goto l_end;
	}

	txq->buffer_ring = rte_zmalloc_socket("tx_buffer_ring",
		sizeof(struct sxe2_tx_buffer) * ring_depth,
		RTE_CACHE_LINE_SIZE, socket_id);
	if (txq->buffer_ring == NULL) {
		PMD_LOG_ERR(TX, "tx buffer alloc failed, queue_id:%d", queue_idx);
		rte_memzone_free(tz);
		rte_free(txq);
		txq = NULL;
		goto l_end;
	}

	txq->mz = tz;
	txq->base_addr = tz->iova;
	txq->desc_ring = (volatile union sxe2_tx_data_desc *)tz->addr;

l_end:
	return txq;
}

int32_t __rte_cold sxe2_tx_queue_setup(struct rte_eth_dev *dev,
		uint16_t queue_idx, uint16_t nb_desc, uint32_t socket_id,
		const struct rte_eth_txconf *tx_conf)
{
	int32_t ret = 0;
	uint16_t tx_rs_thresh;
	uint16_t tx_free_thresh;
	struct sxe2_tx_queue *txq;
	struct sxe2_adapter  *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_vsi      *vsi     = adapter->vsi_ctxt.main_vsi;
	uint64_t offloads;
	PMD_INIT_FUNC_TRACE();

	ret = sxe2_txq_arg_validate(dev, nb_desc, &tx_rs_thresh, &tx_free_thresh, tx_conf);
	if (ret) {
		PMD_LOG_ERR(TX, "tx queue:%u arg validate failed", queue_idx);
		goto end;
	}

	offloads = tx_conf->offloads | dev->data->dev_conf.txmode.offloads;

	txq = sxe2_tx_queue_alloc(dev, queue_idx, nb_desc, socket_id);
	if (txq == NULL) {
		PMD_LOG_ERR(TX, "failed to alloc sxe2vf tx queue:%u resource", queue_idx);
		ret = -ENOMEM;
		goto end;
	}

	txq->vlan_flag         = SXE2_TX_FLAGS_VLAN_TAG_LOC_L2TAG1;
	txq->ring_depth        = nb_desc;
	txq->rs_thresh         = tx_rs_thresh;
	txq->free_thresh       = tx_free_thresh;
	txq->pthresh           = tx_conf->tx_thresh.pthresh;
	txq->hthresh           = tx_conf->tx_thresh.hthresh;
	txq->wthresh           = tx_conf->tx_thresh.wthresh;
	txq->queue_id          = queue_idx;
	txq->idx_in_func       = vsi->txqs.base_idx_in_func + queue_idx;
	txq->port_id           = dev->data->port_id;
	txq->offloads          = offloads;
	txq->tx_deferred_start = tx_conf->tx_deferred_start;
	txq->vsi               = vsi;
	txq->ops               = sxe2_tx_default_ops_get();
	txq->ops.queue_reset(txq);

	dev->data->tx_queues[queue_idx] = txq;
	ret = 0;

end:
	return ret;
}

int32_t __rte_cold sxe2_tx_queue_start(struct rte_eth_dev *dev, uint16_t queue_id)
{
	int32_t    ret = 0;
	struct sxe2_tx_queue *txq;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	PMD_INIT_FUNC_TRACE();

	if (dev->data->tx_queue_state[queue_id] == RTE_ETH_QUEUE_STATE_STARTED) {
		ret = 0;
		goto l_end;
	}

	txq = dev->data->tx_queues[queue_id];
	if (txq == NULL) {
		PMD_LOG_ERR(TX, "tx queue:%u is not available or setup", queue_id);
		ret = -EINVAL;
		goto l_end;
	}

	ret = sxe2_drv_txq_ctxt_cfg(adapter, txq, 1);
	if (ret) {
		PMD_LOG_ERR(TX, "tx queue:%u config ctxt fail", queue_id);

		(void)sxe2_drv_txq_switch(adapter, txq, false);
		txq->ops.mbufs_release(txq);
		txq->ops.queue_reset(txq);
		goto l_end;
	}

	sxe2_tx_tail_init(adapter, txq);

	dev->data->tx_queue_state[queue_id] = RTE_ETH_QUEUE_STATE_STARTED;
	ret = 0;

l_end:
	return ret;
}

int32_t __rte_cold sxe2_txqs_all_start(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	struct sxe2_tx_queue *txq;
	uint16_t nb_txq;
	uint16_t nb_started_txq;
	int32_t ret;
	PMD_INIT_FUNC_TRACE();

	for (nb_txq = 0; nb_txq < data->nb_tx_queues; nb_txq++) {
		txq = dev->data->tx_queues[nb_txq];
		if (!txq || txq->tx_deferred_start)
			continue;

		ret = sxe2_tx_queue_start(dev, nb_txq);
		if (ret) {
			PMD_LOG_ERR(TX, "Fail to start tx queue %u", nb_txq);
			goto l_free_started_queue;
		}
	}
	ret = 0;
	goto l_end;

l_free_started_queue:
	for (nb_started_txq = 0; nb_started_txq <= nb_txq; nb_started_txq++)
		(void)sxe2_tx_queue_stop(dev, nb_started_txq);

l_end:
	return ret;
}

void __rte_cold sxe2_txqs_all_stop(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	uint16_t nb_txq;
	int32_t ret;

	for (nb_txq = 0; nb_txq < data->nb_tx_queues; nb_txq++) {
		ret = sxe2_tx_queue_stop(dev, nb_txq);
		if (ret) {
			PMD_LOG_WARN(TX, "Fail to stop tx queue %u", nb_txq);
			continue;
		}
	}
}
