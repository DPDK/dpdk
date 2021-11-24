/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <errno.h>
#include <stdbool.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <unistd.h>

#include <rte_interrupts.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_atomic.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <ethdev_pci.h>
#include <rte_kvargs.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_dev.h>

#include <iavf_devids.h>

#include "ice_generic_flow.h"
#include "ice_dcf_ethdev.h"
#include "ice_rxtx.h"

static int
ice_dcf_dev_udp_tunnel_port_add(struct rte_eth_dev *dev,
				struct rte_eth_udp_tunnel *udp_tunnel);
static int
ice_dcf_dev_udp_tunnel_port_del(struct rte_eth_dev *dev,
				struct rte_eth_udp_tunnel *udp_tunnel);

static int
ice_dcf_dev_init(struct rte_eth_dev *eth_dev);

static int
ice_dcf_dev_uninit(struct rte_eth_dev *eth_dev);

static uint16_t
ice_dcf_recv_pkts(__rte_unused void *rx_queue,
		  __rte_unused struct rte_mbuf **bufs,
		  __rte_unused uint16_t nb_pkts)
{
	return 0;
}

static uint16_t
ice_dcf_xmit_pkts(__rte_unused void *tx_queue,
		  __rte_unused struct rte_mbuf **bufs,
		  __rte_unused uint16_t nb_pkts)
{
	return 0;
}

static int
ice_dcf_init_rxq(struct rte_eth_dev *dev, struct ice_rx_queue *rxq)
{
	struct ice_dcf_adapter *dcf_ad = dev->data->dev_private;
	struct rte_eth_dev_data *dev_data = dev->data;
	struct iavf_hw *hw = &dcf_ad->real_hw.avf;
	uint16_t buf_size, max_pkt_len;

	buf_size = rte_pktmbuf_data_room_size(rxq->mp) - RTE_PKTMBUF_HEADROOM;
	rxq->rx_hdr_len = 0;
	rxq->rx_buf_len = RTE_ALIGN(buf_size, (1 << ICE_RLAN_CTX_DBUF_S));
	max_pkt_len = RTE_MIN(ICE_SUPPORT_CHAIN_NUM * rxq->rx_buf_len,
			      dev->data->mtu + ICE_ETH_OVERHEAD);

	/* Check maximum packet length is set correctly.  */
	if (max_pkt_len <= RTE_ETHER_MIN_LEN ||
	    max_pkt_len > ICE_FRAME_SIZE_MAX) {
		PMD_DRV_LOG(ERR, "maximum packet length must be "
			    "larger than %u and smaller than %u",
			    (uint32_t)RTE_ETHER_MIN_LEN,
			    (uint32_t)ICE_FRAME_SIZE_MAX);
		return -EINVAL;
	}

	rxq->max_pkt_len = max_pkt_len;
	if ((dev_data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_SCATTER) ||
	    (rxq->max_pkt_len + 2 * RTE_VLAN_HLEN) > buf_size) {
		dev_data->scattered_rx = 1;
	}
	rxq->qrx_tail = hw->hw_addr + IAVF_QRX_TAIL1(rxq->queue_id);
	IAVF_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);
	IAVF_WRITE_FLUSH(hw);

	return 0;
}

static int
ice_dcf_init_rx_queues(struct rte_eth_dev *dev)
{
	struct ice_rx_queue **rxq =
		(struct ice_rx_queue **)dev->data->rx_queues;
	int i, ret;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (!rxq[i] || !rxq[i]->q_set)
			continue;
		ret = ice_dcf_init_rxq(dev, rxq[i]);
		if (ret)
			return ret;
	}

	ice_set_rx_function(dev);
	ice_set_tx_function(dev);

	return 0;
}

#define IAVF_MISC_VEC_ID                RTE_INTR_VEC_ZERO_OFFSET
#define IAVF_RX_VEC_START               RTE_INTR_VEC_RXTX_OFFSET

#define IAVF_ITR_INDEX_DEFAULT          0
#define IAVF_QUEUE_ITR_INTERVAL_DEFAULT 32 /* 32 us */
#define IAVF_QUEUE_ITR_INTERVAL_MAX     8160 /* 8160 us */

static inline uint16_t
iavf_calc_itr_interval(int16_t interval)
{
	if (interval < 0 || interval > IAVF_QUEUE_ITR_INTERVAL_MAX)
		interval = IAVF_QUEUE_ITR_INTERVAL_DEFAULT;

	/* Convert to hardware count, as writing each 1 represents 2 us */
	return interval / 2;
}

static int
ice_dcf_config_rx_queues_irqs(struct rte_eth_dev *dev,
				     struct rte_intr_handle *intr_handle)
{
	struct ice_dcf_adapter *adapter = dev->data->dev_private;
	struct ice_dcf_hw *hw = &adapter->real_hw;
	uint16_t interval, i;
	int vec;

	if (rte_intr_cap_multiple(intr_handle) &&
	    dev->data->dev_conf.intr_conf.rxq) {
		if (rte_intr_efd_enable(intr_handle, dev->data->nb_rx_queues))
			return -1;
	}

	if (rte_intr_dp_is_en(intr_handle)) {
		if (rte_intr_vec_list_alloc(intr_handle, "intr_vec",
						   dev->data->nb_rx_queues)) {
			PMD_DRV_LOG(ERR, "Failed to allocate %d rx intr_vec",
				    dev->data->nb_rx_queues);
			return -1;
		}
	}

