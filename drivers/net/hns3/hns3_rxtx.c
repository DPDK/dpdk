/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#include <bus_pci_driver.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_geneve.h>
#include <rte_vxlan.h>
#include <ethdev_driver.h>
#include <rte_io.h>
#include <rte_net.h>
#include <rte_malloc.h>
#if defined(RTE_ARCH_ARM64)
#include <rte_cpuflags.h>
#include <rte_vect.h>
#endif

#include "hns3_common.h"
#include "hns3_regs.h"
#include "hns3_logs.h"
#include "hns3_mp.h"
#include "hns3_rxtx.h"

#define HNS3_CFG_DESC_NUM(num)	((num) / 8 - 1)
#define HNS3_RX_RING_PREFETCTH_MASK	3

static void
hns3_rx_queue_release_mbufs(struct hns3_rx_queue *rxq)
{
	uint16_t i;

	/* Note: Fake rx queue will not enter here */
	if (rxq->sw_ring == NULL)
		return;

	if (rxq->rx_rearm_nb == 0) {
		for (i = 0; i < rxq->nb_rx_desc; i++) {
			if (rxq->sw_ring[i].mbuf != NULL) {
				rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
				rxq->sw_ring[i].mbuf = NULL;
			}
		}
	} else {
		for (i = rxq->next_to_use;
		     i != rxq->rx_rearm_start;
		     i = (i + 1) % rxq->nb_rx_desc) {
			if (rxq->sw_ring[i].mbuf != NULL) {
				rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
				rxq->sw_ring[i].mbuf = NULL;
			}
		}
	}

	for (i = 0; i < rxq->bulk_mbuf_num; i++)
		rte_pktmbuf_free_seg(rxq->bulk_mbuf[i]);
	rxq->bulk_mbuf_num = 0;

	if (rxq->pkt_first_seg) {
		rte_pktmbuf_free(rxq->pkt_first_seg);
		rxq->pkt_first_seg = NULL;
	}
}

static void
hns3_tx_queue_release_mbufs(struct hns3_tx_queue *txq)
{
	uint16_t i;

	/* Note: Fake tx queue will not enter here */
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
		rte_free(txq->sw_ring);
		rte_free(txq->free);
		rte_free(txq);
	}
}

static void
hns3_rx_queue_release_lock(void *queue)
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
hns3_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t queue_id)
{
	hns3_rx_queue_release_lock(dev->data->rx_queues[queue_id]);
}

static void
hns3_tx_queue_release_lock(void *queue)
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

void
hns3_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t queue_id)
{
	hns3_tx_queue_release_lock(dev->data->tx_queues[queue_id]);
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
	if (idx == hw->fkq_data.nb_fake_rx_queues - 1) {
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
	if (idx == hw->fkq_data.nb_fake_tx_queues - 1) {
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
			hns3_err(hw, "Failed to allocate RXD[%u] for rx queue!",
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
		       (uint32_t)(dma_addr >> 32));

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
		       (uint32_t)(dma_addr >> 32));

	hns3_write_dev(txq, HNS3_RING_TX_BD_NUM_REG,
		       HNS3_CFG_DESC_NUM(txq->nb_tx_desc));
}

void
hns3_update_all_queues_pvid_proc_en(struct hns3_hw *hw)
{
	uint16_t nb_rx_q = hw->data->nb_rx_queues;
	uint16_t nb_tx_q = hw->data->nb_tx_queues;
	struct hns3_rx_queue *rxq;
	struct hns3_tx_queue *txq;
	bool pvid_en;
	int i;

	pvid_en = hw->port_base_vlan_cfg.state == HNS3_PORT_BASE_VLAN_ENABLE;
	for (i = 0; i < hw->cfg_max_queues; i++) {
		if (i < nb_rx_q) {
			rxq = hw->data->rx_queues[i];
			if (rxq != NULL)
				rxq->pvid_sw_discard_en = pvid_en;
		}
		if (i < nb_tx_q) {
			txq = hw->data->tx_queues[i];
			if (txq != NULL)
				txq->pvid_sw_shift_en = pvid_en;
		}
	}
}

static void
hns3_stop_unused_queue(void *tqp_base, enum hns3_ring_type queue_type)
{
	uint32_t reg_offset;
	uint32_t reg;

	reg_offset = queue_type == HNS3_RING_TYPE_TX ?
				   HNS3_RING_TX_EN_REG : HNS3_RING_RX_EN_REG;
	reg = hns3_read_reg(tqp_base, reg_offset);
	reg &= ~BIT(HNS3_RING_EN_B);
	hns3_write_reg(tqp_base, reg_offset, reg);
}

void
hns3_enable_all_queues(struct hns3_hw *hw, bool en)
{
	uint16_t nb_rx_q = hw->data->nb_rx_queues;
	uint16_t nb_tx_q = hw->data->nb_tx_queues;
	struct hns3_rx_queue *rxq;
	struct hns3_tx_queue *txq;
	uint32_t rcb_reg;
	void *tqp_base;
	uint16_t i;

	for (i = 0; i < hw->cfg_max_queues; i++) {
		if (hns3_dev_get_support(hw, INDEP_TXRX)) {
			rxq = i < nb_rx_q ? hw->data->rx_queues[i] : NULL;
			txq = i < nb_tx_q ? hw->data->tx_queues[i] : NULL;

			tqp_base = (void *)((char *)hw->io_base +
					hns3_get_tqp_reg_offset(i));
			/*
			 * If queue struct is not initialized, it means the
			 * related HW ring has not been initialized yet.
			 * So, these queues should be disabled before enable
			 * the tqps to avoid a HW exception since the queues
			 * are enabled by default.
			 */
			if (rxq == NULL)
				hns3_stop_unused_queue(tqp_base,
							HNS3_RING_TYPE_RX);
			if (txq == NULL)
				hns3_stop_unused_queue(tqp_base,
							HNS3_RING_TYPE_TX);
		} else {
			rxq = i < nb_rx_q ? hw->data->rx_queues[i] :
			      hw->fkq_data.rx_queues[i - nb_rx_q];

			tqp_base = rxq->io_base;
		}
		/*
		 * This is the master switch that used to control the enabling
		 * of a pair of Tx and Rx queues. Both the Rx and Tx point to
		 * the same register
		 */
		rcb_reg = hns3_read_reg(tqp_base, HNS3_RING_EN_REG);
		if (en)
			rcb_reg |= BIT(HNS3_RING_EN_B);
		else
			rcb_reg &= ~BIT(HNS3_RING_EN_B);
		hns3_write_reg(tqp_base, HNS3_RING_EN_REG, rcb_reg);
	}
}

static void
hns3_enable_txq(struct hns3_tx_queue *txq, bool en)
{
	struct hns3_hw *hw = &txq->hns->hw;
	uint32_t reg;

	if (hns3_dev_get_support(hw, INDEP_TXRX)) {
		reg = hns3_read_dev(txq, HNS3_RING_TX_EN_REG);
		if (en)
			reg |= BIT(HNS3_RING_EN_B);
		else
			reg &= ~BIT(HNS3_RING_EN_B);
		hns3_write_dev(txq, HNS3_RING_TX_EN_REG, reg);
	}
	txq->enabled = en;
}

static void
hns3_enable_rxq(struct hns3_rx_queue *rxq, bool en)
{
	struct hns3_hw *hw = &rxq->hns->hw;
	uint32_t reg;

	if (hns3_dev_get_support(hw, INDEP_TXRX)) {
		reg = hns3_read_dev(rxq, HNS3_RING_RX_EN_REG);
		if (en)
			reg |= BIT(HNS3_RING_EN_B);
		else
			reg &= ~BIT(HNS3_RING_EN_B);
		hns3_write_dev(rxq, HNS3_RING_RX_EN_REG, reg);
	}
	rxq->enabled = en;
}

int
hns3_start_all_txqs(struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_tx_queue *txq;
	uint16_t i, j;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = hw->data->tx_queues[i];
		if (!txq) {
			hns3_err(hw, "Tx queue %u not available or setup.", i);
			goto start_txqs_fail;
		}
		/*
		 * Tx queue is enabled by default. Therefore, the Tx queues
		 * needs to be disabled when deferred_start is set. There is
		 * another master switch used to control the enabling of a pair
		 * of Tx and Rx queues. And the master switch is disabled by
		 * default.
		 */
		if (txq->tx_deferred_start)
			hns3_enable_txq(txq, false);
		else
			hns3_enable_txq(txq, true);
	}
	return 0;

start_txqs_fail:
	for (j = 0; j < i; j++) {
		txq = hw->data->tx_queues[j];
		hns3_enable_txq(txq, false);
	}
	return -EINVAL;
}

int
hns3_start_all_rxqs(struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_rx_queue *rxq;
	uint16_t i, j;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = hw->data->rx_queues[i];
		if (!rxq) {
			hns3_err(hw, "Rx queue %u not available or setup.", i);
			goto start_rxqs_fail;
		}
		/*
		 * Rx queue is enabled by default. Therefore, the Rx queues
		 * needs to be disabled when deferred_start is set. There is
		 * another master switch used to control the enabling of a pair
		 * of Tx and Rx queues. And the master switch is disabled by
		 * default.
		 */
		if (rxq->rx_deferred_start)
			hns3_enable_rxq(rxq, false);
		else
			hns3_enable_rxq(rxq, true);
	}
	return 0;

start_rxqs_fail:
	for (j = 0; j < i; j++) {
		rxq = hw->data->rx_queues[j];
		hns3_enable_rxq(rxq, false);
	}
	return -EINVAL;
}

void
hns3_restore_tqp_enable_state(struct hns3_hw *hw)
{
	struct hns3_rx_queue *rxq;
	struct hns3_tx_queue *txq;
	uint16_t i;

	for (i = 0; i < hw->data->nb_rx_queues; i++) {
		rxq = hw->data->rx_queues[i];
		if (rxq != NULL)
			hns3_enable_rxq(rxq, rxq->enabled);
	}

	for (i = 0; i < hw->data->nb_tx_queues; i++) {
		txq = hw->data->tx_queues[i];
		if (txq != NULL)
			hns3_enable_txq(txq, txq->enabled);
	}
}

void
hns3_stop_all_txqs(struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_tx_queue *txq;
	uint16_t i;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = hw->data->tx_queues[i];
		if (!txq)
			continue;
		hns3_enable_txq(txq, false);
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
	req->tqp_id = rte_cpu_to_le_16(queue_id);
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
	req->tqp_id = rte_cpu_to_le_16(queue_id);
	hns3_set_bit(req->reset_req, HNS3_TQP_RESET_B, enable ? 1 : 0);
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "send tqp reset cmd error, queue_id = %u, ret = %d",
			 queue_id, ret);

	return ret;
}

static int
hns3_get_tqp_reset_status(struct hns3_hw *hw, uint16_t queue_id,
			  uint8_t *reset_status)
{
	struct hns3_reset_tqp_queue_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RESET_TQP_QUEUE, true);

	req = (struct hns3_reset_tqp_queue_cmd *)desc.data;
	req->tqp_id = rte_cpu_to_le_16(queue_id);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "get tqp reset status error, queue_id = %u, ret = %d.",
			 queue_id, ret);
		return ret;
	}
	*reset_status = hns3_get_bit(req->ready_to_reset, HNS3_TQP_RESET_B);
	return ret;
}

static int
hns3pf_reset_tqp(struct hns3_hw *hw, uint16_t queue_id)
{
#define HNS3_TQP_RESET_TRY_MS	200
	uint16_t wait_time = 0;
	uint8_t reset_status;
	int ret;

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

	do {
		/* Wait for tqp hw reset */
		rte_delay_ms(HNS3_POLL_RESPONE_MS);
		wait_time += HNS3_POLL_RESPONE_MS;
		ret = hns3_get_tqp_reset_status(hw, queue_id, &reset_status);
		if (ret)
			goto tqp_reset_fail;

		if (reset_status)
			break;
	} while (wait_time < HNS3_TQP_RESET_TRY_MS);

	if (!reset_status) {
		ret = -ETIMEDOUT;
		hns3_err(hw, "reset tqp timeout, queue_id = %u, ret = %d",
			 queue_id, ret);
		goto tqp_reset_fail;
	}

	ret = hns3_send_reset_tqp_cmd(hw, queue_id, false);
	if (ret)
		hns3_err(hw, "Deassert the soft reset fail, ret = %d", ret);

	return ret;

tqp_reset_fail:
	hns3_send_reset_tqp_cmd(hw, queue_id, false);
	return ret;
}

static int
hns3vf_reset_tqp(struct hns3_hw *hw, uint16_t queue_id)
{
	uint8_t msg_data[2];
	int ret;

	memcpy(msg_data, &queue_id, sizeof(uint16_t));

	ret = hns3_send_mbx_msg(hw, HNS3_MBX_QUEUE_RESET, 0, msg_data,
				 sizeof(msg_data), true, NULL, 0);
	if (ret)
		hns3_err(hw, "fail to reset tqp, queue_id = %u, ret = %d.",
			 queue_id, ret);
	return ret;
}

static int
hns3_reset_rcb_cmd(struct hns3_hw *hw, uint8_t *reset_status)
{
	struct hns3_reset_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_CFG_RST_TRIGGER, false);
	req = (struct hns3_reset_cmd *)desc.data;
	hns3_set_bit(req->fun_reset_rcb, HNS3_CFG_RESET_RCB_B, 1);

	/*
	 * The start qid should be the global qid of the first tqp of the
	 * function which should be reset in this port. Since our PF not
	 * support take over of VFs, so we only need to reset function 0,
	 * and its start qid is always 0.
	 */
	req->fun_reset_rcb_vqid_start = rte_cpu_to_le_16(0);
	req->fun_reset_rcb_vqid_num = rte_cpu_to_le_16(hw->cfg_max_queues);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "fail to send rcb reset cmd, ret = %d.", ret);
		return ret;
	}

	*reset_status = req->fun_reset_rcb_return_status;
	return 0;
}

static int
hns3pf_reset_all_tqps(struct hns3_hw *hw)
{
#define HNS3_RESET_RCB_NOT_SUPPORT	0U
#define HNS3_RESET_ALL_TQP_SUCCESS	1U
	uint8_t reset_status;
	uint16_t i;
	int ret;

	ret = hns3_reset_rcb_cmd(hw, &reset_status);
	if (ret)
		return ret;

	/*
	 * If the firmware version is low, it may not support the rcb reset
	 * which means reset all the tqps at a time. In this case, we should
	 * reset tqps one by one.
	 */
	if (reset_status == HNS3_RESET_RCB_NOT_SUPPORT) {
		for (i = 0; i < hw->cfg_max_queues; i++) {
			ret = hns3pf_reset_tqp(hw, i);
			if (ret) {
				hns3_err(hw, "fail to reset tqp, queue_id = %d, ret = %d.",
					 i, ret);
				return ret;
			}
		}
	} else if (reset_status != HNS3_RESET_ALL_TQP_SUCCESS) {
		hns3_err(hw, "fail to reset all tqps, reset_status = %u.",
			 reset_status);
		return -EIO;
	}

	return 0;
}

static int
hns3vf_reset_all_tqps(struct hns3_hw *hw)
{
#define HNS3VF_RESET_ALL_TQP_DONE	1U
	uint8_t reset_status;
	uint8_t msg_data[2];
	int ret;
	uint16_t i;

	memset(msg_data, 0, sizeof(msg_data));
	ret = hns3_send_mbx_msg(hw, HNS3_MBX_QUEUE_RESET, 0, msg_data,
				sizeof(msg_data), true, &reset_status,
				sizeof(reset_status));
	if (ret) {
		hns3_err(hw, "fail to send rcb reset mbx, ret = %d.", ret);
		return ret;
	}

	if (reset_status == HNS3VF_RESET_ALL_TQP_DONE)
		return 0;

	/*
	 * If the firmware version or kernel PF version is low, it may not
	 * support the rcb reset which means reset all the tqps at a time.
	 * In this case, we should reset tqps one by one.
	 */
	for (i = 1; i < hw->cfg_max_queues; i++) {
		ret = hns3vf_reset_tqp(hw, i);
		if (ret)
			return ret;
	}

	return 0;
}

