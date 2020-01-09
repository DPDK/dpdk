/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2019 Hisilicon Limited.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <rte_bus_pci.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_dev.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_vxlan.h>
#include <rte_ethdev_driver.h>
#include <rte_io.h>
#include <rte_ip.h>
#include <rte_gre.h>
#include <rte_net.h>
#include <rte_malloc.h>
#include <rte_pci.h>

#include "hns3_ethdev.h"
#include "hns3_rxtx.h"
#include "hns3_regs.h"
#include "hns3_logs.h"

#define HNS3_CFG_DESC_NUM(num)	((num) / 8 - 1)
#define DEFAULT_RX_FREE_THRESH	16

static void
hns3_rx_queue_release_mbufs(struct hns3_rx_queue *rxq)
{
	uint16_t i;

	/* Note: Fake rx queue will not enter here */
	if (rxq->sw_ring) {
		for (i = 0; i < rxq->nb_rx_desc; i++) {
			if (rxq->sw_ring[i].mbuf) {
				rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
				rxq->sw_ring[i].mbuf = NULL;
			}
		}
	}
}

static void
hns3_tx_queue_release_mbufs(struct hns3_tx_queue *txq)
{
	uint16_t i;

	/* Note: Fake rx queue will not enter here */
	if (txq->sw_ring) {
		for (i = 0; i < txq->nb_tx_desc; i++) {
			if (txq->sw_ring[i].mbuf) {
				rte_pktmbuf_free_seg(txq->sw_ring[i].mbuf);
				txq->sw_ring[i].mbuf = NULL;
			}
		}
	}
}

static void
hns3_rx_queue_release(void *queue)
{
	struct hns3_rx_queue *rxq = queue;
	if (rxq) {
		hns3_rx_queue_release_mbufs(rxq);
		if (rxq->mz)
			rte_memzone_free(rxq->mz);
		if (rxq->sw_ring)
			rte_free(rxq->sw_ring);
		rte_free(rxq);
	}
}

static void
hns3_tx_queue_release(void *queue)
{
	struct hns3_tx_queue *txq = queue;
	if (txq) {
		hns3_tx_queue_release_mbufs(txq);
		if (txq->mz)
			rte_memzone_free(txq->mz);
		if (txq->sw_ring)
			rte_free(txq->sw_ring);
		rte_free(txq);
	}
}

void
hns3_dev_rx_queue_release(void *queue)
{
	struct hns3_rx_queue *rxq = queue;
	struct hns3_adapter *hns;

	if (rxq == NULL)
		return;

	hns = rxq->hns;
	rte_spinlock_lock(&hns->hw.lock);
	hns3_rx_queue_release(queue);
	rte_spinlock_unlock(&hns->hw.lock);
}

void
hns3_dev_tx_queue_release(void *queue)
{
	struct hns3_tx_queue *txq = queue;
	struct hns3_adapter *hns;

	if (txq == NULL)
		return;

	hns = txq->hns;
	rte_spinlock_lock(&hns->hw.lock);
	hns3_tx_queue_release(queue);
	rte_spinlock_unlock(&hns->hw.lock);
}

static void
hns3_fake_rx_queue_release(struct hns3_rx_queue *queue)
{
	struct hns3_rx_queue *rxq = queue;
	struct hns3_adapter *hns;
	struct hns3_hw *hw;
	uint16_t idx;

	if (rxq == NULL)
		return;

	hns = rxq->hns;
	hw = &hns->hw;
	idx = rxq->queue_id;
	if (hw->fkq_data.rx_queues[idx]) {
		hns3_rx_queue_release(hw->fkq_data.rx_queues[idx]);
		hw->fkq_data.rx_queues[idx] = NULL;
	}

	/* free fake rx queue arrays */
	if (idx == (hw->fkq_data.nb_fake_rx_queues - 1)) {
		hw->fkq_data.nb_fake_rx_queues = 0;
		rte_free(hw->fkq_data.rx_queues);
		hw->fkq_data.rx_queues = NULL;
	}
}

static void
hns3_fake_tx_queue_release(struct hns3_tx_queue *queue)
{
	struct hns3_tx_queue *txq = queue;
	struct hns3_adapter *hns;
	struct hns3_hw *hw;
	uint16_t idx;

	if (txq == NULL)
		return;

	hns = txq->hns;
	hw = &hns->hw;
	idx = txq->queue_id;
	if (hw->fkq_data.tx_queues[idx]) {
		hns3_tx_queue_release(hw->fkq_data.tx_queues[idx]);
		hw->fkq_data.tx_queues[idx] = NULL;
	}

	/* free fake tx queue arrays */
	if (idx == (hw->fkq_data.nb_fake_tx_queues - 1)) {
		hw->fkq_data.nb_fake_tx_queues = 0;
		rte_free(hw->fkq_data.tx_queues);
		hw->fkq_data.tx_queues = NULL;
	}
}

static void
hns3_free_rx_queues(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_fake_queue_data *fkq_data;
	struct hns3_hw *hw = &hns->hw;
	uint16_t nb_rx_q;
	uint16_t i;

	nb_rx_q = hw->data->nb_rx_queues;
	for (i = 0; i < nb_rx_q; i++) {
		if (dev->data->rx_queues[i]) {
			hns3_rx_queue_release(dev->data->rx_queues[i]);
			dev->data->rx_queues[i] = NULL;
		}
	}

	/* Free fake Rx queues */
	fkq_data = &hw->fkq_data;
	for (i = 0; i < fkq_data->nb_fake_rx_queues; i++) {
		if (fkq_data->rx_queues[i])
			hns3_fake_rx_queue_release(fkq_data->rx_queues[i]);
	}
}

static void
hns3_free_tx_queues(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_fake_queue_data *fkq_data;
	struct hns3_hw *hw = &hns->hw;
	uint16_t nb_tx_q;
	uint16_t i;

	nb_tx_q = hw->data->nb_tx_queues;
	for (i = 0; i < nb_tx_q; i++) {
		if (dev->data->tx_queues[i]) {
			hns3_tx_queue_release(dev->data->tx_queues[i]);
			dev->data->tx_queues[i] = NULL;
		}
	}

	/* Free fake Tx queues */
	fkq_data = &hw->fkq_data;
	for (i = 0; i < fkq_data->nb_fake_tx_queues; i++) {
		if (fkq_data->tx_queues[i])
			hns3_fake_tx_queue_release(fkq_data->tx_queues[i]);
	}
}

void
hns3_free_all_queues(struct rte_eth_dev *dev)
{
	hns3_free_rx_queues(dev);
	hns3_free_tx_queues(dev);
}

static int
hns3_alloc_rx_queue_mbufs(struct hns3_hw *hw, struct hns3_rx_queue *rxq)
{
	struct rte_mbuf *mbuf;
	uint64_t dma_addr;
	uint16_t i;

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		mbuf = rte_mbuf_raw_alloc(rxq->mb_pool);
		if (unlikely(mbuf == NULL)) {
			hns3_err(hw, "Failed to allocate RXD[%d] for rx queue!",
				 i);
			hns3_rx_queue_release_mbufs(rxq);
			return -ENOMEM;
		}

		rte_mbuf_refcnt_set(mbuf, 1);
		mbuf->next = NULL;
		mbuf->data_off = RTE_PKTMBUF_HEADROOM;
		mbuf->nb_segs = 1;
		mbuf->port = rxq->port_id;

		rxq->sw_ring[i].mbuf = mbuf;
		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));
		rxq->rx_ring[i].addr = dma_addr;
		rxq->rx_ring[i].rx.bd_base_info = 0;
	}

	return 0;
}

static int
hns3_buf_size2type(uint32_t buf_size)
{
	int bd_size_type;

	switch (buf_size) {
	case 512:
		bd_size_type = HNS3_BD_SIZE_512_TYPE;
		break;
	case 1024:
		bd_size_type = HNS3_BD_SIZE_1024_TYPE;
		break;
	case 4096:
		bd_size_type = HNS3_BD_SIZE_4096_TYPE;
		break;
	default:
		bd_size_type = HNS3_BD_SIZE_2048_TYPE;
	}

	return bd_size_type;
}

static void
hns3_init_rx_queue_hw(struct hns3_rx_queue *rxq)
{
	uint32_t rx_buf_len = rxq->rx_buf_len;
	uint64_t dma_addr = rxq->rx_ring_phys_addr;

	hns3_write_dev(rxq, HNS3_RING_RX_BASEADDR_L_REG, (uint32_t)dma_addr);
	hns3_write_dev(rxq, HNS3_RING_RX_BASEADDR_H_REG,
		       (uint32_t)((dma_addr >> 31) >> 1));

	hns3_write_dev(rxq, HNS3_RING_RX_BD_LEN_REG,
		       hns3_buf_size2type(rx_buf_len));
	hns3_write_dev(rxq, HNS3_RING_RX_BD_NUM_REG,
		       HNS3_CFG_DESC_NUM(rxq->nb_rx_desc));
}

static void
hns3_init_tx_queue_hw(struct hns3_tx_queue *txq)
{
	uint64_t dma_addr = txq->tx_ring_phys_addr;

	hns3_write_dev(txq, HNS3_RING_TX_BASEADDR_L_REG, (uint32_t)dma_addr);
	hns3_write_dev(txq, HNS3_RING_TX_BASEADDR_H_REG,
		       (uint32_t)((dma_addr >> 31) >> 1));

	hns3_write_dev(txq, HNS3_RING_TX_BD_NUM_REG,
		       HNS3_CFG_DESC_NUM(txq->nb_tx_desc));
}

