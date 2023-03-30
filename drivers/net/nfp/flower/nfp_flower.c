/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#include <rte_common.h>
#include <rte_service_component.h>
#include <rte_malloc.h>
#include <rte_alarm.h>
#include <ethdev_pci.h>
#include <ethdev_driver.h>

#include "../nfp_common.h"
#include "../nfp_logs.h"
#include "../nfp_ctrl.h"
#include "../nfp_cpp_bridge.h"
#include "../nfp_rxtx.h"
#include "../nfpcore/nfp_mip.h"
#include "../nfpcore/nfp_rtsym.h"
#include "../nfpcore/nfp_nsp.h"
#include "nfp_flower.h"
#include "nfp_flower_ctrl.h"
#include "nfp_flower_representor.h"
#include "nfp_flower_cmsg.h"

#define CTRL_VNIC_NB_DESC 512

static void
nfp_pf_repr_enable_queues(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;
	uint64_t enabled_queues = 0;
	int i;
	struct nfp_flower_representor *repr;

	repr = dev->data->dev_private;
	hw = repr->app_fw_flower->pf_hw;

	/* Enabling the required TX queues in the device */
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		enabled_queues |= (1 << i);

	nn_cfg_writeq(hw, NFP_NET_CFG_TXRS_ENABLE, enabled_queues);

	enabled_queues = 0;

	/* Enabling the required RX queues in the device */
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		enabled_queues |= (1 << i);

	nn_cfg_writeq(hw, NFP_NET_CFG_RXRS_ENABLE, enabled_queues);
}

static void
nfp_pf_repr_disable_queues(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;
	uint32_t new_ctrl;
	uint32_t update = 0;
	struct nfp_flower_representor *repr;

	repr = (struct nfp_flower_representor *)dev->data->dev_private;
	hw = repr->app_fw_flower->pf_hw;

	nn_cfg_writeq(hw, NFP_NET_CFG_TXRS_ENABLE, 0);
	nn_cfg_writeq(hw, NFP_NET_CFG_RXRS_ENABLE, 0);

	new_ctrl = hw->ctrl & ~NFP_NET_CFG_CTRL_ENABLE;
	update = NFP_NET_CFG_UPDATE_GEN | NFP_NET_CFG_UPDATE_RING |
		 NFP_NET_CFG_UPDATE_MSIX;

	if (hw->cap & NFP_NET_CFG_CTRL_RINGCFG)
		new_ctrl &= ~NFP_NET_CFG_CTRL_RINGCFG;

	/* If an error when reconfig we avoid to change hw state */
	if (nfp_net_reconfig(hw, new_ctrl, update) < 0)
		return;

	hw->ctrl = new_ctrl;
}

int
nfp_flower_pf_start(struct rte_eth_dev *dev)
{
	int ret;
	uint32_t new_ctrl;
	uint32_t update = 0;
	struct nfp_net_hw *hw;
	struct nfp_flower_representor *repr;

	repr = (struct nfp_flower_representor *)dev->data->dev_private;
	hw = repr->app_fw_flower->pf_hw;

	/* Disabling queues just in case... */
	nfp_pf_repr_disable_queues(dev);

	/* Enabling the required queues in the device */
	nfp_pf_repr_enable_queues(dev);

	new_ctrl = nfp_check_offloads(dev);

	/* Writing configuration parameters in the device */
	nfp_net_params_setup(hw);

	update |= NFP_NET_CFG_UPDATE_RSS;

	if (hw->cap & NFP_NET_CFG_CTRL_RSS2)
		new_ctrl |= NFP_NET_CFG_CTRL_RSS2;
	else
		new_ctrl |= NFP_NET_CFG_CTRL_RSS;

	/* Enable device */
	new_ctrl |= NFP_NET_CFG_CTRL_ENABLE;

	update |= NFP_NET_CFG_UPDATE_GEN | NFP_NET_CFG_UPDATE_RING;

	if (hw->cap & NFP_NET_CFG_CTRL_RINGCFG)
		new_ctrl |= NFP_NET_CFG_CTRL_RINGCFG;

	nn_cfg_writel(hw, NFP_NET_CFG_CTRL, new_ctrl);

	/* If an error when reconfig we avoid to change hw state */
	ret = nfp_net_reconfig(hw, new_ctrl, update);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to reconfig PF vnic");
		return -EIO;
	}

	hw->ctrl = new_ctrl;

	/* Setup the freelist ring */
	ret = nfp_net_rx_freelist_setup(dev);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Error with flower PF vNIC freelist setup");
		return -EIO;
	}

	return 0;
}

/* Stop device: disable rx and tx functions to allow for reconfiguring. */
int
nfp_flower_pf_stop(struct rte_eth_dev *dev)
{
	uint16_t i;
	struct nfp_net_hw *hw;
	struct nfp_net_txq *this_tx_q;
	struct nfp_net_rxq *this_rx_q;
	struct nfp_flower_representor *repr;

	repr = (struct nfp_flower_representor *)dev->data->dev_private;
	hw = repr->app_fw_flower->pf_hw;

	nfp_pf_repr_disable_queues(dev);

	/* Clear queues */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		this_tx_q = (struct nfp_net_txq *)dev->data->tx_queues[i];
		nfp_net_reset_tx_queue(this_tx_q);
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		this_rx_q = (struct nfp_net_rxq *)dev->data->rx_queues[i];
		nfp_net_reset_rx_queue(this_rx_q);
	}

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		/* Configure the physical port down */
		nfp_eth_set_configured(hw->cpp, hw->nfp_idx, 0);
	else
		nfp_eth_set_configured(dev->process_private, hw->nfp_idx, 0);

	return 0;
}