int
hns3_reset_all_tqps(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	uint16_t i;
	int ret;

	/* Disable all queues before reset all queues */
	for (i = 0; i < hw->cfg_max_queues; i++) {
		ret = hns3_tqp_enable(hw, i, false);
		if (ret) {
			hns3_err(hw, "fail to disable tqps before tqps reset, ret = %d.",
				 ret);
			return ret;
		}
	}

	if (hns->is_vf)
		return hns3vf_reset_all_tqps(hw);
	else
		return hns3pf_reset_all_tqps(hw);
}

static int
hns3_send_reset_queue_cmd(struct hns3_hw *hw, uint16_t queue_id,
			  enum hns3_ring_type queue_type, bool enable)
{
	struct hns3_reset_tqp_queue_cmd *req;
	struct hns3_cmd_desc desc;
	int queue_direction;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RESET_TQP_QUEUE_INDEP, false);

	req = (struct hns3_reset_tqp_queue_cmd *)desc.data;
	req->tqp_id = rte_cpu_to_le_16(queue_id);
	queue_direction = queue_type == HNS3_RING_TYPE_TX ? 0 : 1;
	req->queue_direction = rte_cpu_to_le_16(queue_direction);
	hns3_set_bit(req->reset_req, HNS3_TQP_RESET_B, enable ? 1 : 0);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "send queue reset cmd error, queue_id = %u, "
			 "queue_type = %s, ret = %d.", queue_id,
			 queue_type == HNS3_RING_TYPE_TX ? "Tx" : "Rx", ret);
	return ret;
}

static int
hns3_get_queue_reset_status(struct hns3_hw *hw, uint16_t queue_id,
			    enum hns3_ring_type queue_type,
			    uint8_t *reset_status)
{
	struct hns3_reset_tqp_queue_cmd *req;
	struct hns3_cmd_desc desc;
	int queue_direction;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RESET_TQP_QUEUE_INDEP, true);

	req = (struct hns3_reset_tqp_queue_cmd *)desc.data;
	req->tqp_id = rte_cpu_to_le_16(queue_id);
	queue_direction = queue_type == HNS3_RING_TYPE_TX ? 0 : 1;
	req->queue_direction = rte_cpu_to_le_16(queue_direction);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "get queue reset status error, queue_id = %u "
			 "queue_type = %s, ret = %d.", queue_id,
			 queue_type == HNS3_RING_TYPE_TX ? "Tx" : "Rx", ret);
		return ret;
	}

	*reset_status = hns3_get_bit(req->ready_to_reset, HNS3_TQP_RESET_B);
	return  ret;
}

static int
hns3_reset_queue(struct hns3_hw *hw, uint16_t queue_id,
		 enum hns3_ring_type queue_type)
{
#define HNS3_QUEUE_RESET_TRY_MS	200
	struct hns3_tx_queue *txq;
	struct hns3_rx_queue *rxq;
	uint32_t reset_wait_times;
	uint32_t max_wait_times;
	uint8_t reset_status;
	int ret;

	if (queue_type == HNS3_RING_TYPE_TX) {
		txq = hw->data->tx_queues[queue_id];
		hns3_enable_txq(txq, false);
	} else {
		rxq = hw->data->rx_queues[queue_id];
		hns3_enable_rxq(rxq, false);
	}

	ret = hns3_send_reset_queue_cmd(hw, queue_id, queue_type, true);
	if (ret) {
		hns3_err(hw, "send reset queue cmd fail, ret = %d.", ret);
		return ret;
	}

	reset_wait_times = 0;
	max_wait_times = HNS3_QUEUE_RESET_TRY_MS / HNS3_POLL_RESPONE_MS;
	while (reset_wait_times < max_wait_times) {
		/* Wait for queue hw reset */
		rte_delay_ms(HNS3_POLL_RESPONE_MS);
		ret = hns3_get_queue_reset_status(hw, queue_id,
						queue_type, &reset_status);
		if (ret)
			goto queue_reset_fail;

		if (reset_status)
			break;
		reset_wait_times++;
	}

	if (!reset_status) {
		hns3_err(hw, "reset queue timeout, queue_id = %u, queue_type = %s",
			 queue_id,
			 queue_type == HNS3_RING_TYPE_TX ? "Tx" : "Rx");
		ret = -ETIMEDOUT;
		goto queue_reset_fail;
	}

	ret = hns3_send_reset_queue_cmd(hw, queue_id, queue_type, false);
	if (ret)
		hns3_err(hw, "deassert queue reset fail, ret = %d.", ret);

	return ret;

queue_reset_fail:
	hns3_send_reset_queue_cmd(hw, queue_id, queue_type, false);
	return ret;
}

uint32_t
hns3_get_tqp_intr_reg_offset(uint16_t tqp_intr_id)
{
	uint32_t reg_offset;

	/* Need an extend offset to config queues > 64 */
	if (tqp_intr_id < HNS3_MIN_EXT_TQP_INTR_ID)
		reg_offset = HNS3_TQP_INTR_REG_BASE +
			     tqp_intr_id * HNS3_TQP_INTR_LOW_ORDER_OFFSET;
	else
		reg_offset = HNS3_TQP_INTR_EXT_REG_BASE +
			     tqp_intr_id / HNS3_MIN_EXT_TQP_INTR_ID *
			     HNS3_TQP_INTR_HIGH_ORDER_OFFSET +
			     tqp_intr_id % HNS3_MIN_EXT_TQP_INTR_ID *
			     HNS3_TQP_INTR_LOW_ORDER_OFFSET;

	return reg_offset;
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

	addr = offset[gl_idx] + hns3_get_tqp_intr_reg_offset(queue_id);
	if (hw->intr.gl_unit == HNS3_INTR_COALESCE_GL_UINT_1US)
		value = gl_value | HNS3_TQP_INTR_GL_UNIT_1US;
	else
		value = HNS3_GL_USEC_TO_REG(gl_value);

	hns3_write_dev(hw, addr, value);
}

void
hns3_set_queue_intr_rl(struct hns3_hw *hw, uint16_t queue_id, uint16_t rl_value)
{
	uint32_t addr, value;

	if (rl_value > HNS3_TQP_INTR_RL_MAX)
		return;

	addr = HNS3_TQP_INTR_RL_REG + hns3_get_tqp_intr_reg_offset(queue_id);
	value = HNS3_RL_USEC_TO_REG(rl_value);
	if (value > 0)
		value |= HNS3_TQP_INTR_RL_ENABLE_MASK;

	hns3_write_dev(hw, addr, value);
}

void
hns3_set_queue_intr_ql(struct hns3_hw *hw, uint16_t queue_id, uint16_t ql_value)
{
	uint32_t addr;

	/*
	 * int_ql_max == 0 means the hardware does not support QL,
	 * QL regs config is not permitted if QL is not supported,
	 * here just return.
	 */
	if (hw->intr.int_ql_max == HNS3_INTR_QL_NONE)
		return;

	addr = HNS3_TQP_INTR_TX_QL_REG + hns3_get_tqp_intr_reg_offset(queue_id);
	hns3_write_dev(hw, addr, ql_value);

	addr = HNS3_TQP_INTR_RX_QL_REG + hns3_get_tqp_intr_reg_offset(queue_id);
	hns3_write_dev(hw, addr, ql_value);
}

static void
hns3_queue_intr_enable(struct hns3_hw *hw, uint16_t queue_id, bool en)
{
	uint32_t addr, value;

	addr = HNS3_TQP_INTR_CTRL_REG + hns3_get_tqp_intr_reg_offset(queue_id);
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
	uint16_t i;

	if (dev->data->dev_conf.intr_conf.rxq == 0)
		return;

	for (i = 0; i < nb_rx_q; i++)
		hns3_queue_intr_enable(hw, i, en);
}

int
hns3_dev_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
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
hns3_init_rxq(struct hns3_adapter *hns, uint16_t idx)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_rx_queue *rxq;
	int ret;

	PMD_INIT_FUNC_TRACE();

	rxq = (struct hns3_rx_queue *)hw->data->rx_queues[idx];
	ret = hns3_alloc_rx_queue_mbufs(hw, rxq);
	if (ret) {
		hns3_err(hw, "fail to alloc mbuf for Rx queue %u, ret = %d.",
			 idx, ret);
		return ret;
	}

	rxq->next_to_use = 0;
	rxq->rx_rearm_start = 0;
	rxq->rx_free_hold = 0;
	rxq->rx_rearm_nb = 0;
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;
	hns3_init_rx_queue_hw(rxq);
	hns3_rxq_vec_setup(rxq);

	return 0;
}

static void
hns3_init_fake_rxq(struct hns3_adapter *hns, uint16_t idx)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_rx_queue *rxq;

	rxq = (struct hns3_rx_queue *)hw->fkq_data.rx_queues[idx];
	rxq->next_to_use = 0;
	rxq->rx_free_hold = 0;
	rxq->rx_rearm_start = 0;
	rxq->rx_rearm_nb = 0;
	hns3_init_rx_queue_hw(rxq);
}