	if (!dev->data->dev_conf.intr_conf.rxq ||
	    !rte_intr_dp_is_en(intr_handle)) {
		/* Rx interrupt disabled, Map interrupt only for writeback */
		hw->nb_msix = 1;
		if (hw->vf_res->vf_cap_flags &
		    VIRTCHNL_VF_OFFLOAD_WB_ON_ITR) {
			/* If WB_ON_ITR supports, enable it */
			hw->msix_base = IAVF_RX_VEC_START;
			/* Set the ITR for index zero, to 2us to make sure that
			 * we leave time for aggregation to occur, but don't
			 * increase latency dramatically.
			 */
			IAVF_WRITE_REG(&hw->avf,
				       IAVF_VFINT_DYN_CTLN1(hw->msix_base - 1),
				       (0 << IAVF_VFINT_DYN_CTLN1_ITR_INDX_SHIFT) |
				       IAVF_VFINT_DYN_CTLN1_WB_ON_ITR_MASK |
				       (2UL << IAVF_VFINT_DYN_CTLN1_INTERVAL_SHIFT));
		} else {
			/* If no WB_ON_ITR offload flags, need to set
			 * interrupt for descriptor write back.
			 */
			hw->msix_base = IAVF_MISC_VEC_ID;

			/* set ITR to max */
			interval =
			iavf_calc_itr_interval(IAVF_QUEUE_ITR_INTERVAL_MAX);
			IAVF_WRITE_REG(&hw->avf, IAVF_VFINT_DYN_CTL01,
				       IAVF_VFINT_DYN_CTL01_INTENA_MASK |
				       (IAVF_ITR_INDEX_DEFAULT <<
					IAVF_VFINT_DYN_CTL01_ITR_INDX_SHIFT) |
				       (interval <<
					IAVF_VFINT_DYN_CTL01_INTERVAL_SHIFT));
		}
		IAVF_WRITE_FLUSH(&hw->avf);
		/* map all queues to the same interrupt */
		for (i = 0; i < dev->data->nb_rx_queues; i++)
			hw->rxq_map[hw->msix_base] |= 1 << i;
	} else {
		if (!rte_intr_allow_others(intr_handle)) {
			hw->nb_msix = 1;
			hw->msix_base = IAVF_MISC_VEC_ID;
			for (i = 0; i < dev->data->nb_rx_queues; i++) {
				hw->rxq_map[hw->msix_base] |= 1 << i;
				rte_intr_vec_list_index_set(intr_handle,
							i, IAVF_MISC_VEC_ID);
			}
			PMD_DRV_LOG(DEBUG,
				    "vector %u are mapping to all Rx queues",
				    hw->msix_base);
		} else {
			/* If Rx interrupt is required, and we can use
			 * multi interrupts, then the vec is from 1
			 */
			hw->nb_msix = RTE_MIN(hw->vf_res->max_vectors,
				      rte_intr_nb_efd_get(intr_handle));
			hw->msix_base = IAVF_MISC_VEC_ID;
			vec = IAVF_MISC_VEC_ID;
			for (i = 0; i < dev->data->nb_rx_queues; i++) {
				hw->rxq_map[vec] |= 1 << i;
				rte_intr_vec_list_index_set(intr_handle,
								   i, vec++);
				if (vec >= hw->nb_msix)
					vec = IAVF_RX_VEC_START;
			}
			PMD_DRV_LOG(DEBUG,
				    "%u vectors are mapping to %u Rx queues",
				    hw->nb_msix, dev->data->nb_rx_queues);
		}
	}

	if (ice_dcf_config_irq_map(hw)) {
		PMD_DRV_LOG(ERR, "config interrupt mapping failed");
		return -1;
	}
	return 0;
}

static int
alloc_rxq_mbufs(struct ice_rx_queue *rxq)
{
	volatile union ice_rx_flex_desc *rxd;
	struct rte_mbuf *mbuf = NULL;
	uint64_t dma_addr;
	uint16_t i;

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		mbuf = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(!mbuf)) {
			PMD_DRV_LOG(ERR, "Failed to allocate mbuf for RX");
			return -ENOMEM;
		}

		rte_mbuf_refcnt_set(mbuf, 1);
		mbuf->next = NULL;
		mbuf->data_off = RTE_PKTMBUF_HEADROOM;
		mbuf->nb_segs = 1;
		mbuf->port = rxq->port_id;

		dma_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));

		rxd = &rxq->rx_ring[i];
		rxd->read.pkt_addr = dma_addr;
		rxd->read.hdr_addr = 0;
#ifndef RTE_LIBRTE_ICE_16BYTE_RX_DESC
		rxd->read.rsvd1 = 0;
		rxd->read.rsvd2 = 0;
#endif

		rxq->sw_ring[i].mbuf = (void *)mbuf;
	}

	return 0;
}

static int
ice_dcf_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct ice_dcf_adapter *ad = dev->data->dev_private;
	struct iavf_hw *hw = &ad->real_hw.avf;
	struct ice_rx_queue *rxq;
	int err = 0;

	if (rx_queue_id >= dev->data->nb_rx_queues)
		return -EINVAL;

	rxq = dev->data->rx_queues[rx_queue_id];

	err = alloc_rxq_mbufs(rxq);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to allocate RX queue mbuf");
		return err;
	}

	rte_wmb();

	/* Init the RX tail register. */
	IAVF_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);
	IAVF_WRITE_FLUSH(hw);

	/* Ready to switch the queue on */
	err = ice_dcf_switch_queue(&ad->real_hw, rx_queue_id, true, true);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to switch RX queue %u on",
			    rx_queue_id);
		return err;
	}

	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

