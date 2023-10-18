/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <inttypes.h>
#include <ethdev_pci.h>

#include "otx_ep_common.h"
#include "otx_ep_vf.h"
#include "otx2_ep_vf.h"
#include "cnxk_ep_vf.h"
#include "otx_ep_rxtx.h"
#include "otx_ep_mbox.h"

#define OTX_EP_DEV(_eth_dev) \
	((struct otx_ep_device *)(_eth_dev)->data->dev_private)

static const struct rte_eth_desc_lim otx_ep_rx_desc_lim = {
	.nb_max		= OTX_EP_MAX_OQ_DESCRIPTORS,
	.nb_min		= OTX_EP_MIN_OQ_DESCRIPTORS,
	.nb_align	= OTX_EP_RXD_ALIGN,
};

static const struct rte_eth_desc_lim otx_ep_tx_desc_lim = {
	.nb_max		= OTX_EP_MAX_IQ_DESCRIPTORS,
	.nb_min		= OTX_EP_MIN_IQ_DESCRIPTORS,
	.nb_align	= OTX_EP_TXD_ALIGN,
};

static void
otx_ep_set_tx_func(struct rte_eth_dev *eth_dev)
{
	struct otx_ep_device *otx_epvf = OTX_EP_DEV(eth_dev);

	if (otx_epvf->chip_gen == OTX_EP_CN10XX || otx_epvf->chip_gen == OTX_EP_CN9XX) {
		eth_dev->tx_pkt_burst = &cnxk_ep_xmit_pkts;
		if (otx_epvf->tx_offloads & RTE_ETH_TX_OFFLOAD_MULTI_SEGS)
			eth_dev->tx_pkt_burst = &cnxk_ep_xmit_pkts_mseg;
	} else {
		eth_dev->tx_pkt_burst = &otx_ep_xmit_pkts;
	}

	if (eth_dev->data->dev_started)
		rte_eth_fp_ops[eth_dev->data->port_id].tx_pkt_burst =
			eth_dev->tx_pkt_burst;
}

static void
otx_ep_set_rx_func(struct rte_eth_dev *eth_dev)
{
	struct otx_ep_device *otx_epvf = OTX_EP_DEV(eth_dev);

	if (otx_epvf->chip_gen == OTX_EP_CN10XX) {
		eth_dev->rx_pkt_burst = &cnxk_ep_recv_pkts;
		if (otx_epvf->rx_offloads & RTE_ETH_RX_OFFLOAD_SCATTER)
			eth_dev->rx_pkt_burst = &cnxk_ep_recv_pkts_mseg;
	} else if (otx_epvf->chip_gen == OTX_EP_CN9XX) {
		eth_dev->rx_pkt_burst = &cn9k_ep_recv_pkts;
		if (otx_epvf->rx_offloads & RTE_ETH_RX_OFFLOAD_SCATTER)
			eth_dev->rx_pkt_burst = &cn9k_ep_recv_pkts_mseg;
	} else {
		eth_dev->rx_pkt_burst = &otx_ep_recv_pkts;
	}

	if (eth_dev->data->dev_started)
		rte_eth_fp_ops[eth_dev->data->port_id].rx_pkt_burst =
			eth_dev->rx_pkt_burst;
}

static int
otx_ep_dev_info_get(struct rte_eth_dev *eth_dev,
		    struct rte_eth_dev_info *devinfo)
{
	struct otx_ep_device *otx_epvf;
	int max_rx_pktlen;

	otx_epvf = OTX_EP_DEV(eth_dev);

	max_rx_pktlen = otx_ep_mbox_get_max_pkt_len(eth_dev);
	if (!max_rx_pktlen) {
		otx_ep_err("Failed to get Max Rx packet length");
		return -EINVAL;
	}

	devinfo->speed_capa = RTE_ETH_LINK_SPEED_10G;
	devinfo->max_rx_queues = otx_epvf->max_rx_queues;
	devinfo->max_tx_queues = otx_epvf->max_tx_queues;

