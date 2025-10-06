/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020-2021 NXP
 */

#include <inttypes.h>

#include <ethdev_vdev.h>
#include <ethdev_driver.h>
#include <rte_bitops.h>
#include <rte_io.h>

#include "enet_pmd_logs.h"
#include "enet_ethdev.h"
#include "enet_regs.h"
#include "enet_uio.h"

#define ENETFEC_NAME_PMD                net_enetfec

/* FEC receive acceleration */
#define ENETFEC_RACC_IPDIS		RTE_BIT32(1)
#define ENETFEC_RACC_PRODIS		RTE_BIT32(2)
#define ENETFEC_RACC_SHIFT16		RTE_BIT32(7)
#define ENETFEC_RACC_OPTIONS		(ENETFEC_RACC_IPDIS | \
						ENETFEC_RACC_PRODIS)

#define ENETFEC_PAUSE_FLAG_AUTONEG	0x1
#define ENETFEC_PAUSE_FLAG_ENABLE	0x2

/* Pause frame field and FIFO threshold */
#define ENETFEC_FCE			RTE_BIT32(5)
#define ENETFEC_RSEM_V			0x84
#define ENETFEC_RSFL_V			16
#define ENETFEC_RAEM_V			0x8
#define ENETFEC_RAFL_V			0x8
#define ENETFEC_OPD_V			0xFFF0

/* Extended buffer descriptor */
#define ENETFEC_EXTENDED_BD		0
#define NUM_OF_BD_QUEUES		6

/* Supported Rx offloads */
static uint64_t dev_rx_offloads_sup =
		RTE_ETH_RX_OFFLOAD_CHECKSUM |
		RTE_ETH_RX_OFFLOAD_VLAN;

/*
 * This function is called to start or restart the ENETFEC during a link
 * change, transmit timeout, or to reconfigure the ENETFEC. The network
 * packet processing for this device must be stopped before this call.
 */