static inline void
reset_rx_queue(struct ice_rx_queue *rxq)
{
	uint16_t len;
	uint32_t i;

	if (!rxq)
		return;

	len = rxq->nb_rx_desc + ICE_RX_MAX_BURST;

	for (i = 0; i < len * sizeof(union ice_rx_flex_desc); i++)
		((volatile char *)rxq->rx_ring)[i] = 0;

	memset(&rxq->fake_mbuf, 0x0, sizeof(rxq->fake_mbuf));

	for (i = 0; i < ICE_RX_MAX_BURST; i++)
		rxq->sw_ring[rxq->nb_rx_desc + i].mbuf = &rxq->fake_mbuf;

	/* for rx bulk */
	rxq->rx_nb_avail = 0;
	rxq->rx_next_avail = 0;
	rxq->rx_free_trigger = (uint16_t)(rxq->rx_free_thresh - 1);

	rxq->rx_tail = 0;
	rxq->nb_rx_hold = 0;
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;
}

static inline void
reset_tx_queue(struct ice_tx_queue *txq)
{
	struct ice_tx_entry *txe;
	uint32_t i, size;
	uint16_t prev;

	if (!txq) {
		PMD_DRV_LOG(DEBUG, "Pointer to txq is NULL");
		return;
	}

	txe = txq->sw_ring;
	size = sizeof(struct ice_tx_desc) * txq->nb_tx_desc;
	for (i = 0; i < size; i++)
		((volatile char *)txq->tx_ring)[i] = 0;

	prev = (uint16_t)(txq->nb_tx_desc - 1);
	for (i = 0; i < txq->nb_tx_desc; i++) {
		txq->tx_ring[i].cmd_type_offset_bsz =
			rte_cpu_to_le_64(IAVF_TX_DESC_DTYPE_DESC_DONE);
		txe[i].mbuf =  NULL;
		txe[i].last_id = i;
		txe[prev].next_id = i;
		prev = i;
	}

	txq->tx_tail = 0;
	txq->nb_tx_used = 0;

	txq->last_desc_cleaned = txq->nb_tx_desc - 1;
	txq->nb_tx_free = txq->nb_tx_desc - 1;

	txq->tx_next_dd = txq->tx_rs_thresh - 1;
	txq->tx_next_rs = txq->tx_rs_thresh - 1;
}

static int
ice_dcf_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct ice_dcf_adapter *ad = dev->data->dev_private;
	struct ice_dcf_hw *hw = &ad->real_hw;
	struct ice_rx_queue *rxq;
	int err;

	if (rx_queue_id >= dev->data->nb_rx_queues)
		return -EINVAL;

	err = ice_dcf_switch_queue(hw, rx_queue_id, true, false);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to switch RX queue %u off",
			    rx_queue_id);
		return err;
	}

	rxq = dev->data->rx_queues[rx_queue_id];
	rxq->rx_rel_mbufs(rxq);
	reset_rx_queue(rxq);
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

static int
ice_dcf_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct ice_dcf_adapter *ad = dev->data->dev_private;
	struct iavf_hw *hw = &ad->real_hw.avf;
	struct ice_tx_queue *txq;
	int err = 0;

	if (tx_queue_id >= dev->data->nb_tx_queues)
		return -EINVAL;

	txq = dev->data->tx_queues[tx_queue_id];

	/* Init the RX tail register. */
	txq->qtx_tail = hw->hw_addr + IAVF_QTX_TAIL1(tx_queue_id);
	IAVF_PCI_REG_WRITE(txq->qtx_tail, 0);
	IAVF_WRITE_FLUSH(hw);

	/* Ready to switch the queue on */
	err = ice_dcf_switch_queue(&ad->real_hw, tx_queue_id, false, true);

	if (err) {
		PMD_DRV_LOG(ERR, "Failed to switch TX queue %u on",
			    tx_queue_id);
		return err;
	}

	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

static int
ice_dcf_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct ice_dcf_adapter *ad = dev->data->dev_private;
	struct ice_dcf_hw *hw = &ad->real_hw;
	struct ice_tx_queue *txq;
	int err;

	if (tx_queue_id >= dev->data->nb_tx_queues)
		return -EINVAL;

	err = ice_dcf_switch_queue(hw, tx_queue_id, false, false);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to switch TX queue %u off",
			    tx_queue_id);
		return err;
	}

	txq = dev->data->tx_queues[tx_queue_id];
	txq->tx_rel_mbufs(txq);
	reset_tx_queue(txq);
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

static int
ice_dcf_start_queues(struct rte_eth_dev *dev)
{
	struct ice_rx_queue *rxq;
	struct ice_tx_queue *txq;
	int nb_rxq = 0;
	int nb_txq, i;

	for (nb_txq = 0; nb_txq < dev->data->nb_tx_queues; nb_txq++) {
		txq = dev->data->tx_queues[nb_txq];
		if (txq->tx_deferred_start)
			continue;
		if (ice_dcf_tx_queue_start(dev, nb_txq) != 0) {
			PMD_DRV_LOG(ERR, "Fail to start queue %u", nb_txq);
			goto tx_err;
		}
	}

	for (nb_rxq = 0; nb_rxq < dev->data->nb_rx_queues; nb_rxq++) {
		rxq = dev->data->rx_queues[nb_rxq];
		if (rxq->rx_deferred_start)
			continue;
		if (ice_dcf_rx_queue_start(dev, nb_rxq) != 0) {
			PMD_DRV_LOG(ERR, "Fail to start queue %u", nb_rxq);
			goto rx_err;
		}
	}

	return 0;

	/* stop the started queues if failed to start all queues */
rx_err:
	for (i = 0; i < nb_rxq; i++)
		ice_dcf_rx_queue_stop(dev, i);
tx_err:
	for (i = 0; i < nb_txq; i++)
		ice_dcf_tx_queue_stop(dev, i);

	return -1;
}