static void
hns3_enable_all_queues(struct hns3_hw *hw, bool en)
{
	uint16_t nb_rx_q = hw->data->nb_rx_queues;
	uint16_t nb_tx_q = hw->data->nb_tx_queues;
	struct hns3_rx_queue *rxq;
	struct hns3_tx_queue *txq;
	uint32_t rcb_reg;
	int i;

	for (i = 0; i < hw->cfg_max_queues; i++) {
		if (i < nb_rx_q)
			rxq = hw->data->rx_queues[i];
		else
			rxq = hw->fkq_data.rx_queues[i - nb_rx_q];
		if (i < nb_tx_q)
			txq = hw->data->tx_queues[i];
		else
			txq = hw->fkq_data.tx_queues[i - nb_tx_q];
		if (rxq == NULL || txq == NULL ||
		    (en && (rxq->rx_deferred_start || txq->tx_deferred_start)))
			continue;

		rcb_reg = hns3_read_dev(rxq, HNS3_RING_EN_REG);
		if (en)
			rcb_reg |= BIT(HNS3_RING_EN_B);
		else
			rcb_reg &= ~BIT(HNS3_RING_EN_B);
		hns3_write_dev(rxq, HNS3_RING_EN_REG, rcb_reg);
	}
}

static int
hns3_tqp_enable(struct hns3_hw *hw, uint16_t queue_id, bool enable)
{
	struct hns3_cfg_com_tqp_queue_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	req = (struct hns3_cfg_com_tqp_queue_cmd *)desc.data;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_CFG_COM_TQP_QUEUE, false);
	req->tqp_id = rte_cpu_to_le_16(queue_id & HNS3_RING_ID_MASK);
	req->stream_id = 0;
	hns3_set_bit(req->enable, HNS3_TQP_ENABLE_B, enable ? 1 : 0);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "TQP enable fail, ret = %d", ret);

	return ret;
}

static int
hns3_send_reset_tqp_cmd(struct hns3_hw *hw, uint16_t queue_id, bool enable)
{
	struct hns3_reset_tqp_queue_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RESET_TQP_QUEUE, false);

	req = (struct hns3_reset_tqp_queue_cmd *)desc.data;
	req->tqp_id = rte_cpu_to_le_16(queue_id & HNS3_RING_ID_MASK);
	hns3_set_bit(req->reset_req, HNS3_TQP_RESET_B, enable ? 1 : 0);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "Send tqp reset cmd error, ret = %d", ret);

	return ret;
}

static int
hns3_get_reset_status(struct hns3_hw *hw, uint16_t queue_id)
{
	struct hns3_reset_tqp_queue_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RESET_TQP_QUEUE, true);

	req = (struct hns3_reset_tqp_queue_cmd *)desc.data;
	req->tqp_id = rte_cpu_to_le_16(queue_id & HNS3_RING_ID_MASK);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "Get reset status error, ret =%d", ret);
		return ret;
	}

	return hns3_get_bit(req->ready_to_reset, HNS3_TQP_RESET_B);
}

static int
hns3_reset_tqp(struct hns3_hw *hw, uint16_t queue_id)
{
#define HNS3_TQP_RESET_TRY_MS	200
	uint64_t end;
	int reset_status;
	int ret;

	ret = hns3_tqp_enable(hw, queue_id, false);
	if (ret)
		return ret;

	/*
	 * In current version VF is not supported when PF is driven by DPDK
	 * driver, all task queue pairs are mapped to PF function, so PF's queue
	 * id is equals to the global queue id in PF range.
	 */
	ret = hns3_send_reset_tqp_cmd(hw, queue_id, true);
	if (ret) {
		hns3_err(hw, "Send reset tqp cmd fail, ret = %d", ret);
		return ret;
	}
	ret = -ETIMEDOUT;
	end = get_timeofday_ms() + HNS3_TQP_RESET_TRY_MS;
	do {
		/* Wait for tqp hw reset */
		rte_delay_ms(HNS3_POLL_RESPONE_MS);
		reset_status = hns3_get_reset_status(hw, queue_id);
		if (reset_status) {
			ret = 0;
			break;
		}
	} while (get_timeofday_ms() < end);

	if (ret) {
		hns3_err(hw, "Reset TQP fail, ret = %d", ret);
		return ret;
	}

	ret = hns3_send_reset_tqp_cmd(hw, queue_id, false);
	if (ret)
		hns3_err(hw, "Deassert the soft reset fail, ret = %d", ret);

	return ret;
}

static int
hns3vf_reset_tqp(struct hns3_hw *hw, uint16_t queue_id)
{
	uint8_t msg_data[2];
	int ret;

	/* Disable VF's queue before send queue reset msg to PF */
	ret = hns3_tqp_enable(hw, queue_id, false);
	if (ret)
		return ret;

	memcpy(msg_data, &queue_id, sizeof(uint16_t));

	return hns3_send_mbx_msg(hw, HNS3_MBX_QUEUE_RESET, 0, msg_data,
				 sizeof(msg_data), true, NULL, 0);
}

static int
hns3_reset_queue(struct hns3_adapter *hns, uint16_t queue_id)
{
	struct hns3_hw *hw = &hns->hw;
	if (hns->is_vf)
		return hns3vf_reset_tqp(hw, queue_id);
	else
		return hns3_reset_tqp(hw, queue_id);
}

int
hns3_reset_all_queues(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	int ret, i;

	for (i = 0; i < hw->cfg_max_queues; i++) {
		ret = hns3_reset_queue(hns, i);
		if (ret) {
			hns3_err(hw, "Failed to reset No.%d queue: %d", i, ret);
			return ret;
		}
	}
	return 0;
}

void
hns3_tqp_intr_enable(struct hns3_hw *hw, uint16_t tpq_int_num, bool en)
{
	uint32_t addr, value;

	addr = HNS3_TQP_INTR_CTRL_REG + tpq_int_num * HNS3_VECTOR_REG_OFFSET;
	value = en ? 1 : 0;

	hns3_write_dev(hw, addr, value);
}

int
hns3_dev_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (dev->data->dev_conf.intr_conf.rxq == 0)
		return -ENOTSUP;

	/* enable the vectors */
	hns3_tqp_intr_enable(hw, queue_id, true);

	return rte_intr_ack(intr_handle);
}

int
hns3_dev_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (dev->data->dev_conf.intr_conf.rxq == 0)
		return -ENOTSUP;

	/* disable the vectors */
	hns3_tqp_intr_enable(hw, queue_id, false);

	return 0;
}

static int
hns3_dev_rx_queue_start(struct hns3_adapter *hns, uint16_t idx)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_rx_queue *rxq;
	int ret;

	PMD_INIT_FUNC_TRACE();

	rxq = (struct hns3_rx_queue *)hw->data->rx_queues[idx];
	ret = hns3_alloc_rx_queue_mbufs(hw, rxq);
	if (ret) {
		hns3_err(hw, "Failed to alloc mbuf for No.%d rx queue: %d",
			 idx, ret);
		return ret;
	}

	rxq->next_to_use = 0;
	rxq->next_to_clean = 0;
	hns3_init_rx_queue_hw(rxq);

	return 0;
}

static void
hns3_fake_rx_queue_start(struct hns3_adapter *hns, uint16_t idx)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_rx_queue *rxq;

	rxq = (struct hns3_rx_queue *)hw->fkq_data.rx_queues[idx];
	rxq->next_to_use = 0;
	rxq->next_to_clean = 0;
	hns3_init_rx_queue_hw(rxq);
}

static void
hns3_init_tx_queue(struct hns3_tx_queue *queue)
{
	struct hns3_tx_queue *txq = queue;
	struct hns3_desc *desc;
	int i;

	/* Clear tx bd */
	desc = txq->tx_ring;
	for (i = 0; i < txq->nb_tx_desc; i++) {
		desc->tx.tp_fe_sc_vld_ra_ri = 0;
		desc++;
	}

	txq->next_to_use = 0;
	txq->next_to_clean = 0;
	txq->tx_bd_ready = txq->nb_tx_desc - 1;
	hns3_init_tx_queue_hw(txq);
}

static void
hns3_dev_tx_queue_start(struct hns3_adapter *hns, uint16_t idx)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_tx_queue *txq;

	txq = (struct hns3_tx_queue *)hw->data->tx_queues[idx];
	hns3_init_tx_queue(txq);
}

static void
hns3_fake_tx_queue_start(struct hns3_adapter *hns, uint16_t idx)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_tx_queue *txq;

	txq = (struct hns3_tx_queue *)hw->fkq_data.tx_queues[idx];
	hns3_init_tx_queue(txq);
}

static void
hns3_init_tx_ring_tc(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_tx_queue *txq;
	int i, num;

	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		struct hns3_tc_queue_info *tc_queue = &hw->tc_queue[i];
		int j;

		if (!tc_queue->enable)
			continue;

		for (j = 0; j < tc_queue->tqp_count; j++) {
			num = tc_queue->tqp_offset + j;
			txq = (struct hns3_tx_queue *)hw->data->tx_queues[num];
			if (txq == NULL)
				continue;

			hns3_write_dev(txq, HNS3_RING_TX_TC_REG, tc_queue->tc);
		}
	}
}

