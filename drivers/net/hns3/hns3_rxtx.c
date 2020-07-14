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
#define DEFAULT_RX_FREE_THRESH	32

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

void
hns3_update_all_queues_pvid_state(struct hns3_hw *hw)
{
	uint16_t nb_rx_q = hw->data->nb_rx_queues;
	uint16_t nb_tx_q = hw->data->nb_tx_queues;
	struct hns3_rx_queue *rxq;
	struct hns3_tx_queue *txq;
	int pvid_state;
	int i;

	pvid_state = hw->port_base_vlan_cfg.state;
	for (i = 0; i < hw->cfg_max_queues; i++) {
		if (i < nb_rx_q) {
			rxq = hw->data->rx_queues[i];
			if (rxq != NULL)
				rxq->pvid_state = pvid_state;
		}
		if (i < nb_tx_q) {
			txq = hw->data->tx_queues[i];
			if (txq != NULL)
				txq->pvid_state = pvid_state;
		}
	}
}

void
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
hns3_set_queue_intr_gl(struct hns3_hw *hw, uint16_t queue_id,
		       uint8_t gl_idx, uint16_t gl_value)
{
	uint32_t offset[] = {HNS3_TQP_INTR_GL0_REG,
			     HNS3_TQP_INTR_GL1_REG,
			     HNS3_TQP_INTR_GL2_REG};
	uint32_t addr, value;

	if (gl_idx >= RTE_DIM(offset) || gl_value > HNS3_TQP_INTR_GL_MAX)
		return;

	addr = offset[gl_idx] + queue_id * HNS3_TQP_INTR_REG_SIZE;
	value = HNS3_GL_USEC_TO_REG(gl_value);

	hns3_write_dev(hw, addr, value);
}

void
hns3_set_queue_intr_rl(struct hns3_hw *hw, uint16_t queue_id, uint16_t rl_value)
{
	uint32_t addr, value;

	if (rl_value > HNS3_TQP_INTR_RL_MAX)
		return;

	addr = HNS3_TQP_INTR_RL_REG + queue_id * HNS3_TQP_INTR_REG_SIZE;
	value = HNS3_RL_USEC_TO_REG(rl_value);
	if (value > 0)
		value |= HNS3_TQP_INTR_RL_ENABLE_MASK;

	hns3_write_dev(hw, addr, value);
}

static void
hns3_queue_intr_enable(struct hns3_hw *hw, uint16_t queue_id, bool en)
{
	uint32_t addr, value;

	addr = HNS3_TQP_INTR_CTRL_REG + queue_id * HNS3_TQP_INTR_REG_SIZE;
	value = en ? 1 : 0;

	hns3_write_dev(hw, addr, value);
}

/*
 * Enable all rx queue interrupt when in interrupt rx mode.
 * This api was called before enable queue rx&tx (in normal start or reset
 * recover scenes), used to fix hardware rx queue interrupt enable was clear
 * when FLR.
 */
void
hns3_dev_all_rx_queue_intr_enable(struct hns3_hw *hw, bool en)
{
	struct rte_eth_dev *dev = &rte_eth_devices[hw->data->port_id];
	uint16_t nb_rx_q = hw->data->nb_rx_queues;
	int i;

	if (dev->data->dev_conf.intr_conf.rxq == 0)
		return;

	for (i = 0; i < nb_rx_q; i++)
		hns3_queue_intr_enable(hw, i, en);
}

int
hns3_dev_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (dev->data->dev_conf.intr_conf.rxq == 0)
		return -ENOTSUP;

	hns3_queue_intr_enable(hw, queue_id, true);

	return rte_intr_ack(intr_handle);
}

int
hns3_dev_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (dev->data->dev_conf.intr_conf.rxq == 0)
		return -ENOTSUP;

	hns3_queue_intr_enable(hw, queue_id, false);

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
	rxq->nb_rx_hold = 0;
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
	rxq->nb_rx_hold = 0;
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

/*
 * Start all queues.
 * Note: just init and setup queues, and don't enable queue rx&tx.
 */
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
	rxq->rx_buf_len = HNS3_MIN_BD_BUF_SIZE;

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