	devinfo->min_rx_bufsize = OTX_EP_MIN_RX_BUF_SIZE;
	devinfo->max_rx_pktlen = max_rx_pktlen;
	devinfo->max_mtu = devinfo->max_rx_pktlen - OTX_EP_ETH_OVERHEAD;
	devinfo->min_mtu = RTE_ETHER_MIN_LEN;
	devinfo->rx_offload_capa = RTE_ETH_RX_OFFLOAD_SCATTER;
	devinfo->tx_offload_capa = RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

	devinfo->max_mac_addrs = OTX_EP_MAX_MAC_ADDRS;

	devinfo->rx_desc_lim = otx_ep_rx_desc_lim;
	devinfo->tx_desc_lim = otx_ep_tx_desc_lim;

	devinfo->default_rxportconf.ring_size = OTX_EP_MIN_OQ_DESCRIPTORS;
	devinfo->default_txportconf.ring_size = OTX_EP_MIN_IQ_DESCRIPTORS;

	return 0;
}

static int
otx_ep_dev_link_update(struct rte_eth_dev *eth_dev, int wait_to_complete)
{
	RTE_SET_USED(wait_to_complete);

	if (!eth_dev->data->dev_started)
		return 0;
	struct rte_eth_link link;
	int ret = 0;

	memset(&link, 0, sizeof(link));
	ret = otx_ep_mbox_get_link_info(eth_dev, &link);
	if (ret)
		return -EINVAL;
	otx_ep_dbg("link status resp link %d duplex %d autoneg %d link_speed %d\n",
		    link.link_status, link.link_duplex, link.link_autoneg, link.link_speed);
	return rte_eth_linkstatus_set(eth_dev, &link);
}

static int
otx_ep_dev_mtu_set(struct rte_eth_dev *eth_dev, uint16_t mtu)
{
	struct rte_eth_dev_info devinfo;
	int32_t ret = 0;

	if (otx_ep_dev_info_get(eth_dev, &devinfo)) {
		otx_ep_err("Cannot set MTU to %u: failed to get device info", mtu);
		return -EPERM;
	}

	/* Check if MTU is within the allowed range */
	if (mtu < devinfo.min_mtu) {
		otx_ep_err("Invalid MTU %u: lower than minimum MTU %u", mtu, devinfo.min_mtu);
		return -EINVAL;
	}

	if (mtu > devinfo.max_mtu) {
		otx_ep_err("Invalid MTU %u; higher than maximum MTU %u", mtu, devinfo.max_mtu);
		return -EINVAL;
	}

	ret = otx_ep_mbox_set_mtu(eth_dev, mtu);
	if (ret)
		return -EINVAL;

	otx_ep_dbg("MTU is set to %u", mtu);

	return 0;
}

static int
otx_ep_dev_set_default_mac_addr(struct rte_eth_dev *eth_dev,
				struct rte_ether_addr *mac_addr)
{
	int ret;

	ret = otx_ep_mbox_set_mac_addr(eth_dev, mac_addr);
	if (ret)
		return -EINVAL;
	otx_ep_dbg("Default MAC address " RTE_ETHER_ADDR_PRT_FMT "\n",
		    RTE_ETHER_ADDR_BYTES(mac_addr));
	rte_ether_addr_copy(mac_addr, eth_dev->data->mac_addrs);
	return 0;
}