static void
hns3_init_txq(struct hns3_tx_queue *txq)
{
	struct hns3_desc *desc;
	uint16_t i;

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
hns3_init_tx_ring_tc(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_tx_queue *txq;
	int i, num;

	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		struct hns3_tc_queue_info *tc_queue = &hw->tc_queue[i];
		uint16_t j;

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
hns3_init_rx_queues(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_rx_queue *rxq;
	uint16_t i, j;
	int ret;

	/* Initialize RSS for queues */
	ret = hns3_config_rss(hns);
	if (ret) {
		hns3_err(hw, "failed to configure rss, ret = %d.", ret);
		return ret;
	}

	for (i = 0; i < hw->data->nb_rx_queues; i++) {
		rxq = (struct hns3_rx_queue *)hw->data->rx_queues[i];
		if (!rxq) {
			hns3_err(hw, "Rx queue %u not available or setup.", i);
			goto out;
		}

		if (rxq->rx_deferred_start)
			continue;

		ret = hns3_init_rxq(hns, i);
		if (ret) {
			hns3_err(hw, "failed to init Rx queue %u, ret = %d.", i,
				 ret);
			goto out;
		}
	}

	for (i = 0; i < hw->fkq_data.nb_fake_rx_queues; i++)
		hns3_init_fake_rxq(hns, i);

	return 0;

out:
	for (j = 0; j < i; j++) {
		rxq = (struct hns3_rx_queue *)hw->data->rx_queues[j];
		if (rxq->rx_deferred_start)
			continue;

		hns3_rx_queue_release_mbufs(rxq);
	}

	return ret;
}

static int
hns3_init_tx_queues(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_tx_queue *txq;
	uint16_t i;

	for (i = 0; i < hw->data->nb_tx_queues; i++) {
		txq = (struct hns3_tx_queue *)hw->data->tx_queues[i];
		if (!txq) {
			hns3_err(hw, "Tx queue %u not available or setup.", i);
			return -EINVAL;
		}

		if (txq->tx_deferred_start)
			continue;
		hns3_init_txq(txq);
	}

	for (i = 0; i < hw->fkq_data.nb_fake_tx_queues; i++) {
		txq = (struct hns3_tx_queue *)hw->fkq_data.tx_queues[i];
		hns3_init_txq(txq);
	}
	hns3_init_tx_ring_tc(hns);

	return 0;
}

/*
 * Init all queues.
 * Note: just init and setup queues, and don't enable tqps.
 */
int
hns3_init_queues(struct hns3_adapter *hns, bool reset_queue)
{
	struct hns3_hw *hw = &hns->hw;
	int ret;

	if (reset_queue) {
		ret = hns3_reset_all_tqps(hns);
		if (ret) {
			hns3_err(hw, "failed to reset all queues, ret = %d.",
				 ret);
			return ret;
		}
	}

	ret = hns3_init_rx_queues(hns);
	if (ret) {
		hns3_err(hw, "failed to init rx queues, ret = %d.", ret);
		return ret;
	}

	ret = hns3_init_tx_queues(hns);
	if (ret) {
		hns3_dev_release_mbufs(hns);
		hns3_err(hw, "failed to init tx queues, ret = %d.", ret);
	}

	return ret;
}

void
hns3_start_tqps(struct hns3_hw *hw)
{
	struct hns3_tx_queue *txq;
	struct hns3_rx_queue *rxq;
	uint16_t i;

	hns3_enable_all_queues(hw, true);

	for (i = 0; i < hw->data->nb_tx_queues; i++) {
		txq = hw->data->tx_queues[i];
		if (txq->enabled)
			hw->data->tx_queue_state[i] =
				RTE_ETH_QUEUE_STATE_STARTED;
	}

	for (i = 0; i < hw->data->nb_rx_queues; i++) {
		rxq = hw->data->rx_queues[i];
		if (rxq->enabled)
			hw->data->rx_queue_state[i] =
				RTE_ETH_QUEUE_STATE_STARTED;
	}
}

void
hns3_stop_tqps(struct hns3_hw *hw)
{
	uint16_t i;

	hns3_enable_all_queues(hw, false);

	for (i = 0; i < hw->data->nb_tx_queues; i++)
		hw->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;

	for (i = 0; i < hw->data->nb_rx_queues; i++)
		hw->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
}

/*
 * Iterate over all Rx Queue, and call the callback() function for each Rx
 * queue.
 *
 * @param[in] dev
 *   The target eth dev.
 * @param[in] callback
 *   The function to call for each queue.
 *   if callback function return nonzero will stop iterate and return it's value
 * @param[in] arg
 *   The arguments to provide the callback function with.
 *
 * @return
 *   0 on success, otherwise with errno set.
 */
int
hns3_rxq_iterate(struct rte_eth_dev *dev,
		 int (*callback)(struct hns3_rx_queue *, void *), void *arg)
{
	uint32_t i;
	int ret;

	if (dev->data->rx_queues == NULL)
		return -EINVAL;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		ret = callback(dev->data->rx_queues[i], arg);
		if (ret != 0)
			return ret;
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
		hns3_err(hw, "Failed to allocate memory for No.%u rx ring!",
			 q_info->idx);
		return NULL;
	}

	/* Allocate rx ring hardware descriptors. */
	rxq->queue_id = q_info->idx;
	rxq->nb_rx_desc = q_info->nb_desc;

	/*
	 * Allocate a litter more memory because rx vector functions
	 * don't check boundaries each time.
	 */
	rx_desc = (rxq->nb_rx_desc + HNS3_DEFAULT_RX_BURST) *
			sizeof(struct hns3_desc);
	rx_mz = rte_eth_dma_zone_reserve(dev, q_info->ring_name, q_info->idx,
					 rx_desc, HNS3_RING_BASE_ALIGN,
					 q_info->socket_id);
	if (rx_mz == NULL) {
		hns3_err(hw, "Failed to reserve DMA memory for No.%u rx ring!",
			 q_info->idx);
		hns3_rx_queue_release(rxq);
		return NULL;
	}
	rxq->mz = rx_mz;
	rxq->rx_ring = (struct hns3_desc *)rx_mz->addr;
	rxq->rx_ring_phys_addr = rx_mz->iova;

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
		hns3_err(hw, "Failed to setup No.%u fake rx ring.", idx);
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
	uint16_t i;

	txq = rte_zmalloc_socket(q_info->type, sizeof(struct hns3_tx_queue),
				 RTE_CACHE_LINE_SIZE, q_info->socket_id);
	if (txq == NULL) {
		hns3_err(hw, "Failed to allocate memory for No.%u tx ring!",
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
		hns3_err(hw, "Failed to reserve DMA memory for No.%u tx ring!",
			 q_info->idx);
		hns3_tx_queue_release(txq);
		return NULL;
	}
	txq->mz = tx_mz;
	txq->tx_ring = (struct hns3_desc *)tx_mz->addr;
	txq->tx_ring_phys_addr = tx_mz->iova;

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
		hns3_err(hw, "Failed to setup No.%u fake tx ring.", idx);
		return -ENOMEM;
	}

	/* Don't need alloc sw_ring, because upper applications don't use it */
	txq->sw_ring = NULL;
	txq->free = NULL;

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
	uint16_t i;

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
			hns3_rx_queue_release_lock(rxq[i]);

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
			hns3_rx_queue_release_lock(rxq[i]);

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
	uint16_t i;

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
			hns3_tx_queue_release_lock(txq[i]);
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
			hns3_tx_queue_release_lock(txq[i]);

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

	if (hns3_dev_get_support(hw, INDEP_TXRX))
		return 0;

	/* Setup new number of fake RX/TX queues and reconfigure device. */
	rx_need_add_nb_q = hw->cfg_max_queues - nb_rx_q;
	tx_need_add_nb_q = hw->cfg_max_queues - nb_tx_q;
	ret = hns3_fake_rx_queue_config(hw, rx_need_add_nb_q);
	if (ret) {
		hns3_err(hw, "Fail to configure fake rx queues: %d", ret);
		return ret;
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

	return ret;
}

void
hns3_dev_release_mbufs(struct hns3_adapter *hns)
{
	struct rte_eth_dev_data *dev_data = hns->hw.data;
	struct hns3_rx_queue *rxq;
	struct hns3_tx_queue *txq;
	uint16_t i;

	if (dev_data->rx_queues)
		for (i = 0; i < dev_data->nb_rx_queues; i++) {
			rxq = dev_data->rx_queues[i];
			if (rxq == NULL)
				continue;
			hns3_rx_queue_release_mbufs(rxq);
		}

	if (dev_data->tx_queues)
		for (i = 0; i < dev_data->nb_tx_queues; i++) {
			txq = dev_data->tx_queues[i];
			if (txq == NULL)
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

static int
hns3_rxq_conf_runtime_check(struct hns3_hw *hw, uint16_t buf_size,
				uint16_t nb_desc)
{
	struct rte_eth_dev *dev = &rte_eth_devices[hw->data->port_id];
	eth_rx_burst_t pkt_burst = dev->rx_pkt_burst;
	uint32_t frame_size = dev->data->mtu + HNS3_ETH_OVERHEAD;
	uint16_t min_vec_bds;

	/*
	 * HNS3 hardware network engine set scattered as default. If the driver
	 * is not work in scattered mode and the pkts greater than buf_size
	 * but smaller than frame size will be distributed to multiple BDs.
	 * Driver cannot handle this situation.
	 */
	if (!hw->data->scattered_rx && frame_size > buf_size) {
		hns3_err(hw, "frame size is not allowed to be set greater "
			     "than rx_buf_len if scattered is off.");
		return -EINVAL;
	}

	if (pkt_burst == hns3_recv_pkts_vec ||
	    pkt_burst == hns3_recv_pkts_vec_sve) {
		min_vec_bds = HNS3_DEFAULT_RXQ_REARM_THRESH +
			      HNS3_DEFAULT_RX_BURST;
		if (nb_desc < min_vec_bds ||
		    nb_desc % HNS3_DEFAULT_RXQ_REARM_THRESH) {
			hns3_err(hw, "if Rx burst mode is vector, "
				 "number of descriptor is required to be "
				 "bigger than min vector bds:%u, and could be "
				 "divided by rxq rearm thresh:%u.",
				 min_vec_bds, HNS3_DEFAULT_RXQ_REARM_THRESH);
			return -EINVAL;
		}
	}
	return 0;
}

static int
hns3_rx_queue_conf_check(struct hns3_hw *hw, const struct rte_eth_rxconf *conf,
			 struct rte_mempool *mp, uint16_t nb_desc,
			 uint16_t *buf_size)
{
	int ret;

	if (nb_desc > HNS3_MAX_RING_DESC || nb_desc < HNS3_MIN_RING_DESC ||
	    nb_desc % HNS3_ALIGN_RING_DESC) {
		hns3_err(hw, "Number (%u) of rx descriptors is invalid",
			 nb_desc);
		return -EINVAL;
	}

	if (conf->rx_drop_en == 0)
		hns3_warn(hw, "if no descriptors available, packets are always "
			  "dropped and rx_drop_en (1) is fixed on");

	if (hns3_rx_buf_len_calc(mp, buf_size)) {
		hns3_err(hw, "rxq mbufs' data room size (%u) is not enough! "
				"minimal data room size (%u).",
				rte_pktmbuf_data_room_size(mp),
				HNS3_MIN_BD_BUF_SIZE + RTE_PKTMBUF_HEADROOM);
		return -EINVAL;
	}

	if (hw->data->dev_started) {
		ret = hns3_rxq_conf_runtime_check(hw, *buf_size, nb_desc);
		if (ret) {
			hns3_err(hw, "Rx queue runtime setup fail.");
			return ret;
		}
	}

	return 0;
}

uint32_t
hns3_get_tqp_reg_offset(uint16_t queue_id)
{
	uint32_t reg_offset;

	/* Need an extend offset to config queue > 1024 */
	if (queue_id < HNS3_MIN_EXTEND_QUEUE_ID)
		reg_offset = HNS3_TQP_REG_OFFSET + queue_id * HNS3_TQP_REG_SIZE;
	else
		reg_offset = HNS3_TQP_REG_OFFSET + HNS3_TQP_EXT_REG_OFFSET +
			     (queue_id - HNS3_MIN_EXTEND_QUEUE_ID) *
			     HNS3_TQP_REG_SIZE;

	return reg_offset;
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
	int ret;

	ret = hns3_rx_queue_conf_check(hw, conf, mp, nb_desc, &rx_buf_size);
	if (ret)
		return ret;

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
	rxq->ptype_tbl = &hns->ptype_tbl;
	rxq->mb_pool = mp;
	rxq->rx_free_thresh = (conf->rx_free_thresh > 0) ?
		conf->rx_free_thresh : HNS3_DEFAULT_RX_FREE_THRESH;

	rxq->rx_deferred_start = conf->rx_deferred_start;
	if (rxq->rx_deferred_start && !hns3_dev_get_support(hw, INDEP_TXRX)) {
		hns3_warn(hw, "deferred start is not supported.");
		rxq->rx_deferred_start = false;
	}

	rx_entry_len = (rxq->nb_rx_desc + HNS3_DEFAULT_RX_BURST) *
			sizeof(struct hns3_entry);
	rxq->sw_ring = rte_zmalloc_socket("hns3 RX sw ring", rx_entry_len,
					  RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq->sw_ring == NULL) {
		hns3_err(hw, "Failed to allocate memory for rx sw ring!");
		hns3_rx_queue_release(rxq);
		return -ENOMEM;
	}

	rxq->next_to_use = 0;
	rxq->rx_free_hold = 0;
	rxq->rx_rearm_start = 0;
	rxq->rx_rearm_nb = 0;
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;
	rxq->port_id = dev->data->port_id;
	/*
	 * For hns3 PF device, if the VLAN mode is HW_SHIFT_AND_DISCARD_MODE,
	 * the pvid_sw_discard_en in the queue struct should not be changed,
	 * because PVID-related operations do not need to be processed by PMD.
	 * For hns3 VF device, whether it needs to process PVID depends
	 * on the configuration of PF kernel mode netdevice driver. And the
	 * related PF configuration is delivered through the mailbox and finally
	 * reflected in port_base_vlan_cfg.
	 */
	if (hns->is_vf || hw->vlan_mode == HNS3_SW_SHIFT_AND_DISCARD_MODE)
		rxq->pvid_sw_discard_en = hw->port_base_vlan_cfg.state ==
						HNS3_PORT_BASE_VLAN_ENABLE;
	else
		rxq->pvid_sw_discard_en = false;
	rxq->ptype_en = hns3_dev_get_support(hw, RXD_ADV_LAYOUT) ? true : false;
	rxq->configured = true;
	rxq->io_base = (void *)((char *)hw->io_base +
					hns3_get_tqp_reg_offset(idx));
	rxq->io_head_reg = (volatile void *)((char *)rxq->io_base +
			   HNS3_RING_RX_HEAD_REG);
	rxq->rx_buf_len = rx_buf_size;
	memset(&rxq->basic_stats, 0, sizeof(struct hns3_rx_basic_stats));
	memset(&rxq->err_stats, 0, sizeof(struct hns3_rx_bd_errors_stats));
	memset(&rxq->dfx_stats, 0, sizeof(struct hns3_rx_dfx_stats));

	/* CRC len set here is used for amending packet length */
	if (dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
		rxq->crc_len = RTE_ETHER_CRC_LEN;
	else
		rxq->crc_len = 0;

	rxq->bulk_mbuf_num = 0;

	rte_spinlock_lock(&hw->lock);
	dev->data->rx_queues[idx] = rxq;
	rte_spinlock_unlock(&hw->lock);

	return 0;
}

void
hns3_rx_scattered_reset(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	hw->rx_buf_len = 0;
	dev->data->scattered_rx = false;
}

void
hns3_rx_scattered_calc(struct rte_eth_dev *dev)
{
	struct rte_eth_conf *dev_conf = &dev->data->dev_conf;
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_rx_queue *rxq;
	uint32_t queue_id;

	if (dev->data->rx_queues == NULL)
		return;

	for (queue_id = 0; queue_id < dev->data->nb_rx_queues; queue_id++) {
		rxq = dev->data->rx_queues[queue_id];
		if (hw->rx_buf_len == 0)
			hw->rx_buf_len = rxq->rx_buf_len;
		else
			hw->rx_buf_len = RTE_MIN(hw->rx_buf_len,
						 rxq->rx_buf_len);
	}

	if (dev_conf->rxmode.offloads & RTE_ETH_RX_OFFLOAD_SCATTER ||
	    dev->data->mtu + HNS3_ETH_OVERHEAD > hw->rx_buf_len)
		dev->data->scattered_rx = true;
}

const uint32_t *
hns3_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER,
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
		RTE_PTYPE_INNER_L2_ETHER,
		RTE_PTYPE_INNER_L3_IPV4,
		RTE_PTYPE_INNER_L3_IPV6,
		RTE_PTYPE_INNER_L3_IPV4_EXT,
		RTE_PTYPE_INNER_L3_IPV6_EXT,
		RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_SCTP,
		RTE_PTYPE_INNER_L4_ICMP,
		RTE_PTYPE_TUNNEL_GRENAT,
		RTE_PTYPE_TUNNEL_NVGRE,
		RTE_PTYPE_UNKNOWN
	};
	static const uint32_t adv_layout_ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L2_ETHER_TIMESYNC,
		RTE_PTYPE_L2_ETHER_LLDP,
		RTE_PTYPE_L2_ETHER_ARP,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_NONFRAG,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_L4_IGMP,
		RTE_PTYPE_L4_ICMP,
		RTE_PTYPE_TUNNEL_GRE,
		RTE_PTYPE_TUNNEL_GRENAT,
		RTE_PTYPE_INNER_L2_ETHER,
		RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L4_FRAG,
		RTE_PTYPE_INNER_L4_ICMP,
		RTE_PTYPE_INNER_L4_NONFRAG,
		RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_SCTP,
		RTE_PTYPE_INNER_L4_ICMP,
		RTE_PTYPE_UNKNOWN
	};
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (dev->rx_pkt_burst == hns3_recv_pkts_simple ||
	    dev->rx_pkt_burst == hns3_recv_scattered_pkts ||
	    dev->rx_pkt_burst == hns3_recv_pkts_vec ||
	    dev->rx_pkt_burst == hns3_recv_pkts_vec_sve) {
		if (hns3_dev_get_support(hw, RXD_ADV_LAYOUT))
			return adv_layout_ptypes;
		else
			return ptypes;
	}

	return NULL;
}

static void
hns3_init_non_tunnel_ptype_tbl(struct hns3_ptype_table *tbl)
{
	tbl->l3table[0] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4;
	tbl->l3table[1] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6;
	tbl->l3table[2] = RTE_PTYPE_L2_ETHER_ARP;
	tbl->l3table[4] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT;
	tbl->l3table[5] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT;
	tbl->l3table[6] = RTE_PTYPE_L2_ETHER_LLDP;

	tbl->l4table[0] = RTE_PTYPE_L4_UDP;
	tbl->l4table[1] = RTE_PTYPE_L4_TCP;
	tbl->l4table[2] = RTE_PTYPE_TUNNEL_GRE;
	tbl->l4table[3] = RTE_PTYPE_L4_SCTP;
	tbl->l4table[4] = RTE_PTYPE_L4_IGMP;
	tbl->l4table[5] = RTE_PTYPE_L4_ICMP;
}

static void
hns3_init_tunnel_ptype_tbl(struct hns3_ptype_table *tbl)
{
	tbl->inner_l3table[0] = RTE_PTYPE_INNER_L2_ETHER |
				RTE_PTYPE_INNER_L3_IPV4;
	tbl->inner_l3table[1] = RTE_PTYPE_INNER_L2_ETHER |
				RTE_PTYPE_INNER_L3_IPV6;
	/* There is not a ptype for inner ARP/RARP */
	tbl->inner_l3table[2] = RTE_PTYPE_UNKNOWN;
	tbl->inner_l3table[3] = RTE_PTYPE_UNKNOWN;
	tbl->inner_l3table[4] = RTE_PTYPE_INNER_L2_ETHER |
				RTE_PTYPE_INNER_L3_IPV4_EXT;
	tbl->inner_l3table[5] = RTE_PTYPE_INNER_L2_ETHER |
				RTE_PTYPE_INNER_L3_IPV6_EXT;

	tbl->inner_l4table[0] = RTE_PTYPE_INNER_L4_UDP;
	tbl->inner_l4table[1] = RTE_PTYPE_INNER_L4_TCP;
	/* There is not a ptype for inner GRE */
	tbl->inner_l4table[2] = RTE_PTYPE_UNKNOWN;
	tbl->inner_l4table[3] = RTE_PTYPE_INNER_L4_SCTP;
	/* There is not a ptype for inner IGMP */
	tbl->inner_l4table[4] = RTE_PTYPE_UNKNOWN;
	tbl->inner_l4table[5] = RTE_PTYPE_INNER_L4_ICMP;

	tbl->ol3table[0] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4;
	tbl->ol3table[1] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6;
	tbl->ol3table[2] = RTE_PTYPE_UNKNOWN;
	tbl->ol3table[3] = RTE_PTYPE_UNKNOWN;
	tbl->ol3table[4] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT;
	tbl->ol3table[5] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT;

	tbl->ol4table[0] = RTE_PTYPE_UNKNOWN;
	tbl->ol4table[1] = RTE_PTYPE_L4_UDP | RTE_PTYPE_TUNNEL_GRENAT;
	tbl->ol4table[2] = RTE_PTYPE_TUNNEL_NVGRE;
}

static void
hns3_init_adv_layout_ptype(struct hns3_ptype_table *tbl)
{
	uint32_t *ptype = tbl->ptype;

	/* Non-tunnel L2 */
	ptype[1] = RTE_PTYPE_L2_ETHER_ARP;
	ptype[3] = RTE_PTYPE_L2_ETHER_LLDP;
	ptype[8] = RTE_PTYPE_L2_ETHER_TIMESYNC;

	/* Non-tunnel IPv4 */
	ptype[17] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_L4_FRAG;
	ptype[18] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_L4_NONFRAG;
	ptype[19] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_L4_UDP;
	ptype[20] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_L4_TCP;
	ptype[21] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_TUNNEL_GRE;
	ptype[22] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_L4_SCTP;
	ptype[23] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_L4_IGMP;
	ptype[24] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_L4_ICMP;
	/* The next ptype is PTP over IPv4 + UDP */
	ptype[25] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_L4_UDP;

	/* IPv4 --> GRE/Teredo/VXLAN */
	ptype[29] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_TUNNEL_GRENAT;
	/* IPv4 --> GRE/Teredo/VXLAN --> MAC */
	ptype[30] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER;

	/* IPv4 --> GRE/Teredo/VXLAN --> MAC --> IPv4 */
	ptype[31] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		    RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_INNER_L4_FRAG;
	ptype[32] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		    RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_INNER_L4_NONFRAG;
	ptype[33] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		    RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_INNER_L4_UDP;
	ptype[34] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		    RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_INNER_L4_TCP;
	ptype[35] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		    RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_INNER_L4_SCTP;
	/* The next ptype's inner L4 is IGMP */
	ptype[36] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		    RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN;
	ptype[37] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		    RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_INNER_L4_ICMP;

	/* IPv4 --> GRE/Teredo/VXLAN --> MAC --> IPv6 */
	ptype[39] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		    RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		    RTE_PTYPE_INNER_L4_FRAG;
	ptype[40] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		    RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		    RTE_PTYPE_INNER_L4_NONFRAG;
	ptype[41] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		    RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		    RTE_PTYPE_INNER_L4_UDP;
	ptype[42] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		    RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		    RTE_PTYPE_INNER_L4_TCP;
	ptype[43] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		    RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		    RTE_PTYPE_INNER_L4_SCTP;
	/* The next ptype's inner L4 is IGMP */
	ptype[44] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		    RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN;
	ptype[45] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		    RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		    RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		    RTE_PTYPE_INNER_L4_ICMP;

	/* Non-tunnel IPv6 */
	ptype[111] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_FRAG;
	ptype[112] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_NONFRAG;
	ptype[113] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_UDP;
	ptype[114] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_TCP;
	ptype[115] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_TUNNEL_GRE;
	ptype[116] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_SCTP;
	ptype[117] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_IGMP;
	ptype[118] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_ICMP;
	/* Special for PTP over IPv6 + UDP */
	ptype[119] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_UDP;

	/* IPv6 --> GRE/Teredo/VXLAN */
	ptype[123] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_TUNNEL_GRENAT;
	/* IPv6 --> GRE/Teredo/VXLAN --> MAC */
	ptype[124] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER;

	/* IPv6 --> GRE/Teredo/VXLAN --> MAC --> IPv4 */
	ptype[125] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_FRAG;
	ptype[126] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_NONFRAG;
	ptype[127] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_UDP;
	ptype[128] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP;
	ptype[129] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_SCTP;
	/* The next ptype's inner L4 is IGMP */
	ptype[130] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN;
	ptype[131] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_ICMP;

	/* IPv6 --> GRE/Teredo/VXLAN --> MAC --> IPv6 */
	ptype[133] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_FRAG;
	ptype[134] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_NONFRAG;
	ptype[135] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_UDP;
	ptype[136] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP;
	ptype[137] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_SCTP;
	/* The next ptype's inner L4 is IGMP */
	ptype[138] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN;
	ptype[139] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_ICMP;
}

void
hns3_init_rx_ptype_tble(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_ptype_table *tbl = &hns->ptype_tbl;

	memset(tbl, 0, sizeof(*tbl));

	hns3_init_non_tunnel_ptype_tbl(tbl);
	hns3_init_tunnel_ptype_tbl(tbl);
	hns3_init_adv_layout_ptype(tbl);
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
	report_mode = report_type[rxq->pvid_sw_discard_en][strip_status];
	switch (report_mode) {
	case HNS3_NO_STRP_VLAN_VLD:
		mb->vlan_tci = 0;
		return;
	case HNS3_INNER_STRP_VLAN_VLD:
		mb->ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
		mb->vlan_tci = rte_le_to_cpu_16(rxd->rx.vlan_tag);
		return;
	case HNS3_OUTER_STRP_VLAN_VLD:
		mb->ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
		mb->vlan_tci = rte_le_to_cpu_16(rxd->rx.ot_vlan_tag);
		return;
	default:
		mb->vlan_tci = 0;
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

static inline struct rte_mbuf *
hns3_rx_alloc_buffer(struct hns3_rx_queue *rxq)
{
	int ret;

	if (likely(rxq->bulk_mbuf_num > 0))
		return rxq->bulk_mbuf[--rxq->bulk_mbuf_num];

	ret = rte_mempool_get_bulk(rxq->mb_pool, (void **)rxq->bulk_mbuf,
				   HNS3_BULK_ALLOC_MBUF_NUM);
	if (likely(ret == 0)) {
		rxq->bulk_mbuf_num = HNS3_BULK_ALLOC_MBUF_NUM;
		return rxq->bulk_mbuf[--rxq->bulk_mbuf_num];
	} else
		return rte_mbuf_raw_alloc(rxq->mb_pool);
}

static void
hns3_rx_ptp_timestamp_handle(struct hns3_rx_queue *rxq, struct rte_mbuf *mbuf,
			     uint64_t timestamp)
{
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(rxq->hns);

	mbuf->ol_flags |= RTE_MBUF_F_RX_IEEE1588_PTP |
			  RTE_MBUF_F_RX_IEEE1588_TMST;
	if (hns3_timestamp_rx_dynflag > 0) {
		*RTE_MBUF_DYNFIELD(mbuf, hns3_timestamp_dynfield_offset,
			rte_mbuf_timestamp_t *) = timestamp;
		mbuf->ol_flags |= hns3_timestamp_rx_dynflag;
	}

	pf->rx_timestamp = timestamp;
}

uint16_t
hns3_recv_pkts_simple(void *rx_queue,
		      struct rte_mbuf **rx_pkts,
		      uint16_t nb_pkts)
{
	volatile struct hns3_desc *rx_ring;  /* RX ring (desc) */
	volatile struct hns3_desc *rxdp;     /* pointer of the current desc */
	struct hns3_rx_queue *rxq;      /* RX queue */
	struct hns3_entry *sw_ring;
	struct hns3_entry *rxe;
	struct hns3_desc rxd;
	struct rte_mbuf *nmb;           /* pointer of the new mbuf */
	struct rte_mbuf *rxm;
	uint32_t bd_base_info;
	uint32_t l234_info;
	uint32_t ol_info;
	uint64_t dma_addr;
	uint16_t nb_rx_bd;
	uint16_t nb_rx;
	uint16_t rx_id;
	int ret;

	nb_rx = 0;
	nb_rx_bd = 0;
	rxq = rx_queue;
	rx_ring = rxq->rx_ring;
	sw_ring = rxq->sw_ring;
	rx_id = rxq->next_to_use;

	while (nb_rx < nb_pkts) {
		rxdp = &rx_ring[rx_id];
		bd_base_info = rte_le_to_cpu_32(rxdp->rx.bd_base_info);
		if (unlikely(!(bd_base_info & BIT(HNS3_RXD_VLD_B))))
			break;

		rxd = rxdp[(bd_base_info & (1u << HNS3_RXD_VLD_B)) -
			   (1u << HNS3_RXD_VLD_B)];

		nmb = hns3_rx_alloc_buffer(rxq);
		if (unlikely(nmb == NULL)) {
			rte_eth_devices[rxq->port_id].data->
				rx_mbuf_alloc_failed++;
			break;
		}

		nb_rx_bd++;
		rxe = &sw_ring[rx_id];
		rx_id++;
		if (unlikely(rx_id == rxq->nb_rx_desc))
			rx_id = 0;

		rte_prefetch0(sw_ring[rx_id].mbuf);
		if ((rx_id & HNS3_RX_RING_PREFETCTH_MASK) == 0) {
			rte_prefetch0(&rx_ring[rx_id]);
			rte_prefetch0(&sw_ring[rx_id]);
		}

		rxm = rxe->mbuf;
		rxm->ol_flags = 0;
		rxe->mbuf = nmb;

		if (unlikely(bd_base_info & BIT(HNS3_RXD_TS_VLD_B)))
			hns3_rx_ptp_timestamp_handle(rxq, rxm,
				rte_le_to_cpu_64(rxdp->timestamp));

		dma_addr = rte_mbuf_data_iova_default(nmb);
		rxdp->addr = rte_cpu_to_le_64(dma_addr);
		rxdp->rx.bd_base_info = 0;

		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rxm->pkt_len = (uint16_t)(rte_le_to_cpu_16(rxd.rx.pkt_len)) -
				rxq->crc_len;
		rxm->data_len = rxm->pkt_len;
		rxm->port = rxq->port_id;
		rxm->hash.rss = rte_le_to_cpu_32(rxd.rx.rss_hash);
		rxm->ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
		if (unlikely(bd_base_info & BIT(HNS3_RXD_LUM_B))) {
			rxm->hash.fdir.hi =
				rte_le_to_cpu_16(rxd.rx.fd_id);
			rxm->ol_flags |= RTE_MBUF_F_RX_FDIR | RTE_MBUF_F_RX_FDIR_ID;
		}
		rxm->nb_segs = 1;
		rxm->next = NULL;

		/* Load remained descriptor data and extract necessary fields */
		l234_info = rte_le_to_cpu_32(rxd.rx.l234_info);
		ol_info = rte_le_to_cpu_32(rxd.rx.ol_info);
		ret = hns3_handle_bdinfo(rxq, rxm, bd_base_info, l234_info);
		if (unlikely(ret))
			goto pkt_err;

		rxm->packet_type = hns3_rx_calc_ptype(rxq, l234_info, ol_info);

		if (rxm->packet_type == RTE_PTYPE_L2_ETHER_TIMESYNC)
			rxm->ol_flags |= RTE_MBUF_F_RX_IEEE1588_PTP;

		hns3_rxd_to_vlan_tci(rxq, rxm, l234_info, &rxd);

		/* Increment bytes counter  */
		rxq->basic_stats.bytes += rxm->pkt_len;

		rx_pkts[nb_rx++] = rxm;
		continue;
pkt_err:
		rte_pktmbuf_free(rxm);
	}

	rxq->next_to_use = rx_id;
	rxq->rx_free_hold += nb_rx_bd;
	if (rxq->rx_free_hold > rxq->rx_free_thresh) {
		hns3_write_reg_opt(rxq->io_head_reg, rxq->rx_free_hold);
		rxq->rx_free_hold = 0;
	}

	return nb_rx;
}

uint16_t
hns3_recv_scattered_pkts(void *rx_queue,
			 struct rte_mbuf **rx_pkts,
			 uint16_t nb_pkts)
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
	uint64_t timestamp;
	uint32_t l234_info;
	uint32_t gro_size;
	uint32_t ol_info;
	uint64_t dma_addr;
	uint16_t nb_rx_bd;
	uint16_t nb_rx;
	uint16_t rx_id;
	int ret;

	nb_rx = 0;
	nb_rx_bd = 0;
	rxq = rx_queue;

	rx_id = rxq->next_to_use;
	rx_ring = rxq->rx_ring;
	sw_ring = rxq->sw_ring;
	first_seg = rxq->pkt_first_seg;
	last_seg = rxq->pkt_last_seg;

	while (nb_rx < nb_pkts) {
		rxdp = &rx_ring[rx_id];
		bd_base_info = rte_le_to_cpu_32(rxdp->rx.bd_base_info);
		if (unlikely(!(bd_base_info & BIT(HNS3_RXD_VLD_B))))
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

		nmb = hns3_rx_alloc_buffer(rxq);
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
		if ((rx_id & HNS3_RX_RING_PREFETCTH_MASK) == 0) {
			rte_prefetch0(&rx_ring[rx_id]);
			rte_prefetch0(&sw_ring[rx_id]);
		}

		rxm = rxe->mbuf;
		rxe->mbuf = nmb;

		if (unlikely(bd_base_info & BIT(HNS3_RXD_TS_VLD_B)))
			timestamp = rte_le_to_cpu_64(rxdp->timestamp);

		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));
		rxdp->rx.bd_base_info = 0;
		rxdp->addr = dma_addr;

		if (first_seg == NULL) {
			first_seg = rxm;
			first_seg->nb_segs = 1;
		} else {
			first_seg->nb_segs++;
			last_seg->next = rxm;
		}

		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rxm->data_len = rte_le_to_cpu_16(rxd.rx.size);

		if (!(bd_base_info & BIT(HNS3_RXD_FE_B))) {
			last_seg = rxm;
			rxm->next = NULL;
			continue;
		}

		if (unlikely(bd_base_info & BIT(HNS3_RXD_TS_VLD_B)))
			hns3_rx_ptp_timestamp_handle(rxq, first_seg, timestamp);

		/*
		 * The last buffer of the received packet. packet len from
		 * buffer description may contains CRC len, packet len should
		 * subtract it, same as data len.
		 */
		first_seg->pkt_len = rte_le_to_cpu_16(rxd.rx.pkt_len);

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
				rxm->data_len);
		}

		first_seg->port = rxq->port_id;
		first_seg->hash.rss = rte_le_to_cpu_32(rxd.rx.rss_hash);
		first_seg->ol_flags = RTE_MBUF_F_RX_RSS_HASH;
		if (unlikely(bd_base_info & BIT(HNS3_RXD_LUM_B))) {
			first_seg->hash.fdir.hi =
				rte_le_to_cpu_16(rxd.rx.fd_id);
			first_seg->ol_flags |= RTE_MBUF_F_RX_FDIR | RTE_MBUF_F_RX_FDIR_ID;
		}

		gro_size = hns3_get_field(bd_base_info, HNS3_RXD_GRO_SIZE_M,
					  HNS3_RXD_GRO_SIZE_S);
		if (gro_size != 0) {
			first_seg->ol_flags |= RTE_MBUF_F_RX_LRO;
			first_seg->tso_segsz = gro_size;
		}

		l234_info = rte_le_to_cpu_32(rxd.rx.l234_info);
		ol_info = rte_le_to_cpu_32(rxd.rx.ol_info);
		ret = hns3_handle_bdinfo(rxq, first_seg, bd_base_info,
					 l234_info);
		if (unlikely(ret))
			goto pkt_err;

		first_seg->packet_type = hns3_rx_calc_ptype(rxq,
						l234_info, ol_info);

		if (first_seg->packet_type == RTE_PTYPE_L2_ETHER_TIMESYNC)
			rxm->ol_flags |= RTE_MBUF_F_RX_IEEE1588_PTP;

		hns3_rxd_to_vlan_tci(rxq, first_seg, l234_info, &rxd);

		/* Increment bytes counter */
		rxq->basic_stats.bytes += first_seg->pkt_len;

		rx_pkts[nb_rx++] = first_seg;
		first_seg = NULL;
		continue;
pkt_err:
		rte_pktmbuf_free(first_seg);
		first_seg = NULL;
	}

	rxq->next_to_use = rx_id;
	rxq->pkt_first_seg = first_seg;
	rxq->pkt_last_seg = last_seg;

	rxq->rx_free_hold += nb_rx_bd;
	if (rxq->rx_free_hold > rxq->rx_free_thresh) {
		hns3_write_reg_opt(rxq->io_head_reg, rxq->rx_free_hold);
		rxq->rx_free_hold = 0;
	}

	return nb_rx;
}