/* Reset and stop device. The device can not be restarted. */
static int
nfp_flower_pf_close(struct rte_eth_dev *dev)
{
	uint16_t i;
	struct nfp_net_hw *hw;
	struct nfp_pf_dev *pf_dev;
	struct nfp_net_txq *this_tx_q;
	struct nfp_net_rxq *this_rx_q;
	struct nfp_flower_representor *repr;
	struct nfp_app_fw_flower *app_fw_flower;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	repr = (struct nfp_flower_representor *)dev->data->dev_private;
	hw = repr->app_fw_flower->pf_hw;
	pf_dev = hw->pf_dev;
	app_fw_flower = NFP_PRIV_TO_APP_FW_FLOWER(pf_dev->app_fw_priv);

	nfp_mtr_priv_uninit(pf_dev);

	/*
	 * We assume that the DPDK application is stopping all the
	 * threads/queues before calling the device close function.
	 */
	nfp_pf_repr_disable_queues(dev);

	/* Clear queues */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		this_tx_q = (struct nfp_net_txq *)dev->data->tx_queues[i];
		nfp_net_reset_tx_queue(this_tx_q);
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		this_rx_q = (struct nfp_net_rxq *)dev->data->rx_queues[i];
		nfp_net_reset_rx_queue(this_rx_q);
	}

	/* Cancel possible impending LSC work here before releasing the port*/
	rte_eal_alarm_cancel(nfp_net_dev_interrupt_delayed_handler, (void *)dev);

	nn_cfg_writeb(hw, NFP_NET_CFG_LSC, 0xff);

	rte_eth_dev_release_port(dev);

	/* Now it is safe to free all PF resources */
	PMD_DRV_LOG(INFO, "Freeing PF resources");
	nfp_cpp_area_free(pf_dev->ctrl_area);
	nfp_cpp_area_free(pf_dev->hwqueues_area);
	free(pf_dev->hwinfo);
	free(pf_dev->sym_tbl);
	nfp_cpp_free(pf_dev->cpp);
	rte_free(app_fw_flower);
	rte_free(pf_dev);

	return 0;
}

static const struct eth_dev_ops nfp_flower_pf_vnic_ops = {
	.dev_infos_get          = nfp_net_infos_get,
	.link_update            = nfp_net_link_update,
	.dev_configure          = nfp_net_configure,

	.dev_start              = nfp_flower_pf_start,
	.dev_stop               = nfp_flower_pf_stop,
	.dev_close              = nfp_flower_pf_close,
};

static inline void
nfp_flower_parse_metadata(struct nfp_net_rxq *rxq,
		struct nfp_net_rx_desc *rxd,
		struct rte_mbuf *mbuf,
		uint32_t *portid)
{
	uint32_t meta_info;
	uint8_t *meta_offset;
	struct nfp_net_hw *hw;

	hw = rxq->hw;
	if (!((hw->ctrl & NFP_NET_CFG_CTRL_RSS) ||
			(hw->ctrl & NFP_NET_CFG_CTRL_RSS2)))
		return;

	meta_offset = rte_pktmbuf_mtod(mbuf, uint8_t *);
	meta_offset -= NFP_DESC_META_LEN(rxd);
	meta_info = rte_be_to_cpu_32(*(uint32_t *)meta_offset);
	meta_offset += 4;

	while (meta_info != 0) {
		switch (meta_info & NFP_NET_META_FIELD_MASK) {
		/* Expect flower firmware to only send packets with META_PORTID */
		case NFP_NET_META_PORTID:
			*portid = rte_be_to_cpu_32(*(uint32_t *)meta_offset);
			meta_offset += 4;
			meta_info >>= NFP_NET_META_FIELD_SIZE;
			break;
		default:
			/* Unsupported metadata can be a performance issue */
			return;
		}
	}
}

static inline struct nfp_flower_representor *
nfp_flower_get_repr(struct nfp_net_hw *hw,
		uint32_t port_id)
{
	uint8_t port;
	struct nfp_app_fw_flower *app_fw_flower;

	/* Obtain handle to app_fw_flower here */
	app_fw_flower = NFP_PRIV_TO_APP_FW_FLOWER(hw->pf_dev->app_fw_priv);

	switch (NFP_FLOWER_CMSG_PORT_TYPE(port_id)) {
	case NFP_FLOWER_CMSG_PORT_TYPE_PHYS_PORT:
		port =  NFP_FLOWER_CMSG_PORT_PHYS_PORT_NUM(port_id);
		return app_fw_flower->phy_reprs[port];
	case NFP_FLOWER_CMSG_PORT_TYPE_PCIE_PORT:
		port = NFP_FLOWER_CMSG_PORT_VNIC(port_id);
		return app_fw_flower->vf_reprs[port];
	default:
		break;
	}

	return NULL;
}