static int
hns3_start_rx_queues(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_rx_queue *rxq;
	int i, j;
	int ret;

	/* Initialize RSS for queues */
	ret = hns3_config_rss(hns);
	if (ret) {
		hns3_err(hw, "Failed to configure rss %d", ret);
		return ret;
	}

	for (i = 0; i < hw->data->nb_rx_queues; i++) {
		rxq = (struct hns3_rx_queue *)hw->data->rx_queues[i];
		if (rxq == NULL || rxq->rx_deferred_start)
			continue;
		ret = hns3_dev_rx_queue_start(hns, i);
		if (ret) {
			hns3_err(hw, "Failed to start No.%d rx queue: %d", i,
				 ret);
			goto out;
		}
	}

	for (i = 0; i < hw->fkq_data.nb_fake_rx_queues; i++) {
		rxq = (struct hns3_rx_queue *)hw->fkq_data.rx_queues[i];
		if (rxq == NULL || rxq->rx_deferred_start)
			continue;
		hns3_fake_rx_queue_start(hns, i);
	}
	return 0;

out:
	for (j = 0; j < i; j++) {
		rxq = (struct hns3_rx_queue *)hw->data->rx_queues[j];
		hns3_rx_queue_release_mbufs(rxq);
	}

	return ret;
}

static void
hns3_start_tx_queues(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_tx_queue *txq;
	int i;

	for (i = 0; i < hw->data->nb_tx_queues; i++) {
		txq = (struct hns3_tx_queue *)hw->data->tx_queues[i];
		if (txq == NULL || txq->tx_deferred_start)
			continue;
		hns3_dev_tx_queue_start(hns, i);
	}

	for (i = 0; i < hw->fkq_data.nb_fake_tx_queues; i++) {
		txq = (struct hns3_tx_queue *)hw->fkq_data.tx_queues[i];
		if (txq == NULL || txq->tx_deferred_start)
			continue;
		hns3_fake_tx_queue_start(hns, i);
	}

	hns3_init_tx_ring_tc(hns);
}

int
hns3_start_queues(struct hns3_adapter *hns, bool reset_queue)
{
	struct hns3_hw *hw = &hns->hw;
	int ret;

	if (reset_queue) {
		ret = hns3_reset_all_queues(hns);
		if (ret) {
			hns3_err(hw, "Failed to reset all queues %d", ret);
			return ret;
		}
	}

	ret = hns3_start_rx_queues(hns);
	if (ret) {
		hns3_err(hw, "Failed to start rx queues: %d", ret);
		return ret;
	}

	hns3_start_tx_queues(hns);
	hns3_enable_all_queues(hw, true);

	return 0;
}

int
hns3_stop_queues(struct hns3_adapter *hns, bool reset_queue)
{
	struct hns3_hw *hw = &hns->hw;
	int ret;

	hns3_enable_all_queues(hw, false);
	if (reset_queue) {
		ret = hns3_reset_all_queues(hns);
		if (ret) {
			hns3_err(hw, "Failed to reset all queues %d", ret);
			return ret;
		}
	}
	return 0;
}

static void*
hns3_alloc_rxq_and_dma_zone(struct rte_eth_dev *dev,
			    struct hns3_queue_info *q_info)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	const struct rte_memzone *rx_mz;
	struct hns3_rx_queue *rxq;
	unsigned int rx_desc;

	rxq = rte_zmalloc_socket(q_info->type, sizeof(struct hns3_rx_queue),
				 RTE_CACHE_LINE_SIZE, q_info->socket_id);
	if (rxq == NULL) {
		hns3_err(hw, "Failed to allocate memory for No.%d rx ring!",
			 q_info->idx);
		return NULL;
	}

	/* Allocate rx ring hardware descriptors. */
	rxq->queue_id = q_info->idx;
	rxq->nb_rx_desc = q_info->nb_desc;
	rx_desc = rxq->nb_rx_desc * sizeof(struct hns3_desc);
	rx_mz = rte_eth_dma_zone_reserve(dev, q_info->ring_name, q_info->idx,
					 rx_desc, HNS3_RING_BASE_ALIGN,
					 q_info->socket_id);
	if (rx_mz == NULL) {
		hns3_err(hw, "Failed to reserve DMA memory for No.%d rx ring!",
			 q_info->idx);
		hns3_rx_queue_release(rxq);
		return NULL;
	}
	rxq->mz = rx_mz;
	rxq->rx_ring = (struct hns3_desc *)rx_mz->addr;
	rxq->rx_ring_phys_addr = rx_mz->iova;

	hns3_dbg(hw, "No.%d rx descriptors iova 0x%" PRIx64, q_info->idx,
		 rxq->rx_ring_phys_addr);

	return rxq;
}

static int
hns3_fake_rx_queue_setup(struct rte_eth_dev *dev, uint16_t idx,
			 uint16_t nb_desc, unsigned int socket_id)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_queue_info q_info;
	struct hns3_rx_queue *rxq;
	uint16_t nb_rx_q;

	if (hw->fkq_data.rx_queues[idx]) {
		hns3_rx_queue_release(hw->fkq_data.rx_queues[idx]);
		hw->fkq_data.rx_queues[idx] = NULL;
	}

	q_info.idx = idx;
	q_info.socket_id = socket_id;
	q_info.nb_desc = nb_desc;
	q_info.type = "hns3 fake RX queue";
	q_info.ring_name = "rx_fake_ring";
	rxq = hns3_alloc_rxq_and_dma_zone(dev, &q_info);
	if (rxq == NULL) {
		hns3_err(hw, "Failed to setup No.%d fake rx ring.", idx);
		return -ENOMEM;
	}

	/* Don't need alloc sw_ring, because upper applications don't use it */
	rxq->sw_ring = NULL;

	rxq->hns = hns;
	rxq->rx_deferred_start = false;
	rxq->port_id = dev->data->port_id;
	rxq->configured = true;
	nb_rx_q = dev->data->nb_rx_queues;
	rxq->io_base = (void *)((char *)hw->io_base + HNS3_TQP_REG_OFFSET +
				(nb_rx_q + idx) * HNS3_TQP_REG_SIZE);
	rxq->rx_buf_len = hw->rx_buf_len;

	rte_spinlock_lock(&hw->lock);
	hw->fkq_data.rx_queues[idx] = rxq;
	rte_spinlock_unlock(&hw->lock);

	return 0;
}

static void*
hns3_alloc_txq_and_dma_zone(struct rte_eth_dev *dev,
			    struct hns3_queue_info *q_info)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	const struct rte_memzone *tx_mz;
	struct hns3_tx_queue *txq;
	struct hns3_desc *desc;
	unsigned int tx_desc;
	int i;

	txq = rte_zmalloc_socket(q_info->type, sizeof(struct hns3_tx_queue),
				 RTE_CACHE_LINE_SIZE, q_info->socket_id);
	if (txq == NULL) {
		hns3_err(hw, "Failed to allocate memory for No.%d tx ring!",
			 q_info->idx);
		return NULL;
	}

	/* Allocate tx ring hardware descriptors. */
	txq->queue_id = q_info->idx;
	txq->nb_tx_desc = q_info->nb_desc;
	tx_desc = txq->nb_tx_desc * sizeof(struct hns3_desc);
	tx_mz = rte_eth_dma_zone_reserve(dev, q_info->ring_name, q_info->idx,
					 tx_desc, HNS3_RING_BASE_ALIGN,
					 q_info->socket_id);
	if (tx_mz == NULL) {
		hns3_err(hw, "Failed to reserve DMA memory for No.%d tx ring!",
			 q_info->idx);
		hns3_tx_queue_release(txq);
		return NULL;
	}
	txq->mz = tx_mz;
	txq->tx_ring = (struct hns3_desc *)tx_mz->addr;
	txq->tx_ring_phys_addr = tx_mz->iova;

	hns3_dbg(hw, "No.%d tx descriptors iova 0x%" PRIx64, q_info->idx,
		 txq->tx_ring_phys_addr);

	/* Clear tx bd */
	desc = txq->tx_ring;
	for (i = 0; i < txq->nb_tx_desc; i++) {
		desc->tx.tp_fe_sc_vld_ra_ri = 0;
		desc++;
	}

	return txq;
}

static int
hns3_fake_tx_queue_setup(struct rte_eth_dev *dev, uint16_t idx,
			 uint16_t nb_desc, unsigned int socket_id)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_queue_info q_info;
	struct hns3_tx_queue *txq;
	uint16_t nb_tx_q;

	if (hw->fkq_data.tx_queues[idx] != NULL) {
		hns3_tx_queue_release(hw->fkq_data.tx_queues[idx]);
		hw->fkq_data.tx_queues[idx] = NULL;
	}

	q_info.idx = idx;
	q_info.socket_id = socket_id;
	q_info.nb_desc = nb_desc;
	q_info.type = "hns3 fake TX queue";
	q_info.ring_name = "tx_fake_ring";
	txq = hns3_alloc_txq_and_dma_zone(dev, &q_info);
	if (txq == NULL) {
		hns3_err(hw, "Failed to setup No.%d fake tx ring.", idx);
		return -ENOMEM;
	}

	/* Don't need alloc sw_ring, because upper applications don't use it */
	txq->sw_ring = NULL;

	txq->hns = hns;
	txq->tx_deferred_start = false;
	txq->port_id = dev->data->port_id;
	txq->configured = true;
	nb_tx_q = dev->data->nb_tx_queues;
	txq->io_base = (void *)((char *)hw->io_base + HNS3_TQP_REG_OFFSET +
				(nb_tx_q + idx) * HNS3_TQP_REG_SIZE);

	rte_spinlock_lock(&hw->lock);
	hw->fkq_data.tx_queues[idx] = txq;
	rte_spinlock_unlock(&hw->lock);

	return 0;
}