void __rte_weak
hns3_rxq_vec_setup(__rte_unused struct hns3_rx_queue *rxq)
{
}

int __rte_weak
hns3_rx_check_vec_support(__rte_unused struct rte_eth_dev *dev)
{
	return -ENOTSUP;
}

uint16_t __rte_weak
hns3_recv_pkts_vec(__rte_unused void *rx_queue,
		   __rte_unused struct rte_mbuf **rx_pkts,
		   __rte_unused uint16_t nb_pkts)
{
	return 0;
}

uint16_t __rte_weak
hns3_recv_pkts_vec_sve(__rte_unused void *rx_queue,
		       __rte_unused struct rte_mbuf **rx_pkts,
		       __rte_unused uint16_t nb_pkts)
{
	return 0;
}

int
hns3_rx_burst_mode_get(struct rte_eth_dev *dev, __rte_unused uint16_t queue_id,
		       struct rte_eth_burst_mode *mode)
{
	static const struct {
		eth_rx_burst_t pkt_burst;
		const char *info;
	} burst_infos[] = {
		{ hns3_recv_pkts_simple,	"Scalar Simple" },
		{ hns3_recv_scattered_pkts,	"Scalar Scattered" },
		{ hns3_recv_pkts_vec,		"Vector Neon"   },
		{ hns3_recv_pkts_vec_sve,	"Vector Sve"    },
		{ rte_eth_pkt_burst_dummy,	"Dummy"         },
	};

	eth_rx_burst_t pkt_burst = dev->rx_pkt_burst;
	int ret = -EINVAL;
	unsigned int i;

	for (i = 0; i < RTE_DIM(burst_infos); i++) {
		if (pkt_burst == burst_infos[i].pkt_burst) {
			snprintf(mode->info, sizeof(mode->info), "%s",
				 burst_infos[i].info);
			ret = 0;
			break;
		}
	}

	return ret;
}

static bool
hns3_get_default_vec_support(void)
{
#if defined(RTE_ARCH_ARM64)
	if (rte_vect_get_max_simd_bitwidth() < RTE_VECT_SIMD_128)
		return false;
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_NEON))
		return true;
#endif
	return false;
}

static bool
hns3_get_sve_support(void)
{
#if defined(RTE_HAS_SVE_ACLE)
	if (rte_vect_get_max_simd_bitwidth() < RTE_VECT_SIMD_256)
		return false;
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_SVE))
		return true;
#endif
	return false;
}