static int
hns3_rx_buf_len_calc(struct rte_mempool *mp, uint16_t *rx_buf_len)
{
	uint16_t vld_buf_size;
	uint16_t num_hw_specs;
	uint16_t i;

	/*
	 * hns3 network engine only support to set 4 typical specification, and
	 * different buffer size will affect the max packet_len and the max
	 * number of segmentation when hw gro is turned on in receive side. The
	 * relationship between them is as follows:
	 *      rx_buf_size     |  max_gro_pkt_len  |  max_gro_nb_seg
	 * ---------------------|-------------------|----------------
	 * HNS3_4K_BD_BUF_SIZE  |        60KB       |       15
	 * HNS3_2K_BD_BUF_SIZE  |        62KB       |       31
	 * HNS3_1K_BD_BUF_SIZE  |        63KB       |       63
	 * HNS3_512_BD_BUF_SIZE |      31.5KB       |       63
	 */
	static const uint16_t hw_rx_buf_size[] = {
		HNS3_4K_BD_BUF_SIZE,
		HNS3_2K_BD_BUF_SIZE,
		HNS3_1K_BD_BUF_SIZE,
		HNS3_512_BD_BUF_SIZE
	};

	vld_buf_size = (uint16_t)(rte_pktmbuf_data_room_size(mp) -
			RTE_PKTMBUF_HEADROOM);

	if (vld_buf_size < HNS3_MIN_BD_BUF_SIZE)
		return -EINVAL;

	num_hw_specs = RTE_DIM(hw_rx_buf_size);
	for (i = 0; i < num_hw_specs; i++) {
		if (vld_buf_size >= hw_rx_buf_size[i]) {
			*rx_buf_len = hw_rx_buf_size[i];
			break;
		}
	}
	return 0;
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
	uint16_t rx_buf_size;
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

	if (hns3_rx_buf_len_calc(mp, &rx_buf_size)) {
		hns3_err(hw, "rxq mbufs' data room size:%u is not enough! "
				"minimal data room size:%u.",
				rte_pktmbuf_data_room_size(mp),
				HNS3_MIN_BD_BUF_SIZE + RTE_PKTMBUF_HEADROOM);
		return -EINVAL;
	}

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
	rxq->pvid_state = hw->port_base_vlan_cfg.state;
	rxq->configured = true;
	rxq->io_base = (void *)((char *)hw->io_base + HNS3_TQP_REG_OFFSET +
				idx * HNS3_TQP_REG_SIZE);
	rxq->rx_buf_len = rx_buf_size;
	rxq->l2_errors = 0;
	rxq->pkt_len_errors = 0;
	rxq->l3_csum_erros = 0;
	rxq->l4_csum_erros = 0;
	rxq->ol3_csum_erros = 0;
	rxq->ol4_csum_erros = 0;

	/* CRC len set here is used for amending packet length */
	if (dev->data->dev_conf.rxmode.offloads & DEV_RX_OFFLOAD_KEEP_CRC)
		rxq->crc_len = RTE_ETHER_CRC_LEN;
	else
		rxq->crc_len = 0;

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
		RTE_PTYPE_L2_ETHER_QINQ,
		RTE_PTYPE_L2_ETHER_VLAN,
		RTE_PTYPE_L2_ETHER_VLAN
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

static inline void
hns3_rxd_to_vlan_tci(struct hns3_rx_queue *rxq, struct rte_mbuf *mb,
		     uint32_t l234_info, const struct hns3_desc *rxd)
{
#define HNS3_STRP_STATUS_NUM		0x4

#define HNS3_NO_STRP_VLAN_VLD		0x0
#define HNS3_INNER_STRP_VLAN_VLD	0x1
#define HNS3_OUTER_STRP_VLAN_VLD	0x2
	uint32_t strip_status;
	uint32_t report_mode;

	/*
	 * Since HW limitation, the vlan tag will always be inserted into RX
	 * descriptor when strip the tag from packet, driver needs to determine
	 * reporting which tag to mbuf according to the PVID configuration
	 * and vlan striped status.
	 */
	static const uint32_t report_type[][HNS3_STRP_STATUS_NUM] = {
		{
			HNS3_NO_STRP_VLAN_VLD,
			HNS3_OUTER_STRP_VLAN_VLD,
			HNS3_INNER_STRP_VLAN_VLD,
			HNS3_OUTER_STRP_VLAN_VLD
		},
		{
			HNS3_NO_STRP_VLAN_VLD,
			HNS3_NO_STRP_VLAN_VLD,
			HNS3_NO_STRP_VLAN_VLD,
			HNS3_INNER_STRP_VLAN_VLD
		}
	};
	strip_status = hns3_get_field(l234_info, HNS3_RXD_STRP_TAGP_M,
				      HNS3_RXD_STRP_TAGP_S);
	report_mode = report_type[rxq->pvid_state][strip_status];
	switch (report_mode) {
	case HNS3_NO_STRP_VLAN_VLD:
		mb->vlan_tci = 0;
		return;
	case HNS3_INNER_STRP_VLAN_VLD:
		mb->ol_flags |= PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED;
		mb->vlan_tci = rte_le_to_cpu_16(rxd->rx.vlan_tag);
		return;
	case HNS3_OUTER_STRP_VLAN_VLD:
		mb->ol_flags |= PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED;
		mb->vlan_tci = rte_le_to_cpu_16(rxd->rx.ot_vlan_tag);
		return;
	}
}

static inline void
recalculate_data_len(struct rte_mbuf *first_seg, struct rte_mbuf *last_seg,
		    struct rte_mbuf *rxm, struct hns3_rx_queue *rxq,
		    uint16_t data_len)
{
	uint8_t crc_len = rxq->crc_len;

	if (data_len <= crc_len) {
		rte_pktmbuf_free_seg(rxm);
		first_seg->nb_segs--;
		last_seg->data_len = (uint16_t)(last_seg->data_len -
			(crc_len - data_len));
		last_seg->next = NULL;
	} else
		rxm->data_len = (uint16_t)(data_len - crc_len);
}

uint16_t
hns3_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	volatile struct hns3_desc *rx_ring;  /* RX ring (desc) */
	volatile struct hns3_desc *rxdp;     /* pointer of the current desc */
	struct hns3_rx_queue *rxq;      /* RX queue */
	struct hns3_entry *sw_ring;
	struct hns3_entry *rxe;
	struct rte_mbuf *first_seg;
	struct rte_mbuf *last_seg;
	struct hns3_desc rxd;
	struct rte_mbuf *nmb;           /* pointer of the new mbuf */
	struct rte_mbuf *rxm;
	struct rte_eth_dev *dev;
	uint32_t bd_base_info;
	uint32_t cksum_err;
	uint32_t l234_info;
	uint32_t gro_size;
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
		/*
		 * The interactive process between software and hardware of
		 * receiving a new packet in hns3 network engine:
		 * 1. Hardware network engine firstly writes the packet content
		 *    to the memory pointed by the 'addr' field of the Rx Buffer
		 *    Descriptor, secondly fills the result of parsing the
		 *    packet include the valid field into the Rx Buffer
		 *    Descriptor in one write operation.
		 * 2. Driver reads the Rx BD's valid field in the loop to check
		 *    whether it's valid, if valid then assign a new address to
		 *    the addr field, clear the valid field, get the other
		 *    information of the packet by parsing Rx BD's other fields,
		 *    finally write back the number of Rx BDs processed by the
		 *    driver to the HNS3_RING_RX_HEAD_REG register to inform
		 *    hardware.
		 * In the above process, the ordering is very important. We must
		 * make sure that CPU read Rx BD's other fields only after the
		 * Rx BD is valid.
		 *
		 * There are two type of re-ordering: compiler re-ordering and
		 * CPU re-ordering under the ARMv8 architecture.
		 * 1. we use volatile to deal with compiler re-ordering, so you
		 *    can see that rx_ring/rxdp defined with volatile.
		 * 2. we commonly use memory barrier to deal with CPU
		 *    re-ordering, but the cost is high.
		 *
		 * In order to solve the high cost of using memory barrier, we
		 * use the data dependency order under the ARMv8 architecture,
		 * for example:
		 *      instr01: load A
		 *      instr02: load B <- A
		 * the instr02 will always execute after instr01.
		 *
		 * To construct the data dependency ordering, we use the
		 * following assignment:
		 *      rxd = rxdp[(bd_base_info & (1u << HNS3_RXD_VLD_B)) -
		 *                 (1u<<HNS3_RXD_VLD_B)]
		 * Using gcc compiler under the ARMv8 architecture, the related
		 * assembly code example as follows:
		 * note: (1u << HNS3_RXD_VLD_B) equal 0x10
		 *      instr01: ldr w26, [x22, #28]  --read bd_base_info
		 *      instr02: and w0, w26, #0x10   --calc bd_base_info & 0x10
		 *      instr03: sub w0, w0, #0x10    --calc (bd_base_info &
		 *                                            0x10) - 0x10
		 *      instr04: add x0, x22, x0, lsl #5 --calc copy source addr
		 *      instr05: ldp x2, x3, [x0]
		 *      instr06: stp x2, x3, [x29, #256] --copy BD's [0 ~ 15]B
		 *      instr07: ldp x4, x5, [x0, #16]
		 *      instr08: stp x4, x5, [x29, #272] --copy BD's [16 ~ 31]B
		 * the instr05~08 depend on x0's value, x0 depent on w26's
		 * value, the w26 is the bd_base_info, this form the data
		 * dependency ordering.
		 * note: if BD is valid, (bd_base_info & (1u<<HNS3_RXD_VLD_B)) -
		 *       (1u<<HNS3_RXD_VLD_B) will always zero, so the
		 *       assignment is correct.
		 *
		 * So we use the data dependency ordering instead of memory
		 * barrier to improve receive performance.
		 */
		rxd = rxdp[(bd_base_info & (1u << HNS3_RXD_VLD_B)) -
			   (1u << HNS3_RXD_VLD_B)];

		nmb = rte_mbuf_raw_alloc(rxq->mb_pool);
		if (unlikely(nmb == NULL)) {
			dev = &rte_eth_devices[rxq->port_id];
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
		rxdp->rx.bd_base_info = 0;
		rxdp->addr = dma_addr;

		/*
		 * Load remained descriptor data and extract necessary fields.
		 * Data size from buffer description may contains CRC len,
		 * packet len should subtract it.
		 */
		data_len = (uint16_t)(rte_le_to_cpu_16(rxd.rx.size));
		l234_info = rte_le_to_cpu_32(rxd.rx.l234_info);
		ol_info = rte_le_to_cpu_32(rxd.rx.ol_info);

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

		/*
		 * The last buffer of the received packet. packet len from
		 * buffer description may contains CRC len, packet len should
		 * subtract it, same as data len.
		 */
		pkt_len = (uint16_t)(rte_le_to_cpu_16(rxd.rx.pkt_len));
		first_seg->pkt_len = pkt_len;

		/*
		 * This is the last buffer of the received packet. If the CRC
		 * is not stripped by the hardware:
		 *  - Subtract the CRC length from the total packet length.
		 *  - If the last buffer only contains the whole CRC or a part
		 *  of it, free the mbuf associated to the last buffer. If part
		 *  of the CRC is also contained in the previous mbuf, subtract
		 *  the length of that CRC part from the data length of the
		 *  previous mbuf.
		 */
		rxm->next = NULL;
		if (unlikely(rxq->crc_len > 0)) {
			first_seg->pkt_len -= rxq->crc_len;
			recalculate_data_len(first_seg, last_seg, rxm, rxq,
				data_len);
		}

		first_seg->port = rxq->port_id;
		first_seg->hash.rss = rte_le_to_cpu_32(rxd.rx.rss_hash);
		first_seg->ol_flags = PKT_RX_RSS_HASH;
		if (unlikely(hns3_get_bit(bd_base_info, HNS3_RXD_LUM_B))) {
			first_seg->hash.fdir.hi =
				rte_le_to_cpu_32(rxd.rx.fd_id);
			first_seg->ol_flags |= PKT_RX_FDIR | PKT_RX_FDIR_ID;
		}

		gro_size = hns3_get_field(bd_base_info, HNS3_RXD_GRO_SIZE_M,
					  HNS3_RXD_GRO_SIZE_S);
		if (gro_size != 0) {
			first_seg->ol_flags |= PKT_RX_LRO;
			first_seg->tso_segsz = gro_size;
		}

		ret = hns3_handle_bdinfo(rxq, first_seg, bd_base_info,
					 l234_info, &cksum_err);
		if (unlikely(ret))
			goto pkt_err;

		first_seg->packet_type = rxd_pkt_info_to_pkt_type(l234_info,
								  ol_info);

		if (bd_base_info & BIT(HNS3_RXD_L3L4P_B))
			hns3_rx_set_cksum_flag(first_seg,
					       first_seg->packet_type,
					       cksum_err);
		hns3_rxd_to_vlan_tci(rxq, first_seg, l234_info, &rxd);

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

	nb_rx_bd = nb_rx_bd + rxq->nb_rx_hold;
	if (nb_rx_bd > rxq->rx_free_thresh) {
		hns3_clean_rx_buffers(rxq, nb_rx_bd);
		nb_rx_bd = 0;
	}
	rxq->nb_rx_hold = nb_rx_bd;

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
	txq->pvid_state = hw->port_base_vlan_cfg.state;
	txq->configured = true;
	txq->io_base = (void *)((char *)hw->io_base + HNS3_TQP_REG_OFFSET +
				idx * HNS3_TQP_REG_SIZE);
	txq->over_length_pkt_cnt = 0;
	txq->exceed_limit_bd_pkt_cnt = 0;
	txq->exceed_limit_bd_reassem_fail = 0;
	txq->unsupported_tunnel_pkt_cnt = 0;
	txq->queue_full_cnt = 0;
	txq->pkt_padding_fail_cnt = 0;
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

static int
hns3_tso_proc_tunnel(struct hns3_desc *desc, uint64_t ol_flags,
		     struct rte_mbuf *rxm, uint8_t *l2_len)
{
	uint64_t tun_flags;
	uint8_t ol4_len;
	uint32_t otmp;

	tun_flags = ol_flags & PKT_TX_TUNNEL_MASK;
	if (tun_flags == 0)
		return 0;

	otmp = rte_le_to_cpu_32(desc->tx.ol_type_vlan_len_msec);
	switch (tun_flags) {
	case PKT_TX_TUNNEL_GENEVE:
	case PKT_TX_TUNNEL_VXLAN:
		*l2_len = rxm->l2_len - RTE_ETHER_VXLAN_HLEN;
		break;
	case PKT_TX_TUNNEL_GRE:
		/*
		 * OL4 header size, defined in 4 Bytes, it contains outer
		 * L4(GRE) length and tunneling length.
		 */
		ol4_len = hns3_get_field(otmp, HNS3_TXD_L4LEN_M,
					 HNS3_TXD_L4LEN_S);
		*l2_len = rxm->l2_len - (ol4_len << HNS3_L4_LEN_UNIT);
		break;
	default:
		/* For non UDP / GRE tunneling, drop the tunnel packet */
		return -EINVAL;
	}
	hns3_set_field(otmp, HNS3_TXD_L2LEN_M, HNS3_TXD_L2LEN_S,
		       rxm->outer_l2_len >> HNS3_L2_LEN_UNIT);
	desc->tx.ol_type_vlan_len_msec = rte_cpu_to_le_32(otmp);

	return 0;
}

int
hns3_config_gro(struct hns3_hw *hw, bool en)
{
	struct hns3_cfg_gro_status_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_GRO_GENERIC_CONFIG, false);
	req = (struct hns3_cfg_gro_status_cmd *)desc.data;

	req->gro_en = rte_cpu_to_le_16(en ? 1 : 0);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "%s hardware GRO failed, ret = %d",
			 en ? "enable" : "disable", ret);

	return ret;
}