static int
otx_ep_dev_start(struct rte_eth_dev *eth_dev)
{
	struct otx_ep_device *otx_epvf;
	unsigned int q;
	int ret;

	otx_epvf = (struct otx_ep_device *)OTX_EP_DEV(eth_dev);
	/* Enable IQ/OQ for this device */
	ret = otx_epvf->fn_list.enable_io_queues(otx_epvf);
	if (ret) {
		otx_ep_err("IOQ enable failed\n");
		return ret;
	}

	for (q = 0; q < otx_epvf->nb_rx_queues; q++) {
		rte_write32(otx_epvf->droq[q]->nb_desc,
			    otx_epvf->droq[q]->pkts_credit_reg);

		rte_wmb();
		otx_ep_info("OQ[%d] dbells [%d]\n", q,
		rte_read32(otx_epvf->droq[q]->pkts_credit_reg));
	}

	otx_ep_dev_link_update(eth_dev, 0);

	otx_ep_set_tx_func(eth_dev);
	otx_ep_set_rx_func(eth_dev);

	otx_ep_info("dev started\n");

	for (q = 0; q < eth_dev->data->nb_rx_queues; q++)
		eth_dev->data->rx_queue_state[q] = RTE_ETH_QUEUE_STATE_STARTED;
	for (q = 0; q < eth_dev->data->nb_tx_queues; q++)
		eth_dev->data->tx_queue_state[q] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

/* Stop device and disable input/output functions */
static int
otx_ep_dev_stop(struct rte_eth_dev *eth_dev)
{
	struct otx_ep_device *otx_epvf = OTX_EP_DEV(eth_dev);
	uint16_t i;

	otx_epvf->fn_list.disable_io_queues(otx_epvf);

	for (i = 0; i < eth_dev->data->nb_rx_queues; i++)
		eth_dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
		eth_dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

/*
 * We only need 2 uint32_t locations per IOQ, but separate these so
 * each IOQ has the variables on its own cache line.
 */
#define OTX_EP_ISM_BUFFER_SIZE (OTX_EP_MAX_IOQS_PER_VF * RTE_CACHE_LINE_SIZE)
static int
otx_ep_ism_setup(struct otx_ep_device *otx_epvf)
{
	otx_epvf->ism_buffer_mz =
		rte_eth_dma_zone_reserve(otx_epvf->eth_dev, "ism",
					 0, OTX_EP_ISM_BUFFER_SIZE,
					 OTX_EP_PCI_RING_ALIGN, 0);

	/* Same DMA buffer is shared by OQ and IQ, clear it at start */
	memset(otx_epvf->ism_buffer_mz->addr, 0, OTX_EP_ISM_BUFFER_SIZE);
	if (otx_epvf->ism_buffer_mz == NULL) {
		otx_ep_err("Failed to allocate ISM buffer\n");
		return(-1);
	}
	otx_ep_dbg("ISM: virt: 0x%p, dma: 0x%" PRIX64,
		    (void *)otx_epvf->ism_buffer_mz->addr,
		    otx_epvf->ism_buffer_mz->iova);

	return 0;
}

static int
otx_ep_chip_specific_setup(struct otx_ep_device *otx_epvf)
{
	struct rte_pci_device *pdev = otx_epvf->pdev;
	uint32_t dev_id = pdev->id.device_id;
	int ret = 0;

	switch (dev_id) {
	case PCI_DEVID_OCTEONTX_EP_VF:
		otx_epvf->chip_id = dev_id;
		ret = otx_ep_vf_setup_device(otx_epvf);
		otx_epvf->fn_list.disable_io_queues(otx_epvf);
		break;
	case PCI_DEVID_CN9K_EP_NET_VF:
	case PCI_DEVID_CN98XX_EP_NET_VF:
	case PCI_DEVID_CNF95N_EP_NET_VF:
	case PCI_DEVID_CNF95O_EP_NET_VF:
		otx_epvf->chip_id = dev_id;
		ret = otx2_ep_vf_setup_device(otx_epvf);
		otx_epvf->fn_list.disable_io_queues(otx_epvf);
		if (otx_ep_ism_setup(otx_epvf))
			ret = -EINVAL;
		break;
	case PCI_DEVID_CN10KA_EP_NET_VF:
	case PCI_DEVID_CN10KB_EP_NET_VF:
	case PCI_DEVID_CNF10KA_EP_NET_VF:
	case PCI_DEVID_CNF10KB_EP_NET_VF:
		otx_epvf->chip_id = dev_id;
		ret = cnxk_ep_vf_setup_device(otx_epvf);
		otx_epvf->fn_list.disable_io_queues(otx_epvf);
		if (otx_ep_ism_setup(otx_epvf))
			ret = -EINVAL;
		break;
	default:
		otx_ep_err("Unsupported device\n");
		ret = -EINVAL;
	}

	if (!ret)
		otx_ep_info("OTX_EP dev_id[%d]\n", dev_id);

	return ret;
}

/* OTX_EP VF device initialization */
static int
otx_epdev_init(struct otx_ep_device *otx_epvf)
{
	uint32_t ethdev_queues;
	int ret = 0;

	ret = otx_ep_chip_specific_setup(otx_epvf);
	if (ret) {
		otx_ep_err("Chip specific setup failed\n");
		goto setup_fail;
	}

	otx_epvf->fn_list.setup_device_regs(otx_epvf);

	otx_epvf->eth_dev->tx_pkt_burst = &cnxk_ep_xmit_pkts;
	otx_epvf->eth_dev->rx_pkt_burst = &otx_ep_recv_pkts;
	if (otx_epvf->chip_id == PCI_DEVID_OCTEONTX_EP_VF) {
		otx_epvf->eth_dev->tx_pkt_burst = &otx_ep_xmit_pkts;
		otx_epvf->chip_gen = OTX_EP_CN8XX;
	} else if (otx_epvf->chip_id == PCI_DEVID_CN9K_EP_NET_VF ||
		 otx_epvf->chip_id == PCI_DEVID_CN98XX_EP_NET_VF ||
		 otx_epvf->chip_id == PCI_DEVID_CNF95N_EP_NET_VF ||
		 otx_epvf->chip_id == PCI_DEVID_CNF95O_EP_NET_VF) {
		otx_epvf->eth_dev->rx_pkt_burst = &cn9k_ep_recv_pkts;
		otx_epvf->chip_gen = OTX_EP_CN9XX;
	} else if (otx_epvf->chip_id == PCI_DEVID_CN10KA_EP_NET_VF ||
		   otx_epvf->chip_id == PCI_DEVID_CN10KB_EP_NET_VF ||
		   otx_epvf->chip_id == PCI_DEVID_CNF10KA_EP_NET_VF ||
		   otx_epvf->chip_id == PCI_DEVID_CNF10KB_EP_NET_VF) {
		otx_epvf->eth_dev->rx_pkt_burst = &cnxk_ep_recv_pkts;
		otx_epvf->chip_gen = OTX_EP_CN10XX;
	} else {
		otx_ep_err("Invalid chip_id\n");
		ret = -EINVAL;
		goto setup_fail;
	}
	ethdev_queues = (uint32_t)(otx_epvf->sriov_info.rings_per_vf);
	otx_epvf->max_rx_queues = ethdev_queues;
	otx_epvf->max_tx_queues = ethdev_queues;

	otx_ep_info("OTX_EP Device is Ready\n");

setup_fail:
	return ret;
}

static int
otx_ep_dev_configure(struct rte_eth_dev *eth_dev)
{
	struct otx_ep_device *otx_epvf = OTX_EP_DEV(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	struct rte_eth_rxmode *rxmode;
	struct rte_eth_txmode *txmode;
	struct rte_eth_conf *conf;

	conf = &data->dev_conf;
	rxmode = &conf->rxmode;
	txmode = &conf->txmode;
	if (eth_dev->data->nb_rx_queues > otx_epvf->max_rx_queues ||
	    eth_dev->data->nb_tx_queues > otx_epvf->max_tx_queues) {
		otx_ep_err("invalid num queues\n");
		return -EINVAL;
	}
	otx_ep_info("OTX_EP Device is configured with num_txq %d num_rxq %d\n",
		    eth_dev->data->nb_rx_queues, eth_dev->data->nb_tx_queues);

	otx_epvf->rx_offloads = rxmode->offloads;
	otx_epvf->tx_offloads = txmode->offloads;

	return 0;
}

/**
 * Setup our receive queue/ringbuffer. This is the
 * queue the Octeon uses to send us packets and
 * responses. We are given a memory pool for our
 * packet buffers that are used to populate the receive
 * queue.
 *
 * @param eth_dev
 *    Pointer to the structure rte_eth_dev
 * @param q_no
 *    Queue number
 * @param num_rx_descs
 *    Number of entries in the queue
 * @param socket_id
 *    Where to allocate memory
 * @param rx_conf
 *    Pointer to the struction rte_eth_rxconf
 * @param mp
 *    Pointer to the packet pool
 *
 * @return
 *    - On success, return 0
 *    - On failure, return -1
 */
static int
otx_ep_rx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t q_no,
		       uint16_t num_rx_descs, unsigned int socket_id,
		       const struct rte_eth_rxconf *rx_conf __rte_unused,
		       struct rte_mempool *mp)
{
	struct otx_ep_device *otx_epvf = OTX_EP_DEV(eth_dev);
	struct rte_pktmbuf_pool_private *mbp_priv;
	uint16_t buf_size;

	if (q_no >= otx_epvf->max_rx_queues) {
		otx_ep_err("Invalid rx queue number %u\n", q_no);
		return -EINVAL;
	}

	if (num_rx_descs & (num_rx_descs - 1)) {
		otx_ep_err("Invalid rx desc number should be pow 2  %u\n",
			   num_rx_descs);
		return -EINVAL;
	}
	if (num_rx_descs < (SDP_GBL_WMARK * 8)) {
		otx_ep_err("Invalid rx desc number(%u) should at least be greater than 8xwmark  %u\n",
			   num_rx_descs, (SDP_GBL_WMARK * 8));
		return -EINVAL;
	}

	otx_ep_dbg("setting up rx queue %u\n", q_no);

	mbp_priv = rte_mempool_get_priv(mp);
	buf_size = mbp_priv->mbuf_data_room_size - RTE_PKTMBUF_HEADROOM;

	if (otx_ep_setup_oqs(otx_epvf, q_no, num_rx_descs, buf_size, mp,
			     socket_id)) {
		otx_ep_err("droq allocation failed\n");
		return -1;
	}

	eth_dev->data->rx_queues[q_no] = otx_epvf->droq[q_no];

	return 0;
}

/**
 * Release the receive queue/ringbuffer. Called by
 * the upper layers.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param q_no
 *   Receive queue index.
 *
 * @return
 *    - nothing
 */
static void
otx_ep_rx_queue_release(struct rte_eth_dev *dev, uint16_t q_no)
{
	struct otx_ep_droq *rq = dev->data->rx_queues[q_no];
	struct otx_ep_device *otx_epvf = rq->otx_ep_dev;
	int q_id = rq->q_no;

	if (otx_ep_delete_oqs(otx_epvf, q_id))
		otx_ep_err("Failed to delete OQ:%d\n", q_id);
}

/**
 * Allocate and initialize SW ring. Initialize associated HW registers.
 *
 * @param eth_dev
 *   Pointer to structure rte_eth_dev
 *
 * @param q_no
 *   Queue number
 *
 * @param num_tx_descs
 *   Number of ringbuffer descriptors
 *
 * @param socket_id
 *   NUMA socket id, used for memory allocations
 *
 * @param tx_conf
 *   Pointer to the structure rte_eth_txconf
 *
 * @return
 *   - On success, return 0
 *   - On failure, return -errno value
 */
static int
otx_ep_tx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t q_no,
		       uint16_t num_tx_descs, unsigned int socket_id,
		       const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct otx_ep_device *otx_epvf = OTX_EP_DEV(eth_dev);
	int retval;

	if (q_no >= otx_epvf->max_tx_queues) {
		otx_ep_err("Invalid tx queue number %u\n", q_no);
		return -EINVAL;
	}
	if (num_tx_descs & (num_tx_descs - 1)) {
		otx_ep_err("Invalid tx desc number should be pow 2  %u\n",
			   num_tx_descs);
		return -EINVAL;
	}
	if (num_tx_descs < (SDP_GBL_WMARK * 8)) {
		otx_ep_err("Invalid tx desc number(%u) should at least be greater than 8*wmark(%u)\n",
			   num_tx_descs, (SDP_GBL_WMARK * 8));
		return -EINVAL;
	}

	retval = otx_ep_setup_iqs(otx_epvf, q_no, num_tx_descs, socket_id);

	if (retval) {
		otx_ep_err("IQ(TxQ) creation failed.\n");
		return retval;
	}

	eth_dev->data->tx_queues[q_no] = otx_epvf->instr_queue[q_no];
	otx_ep_dbg("tx queue[%d] setup\n", q_no);
	return 0;
}

/**
 * Release the transmit queue/ringbuffer. Called by
 * the upper layers.
 *
 * @param dev
 *    Pointer to Ethernet device structure.
 * @param q_no
 *    Transmit queue index.
 *
 * @return
 *    - nothing
 */
static void
otx_ep_tx_queue_release(struct rte_eth_dev *dev, uint16_t q_no)
{
	struct otx_ep_instr_queue *tq = dev->data->tx_queues[q_no];

	otx_ep_delete_iqs(tq->otx_ep_dev, tq->q_no);
}

static int
otx_ep_dev_stats_reset(struct rte_eth_dev *dev)
{
	struct otx_ep_device *otx_epvf = OTX_EP_DEV(dev);
	uint32_t i;

	for (i = 0; i < otx_epvf->nb_tx_queues; i++)
		memset(&otx_epvf->instr_queue[i]->stats, 0,
		       sizeof(struct otx_ep_iq_stats));

	for (i = 0; i < otx_epvf->nb_rx_queues; i++)
		memset(&otx_epvf->droq[i]->stats, 0,
		       sizeof(struct otx_ep_droq_stats));

	return 0;
}

static int
otx_ep_dev_stats_get(struct rte_eth_dev *eth_dev,
				struct rte_eth_stats *stats)
{
	struct otx_ep_device *otx_epvf = OTX_EP_DEV(eth_dev);
	struct otx_ep_iq_stats *ostats;
	struct otx_ep_droq_stats *istats;
	uint32_t i;

	memset(stats, 0, sizeof(struct rte_eth_stats));

	for (i = 0; i < otx_epvf->nb_tx_queues; i++) {
		ostats = &otx_epvf->instr_queue[i]->stats;
		stats->q_opackets[i] = ostats->tx_pkts;
		stats->q_obytes[i] = ostats->tx_bytes;
		stats->opackets += ostats->tx_pkts;
		stats->obytes += ostats->tx_bytes;
		stats->oerrors += ostats->instr_dropped;
	}
	for (i = 0; i < otx_epvf->nb_rx_queues; i++) {
		istats = &otx_epvf->droq[i]->stats;
		stats->q_ipackets[i] = istats->pkts_received;
		stats->q_ibytes[i] = istats->bytes_received;
		stats->q_errors[i] = istats->rx_err;
		stats->ipackets += istats->pkts_received;
		stats->ibytes += istats->bytes_received;
		stats->imissed += istats->rx_alloc_failure;
		stats->ierrors += istats->rx_err;
		stats->rx_nombuf += istats->rx_alloc_failure;
	}
	return 0;
}

static int
otx_ep_dev_close(struct rte_eth_dev *eth_dev)
{
	struct otx_ep_device *otx_epvf;
	uint32_t num_queues, q_no;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		eth_dev->dev_ops = NULL;
		eth_dev->rx_pkt_burst = NULL;
		eth_dev->tx_pkt_burst = NULL;
		return 0;
	}

	otx_epvf = OTX_EP_DEV(eth_dev);
	otx_ep_mbox_send_dev_exit(eth_dev);
	otx_epvf->fn_list.disable_io_queues(otx_epvf);
	num_queues = otx_epvf->nb_rx_queues;
	for (q_no = 0; q_no < num_queues; q_no++) {
		if (otx_ep_delete_oqs(otx_epvf, q_no)) {
			otx_ep_err("Failed to delete OQ:%d\n", q_no);
			return -EINVAL;
		}
	}
	otx_ep_dbg("Num OQs:%d freed\n", otx_epvf->nb_rx_queues);

	num_queues = otx_epvf->nb_tx_queues;
	for (q_no = 0; q_no < num_queues; q_no++) {
		if (otx_ep_delete_iqs(otx_epvf, q_no)) {
			otx_ep_err("Failed to delete IQ:%d\n", q_no);
			return -EINVAL;
		}
	}
	otx_ep_dbg("Num IQs:%d freed\n", otx_epvf->nb_tx_queues);

	if (rte_eth_dma_zone_free(eth_dev, "ism", 0)) {
		otx_ep_err("Failed to delete ISM buffer\n");
		return -EINVAL;
	}

	return 0;
}