uint16_t
nfp_flower_pf_recv_pkts(void *rx_queue,
		struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	/*
	 * We need different counters for packets given to the caller
	 * and packets sent to representors
	 */
	int avail = 0;
	int avail_multiplexed = 0;
	uint64_t dma_addr;
	uint32_t meta_portid;
	uint16_t nb_hold = 0;
	struct rte_mbuf *mb;
	struct nfp_net_hw *hw;
	struct rte_mbuf *new_mb;
	struct nfp_net_rxq *rxq;
	struct nfp_net_rx_buff *rxb;
	struct nfp_net_rx_desc *rxds;
	struct nfp_flower_representor *repr;

	rxq = rx_queue;
	if (unlikely(rxq == NULL)) {
		/*
		 * DPDK just checks the queue is lower than max queues
		 * enabled. But the queue needs to be configured
		 */
		PMD_RX_LOG(ERR, "RX Bad queue");
		return 0;
	}

	hw = rxq->hw;

	/*
	 * This is tunable as we could allow to receive more packets than
	 * requested if most are multiplexed.
	 */
	while (avail + avail_multiplexed < nb_pkts) {
		rxb = &rxq->rxbufs[rxq->rd_p];
		if (unlikely(rxb == NULL)) {
			PMD_RX_LOG(ERR, "rxb does not exist!");
			break;
		}

		rxds = &rxq->rxds[rxq->rd_p];
		if ((rxds->rxd.meta_len_dd & PCIE_DESC_RX_DD) == 0)
			break;

		/*
		 * Memory barrier to ensure that we won't do other
		 * reads before the DD bit.
		 */
		rte_rmb();

		/*
		 * We got a packet. Let's alloc a new mbuf for refilling the
		 * free descriptor ring as soon as possible
		 */
		new_mb = rte_pktmbuf_alloc(rxq->mem_pool);
		if (unlikely(new_mb == NULL)) {
			PMD_RX_LOG(DEBUG,
			"RX mbuf alloc failed port_id=%u queue_id=%d",
				rxq->port_id, rxq->qidx);
			nfp_net_mbuf_alloc_failed(rxq);
			break;
		}

		/*
		 * Grab the mbuf and refill the descriptor with the
		 * previously allocated mbuf
		 */
		mb = rxb->mbuf;
		rxb->mbuf = new_mb;

		PMD_RX_LOG(DEBUG, "Packet len: %u, mbuf_size: %u",
				rxds->rxd.data_len, rxq->mbuf_size);

		/* Size of this segment */
		mb->data_len = rxds->rxd.data_len - NFP_DESC_META_LEN(rxds);
		/* Size of the whole packet. We just support 1 segment */
		mb->pkt_len = rxds->rxd.data_len - NFP_DESC_META_LEN(rxds);

		if (unlikely((mb->data_len + hw->rx_offset) > rxq->mbuf_size)) {
			/*
			 * This should not happen and the user has the
			 * responsibility of avoiding it. But we have
			 * to give some info about the error
			 */
			PMD_RX_LOG(ERR,
				"mbuf overflow likely due to the RX offset.\n"
				"\t\tYour mbuf size should have extra space for"
				" RX offset=%u bytes.\n"
				"\t\tCurrently you just have %u bytes available"
				" but the received packet is %u bytes long",
				hw->rx_offset,
				rxq->mbuf_size - hw->rx_offset,
				mb->data_len);
			rte_pktmbuf_free(mb);
			break;
		}

		/* Filling the received mbuf with packet info */
		if (hw->rx_offset != 0)
			mb->data_off = RTE_PKTMBUF_HEADROOM + hw->rx_offset;
		else
			mb->data_off = RTE_PKTMBUF_HEADROOM + NFP_DESC_META_LEN(rxds);

		/* No scatter mode supported */
		mb->nb_segs = 1;
		mb->next = NULL;

		mb->port = rxq->port_id;
		meta_portid = 0;

		/* Checking the RSS flag */
		nfp_flower_parse_metadata(rxq, rxds, mb, &meta_portid);
		PMD_RX_LOG(DEBUG, "Received from port %u type %u",
				NFP_FLOWER_CMSG_PORT_VNIC(meta_portid),
				NFP_FLOWER_CMSG_PORT_VNIC_TYPE(meta_portid));

		/* Checking the checksum flag */
		nfp_net_rx_cksum(rxq, rxds, mb);

		if ((rxds->rxd.flags & PCIE_DESC_RX_VLAN) &&
				(hw->ctrl & NFP_NET_CFG_CTRL_RXVLAN)) {
			mb->vlan_tci = rte_cpu_to_le_32(rxds->rxd.vlan);
			mb->ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
		}

		repr = nfp_flower_get_repr(hw, meta_portid);
		if (repr != NULL && repr->ring != NULL) {
			PMD_RX_LOG(DEBUG, "Using representor %s", repr->name);
			rte_ring_enqueue(repr->ring, (void *)mb);
			avail_multiplexed++;
		} else if (repr != NULL) {
			PMD_RX_LOG(ERR, "[%u] No ring available for repr_port %s\n",
					hw->idx, repr->name);
			PMD_RX_LOG(DEBUG, "Adding the mbuf to the mbuf array passed by the app");
			rx_pkts[avail++] = mb;
		} else {
			PMD_RX_LOG(DEBUG, "Adding the mbuf to the mbuf array passed by the app");
			rx_pkts[avail++] = mb;
		}

		/* Now resetting and updating the descriptor */
		rxds->vals[0] = 0;
		rxds->vals[1] = 0;
		dma_addr = rte_cpu_to_le_64(RTE_MBUF_DMA_ADDR_DEFAULT(new_mb));
		rxds->fld.dd = 0;
		rxds->fld.dma_addr_hi = (dma_addr >> 32) & 0xffff;
		rxds->fld.dma_addr_lo = dma_addr & 0xffffffff;
		nb_hold++;

		rxq->rd_p++;
		if (unlikely(rxq->rd_p == rxq->rx_count))
			rxq->rd_p = 0;
	}

	if (nb_hold == 0)
		return nb_hold;

	PMD_RX_LOG(DEBUG, "RX port_id=%u queue_id=%d, %d packets received",
			rxq->port_id, rxq->qidx, nb_hold);

	nb_hold += rxq->nb_rx_hold;

	/*
	 * FL descriptors needs to be written before incrementing the
	 * FL queue WR pointer
	 */
	rte_wmb();
	if (nb_hold > rxq->rx_free_thresh) {
		PMD_RX_LOG(DEBUG, "port=%u queue=%d nb_hold=%u avail=%d",
				rxq->port_id, rxq->qidx, nb_hold, avail);
		nfp_qcp_ptr_add(rxq->qcp_fl, NFP_QCP_WRITE_PTR, nb_hold);
		nb_hold = 0;
	}

	rxq->nb_rx_hold = nb_hold;

	return avail;
}