static eth_rx_burst_t
hns3_get_rx_function(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	uint64_t offloads = dev->data->dev_conf.rxmode.offloads;
	bool vec_allowed, sve_allowed, simple_allowed;
	bool vec_support;

	vec_support = hns3_rx_check_vec_support(dev) == 0;
	vec_allowed = vec_support && hns3_get_default_vec_support();
	sve_allowed = vec_support && hns3_get_sve_support();
	simple_allowed = !dev->data->scattered_rx &&
			 (offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO) == 0;

	if (hns->rx_func_hint == HNS3_IO_FUNC_HINT_VEC && vec_allowed)
		return hns3_recv_pkts_vec;
	if (hns->rx_func_hint == HNS3_IO_FUNC_HINT_SVE && sve_allowed)
		return hns3_recv_pkts_vec_sve;
	if (hns->rx_func_hint == HNS3_IO_FUNC_HINT_SIMPLE && simple_allowed)
		return hns3_recv_pkts_simple;
	if (hns->rx_func_hint == HNS3_IO_FUNC_HINT_COMMON)
		return hns3_recv_scattered_pkts;

	if (vec_allowed)
		return hns3_recv_pkts_vec;
	if (simple_allowed)
		return hns3_recv_pkts_simple;

	return hns3_recv_scattered_pkts;
}

static int
hns3_tx_queue_conf_check(struct hns3_hw *hw, const struct rte_eth_txconf *conf,
			 uint16_t nb_desc, uint16_t *tx_rs_thresh,
			 uint16_t *tx_free_thresh, uint16_t idx)
{
#define HNS3_TX_RS_FREE_THRESH_GAP	8
	uint16_t rs_thresh, free_thresh, fast_free_thresh;

	if (nb_desc > HNS3_MAX_RING_DESC || nb_desc < HNS3_MIN_RING_DESC ||
	    nb_desc % HNS3_ALIGN_RING_DESC) {
		hns3_err(hw, "number (%u) of tx descriptors is invalid",
			 nb_desc);
		return -EINVAL;
	}

	rs_thresh = (conf->tx_rs_thresh > 0) ?
			conf->tx_rs_thresh : HNS3_DEFAULT_TX_RS_THRESH;
	free_thresh = (conf->tx_free_thresh > 0) ?
			conf->tx_free_thresh : HNS3_DEFAULT_TX_FREE_THRESH;
	if (rs_thresh + free_thresh > nb_desc || nb_desc % rs_thresh ||
	    rs_thresh >= nb_desc - HNS3_TX_RS_FREE_THRESH_GAP ||
	    free_thresh >= nb_desc - HNS3_TX_RS_FREE_THRESH_GAP) {
		hns3_err(hw, "tx_rs_thresh (%u) tx_free_thresh (%u) nb_desc "
			 "(%u) of tx descriptors for port=%u queue=%u check "
			 "fail!",
			 rs_thresh, free_thresh, nb_desc, hw->data->port_id,
			 idx);
		return -EINVAL;
	}

	if (conf->tx_free_thresh == 0) {
		/* Fast free Tx memory buffer to improve cache hit rate */
		fast_free_thresh = nb_desc - rs_thresh;
		if (fast_free_thresh >=
		    HNS3_TX_FAST_FREE_AHEAD + HNS3_DEFAULT_TX_FREE_THRESH)
			free_thresh = fast_free_thresh -
					HNS3_TX_FAST_FREE_AHEAD;
	}

	*tx_rs_thresh = rs_thresh;
	*tx_free_thresh = free_thresh;
	return 0;
}

static void *
hns3_tx_push_get_queue_tail_reg(struct rte_eth_dev *dev, uint16_t queue_id)
{
#define HNS3_TX_PUSH_TQP_REGION_SIZE		0x10000
#define HNS3_TX_PUSH_QUICK_DOORBELL_OFFSET	64
#define HNS3_TX_PUSH_PCI_BAR_INDEX		4

	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);
	uint8_t bar_id = HNS3_TX_PUSH_PCI_BAR_INDEX;

	/*
	 * If device support Tx push then its PCIe bar45 must exist, and DPDK
	 * framework will mmap the bar45 default in PCI probe stage.
	 *
	 * In the bar45, the first half is for RoCE (RDMA over Converged
	 * Ethernet), and the second half is for NIC, every TQP occupy 64KB.
	 *
	 * The quick doorbell located at 64B offset in the TQP region.
	 */
	return (char *)pci_dev->mem_resource[bar_id].addr +
			(pci_dev->mem_resource[bar_id].len >> 1) +
			HNS3_TX_PUSH_TQP_REGION_SIZE * queue_id +
			HNS3_TX_PUSH_QUICK_DOORBELL_OFFSET;
}

void
hns3_tx_push_init(struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	volatile uint32_t *reg;
	uint32_t val;

	if (!hns3_dev_get_support(hw, TX_PUSH))
		return;

	reg = (volatile uint32_t *)hns3_tx_push_get_queue_tail_reg(dev, 0);
	/*
	 * Because the size of bar45 is about 8GB size, it may take a long time
	 * to do the page fault in Tx process when work with vfio-pci, so use
	 * one read operation to make kernel setup page table mapping for bar45
	 * in the init stage.
	 * Note: the bar45 is readable but the result is all 1.
	 */
	val = *reg;
	RTE_SET_USED(val);
}

static void
hns3_tx_push_queue_init(struct rte_eth_dev *dev,
			uint16_t queue_id,
			struct hns3_tx_queue *txq)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	if (!hns3_dev_get_support(hw, TX_PUSH)) {
		txq->tx_push_enable = false;
		return;
	}

	txq->io_tail_reg = (volatile void *)hns3_tx_push_get_queue_tail_reg(dev,
						queue_id);
	txq->tx_push_enable = true;
}

int
hns3_tx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t nb_desc,
		    unsigned int socket_id, const struct rte_eth_txconf *conf)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	uint16_t tx_rs_thresh, tx_free_thresh;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_queue_info q_info;
	struct hns3_tx_queue *txq;
	int tx_entry_len;
	int ret;

	ret = hns3_tx_queue_conf_check(hw, conf, nb_desc,
				       &tx_rs_thresh, &tx_free_thresh, idx);
	if (ret)
		return ret;

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
	if (txq->tx_deferred_start && !hns3_dev_get_support(hw, INDEP_TXRX)) {
		hns3_warn(hw, "deferred start is not supported.");
		txq->tx_deferred_start = false;
	}

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
	txq->tx_free_thresh = tx_free_thresh;
	txq->tx_rs_thresh = tx_rs_thresh;
	txq->free = rte_zmalloc_socket("hns3 TX mbuf free array",
				sizeof(struct rte_mbuf *) * txq->tx_rs_thresh,
				RTE_CACHE_LINE_SIZE, socket_id);
	if (!txq->free) {
		hns3_err(hw, "failed to allocate tx mbuf free array!");
		hns3_tx_queue_release(txq);
		return -ENOMEM;
	}

	txq->port_id = dev->data->port_id;
	/*
	 * For hns3 PF device, if the VLAN mode is HW_SHIFT_AND_DISCARD_MODE,
	 * the pvid_sw_shift_en in the queue struct should not be changed,
	 * because PVID-related operations do not need to be processed by PMD.
	 * For hns3 VF device, whether it needs to process PVID depends
	 * on the configuration of PF kernel mode netdev driver. And the
	 * related PF configuration is delivered through the mailbox and finally
	 * reflected in port_base_vlan_cfg.
	 */
	if (hns->is_vf || hw->vlan_mode == HNS3_SW_SHIFT_AND_DISCARD_MODE)
		txq->pvid_sw_shift_en = hw->port_base_vlan_cfg.state ==
					HNS3_PORT_BASE_VLAN_ENABLE;
	else
		txq->pvid_sw_shift_en = false;
	txq->max_non_tso_bd_num = hw->max_non_tso_bd_num;
	txq->configured = true;
	txq->io_base = (void *)((char *)hw->io_base +
						hns3_get_tqp_reg_offset(idx));
	txq->io_tail_reg = (volatile void *)((char *)txq->io_base +
					     HNS3_RING_TX_TAIL_REG);
	txq->min_tx_pkt_len = hw->min_tx_pkt_len;
	txq->tso_mode = hw->tso_mode;
	txq->udp_cksum_mode = hw->udp_cksum_mode;
	txq->mbuf_fast_free_en = !!(dev->data->dev_conf.txmode.offloads &
				    RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE);
	memset(&txq->basic_stats, 0, sizeof(struct hns3_tx_basic_stats));
	memset(&txq->dfx_stats, 0, sizeof(struct hns3_tx_dfx_stats));

	/*
	 * Call hns3_tx_push_queue_init after assigned io_tail_reg field because
	 * it may overwrite the io_tail_reg field.
	 */
	hns3_tx_push_queue_init(dev, idx, txq);

	rte_spinlock_lock(&hw->lock);
	dev->data->tx_queues[idx] = txq;
	rte_spinlock_unlock(&hw->lock);

	return 0;
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

	while ((!(desc->tx.tp_fe_sc_vld_ra_ri &
		rte_cpu_to_le_16(BIT(HNS3_TXD_VLD_B)))) &&
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
	gro_en = offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO ? true : false;
	ret = hns3_config_gro(hw, gro_en);
	if (ret)
		hns3_err(hw, "restore hardware GRO to %s failed, ret = %d",
			 gro_en ? "enabled" : "disabled", ret);

	return ret;
}

static inline bool
hns3_pkt_is_tso(struct rte_mbuf *m)
{
	return (m->tso_segsz != 0 && m->ol_flags & RTE_MBUF_F_TX_TCP_SEG);
}

static void
hns3_set_tso(struct hns3_desc *desc, uint32_t paylen, struct rte_mbuf *rxm)
{
	if (!hns3_pkt_is_tso(rxm))
		return;

	if (paylen <= rxm->tso_segsz)
		return;

	desc->tx.type_cs_vlan_tso_len |= rte_cpu_to_le_32(BIT(HNS3_TXD_TSO_B));
	desc->tx.mss = rte_cpu_to_le_16(rxm->tso_segsz);
}

static inline void
hns3_fill_per_desc(struct hns3_desc *desc, struct rte_mbuf *rxm)
{
	desc->addr = rte_mbuf_data_iova(rxm);
	desc->tx.send_size = rte_cpu_to_le_16(rte_pktmbuf_data_len(rxm));
	desc->tx.tp_fe_sc_vld_ra_ri |= rte_cpu_to_le_16(BIT(HNS3_TXD_VLD_B));
}

static void
hns3_fill_first_desc(struct hns3_tx_queue *txq, struct hns3_desc *desc,
		     struct rte_mbuf *rxm)
{
	uint64_t ol_flags = rxm->ol_flags;
	uint32_t hdr_len;
	uint32_t paylen;

	hdr_len = rxm->l2_len + rxm->l3_len + rxm->l4_len;
	hdr_len += (ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) ?
			   rxm->outer_l2_len + rxm->outer_l3_len : 0;
	paylen = rxm->pkt_len - hdr_len;
	desc->tx.paylen_fd_dop_ol4cs |= rte_cpu_to_le_32(paylen);
	hns3_set_tso(desc, paylen, rxm);

	/*
	 * Currently, hardware doesn't support more than two layers VLAN offload
	 * in Tx direction based on hns3 network engine. So when the number of
	 * VLANs in the packets represented by rxm plus the number of VLAN
	 * offload by hardware such as PVID etc, exceeds two, the packets will
	 * be discarded or the original VLAN of the packets will be overwritten
	 * by hardware. When the PF PVID is enabled by calling the API function
	 * named rte_eth_dev_set_vlan_pvid or the VF PVID is enabled by the hns3
	 * PF kernel ether driver, the outer VLAN tag will always be the PVID.
	 * To avoid the VLAN of Tx descriptor is overwritten by PVID, it should
	 * be added to the position close to the IP header when PVID is enabled.
	 */
	if (!txq->pvid_sw_shift_en &&
	    ol_flags & (RTE_MBUF_F_TX_VLAN | RTE_MBUF_F_TX_QINQ)) {
		desc->tx.ol_type_vlan_len_msec |=
				rte_cpu_to_le_32(BIT(HNS3_TXD_OVLAN_B));
		if (ol_flags & RTE_MBUF_F_TX_QINQ)
			desc->tx.outer_vlan_tag =
					rte_cpu_to_le_16(rxm->vlan_tci_outer);
		else
			desc->tx.outer_vlan_tag =
					rte_cpu_to_le_16(rxm->vlan_tci);
	}

	if (ol_flags & RTE_MBUF_F_TX_QINQ ||
	    ((ol_flags & RTE_MBUF_F_TX_VLAN) && txq->pvid_sw_shift_en)) {
		desc->tx.type_cs_vlan_tso_len |=
					rte_cpu_to_le_32(BIT(HNS3_TXD_VLAN_B));
		desc->tx.vlan_tag = rte_cpu_to_le_16(rxm->vlan_tci);
	}

	if (ol_flags & RTE_MBUF_F_TX_IEEE1588_TMST)
		desc->tx.tp_fe_sc_vld_ra_ri |=
				rte_cpu_to_le_16(BIT(HNS3_TXD_TSYN_B));
}

static inline int
hns3_tx_alloc_mbufs(struct rte_mempool *mb_pool, uint16_t nb_new_buf,
			struct rte_mbuf **alloc_mbuf)
{
#define MAX_NON_TSO_BD_PER_PKT 18
	struct rte_mbuf *pkt_segs[MAX_NON_TSO_BD_PER_PKT];
	uint16_t i;

	/* Allocate enough mbufs */
	if (rte_mempool_get_bulk(mb_pool, (void **)pkt_segs, nb_new_buf))
		return -ENOMEM;

	for (i = 0; i < nb_new_buf - 1; i++)
		pkt_segs[i]->next = pkt_segs[i + 1];