static int
ice_dcf_dev_start(struct rte_eth_dev *dev)
{
	struct ice_dcf_adapter *dcf_ad = dev->data->dev_private;
	struct rte_intr_handle *intr_handle = dev->intr_handle;
	struct ice_adapter *ad = &dcf_ad->parent;
	struct ice_dcf_hw *hw = &dcf_ad->real_hw;
	int ret;

	if (hw->resetting) {
		PMD_DRV_LOG(ERR,
			    "The DCF has been reset by PF, please reinit first");
		return -EIO;
	}

	if (hw->tm_conf.root && !hw->tm_conf.committed) {
		PMD_DRV_LOG(ERR,
			"please call hierarchy_commit() before starting the port");
		return -EIO;
	}

	ad->pf.adapter_stopped = 0;

	hw->num_queue_pairs = RTE_MAX(dev->data->nb_rx_queues,
				      dev->data->nb_tx_queues);

	ret = ice_dcf_init_rx_queues(dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "Fail to init queues");
		return ret;
	}

	if (hw->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF) {
		ret = ice_dcf_init_rss(hw);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to configure RSS");
			return ret;
		}
	}

	ret = ice_dcf_configure_queues(hw);
	if (ret) {
		PMD_DRV_LOG(ERR, "Fail to config queues");
		return ret;
	}

	ret = ice_dcf_config_rx_queues_irqs(dev, intr_handle);
	if (ret) {
		PMD_DRV_LOG(ERR, "Fail to config rx queues' irqs");
		return ret;
	}

	if (dev->data->dev_conf.intr_conf.rxq != 0) {
		rte_intr_disable(intr_handle);
		rte_intr_enable(intr_handle);
	}

	ret = ice_dcf_start_queues(dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to enable queues");
		return ret;
	}

	ret = ice_dcf_add_del_all_mac_addr(hw, true);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to add mac addr");
		return ret;
	}

	dev->data->dev_link.link_status = RTE_ETH_LINK_UP;

	return 0;
}

static void
ice_dcf_stop_queues(struct rte_eth_dev *dev)
{
	struct ice_dcf_adapter *ad = dev->data->dev_private;
	struct ice_dcf_hw *hw = &ad->real_hw;
	struct ice_rx_queue *rxq;
	struct ice_tx_queue *txq;
	int ret, i;

	/* Stop All queues */
	ret = ice_dcf_disable_queues(hw);
	if (ret)
		PMD_DRV_LOG(WARNING, "Fail to stop queues");

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (!txq)
			continue;
		txq->tx_rel_mbufs(txq);
		reset_tx_queue(txq);
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	}
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (!rxq)
			continue;
		rxq->rx_rel_mbufs(rxq);
		reset_rx_queue(rxq);
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	}
}

static int
ice_dcf_dev_stop(struct rte_eth_dev *dev)
{
	struct ice_dcf_adapter *dcf_ad = dev->data->dev_private;
	struct rte_intr_handle *intr_handle = dev->intr_handle;
	struct ice_adapter *ad = &dcf_ad->parent;
	struct ice_dcf_hw *hw = &dcf_ad->real_hw;

	if (ad->pf.adapter_stopped == 1) {
		PMD_DRV_LOG(DEBUG, "Port is already stopped");
		return 0;
	}

	/* Stop the VF representors for this device */
	ice_dcf_vf_repr_stop_all(dcf_ad);

	ice_dcf_stop_queues(dev);

	rte_intr_efd_disable(intr_handle);
	rte_intr_vec_list_free(intr_handle);

	ice_dcf_add_del_all_mac_addr(&dcf_ad->real_hw, false);
	dev->data->dev_link.link_status = RTE_ETH_LINK_DOWN;
	ad->pf.adapter_stopped = 1;
	hw->tm_conf.committed = false;

	return 0;
}

static int
ice_dcf_dev_configure(struct rte_eth_dev *dev)
{
	struct ice_dcf_adapter *dcf_ad = dev->data->dev_private;
	struct ice_adapter *ad = &dcf_ad->parent;

	ad->rx_bulk_alloc_allowed = true;
	ad->tx_simple_allowed = true;

	if (dev->data->dev_conf.rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG)
		dev->data->dev_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;

	return 0;
}

static int
ice_dcf_dev_info_get(struct rte_eth_dev *dev,
		     struct rte_eth_dev_info *dev_info)
{
	struct ice_dcf_adapter *adapter = dev->data->dev_private;
	struct ice_dcf_hw *hw = &adapter->real_hw;

	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_queues = hw->vsi_res->num_queue_pairs;
	dev_info->max_tx_queues = hw->vsi_res->num_queue_pairs;
	dev_info->min_rx_bufsize = ICE_BUF_SIZE_MIN;
	dev_info->max_rx_pktlen = ICE_FRAME_SIZE_MAX;
	dev_info->hash_key_size = hw->vf_res->rss_key_size;
	dev_info->reta_size = hw->vf_res->rss_lut_size;
	dev_info->flow_type_rss_offloads = ICE_RSS_OFFLOAD_ALL;
	dev_info->dev_capa &= ~RTE_ETH_DEV_CAPA_FLOW_RULE_KEEP;