uint16_t
nfp_flower_pf_xmit_pkts(void *tx_queue,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	int i;
	int pkt_size;
	int dma_size;
	uint64_t dma_addr;
	uint16_t free_descs;
	uint16_t issued_descs;
	struct rte_mbuf *pkt;
	struct nfp_net_hw *hw;
	struct rte_mbuf **lmbuf;
	struct nfp_net_txq *txq;
	struct nfp_net_nfd3_tx_desc txd;
	struct nfp_net_nfd3_tx_desc *txds;

	txq = tx_queue;
	hw = txq->hw;
	txds = &txq->txds[txq->wr_p];

	PMD_TX_LOG(DEBUG, "working for queue %d at pos %u and %u packets",
			txq->qidx, txq->wr_p, nb_pkts);

	if ((nfp_net_nfd3_free_tx_desc(txq) < nb_pkts) || (nfp_net_nfd3_txq_full(txq)))
		nfp_net_tx_free_bufs(txq);

	free_descs = (uint16_t)nfp_net_nfd3_free_tx_desc(txq);
	if (unlikely(free_descs == 0))
		return 0;

	pkt = *tx_pkts;
	issued_descs = 0;

	/* Sending packets */
	for (i = 0; i < nb_pkts && free_descs > 0; i++) {
		/* Grabbing the mbuf linked to the current descriptor */
		lmbuf = &txq->txbufs[txq->wr_p].mbuf;
		/* Warming the cache for releasing the mbuf later on */
		RTE_MBUF_PREFETCH_TO_FREE(*lmbuf);

		pkt = *(tx_pkts + i);

		if (unlikely(pkt->nb_segs > 1 &&
				!(hw->cap & NFP_NET_CFG_CTRL_GATHER))) {
			PMD_INIT_LOG(ERR, "Multisegment packet not supported");
			goto xmit_end;
		}

		/* Checking if we have enough descriptors */
		if (unlikely(pkt->nb_segs > free_descs))
			goto xmit_end;

		/*
		 * Checksum and VLAN flags just in the first descriptor for a
		 * multisegment packet, but TSO info needs to be in all of them.
		 */
		txd.data_len = pkt->pkt_len;
		nfp_net_nfd3_tx_tso(txq, &txd, pkt);
		nfp_net_nfd3_tx_cksum(txq, &txd, pkt);

		if ((pkt->ol_flags & RTE_MBUF_F_TX_VLAN) &&
				(hw->cap & NFP_NET_CFG_CTRL_TXVLAN)) {
			txd.flags |= PCIE_DESC_TX_VLAN;
			txd.vlan = pkt->vlan_tci;
		}

		/*
		 * mbuf data_len is the data in one segment and pkt_len data
		 * in the whole packet. When the packet is just one segment,
		 * then data_len = pkt_len
		 */
		pkt_size = pkt->pkt_len;

		while (pkt != NULL && free_descs > 0) {
			/* Copying TSO, VLAN and cksum info */
			*txds = txd;

			/* Releasing mbuf used by this descriptor previously*/
			if (*lmbuf != NULL)
				rte_pktmbuf_free_seg(*lmbuf);

			/*
			 * Linking mbuf with descriptor for being released
			 * next time descriptor is used
			 */
			*lmbuf = pkt;

			dma_size = pkt->data_len;
			dma_addr = rte_mbuf_data_iova(pkt);

			/* Filling descriptors fields */
			txds->dma_len = dma_size;
			txds->data_len = txd.data_len;
			txds->dma_addr_hi = (dma_addr >> 32) & 0xff;
			txds->dma_addr_lo = (dma_addr & 0xffffffff);
			free_descs--;

			txq->wr_p++;
			if (unlikely(txq->wr_p == txq->tx_count)) /* wrapping?*/
				txq->wr_p = 0;

			pkt_size -= dma_size;

			/*
			 * Making the EOP, packets with just one segment
			 * the priority
			 */
			if (likely(pkt_size == 0))
				txds->offset_eop = PCIE_DESC_TX_EOP | FLOWER_PKT_DATA_OFFSET;
			else
				txds->offset_eop = 0;

			pkt = pkt->next;
			/* Referencing next free TX descriptor */
			txds = &txq->txds[txq->wr_p];
			lmbuf = &txq->txbufs[txq->wr_p].mbuf;
			issued_descs++;
		}
	}

xmit_end:
	/* Increment write pointers. Force memory write before we let HW know */
	rte_wmb();
	nfp_qcp_ptr_add(txq->qcp_q, NFP_QCP_WRITE_PTR, issued_descs);

	return i;
}