static void
enetfec_restart(struct rte_eth_dev *dev)
{
	struct enetfec_private *fep = dev->data->dev_private;
	uint32_t rcntl = OPT_FRAME_SIZE | 0x04;
	uint32_t ecntl = ENETFEC_ETHEREN;
	uint32_t val;
	int i;

	/* Clear any outstanding interrupt. */
	writel(0xffffffff, (uint8_t *)fep->hw_baseaddr_v + ENETFEC_EIR);

	/* Enable MII mode */
	if (fep->full_duplex == FULL_DUPLEX) {
		/* FD enable */
		rte_write32(rte_cpu_to_le_32(0x04),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_TCR);
	} else {
	/* No Rcv on Xmit */
		rcntl |= 0x02;
		rte_write32(0, (uint8_t *)fep->hw_baseaddr_v + ENETFEC_TCR);
	}

	if (fep->quirks & QUIRK_RACC) {
		val = rte_read32((uint8_t *)fep->hw_baseaddr_v + ENETFEC_RACC);
		/* align IP header */
		val |= ENETFEC_RACC_SHIFT16;
		if (fep->flag_csum & RX_FLAG_CSUM_EN)
			/* set RX checksum */
			val |= ENETFEC_RACC_OPTIONS;
		else
			val &= ~ENETFEC_RACC_OPTIONS;
		rte_write32(rte_cpu_to_le_32(val),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_RACC);
		rte_write32(rte_cpu_to_le_32(PKT_MAX_BUF_SIZE),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_FRAME_TRL);
	}

	/*
	 * The phy interface and speed need to get configured
	 * differently on enet-mac.
	 */
	if (fep->quirks & QUIRK_HAS_ENETFEC_MAC) {
		/* Enable flow control and length check */
		rcntl |= 0x40000000 | 0x00000020;

		/* RGMII, RMII or MII */
		rcntl |= RTE_BIT32(6);
		ecntl |= RTE_BIT32(5);
	}

	/* enable pause frame*/
	if ((fep->flag_pause & ENETFEC_PAUSE_FLAG_ENABLE) ||
		((fep->flag_pause & ENETFEC_PAUSE_FLAG_AUTONEG)
		/*&& ndev->phydev && ndev->phydev->pause*/)) {
		rcntl |= ENETFEC_FCE;

		/* set FIFO threshold parameter to reduce overrun */
		rte_write32(rte_cpu_to_le_32(ENETFEC_RSEM_V),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_R_FIFO_SEM);
		rte_write32(rte_cpu_to_le_32(ENETFEC_RSFL_V),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_R_FIFO_SFL);
		rte_write32(rte_cpu_to_le_32(ENETFEC_RAEM_V),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_R_FIFO_AEM);
		rte_write32(rte_cpu_to_le_32(ENETFEC_RAFL_V),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_R_FIFO_AFL);

		/* OPD */
		rte_write32(rte_cpu_to_le_32(ENETFEC_OPD_V),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_OPD);
	} else {
		rcntl &= ~ENETFEC_FCE;
	}

	rte_write32(rte_cpu_to_le_32(rcntl),
		(uint8_t *)fep->hw_baseaddr_v + ENETFEC_RCR);

	rte_write32(0, (uint8_t *)fep->hw_baseaddr_v + ENETFEC_IAUR);
	rte_write32(0, (uint8_t *)fep->hw_baseaddr_v + ENETFEC_IALR);

	if (fep->quirks & QUIRK_HAS_ENETFEC_MAC) {
		/* enable ENETFEC endian swap */
		ecntl |= (1 << 8);
		/* enable ENETFEC store and forward mode */
		rte_write32(rte_cpu_to_le_32(1 << 8),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_TFWR);
	}
	if (fep->bufdesc_ex)
		ecntl |= (1 << 4);
	if (fep->quirks & QUIRK_SUPPORT_DELAYED_CLKS &&
		fep->rgmii_txc_delay)
		ecntl |= ENETFEC_TXC_DLY;
	if (fep->quirks & QUIRK_SUPPORT_DELAYED_CLKS &&
		fep->rgmii_rxc_delay)
		ecntl |= ENETFEC_RXC_DLY;
	/* Enable the MIB statistic event counters */
	rte_write32(0, (uint8_t *)fep->hw_baseaddr_v + ENETFEC_MIBC);

	ecntl |= 0x70000000;
	fep->enetfec_e_cntl = ecntl;
	/* And last, enable the transmit and receive processing */
	rte_write32(rte_cpu_to_le_32(ecntl),
		(uint8_t *)fep->hw_baseaddr_v + ENETFEC_ECR);

	for (i = 0; i < fep->max_rx_queues; i++)
		rte_write32(0, fep->rx_queues[i]->bd.active_reg_desc);
	rte_delay_us(10);
}

static void
enet_free_buffers(struct rte_eth_dev *dev)
{
	struct enetfec_private *fep = dev->data->dev_private;
	unsigned int i, q;
	struct rte_mbuf *mbuf;
	struct bufdesc  *bdp;
	struct enetfec_priv_rx_q *rxq;
	struct enetfec_priv_tx_q *txq;

	for (q = 0; q < dev->data->nb_rx_queues; q++) {
		rxq = fep->rx_queues[q];
		bdp = rxq->bd.base;
		for (i = 0; i < rxq->bd.ring_size; i++) {
			mbuf = rxq->rx_mbuf[i];
			rxq->rx_mbuf[i] = NULL;
			rte_pktmbuf_free(mbuf);
			bdp = enet_get_nextdesc(bdp, &rxq->bd);
		}
	}

	for (q = 0; q < dev->data->nb_tx_queues; q++) {
		txq = fep->tx_queues[q];
		bdp = txq->bd.base;
		for (i = 0; i < txq->bd.ring_size; i++) {
			mbuf = txq->tx_mbuf[i];
			txq->tx_mbuf[i] = NULL;
			rte_pktmbuf_free(mbuf);
		}
	}
}