	dev_info->rx_offload_capa =
		RTE_ETH_RX_OFFLOAD_VLAN_STRIP |
		RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
		RTE_ETH_RX_OFFLOAD_TCP_CKSUM |
		RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM |
		RTE_ETH_RX_OFFLOAD_SCATTER |
		RTE_ETH_RX_OFFLOAD_VLAN_FILTER |
		RTE_ETH_RX_OFFLOAD_RSS_HASH;
	dev_info->tx_offload_capa =
		RTE_ETH_TX_OFFLOAD_VLAN_INSERT |
		RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
		RTE_ETH_TX_OFFLOAD_TCP_CKSUM |
		RTE_ETH_TX_OFFLOAD_SCTP_CKSUM |
		RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM |
		RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM |
		RTE_ETH_TX_OFFLOAD_TCP_TSO |
		RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO |
		RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO |
		RTE_ETH_TX_OFFLOAD_IPIP_TNL_TSO |
		RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO |
		RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = ICE_DEFAULT_RX_PTHRESH,
			.hthresh = ICE_DEFAULT_RX_HTHRESH,
			.wthresh = ICE_DEFAULT_RX_WTHRESH,
		},
		.rx_free_thresh = ICE_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = ICE_DEFAULT_TX_PTHRESH,
			.hthresh = ICE_DEFAULT_TX_HTHRESH,
			.wthresh = ICE_DEFAULT_TX_WTHRESH,
		},
		.tx_free_thresh = ICE_DEFAULT_TX_FREE_THRESH,
		.tx_rs_thresh = ICE_DEFAULT_TX_RSBIT_THRESH,
		.offloads = 0,
	};

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = ICE_MAX_RING_DESC,
		.nb_min = ICE_MIN_RING_DESC,
		.nb_align = ICE_ALIGN_RING_DESC,
	};

	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = ICE_MAX_RING_DESC,
		.nb_min = ICE_MIN_RING_DESC,
		.nb_align = ICE_ALIGN_RING_DESC,
	};

	return 0;
}

static int
ice_dcf_dev_promiscuous_enable(__rte_unused struct rte_eth_dev *dev)
{
	return 0;
}

static int
ice_dcf_dev_promiscuous_disable(__rte_unused struct rte_eth_dev *dev)
{
	return 0;
}

static int
ice_dcf_dev_allmulticast_enable(__rte_unused struct rte_eth_dev *dev)
{
	return 0;
}

static int
ice_dcf_dev_allmulticast_disable(__rte_unused struct rte_eth_dev *dev)
{
	return 0;
}

static int
ice_dcf_dev_flow_ops_get(struct rte_eth_dev *dev,
			 const struct rte_flow_ops **ops)
{
	if (!dev)
		return -EINVAL;

	*ops = &ice_flow_ops;
	return 0;
}

#define ICE_DCF_32_BIT_WIDTH (CHAR_BIT * 4)
#define ICE_DCF_48_BIT_WIDTH (CHAR_BIT * 6)
#define ICE_DCF_48_BIT_MASK  RTE_LEN2MASK(ICE_DCF_48_BIT_WIDTH, uint64_t)

static void
ice_dcf_stat_update_48(uint64_t *offset, uint64_t *stat)
{
	if (*stat >= *offset)
		*stat = *stat - *offset;
	else
		*stat = (uint64_t)((*stat +
			((uint64_t)1 << ICE_DCF_48_BIT_WIDTH)) - *offset);

	*stat &= ICE_DCF_48_BIT_MASK;
}

static void
ice_dcf_stat_update_32(uint64_t *offset, uint64_t *stat)
{
	if (*stat >= *offset)
		*stat = (uint64_t)(*stat - *offset);
	else
		*stat = (uint64_t)((*stat +
			((uint64_t)1 << ICE_DCF_32_BIT_WIDTH)) - *offset);
}

static void
ice_dcf_update_stats(struct virtchnl_eth_stats *oes,
		     struct virtchnl_eth_stats *nes)
{
	ice_dcf_stat_update_48(&oes->rx_bytes, &nes->rx_bytes);
	ice_dcf_stat_update_48(&oes->rx_unicast, &nes->rx_unicast);
	ice_dcf_stat_update_48(&oes->rx_multicast, &nes->rx_multicast);
	ice_dcf_stat_update_48(&oes->rx_broadcast, &nes->rx_broadcast);
	ice_dcf_stat_update_32(&oes->rx_discards, &nes->rx_discards);
	ice_dcf_stat_update_48(&oes->tx_bytes, &nes->tx_bytes);
	ice_dcf_stat_update_48(&oes->tx_unicast, &nes->tx_unicast);
	ice_dcf_stat_update_48(&oes->tx_multicast, &nes->tx_multicast);
	ice_dcf_stat_update_48(&oes->tx_broadcast, &nes->tx_broadcast);
	ice_dcf_stat_update_32(&oes->tx_errors, &nes->tx_errors);
	ice_dcf_stat_update_32(&oes->tx_discards, &nes->tx_discards);
}


static int
ice_dcf_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct ice_dcf_adapter *ad = dev->data->dev_private;
	struct ice_dcf_hw *hw = &ad->real_hw;
	struct virtchnl_eth_stats pstats;
	int ret;

	if (hw->resetting) {
		PMD_DRV_LOG(ERR,
			    "The DCF has been reset by PF, please reinit first");
		return -EIO;
	}

	ret = ice_dcf_query_stats(hw, &pstats);
	if (ret == 0) {
		ice_dcf_update_stats(&hw->eth_stats_offset, &pstats);
		stats->ipackets = pstats.rx_unicast + pstats.rx_multicast +
				pstats.rx_broadcast - pstats.rx_discards;
		stats->opackets = pstats.tx_broadcast + pstats.tx_multicast +
						pstats.tx_unicast;
		stats->imissed = pstats.rx_discards;
		stats->oerrors = pstats.tx_errors + pstats.tx_discards;
		stats->ibytes = pstats.rx_bytes;
		stats->ibytes -= stats->ipackets * RTE_ETHER_CRC_LEN;
		stats->obytes = pstats.tx_bytes;
	} else {
		PMD_DRV_LOG(ERR, "Get statistics failed");
	}
	return ret;
}