static int
hns3_fake_rx_queue_config(struct hns3_hw *hw, uint16_t nb_queues)
{
	uint16_t old_nb_queues = hw->fkq_data.nb_fake_rx_queues;
	void **rxq;
	uint8_t i;

	if (hw->fkq_data.rx_queues == NULL && nb_queues != 0) {
		/* first time configuration */

		uint32_t size;
		size = sizeof(hw->fkq_data.rx_queues[0]) * nb_queues;
		hw->fkq_data.rx_queues = rte_zmalloc("fake_rx_queues", size,
						     RTE_CACHE_LINE_SIZE);
		if (hw->fkq_data.rx_queues == NULL) {
			hw->fkq_data.nb_fake_rx_queues = 0;
			return -ENOMEM;
		}
	} else if (hw->fkq_data.rx_queues != NULL && nb_queues != 0) {
		/* re-configure */

		rxq = hw->fkq_data.rx_queues;
		for (i = nb_queues; i < old_nb_queues; i++)
			hns3_dev_rx_queue_release(rxq[i]);

		rxq = rte_realloc(rxq, sizeof(rxq[0]) * nb_queues,
				  RTE_CACHE_LINE_SIZE);
		if (rxq == NULL)
			return -ENOMEM;
		if (nb_queues > old_nb_queues) {
			uint16_t new_qs = nb_queues - old_nb_queues;
			memset(rxq + old_nb_queues, 0, sizeof(rxq[0]) * new_qs);
		}

		hw->fkq_data.rx_queues = rxq;
	} else if (hw->fkq_data.rx_queues != NULL && nb_queues == 0) {
		rxq = hw->fkq_data.rx_queues;
		for (i = nb_queues; i < old_nb_queues; i++)
			hns3_dev_rx_queue_release(rxq[i]);

		rte_free(hw->fkq_data.rx_queues);
		hw->fkq_data.rx_queues = NULL;
	}

	hw->fkq_data.nb_fake_rx_queues = nb_queues;

	return 0;
}

static int
hns3_fake_tx_queue_config(struct hns3_hw *hw, uint16_t nb_queues)
{
	uint16_t old_nb_queues = hw->fkq_data.nb_fake_tx_queues;
	void **txq;
	uint8_t i;

	if (hw->fkq_data.tx_queues == NULL && nb_queues != 0) {
		/* first time configuration */

		uint32_t size;
		size = sizeof(hw->fkq_data.tx_queues[0]) * nb_queues;
		hw->fkq_data.tx_queues = rte_zmalloc("fake_tx_queues", size,
						     RTE_CACHE_LINE_SIZE);
		if (hw->fkq_data.tx_queues == NULL) {
			hw->fkq_data.nb_fake_tx_queues = 0;
			return -ENOMEM;
		}
	} else if (hw->fkq_data.tx_queues != NULL && nb_queues != 0) {
		/* re-configure */

		txq = hw->fkq_data.tx_queues;
		for (i = nb_queues; i < old_nb_queues; i++)
			hns3_dev_tx_queue_release(txq[i]);
		txq = rte_realloc(txq, sizeof(txq[0]) * nb_queues,
				  RTE_CACHE_LINE_SIZE);
		if (txq == NULL)
			return -ENOMEM;
		if (nb_queues > old_nb_queues) {
			uint16_t new_qs = nb_queues - old_nb_queues;
			memset(txq + old_nb_queues, 0, sizeof(txq[0]) * new_qs);
		}

		hw->fkq_data.tx_queues = txq;
	} else if (hw->fkq_data.tx_queues != NULL && nb_queues == 0) {
		txq = hw->fkq_data.tx_queues;
		for (i = nb_queues; i < old_nb_queues; i++)
			hns3_dev_tx_queue_release(txq[i]);

		rte_free(hw->fkq_data.tx_queues);
		hw->fkq_data.tx_queues = NULL;
	}
	hw->fkq_data.nb_fake_tx_queues = nb_queues;

	return 0;
}

int
hns3_set_fake_rx_or_tx_queues(struct rte_eth_dev *dev, uint16_t nb_rx_q,
			      uint16_t nb_tx_q)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint16_t rx_need_add_nb_q;
	uint16_t tx_need_add_nb_q;
	uint16_t port_id;
	uint16_t q;
	int ret;

	/* Setup new number of fake RX/TX queues and reconfigure device. */
	hw->cfg_max_queues = RTE_MAX(nb_rx_q, nb_tx_q);
	rx_need_add_nb_q = hw->cfg_max_queues - nb_rx_q;
	tx_need_add_nb_q = hw->cfg_max_queues - nb_tx_q;
	ret = hns3_fake_rx_queue_config(hw, rx_need_add_nb_q);
	if (ret) {
		hns3_err(hw, "Fail to configure fake rx queues: %d", ret);
		goto cfg_fake_rx_q_fail;
	}

	ret = hns3_fake_tx_queue_config(hw, tx_need_add_nb_q);
	if (ret) {
		hns3_err(hw, "Fail to configure fake rx queues: %d", ret);
		goto cfg_fake_tx_q_fail;
	}

	/* Allocate and set up fake RX queue per Ethernet port. */
	port_id = hw->data->port_id;
	for (q = 0; q < rx_need_add_nb_q; q++) {
		ret = hns3_fake_rx_queue_setup(dev, q, HNS3_MIN_RING_DESC,
					       rte_eth_dev_socket_id(port_id));
		if (ret)
			goto setup_fake_rx_q_fail;
	}

	/* Allocate and set up fake TX queue per Ethernet port. */
	for (q = 0; q < tx_need_add_nb_q; q++) {
		ret = hns3_fake_tx_queue_setup(dev, q, HNS3_MIN_RING_DESC,
					       rte_eth_dev_socket_id(port_id));
		if (ret)
			goto setup_fake_tx_q_fail;
	}

	return 0;

setup_fake_tx_q_fail:
setup_fake_rx_q_fail:
	(void)hns3_fake_tx_queue_config(hw, 0);
cfg_fake_tx_q_fail:
	(void)hns3_fake_rx_queue_config(hw, 0);
cfg_fake_rx_q_fail:
	hw->cfg_max_queues = 0;

	return ret;
}

void
hns3_dev_release_mbufs(struct hns3_adapter *hns)
{
	struct rte_eth_dev_data *dev_data = hns->hw.data;
	struct hns3_rx_queue *rxq;
	struct hns3_tx_queue *txq;
	int i;

	if (dev_data->rx_queues)
		for (i = 0; i < dev_data->nb_rx_queues; i++) {
			rxq = dev_data->rx_queues[i];
			if (rxq == NULL || rxq->rx_deferred_start)
				continue;
			hns3_rx_queue_release_mbufs(rxq);
		}

	if (dev_data->tx_queues)
		for (i = 0; i < dev_data->nb_tx_queues; i++) {
			txq = dev_data->tx_queues[i];
			if (txq == NULL || txq->tx_deferred_start)
				continue;
			hns3_tx_queue_release_mbufs(txq);
		}
}

int
hns3_rx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t nb_desc,
		    unsigned int socket_id, const struct rte_eth_rxconf *conf,
		    struct rte_mempool *mp)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_queue_info q_info;
	struct hns3_rx_queue *rxq;
	int rx_entry_len;

	if (dev->data->dev_started) {
		hns3_err(hw, "rx_queue_setup after dev_start no supported");
		return -EINVAL;
	}

	if (nb_desc > HNS3_MAX_RING_DESC || nb_desc < HNS3_MIN_RING_DESC ||
	    nb_desc % HNS3_ALIGN_RING_DESC) {
		hns3_err(hw, "Number (%u) of rx descriptors is invalid",
			 nb_desc);
		return -EINVAL;
	}

	if (dev->data->rx_queues[idx]) {
		hns3_rx_queue_release(dev->data->rx_queues[idx]);
		dev->data->rx_queues[idx] = NULL;
	}

	q_info.idx = idx;
	q_info.socket_id = socket_id;
	q_info.nb_desc = nb_desc;
	q_info.type = "hns3 RX queue";
	q_info.ring_name = "rx_ring";
	rxq = hns3_alloc_rxq_and_dma_zone(dev, &q_info);
	if (rxq == NULL) {
		hns3_err(hw,
			 "Failed to alloc mem and reserve DMA mem for rx ring!");
		return -ENOMEM;
	}

	rxq->hns = hns;
	rxq->mb_pool = mp;
	if (conf->rx_free_thresh <= 0)
		rxq->rx_free_thresh = DEFAULT_RX_FREE_THRESH;
	else
		rxq->rx_free_thresh = conf->rx_free_thresh;
	rxq->rx_deferred_start = conf->rx_deferred_start;

	rx_entry_len = sizeof(struct hns3_entry) * rxq->nb_rx_desc;
	rxq->sw_ring = rte_zmalloc_socket("hns3 RX sw ring", rx_entry_len,
					  RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq->sw_ring == NULL) {
		hns3_err(hw, "Failed to allocate memory for rx sw ring!");
		hns3_rx_queue_release(rxq);
		return -ENOMEM;
	}

	rxq->next_to_use = 0;
	rxq->next_to_clean = 0;
	rxq->nb_rx_hold = 0;
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;
	rxq->port_id = dev->data->port_id;
	rxq->configured = true;
	rxq->io_base = (void *)((char *)hw->io_base + HNS3_TQP_REG_OFFSET +
				idx * HNS3_TQP_REG_SIZE);
	rxq->rx_buf_len = hw->rx_buf_len;
	rxq->l2_errors = 0;
	rxq->pkt_len_errors = 0;
	rxq->l3_csum_erros = 0;
	rxq->l4_csum_erros = 0;
	rxq->ol3_csum_erros = 0;
	rxq->ol4_csum_erros = 0;

	rte_spinlock_lock(&hw->lock);
	dev->data->rx_queues[idx] = rxq;
	rte_spinlock_unlock(&hw->lock);

	return 0;
}