static int
nfp_flower_init_vnic_common(struct nfp_net_hw *hw, const char *vnic_type)
{
	uint32_t start_q;
	uint64_t rx_bar_off;
	uint64_t tx_bar_off;
	const int stride = 4;
	struct nfp_pf_dev *pf_dev;
	struct rte_pci_device *pci_dev;

	pf_dev = hw->pf_dev;
	pci_dev = hw->pf_dev->pci_dev;

	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->subsystem_device_id = pci_dev->id.subsystem_device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;

	PMD_INIT_LOG(DEBUG, "%s vNIC ctrl bar: %p", vnic_type, hw->ctrl_bar);

	/* Read the number of available rx/tx queues from hardware */
	hw->max_rx_queues = nn_cfg_readl(hw, NFP_NET_CFG_MAX_RXRINGS);
	hw->max_tx_queues = nn_cfg_readl(hw, NFP_NET_CFG_MAX_TXRINGS);

	/* Work out where in the BAR the queues start */
	start_q = nn_cfg_readl(hw, NFP_NET_CFG_START_TXQ);
	tx_bar_off = (uint64_t)start_q * NFP_QCP_QUEUE_ADDR_SZ;
	start_q = nn_cfg_readl(hw, NFP_NET_CFG_START_RXQ);
	rx_bar_off = (uint64_t)start_q * NFP_QCP_QUEUE_ADDR_SZ;

	hw->tx_bar = pf_dev->hw_queues + tx_bar_off;
	hw->rx_bar = pf_dev->hw_queues + rx_bar_off;

	/* Get some of the read-only fields from the config BAR */
	hw->ver = nn_cfg_readl(hw, NFP_NET_CFG_VERSION);
	hw->cap = nn_cfg_readl(hw, NFP_NET_CFG_CAP);
	hw->max_mtu = nn_cfg_readl(hw, NFP_NET_CFG_MAX_MTU);
	/* Set the current MTU to the maximum supported */
	hw->mtu = hw->max_mtu;
	hw->flbufsz = DEFAULT_FLBUF_SIZE;

	if (nfp_net_check_dma_mask(hw, pci_dev->name) != 0)
		return -ENODEV;

	/* read the Rx offset configured from firmware */
	if (NFD_CFG_MAJOR_VERSION_of(hw->ver) < 2)
		hw->rx_offset = NFP_NET_RX_OFFSET;
	else
		hw->rx_offset = nn_cfg_readl(hw, NFP_NET_CFG_RX_OFFSET_ADDR);

	hw->ctrl = 0;
	hw->stride_rx = stride;
	hw->stride_tx = stride;

	/* Reuse cfg queue setup function */
	nfp_net_cfg_queue_setup(hw);

	PMD_INIT_LOG(INFO, "%s vNIC max_rx_queues: %u, max_tx_queues: %u",
			vnic_type, hw->max_rx_queues, hw->max_tx_queues);

	/* Initializing spinlock for reconfigs */
	rte_spinlock_init(&hw->reconfig_lock);

	return 0;
}