static int
enetfec_eth_configure(struct rte_eth_dev *dev)
{
	struct enetfec_private *fep = dev->data->dev_private;

	if (dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_CHECKSUM)
		fep->flag_csum |= RX_FLAG_CSUM_EN;

	if (dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
		ENETFEC_PMD_ERR("PMD does not support KEEP_CRC offload");

	return 0;
}

static int
enetfec_eth_start(struct rte_eth_dev *dev)
{
	enetfec_restart(dev);
	dev->rx_pkt_burst = &enetfec_recv_pkts;
	dev->tx_pkt_burst = &enetfec_xmit_pkts;

	return 0;
}

/* ENETFEC disable function.
 * @param[in] base      ENETFEC base address
 */
static void
enetfec_disable(struct enetfec_private *fep)
{
	rte_write32(rte_read32((uint8_t *)fep->hw_baseaddr_v + ENETFEC_ECR)
		    & ~(fep->enetfec_e_cntl),
		    (uint8_t *)fep->hw_baseaddr_v + ENETFEC_ECR);
}

static int
enetfec_eth_stop(struct rte_eth_dev *dev)
{
	struct enetfec_private *fep = dev->data->dev_private;

	dev->data->dev_started = 0;
	enetfec_disable(fep);

	return 0;
}

static int
enetfec_eth_close(struct rte_eth_dev *dev)
{
	enet_free_buffers(dev);
	return 0;
}

static int
enetfec_eth_link_update(struct rte_eth_dev *dev,
			int wait_to_complete __rte_unused)
{
	struct rte_eth_link link;
	unsigned int lstatus = 1;

	memset(&link, 0, sizeof(struct rte_eth_link));

	link.link_status = lstatus;
	link.link_speed = RTE_ETH_SPEED_NUM_1G;

	ENETFEC_PMD_INFO("Port (%d) link is %s", dev->data->port_id,
			 "Up");

	return rte_eth_linkstatus_set(dev, &link);
}

static int
enetfec_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct enetfec_private *fep = dev->data->dev_private;
	uint32_t tmp;

	tmp = rte_read32((uint8_t *)fep->hw_baseaddr_v + ENETFEC_RCR);
	tmp |= 0x8;
	tmp &= ~0x2;
	rte_write32(rte_cpu_to_le_32(tmp),
		(uint8_t *)fep->hw_baseaddr_v + ENETFEC_RCR);

	return 0;
}

static int
enetfec_multicast_enable(struct rte_eth_dev *dev)
{
	struct enetfec_private *fep = dev->data->dev_private;

	rte_write32(rte_cpu_to_le_32(0xffffffff),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_GAUR);
	rte_write32(rte_cpu_to_le_32(0xffffffff),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_GALR);
	dev->data->all_multicast = 1;

	rte_write32(rte_cpu_to_le_32(0x04400002),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_GAUR);
	rte_write32(rte_cpu_to_le_32(0x10800049),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_GALR);

	return 0;
}

/* Set a MAC change in hardware. */
static int
enetfec_set_mac_address(struct rte_eth_dev *dev,
		    struct rte_ether_addr *addr)
{
	struct enetfec_private *fep = dev->data->dev_private;

	writel(addr->addr_bytes[3] | (addr->addr_bytes[2] << 8) |
		(addr->addr_bytes[1] << 16) | (addr->addr_bytes[0] << 24),
		(uint8_t *)fep->hw_baseaddr_v + ENETFEC_PALR);
	writel((addr->addr_bytes[5] << 16) | (addr->addr_bytes[4] << 24),
		(uint8_t *)fep->hw_baseaddr_v + ENETFEC_PAUR);

	rte_ether_addr_copy(addr, &dev->data->mac_addrs[0]);

	return 0;
}