int
hns3_restore_gro_conf(struct hns3_hw *hw)
{
	uint64_t offloads;
	bool gro_en;
	int ret;

	offloads = hw->data->dev_conf.rxmode.offloads;
	gro_en = offloads & DEV_RX_OFFLOAD_TCP_LRO ? true : false;
	ret = hns3_config_gro(hw, gro_en);
	if (ret)
		hns3_err(hw, "restore hardware GRO to %s failed, ret = %d",
			 gro_en ? "enabled" : "disabled", ret);

	return ret;
}

static inline bool
hns3_pkt_is_tso(struct rte_mbuf *m)
{
	return (m->tso_segsz != 0 && m->ol_flags & PKT_TX_TCP_SEG);
}

static void
hns3_set_tso(struct hns3_desc *desc, uint64_t ol_flags,
		uint32_t paylen, struct rte_mbuf *rxm)
{
	uint8_t l2_len = rxm->l2_len;
	uint32_t tmp;

	if (!hns3_pkt_is_tso(rxm))
		return;

	if (hns3_tso_proc_tunnel(desc, ol_flags, rxm, &l2_len))
		return;

	if (paylen <= rxm->tso_segsz)
		return;

	tmp = rte_le_to_cpu_32(desc->tx.type_cs_vlan_tso_len);
	hns3_set_bit(tmp, HNS3_TXD_TSO_B, 1);
	hns3_set_bit(tmp, HNS3_TXD_L3CS_B, 1);
	hns3_set_field(tmp, HNS3_TXD_L4T_M, HNS3_TXD_L4T_S, HNS3_L4T_TCP);
	hns3_set_bit(tmp, HNS3_TXD_L4CS_B, 1);
	hns3_set_field(tmp, HNS3_TXD_L4LEN_M, HNS3_TXD_L4LEN_S,
		       sizeof(struct rte_tcp_hdr) >> HNS3_L4_LEN_UNIT);
	hns3_set_field(tmp, HNS3_TXD_L2LEN_M, HNS3_TXD_L2LEN_S,
		       l2_len >> HNS3_L2_LEN_UNIT);
	desc->tx.type_cs_vlan_tso_len = rte_cpu_to_le_32(tmp);
	desc->tx.mss = rte_cpu_to_le_16(rxm->tso_segsz);
}