static inline uint32_t
rxd_pkt_info_to_pkt_type(uint32_t pkt_info, uint32_t ol_info)
{
#define HNS3_L2TBL_NUM	4
#define HNS3_L3TBL_NUM	16
#define HNS3_L4TBL_NUM	16
#define HNS3_OL3TBL_NUM	16
#define HNS3_OL4TBL_NUM	16
	uint32_t pkt_type = 0;
	uint32_t l2id, l3id, l4id;
	uint32_t ol3id, ol4id;

	static const uint32_t l2table[HNS3_L2TBL_NUM] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L2_ETHER_VLAN,
		RTE_PTYPE_L2_ETHER_QINQ,
		0
	};

	static const uint32_t l3table[HNS3_L3TBL_NUM] = {
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV6,
		RTE_PTYPE_L2_ETHER_ARP,
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L3_IPV4_EXT,
		RTE_PTYPE_L3_IPV6_EXT,
		RTE_PTYPE_L2_ETHER_LLDP,
		0, 0, 0, 0, 0, 0, 0, 0, 0
	};

	static const uint32_t l4table[HNS3_L4TBL_NUM] = {
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_TUNNEL_GRE,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_L4_IGMP,
		RTE_PTYPE_L4_ICMP,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};

	static const uint32_t inner_l2table[HNS3_L2TBL_NUM] = {
		RTE_PTYPE_INNER_L2_ETHER,
		RTE_PTYPE_INNER_L2_ETHER_VLAN,
		RTE_PTYPE_INNER_L2_ETHER_QINQ,
		0
	};

	static const uint32_t inner_l3table[HNS3_L3TBL_NUM] = {
		RTE_PTYPE_INNER_L3_IPV4,
		RTE_PTYPE_INNER_L3_IPV6,
		0,
		RTE_PTYPE_INNER_L2_ETHER,
		RTE_PTYPE_INNER_L3_IPV4_EXT,
		RTE_PTYPE_INNER_L3_IPV6_EXT,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};

	static const uint32_t inner_l4table[HNS3_L4TBL_NUM] = {
		RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_TUNNEL_GRE,
		RTE_PTYPE_INNER_L4_SCTP,
		RTE_PTYPE_L4_IGMP,
		RTE_PTYPE_INNER_L4_ICMP,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};

	static const uint32_t ol3table[HNS3_OL3TBL_NUM] = {
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV6,
		0, 0,
		RTE_PTYPE_L3_IPV4_EXT,
		RTE_PTYPE_L3_IPV6_EXT,
		0, 0, 0, 0, 0, 0, 0, 0, 0,
		RTE_PTYPE_UNKNOWN
	};

	static const uint32_t ol4table[HNS3_OL4TBL_NUM] = {
		0,
		RTE_PTYPE_TUNNEL_VXLAN,
		RTE_PTYPE_TUNNEL_NVGRE,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};

	l2id = hns3_get_field(pkt_info, HNS3_RXD_STRP_TAGP_M,
			      HNS3_RXD_STRP_TAGP_S);
	l3id = hns3_get_field(pkt_info, HNS3_RXD_L3ID_M, HNS3_RXD_L3ID_S);
	l4id = hns3_get_field(pkt_info, HNS3_RXD_L4ID_M, HNS3_RXD_L4ID_S);
	ol3id = hns3_get_field(ol_info, HNS3_RXD_OL3ID_M, HNS3_RXD_OL3ID_S);
	ol4id = hns3_get_field(ol_info, HNS3_RXD_OL4ID_M, HNS3_RXD_OL4ID_S);

	if (ol4table[ol4id])
		pkt_type |= (inner_l2table[l2id] | inner_l3table[l3id] |
			     inner_l4table[l4id] | ol3table[ol3id] |
			     ol4table[ol4id]);
	else
		pkt_type |= (l2table[l2id] | l3table[l3id] | l4table[l4id]);
	return pkt_type;
}

const uint32_t *
hns3_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L2_ETHER_VLAN,
		RTE_PTYPE_L2_ETHER_QINQ,
		RTE_PTYPE_L2_ETHER_LLDP,
		RTE_PTYPE_L2_ETHER_ARP,
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV4_EXT,
		RTE_PTYPE_L3_IPV6,
		RTE_PTYPE_L3_IPV6_EXT,
		RTE_PTYPE_L4_IGMP,
		RTE_PTYPE_L4_ICMP,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_TUNNEL_GRE,
		RTE_PTYPE_UNKNOWN
	};

	if (dev->rx_pkt_burst == hns3_recv_pkts)
		return ptypes;

	return NULL;
}

static void
hns3_clean_rx_buffers(struct hns3_rx_queue *rxq, int count)
{
	rxq->next_to_use += count;
	if (rxq->next_to_use >= rxq->nb_rx_desc)
		rxq->next_to_use -= rxq->nb_rx_desc;

	hns3_write_dev(rxq, HNS3_RING_RX_HEAD_REG, count);
}

static int
hns3_handle_bdinfo(struct hns3_rx_queue *rxq, struct rte_mbuf *rxm,
		   uint32_t bd_base_info, uint32_t l234_info,
		   uint32_t *cksum_err)
{
	uint32_t tmp = 0;

	if (unlikely(l234_info & BIT(HNS3_RXD_L2E_B))) {
		rxq->l2_errors++;
		return -EINVAL;
	}

	if (unlikely(rxm->pkt_len == 0 ||
		(l234_info & BIT(HNS3_RXD_TRUNCAT_B)))) {
		rxq->pkt_len_errors++;
		return -EINVAL;
	}

	if (bd_base_info & BIT(HNS3_RXD_L3L4P_B)) {
		if (unlikely(l234_info & BIT(HNS3_RXD_L3E_B))) {
			rxm->ol_flags |= PKT_RX_IP_CKSUM_BAD;
			rxq->l3_csum_erros++;
			tmp |= HNS3_L3_CKSUM_ERR;
		}

		if (unlikely(l234_info & BIT(HNS3_RXD_L4E_B))) {
			rxm->ol_flags |= PKT_RX_L4_CKSUM_BAD;
			rxq->l4_csum_erros++;
			tmp |= HNS3_L4_CKSUM_ERR;
		}

		if (unlikely(l234_info & BIT(HNS3_RXD_OL3E_B))) {
			rxq->ol3_csum_erros++;
			tmp |= HNS3_OUTER_L3_CKSUM_ERR;
		}

		if (unlikely(l234_info & BIT(HNS3_RXD_OL4E_B))) {
			rxm->ol_flags |= PKT_RX_OUTER_L4_CKSUM_BAD;
			rxq->ol4_csum_erros++;
			tmp |= HNS3_OUTER_L4_CKSUM_ERR;
		}
	}
	*cksum_err = tmp;

	return 0;
}

static void
hns3_rx_set_cksum_flag(struct rte_mbuf *rxm, uint64_t packet_type,
		       const uint32_t cksum_err)
{
	if (unlikely((packet_type & RTE_PTYPE_TUNNEL_MASK))) {
		if (likely(packet_type & RTE_PTYPE_INNER_L3_MASK) &&
		    (cksum_err & HNS3_L3_CKSUM_ERR) == 0)
			rxm->ol_flags |= PKT_RX_IP_CKSUM_GOOD;
		if (likely(packet_type & RTE_PTYPE_INNER_L4_MASK) &&
		    (cksum_err & HNS3_L4_CKSUM_ERR) == 0)
			rxm->ol_flags |= PKT_RX_L4_CKSUM_GOOD;
		if (likely(packet_type & RTE_PTYPE_L4_MASK) &&
		    (cksum_err & HNS3_OUTER_L4_CKSUM_ERR) == 0)
			rxm->ol_flags |= PKT_RX_OUTER_L4_CKSUM_GOOD;
	} else {
		if (likely(packet_type & RTE_PTYPE_L3_MASK) &&
		    (cksum_err & HNS3_L3_CKSUM_ERR) == 0)
			rxm->ol_flags |= PKT_RX_IP_CKSUM_GOOD;
		if (likely(packet_type & RTE_PTYPE_L4_MASK) &&
		    (cksum_err & HNS3_L4_CKSUM_ERR) == 0)
			rxm->ol_flags |= PKT_RX_L4_CKSUM_GOOD;
	}
}