static int
enetfec_stats_get(struct rte_eth_dev *dev,
	      struct rte_eth_stats *stats)
{
	struct enetfec_private *fep = dev->data->dev_private;
	struct rte_eth_stats *eth_stats = &fep->stats;

	stats->ipackets = eth_stats->ipackets;
	stats->ibytes = eth_stats->ibytes;
	stats->ierrors = eth_stats->ierrors;
	stats->opackets = eth_stats->opackets;
	stats->obytes = eth_stats->obytes;
	stats->oerrors = eth_stats->oerrors;
	stats->rx_nombuf = eth_stats->rx_nombuf;

	return 0;
}

static int
enetfec_eth_info(__rte_unused struct rte_eth_dev *dev,
	struct rte_eth_dev_info *dev_info)
{
	dev_info->max_rx_pktlen = ENETFEC_MAX_RX_PKT_LEN;
	dev_info->max_rx_queues = ENETFEC_MAX_Q;
	dev_info->max_tx_queues = ENETFEC_MAX_Q;
	dev_info->rx_offload_capa = dev_rx_offloads_sup;
	return 0;
}

static void
enet_free_queue(struct rte_eth_dev *dev)
{
	struct enetfec_private *fep = dev->data->dev_private;
	unsigned int i;

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		rte_free(fep->rx_queues[i]);
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		rte_free(fep->rx_queues[i]);
}

static const unsigned short offset_des_active_rxq[] = {
	ENETFEC_RDAR_0, ENETFEC_RDAR_1, ENETFEC_RDAR_2
};

static const unsigned short offset_des_active_txq[] = {
	ENETFEC_TDAR_0, ENETFEC_TDAR_1, ENETFEC_TDAR_2
};

static int
enetfec_tx_queue_setup(struct rte_eth_dev *dev,
			uint16_t queue_idx,
			uint16_t nb_desc,
			unsigned int socket_id __rte_unused,
			const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct enetfec_private *fep = dev->data->dev_private;
	unsigned int i;
	struct bufdesc *bdp, *bd_base;
	struct enetfec_priv_tx_q *txq;
	unsigned int size;
	unsigned int dsize = fep->bufdesc_ex ? sizeof(struct bufdesc_ex) :
		sizeof(struct bufdesc);
	unsigned int dsize_log2 = rte_fls_u64(dsize) - 1;

	/* allocate transmit queue */
	txq = rte_zmalloc(NULL, sizeof(*txq), RTE_CACHE_LINE_SIZE);
	if (txq == NULL) {
		ENETFEC_PMD_ERR("transmit queue allocation failed");
		return -ENOMEM;
	}

	if (nb_desc != MAX_TX_BD_RING_SIZE) {
		nb_desc = MAX_TX_BD_RING_SIZE;
		ENETFEC_PMD_WARN("modified the nb_desc to MAX_TX_BD_RING_SIZE");
	}
	txq->bd.ring_size = nb_desc;
	fep->total_tx_ring_size += txq->bd.ring_size;
	fep->tx_queues[queue_idx] = txq;

	rte_write32(rte_cpu_to_le_32(fep->bd_addr_p_t[queue_idx]),
		(uint8_t *)fep->hw_baseaddr_v + ENETFEC_TD_START(queue_idx));

	/* Set transmit descriptor base. */
	txq = fep->tx_queues[queue_idx];
	txq->fep = fep;
	size = dsize * txq->bd.ring_size;
	bd_base = (struct bufdesc *)fep->dma_baseaddr_t[queue_idx];
	txq->bd.queue_id = queue_idx;
	txq->bd.base = bd_base;
	txq->bd.cur = bd_base;
	txq->bd.d_size = dsize;
	txq->bd.d_size_log2 = dsize_log2;
	txq->bd.active_reg_desc = (uint8_t *)fep->hw_baseaddr_v +
			offset_des_active_txq[queue_idx];
	bd_base = (struct bufdesc *)(((uintptr_t)bd_base) + size);
	txq->bd.last = (struct bufdesc *)(((uintptr_t)bd_base) - dsize);
	bdp = txq->bd.cur;

	for (i = 0; i < txq->bd.ring_size; i++) {
		/* Initialize the BD for every fragment in the page. */
		rte_write16(rte_cpu_to_le_16(0), &bdp->bd_sc);
		if (txq->tx_mbuf[i] != NULL) {
			rte_pktmbuf_free(txq->tx_mbuf[i]);
			txq->tx_mbuf[i] = NULL;
		}
		rte_write32(0, &bdp->bd_bufaddr);
		bdp = enet_get_nextdesc(bdp, &txq->bd);
	}

	/* Set the last buffer to wrap */
	bdp = enet_get_prevdesc(bdp, &txq->bd);
	rte_write16((rte_cpu_to_le_16(TX_BD_WRAP) |
		rte_read16(&bdp->bd_sc)), &bdp->bd_sc);
	txq->dirty_tx = bdp;
	dev->data->tx_queues[queue_idx] = fep->tx_queues[queue_idx];
	return 0;
}