static inline void
hns3_fill_per_desc(struct hns3_desc *desc, struct rte_mbuf *rxm)
{
	desc->addr = rte_mbuf_data_iova(rxm);
	desc->tx.send_size = rte_cpu_to_le_16(rte_pktmbuf_data_len(rxm));
	desc->tx.tp_fe_sc_vld_ra_ri = rte_cpu_to_le_16(BIT(HNS3_TXD_VLD_B));
}

static void
hns3_fill_first_desc(struct hns3_tx_queue *txq, struct hns3_desc *desc,
		     struct rte_mbuf *rxm)
{
	uint64_t ol_flags = rxm->ol_flags;
	uint32_t hdr_len;
	uint32_t paylen;

	hdr_len = rxm->l2_len + rxm->l3_len + rxm->l4_len;
	hdr_len += (ol_flags & PKT_TX_TUNNEL_MASK) ?
			   rxm->outer_l2_len + rxm->outer_l3_len : 0;
	paylen = rxm->pkt_len - hdr_len;
	desc->tx.paylen = rte_cpu_to_le_32(paylen);
	hns3_set_tso(desc, ol_flags, paylen, rxm);

	/*
	 * Currently, hardware doesn't support more than two layers VLAN offload
	 * in Tx direction based on hns3 network engine. So when the number of
	 * VLANs in the packets represented by rxm plus the number of VLAN
	 * offload by hardware such as PVID etc, exceeds two, the packets will
	 * be discarded or the original VLAN of the packets will be overwitted
	 * by hardware. When the PF PVID is enabled by calling the API function
	 * named rte_eth_dev_set_vlan_pvid or the VF PVID is enabled by the hns3
	 * PF kernel ether driver, the outer VLAN tag will always be the PVID.
	 * To avoid the VLAN of Tx descriptor is overwritten by PVID, it should
	 * be added to the position close to the IP header when PVID is enabled.
	 */
	if (!txq->pvid_state && ol_flags & (PKT_TX_VLAN_PKT |
				PKT_TX_QINQ_PKT)) {
		desc->tx.ol_type_vlan_len_msec |=
				rte_cpu_to_le_32(BIT(HNS3_TXD_OVLAN_B));
		if (ol_flags & PKT_TX_QINQ_PKT)
			desc->tx.outer_vlan_tag =
					rte_cpu_to_le_16(rxm->vlan_tci_outer);
		else
			desc->tx.outer_vlan_tag =
					rte_cpu_to_le_16(rxm->vlan_tci);
	}