static int
otx_ep_dev_get_mac_addr(struct rte_eth_dev *eth_dev,
			struct rte_ether_addr *mac_addr)
{
	int ret;

	ret = otx_ep_mbox_get_mac_addr(eth_dev, mac_addr);
	if (ret)
		return -EINVAL;
	otx_ep_dbg("Get MAC address " RTE_ETHER_ADDR_PRT_FMT "\n",
		    RTE_ETHER_ADDR_BYTES(mac_addr));
	return 0;
}

/* Define our ethernet definitions */
static const struct eth_dev_ops otx_ep_eth_dev_ops = {
	.dev_configure		= otx_ep_dev_configure,
	.dev_start		= otx_ep_dev_start,
	.dev_stop		= otx_ep_dev_stop,
	.rx_queue_setup	        = otx_ep_rx_queue_setup,
	.rx_queue_release	= otx_ep_rx_queue_release,
	.tx_queue_setup	        = otx_ep_tx_queue_setup,
	.tx_queue_release	= otx_ep_tx_queue_release,
	.dev_infos_get		= otx_ep_dev_info_get,
	.stats_get		= otx_ep_dev_stats_get,
	.stats_reset		= otx_ep_dev_stats_reset,
	.link_update		= otx_ep_dev_link_update,
	.dev_close		= otx_ep_dev_close,
	.mtu_set		= otx_ep_dev_mtu_set,
	.mac_addr_set           = otx_ep_dev_set_default_mac_addr,
};