static int
enetfec_rx_queue_setup(struct rte_eth_dev *dev,
			uint16_t queue_idx,
			uint16_t nb_rx_desc,
			unsigned int socket_id __rte_unused,
			const struct rte_eth_rxconf *rx_conf __rte_unused,
			struct rte_mempool *mb_pool)
{
	struct enetfec_private *fep = dev->data->dev_private;
	unsigned int i;
	struct bufdesc *bd_base;
	struct bufdesc *bdp;
	struct enetfec_priv_rx_q *rxq;
	unsigned int size;
	unsigned int dsize = fep->bufdesc_ex ? sizeof(struct bufdesc_ex) :
			sizeof(struct bufdesc);
	unsigned int dsize_log2 = rte_fls_u64(dsize) - 1;

	if (queue_idx >= ENETFEC_MAX_Q) {
		ENETFEC_PMD_ERR("Invalid queue id %" PRIu16 ", max %d",
			queue_idx, ENETFEC_MAX_Q);
		return -EINVAL;
	}

	/* allocate receive queue */
	rxq = rte_zmalloc(NULL, sizeof(*rxq), RTE_CACHE_LINE_SIZE);
	if (rxq == NULL) {
		ENETFEC_PMD_ERR("receive queue allocation failed");
		return -ENOMEM;
	}

	if (nb_rx_desc != MAX_RX_BD_RING_SIZE) {
		nb_rx_desc = MAX_RX_BD_RING_SIZE;
		ENETFEC_PMD_WARN("modified the nb_desc to MAX_RX_BD_RING_SIZE");
	}

	rxq->bd.ring_size = nb_rx_desc;
	fep->total_rx_ring_size += rxq->bd.ring_size;
	fep->rx_queues[queue_idx] = rxq;

	rte_write32(rte_cpu_to_le_32(fep->bd_addr_p_r[queue_idx]),
		(uint8_t *)fep->hw_baseaddr_v + ENETFEC_RD_START(queue_idx));
	rte_write32(rte_cpu_to_le_32(PKT_MAX_BUF_SIZE),
		(uint8_t *)fep->hw_baseaddr_v + ENETFEC_MRB_SIZE(queue_idx));

	/* Set receive descriptor base. */
	rxq = fep->rx_queues[queue_idx];
	rxq->pool = mb_pool;
	size = dsize * rxq->bd.ring_size;
	bd_base = (struct bufdesc *)fep->dma_baseaddr_r[queue_idx];
	rxq->bd.queue_id = queue_idx;
	rxq->bd.base = bd_base;
	rxq->bd.cur = bd_base;
	rxq->bd.d_size = dsize;
	rxq->bd.d_size_log2 = dsize_log2;
	rxq->bd.active_reg_desc = (uint8_t *)fep->hw_baseaddr_v +
			offset_des_active_rxq[queue_idx];
	bd_base = (struct bufdesc *)(((uintptr_t)bd_base) + size);
	rxq->bd.last = (struct bufdesc *)(((uintptr_t)bd_base) - dsize);