	if (ol_flags & PKT_TX_QINQ_PKT ||
	    ((ol_flags & PKT_TX_VLAN_PKT) && txq->pvid_state)) {
		desc->tx.type_cs_vlan_tso_len |=
					rte_cpu_to_le_32(BIT(HNS3_TXD_VLAN_B));
		desc->tx.vlan_tag = rte_cpu_to_le_16(rxm->vlan_tci);
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

static inline void
hns3_pktmbuf_copy_hdr(struct rte_mbuf *new_pkt, struct rte_mbuf *old_pkt)
{
	new_pkt->ol_flags = old_pkt->ol_flags;
	new_pkt->pkt_len = rte_pktmbuf_pkt_len(old_pkt);
	new_pkt->outer_l2_len = old_pkt->outer_l2_len;
	new_pkt->outer_l3_len = old_pkt->outer_l3_len;
	new_pkt->l2_len = old_pkt->l2_len;
	new_pkt->l3_len = old_pkt->l3_len;
	new_pkt->l4_len = old_pkt->l4_len;
	new_pkt->vlan_tci_outer = old_pkt->vlan_tci_outer;
	new_pkt->vlan_tci = old_pkt->vlan_tci;
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
	nb_new_buf = (rte_pktmbuf_pkt_len(tx_pkt) - 1) / buf_size + 1;
	if (nb_new_buf > HNS3_MAX_NON_TSO_BD_PER_PKT)
		return -EINVAL;

	last_buf_len = rte_pktmbuf_pkt_len(tx_pkt) % buf_size;
	if (last_buf_len == 0)
		last_buf_len = buf_size;

	/* Allocate enough mbufs */
	ret = hns3_tx_alloc_mbufs(txq, mb_pool, nb_new_buf, &new_mbuf);
	if (ret)
		return ret;

	/* Copy the original packet content to the new mbufs */
	temp = tx_pkt;
	s = rte_pktmbuf_mtod(temp, char *);
	len_s = rte_pktmbuf_data_len(temp);
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
				len_s = rte_pktmbuf_data_len(temp);
			}
		}