static int
nfp_flower_init_ctrl_vnic(struct nfp_net_hw *hw)
{
	uint32_t i;
	int ret = 0;
	uint16_t n_txq;
	uint16_t n_rxq;
	unsigned int numa_node;
	struct rte_mempool *mp;
	struct nfp_net_rxq *rxq;
	struct nfp_net_txq *txq;
	struct nfp_pf_dev *pf_dev;
	struct rte_eth_dev *eth_dev;
	const struct rte_memzone *tz;
	struct nfp_app_fw_flower *app_fw_flower;

	/* Set up some pointers here for ease of use */
	pf_dev = hw->pf_dev;
	app_fw_flower = NFP_PRIV_TO_APP_FW_FLOWER(pf_dev->app_fw_priv);

	ret = nfp_flower_init_vnic_common(hw, "ctrl_vnic");
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Could not init pf vnic");
		return -EINVAL;
	}

	/* Allocate memory for the eth_dev of the vNIC */
	hw->eth_dev = rte_zmalloc("nfp_ctrl_vnic",
			sizeof(struct rte_eth_dev), RTE_CACHE_LINE_SIZE);
	if (hw->eth_dev == NULL) {
		PMD_INIT_LOG(ERR, "Could not allocate ctrl vnic");
		return -ENOMEM;
	}

	/* Grab the pointer to the newly created rte_eth_dev here */
	eth_dev = hw->eth_dev;

	/* Also allocate memory for the data part of the eth_dev */
	eth_dev->data = rte_zmalloc("nfp_ctrl_vnic_data",
			sizeof(struct rte_eth_dev_data), RTE_CACHE_LINE_SIZE);
	if (eth_dev->data == NULL) {
		PMD_INIT_LOG(ERR, "Could not allocate ctrl vnic data");
		ret = -ENOMEM;
		goto eth_dev_cleanup;
	}

	/* Create a mbuf pool for the ctrl vNIC */
	numa_node = rte_socket_id();
	app_fw_flower->ctrl_pktmbuf_pool = rte_pktmbuf_pool_create("ctrl_mbuf_pool",
			4 * CTRL_VNIC_NB_DESC, 64, 0, 9216, numa_node);
	if (app_fw_flower->ctrl_pktmbuf_pool == NULL) {
		PMD_INIT_LOG(ERR, "Create mbuf pool for ctrl vnic failed");
		ret = -ENOMEM;
		goto dev_data_cleanup;
	}

	mp = app_fw_flower->ctrl_pktmbuf_pool;

	/* Configure the ctrl vNIC device */
	n_rxq = hw->max_rx_queues;
	n_txq = hw->max_tx_queues;
	eth_dev->data->rx_queues = rte_zmalloc("ethdev->rx_queues",
			sizeof(eth_dev->data->rx_queues[0]) * n_rxq,
			RTE_CACHE_LINE_SIZE);
	if (eth_dev->data->rx_queues == NULL) {
		PMD_INIT_LOG(ERR, "rte_zmalloc failed for ctrl vNIC rx queues");
		ret = -ENOMEM;
		goto mempool_cleanup;
	}

	eth_dev->data->tx_queues = rte_zmalloc("ethdev->tx_queues",
			sizeof(eth_dev->data->tx_queues[0]) * n_txq,
			RTE_CACHE_LINE_SIZE);
	if (eth_dev->data->tx_queues == NULL) {
		PMD_INIT_LOG(ERR, "rte_zmalloc failed for ctrl vNIC tx queues");
		ret = -ENOMEM;
		goto rx_queue_free;
	}

	/* Fill in some of the eth_dev fields */
	eth_dev->device = &pf_dev->pci_dev->device;
	eth_dev->data->nb_tx_queues = n_rxq;
	eth_dev->data->nb_rx_queues = n_txq;
	eth_dev->data->dev_private = hw;

	/* Set up the Rx queues */
	for (i = 0; i < n_rxq; i++) {
		rxq = rte_zmalloc_socket("ethdev RX queue",
				sizeof(struct nfp_net_rxq), RTE_CACHE_LINE_SIZE,
				numa_node);
		if (rxq == NULL) {
			PMD_DRV_LOG(ERR, "Error allocating rxq");
			ret = -ENOMEM;
			goto rx_queue_setup_cleanup;
		}

		eth_dev->data->rx_queues[i] = rxq;

		/* Hw queues mapping based on firmware configuration */
		rxq->qidx = i;
		rxq->fl_qcidx = i * hw->stride_rx;
		rxq->rx_qcidx = rxq->fl_qcidx + (hw->stride_rx - 1);
		rxq->qcp_fl = hw->rx_bar + NFP_QCP_QUEUE_OFF(rxq->fl_qcidx);
		rxq->qcp_rx = hw->rx_bar + NFP_QCP_QUEUE_OFF(rxq->rx_qcidx);

		/*
		 * Tracking mbuf size for detecting a potential mbuf overflow due to
		 * RX offset
		 */
		rxq->mem_pool = mp;
		rxq->mbuf_size = rxq->mem_pool->elt_size;
		rxq->mbuf_size -= (sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM);
		hw->flbufsz = rxq->mbuf_size;

		rxq->rx_count = CTRL_VNIC_NB_DESC;
		rxq->rx_free_thresh = DEFAULT_RX_FREE_THRESH;
		rxq->drop_en = 1;

		/*
		 * Allocate RX ring hardware descriptors. A memzone large enough to
		 * handle the maximum ring size is allocated in order to allow for
		 * resizing in later calls to the queue setup function.
		 */
		tz = rte_eth_dma_zone_reserve(eth_dev, "ctrl_rx_ring", i,
				sizeof(struct nfp_net_rx_desc) * NFP_NET_MAX_RX_DESC,
				NFP_MEMZONE_ALIGN, numa_node);
		if (tz == NULL) {
			PMD_DRV_LOG(ERR, "Error allocating rx dma");
			rte_free(rxq);
			ret = -ENOMEM;
			goto rx_queue_setup_cleanup;
		}

		/* Saving physical and virtual addresses for the RX ring */
		rxq->dma = (uint64_t)tz->iova;
		rxq->rxds = (struct nfp_net_rx_desc *)tz->addr;

		/* Mbuf pointers array for referencing mbufs linked to RX descriptors */
		rxq->rxbufs = rte_zmalloc_socket("rxq->rxbufs",
				sizeof(*rxq->rxbufs) * CTRL_VNIC_NB_DESC,
				RTE_CACHE_LINE_SIZE, numa_node);
		if (rxq->rxbufs == NULL) {
			rte_eth_dma_zone_free(eth_dev, "ctrl_rx_ring", i);
			rte_free(rxq);
			ret = -ENOMEM;
			goto rx_queue_setup_cleanup;
		}

		nfp_net_reset_rx_queue(rxq);

		rxq->hw = hw;

		/*
		 * Telling the HW about the physical address of the RX ring and number
		 * of descriptors in log2 format
		 */
		nn_cfg_writeq(hw, NFP_NET_CFG_RXR_ADDR(i), rxq->dma);
		nn_cfg_writeb(hw, NFP_NET_CFG_RXR_SZ(i), rte_log2_u32(CTRL_VNIC_NB_DESC));
	}

	/* Set up the Tx queues */
	for (i = 0; i < n_txq; i++) {
		txq = rte_zmalloc_socket("ethdev TX queue",
				sizeof(struct nfp_net_txq), RTE_CACHE_LINE_SIZE,
				numa_node);
		if (txq == NULL) {
			PMD_DRV_LOG(ERR, "Error allocating txq");
			ret = -ENOMEM;
			goto tx_queue_setup_cleanup;
		}

		eth_dev->data->tx_queues[i] = txq;

		/*
		 * Allocate TX ring hardware descriptors. A memzone large enough to
		 * handle the maximum ring size is allocated in order to allow for
		 * resizing in later calls to the queue setup function.
		 */
		tz = rte_eth_dma_zone_reserve(eth_dev, "ctrl_tx_ring", i,
				sizeof(struct nfp_net_nfd3_tx_desc) * NFP_NET_MAX_TX_DESC,
				NFP_MEMZONE_ALIGN, numa_node);
		if (tz == NULL) {
			PMD_DRV_LOG(ERR, "Error allocating tx dma");
			rte_free(txq);
			ret = -ENOMEM;
			goto tx_queue_setup_cleanup;
		}

		txq->tx_count = CTRL_VNIC_NB_DESC;
		txq->tx_free_thresh = DEFAULT_RX_FREE_THRESH;
		txq->tx_pthresh = DEFAULT_TX_PTHRESH;
		txq->tx_hthresh = DEFAULT_TX_HTHRESH;
		txq->tx_wthresh = DEFAULT_TX_WTHRESH;

		/* Queue mapping based on firmware configuration */
		txq->qidx = i;
		txq->tx_qcidx = i * hw->stride_tx;
		txq->qcp_q = hw->tx_bar + NFP_QCP_QUEUE_OFF(txq->tx_qcidx);

		/* Saving physical and virtual addresses for the TX ring */
		txq->dma = (uint64_t)tz->iova;
		txq->txds = (struct nfp_net_nfd3_tx_desc *)tz->addr;

		/* Mbuf pointers array for referencing mbufs linked to TX descriptors */
		txq->txbufs = rte_zmalloc_socket("txq->txbufs",
				sizeof(*txq->txbufs) * CTRL_VNIC_NB_DESC,
				RTE_CACHE_LINE_SIZE, numa_node);
		if (txq->txbufs == NULL) {
			rte_eth_dma_zone_free(eth_dev, "ctrl_tx_ring", i);
			rte_free(txq);
			ret = -ENOMEM;
			goto tx_queue_setup_cleanup;
		}

		nfp_net_reset_tx_queue(txq);

		txq->hw = hw;

		/*
		 * Telling the HW about the physical address of the TX ring and number
		 * of descriptors in log2 format
		 */
		nn_cfg_writeq(hw, NFP_NET_CFG_TXR_ADDR(i), txq->dma);
		nn_cfg_writeb(hw, NFP_NET_CFG_TXR_SZ(i), rte_log2_u32(CTRL_VNIC_NB_DESC));
	}

	return 0;