uint16_t
hns3_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct hns3_rx_queue *rxq;      /* RX queue */
	struct hns3_desc *rx_ring;      /* RX ring (desc) */
	struct hns3_entry *sw_ring;
	struct hns3_entry *rxe;
	struct hns3_desc *rxdp;         /* pointer of the current desc */
	struct rte_mbuf *first_seg;
	struct rte_mbuf *last_seg;
	struct rte_mbuf *nmb;           /* pointer of the new mbuf */
	struct rte_mbuf *rxm;
	struct rte_eth_dev *dev;
	uint32_t bd_base_info;
	uint32_t cksum_err;
	uint32_t l234_info;
	uint32_t ol_info;
	uint64_t dma_addr;
	uint16_t data_len;
	uint16_t nb_rx_bd;
	uint16_t pkt_len;
	uint16_t nb_rx;
	uint16_t rx_id;
	int ret;

	nb_rx = 0;
	nb_rx_bd = 0;
	rxq = rx_queue;
	dev = &rte_eth_devices[rxq->port_id];

	rx_id = rxq->next_to_clean;
	rx_ring = rxq->rx_ring;
	first_seg = rxq->pkt_first_seg;
	last_seg = rxq->pkt_last_seg;
	sw_ring = rxq->sw_ring;

	while (nb_rx < nb_pkts) {
		rxdp = &rx_ring[rx_id];
		bd_base_info = rte_le_to_cpu_32(rxdp->rx.bd_base_info);
		if (unlikely(!hns3_get_bit(bd_base_info, HNS3_RXD_VLD_B)))
			break;

		nmb = rte_mbuf_raw_alloc(rxq->mb_pool);
		if (unlikely(nmb == NULL)) {
			dev->data->rx_mbuf_alloc_failed++;
			break;
		}

		nb_rx_bd++;
		rxe = &sw_ring[rx_id];
		rx_id++;
		if (unlikely(rx_id == rxq->nb_rx_desc))
			rx_id = 0;

		rte_prefetch0(sw_ring[rx_id].mbuf);
		if ((rx_id & 0x3) == 0) {
			rte_prefetch0(&rx_ring[rx_id]);
			rte_prefetch0(&sw_ring[rx_id]);
		}

		rxm = rxe->mbuf;
		rxe->mbuf = nmb;

		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));
		rxdp->addr = dma_addr;
		rxdp->rx.bd_base_info = 0;

		rte_cio_rmb();
		/* Load remained descriptor data and extract necessary fields */
		data_len = (uint16_t)(rte_le_to_cpu_16(rxdp->rx.size));
		l234_info = rte_le_to_cpu_32(rxdp->rx.l234_info);
		ol_info = rte_le_to_cpu_32(rxdp->rx.ol_info);

		if (first_seg == NULL) {
			first_seg = rxm;
			first_seg->nb_segs = 1;
		} else {
			first_seg->nb_segs++;
			last_seg->next = rxm;
		}

		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rxm->data_len = data_len;

		if (!hns3_get_bit(bd_base_info, HNS3_RXD_FE_B)) {
			last_seg = rxm;
			continue;
		}

		/* The last buffer of the received packet */
		pkt_len = (uint16_t)(rte_le_to_cpu_16(rxdp->rx.pkt_len));
		first_seg->pkt_len = pkt_len;
		first_seg->port = rxq->port_id;
		first_seg->hash.rss = rte_le_to_cpu_32(rxdp->rx.rss_hash);
		first_seg->ol_flags |= PKT_RX_RSS_HASH;
		if (unlikely(hns3_get_bit(bd_base_info, HNS3_RXD_LUM_B))) {
			first_seg->hash.fdir.hi =
				rte_le_to_cpu_32(rxdp->rx.fd_id);
			first_seg->ol_flags |= PKT_RX_FDIR | PKT_RX_FDIR_ID;
		}
		rxm->next = NULL;

		ret = hns3_handle_bdinfo(rxq, first_seg, bd_base_info,
					 l234_info, &cksum_err);
		if (unlikely(ret))
			goto pkt_err;

		first_seg->packet_type = rxd_pkt_info_to_pkt_type(l234_info,
								  ol_info);

		if (bd_base_info & BIT(HNS3_RXD_L3L4P_B))
			hns3_rx_set_cksum_flag(rxm, first_seg->packet_type,
					       cksum_err);

		first_seg->vlan_tci = rte_le_to_cpu_16(rxdp->rx.vlan_tag);
		first_seg->vlan_tci_outer =
			rte_le_to_cpu_16(rxdp->rx.ot_vlan_tag);
		rx_pkts[nb_rx++] = first_seg;
		first_seg = NULL;
		continue;
pkt_err:
		rte_pktmbuf_free(first_seg);
		first_seg = NULL;
	}

	rxq->next_to_clean = rx_id;
	rxq->pkt_first_seg = first_seg;
	rxq->pkt_last_seg = last_seg;
	hns3_clean_rx_buffers(rxq, nb_rx_bd);

	return nb_rx;
}

int
hns3_tx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t nb_desc,
		    unsigned int socket_id, const struct rte_eth_txconf *conf)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_queue_info q_info;
	struct hns3_tx_queue *txq;
	int tx_entry_len;

	if (dev->data->dev_started) {
		hns3_err(hw, "tx_queue_setup after dev_start no supported");
		return -EINVAL;
	}

	if (nb_desc > HNS3_MAX_RING_DESC || nb_desc < HNS3_MIN_RING_DESC ||
	    nb_desc % HNS3_ALIGN_RING_DESC) {
		hns3_err(hw, "Number (%u) of tx descriptors is invalid",
			    nb_desc);
		return -EINVAL;
	}

	if (dev->data->tx_queues[idx] != NULL) {
		hns3_tx_queue_release(dev->data->tx_queues[idx]);
		dev->data->tx_queues[idx] = NULL;
	}

	q_info.idx = idx;
	q_info.socket_id = socket_id;
	q_info.nb_desc = nb_desc;
	q_info.type = "hns3 TX queue";
	q_info.ring_name = "tx_ring";
	txq = hns3_alloc_txq_and_dma_zone(dev, &q_info);
	if (txq == NULL) {
		hns3_err(hw,
			 "Failed to alloc mem and reserve DMA mem for tx ring!");
		return -ENOMEM;
	}

	txq->tx_deferred_start = conf->tx_deferred_start;
	tx_entry_len = sizeof(struct hns3_entry) * txq->nb_tx_desc;
	txq->sw_ring = rte_zmalloc_socket("hns3 TX sw ring", tx_entry_len,
					  RTE_CACHE_LINE_SIZE, socket_id);
	if (txq->sw_ring == NULL) {
		hns3_err(hw, "Failed to allocate memory for tx sw ring!");
		hns3_tx_queue_release(txq);
		return -ENOMEM;
	}

	txq->hns = hns;
	txq->next_to_use = 0;
	txq->next_to_clean = 0;
	txq->tx_bd_ready = txq->nb_tx_desc - 1;
	txq->port_id = dev->data->port_id;
	txq->configured = true;
	txq->io_base = (void *)((char *)hw->io_base + HNS3_TQP_REG_OFFSET +
				idx * HNS3_TQP_REG_SIZE);
	rte_spinlock_lock(&hw->lock);
	dev->data->tx_queues[idx] = txq;
	rte_spinlock_unlock(&hw->lock);

	return 0;
}

static inline void
hns3_queue_xmit(struct hns3_tx_queue *txq, uint32_t buf_num)
{
	hns3_write_dev(txq, HNS3_RING_TX_TAIL_REG, buf_num);
}

static void
hns3_tx_free_useless_buffer(struct hns3_tx_queue *txq)
{
	uint16_t tx_next_clean = txq->next_to_clean;
	uint16_t tx_next_use   = txq->next_to_use;
	uint16_t tx_bd_ready   = txq->tx_bd_ready;
	uint16_t tx_bd_max     = txq->nb_tx_desc;
	struct hns3_entry *tx_bak_pkt = &txq->sw_ring[tx_next_clean];
	struct hns3_desc *desc = &txq->tx_ring[tx_next_clean];
	struct rte_mbuf *mbuf;

	while ((!hns3_get_bit(desc->tx.tp_fe_sc_vld_ra_ri, HNS3_TXD_VLD_B)) &&
		tx_next_use != tx_next_clean) {
		mbuf = tx_bak_pkt->mbuf;
		if (mbuf) {
			rte_pktmbuf_free_seg(mbuf);
			tx_bak_pkt->mbuf = NULL;
		}

		desc++;
		tx_bak_pkt++;
		tx_next_clean++;
		tx_bd_ready++;

		if (tx_next_clean >= tx_bd_max) {
			tx_next_clean = 0;
			desc = txq->tx_ring;
			tx_bak_pkt = txq->sw_ring;
		}
	}

	txq->next_to_clean = tx_next_clean;
	txq->tx_bd_ready   = tx_bd_ready;
}

static void
fill_desc(struct hns3_tx_queue *txq, uint16_t tx_desc_id, struct rte_mbuf *rxm,
	  bool first, int offset)
{
	struct hns3_desc *tx_ring = txq->tx_ring;
	struct hns3_desc *desc = &tx_ring[tx_desc_id];
	uint8_t frag_end = rxm->next == NULL ? 1 : 0;
	uint16_t size = rxm->data_len;
	uint16_t rrcfv = 0;
	uint64_t ol_flags = rxm->ol_flags;
	uint32_t hdr_len;
	uint32_t paylen;
	uint32_t tmp;

	desc->addr = rte_mbuf_data_iova(rxm) + offset;
	desc->tx.send_size = rte_cpu_to_le_16(size);
	hns3_set_bit(rrcfv, HNS3_TXD_VLD_B, 1);

	if (first) {
		hdr_len = rxm->l2_len + rxm->l3_len + rxm->l4_len;
		hdr_len += (ol_flags & PKT_TX_TUNNEL_MASK) ?
			   rxm->outer_l2_len + rxm->outer_l3_len : 0;
		paylen = rxm->pkt_len - hdr_len;
		desc->tx.paylen = rte_cpu_to_le_32(paylen);
	}

	hns3_set_bit(rrcfv, HNS3_TXD_FE_B, frag_end);
	desc->tx.tp_fe_sc_vld_ra_ri = rte_cpu_to_le_16(rrcfv);

	if (frag_end) {
		if (ol_flags & (PKT_TX_VLAN_PKT | PKT_TX_QINQ_PKT)) {
			tmp = rte_le_to_cpu_32(desc->tx.type_cs_vlan_tso_len);
			hns3_set_bit(tmp, HNS3_TXD_VLAN_B, 1);
			desc->tx.type_cs_vlan_tso_len = rte_cpu_to_le_32(tmp);
			desc->tx.vlan_tag = rte_cpu_to_le_16(rxm->vlan_tci);
		}

		if (ol_flags & PKT_TX_QINQ_PKT) {
			tmp = rte_le_to_cpu_32(desc->tx.ol_type_vlan_len_msec);
			hns3_set_bit(tmp, HNS3_TXD_OVLAN_B, 1);
			desc->tx.ol_type_vlan_len_msec = rte_cpu_to_le_32(tmp);
			desc->tx.outer_vlan_tag =
				rte_cpu_to_le_16(rxm->vlan_tci_outer);
		}
	}
}