	rxq->fep = fep;
	bdp = rxq->bd.base;
	rxq->bd.cur = bdp;

	for (i = 0; i < nb_rx_desc; i++) {
		/* Initialize Rx buffers from pktmbuf pool */
		struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mb_pool);
		if (mbuf == NULL) {
			ENETFEC_PMD_ERR("mbuf failed");
			goto err_alloc;
		}

		/* Get the virtual address & physical address */
		rte_write32(rte_cpu_to_le_32(rte_pktmbuf_iova(mbuf)),
			&bdp->bd_bufaddr);

		rxq->rx_mbuf[i] = mbuf;
		rte_write16(rte_cpu_to_le_16(RX_BD_EMPTY), &bdp->bd_sc);

		bdp = enet_get_nextdesc(bdp, &rxq->bd);
	}

	/* Initialize the receive buffer descriptors. */
	bdp = rxq->bd.cur;
	for (i = 0; i < rxq->bd.ring_size; i++) {
		/* Initialize the BD for every fragment in the page. */
		if (rte_read32(&bdp->bd_bufaddr) > 0)
			rte_write16(rte_cpu_to_le_16(RX_BD_EMPTY),
				&bdp->bd_sc);
		else
			rte_write16(rte_cpu_to_le_16(0), &bdp->bd_sc);

		bdp = enet_get_nextdesc(bdp, &rxq->bd);
	}

	/* Set the last buffer to wrap */
	bdp = enet_get_prevdesc(bdp, &rxq->bd);
	rte_write16((rte_cpu_to_le_16(RX_BD_WRAP) |
		rte_read16(&bdp->bd_sc)),  &bdp->bd_sc);
	dev->data->rx_queues[queue_idx] = fep->rx_queues[queue_idx];
	rte_write32(0, fep->rx_queues[queue_idx]->bd.active_reg_desc);
	return 0;

err_alloc:
	for (i = 0; i < nb_rx_desc; i++) {
		if (rxq->rx_mbuf[i] != NULL) {
			rte_pktmbuf_free(rxq->rx_mbuf[i]);
			rxq->rx_mbuf[i] = NULL;
		}
	}
	rte_free(rxq);
	return errno;
}

static const struct eth_dev_ops enetfec_ops = {
	.dev_configure          = enetfec_eth_configure,
	.dev_start              = enetfec_eth_start,
	.dev_stop               = enetfec_eth_stop,
	.dev_close              = enetfec_eth_close,
	.link_update            = enetfec_eth_link_update,
	.promiscuous_enable     = enetfec_promiscuous_enable,
	.allmulticast_enable    = enetfec_multicast_enable,
	.mac_addr_set           = enetfec_set_mac_address,
	.stats_get              = enetfec_stats_get,
	.dev_infos_get          = enetfec_eth_info,
	.rx_queue_setup         = enetfec_rx_queue_setup,
	.tx_queue_setup         = enetfec_tx_queue_setup
};

static int
enetfec_eth_init(struct rte_eth_dev *dev)
{
	struct enetfec_private *fep = dev->data->dev_private;

	fep->full_duplex = FULL_DUPLEX;
	dev->dev_ops = &enetfec_ops;
	rte_eth_dev_probing_finish(dev);

	return 0;
}