tx_queue_setup_cleanup:
	for (i = 0; i < hw->max_tx_queues; i++) {
		txq = eth_dev->data->tx_queues[i];
		if (txq != NULL) {
			rte_free(txq->txbufs);
			rte_eth_dma_zone_free(eth_dev, "ctrl_tx_ring", i);
			rte_free(txq);
		}
	}
rx_queue_setup_cleanup:
	for (i = 0; i < hw->max_rx_queues; i++) {
		rxq = eth_dev->data->rx_queues[i];
		if (rxq != NULL) {
			rte_free(rxq->rxbufs);
			rte_eth_dma_zone_free(eth_dev, "ctrl_rx_ring", i);
			rte_free(rxq);
		}
	}
	rte_free(eth_dev->data->tx_queues);
rx_queue_free:
	rte_free(eth_dev->data->rx_queues);
mempool_cleanup:
	rte_mempool_free(mp);
dev_data_cleanup:
	rte_free(eth_dev->data);
eth_dev_cleanup:
	rte_free(eth_dev);

	return ret;
}

static void
nfp_flower_cleanup_ctrl_vnic(struct nfp_net_hw *hw)
{
	uint32_t i;
	struct nfp_net_rxq *rxq;
	struct nfp_net_txq *txq;
	struct rte_eth_dev *eth_dev;
	struct nfp_app_fw_flower *app_fw_flower;

	eth_dev = hw->eth_dev;
	app_fw_flower = NFP_PRIV_TO_APP_FW_FLOWER(hw->pf_dev->app_fw_priv);

	for (i = 0; i < hw->max_tx_queues; i++) {
		txq = eth_dev->data->tx_queues[i];
		if (txq != NULL) {
			rte_free(txq->txbufs);
			rte_eth_dma_zone_free(eth_dev, "ctrl_tx_ring", i);
			rte_free(txq);
		}
	}

	for (i = 0; i < hw->max_rx_queues; i++) {
		rxq = eth_dev->data->rx_queues[i];
		if (rxq != NULL) {
			rte_free(rxq->rxbufs);
			rte_eth_dma_zone_free(eth_dev, "ctrl_rx_ring", i);
			rte_free(rxq);
		}
	}

	rte_free(eth_dev->data->tx_queues);
	rte_free(eth_dev->data->rx_queues);
	rte_mempool_free(app_fw_flower->ctrl_pktmbuf_pool);
	rte_free(eth_dev->data);
	rte_free(eth_dev);
}

static int
nfp_flower_start_ctrl_vnic(struct nfp_net_hw *hw)
{
	int ret;
	uint32_t update;
	uint32_t new_ctrl;
	struct rte_eth_dev *dev;

	dev = hw->eth_dev;

	/* Disabling queues just in case... */
	nfp_net_disable_queues(dev);

	/* Enabling the required queues in the device */
	nfp_net_enable_queues(dev);

	/* Writing configuration parameters in the device */
	nfp_net_params_setup(hw);

	new_ctrl = NFP_NET_CFG_CTRL_ENABLE;
	update = NFP_NET_CFG_UPDATE_GEN | NFP_NET_CFG_UPDATE_RING |
			NFP_NET_CFG_UPDATE_MSIX;

	rte_wmb();

	/* If an error when reconfig we avoid to change hw state */
	ret = nfp_net_reconfig(hw, new_ctrl, update);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to reconfig ctrl vnic");
		return -EIO;
	}

	hw->ctrl = new_ctrl;

	/* Setup the freelist ring */
	ret = nfp_net_rx_freelist_setup(dev);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Error with flower ctrl vNIC freelist setup");
		return -EIO;
	}

	return 0;
}

static int
nfp_flower_ctrl_vnic_service(void *arg)
{
	struct nfp_app_fw_flower *app_fw_flower = arg;

	nfp_flower_ctrl_vnic_poll(app_fw_flower);

	return 0;
}

static int
nfp_flower_enable_services(struct nfp_app_fw_flower *app_fw_flower)
{
	int ret;
	uint32_t service_id;
	const struct rte_service_spec flower_service = {
		.name              = "flower_ctrl_vnic_service",
		.callback          = nfp_flower_ctrl_vnic_service,
		.callback_userdata = (void *)app_fw_flower,
	};

	/* Register the flower services */
	ret = rte_service_component_register(&flower_service, &service_id);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Could not register %s", flower_service.name);
		return -EINVAL;
	}

	app_fw_flower->ctrl_vnic_id = service_id;
	PMD_INIT_LOG(INFO, "%s registered", flower_service.name);

	/* Map them to available service cores*/
	ret = nfp_map_service(service_id);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Could not map %s", flower_service.name);
		return -EINVAL;
	}

	return 0;
}