static int
ice_dcf_stats_reset(struct rte_eth_dev *dev)
{
	struct ice_dcf_adapter *ad = dev->data->dev_private;
	struct ice_dcf_hw *hw = &ad->real_hw;
	struct virtchnl_eth_stats pstats;
	int ret;

	if (hw->resetting)
		return 0;

	/* read stat values to clear hardware registers */
	ret = ice_dcf_query_stats(hw, &pstats);
	if (ret != 0)
		return ret;

	/* set stats offset base on current values */
	hw->eth_stats_offset = pstats;

	return 0;
}

static void
ice_dcf_free_repr_info(struct ice_dcf_adapter *dcf_adapter)
{
	if (dcf_adapter->repr_infos) {
		rte_free(dcf_adapter->repr_infos);
		dcf_adapter->repr_infos = NULL;
	}
}

static int
ice_dcf_init_repr_info(struct ice_dcf_adapter *dcf_adapter)
{
	dcf_adapter->repr_infos =
			rte_calloc("ice_dcf_rep_info",
				   dcf_adapter->real_hw.num_vfs,
				   sizeof(dcf_adapter->repr_infos[0]), 0);
	if (!dcf_adapter->repr_infos) {
		PMD_DRV_LOG(ERR, "Failed to alloc memory for VF representors\n");
		return -ENOMEM;
	}

	return 0;
}

static int
ice_dcf_dev_close(struct rte_eth_dev *dev)
{
	struct ice_dcf_adapter *adapter = dev->data->dev_private;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	(void)ice_dcf_dev_stop(dev);

	ice_free_queues(dev);

	ice_dcf_free_repr_info(adapter);
	ice_dcf_uninit_parent_adapter(dev);
	ice_dcf_uninit_hw(dev, &adapter->real_hw);

	return 0;
}

int
ice_dcf_link_update(struct rte_eth_dev *dev,
		    __rte_unused int wait_to_complete)
{
	struct ice_dcf_adapter *ad = dev->data->dev_private;
	struct ice_dcf_hw *hw = &ad->real_hw;
	struct rte_eth_link new_link;

	memset(&new_link, 0, sizeof(new_link));

	/* Only read status info stored in VF, and the info is updated
	 * when receive LINK_CHANGE event from PF by virtchnl.
	 */
	switch (hw->link_speed) {
	case 10:
		new_link.link_speed = RTE_ETH_SPEED_NUM_10M;
		break;
	case 100:
		new_link.link_speed = RTE_ETH_SPEED_NUM_100M;
		break;
	case 1000:
		new_link.link_speed = RTE_ETH_SPEED_NUM_1G;
		break;
	case 10000:
		new_link.link_speed = RTE_ETH_SPEED_NUM_10G;
		break;
	case 20000:
		new_link.link_speed = RTE_ETH_SPEED_NUM_20G;
		break;
	case 25000:
		new_link.link_speed = RTE_ETH_SPEED_NUM_25G;
		break;
	case 40000:
		new_link.link_speed = RTE_ETH_SPEED_NUM_40G;
		break;
	case 50000:
		new_link.link_speed = RTE_ETH_SPEED_NUM_50G;
		break;
	case 100000:
		new_link.link_speed = RTE_ETH_SPEED_NUM_100G;
		break;
	default:
		new_link.link_speed = RTE_ETH_SPEED_NUM_NONE;
		break;
	}

	new_link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	new_link.link_status = hw->link_up ? RTE_ETH_LINK_UP :
					     RTE_ETH_LINK_DOWN;
	new_link.link_autoneg = !(dev->data->dev_conf.link_speeds &
				RTE_ETH_LINK_SPEED_FIXED);

	return rte_eth_linkstatus_set(dev, &new_link);
}

bool
ice_dcf_adminq_need_retry(struct ice_adapter *ad)
{
	return ad->hw.dcf_enabled &&
	       !__atomic_load_n(&ad->dcf_state_on, __ATOMIC_RELAXED);
}

/* Add UDP tunneling port */
static int
ice_dcf_dev_udp_tunnel_port_add(struct rte_eth_dev *dev,
				struct rte_eth_udp_tunnel *udp_tunnel)
{
	struct ice_dcf_adapter *adapter = dev->data->dev_private;
	struct ice_adapter *parent_adapter = &adapter->parent;
	struct ice_hw *parent_hw = &parent_adapter->hw;
	int ret = 0;

	if (!udp_tunnel)
		return -EINVAL;