		temp_new->data_len = buf_len;
		temp_new = temp_new->next;
	}
	hns3_pktmbuf_copy_hdr(new_mbuf, tx_pkt);

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

static bool
hns3_pkt_need_linearized(struct rte_mbuf *tx_pkts, uint32_t bd_num)
{
	struct rte_mbuf *m_first = tx_pkts;
	struct rte_mbuf *m_last = tx_pkts;
	uint32_t tot_len = 0;
	uint32_t hdr_len;
	uint32_t i;

	/*
	 * Hardware requires that the sum of the data length of every 8
	 * consecutive buffers is greater than MSS in hns3 network engine.
	 * We simplify it by ensuring pkt_headlen + the first 8 consecutive
	 * frags greater than gso header len + mss, and the remaining 7
	 * consecutive frags greater than MSS except the last 7 frags.
	 */
	if (bd_num <= HNS3_MAX_NON_TSO_BD_PER_PKT)
		return false;

	for (i = 0; m_last && i < HNS3_MAX_NON_TSO_BD_PER_PKT - 1;
	     i++, m_last = m_last->next)
		tot_len += m_last->data_len;

	if (!m_last)
		return true;

	/* ensure the first 8 frags is greater than mss + header */
	hdr_len = tx_pkts->l2_len + tx_pkts->l3_len + tx_pkts->l4_len;
	hdr_len += (tx_pkts->ol_flags & PKT_TX_TUNNEL_MASK) ?
		   tx_pkts->outer_l2_len + tx_pkts->outer_l3_len : 0;
	if (tot_len + m_last->data_len < tx_pkts->tso_segsz + hdr_len)
		return true;

	/*
	 * ensure the sum of the data length of every 7 consecutive buffer
	 * is greater than mss except the last one.
	 */
	for (i = 0; m_last && i < bd_num - HNS3_MAX_NON_TSO_BD_PER_PKT; i++) {
		tot_len -= m_first->data_len;
		tot_len += m_last->data_len;

		if (tot_len < tx_pkts->tso_segsz)
			return true;

		m_first = m_first->next;
		m_last = m_last->next;
	}

	return false;
}