static int
otx_ep_eth_dev_uninit(struct rte_eth_dev *eth_dev)
{
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		eth_dev->dev_ops = NULL;
		eth_dev->rx_pkt_burst = NULL;
		eth_dev->tx_pkt_burst = NULL;
		return 0;
	}

	eth_dev->dev_ops = NULL;
	eth_dev->rx_pkt_burst = NULL;
	eth_dev->tx_pkt_burst = NULL;

	return 0;
}

static int otx_ep_eth_dev_query_set_vf_mac(struct rte_eth_dev *eth_dev,
					   struct rte_ether_addr *mac_addr)
{
	int ret_val;

	memset(mac_addr, 0, sizeof(struct rte_ether_addr));
	ret_val = otx_ep_dev_get_mac_addr(eth_dev, mac_addr);
	if (!ret_val) {
		if (!rte_is_valid_assigned_ether_addr(mac_addr)) {
			otx_ep_dbg("PF doesn't have valid VF MAC addr" RTE_ETHER_ADDR_PRT_FMT "\n",
				    RTE_ETHER_ADDR_BYTES(mac_addr));
			rte_eth_random_addr(mac_addr->addr_bytes);
			otx_ep_dbg("Setting Random MAC address" RTE_ETHER_ADDR_PRT_FMT "\n",
				    RTE_ETHER_ADDR_BYTES(mac_addr));
			ret_val = otx_ep_dev_set_default_mac_addr(eth_dev, mac_addr);
			if (ret_val) {
				otx_ep_err("Setting MAC address " RTE_ETHER_ADDR_PRT_FMT "fails\n",
					    RTE_ETHER_ADDR_BYTES(mac_addr));
				return ret_val;
			}
		}
		otx_ep_dbg("Received valid MAC addr from PF" RTE_ETHER_ADDR_PRT_FMT "\n",
			    RTE_ETHER_ADDR_BYTES(mac_addr));
	} else {
		otx_ep_err("Getting MAC address from PF via Mbox fails with ret_val: %d\n",
			    ret_val);
		return ret_val;
	}
	return 0;
}