	pkt_segs[nb_new_buf - 1]->next = NULL;
	pkt_segs[0]->nb_segs = nb_new_buf;
	*alloc_mbuf = pkt_segs[0];

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
hns3_reassemble_tx_pkts(struct rte_mbuf *tx_pkt, struct rte_mbuf **new_pkt,
				  uint8_t max_non_tso_bd_num)
{
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
	int ret;
	char *s;
	char *d;

	mb_pool = tx_pkt->pool;
	buf_size = tx_pkt->buf_len - RTE_PKTMBUF_HEADROOM;
	nb_new_buf = (rte_pktmbuf_pkt_len(tx_pkt) - 1) / buf_size + 1;
	if (nb_new_buf > max_non_tso_bd_num)
		return -EINVAL;

	last_buf_len = rte_pktmbuf_pkt_len(tx_pkt) % buf_size;
	if (last_buf_len == 0)
		last_buf_len = buf_size;

	/* Allocate enough mbufs */
	ret = hns3_tx_alloc_mbufs(mb_pool, nb_new_buf, &new_mbuf);
	if (ret)
		return ret;

	/* Copy the original packet content to the new mbufs */
	temp = tx_pkt;
	s = rte_pktmbuf_mtod(temp, char *);
	len_s = rte_pktmbuf_data_len(temp);
	temp_new = new_mbuf;
	while (temp != NULL && temp_new != NULL) {
		d = rte_pktmbuf_mtod(temp_new, char *);
		buf_len = temp_new->next == NULL ? last_buf_len : buf_size;
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
hns3_parse_outer_params(struct rte_mbuf *m, uint32_t *ol_type_vlan_len_msec)
{
	uint32_t tmp = *ol_type_vlan_len_msec;
	uint64_t ol_flags = m->ol_flags;

	/* (outer) IP header type */
	if (ol_flags & RTE_MBUF_F_TX_OUTER_IPV4) {
		if (ol_flags & RTE_MBUF_F_TX_OUTER_IP_CKSUM)
			tmp |= hns3_gen_field_val(HNS3_TXD_OL3T_M,
					HNS3_TXD_OL3T_S, HNS3_OL3T_IPV4_CSUM);
		else
			tmp |= hns3_gen_field_val(HNS3_TXD_OL3T_M,
				HNS3_TXD_OL3T_S, HNS3_OL3T_IPV4_NO_CSUM);
	} else if (ol_flags & RTE_MBUF_F_TX_OUTER_IPV6) {
		tmp |= hns3_gen_field_val(HNS3_TXD_OL3T_M, HNS3_TXD_OL3T_S,
					HNS3_OL3T_IPV6);
	}
	/* OL3 header size, defined in 4 bytes */
	tmp |= hns3_gen_field_val(HNS3_TXD_L3LEN_M, HNS3_TXD_L3LEN_S,
				m->outer_l3_len >> HNS3_L3_LEN_UNIT);
	*ol_type_vlan_len_msec = tmp;
}

static int
hns3_parse_inner_params(struct rte_mbuf *m, uint32_t *ol_type_vlan_len_msec,
			uint32_t *type_cs_vlan_tso_len)
{
#define HNS3_NVGRE_HLEN 8
	uint32_t tmp_outer = *ol_type_vlan_len_msec;
	uint32_t tmp_inner = *type_cs_vlan_tso_len;
	uint64_t ol_flags = m->ol_flags;
	uint16_t inner_l2_len;

	switch (ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) {
	case RTE_MBUF_F_TX_TUNNEL_VXLAN_GPE:
	case RTE_MBUF_F_TX_TUNNEL_GENEVE:
	case RTE_MBUF_F_TX_TUNNEL_VXLAN:
		/* MAC in UDP tunnelling packet, include VxLAN and GENEVE */
		tmp_outer |= hns3_gen_field_val(HNS3_TXD_TUNTYPE_M,
				HNS3_TXD_TUNTYPE_S, HNS3_TUN_MAC_IN_UDP);
		/*
		 * The inner l2 length of mbuf is the sum of outer l4 length,
		 * tunneling header length and inner l2 length for a tunnel
		 * packet. But in hns3 tx descriptor, the tunneling header
		 * length is contained in the field of outer L4 length.
		 * Therefore, driver need to calculate the outer L4 length and
		 * inner L2 length.
		 */
		tmp_outer |= hns3_gen_field_val(HNS3_TXD_L4LEN_M,
						HNS3_TXD_L4LEN_S,
						(uint8_t)RTE_ETHER_VXLAN_HLEN >>
						HNS3_L4_LEN_UNIT);

		inner_l2_len = m->l2_len - RTE_ETHER_VXLAN_HLEN;
		break;
	case RTE_MBUF_F_TX_TUNNEL_GRE:
		tmp_outer |= hns3_gen_field_val(HNS3_TXD_TUNTYPE_M,
					HNS3_TXD_TUNTYPE_S, HNS3_TUN_NVGRE);
		/*
		 * For NVGRE tunnel packet, the outer L4 is empty. So only
		 * fill the NVGRE header length to the outer L4 field.
		 */
		tmp_outer |= hns3_gen_field_val(HNS3_TXD_L4LEN_M,
				HNS3_TXD_L4LEN_S,
				(uint8_t)HNS3_NVGRE_HLEN >> HNS3_L4_LEN_UNIT);

		inner_l2_len = m->l2_len - HNS3_NVGRE_HLEN;
		break;
	default:
		/* For non UDP / GRE tunneling, drop the tunnel packet */
		return -EINVAL;
	}

	tmp_inner |= hns3_gen_field_val(HNS3_TXD_L2LEN_M, HNS3_TXD_L2LEN_S,
					inner_l2_len >> HNS3_L2_LEN_UNIT);
	/* OL2 header size, defined in 2 bytes */
	tmp_outer |= hns3_gen_field_val(HNS3_TXD_L2LEN_M, HNS3_TXD_L2LEN_S,
					m->outer_l2_len >> HNS3_L2_LEN_UNIT);

	*type_cs_vlan_tso_len = tmp_inner;
	*ol_type_vlan_len_msec = tmp_outer;

	return 0;
}

static int
hns3_parse_tunneling_params(struct hns3_tx_queue *txq, struct rte_mbuf *m,
			    uint16_t tx_desc_id)
{
	struct hns3_desc *tx_ring = txq->tx_ring;
	struct hns3_desc *desc = &tx_ring[tx_desc_id];
	uint64_t ol_flags = m->ol_flags;
	uint32_t tmp_outer = 0;
	uint32_t tmp_inner = 0;
	uint32_t tmp_ol4cs;
	int ret;

	/*
	 * The tunnel header is contained in the inner L2 header field of the
	 * mbuf, but for hns3 descriptor, it is contained in the outer L4. So,
	 * there is a need that switching between them. To avoid multiple
	 * calculations, the length of the L2 header include the outer and
	 * inner, will be filled during the parsing of tunnel packets.
	 */
	if (!(ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK)) {
		/*
		 * For non tunnel type the tunnel type id is 0, so no need to
		 * assign a value to it. Only the inner(normal) L2 header length
		 * is assigned.
		 */
		tmp_inner |= hns3_gen_field_val(HNS3_TXD_L2LEN_M,
			       HNS3_TXD_L2LEN_S, m->l2_len >> HNS3_L2_LEN_UNIT);
	} else {
		/*
		 * If outer csum is not offload, the outer length may be filled
		 * with 0. And the length of the outer header is added to the
		 * inner l2_len. It would lead a cksum error. So driver has to
		 * calculate the header length.
		 */
		if (unlikely(!(ol_flags &
			(RTE_MBUF_F_TX_OUTER_IP_CKSUM | RTE_MBUF_F_TX_OUTER_UDP_CKSUM)) &&
					m->outer_l2_len == 0)) {
			struct rte_net_hdr_lens hdr_len;
			(void)rte_net_get_ptype(m, &hdr_len,
					RTE_PTYPE_L2_MASK | RTE_PTYPE_L3_MASK);
			m->outer_l3_len = hdr_len.l3_len;
			m->outer_l2_len = hdr_len.l2_len;
			m->l2_len = m->l2_len - hdr_len.l2_len - hdr_len.l3_len;
		}
		hns3_parse_outer_params(m, &tmp_outer);
		ret = hns3_parse_inner_params(m, &tmp_outer, &tmp_inner);
		if (ret)
			return -EINVAL;
	}

	desc->tx.ol_type_vlan_len_msec = rte_cpu_to_le_32(tmp_outer);
	desc->tx.type_cs_vlan_tso_len = rte_cpu_to_le_32(tmp_inner);
	tmp_ol4cs = ol_flags & RTE_MBUF_F_TX_OUTER_UDP_CKSUM ?
			BIT(HNS3_TXD_OL4CS_B) : 0;
	desc->tx.paylen_fd_dop_ol4cs = rte_cpu_to_le_32(tmp_ol4cs);

	return 0;
}

static void
hns3_parse_l3_cksum_params(struct rte_mbuf *m, uint32_t *type_cs_vlan_tso_len)
{
	uint64_t ol_flags = m->ol_flags;
	uint32_t l3_type;
	uint32_t tmp;

	tmp = *type_cs_vlan_tso_len;
	if (ol_flags & RTE_MBUF_F_TX_IPV4)
		l3_type = HNS3_L3T_IPV4;
	else if (ol_flags & RTE_MBUF_F_TX_IPV6)
		l3_type = HNS3_L3T_IPV6;
	else
		l3_type = HNS3_L3T_NONE;

	/* inner(/normal) L3 header size, defined in 4 bytes */
	tmp |= hns3_gen_field_val(HNS3_TXD_L3LEN_M, HNS3_TXD_L3LEN_S,
					m->l3_len >> HNS3_L3_LEN_UNIT);

	tmp |= hns3_gen_field_val(HNS3_TXD_L3T_M, HNS3_TXD_L3T_S, l3_type);

	/* Enable L3 checksum offloads */
	if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM)
		tmp |= BIT(HNS3_TXD_L3CS_B);
	*type_cs_vlan_tso_len = tmp;
}

static void
hns3_parse_l4_cksum_params(struct rte_mbuf *m, uint32_t *type_cs_vlan_tso_len)
{
	uint64_t ol_flags = m->ol_flags;
	uint32_t tmp;
	/* Enable L4 checksum offloads */
	switch (ol_flags & (RTE_MBUF_F_TX_L4_MASK | RTE_MBUF_F_TX_TCP_SEG)) {
	case RTE_MBUF_F_TX_TCP_CKSUM | RTE_MBUF_F_TX_TCP_SEG:
	case RTE_MBUF_F_TX_TCP_CKSUM:
	case RTE_MBUF_F_TX_TCP_SEG:
		tmp = *type_cs_vlan_tso_len;
		tmp |= hns3_gen_field_val(HNS3_TXD_L4T_M, HNS3_TXD_L4T_S,
					HNS3_L4T_TCP);
		break;
	case RTE_MBUF_F_TX_UDP_CKSUM:
		tmp = *type_cs_vlan_tso_len;
		tmp |= hns3_gen_field_val(HNS3_TXD_L4T_M, HNS3_TXD_L4T_S,
					HNS3_L4T_UDP);
		break;
	case RTE_MBUF_F_TX_SCTP_CKSUM:
		tmp = *type_cs_vlan_tso_len;
		tmp |= hns3_gen_field_val(HNS3_TXD_L4T_M, HNS3_TXD_L4T_S,
					HNS3_L4T_SCTP);
		break;
	default:
		return;
	}
	tmp |= BIT(HNS3_TXD_L4CS_B);
	tmp |= hns3_gen_field_val(HNS3_TXD_L4LEN_M, HNS3_TXD_L4LEN_S,
					m->l4_len >> HNS3_L4_LEN_UNIT);
	*type_cs_vlan_tso_len = tmp;
}

static void
hns3_txd_enable_checksum(struct hns3_tx_queue *txq, struct rte_mbuf *m,
			 uint16_t tx_desc_id)
{
	struct hns3_desc *tx_ring = txq->tx_ring;
	struct hns3_desc *desc = &tx_ring[tx_desc_id];
	uint32_t value = 0;

	hns3_parse_l3_cksum_params(m, &value);
	hns3_parse_l4_cksum_params(m, &value);

	desc->tx.type_cs_vlan_tso_len |= rte_cpu_to_le_32(value);
}

static bool
hns3_pkt_need_linearized(struct rte_mbuf *tx_pkts, uint32_t bd_num,
				 uint32_t max_non_tso_bd_num)
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
	if (bd_num <= max_non_tso_bd_num)
		return false;

	for (i = 0; m_last && i < max_non_tso_bd_num - 1;
	     i++, m_last = m_last->next)
		tot_len += m_last->data_len;

	if (!m_last)
		return true;

	/* ensure the first 8 frags is greater than mss + header */
	hdr_len = tx_pkts->l2_len + tx_pkts->l3_len + tx_pkts->l4_len;
	hdr_len += (tx_pkts->ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) ?
		   tx_pkts->outer_l2_len + tx_pkts->outer_l3_len : 0;
	if (tot_len + m_last->data_len < tx_pkts->tso_segsz + hdr_len)
		return true;

	/*
	 * ensure the sum of the data length of every 7 consecutive buffer
	 * is greater than mss except the last one.
	 */
	for (i = 0; m_last && i < bd_num - max_non_tso_bd_num; i++) {
		tot_len -= m_first->data_len;
		tot_len += m_last->data_len;

		if (tot_len < tx_pkts->tso_segsz)
			return true;

		m_first = m_first->next;
		m_last = m_last->next;
	}

	return false;
}

static bool
hns3_outer_ipv4_cksum_prepared(struct rte_mbuf *m, uint64_t ol_flags,
				uint32_t *l4_proto)
{
	struct rte_ipv4_hdr *ipv4_hdr;
	ipv4_hdr = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *,
					   m->outer_l2_len);
	if (ol_flags & RTE_MBUF_F_TX_OUTER_IP_CKSUM)
		ipv4_hdr->hdr_checksum = 0;
	if (ol_flags & RTE_MBUF_F_TX_OUTER_UDP_CKSUM) {
		struct rte_udp_hdr *udp_hdr;
		/*
		 * If OUTER_UDP_CKSUM is support, HW can calculate the pseudo
		 * header for TSO packets
		 */
		if (ol_flags & RTE_MBUF_F_TX_TCP_SEG)
			return true;
		udp_hdr = rte_pktmbuf_mtod_offset(m, struct rte_udp_hdr *,
				m->outer_l2_len + m->outer_l3_len);
		udp_hdr->dgram_cksum = rte_ipv4_phdr_cksum(ipv4_hdr, ol_flags);

		return true;
	}
	*l4_proto = ipv4_hdr->next_proto_id;
	return false;
}

static bool
hns3_outer_ipv6_cksum_prepared(struct rte_mbuf *m, uint64_t ol_flags,
				uint32_t *l4_proto)
{
	struct rte_ipv6_hdr *ipv6_hdr;
	ipv6_hdr = rte_pktmbuf_mtod_offset(m, struct rte_ipv6_hdr *,
					   m->outer_l2_len);
	if (ol_flags & RTE_MBUF_F_TX_OUTER_UDP_CKSUM) {
		struct rte_udp_hdr *udp_hdr;
		/*
		 * If OUTER_UDP_CKSUM is support, HW can calculate the pseudo
		 * header for TSO packets
		 */
		if (ol_flags & RTE_MBUF_F_TX_TCP_SEG)
			return true;
		udp_hdr = rte_pktmbuf_mtod_offset(m, struct rte_udp_hdr *,
				m->outer_l2_len + m->outer_l3_len);
		udp_hdr->dgram_cksum = rte_ipv6_phdr_cksum(ipv6_hdr, ol_flags);

		return true;
	}
	*l4_proto = ipv6_hdr->proto;
	return false;
}

static void
hns3_outer_header_cksum_prepare(struct rte_mbuf *m)
{
	uint64_t ol_flags = m->ol_flags;
	uint32_t paylen, hdr_len, l4_proto;
	struct rte_udp_hdr *udp_hdr;

	if (!(ol_flags & (RTE_MBUF_F_TX_OUTER_IPV4 | RTE_MBUF_F_TX_OUTER_IPV6)))
		return;

	if (ol_flags & RTE_MBUF_F_TX_OUTER_IPV4) {
		if (hns3_outer_ipv4_cksum_prepared(m, ol_flags, &l4_proto))
			return;
	} else {
		if (hns3_outer_ipv6_cksum_prepared(m, ol_flags, &l4_proto))
			return;
	}

	/* driver should ensure the outer udp cksum is 0 for TUNNEL TSO */
	if (l4_proto == IPPROTO_UDP && (ol_flags & RTE_MBUF_F_TX_TCP_SEG)) {
		hdr_len = m->l2_len + m->l3_len + m->l4_len;
		hdr_len += m->outer_l2_len + m->outer_l3_len;
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
	hdr_len += (m->ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) ?
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

	if (!txq->pvid_sw_shift_en)
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
	if (m->ol_flags & RTE_MBUF_F_TX_QINQ)
		return -EINVAL;

	eh = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	if (eh->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN)) {
		if (m->ol_flags & RTE_MBUF_F_TX_VLAN)
			return -EINVAL;

		/* Ensure the incoming packet is not a QinQ packet */
		vh = (struct rte_vlan_hdr *)(eh + 1);
		if (vh->eth_proto == rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN))
			return -EINVAL;
	}

	return 0;
}
#endif

static uint16_t
hns3_udp_cksum_help(struct rte_mbuf *m)
{
	uint64_t ol_flags = m->ol_flags;
	uint16_t cksum = 0;
	uint32_t l4_len;

	if (ol_flags & RTE_MBUF_F_TX_IPV4) {
		struct rte_ipv4_hdr *ipv4_hdr = rte_pktmbuf_mtod_offset(m,
				struct rte_ipv4_hdr *, m->l2_len);
		l4_len = rte_be_to_cpu_16(ipv4_hdr->total_length) - m->l3_len;
	} else {
		struct rte_ipv6_hdr *ipv6_hdr = rte_pktmbuf_mtod_offset(m,
				struct rte_ipv6_hdr *, m->l2_len);
		l4_len = rte_be_to_cpu_16(ipv6_hdr->payload_len);
	}

	rte_raw_cksum_mbuf(m, m->l2_len + m->l3_len, l4_len, &cksum);

	cksum = ~cksum;
	/*
	 * RFC 768:If the computed checksum is zero for UDP, it is transmitted
	 * as all ones
	 */
	if (cksum == 0)
		cksum = 0xffff;

	return (uint16_t)cksum;
}