static void
hns3_outer_header_cksum_prepare(struct rte_mbuf *m)
{
	uint64_t ol_flags = m->ol_flags;
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_udp_hdr *udp_hdr;
	uint32_t paylen, hdr_len;

	if (!(ol_flags & (PKT_TX_OUTER_IPV4 | PKT_TX_OUTER_IPV6)))
		return;

	if (ol_flags & PKT_TX_IPV4) {
		ipv4_hdr = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *,
						   m->outer_l2_len);

		if (ol_flags & PKT_TX_IP_CKSUM)
			ipv4_hdr->hdr_checksum = 0;
	}

	if ((ol_flags & PKT_TX_L4_MASK) == PKT_TX_UDP_CKSUM &&
	    ol_flags & PKT_TX_TCP_SEG) {
		hdr_len = m->l2_len + m->l3_len + m->l4_len;
		hdr_len += (ol_flags & PKT_TX_TUNNEL_MASK) ?
				m->outer_l2_len + m->outer_l3_len : 0;
		paylen = m->pkt_len - hdr_len;
		if (paylen <= m->tso_segsz)
			return;
		udp_hdr = rte_pktmbuf_mtod_offset(m, struct rte_udp_hdr *,
						  m->outer_l2_len +
						  m->outer_l3_len);
		udp_hdr->dgram_cksum = 0;
	}
}

static int
hns3_check_tso_pkt_valid(struct rte_mbuf *m)
{
	uint32_t tmp_data_len_sum = 0;
	uint16_t nb_buf = m->nb_segs;
	uint32_t paylen, hdr_len;
	struct rte_mbuf *m_seg;
	int i;

	if (nb_buf > HNS3_MAX_TSO_BD_PER_PKT)
		return -EINVAL;

	hdr_len = m->l2_len + m->l3_len + m->l4_len;
	hdr_len += (m->ol_flags & PKT_TX_TUNNEL_MASK) ?
			m->outer_l2_len + m->outer_l3_len : 0;
	if (hdr_len > HNS3_MAX_TSO_HDR_SIZE)
		return -EINVAL;

	paylen = m->pkt_len - hdr_len;
	if (paylen > HNS3_MAX_BD_PAYLEN)
		return -EINVAL;

	/*
	 * The TSO header (include outer and inner L2, L3 and L4 header)
	 * should be provided by three descriptors in maximum in hns3 network
	 * engine.
	 */
	m_seg = m;
	for (i = 0; m_seg != NULL && i < HNS3_MAX_TSO_HDR_BD_NUM && i < nb_buf;
	     i++, m_seg = m_seg->next) {
		tmp_data_len_sum += m_seg->data_len;
	}

	if (hdr_len > tmp_data_len_sum)
		return -EINVAL;

	return 0;
}

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
static inline int
hns3_vld_vlan_chk(struct hns3_tx_queue *txq, struct rte_mbuf *m)
{
	struct rte_ether_hdr *eh;
	struct rte_vlan_hdr *vh;

	if (!txq->pvid_state)
		return 0;

	/*
	 * Due to hardware limitations, we only support two-layer VLAN hardware
	 * offload in Tx direction based on hns3 network engine, so when PVID is
	 * enabled, QinQ insert is no longer supported.
	 * And when PVID is enabled, in the following two cases:
	 *  i) packets with more than two VLAN tags.
	 *  ii) packets with one VLAN tag while the hardware VLAN insert is
	 *      enabled.
	 * The packets will be regarded as abnormal packets and discarded by
	 * hardware in Tx direction. For debugging purposes, a validation check
	 * for these types of packets is added to the '.tx_pkt_prepare' ops
	 * implementation function named hns3_prep_pkts to inform users that
	 * these packets will be discarded.
	 */
	if (m->ol_flags & PKT_TX_QINQ_PKT)
		return -EINVAL;

	eh = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	if (eh->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN)) {
		if (m->ol_flags & PKT_TX_VLAN_PKT)
			return -EINVAL;

		/* Ensure the incoming packet is not a QinQ packet */
		vh = (struct rte_vlan_hdr *)(eh + 1);
		if (vh->eth_proto == rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN))
			return -EINVAL;
	}

	return 0;
}
#endif