static int
otx_ep_eth_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pdev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct otx_ep_device *otx_epvf = OTX_EP_DEV(eth_dev);
	struct rte_ether_addr vf_mac_addr;

	/* Single process support */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		eth_dev->dev_ops = &otx_ep_eth_dev_ops;
		otx_ep_set_tx_func(eth_dev);
		otx_ep_set_rx_func(eth_dev);
		return 0;
	}

	rte_eth_copy_pci_info(eth_dev, pdev);
	otx_epvf->eth_dev = eth_dev;
	otx_epvf->port_id = eth_dev->data->port_id;
	eth_dev->dev_ops = &otx_ep_eth_dev_ops;
	rte_spinlock_init(&otx_epvf->mbox_lock);

	/*
	 * Initialize negotiated Mbox version to base version of VF Mbox
	 * This will address working legacy PF with latest VF.
	 */
	otx_epvf->mbox_neg_ver = OTX_EP_MBOX_VERSION_V1;
	eth_dev->data->mac_addrs = rte_zmalloc("otx_ep", RTE_ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		otx_ep_err("MAC addresses memory allocation failed\n");
		eth_dev->dev_ops = NULL;
		return -ENOMEM;
	}
	rte_eth_random_addr(vf_mac_addr.addr_bytes);
	rte_ether_addr_copy(&vf_mac_addr, eth_dev->data->mac_addrs);
	otx_epvf->hw_addr = pdev->mem_resource[0].addr;
	otx_epvf->pdev = pdev;

	if (otx_epdev_init(otx_epvf))
		return -ENOMEM;
	if (otx_epvf->chip_id == PCI_DEVID_CN9K_EP_NET_VF ||
	    otx_epvf->chip_id == PCI_DEVID_CN98XX_EP_NET_VF ||
	    otx_epvf->chip_id == PCI_DEVID_CNF95N_EP_NET_VF ||
	    otx_epvf->chip_id == PCI_DEVID_CNF95O_EP_NET_VF ||
	    otx_epvf->chip_id == PCI_DEVID_CN10KA_EP_NET_VF ||
	    otx_epvf->chip_id == PCI_DEVID_CN10KB_EP_NET_VF ||
	    otx_epvf->chip_id == PCI_DEVID_CNF10KA_EP_NET_VF ||
	    otx_epvf->chip_id == PCI_DEVID_CNF10KB_EP_NET_VF) {
		otx_epvf->pkind = SDP_OTX2_PKIND_FS0;
		otx_ep_info("using pkind %d\n", otx_epvf->pkind);
	} else if (otx_epvf->chip_id == PCI_DEVID_OCTEONTX_EP_VF) {
		otx_epvf->pkind = SDP_PKIND;
		otx_ep_info("Using pkind %d.\n", otx_epvf->pkind);
	} else {
		otx_ep_err("Invalid chip id\n");
		return -EINVAL;
	}

	if (otx_ep_mbox_version_check(eth_dev))
		return -EINVAL;

	if (otx_ep_eth_dev_query_set_vf_mac(eth_dev,
				(struct rte_ether_addr *)&vf_mac_addr)) {
		otx_ep_err("set mac addr failed\n");
		return -ENODEV;
	}
	rte_ether_addr_copy(&vf_mac_addr, eth_dev->data->mac_addrs);

	return 0;
}