int
nfp_init_app_fw_flower(struct nfp_pf_dev *pf_dev)
{
	int ret;
	int err;
	uint64_t ext_features;
	unsigned int numa_node;
	struct nfp_net_hw *pf_hw;
	struct nfp_net_hw *ctrl_hw;
	struct nfp_app_fw_flower *app_fw_flower;

	numa_node = rte_socket_id();

	/* Allocate memory for the Flower app */
	app_fw_flower = rte_zmalloc_socket("nfp_app_fw_flower", sizeof(*app_fw_flower),
			RTE_CACHE_LINE_SIZE, numa_node);
	if (app_fw_flower == NULL) {
		PMD_INIT_LOG(ERR, "Could not malloc app fw flower");
		return -ENOMEM;
	}

	pf_dev->app_fw_priv = app_fw_flower;

	ret = nfp_flow_priv_init(pf_dev);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "init flow priv failed");
		goto app_cleanup;
	}

	ret = nfp_mtr_priv_init(pf_dev);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Error initializing metering private data");
		goto flow_priv_cleanup;
	}

	/* Allocate memory for the PF AND ctrl vNIC here (hence the * 2) */
	pf_hw = rte_zmalloc_socket("nfp_pf_vnic", 2 * sizeof(struct nfp_net_adapter),
			RTE_CACHE_LINE_SIZE, numa_node);
	if (pf_hw == NULL) {
		PMD_INIT_LOG(ERR, "Could not malloc nfp pf vnic");
		ret = -ENOMEM;
		goto mtr_priv_cleanup;
	}

	/* Map the PF ctrl bar */
	pf_dev->ctrl_bar = nfp_rtsym_map(pf_dev->sym_tbl, "_pf0_net_bar0",
			32768, &pf_dev->ctrl_area);
	if (pf_dev->ctrl_bar == NULL) {
		PMD_INIT_LOG(ERR, "Cloud not map the PF vNIC ctrl bar");
		ret = -ENODEV;
		goto vnic_cleanup;
	}

	/* Read the extra features */
	ext_features = nfp_rtsym_read_le(pf_dev->sym_tbl, "_abi_flower_extra_features",
			&err);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "Couldn't read extra features from fw");
		ret = -EIO;
		goto pf_cpp_area_cleanup;
	}

	/* Store the extra features */
	app_fw_flower->ext_features = ext_features;

	/* Fill in the PF vNIC and populate app struct */
	app_fw_flower->pf_hw = pf_hw;
	pf_hw->ctrl_bar = pf_dev->ctrl_bar;
	pf_hw->pf_dev = pf_dev;
	pf_hw->cpp = pf_dev->cpp;

	ret = nfp_flower_init_vnic_common(app_fw_flower->pf_hw, "pf_vnic");
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Could not initialize flower PF vNIC");
		goto pf_cpp_area_cleanup;
	}

	/* The ctrl vNIC struct comes directly after the PF one */
	app_fw_flower->ctrl_hw = pf_hw + 1;
	ctrl_hw = app_fw_flower->ctrl_hw;

	/* Map the ctrl vNIC ctrl bar */
	ctrl_hw->ctrl_bar = nfp_rtsym_map(pf_dev->sym_tbl, "_pf0_net_ctrl_bar",
			32768, &ctrl_hw->ctrl_area);
	if (ctrl_hw->ctrl_bar == NULL) {
		PMD_INIT_LOG(ERR, "Cloud not map the ctrl vNIC ctrl bar");
		ret = -ENODEV;
		goto pf_cpp_area_cleanup;
	}

	/* Now populate the ctrl vNIC */
	ctrl_hw->pf_dev = pf_dev;
	ctrl_hw->cpp = pf_dev->cpp;

	ret = nfp_flower_init_ctrl_vnic(app_fw_flower->ctrl_hw);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Could not initialize flower ctrl vNIC");
		goto ctrl_cpp_area_cleanup;
	}

	/* Start the ctrl vNIC */
	ret = nfp_flower_start_ctrl_vnic(app_fw_flower->ctrl_hw);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Could not start flower ctrl vNIC");
		goto ctrl_vnic_cleanup;
	}

	/* Start up flower services */
	ret = nfp_flower_enable_services(app_fw_flower);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Could not enable flower services");
		ret = -ESRCH;
		goto ctrl_vnic_cleanup;
	}

	ret = nfp_flower_repr_create(app_fw_flower);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Could not create representor ports");
		goto ctrl_vnic_cleanup;
	}

	return 0;

ctrl_vnic_cleanup:
	nfp_flower_cleanup_ctrl_vnic(app_fw_flower->ctrl_hw);
ctrl_cpp_area_cleanup:
	nfp_cpp_area_free(ctrl_hw->ctrl_area);
pf_cpp_area_cleanup:
	nfp_cpp_area_free(pf_dev->ctrl_area);
vnic_cleanup:
	rte_free(pf_hw);
mtr_priv_cleanup:
	nfp_mtr_priv_uninit(pf_dev);
flow_priv_cleanup:
	nfp_flow_priv_uninit(pf_dev);
app_cleanup:
	rte_free(app_fw_flower);

	return ret;
}

int
nfp_secondary_init_app_fw_flower(struct nfp_cpp *cpp)
{
	struct rte_eth_dev *eth_dev;
	const char *port_name = "pf_vnic_eth_dev";

	PMD_INIT_LOG(DEBUG, "Secondary attaching to port %s", port_name);

	eth_dev = rte_eth_dev_attach_secondary(port_name);
	if (eth_dev == NULL) {
		PMD_INIT_LOG(ERR, "Secondary process attach to port %s failed", port_name);
		return -ENODEV;
	}

	eth_dev->process_private = cpp;
	eth_dev->dev_ops = &nfp_flower_pf_vnic_ops;
	eth_dev->rx_pkt_burst = nfp_flower_pf_recv_pkts;
	eth_dev->tx_pkt_burst = nfp_flower_pf_xmit_pkts;
	rte_eth_dev_probing_finish(eth_dev);

	return 0;
}