static int
pmd_enetfec_probe(struct rte_vdev_device *vdev)
{
	struct rte_eth_dev *dev = NULL;
	struct enetfec_private *fep;
	const char *name;
	int rc;
	int i;
	unsigned int bdsize;
	struct rte_ether_addr macaddr = {
		.addr_bytes = { 0x1, 0x1, 0x1, 0x1, 0x1, 0x1 }
	};

	name = rte_vdev_device_name(vdev);
	ENETFEC_PMD_LOG(INFO, "Initializing pmd_fec for %s", name);

	dev = rte_eth_vdev_allocate(vdev, sizeof(*fep));
	if (dev == NULL)
		return -ENOMEM;

	/* setup board info structure */
	fep = dev->data->dev_private;
	fep->dev = dev;

	fep->max_rx_queues = ENETFEC_MAX_Q;
	fep->max_tx_queues = ENETFEC_MAX_Q;
	fep->quirks = QUIRK_HAS_ENETFEC_MAC | QUIRK_GBIT
		| QUIRK_RACC;

	rc = enetfec_configure();
	if (rc != 0)
		return -ENOMEM;
	rc = config_enetfec_uio(fep);
	if (rc != 0)
		return -ENOMEM;

	/* Get the BD size for distributing among six queues */
	bdsize = (fep->bd_size) / NUM_OF_BD_QUEUES;

	for (i = 0; i < fep->max_tx_queues; i++) {
		fep->dma_baseaddr_t[i] = fep->bd_addr_v;
		fep->bd_addr_p_t[i] = fep->bd_addr_p;
		fep->bd_addr_v = (uint8_t *)fep->bd_addr_v + bdsize;
		fep->bd_addr_p = fep->bd_addr_p + bdsize;
	}
	for (i = 0; i < fep->max_rx_queues; i++) {
		fep->dma_baseaddr_r[i] = fep->bd_addr_v;
		fep->bd_addr_p_r[i] = fep->bd_addr_p;
		fep->bd_addr_v = (uint8_t *)fep->bd_addr_v + bdsize;
		fep->bd_addr_p = fep->bd_addr_p + bdsize;
	}

	/* Copy the station address into the dev structure, */
	dev->data->mac_addrs = rte_zmalloc("mac_addr", RTE_ETHER_ADDR_LEN, 0);
	if (dev->data->mac_addrs == NULL) {
		ENETFEC_PMD_ERR("Failed to allocate mem %d to store MAC addresses",
			RTE_ETHER_ADDR_LEN);
		rc = -ENOMEM;
		goto err;
	}

	/*
	 * Set default mac address
	 */
	enetfec_set_mac_address(dev, &macaddr);

	fep->bufdesc_ex = ENETFEC_EXTENDED_BD;
	rc = enetfec_eth_init(dev);
	if (rc)
		goto failed_init;

	return 0;

failed_init:
	ENETFEC_PMD_ERR("Failed to init");
err:
	rte_eth_dev_release_port(dev);
	return rc;
}

static int
pmd_enetfec_remove(struct rte_vdev_device *vdev)
{
	struct rte_eth_dev *eth_dev = NULL;
	struct enetfec_private *fep;
	struct enetfec_priv_rx_q *rxq;
	int ret;

	/* find the ethdev entry */
	eth_dev = rte_eth_dev_allocated(rte_vdev_device_name(vdev));
	if (eth_dev == NULL)
		return -ENODEV;

	fep = eth_dev->data->dev_private;
	/* Free descriptor base of first RX queue as it was configured
	 * first in enetfec_eth_init().
	 */
	rxq = fep->rx_queues[0];
	rte_free(rxq->bd.base);
	enet_free_queue(eth_dev);
	enetfec_eth_stop(eth_dev);

	ret = rte_eth_dev_release_port(eth_dev);
	if (ret != 0)
		return -EINVAL;

	ENETFEC_PMD_INFO("Release enetfec sw device");
	enetfec_cleanup(fep);

	return 0;
}

static struct rte_vdev_driver pmd_enetfec_drv = {
	.probe = pmd_enetfec_probe,
	.remove = pmd_enetfec_remove,
};

RTE_PMD_REGISTER_VDEV(ENETFEC_NAME_PMD, pmd_enetfec_drv);
RTE_LOG_REGISTER_DEFAULT(enetfec_logtype_pmd, NOTICE);