static int
otx_ep_eth_dev_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		      struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
					     sizeof(struct otx_ep_device),
					     otx_ep_eth_dev_init);
}

static int
otx_ep_eth_dev_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev,
					      otx_ep_eth_dev_uninit);
}

/* Set of PCI devices this driver supports */
static const struct rte_pci_id pci_id_otx_ep_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_OCTEONTX_EP_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_CN9K_EP_NET_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_CN98XX_EP_NET_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_CNF95N_EP_NET_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_CNF95O_EP_NET_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_CN10KA_EP_NET_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_CN10KB_EP_NET_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_CNF10KA_EP_NET_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_CNF10KB_EP_NET_VF) },
	{ .vendor_id = 0, /* sentinel */ }
};

static struct rte_pci_driver rte_otx_ep_pmd = {
	.id_table	= pci_id_otx_ep_map,
	.drv_flags      = RTE_PCI_DRV_NEED_MAPPING,
	.probe		= otx_ep_eth_dev_pci_probe,
	.remove		= otx_ep_eth_dev_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_otx_ep, rte_otx_ep_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_otx_ep, pci_id_otx_ep_map);
RTE_PMD_REGISTER_KMOD_DEP(net_otx_ep, "* igb_uio | vfio-pci");
RTE_LOG_REGISTER_DEFAULT(otx_net_ep_logtype, NOTICE);