static int
hns3_tx_alloc_mbufs(struct hns3_tx_queue *txq, struct rte_mempool *mb_pool,
		    uint16_t nb_new_buf, struct rte_mbuf **alloc_mbuf)
{
	struct rte_mbuf *new_mbuf = NULL;
	struct rte_eth_dev *dev;
	struct rte_mbuf *temp;
	struct hns3_hw *hw;
	uint16_t i;

	/* Allocate enough mbufs */
	for (i = 0; i < nb_new_buf; i++) {
		temp = rte_pktmbuf_alloc(mb_pool);
		if (unlikely(temp == NULL)) {
			dev = &rte_eth_devices[txq->port_id];
			hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
			hns3_err(hw, "Failed to alloc TX mbuf port_id=%d,"
				     "queue_id=%d in reassemble tx pkts.",
				     txq->port_id, txq->queue_id);
			rte_pktmbuf_free(new_mbuf);
			return -ENOMEM;
		}
		temp->next = new_mbuf;
		new_mbuf = temp;
	}

	if (new_mbuf == NULL)
		return -ENOMEM;

	new_mbuf->nb_segs = nb_new_buf;
	*alloc_mbuf = new_mbuf;

	return 0;
}

static int
hns3_reassemble_tx_pkts(void *tx_queue, struct rte_mbuf *tx_pkt,
			struct rte_mbuf **new_pkt)
{
	struct hns3_tx_queue *txq = tx_queue;
	struct rte_mempool *mb_pool;
	struct rte_mbuf *new_mbuf;
	struct rte_mbuf *temp_new;
	struct rte_mbuf *temp;
	uint16_t last_buf_len;
	uint16_t nb_new_buf;
	uint16_t buf_size;
	uint16_t buf_len;
	uint16_t len_s;
	uint16_t len_d;
	uint16_t len;
	uint16_t i;
	int ret;
	char *s;
	char *d;

	mb_pool = tx_pkt->pool;
	buf_size = tx_pkt->buf_len - RTE_PKTMBUF_HEADROOM;
	nb_new_buf = (tx_pkt->pkt_len - 1) / buf_size + 1;

	last_buf_len = tx_pkt->pkt_len % buf_size;
	if (last_buf_len == 0)
		last_buf_len = buf_size;

	/* Allocate enough mbufs */
	ret = hns3_tx_alloc_mbufs(txq, mb_pool, nb_new_buf, &new_mbuf);
	if (ret)
		return ret;

	/* Copy the original packet content to the new mbufs */
	temp = tx_pkt;
	s = rte_pktmbuf_mtod(temp, char *);
	len_s = temp->data_len;
	temp_new = new_mbuf;
	for (i = 0; i < nb_new_buf; i++) {
		d = rte_pktmbuf_mtod(temp_new, char *);
		if (i < nb_new_buf - 1)
			buf_len = buf_size;
		else
			buf_len = last_buf_len;
		len_d = buf_len;

		while (len_d) {
			len = RTE_MIN(len_s, len_d);
			memcpy(d, s, len);
			s = s + len;
			d = d + len;
			len_d = len_d - len;
			len_s = len_s - len;

			if (len_s == 0) {
				temp = temp->next;
				if (temp == NULL)
					break;
				s = rte_pktmbuf_mtod(temp, char *);
				len_s = temp->data_len;
			}
		}

		temp_new->data_len = buf_len;
		temp_new = temp_new->next;
	}

	/* free original mbufs */
	rte_pktmbuf_free(tx_pkt);

	*new_pkt = new_mbuf;

	return 0;
}

static void
hns3_parse_outer_params(uint64_t ol_flags, uint32_t *ol_type_vlan_len_msec)
{
	uint32_t tmp = *ol_type_vlan_len_msec;

	/* (outer) IP header type */
	if (ol_flags & PKT_TX_OUTER_IPV4) {
		/* OL3 header size, defined in 4 bytes */
		hns3_set_field(tmp, HNS3_TXD_L3LEN_M, HNS3_TXD_L3LEN_S,
			       sizeof(struct rte_ipv4_hdr) >> HNS3_L3_LEN_UNIT);
		if (ol_flags & PKT_TX_OUTER_IP_CKSUM)
			hns3_set_field(tmp, HNS3_TXD_OL3T_M,
				       HNS3_TXD_OL3T_S, HNS3_OL3T_IPV4_CSUM);
		else
			hns3_set_field(tmp, HNS3_TXD_OL3T_M, HNS3_TXD_OL3T_S,
				       HNS3_OL3T_IPV4_NO_CSUM);
	} else if (ol_flags & PKT_TX_OUTER_IPV6) {
		hns3_set_field(tmp, HNS3_TXD_OL3T_M, HNS3_TXD_OL3T_S,
			       HNS3_OL3T_IPV6);
		/* OL3 header size, defined in 4 bytes */
		hns3_set_field(tmp, HNS3_TXD_L3LEN_M, HNS3_TXD_L3LEN_S,
			       sizeof(struct rte_ipv6_hdr) >> HNS3_L3_LEN_UNIT);
	}

	*ol_type_vlan_len_msec = tmp;
}

static int
hns3_parse_inner_params(uint64_t ol_flags, uint32_t *ol_type_vlan_len_msec,
			struct rte_net_hdr_lens *hdr_lens)
{
	uint32_t tmp = *ol_type_vlan_len_msec;
	uint8_t l4_len;

	/* OL2 header size, defined in 2 bytes */
	hns3_set_field(tmp, HNS3_TXD_L2LEN_M, HNS3_TXD_L2LEN_S,
		       sizeof(struct rte_ether_hdr) >> HNS3_L2_LEN_UNIT);

	/* L4TUNT: L4 Tunneling Type */
	switch (ol_flags & PKT_TX_TUNNEL_MASK) {
	case PKT_TX_TUNNEL_GENEVE:
	case PKT_TX_TUNNEL_VXLAN:
		/* MAC in UDP tunnelling packet, include VxLAN */
		hns3_set_field(tmp, HNS3_TXD_TUNTYPE_M, HNS3_TXD_TUNTYPE_S,
			       HNS3_TUN_MAC_IN_UDP);
		/*
		 * OL4 header size, defined in 4 Bytes, it contains outer
		 * L4(UDP) length and tunneling length.
		 */
		hns3_set_field(tmp, HNS3_TXD_L4LEN_M, HNS3_TXD_L4LEN_S,
			       (uint8_t)RTE_ETHER_VXLAN_HLEN >>
			       HNS3_L4_LEN_UNIT);
		break;
	case PKT_TX_TUNNEL_GRE:
		hns3_set_field(tmp, HNS3_TXD_TUNTYPE_M, HNS3_TXD_TUNTYPE_S,
			       HNS3_TUN_NVGRE);
		/*
		 * OL4 header size, defined in 4 Bytes, it contains outer
		 * L4(GRE) length and tunneling length.
		 */
		l4_len = hdr_lens->l4_len + hdr_lens->tunnel_len;
		hns3_set_field(tmp, HNS3_TXD_L4LEN_M, HNS3_TXD_L4LEN_S,
			       l4_len >> HNS3_L4_LEN_UNIT);
		break;
	default:
		/* For non UDP / GRE tunneling, drop the tunnel packet */
		return -EINVAL;
	}

	*ol_type_vlan_len_msec = tmp;

	return 0;
}

static int
hns3_parse_tunneling_params(struct hns3_tx_queue *txq, uint16_t tx_desc_id,
			    uint64_t ol_flags,
			    struct rte_net_hdr_lens *hdr_lens)
{
	struct hns3_desc *tx_ring = txq->tx_ring;
	struct hns3_desc *desc = &tx_ring[tx_desc_id];
	uint32_t value = 0;
	int ret;

	hns3_parse_outer_params(ol_flags, &value);
	ret = hns3_parse_inner_params(ol_flags, &value, hdr_lens);
	if (ret)
		return -EINVAL;

	desc->tx.ol_type_vlan_len_msec |= rte_cpu_to_le_32(value);

	return 0;
}