	switch (udp_tunnel->prot_type) {
	case RTE_ETH_TUNNEL_TYPE_VXLAN:
		ret = ice_create_tunnel(parent_hw, TNL_VXLAN,
					udp_tunnel->udp_port);
		break;
	case RTE_ETH_TUNNEL_TYPE_ECPRI:
		ret = ice_create_tunnel(parent_hw, TNL_ECPRI,
					udp_tunnel->udp_port);
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid tunnel type");
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* Delete UDP tunneling port */
static int
ice_dcf_dev_udp_tunnel_port_del(struct rte_eth_dev *dev,
				struct rte_eth_udp_tunnel *udp_tunnel)
{
	struct ice_dcf_adapter *adapter = dev->data->dev_private;
	struct ice_adapter *parent_adapter = &adapter->parent;
	struct ice_hw *parent_hw = &parent_adapter->hw;
	int ret = 0;

	if (!udp_tunnel)
		return -EINVAL;

	switch (udp_tunnel->prot_type) {
	case RTE_ETH_TUNNEL_TYPE_VXLAN:
	case RTE_ETH_TUNNEL_TYPE_ECPRI:
		ret = ice_destroy_tunnel(parent_hw, udp_tunnel->udp_port, 0);
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid tunnel type");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int
ice_dcf_tm_ops_get(struct rte_eth_dev *dev __rte_unused,
		void *arg)
{
	if (!arg)
		return -EINVAL;

	*(const void **)arg = &ice_dcf_tm_ops;

	return 0;
}

static inline void
ice_dcf_reset_hw(struct rte_eth_dev *eth_dev, struct ice_dcf_hw *hw)
{
	ice_dcf_uninit_hw(eth_dev, hw);
	ice_dcf_init_hw(eth_dev, hw);
}

/* Check if reset has been triggered by PF */
static inline bool
ice_dcf_is_reset(struct rte_eth_dev *dev)
{
	struct ice_dcf_adapter *ad = dev->data->dev_private;
	struct iavf_hw *hw = &ad->real_hw.avf;

	return !(IAVF_READ_REG(hw, IAVF_VF_ARQLEN1) &
		 IAVF_VF_ARQLEN1_ARQENABLE_MASK);
}

static int
ice_dcf_dev_reset(struct rte_eth_dev *dev)
{
	struct ice_dcf_adapter *ad = dev->data->dev_private;
	struct ice_dcf_hw *hw = &ad->real_hw;
	int ret;

	if (ice_dcf_is_reset(dev)) {
		if (!ad->real_hw.resetting)
			ad->real_hw.resetting = true;
		PMD_DRV_LOG(ERR, "The DCF has been reset by PF");

		/*
		 * Simply reset hw to trigger an additional DCF enable/disable
		 * cycle which help to workaround the issue that kernel driver
		 * may not clean up resource during previous reset.
		 */
		ice_dcf_reset_hw(dev, hw);
	}

	ret = ice_dcf_dev_uninit(dev);
	if (ret)
		return ret;

	ret = ice_dcf_dev_init(dev);

	return ret;
}

static const struct eth_dev_ops ice_dcf_eth_dev_ops = {
	.dev_start               = ice_dcf_dev_start,
	.dev_stop                = ice_dcf_dev_stop,
	.dev_close               = ice_dcf_dev_close,
	.dev_reset               = ice_dcf_dev_reset,
	.dev_configure           = ice_dcf_dev_configure,
	.dev_infos_get           = ice_dcf_dev_info_get,
	.rx_queue_setup          = ice_rx_queue_setup,
	.tx_queue_setup          = ice_tx_queue_setup,
	.rx_queue_release        = ice_dev_rx_queue_release,
	.tx_queue_release        = ice_dev_tx_queue_release,
	.rx_queue_start          = ice_dcf_rx_queue_start,
	.tx_queue_start          = ice_dcf_tx_queue_start,
	.rx_queue_stop           = ice_dcf_rx_queue_stop,
	.tx_queue_stop           = ice_dcf_tx_queue_stop,
	.link_update             = ice_dcf_link_update,
	.stats_get               = ice_dcf_stats_get,
	.stats_reset             = ice_dcf_stats_reset,
	.promiscuous_enable      = ice_dcf_dev_promiscuous_enable,
	.promiscuous_disable     = ice_dcf_dev_promiscuous_disable,
	.allmulticast_enable     = ice_dcf_dev_allmulticast_enable,
	.allmulticast_disable    = ice_dcf_dev_allmulticast_disable,
	.flow_ops_get            = ice_dcf_dev_flow_ops_get,
	.udp_tunnel_port_add	 = ice_dcf_dev_udp_tunnel_port_add,
	.udp_tunnel_port_del	 = ice_dcf_dev_udp_tunnel_port_del,
	.tm_ops_get              = ice_dcf_tm_ops_get,
};

static int
ice_dcf_dev_init(struct rte_eth_dev *eth_dev)
{
	struct ice_dcf_adapter *adapter = eth_dev->data->dev_private;
	struct ice_adapter *parent_adapter = &adapter->parent;

	eth_dev->dev_ops = &ice_dcf_eth_dev_ops;
	eth_dev->rx_pkt_burst = ice_dcf_recv_pkts;
	eth_dev->tx_pkt_burst = ice_dcf_xmit_pkts;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	adapter->real_hw.vc_event_msg_cb = ice_dcf_handle_pf_event_msg;
	if (ice_dcf_init_hw(eth_dev, &adapter->real_hw) != 0) {
		PMD_INIT_LOG(ERR, "Failed to init DCF hardware");
		__atomic_store_n(&parent_adapter->dcf_state_on, false,
				 __ATOMIC_RELAXED);
		return -1;
	}

	__atomic_store_n(&parent_adapter->dcf_state_on, true, __ATOMIC_RELAXED);

	if (ice_dcf_init_parent_adapter(eth_dev) != 0) {
		PMD_INIT_LOG(ERR, "Failed to init DCF parent adapter");
		ice_dcf_uninit_hw(eth_dev, &adapter->real_hw);
		return -1;
	}

	return 0;
}

static int
ice_dcf_dev_uninit(struct rte_eth_dev *eth_dev)
{
	ice_dcf_dev_close(eth_dev);

	return 0;
}

static int
ice_dcf_cap_check_handler(__rte_unused const char *key,
			  const char *value, __rte_unused void *opaque)
{
	if (strcmp(value, "dcf"))
		return -1;

	return 0;
}

static int
ice_dcf_cap_selected(struct rte_devargs *devargs)
{
	struct rte_kvargs *kvlist;
	const char *key = "cap";
	int ret = 0;

	if (devargs == NULL)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		return 0;

	if (!rte_kvargs_count(kvlist, key))
		goto exit;

	/* dcf capability selected when there's a key-value pair: cap=dcf */
	if (rte_kvargs_process(kvlist, key,
			       ice_dcf_cap_check_handler, NULL) < 0)
		goto exit;

	ret = 1;

exit:
	rte_kvargs_free(kvlist);
	return ret;
}

static int
eth_ice_dcf_pci_probe(__rte_unused struct rte_pci_driver *pci_drv,
		      struct rte_pci_device *pci_dev)
{
	struct rte_eth_devargs eth_da = { .nb_representor_ports = 0 };
	struct ice_dcf_vf_repr_param repr_param;
	char repr_name[RTE_ETH_NAME_MAX_LEN];
	struct ice_dcf_adapter *dcf_adapter;
	struct rte_eth_dev *dcf_ethdev;
	uint16_t dcf_vsi_id;
	int i, ret;

	if (!ice_dcf_cap_selected(pci_dev->device.devargs))
		return 1;

	ret = rte_eth_devargs_parse(pci_dev->device.devargs->args, &eth_da);
	if (ret)
		return ret;

	ret = rte_eth_dev_pci_generic_probe(pci_dev,
					    sizeof(struct ice_dcf_adapter),
					    ice_dcf_dev_init);
	if (ret || !eth_da.nb_representor_ports)
		return ret;
	if (eth_da.type != RTE_ETH_REPRESENTOR_VF)
		return -ENOTSUP;

	dcf_ethdev = rte_eth_dev_allocated(pci_dev->device.name);
	if (dcf_ethdev == NULL)
		return -ENODEV;

	dcf_adapter = dcf_ethdev->data->dev_private;
	ret = ice_dcf_init_repr_info(dcf_adapter);
	if (ret)
		return ret;

	if (eth_da.nb_representor_ports > dcf_adapter->real_hw.num_vfs ||
	    eth_da.nb_representor_ports >= RTE_MAX_ETHPORTS) {
		PMD_DRV_LOG(ERR, "the number of port representors is too large: %u",
			    eth_da.nb_representor_ports);
		ice_dcf_free_repr_info(dcf_adapter);
		return -EINVAL;
	}

	dcf_vsi_id = dcf_adapter->real_hw.vsi_id | VIRTCHNL_DCF_VF_VSI_VALID;

	repr_param.dcf_eth_dev = dcf_ethdev;
	repr_param.switch_domain_id = 0;

	for (i = 0; i < eth_da.nb_representor_ports; i++) {
		uint16_t vf_id = eth_da.representor_ports[i];
		struct rte_eth_dev *vf_rep_eth_dev;

		if (vf_id >= dcf_adapter->real_hw.num_vfs) {
			PMD_DRV_LOG(ERR, "VF ID %u is out of range (0 ~ %u)",
				    vf_id, dcf_adapter->real_hw.num_vfs - 1);
			ret = -EINVAL;
			break;
		}

		if (dcf_adapter->real_hw.vf_vsi_map[vf_id] == dcf_vsi_id) {
			PMD_DRV_LOG(ERR, "VF ID %u is DCF's ID.\n", vf_id);
			ret = -EINVAL;
			break;
		}

		repr_param.vf_id = vf_id;
		snprintf(repr_name, sizeof(repr_name), "net_%s_representor_%u",
			 pci_dev->device.name, vf_id);
		ret = rte_eth_dev_create(&pci_dev->device, repr_name,
					 sizeof(struct ice_dcf_vf_repr),
					 NULL, NULL, ice_dcf_vf_repr_init,
					 &repr_param);
		if (ret) {
			PMD_DRV_LOG(ERR, "failed to create DCF VF representor %s",
				    repr_name);
			break;
		}

		vf_rep_eth_dev = rte_eth_dev_allocated(repr_name);
		if (!vf_rep_eth_dev) {
			PMD_DRV_LOG(ERR,
				    "Failed to find the ethdev for DCF VF representor: %s",
				    repr_name);
			ret = -ENODEV;
			break;
		}

		dcf_adapter->repr_infos[vf_id].vf_rep_eth_dev = vf_rep_eth_dev;
		dcf_adapter->num_reprs++;
	}

	return ret;
}

static int
eth_ice_dcf_pci_remove(struct rte_pci_device *pci_dev)
{
	struct rte_eth_dev *eth_dev;

	eth_dev = rte_eth_dev_allocated(pci_dev->device.name);
	if (!eth_dev)
		return 0;

	if (eth_dev->data->dev_flags & RTE_ETH_DEV_REPRESENTOR)
		return rte_eth_dev_pci_generic_remove(pci_dev,
						      ice_dcf_vf_repr_uninit);
	else
		return rte_eth_dev_pci_generic_remove(pci_dev,
						      ice_dcf_dev_uninit);
}

static const struct rte_pci_id pci_id_ice_dcf_map[] = {
	{ RTE_PCI_DEVICE(IAVF_INTEL_VENDOR_ID, IAVF_DEV_ID_ADAPTIVE_VF) },
	{ .vendor_id = 0, /* sentinel */ },
};

static struct rte_pci_driver rte_ice_dcf_pmd = {
	.id_table = pci_id_ice_dcf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = eth_ice_dcf_pci_probe,
	.remove = eth_ice_dcf_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_ice_dcf, rte_ice_dcf_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_ice_dcf, pci_id_ice_dcf_map);
RTE_PMD_REGISTER_KMOD_DEP(net_ice_dcf, "* igb_uio | vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(net_ice_dcf, "cap=dcf");