static bool
hns3_validate_tunnel_cksum(struct hns3_tx_queue *tx_queue, struct rte_mbuf *m)
{
	uint64_t ol_flags = m->ol_flags;
	struct rte_udp_hdr *udp_hdr;
	uint16_t dst_port;

	if (tx_queue->udp_cksum_mode == HNS3_SPECIAL_PORT_HW_CKSUM_MODE ||
	    ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK ||
	    (ol_flags & RTE_MBUF_F_TX_L4_MASK) != RTE_MBUF_F_TX_UDP_CKSUM)
		return true;
	/*
	 * A UDP packet with the same dst_port as VXLAN\VXLAN_GPE\GENEVE will
	 * be recognized as a tunnel packet in HW. In this case, if UDP CKSUM
	 * offload is set and the tunnel mask has not been set, the CKSUM will
	 * be wrong since the header length is wrong and driver should complete
	 * the CKSUM to avoid CKSUM error.
	 */
	udp_hdr = rte_pktmbuf_mtod_offset(m, struct rte_udp_hdr *,
						m->l2_len + m->l3_len);
	dst_port = rte_be_to_cpu_16(udp_hdr->dst_port);
	switch (dst_port) {
	case RTE_VXLAN_DEFAULT_PORT:
	case RTE_VXLAN_GPE_DEFAULT_PORT:
	case RTE_GENEVE_DEFAULT_PORT:
		udp_hdr->dgram_cksum = hns3_udp_cksum_help(m);
		m->ol_flags = ol_flags & ~RTE_MBUF_F_TX_L4_MASK;
		return false;
	default:
		return true;
	}
}

static int
hns3_prep_pkt_proc(struct hns3_tx_queue *tx_queue, struct rte_mbuf *m)
{
	int ret;

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
	ret = rte_validate_tx_offload(m);
	if (ret != 0) {
		rte_errno = -ret;
		return ret;
	}

	ret = hns3_vld_vlan_chk(tx_queue, m);
	if (ret != 0) {
		rte_errno = EINVAL;
		return ret;
	}
#endif
	if (hns3_pkt_is_tso(m)) {
		if (hns3_pkt_need_linearized(m, m->nb_segs,
		    tx_queue->max_non_tso_bd_num) ||
		    hns3_check_tso_pkt_valid(m)) {
			rte_errno = EINVAL;
			return -EINVAL;
		}

		if (tx_queue->tso_mode != HNS3_TSO_SW_CAL_PSEUDO_H_CSUM) {
			/*
			 * (tso mode != HNS3_TSO_SW_CAL_PSEUDO_H_CSUM) means
			 * hardware support recalculate the TCP pseudo header
			 * checksum of packets that need TSO, so network driver
			 * software not need to recalculate it.
			 */
			hns3_outer_header_cksum_prepare(m);
			return 0;
		}
	}

	ret = rte_net_intel_cksum_prepare(m);
	if (ret != 0) {
		rte_errno = -ret;
		return ret;
	}

	if (!hns3_validate_tunnel_cksum(tx_queue, m))
		return 0;

	hns3_outer_header_cksum_prepare(m);

	return 0;
}

uint16_t
hns3_prep_pkts(__rte_unused void *tx_queue, struct rte_mbuf **tx_pkts,
	       uint16_t nb_pkts)
{
	struct rte_mbuf *m;
	uint16_t i;

	for (i = 0; i < nb_pkts; i++) {
		m = tx_pkts[i];
		if (hns3_prep_pkt_proc(tx_queue, m))
			return i;
	}

	return i;
}

static int
hns3_parse_cksum(struct hns3_tx_queue *txq, uint16_t tx_desc_id,
		 struct rte_mbuf *m)
{
	struct hns3_desc *tx_ring = txq->tx_ring;
	struct hns3_desc *desc = &tx_ring[tx_desc_id];

	/* Enable checksum offloading */
	if (m->ol_flags & HNS3_TX_CKSUM_OFFLOAD_MASK) {
		/* Fill in tunneling parameters if necessary */
		if (hns3_parse_tunneling_params(txq, m, tx_desc_id)) {
			txq->dfx_stats.unsupported_tunnel_pkt_cnt++;
				return -EINVAL;
		}

		hns3_txd_enable_checksum(txq, m, tx_desc_id);
	} else {
		/* clear the control bit */
		desc->tx.type_cs_vlan_tso_len  = 0;
		desc->tx.ol_type_vlan_len_msec = 0;
	}

	return 0;
}

static int
hns3_check_non_tso_pkt(uint16_t nb_buf, struct rte_mbuf **m_seg,
		      struct rte_mbuf *tx_pkt, struct hns3_tx_queue *txq)
{
	uint8_t max_non_tso_bd_num;
	struct rte_mbuf *new_pkt;
	int ret;

	if (hns3_pkt_is_tso(*m_seg))
		return 0;

	/*
	 * If packet length is greater than HNS3_MAX_FRAME_LEN
	 * driver support, the packet will be ignored.
	 */
	if (unlikely(rte_pktmbuf_pkt_len(tx_pkt) > HNS3_MAX_FRAME_LEN)) {
		txq->dfx_stats.over_length_pkt_cnt++;
		return -EINVAL;
	}

	max_non_tso_bd_num = txq->max_non_tso_bd_num;
	if (unlikely(nb_buf > max_non_tso_bd_num)) {
		txq->dfx_stats.exceed_limit_bd_pkt_cnt++;
		ret = hns3_reassemble_tx_pkts(tx_pkt, &new_pkt,
					      max_non_tso_bd_num);
		if (ret) {
			txq->dfx_stats.exceed_limit_bd_reassem_fail++;
			return ret;
		}
		*m_seg = new_pkt;
	}

	return 0;
}

static inline void
hns3_tx_free_buffer_simple(struct hns3_tx_queue *txq)
{
	struct hns3_entry *tx_entry;
	struct hns3_desc *desc;
	uint16_t tx_next_clean;
	uint16_t i;

	while (1) {
		if (HNS3_GET_TX_QUEUE_PEND_BD_NUM(txq) < txq->tx_rs_thresh)
			break;

		/*
		 * All mbufs can be released only when the VLD bits of all
		 * descriptors in a batch are cleared.
		 */
		tx_next_clean = (txq->next_to_clean + txq->tx_rs_thresh - 1) %
				txq->nb_tx_desc;
		desc = &txq->tx_ring[tx_next_clean];
		for (i = 0; i < txq->tx_rs_thresh; i++) {
			if (rte_le_to_cpu_16(desc->tx.tp_fe_sc_vld_ra_ri) &
					BIT(HNS3_TXD_VLD_B))
				return;
			desc--;
		}

		tx_entry = &txq->sw_ring[txq->next_to_clean];

		if (txq->mbuf_fast_free_en) {
			rte_mempool_put_bulk(tx_entry->mbuf->pool,
					(void **)tx_entry, txq->tx_rs_thresh);
			for (i = 0; i < txq->tx_rs_thresh; i++)
				tx_entry[i].mbuf = NULL;
			goto update_field;
		}

		for (i = 0; i < txq->tx_rs_thresh; i++)
			rte_prefetch0((tx_entry + i)->mbuf);
		for (i = 0; i < txq->tx_rs_thresh; i++, tx_entry++) {
			rte_mempool_put(tx_entry->mbuf->pool, tx_entry->mbuf);
			tx_entry->mbuf = NULL;
		}

update_field:
		txq->next_to_clean = (tx_next_clean + 1) % txq->nb_tx_desc;
		txq->tx_bd_ready += txq->tx_rs_thresh;
	}
}

static inline void
hns3_tx_backup_1mbuf(struct hns3_entry *tx_entry, struct rte_mbuf **pkts)
{
	tx_entry->mbuf = pkts[0];
}

static inline void
hns3_tx_backup_4mbuf(struct hns3_entry *tx_entry, struct rte_mbuf **pkts)
{
	hns3_tx_backup_1mbuf(&tx_entry[0], &pkts[0]);
	hns3_tx_backup_1mbuf(&tx_entry[1], &pkts[1]);
	hns3_tx_backup_1mbuf(&tx_entry[2], &pkts[2]);
	hns3_tx_backup_1mbuf(&tx_entry[3], &pkts[3]);
}

static inline void
hns3_tx_setup_4bd(struct hns3_desc *txdp, struct rte_mbuf **pkts)
{
#define PER_LOOP_NUM	4
	uint16_t bd_flag = BIT(HNS3_TXD_VLD_B) | BIT(HNS3_TXD_FE_B);
	uint64_t dma_addr;
	uint32_t i;

	for (i = 0; i < PER_LOOP_NUM; i++, txdp++, pkts++) {
		dma_addr = rte_mbuf_data_iova(*pkts);
		txdp->addr = rte_cpu_to_le_64(dma_addr);
		txdp->tx.send_size = rte_cpu_to_le_16((*pkts)->data_len);
		txdp->tx.paylen_fd_dop_ol4cs = 0;
		txdp->tx.type_cs_vlan_tso_len = 0;
		txdp->tx.ol_type_vlan_len_msec = 0;
		if (unlikely((*pkts)->ol_flags & RTE_MBUF_F_TX_IEEE1588_TMST))
			bd_flag |= BIT(HNS3_TXD_TSYN_B);
		txdp->tx.tp_fe_sc_vld_ra_ri = rte_cpu_to_le_16(bd_flag);
	}
}

static inline void
hns3_tx_setup_1bd(struct hns3_desc *txdp, struct rte_mbuf **pkts)
{
	uint16_t bd_flag = BIT(HNS3_TXD_VLD_B) | BIT(HNS3_TXD_FE_B);
	uint64_t dma_addr;

	dma_addr = rte_mbuf_data_iova(*pkts);
	txdp->addr = rte_cpu_to_le_64(dma_addr);
	txdp->tx.send_size = rte_cpu_to_le_16((*pkts)->data_len);
	txdp->tx.paylen_fd_dop_ol4cs = 0;
	txdp->tx.type_cs_vlan_tso_len = 0;
	txdp->tx.ol_type_vlan_len_msec = 0;
	if (unlikely((*pkts)->ol_flags & RTE_MBUF_F_TX_IEEE1588_TMST))
		bd_flag |= BIT(HNS3_TXD_TSYN_B);
	txdp->tx.tp_fe_sc_vld_ra_ri = rte_cpu_to_le_16(bd_flag);
}

static inline void
hns3_tx_fill_hw_ring(struct hns3_tx_queue *txq,
		     struct rte_mbuf **pkts,
		     uint16_t nb_pkts)
{
#define PER_LOOP_NUM	4
#define PER_LOOP_MASK	(PER_LOOP_NUM - 1)
	struct hns3_desc *txdp = &txq->tx_ring[txq->next_to_use];
	struct hns3_entry *tx_entry = &txq->sw_ring[txq->next_to_use];
	const uint32_t mainpart = (nb_pkts & ((uint32_t)~PER_LOOP_MASK));
	const uint32_t leftover = (nb_pkts & ((uint32_t)PER_LOOP_MASK));
	uint32_t i;

	for (i = 0; i < mainpart; i += PER_LOOP_NUM) {
		hns3_tx_backup_4mbuf(tx_entry + i, pkts + i);
		hns3_tx_setup_4bd(txdp + i, pkts + i);

		/* Increment bytes counter */
		uint32_t j;
		for (j = 0; j < PER_LOOP_NUM; j++)
			txq->basic_stats.bytes += pkts[i + j]->pkt_len;
	}
	if (unlikely(leftover > 0)) {
		for (i = 0; i < leftover; i++) {
			hns3_tx_backup_1mbuf(tx_entry + mainpart + i,
					     pkts + mainpart + i);
			hns3_tx_setup_1bd(txdp + mainpart + i,
					  pkts + mainpart + i);

			/* Increment bytes counter */
			txq->basic_stats.bytes += pkts[mainpart + i]->pkt_len;
		}
	}
}

uint16_t
hns3_xmit_pkts_simple(void *tx_queue,
		      struct rte_mbuf **tx_pkts,
		      uint16_t nb_pkts)
{
	struct hns3_tx_queue *txq = tx_queue;
	uint16_t nb_tx = 0;

	hns3_tx_free_buffer_simple(txq);

	nb_pkts = RTE_MIN(txq->tx_bd_ready, nb_pkts);
	if (unlikely(nb_pkts == 0)) {
		if (txq->tx_bd_ready == 0)
			txq->dfx_stats.queue_full_cnt++;
		return 0;
	}

	txq->tx_bd_ready -= nb_pkts;
	if (txq->next_to_use + nb_pkts >= txq->nb_tx_desc) {
		nb_tx = txq->nb_tx_desc - txq->next_to_use;
		hns3_tx_fill_hw_ring(txq, tx_pkts, nb_tx);
		txq->next_to_use = 0;
	}

	if (nb_pkts > nb_tx) {
		hns3_tx_fill_hw_ring(txq, tx_pkts + nb_tx, nb_pkts - nb_tx);
		txq->next_to_use += nb_pkts - nb_tx;
	}

	hns3_write_txq_tail_reg(txq, nb_pkts);

	return nb_pkts;
}

uint16_t
hns3_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
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
			txq->dfx_stats.queue_full_cnt++;
			if (nb_tx == 0)
				return 0;
			goto end_of_tx;
		}

		/*
		 * If packet length is less than minimum packet length supported
		 * by hardware in Tx direction, driver need to pad it to avoid
		 * error.
		 */
		if (unlikely(rte_pktmbuf_pkt_len(tx_pkt) <
						txq->min_tx_pkt_len)) {
			uint16_t add_len;
			char *appended;

			add_len = txq->min_tx_pkt_len -
					 rte_pktmbuf_pkt_len(tx_pkt);
			appended = rte_pktmbuf_append(tx_pkt, add_len);
			if (appended == NULL) {
				txq->dfx_stats.pkt_padding_fail_cnt++;
				break;
			}

			memset(appended, 0, add_len);
		}

		m_seg = tx_pkt;

		if (hns3_check_non_tso_pkt(nb_buf, &m_seg, tx_pkt, txq))
			goto end_of_tx;

		if (hns3_parse_cksum(txq, tx_next_use, m_seg))
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

		/* Increment bytes counter */
		txq->basic_stats.bytes += tx_pkt->pkt_len;
		nb_hold += i;
		txq->next_to_use = tx_next_use;
		txq->tx_bd_ready -= i;
	}

end_of_tx:

	if (likely(nb_tx))
		hns3_write_txq_tail_reg(txq, nb_hold);

	return nb_tx;
}

int __rte_weak
hns3_tx_check_vec_support(__rte_unused struct rte_eth_dev *dev)
{
	return -ENOTSUP;
}

uint16_t __rte_weak
hns3_xmit_pkts_vec(__rte_unused void *tx_queue,
		   __rte_unused struct rte_mbuf **tx_pkts,
		   __rte_unused uint16_t nb_pkts)
{
	return 0;
}

uint16_t __rte_weak
hns3_xmit_pkts_vec_sve(void __rte_unused * tx_queue,
		       struct rte_mbuf __rte_unused **tx_pkts,
		       uint16_t __rte_unused nb_pkts)
{
	return 0;
}

int
hns3_tx_burst_mode_get(struct rte_eth_dev *dev, __rte_unused uint16_t queue_id,
		       struct rte_eth_burst_mode *mode)
{
	static const struct {
		eth_tx_burst_t pkt_burst;
		const char *info;
	} burst_infos[] = {
		{ hns3_xmit_pkts_simple,	"Scalar Simple" },
		{ hns3_xmit_pkts,		"Scalar"        },
		{ hns3_xmit_pkts_vec,		"Vector Neon"   },
		{ hns3_xmit_pkts_vec_sve,	"Vector Sve"    },
		{ rte_eth_pkt_burst_dummy,	"Dummy"         },
	};

	eth_tx_burst_t pkt_burst = dev->tx_pkt_burst;
	int ret = -EINVAL;
	unsigned int i;

	for (i = 0; i < RTE_DIM(burst_infos); i++) {
		if (pkt_burst == burst_infos[i].pkt_burst) {
			snprintf(mode->info, sizeof(mode->info), "%s",
				 burst_infos[i].info);
			ret = 0;
			break;
		}
	}

	return ret;
}

static bool
hns3_tx_check_simple_support(struct rte_eth_dev *dev)
{
	uint64_t offloads = dev->data->dev_conf.txmode.offloads;

	return (offloads == (offloads & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE));
}

static bool
hns3_get_tx_prep_needed(struct rte_eth_dev *dev)
{
#define HNS3_DEV_TX_CSKUM_TSO_OFFLOAD_MASK (\
		RTE_ETH_TX_OFFLOAD_IPV4_CKSUM | \
		RTE_ETH_TX_OFFLOAD_TCP_CKSUM | \
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM | \
		RTE_ETH_TX_OFFLOAD_SCTP_CKSUM | \
		RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM | \
		RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM | \
		RTE_ETH_TX_OFFLOAD_TCP_TSO | \
		RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO | \
		RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO | \
		RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO)

	uint64_t tx_offload = dev->data->dev_conf.txmode.offloads;

	if (tx_offload & HNS3_DEV_TX_CSKUM_TSO_OFFLOAD_MASK)
		return true;

	return false;
}