static void
hns3_parse_l3_cksum_params(uint64_t ol_flags, uint32_t *type_cs_vlan_tso_len)
{
	uint32_t tmp;

	/* Enable L3 checksum offloads */
	if (ol_flags & PKT_TX_IPV4) {
		tmp = *type_cs_vlan_tso_len;
		hns3_set_field(tmp, HNS3_TXD_L3T_M, HNS3_TXD_L3T_S,
			       HNS3_L3T_IPV4);
		/* inner(/normal) L3 header size, defined in 4 bytes */
		hns3_set_field(tmp, HNS3_TXD_L3LEN_M, HNS3_TXD_L3LEN_S,
			       sizeof(struct rte_ipv4_hdr) >> HNS3_L3_LEN_UNIT);
		if (ol_flags & PKT_TX_IP_CKSUM)
			hns3_set_bit(tmp, HNS3_TXD_L3CS_B, 1);
		*type_cs_vlan_tso_len = tmp;
	} else if (ol_flags & PKT_TX_IPV6) {
		tmp = *type_cs_vlan_tso_len;
		/* L3T, IPv6 don't do checksum */
		hns3_set_field(tmp, HNS3_TXD_L3T_M, HNS3_TXD_L3T_S,
			       HNS3_L3T_IPV6);
		/* inner(/normal) L3 header size, defined in 4 bytes */
		hns3_set_field(tmp, HNS3_TXD_L3LEN_M, HNS3_TXD_L3LEN_S,
			       sizeof(struct rte_ipv6_hdr) >> HNS3_L3_LEN_UNIT);
		*type_cs_vlan_tso_len = tmp;
	}
}

static void
hns3_parse_l4_cksum_params(uint64_t ol_flags, uint32_t *type_cs_vlan_tso_len)
{
	uint32_t tmp;

	/* Enable L4 checksum offloads */
	switch (ol_flags & PKT_TX_L4_MASK) {
	case PKT_TX_TCP_CKSUM:
		tmp = *type_cs_vlan_tso_len;
		hns3_set_field(tmp, HNS3_TXD_L4T_M, HNS3_TXD_L4T_S,
			       HNS3_L4T_TCP);
		hns3_set_bit(tmp, HNS3_TXD_L4CS_B, 1);
		hns3_set_field(tmp, HNS3_TXD_L4LEN_M, HNS3_TXD_L4LEN_S,
			       sizeof(struct rte_tcp_hdr) >> HNS3_L4_LEN_UNIT);
		*type_cs_vlan_tso_len = tmp;
		break;
	case PKT_TX_UDP_CKSUM:
		tmp = *type_cs_vlan_tso_len;
		hns3_set_field(tmp, HNS3_TXD_L4T_M, HNS3_TXD_L4T_S,
			       HNS3_L4T_UDP);
		hns3_set_bit(tmp, HNS3_TXD_L4CS_B, 1);
		hns3_set_field(tmp, HNS3_TXD_L4LEN_M, HNS3_TXD_L4LEN_S,
			       sizeof(struct rte_udp_hdr) >> HNS3_L4_LEN_UNIT);
		*type_cs_vlan_tso_len = tmp;
		break;
	case PKT_TX_SCTP_CKSUM:
		tmp = *type_cs_vlan_tso_len;
		hns3_set_field(tmp, HNS3_TXD_L4T_M, HNS3_TXD_L4T_S,
			       HNS3_L4T_SCTP);
		hns3_set_bit(tmp, HNS3_TXD_L4CS_B, 1);
		hns3_set_field(tmp, HNS3_TXD_L4LEN_M, HNS3_TXD_L4LEN_S,
			       sizeof(struct rte_sctp_hdr) >> HNS3_L4_LEN_UNIT);
		*type_cs_vlan_tso_len = tmp;
		break;
	default:
		break;
	}
}

static void
hns3_txd_enable_checksum(struct hns3_tx_queue *txq, uint16_t tx_desc_id,
			 uint64_t ol_flags)
{
	struct hns3_desc *tx_ring = txq->tx_ring;
	struct hns3_desc *desc = &tx_ring[tx_desc_id];
	uint32_t value = 0;

	/* inner(/normal) L2 header size, defined in 2 bytes */
	hns3_set_field(value, HNS3_TXD_L2LEN_M, HNS3_TXD_L2LEN_S,
		       sizeof(struct rte_ether_hdr) >> HNS3_L2_LEN_UNIT);

	hns3_parse_l3_cksum_params(ol_flags, &value);
	hns3_parse_l4_cksum_params(ol_flags, &value);

	desc->tx.type_cs_vlan_tso_len |= rte_cpu_to_le_32(value);
}

uint16_t
hns3_prep_pkts(__rte_unused void *tx_queue, struct rte_mbuf **tx_pkts,
	       uint16_t nb_pkts)
{
	struct rte_mbuf *m;
	uint16_t i;
	int ret;

	for (i = 0; i < nb_pkts; i++) {
		m = tx_pkts[i];

		/* check the size of packet */
		if (m->pkt_len < RTE_ETHER_MIN_LEN) {
			rte_errno = EINVAL;
			return i;
		}

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
		ret = rte_validate_tx_offload(m);
		if (ret != 0) {
			rte_errno = -ret;
			return i;
		}
#endif
		ret = rte_net_intel_cksum_prepare(m);
		if (ret != 0) {
			rte_errno = -ret;
			return i;
		}
	}

	return i;
}

static int
hns3_parse_cksum(struct hns3_tx_queue *txq, uint16_t tx_desc_id,
		 const struct rte_mbuf *m, struct rte_net_hdr_lens *hdr_lens)
{
	/* Fill in tunneling parameters if necessary */
	if (m->ol_flags & PKT_TX_TUNNEL_MASK) {
		(void)rte_net_get_ptype(m, hdr_lens, RTE_PTYPE_ALL_MASK);
		if (hns3_parse_tunneling_params(txq, tx_desc_id, m->ol_flags,
						hdr_lens))
			return -EINVAL;
	}
	/* Enable checksum offloading */
	if (m->ol_flags & HNS3_TX_CKSUM_OFFLOAD_MASK)
		hns3_txd_enable_checksum(txq, tx_desc_id, m->ol_flags);

	return 0;
}

uint16_t
hns3_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct rte_net_hdr_lens hdr_lens = {0};
	struct hns3_tx_queue *txq = tx_queue;
	struct hns3_entry *tx_bak_pkt;
	struct rte_mbuf *new_pkt;
	struct rte_mbuf *tx_pkt;
	struct rte_mbuf *m_seg;
	uint32_t nb_hold = 0;
	uint16_t tx_next_use;
	uint16_t tx_pkt_num;
	uint16_t tx_bd_max;
	uint16_t nb_buf;
	uint16_t nb_tx;
	uint16_t i;

	/* free useless buffer */
	hns3_tx_free_useless_buffer(txq);

	tx_next_use   = txq->next_to_use;
	tx_bd_max     = txq->nb_tx_desc;
	tx_pkt_num = nb_pkts;

	/* send packets */
	tx_bak_pkt = &txq->sw_ring[tx_next_use];
	for (nb_tx = 0; nb_tx < tx_pkt_num; nb_tx++) {
		tx_pkt = *tx_pkts++;

		nb_buf = tx_pkt->nb_segs;

		if (nb_buf > txq->tx_bd_ready) {
			if (nb_tx == 0)
				return 0;

			goto end_of_tx;
		}

		/*
		 * If packet length is greater than HNS3_MAX_FRAME_LEN
		 * driver support, the packet will be ignored.
		 */
		if (unlikely(rte_pktmbuf_pkt_len(tx_pkt) > HNS3_MAX_FRAME_LEN))
			break;

		/*
		 * If packet length is less than minimum packet size, driver
		 * need to pad it.
		 */
		if (unlikely(rte_pktmbuf_pkt_len(tx_pkt) < HNS3_MIN_PKT_SIZE)) {
			uint16_t add_len;
			char *appended;

			add_len = HNS3_MIN_PKT_SIZE -
					 rte_pktmbuf_pkt_len(tx_pkt);
			appended = rte_pktmbuf_append(tx_pkt, add_len);
			if (appended == NULL)
				break;

			memset(appended, 0, add_len);
		}

		m_seg = tx_pkt;
		if (unlikely(nb_buf > HNS3_MAX_TX_BD_PER_PKT)) {
			if (hns3_reassemble_tx_pkts(txq, tx_pkt, &new_pkt))
				goto end_of_tx;
			m_seg = new_pkt;
			nb_buf = m_seg->nb_segs;
		}

		if (hns3_parse_cksum(txq, tx_next_use, m_seg, &hdr_lens))
			goto end_of_tx;

		i = 0;
		do {
			fill_desc(txq, tx_next_use, m_seg, (i == 0), 0);
			tx_bak_pkt->mbuf = m_seg;
			m_seg = m_seg->next;
			tx_next_use++;
			tx_bak_pkt++;
			if (tx_next_use >= tx_bd_max) {
				tx_next_use = 0;
				tx_bak_pkt = txq->sw_ring;
			}

			i++;
		} while (m_seg != NULL);

		nb_hold += i;
		txq->next_to_use = tx_next_use;
		txq->tx_bd_ready -= i;
	}

end_of_tx:

	if (likely(nb_tx))
		hns3_queue_xmit(txq, nb_hold);

	return nb_tx;
}

static uint16_t
hns3_dummy_rxtx_burst(void *dpdk_txq __rte_unused,
		      struct rte_mbuf **pkts __rte_unused,
		      uint16_t pkts_n __rte_unused)
{
	return 0;
}

void hns3_set_rxtx_function(struct rte_eth_dev *eth_dev)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;

	if (hns->hw.adapter_state == HNS3_NIC_STARTED &&
	    rte_atomic16_read(&hns->hw.reset.resetting) == 0) {
		eth_dev->rx_pkt_burst = hns3_recv_pkts;
		eth_dev->tx_pkt_burst = hns3_xmit_pkts;
		eth_dev->tx_pkt_prepare = hns3_prep_pkts;
	} else {
		eth_dev->rx_pkt_burst = hns3_dummy_rxtx_burst;
		eth_dev->tx_pkt_burst = hns3_dummy_rxtx_burst;
		eth_dev->tx_pkt_prepare = hns3_dummy_rxtx_burst;
	}
}