uint16_t
hns3_prep_pkts(__rte_unused void *tx_queue, struct rte_mbuf **tx_pkts,
	       uint16_t nb_pkts)
{
	struct rte_mbuf *m;
	uint16_t i;
	int ret;

	for (i = 0; i < nb_pkts; i++) {
		m = tx_pkts[i];

		if (hns3_pkt_is_tso(m) &&
		    (hns3_pkt_need_linearized(m, m->nb_segs) ||
		     hns3_check_tso_pkt_valid(m))) {
			rte_errno = EINVAL;
			return i;
		}

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
		ret = rte_validate_tx_offload(m);
		if (ret != 0) {
			rte_errno = -ret;
			return i;
		}

		if (hns3_vld_vlan_chk(tx_queue, m)) {
			rte_errno = EINVAL;
			return i;
		}
#endif
		ret = rte_net_intel_cksum_prepare(m);
		if (ret != 0) {
			rte_errno = -ret;
			return i;
		}

		hns3_outer_header_cksum_prepare(m);
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
						hdr_lens)) {
			txq->unsupported_tunnel_pkt_cnt++;
			return -EINVAL;
		}
	}
	/* Enable checksum offloading */
	if (m->ol_flags & HNS3_TX_CKSUM_OFFLOAD_MASK)
		hns3_txd_enable_checksum(txq, tx_desc_id, m->ol_flags);

	return 0;
}

static int
hns3_check_non_tso_pkt(uint16_t nb_buf, struct rte_mbuf **m_seg,
		      struct rte_mbuf *tx_pkt, struct hns3_tx_queue *txq)
{
	struct rte_mbuf *new_pkt;
	int ret;

	if (hns3_pkt_is_tso(*m_seg))
		return 0;

	/*
	 * If packet length is greater than HNS3_MAX_FRAME_LEN
	 * driver support, the packet will be ignored.
	 */
	if (unlikely(rte_pktmbuf_pkt_len(tx_pkt) > HNS3_MAX_FRAME_LEN)) {
		txq->over_length_pkt_cnt++;
		return -EINVAL;
	}

	if (unlikely(nb_buf > HNS3_MAX_NON_TSO_BD_PER_PKT)) {
		txq->exceed_limit_bd_pkt_cnt++;
		ret = hns3_reassemble_tx_pkts(txq, tx_pkt, &new_pkt);
		if (ret) {
			txq->exceed_limit_bd_reassem_fail++;
			return ret;
		}
		*m_seg = new_pkt;
	}

	return 0;
}

uint16_t
hns3_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct rte_net_hdr_lens hdr_lens = {0};
	struct hns3_tx_queue *txq = tx_queue;
	struct hns3_entry *tx_bak_pkt;
	struct hns3_desc *tx_ring;
	struct rte_mbuf *tx_pkt;
	struct rte_mbuf *m_seg;
	struct hns3_desc *desc;
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
	tx_ring = txq->tx_ring;

	/* send packets */
	tx_bak_pkt = &txq->sw_ring[tx_next_use];
	for (nb_tx = 0; nb_tx < tx_pkt_num; nb_tx++) {
		tx_pkt = *tx_pkts++;

		nb_buf = tx_pkt->nb_segs;

		if (nb_buf > txq->tx_bd_ready) {
			txq->queue_full_cnt++;
			if (nb_tx == 0)
				return 0;

			goto end_of_tx;
		}

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
			if (appended == NULL) {
				txq->pkt_padding_fail_cnt++;
				break;
			}

			memset(appended, 0, add_len);
		}

		m_seg = tx_pkt;

		if (hns3_check_non_tso_pkt(nb_buf, &m_seg, tx_pkt, txq))
			goto end_of_tx;

		if (hns3_parse_cksum(txq, tx_next_use, m_seg, &hdr_lens))
			goto end_of_tx;

		i = 0;
		desc = &tx_ring[tx_next_use];

		/*
		 * If the packet is divided into multiple Tx Buffer Descriptors,
		 * only need to fill vlan, paylen and tso into the first Tx
		 * Buffer Descriptor.
		 */
		hns3_fill_first_desc(txq, desc, m_seg);

		do {
			desc = &tx_ring[tx_next_use];
			/*
			 * Fill valid bits, DMA address and data length for each
			 * Tx Buffer Descriptor.
			 */
			hns3_fill_per_desc(desc, m_seg);
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

		/* Add end flag for the last Tx Buffer Descriptor */
		desc->tx.tp_fe_sc_vld_ra_ri |=
				 rte_cpu_to_le_16(BIT(HNS3_TXD_FE_B));

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