static eth_tx_prep_t
hns3_get_tx_prepare(struct rte_eth_dev *dev)
{
	return hns3_get_tx_prep_needed(dev) ? hns3_prep_pkts : NULL;
}

static eth_tx_burst_t
hns3_get_tx_function(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	bool vec_allowed, sve_allowed, simple_allowed;
	bool vec_support;

	vec_support = hns3_tx_check_vec_support(dev) == 0;
	vec_allowed = vec_support && hns3_get_default_vec_support();
	sve_allowed = vec_support && hns3_get_sve_support();
	simple_allowed = hns3_tx_check_simple_support(dev);

	if (hns->tx_func_hint == HNS3_IO_FUNC_HINT_VEC && vec_allowed)
		return hns3_xmit_pkts_vec;
	if (hns->tx_func_hint == HNS3_IO_FUNC_HINT_SVE && sve_allowed)
		return hns3_xmit_pkts_vec_sve;
	if (hns->tx_func_hint == HNS3_IO_FUNC_HINT_SIMPLE && simple_allowed)
		return hns3_xmit_pkts_simple;
	if (hns->tx_func_hint == HNS3_IO_FUNC_HINT_COMMON)
		return hns3_xmit_pkts;

	if (vec_allowed)
		return hns3_xmit_pkts_vec;
	if (simple_allowed)
		return hns3_xmit_pkts_simple;

	return hns3_xmit_pkts;
}

static void
hns3_trace_rxtx_function(struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_eth_burst_mode rx_mode;
	struct rte_eth_burst_mode tx_mode;

	memset(&rx_mode, 0, sizeof(rx_mode));
	memset(&tx_mode, 0, sizeof(tx_mode));
	(void)hns3_rx_burst_mode_get(dev, 0, &rx_mode);
	(void)hns3_tx_burst_mode_get(dev, 0, &tx_mode);

	hns3_dbg(hw, "using rx_pkt_burst: %s, tx_pkt_burst: %s.",
		 rx_mode.info, tx_mode.info);
}

static void
hns3_eth_dev_fp_ops_config(const struct rte_eth_dev *dev)
{
	struct rte_eth_fp_ops *fpo = rte_eth_fp_ops;
	uint16_t port_id = dev->data->port_id;

	fpo[port_id].rx_pkt_burst = dev->rx_pkt_burst;
	fpo[port_id].tx_pkt_burst = dev->tx_pkt_burst;
	fpo[port_id].tx_pkt_prepare = dev->tx_pkt_prepare;
	fpo[port_id].rx_descriptor_status = dev->rx_descriptor_status;
	fpo[port_id].tx_descriptor_status = dev->tx_descriptor_status;
	fpo[port_id].rxq.data = dev->data->rx_queues;
	fpo[port_id].txq.data = dev->data->tx_queues;
}

void
hns3_set_rxtx_function(struct rte_eth_dev *eth_dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct hns3_adapter *hns = eth_dev->data->dev_private;

	if (hns->hw.adapter_state == HNS3_NIC_STARTED &&
	    __atomic_load_n(&hns->hw.reset.resetting, __ATOMIC_RELAXED) == 0) {
		eth_dev->rx_pkt_burst = hns3_get_rx_function(eth_dev);
		eth_dev->rx_descriptor_status = hns3_dev_rx_descriptor_status;
		eth_dev->tx_pkt_burst = hw->set_link_down ?
					rte_eth_pkt_burst_dummy :
					hns3_get_tx_function(eth_dev);
		eth_dev->tx_pkt_prepare = hns3_get_tx_prepare(eth_dev);
		eth_dev->tx_descriptor_status = hns3_dev_tx_descriptor_status;
	} else {
		eth_dev->rx_pkt_burst = rte_eth_pkt_burst_dummy;
		eth_dev->tx_pkt_burst = rte_eth_pkt_burst_dummy;
		eth_dev->tx_pkt_prepare = NULL;
	}

	hns3_trace_rxtx_function(eth_dev);
	hns3_eth_dev_fp_ops_config(eth_dev);
}

void
hns3_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		  struct rte_eth_rxq_info *qinfo)
{
	struct hns3_rx_queue *rxq = dev->data->rx_queues[queue_id];

	qinfo->mp = rxq->mb_pool;
	qinfo->nb_desc = rxq->nb_rx_desc;
	qinfo->scattered_rx = dev->data->scattered_rx;
	/* Report the HW Rx buffer length to user */
	qinfo->rx_buf_size = rxq->rx_buf_len;

	/*
	 * If there are no available Rx buffer descriptors, incoming packets
	 * are always dropped by hardware based on hns3 network engine.
	 */
	qinfo->conf.rx_drop_en = 1;
	qinfo->conf.offloads = dev->data->dev_conf.rxmode.offloads;
	qinfo->conf.rx_free_thresh = rxq->rx_free_thresh;
	qinfo->conf.rx_deferred_start = rxq->rx_deferred_start;
}

void
hns3_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		  struct rte_eth_txq_info *qinfo)
{
	struct hns3_tx_queue *txq = dev->data->tx_queues[queue_id];

	qinfo->nb_desc = txq->nb_tx_desc;
	qinfo->conf.offloads = dev->data->dev_conf.txmode.offloads;
	qinfo->conf.tx_rs_thresh = txq->tx_rs_thresh;
	qinfo->conf.tx_free_thresh = txq->tx_free_thresh;
	qinfo->conf.tx_deferred_start = txq->tx_deferred_start;
}

int
hns3_dev_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_rx_queue *rxq = dev->data->rx_queues[rx_queue_id];
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	int ret;

	if (!hns3_dev_get_support(hw, INDEP_TXRX))
		return -ENOTSUP;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_reset_queue(hw, rx_queue_id, HNS3_RING_TYPE_RX);
	if (ret) {
		hns3_err(hw, "fail to reset Rx queue %u, ret = %d.",
			 rx_queue_id, ret);
		rte_spinlock_unlock(&hw->lock);
		return ret;
	}

	ret = hns3_init_rxq(hns, rx_queue_id);
	if (ret) {
		hns3_err(hw, "fail to init Rx queue %u, ret = %d.",
			 rx_queue_id, ret);
		rte_spinlock_unlock(&hw->lock);
		return ret;
	}

	hns3_enable_rxq(rxq, true);
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

static void
hns3_reset_sw_rxq(struct hns3_rx_queue *rxq)
{
	rxq->next_to_use = 0;
	rxq->rx_rearm_start = 0;
	rxq->rx_free_hold = 0;
	rxq->rx_rearm_nb = 0;
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;
	memset(&rxq->rx_ring[0], 0, rxq->nb_rx_desc * sizeof(struct hns3_desc));
	hns3_rxq_vec_setup(rxq);
}

int
hns3_dev_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_rx_queue *rxq = dev->data->rx_queues[rx_queue_id];

	if (!hns3_dev_get_support(hw, INDEP_TXRX))
		return -ENOTSUP;

	rte_spinlock_lock(&hw->lock);
	hns3_enable_rxq(rxq, false);

	hns3_rx_queue_release_mbufs(rxq);

	hns3_reset_sw_rxq(rxq);
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;
	rte_spinlock_unlock(&hw->lock);

	return 0;
}

int
hns3_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_tx_queue *txq = dev->data->tx_queues[tx_queue_id];
	int ret;

	if (!hns3_dev_get_support(hw, INDEP_TXRX))
		return -ENOTSUP;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_reset_queue(hw, tx_queue_id, HNS3_RING_TYPE_TX);
	if (ret) {
		hns3_err(hw, "fail to reset Tx queue %u, ret = %d.",
			 tx_queue_id, ret);
		rte_spinlock_unlock(&hw->lock);
		return ret;
	}

	hns3_init_txq(txq);
	hns3_enable_txq(txq, true);
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

int
hns3_dev_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_tx_queue *txq = dev->data->tx_queues[tx_queue_id];

	if (!hns3_dev_get_support(hw, INDEP_TXRX))
		return -ENOTSUP;

	rte_spinlock_lock(&hw->lock);
	hns3_enable_txq(txq, false);
	hns3_tx_queue_release_mbufs(txq);
	/*
	 * All the mbufs in sw_ring are released and all the pointers in sw_ring
	 * are set to NULL. If this queue is still called by upper layer,
	 * residual SW status of this txq may cause these pointers in sw_ring
	 * which have been set to NULL to be released again. To avoid it,
	 * reinit the txq.
	 */
	hns3_init_txq(txq);
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;
	rte_spinlock_unlock(&hw->lock);

	return 0;
}

static int
hns3_tx_done_cleanup_full(struct hns3_tx_queue *txq, uint32_t free_cnt)
{
	uint16_t next_to_clean = txq->next_to_clean;
	uint16_t next_to_use   = txq->next_to_use;
	uint16_t tx_bd_ready   = txq->tx_bd_ready;
	struct hns3_entry *tx_pkt = &txq->sw_ring[next_to_clean];
	struct hns3_desc *desc = &txq->tx_ring[next_to_clean];
	uint32_t idx;

	if (free_cnt == 0 || free_cnt > txq->nb_tx_desc)
		free_cnt = txq->nb_tx_desc;

	for (idx = 0; idx < free_cnt; idx++) {
		if (next_to_clean == next_to_use)
			break;
		if (desc->tx.tp_fe_sc_vld_ra_ri &
		    rte_cpu_to_le_16(BIT(HNS3_TXD_VLD_B)))
			break;
		if (tx_pkt->mbuf != NULL) {
			rte_pktmbuf_free_seg(tx_pkt->mbuf);
			tx_pkt->mbuf = NULL;
		}
		next_to_clean++;
		tx_bd_ready++;
		tx_pkt++;
		desc++;
		if (next_to_clean == txq->nb_tx_desc) {
			tx_pkt = txq->sw_ring;
			desc = txq->tx_ring;
			next_to_clean = 0;
		}
	}

	if (idx > 0) {
		txq->next_to_clean = next_to_clean;
		txq->tx_bd_ready = tx_bd_ready;
	}

	return (int)idx;
}

int
hns3_tx_done_cleanup(void *txq, uint32_t free_cnt)
{
	struct hns3_tx_queue *q = (struct hns3_tx_queue *)txq;
	struct rte_eth_dev *dev = &rte_eth_devices[q->port_id];

	if (dev->tx_pkt_burst == hns3_xmit_pkts)
		return hns3_tx_done_cleanup_full(q, free_cnt);
	else if (dev->tx_pkt_burst == rte_eth_pkt_burst_dummy)
		return 0;
	else
		return -ENOTSUP;
}

int
hns3_dev_rx_descriptor_status(void *rx_queue, uint16_t offset)
{
	volatile struct hns3_desc *rxdp;
	struct hns3_rx_queue *rxq;
	struct rte_eth_dev *dev;
	uint32_t bd_base_info;
	uint16_t desc_id;

	rxq = (struct hns3_rx_queue *)rx_queue;
	if (offset >= rxq->nb_rx_desc)
		return -EINVAL;

	desc_id = (rxq->next_to_use + offset) % rxq->nb_rx_desc;
	rxdp = &rxq->rx_ring[desc_id];
	bd_base_info = rte_le_to_cpu_32(rxdp->rx.bd_base_info);
	dev = &rte_eth_devices[rxq->port_id];
	if (dev->rx_pkt_burst == hns3_recv_pkts_simple ||
	    dev->rx_pkt_burst == hns3_recv_scattered_pkts) {
		if (offset >= rxq->nb_rx_desc - rxq->rx_free_hold)
			return RTE_ETH_RX_DESC_UNAVAIL;
	} else if (dev->rx_pkt_burst == hns3_recv_pkts_vec ||
		   dev->rx_pkt_burst == hns3_recv_pkts_vec_sve) {
		if (offset >= rxq->nb_rx_desc - rxq->rx_rearm_nb)
			return RTE_ETH_RX_DESC_UNAVAIL;
	} else {
		return RTE_ETH_RX_DESC_UNAVAIL;
	}

	if (!(bd_base_info & BIT(HNS3_RXD_VLD_B)))
		return RTE_ETH_RX_DESC_AVAIL;
	else
		return RTE_ETH_RX_DESC_DONE;
}

int
hns3_dev_tx_descriptor_status(void *tx_queue, uint16_t offset)
{
	volatile struct hns3_desc *txdp;
	struct hns3_tx_queue *txq;
	struct rte_eth_dev *dev;
	uint16_t desc_id;

	txq = (struct hns3_tx_queue *)tx_queue;
	if (offset >= txq->nb_tx_desc)
		return -EINVAL;

	dev = &rte_eth_devices[txq->port_id];
	if (dev->tx_pkt_burst != hns3_xmit_pkts_simple &&
	    dev->tx_pkt_burst != hns3_xmit_pkts &&
	    dev->tx_pkt_burst != hns3_xmit_pkts_vec_sve &&
	    dev->tx_pkt_burst != hns3_xmit_pkts_vec)
		return RTE_ETH_TX_DESC_UNAVAIL;

	desc_id = (txq->next_to_use + offset) % txq->nb_tx_desc;
	txdp = &txq->tx_ring[desc_id];
	if (txdp->tx.tp_fe_sc_vld_ra_ri & rte_cpu_to_le_16(BIT(HNS3_TXD_VLD_B)))
		return RTE_ETH_TX_DESC_FULL;
	else
		return RTE_ETH_TX_DESC_DONE;
}

uint32_t
hns3_rx_queue_count(void *rx_queue)
{
	/*
	 * Number of BDs that have been processed by the driver
	 * but have not been notified to the hardware.
	 */
	uint32_t driver_hold_bd_num;
	struct hns3_rx_queue *rxq;
	const struct rte_eth_dev *dev;
	uint32_t fbd_num;

	rxq = rx_queue;
	dev = &rte_eth_devices[rxq->port_id];

	fbd_num = hns3_read_dev(rxq, HNS3_RING_RX_FBDNUM_REG);
	if (dev->rx_pkt_burst == hns3_recv_pkts_vec ||
	    dev->rx_pkt_burst == hns3_recv_pkts_vec_sve)
		driver_hold_bd_num = rxq->rx_rearm_nb;
	else
		driver_hold_bd_num = rxq->rx_free_hold;

	if (fbd_num <= driver_hold_bd_num)
		return 0;
	else
		return fbd_num - driver_hold_bd_num;
}

void
hns3_enable_rxd_adv_layout(struct hns3_hw *hw)
{
	/*
	 * If the hardware support rxd advanced layout, then driver enable it
	 * default.
	 */
	if (hns3_dev_get_support(hw, RXD_ADV_LAYOUT))
		hns3_write_dev(hw, HNS3_RXD_ADV_LAYOUT_EN_REG, 1);
}

void
hns3_stop_tx_datapath(struct rte_eth_dev *dev)
{
	dev->tx_pkt_burst = rte_eth_pkt_burst_dummy;
	dev->tx_pkt_prepare = NULL;
	hns3_eth_dev_fp_ops_config(dev);

	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		return;

	rte_wmb();
	/* Disable tx datapath on secondary process. */
	hns3_mp_req_stop_tx(dev);
	/* Prevent crashes when queues are still in use. */
	rte_delay_ms(dev->data->nb_tx_queues);
}

void
hns3_start_tx_datapath(struct rte_eth_dev *dev)
{
	dev->tx_pkt_burst = hns3_get_tx_function(dev);
	dev->tx_pkt_prepare = hns3_get_tx_prepare(dev);
	hns3_eth_dev_fp_ops_config(dev);

	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		return;

	hns3_mp_req_start_tx(dev);
}

void
hns3_stop_rxtx_datapath(struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	hns3_set_rxtx_function(dev);

	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		return;

	rte_wmb();
	/* Disable datapath on secondary process. */
	hns3_mp_req_stop_rxtx(dev);
	/* Prevent crashes when queues are still in use. */
	rte_delay_ms(hw->cfg_max_queues);
}

void
hns3_start_rxtx_datapath(struct rte_eth_dev *dev)
{
	hns3_set_rxtx_function(dev);

	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		return;

	hns3_mp_req_start_rxtx(dev);
}
