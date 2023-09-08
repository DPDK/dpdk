/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 NXP
 */

#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <rte_kvargs.h>
#include <ethdev_vdev.h>
#include <bus_vdev_driver.h>
#include <rte_ether.h>
#include <dpaa_of.h>

#include "pfe_logs.h"
#include "pfe_mod.h"

#define PFE_MAX_MACS 1 /* we can support up to 4 MACs per IF */
#define PFE_VDEV_GEM_ID_ARG	"intf"

struct pfe_vdev_init_params {
	int8_t	gem_id;
};
static struct pfe *g_pfe;
/* Supported Rx offloads */
static uint64_t dev_rx_offloads_sup =
		RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
		RTE_ETH_RX_OFFLOAD_TCP_CKSUM;

/* Supported Tx offloads */
static uint64_t dev_tx_offloads_sup =
		RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
		RTE_ETH_TX_OFFLOAD_TCP_CKSUM;

/* TODO: make pfe_svr a runtime option.
 * Driver should be able to get the SVR
 * information from HW.
 */
unsigned int pfe_svr = SVR_LS1012A_REV1;
static void *cbus_emac_base[3];
static void *cbus_gpi_base[3];

/* pfe_gemac_init
 */
static int
pfe_gemac_init(struct pfe_eth_priv_s *priv)
{
	struct gemac_cfg cfg;

	cfg.speed = SPEED_1000M;
	cfg.duplex = DUPLEX_FULL;

	gemac_set_config(priv->EMAC_baseaddr, &cfg);
	gemac_allow_broadcast(priv->EMAC_baseaddr);
	gemac_enable_1536_rx(priv->EMAC_baseaddr);
	gemac_enable_stacked_vlan(priv->EMAC_baseaddr);
	gemac_enable_pause_rx(priv->EMAC_baseaddr);
	gemac_set_bus_width(priv->EMAC_baseaddr, 64);
	gemac_enable_rx_checksum_offload(priv->EMAC_baseaddr);

	return 0;
}

static void
pfe_soc_version_get(void)
{
	FILE *svr_file = NULL;
	unsigned int svr_ver = 0;

	PMD_INIT_FUNC_TRACE();

	svr_file = fopen(PFE_SOC_ID_FILE, "r");
	if (!svr_file) {
		PFE_PMD_ERR("Unable to open SoC device");
		return; /* Not supported on this infra */
	}

	if (fscanf(svr_file, "svr:%x", &svr_ver) > 0)
		pfe_svr = svr_ver;
	else
		PFE_PMD_ERR("Unable to read SoC device");

	fclose(svr_file);
}

static int pfe_eth_start(struct pfe_eth_priv_s *priv)
{
	gpi_enable(priv->GPI_baseaddr);
	gemac_enable(priv->EMAC_baseaddr);

	return 0;
}

static void
pfe_eth_flush_txQ(struct pfe_eth_priv_s *priv, int tx_q_num, int
		  __rte_unused from_tx, __rte_unused int n_desc)
{
	struct rte_mbuf *mbuf;
	unsigned int flags;

	/* Clean HIF and client queue */
	while ((mbuf = hif_lib_tx_get_next_complete(&priv->client,
						   tx_q_num, &flags,
						   HIF_TX_DESC_NT))) {
		if (mbuf) {
			mbuf->next = NULL;
			mbuf->nb_segs = 1;
			rte_pktmbuf_free(mbuf);
		}
	}
}


static void
pfe_eth_flush_tx(struct pfe_eth_priv_s *priv)
{
	unsigned int ii;

	for (ii = 0; ii < emac_txq_cnt; ii++)
		pfe_eth_flush_txQ(priv, ii, 0, 0);
}

static int
pfe_eth_event_handler(void *data, int event, __rte_unused int qno)
{
	struct pfe_eth_priv_s *priv = data;

	switch (event) {
	case EVENT_TXDONE_IND:
		pfe_eth_flush_tx(priv);
		hif_lib_event_handler_start(&priv->client, EVENT_TXDONE_IND, 0);
		break;
	case EVENT_HIGH_RX_WM:
	default:
		break;
	}

	return 0;
}

static uint16_t
pfe_recv_pkts_on_intr(void *rxq, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct hif_client_rx_queue *queue = rxq;
	struct pfe_eth_priv_s *priv = queue->priv;
	struct epoll_event epoll_ev;
	uint64_t ticks = 1;  /* 1 msec */
	int ret;
	int have_something, work_done;

#define RESET_STATUS (HIF_INT | HIF_RXPKT_INT)

	/*TODO can we remove this cleanup from here?*/
	pfe_tx_do_cleanup(priv->pfe);
	have_something = pfe_hif_rx_process(priv->pfe, nb_pkts);
	work_done = hif_lib_receive_pkt(rxq, priv->pfe->hif.shm->pool,
			rx_pkts, nb_pkts);

	if (!have_something || !work_done) {
		writel(RESET_STATUS, HIF_INT_SRC);
		writel(readl(HIF_INT_ENABLE) | HIF_RXPKT_INT, HIF_INT_ENABLE);
		ret = epoll_wait(priv->pfe->hif.epoll_fd, &epoll_ev, 1, ticks);
		if (ret < 0 && errno != EINTR)
			PFE_PMD_ERR("epoll_wait fails with %d\n", errno);
	}

	return work_done;
}

static uint16_t
pfe_recv_pkts(void *rxq, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct hif_client_rx_queue *queue = rxq;
	struct pfe_eth_priv_s *priv = queue->priv;
	struct rte_mempool *pool;

	/*TODO can we remove this cleanup from here?*/
	pfe_tx_do_cleanup(priv->pfe);
	pfe_hif_rx_process(priv->pfe, nb_pkts);
	pool = priv->pfe->hif.shm->pool;

	return hif_lib_receive_pkt(rxq, pool, rx_pkts, nb_pkts);
}

static uint16_t
pfe_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct hif_client_tx_queue *queue = tx_queue;
	struct pfe_eth_priv_s *priv = queue->priv;
	struct rte_eth_stats *stats = &priv->stats;
	int i;

	for (i = 0; i < nb_pkts; i++) {
		if (tx_pkts[i]->nb_segs > 1) {
			struct rte_mbuf *mbuf;
			int j;

			hif_lib_xmit_pkt(&priv->client, queue->queue_id,
				(void *)(size_t)rte_pktmbuf_iova(tx_pkts[i]),
				tx_pkts[i]->buf_addr + tx_pkts[i]->data_off,
				tx_pkts[i]->data_len, 0x0, HIF_FIRST_BUFFER,
				tx_pkts[i]);

			mbuf = tx_pkts[i]->next;
			for (j = 0; j < (tx_pkts[i]->nb_segs - 2); j++) {
				hif_lib_xmit_pkt(&priv->client, queue->queue_id,
					(void *)(size_t)rte_pktmbuf_iova(mbuf),
					mbuf->buf_addr + mbuf->data_off,
					mbuf->data_len,
					0x0, 0x0, mbuf);
				mbuf = mbuf->next;
			}

			hif_lib_xmit_pkt(&priv->client, queue->queue_id,
					(void *)(size_t)rte_pktmbuf_iova(mbuf),
					mbuf->buf_addr + mbuf->data_off,
					mbuf->data_len,
					0x0, HIF_LAST_BUFFER | HIF_DATA_VALID,
					mbuf);
		} else {
			hif_lib_xmit_pkt(&priv->client, queue->queue_id,
				(void *)(size_t)rte_pktmbuf_iova(tx_pkts[i]),
				tx_pkts[i]->buf_addr + tx_pkts[i]->data_off,
				tx_pkts[i]->pkt_len, 0 /*ctrl*/,
				HIF_FIRST_BUFFER | HIF_LAST_BUFFER |
				HIF_DATA_VALID,
				tx_pkts[i]);
		}
		stats->obytes += tx_pkts[i]->pkt_len;
		hif_tx_dma_start();
	}
	stats->opackets += nb_pkts;
	pfe_tx_do_cleanup(priv->pfe);

	return nb_pkts;
}

static int
pfe_eth_open(struct rte_eth_dev *dev)
{
	struct pfe_eth_priv_s *priv = dev->data->dev_private;
	struct hif_client_s *client;
	struct hif_shm *hif_shm;
	uint16_t i;
	int rc;

	/* Register client driver with HIF */
	client = &priv->client;

	if (client->pfe) {
		hif_shm = client->pfe->hif.shm;
		/* TODO please remove the below code of if block, once we add
		 * the proper cleanup in eth_close
		 */
		if (!test_bit(PFE_CL_GEM0 + priv->id,
			      &hif_shm->g_client_status[0])) {
			/* Register client driver with HIF */
			memset(client, 0, sizeof(*client));
			client->id = PFE_CL_GEM0 + priv->id;
			client->tx_qn = emac_txq_cnt;
			client->rx_qn = EMAC_RXQ_CNT;
			client->priv = priv;
			client->pfe = priv->pfe;
			client->port_id = dev->data->port_id;
			client->event_handler = pfe_eth_event_handler;

			client->tx_qsize = EMAC_TXQ_DEPTH;
			client->rx_qsize = EMAC_RXQ_DEPTH;

			rc = hif_lib_client_register(client);
			if (rc) {
				PFE_PMD_ERR("hif_lib_client_register(%d)"
					    " failed", client->id);
				goto err0;
			}
		} else {
			/* Freeing the packets if already exists */
			int ret = 0;
			struct rte_mbuf *rx_pkts[32];
			/* TODO multiqueue support */
			ret = hif_lib_receive_pkt(&client->rx_q[0],
						  hif_shm->pool, rx_pkts, 32);
			while (ret) {
				int i;
				for (i = 0; i < ret; i++)
					rte_pktmbuf_free(rx_pkts[i]);
				ret = hif_lib_receive_pkt(&client->rx_q[0],
							  hif_shm->pool,
							  rx_pkts, 32);
			}
		}
	} else {
		/* Register client driver with HIF */
		memset(client, 0, sizeof(*client));
		client->id = PFE_CL_GEM0 + priv->id;
		client->tx_qn = emac_txq_cnt;
		client->rx_qn = EMAC_RXQ_CNT;
		client->priv = priv;
		client->pfe = priv->pfe;
		client->port_id = dev->data->port_id;
		client->event_handler = pfe_eth_event_handler;

		client->tx_qsize = EMAC_TXQ_DEPTH;
		client->rx_qsize = EMAC_RXQ_DEPTH;

		rc = hif_lib_client_register(client);
		if (rc) {
			PFE_PMD_ERR("hif_lib_client_register(%d) failed",
				    client->id);
			goto err0;
		}
	}
	rc = pfe_eth_start(priv);
	dev->rx_pkt_burst = &pfe_recv_pkts;
	dev->tx_pkt_burst = &pfe_xmit_pkts;
	/* If no prefetch is configured. */
	if (getenv("PFE_INTR_SUPPORT")) {
		dev->rx_pkt_burst = &pfe_recv_pkts_on_intr;
		PFE_PMD_INFO("PFE INTERRUPT Mode enabled");
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;

err0:
	return rc;
}

static int
pfe_eth_open_cdev(struct pfe_eth_priv_s *priv)
{
	int pfe_cdev_fd;

	if (priv == NULL)
		return -1;

	pfe_cdev_fd = open(PFE_CDEV_PATH, O_RDONLY);
	if (pfe_cdev_fd < 0) {
		PFE_PMD_WARN("Unable to open PFE device file (%s).\n",
			     PFE_CDEV_PATH);
		PFE_PMD_WARN("Link status update will not be available.\n");
		priv->link_fd = PFE_CDEV_INVALID_FD;
		return -1;
	}

	priv->link_fd = pfe_cdev_fd;

	return 0;
}

static void
pfe_eth_close_cdev(struct pfe_eth_priv_s *priv)
{
	if (priv == NULL)
		return;

	if (priv->link_fd != PFE_CDEV_INVALID_FD) {
		close(priv->link_fd);
		priv->link_fd = PFE_CDEV_INVALID_FD;
	}
}

static int
pfe_eth_stop(struct rte_eth_dev *dev/*, int wake*/)
{
	struct pfe_eth_priv_s *priv = dev->data->dev_private;
	uint16_t i;

	dev->data->dev_started = 0;

	gemac_disable(priv->EMAC_baseaddr);
	gpi_disable(priv->GPI_baseaddr);

	dev->rx_pkt_burst = rte_eth_pkt_burst_dummy;
	dev->tx_pkt_burst = rte_eth_pkt_burst_dummy;

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

static int
pfe_eth_close(struct rte_eth_dev *dev)
{
	int ret;
	PMD_INIT_FUNC_TRACE();

	if (!dev)
		return -1;

	if (!g_pfe)
		return -1;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	ret = pfe_eth_stop(dev);
	/* Close the device file for link status */
	pfe_eth_close_cdev(dev->data->dev_private);

	munmap(g_pfe->cbus_baseaddr, g_pfe->cbus_size);
	g_pfe->nb_devs--;

	if (g_pfe->nb_devs == 0) {
		pfe_hif_exit(g_pfe);
		pfe_hif_lib_exit(g_pfe);
		rte_free(g_pfe);
		g_pfe = NULL;
	}

	return ret;
}

static int
pfe_eth_configure(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

static int
pfe_eth_info(struct rte_eth_dev *dev,
		struct rte_eth_dev_info *dev_info)
{
	dev_info->max_mac_addrs = PFE_MAX_MACS;
	dev_info->max_rx_queues = dev->data->nb_rx_queues;
	dev_info->max_tx_queues = dev->data->nb_tx_queues;
	dev_info->min_rx_bufsize = HIF_RX_PKT_MIN_SIZE;
	dev_info->min_mtu = RTE_ETHER_MIN_MTU;
	dev_info->rx_offload_capa = dev_rx_offloads_sup;
	dev_info->tx_offload_capa = dev_tx_offloads_sup;
	if (pfe_svr == SVR_LS1012A_REV1) {
		dev_info->max_rx_pktlen = MAX_MTU_ON_REV1 + PFE_ETH_OVERHEAD;
		dev_info->max_mtu = MAX_MTU_ON_REV1;
	} else {
		dev_info->max_rx_pktlen = JUMBO_FRAME_SIZE;
		dev_info->max_mtu = JUMBO_FRAME_SIZE - PFE_ETH_OVERHEAD;
	}

	return 0;
}

/* Only first mb_pool given on first call of this API will be used
 * in whole system, also nb_rx_desc and rx_conf are unused params
 */
static int
pfe_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		__rte_unused uint16_t nb_rx_desc,
		__rte_unused unsigned int socket_id,
		__rte_unused const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool)
{
	int rc = 0;
	struct pfe *pfe;
	struct pfe_eth_priv_s *priv = dev->data->dev_private;

	pfe = priv->pfe;

	if (queue_idx >= EMAC_RXQ_CNT) {
		PFE_PMD_ERR("Invalid queue idx = %d, Max queues = %d",
				queue_idx, EMAC_RXQ_CNT);
		return -1;
	}

	if (!pfe->hif.setuped) {
		rc = pfe_hif_shm_init(pfe->hif.shm, mb_pool);
		if (rc) {
			PFE_PMD_ERR("Could not allocate buffer descriptors");
			return -1;
		}

		pfe->hif.shm->pool = mb_pool;
		if (pfe_hif_init_buffers(&pfe->hif)) {
			PFE_PMD_ERR("Could not initialize buffer descriptors");
			return -1;
		}
		hif_init();
		hif_rx_enable();
		hif_tx_enable();
		pfe->hif.setuped = 1;
	}
	dev->data->rx_queues[queue_idx] = &priv->client.rx_q[queue_idx];
	priv->client.rx_q[queue_idx].queue_id = queue_idx;

	return 0;
}

static int
pfe_tx_queue_setup(struct rte_eth_dev *dev,
		   uint16_t queue_idx,
		   __rte_unused uint16_t nb_desc,
		   __rte_unused unsigned int socket_id,
		   __rte_unused const struct rte_eth_txconf *tx_conf)
{
	struct pfe_eth_priv_s *priv = dev->data->dev_private;

	if (queue_idx >= emac_txq_cnt) {
		PFE_PMD_ERR("Invalid queue idx = %d, Max queues = %d",
				queue_idx, emac_txq_cnt);
		return -1;
	}
	dev->data->tx_queues[queue_idx] = &priv->client.tx_q[queue_idx];
	priv->client.tx_q[queue_idx].queue_id = queue_idx;
	return 0;
}

static const uint32_t *
pfe_supported_ptypes_get(struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		/*todo -= add more types */
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV4_EXT,
		RTE_PTYPE_L3_IPV6,
		RTE_PTYPE_L3_IPV6_EXT,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_SCTP
	};

	if (dev->rx_pkt_burst == pfe_recv_pkts ||
			dev->rx_pkt_burst == pfe_recv_pkts_on_intr)
		return ptypes;
	return NULL;
}

static inline int
pfe_eth_atomic_read_link_status(struct rte_eth_dev *dev,
				struct rte_eth_link *link)
{
	struct rte_eth_link *dst = link;
	struct rte_eth_link *src = &dev->data->dev_link;

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
				*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

static inline int
pfe_eth_atomic_write_link_status(struct rte_eth_dev *dev,
				 struct rte_eth_link *link)
{
	struct rte_eth_link *dst = &dev->data->dev_link;
	struct rte_eth_link *src = link;

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
				*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

static int
pfe_eth_link_update(struct rte_eth_dev *dev, int wait_to_complete __rte_unused)
{
	int ret, ioctl_cmd = 0;
	struct pfe_eth_priv_s *priv = dev->data->dev_private;
	struct rte_eth_link link, old;
	unsigned int lstatus = 1;

	memset(&old, 0, sizeof(old));
	memset(&link, 0, sizeof(struct rte_eth_link));

	pfe_eth_atomic_read_link_status(dev, &old);

	/* Read from PFE CDEV, status of link, if file was successfully
	 * opened.
	 */
	if (priv->link_fd != PFE_CDEV_INVALID_FD) {
		if (priv->id == 0)
			ioctl_cmd = PFE_CDEV_ETH0_STATE_GET;
		if (priv->id == 1)
			ioctl_cmd = PFE_CDEV_ETH1_STATE_GET;

		ret = ioctl(priv->link_fd, ioctl_cmd, &lstatus);
		if (ret != 0) {
			PFE_PMD_ERR("Unable to fetch link status (ioctl)\n");
			return -1;
		}
		PFE_PMD_DEBUG("Fetched link state (%d) for dev %d.\n",
			      lstatus, priv->id);
	}

	if (old.link_status == lstatus) {
		/* no change in status */
		PFE_PMD_DEBUG("No change in link status; Not updating.\n");
		return -1;
	}

	link.link_status = lstatus;
	link.link_speed = RTE_ETH_LINK_SPEED_1G;
	link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	link.link_autoneg = RTE_ETH_LINK_AUTONEG;

	pfe_eth_atomic_write_link_status(dev, &link);

	PFE_PMD_INFO("Port (%d) link is %s\n", dev->data->port_id,
		     link.link_status ? "up" : "down");

	return 0;
}

static int
pfe_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct pfe_eth_priv_s *priv = dev->data->dev_private;

	priv->promisc = 1;
	dev->data->promiscuous = 1;
	gemac_enable_copy_all(priv->EMAC_baseaddr);

	return 0;
}

static int
pfe_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct pfe_eth_priv_s *priv = dev->data->dev_private;

	priv->promisc = 0;
	dev->data->promiscuous = 0;
	gemac_disable_copy_all(priv->EMAC_baseaddr);

	return 0;
}

static int
pfe_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct pfe_eth_priv_s *priv = dev->data->dev_private;
	struct pfe_mac_addr    hash_addr; /* hash register structure */

	/* Set the hash to rx all multicast frames */
	hash_addr.bottom = 0xFFFFFFFF;
	hash_addr.top = 0xFFFFFFFF;
	gemac_set_hash(priv->EMAC_baseaddr, &hash_addr);
	dev->data->all_multicast = 1;

	return 0;
}

static int
pfe_link_down(struct rte_eth_dev *dev)
{
	return pfe_eth_stop(dev);
}

static int
pfe_link_up(struct rte_eth_dev *dev)
{
	struct pfe_eth_priv_s *priv = dev->data->dev_private;

	pfe_eth_start(priv);
	return 0;
}

static int
pfe_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct pfe_eth_priv_s *priv = dev->data->dev_private;
	uint16_t frame_size = mtu + RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN;

	/*TODO Support VLAN*/
	return gemac_set_rx(priv->EMAC_baseaddr, frame_size);
}

/* pfe_eth_enet_addr_byte_mac
 */
static int
pfe_eth_enet_addr_byte_mac(u8 *enet_byte_addr,
			   struct pfe_mac_addr *enet_addr)
{
	if (!enet_byte_addr || !enet_addr) {
		return -1;

	} else {
		enet_addr->bottom = enet_byte_addr[0] |
			(enet_byte_addr[1] << 8) |
			(enet_byte_addr[2] << 16) |
			(enet_byte_addr[3] << 24);
		enet_addr->top = enet_byte_addr[4] |
			(enet_byte_addr[5] << 8);
		return 0;
	}
}

static int
pfe_dev_set_mac_addr(struct rte_eth_dev *dev,
		       struct rte_ether_addr *addr)
{
	struct pfe_eth_priv_s *priv = dev->data->dev_private;
	struct pfe_mac_addr spec_addr;
	int ret;

	ret = pfe_eth_enet_addr_byte_mac(addr->addr_bytes, &spec_addr);
	if (ret)
		return ret;

	gemac_set_laddrN(priv->EMAC_baseaddr,
			 (struct pfe_mac_addr *)&spec_addr, 1);
	rte_ether_addr_copy(addr, &dev->data->mac_addrs[0]);
	return 0;
}

static int
pfe_stats_get(struct rte_eth_dev *dev,
	      struct rte_eth_stats *stats)
{
	struct pfe_eth_priv_s *priv = dev->data->dev_private;
	struct rte_eth_stats *eth_stats = &priv->stats;

	if (stats == NULL)
		return -1;

	memset(stats, 0, sizeof(struct rte_eth_stats));

	stats->ipackets = eth_stats->ipackets;
	stats->ibytes = eth_stats->ibytes;
	stats->opackets = eth_stats->opackets;
	stats->obytes = eth_stats->obytes;

	return 0;
}

static const struct eth_dev_ops ops = {
	.dev_start = pfe_eth_open,
	.dev_stop = pfe_eth_stop,
	.dev_close = pfe_eth_close,
	.dev_configure = pfe_eth_configure,
	.dev_infos_get = pfe_eth_info,
	.rx_queue_setup = pfe_rx_queue_setup,
	.tx_queue_setup = pfe_tx_queue_setup,
	.dev_supported_ptypes_get = pfe_supported_ptypes_get,
	.link_update  = pfe_eth_link_update,
	.promiscuous_enable   = pfe_promiscuous_enable,
	.promiscuous_disable  = pfe_promiscuous_disable,
	.allmulticast_enable  = pfe_allmulticast_enable,
	.dev_set_link_down    = pfe_link_down,
	.dev_set_link_up      = pfe_link_up,
	.mtu_set              = pfe_mtu_set,
	.mac_addr_set	      = pfe_dev_set_mac_addr,
	.stats_get            = pfe_stats_get,
};

static int
pfe_eth_init(struct rte_vdev_device *vdev, struct pfe *pfe, int id)
{
	struct rte_eth_dev *eth_dev = NULL;
	struct pfe_eth_priv_s *priv = NULL;
	struct ls1012a_eth_platform_data *einfo;
	struct ls1012a_pfe_platform_data *pfe_info;
	struct rte_ether_addr addr;
	int err;

	eth_dev = rte_eth_vdev_allocate(vdev, sizeof(*priv));
	if (eth_dev == NULL)
		return -ENOMEM;

	/* Extract platform data */
	pfe_info = (struct ls1012a_pfe_platform_data *)&pfe->platform_data;
	if (!pfe_info) {
		PFE_PMD_ERR("pfe missing additional platform data");
		err = -ENODEV;
		goto err0;
	}

	einfo = (struct ls1012a_eth_platform_data *)pfe_info->ls1012a_eth_pdata;

	/* einfo never be NULL, but no harm in having this check */
	if (!einfo) {
		PFE_PMD_ERR("pfe missing additional gemacs platform data");
		err = -ENODEV;
		goto err0;
	}

	priv = eth_dev->data->dev_private;
	priv->ndev = eth_dev;
	priv->id = einfo[id].gem_id;
	priv->pfe = pfe;

	pfe->eth.eth_priv[id] = priv;

	/* Set the info in the priv to the current info */
	priv->einfo = &einfo[id];
	priv->EMAC_baseaddr = cbus_emac_base[id];
	priv->PHY_baseaddr = cbus_emac_base[id];
	priv->GPI_baseaddr = cbus_gpi_base[id];

#define HIF_GEMAC_TMUQ_BASE	6
	priv->low_tmu_q = HIF_GEMAC_TMUQ_BASE + (id * 2);
	priv->high_tmu_q = priv->low_tmu_q + 1;

	rte_spinlock_init(&priv->lock);

	/* Copy the station address into the dev structure, */
	eth_dev->data->mac_addrs = rte_zmalloc("mac_addr",
			ETHER_ADDR_LEN * PFE_MAX_MACS, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PFE_PMD_ERR("Failed to allocate mem %d to store MAC addresses",
			ETHER_ADDR_LEN * PFE_MAX_MACS);
		err = -ENOMEM;
		goto err0;
	}

	memcpy(addr.addr_bytes, priv->einfo->mac_addr,
		       ETH_ALEN);

	pfe_dev_set_mac_addr(eth_dev, &addr);
	rte_ether_addr_copy(&addr, &eth_dev->data->mac_addrs[0]);

	eth_dev->data->mtu = 1500;
	eth_dev->dev_ops = &ops;
	err = pfe_eth_stop(eth_dev);
	if (err != 0)
		goto err0;
	pfe_gemac_init(priv);

	eth_dev->data->nb_rx_queues = 1;
	eth_dev->data->nb_tx_queues = 1;

	/* For link status, open the PFE CDEV; Error from this function
	 * is silently ignored; In case of error, the link status will not
	 * be available.
	 */
	pfe_eth_open_cdev(priv);
	rte_eth_dev_probing_finish(eth_dev);

	return 0;
err0:
	rte_eth_dev_release_port(eth_dev);
	return err;
}

static int
pfe_get_gemac_if_proprties(struct pfe *pfe,
		__rte_unused const struct device_node *parent,
		unsigned int port, unsigned int if_cnt,
		struct ls1012a_pfe_platform_data *pdata)
{
	const struct device_node *gem = NULL;
	size_t size;
	unsigned int ii = 0, phy_id = 0;
	const u32 *addr;
	const void *mac_addr;

	for (ii = 0; ii < if_cnt; ii++) {
		gem = of_get_next_child(parent, gem);
		if (!gem)
			goto err;
		addr = of_get_property(gem, "reg", &size);
		if (addr && (rte_be_to_cpu_32((unsigned int)*addr) == port))
			break;
	}

	if (ii >= if_cnt) {
		PFE_PMD_ERR("Failed to find interface = %d", if_cnt);
		goto err;
	}

	pdata->ls1012a_eth_pdata[port].gem_id = port;

	mac_addr = of_get_mac_address(gem);

	if (mac_addr) {
		memcpy(pdata->ls1012a_eth_pdata[port].mac_addr, mac_addr,
		       ETH_ALEN);
	}

	addr = of_get_property(gem, "fsl,mdio-mux-val", &size);
	if (!addr) {
		PFE_PMD_ERR("Invalid mdio-mux-val....");
	} else {
		phy_id = rte_be_to_cpu_32((unsigned int)*addr);
		pdata->ls1012a_eth_pdata[port].mdio_muxval = phy_id;
	}
	if (pdata->ls1012a_eth_pdata[port].phy_id < 32)
		pfe->mdio_muxval[pdata->ls1012a_eth_pdata[port].phy_id] =
			 pdata->ls1012a_eth_pdata[port].mdio_muxval;

	return 0;

err:
	return -1;
}

/* Parse integer from integer argument */
static int
parse_integer_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	int i;
	char *end;
	errno = 0;

	i = strtol(value, &end, 10);
	if (*end != 0 || errno != 0 || i < 0 || i > 1) {
		PFE_PMD_ERR("Supported Port IDS are 0 and 1");
		return -EINVAL;
	}

	*((uint32_t *)extra_args) = i;

	return 0;
}

static int
pfe_parse_vdev_init_params(struct pfe_vdev_init_params *params,
			   struct rte_vdev_device *dev)
{
	struct rte_kvargs *kvlist = NULL;
	int ret = 0;

	static const char * const pfe_vdev_valid_params[] = {
		PFE_VDEV_GEM_ID_ARG,
		NULL
	};

	const char *input_args = rte_vdev_device_args(dev);

	if (!input_args)
		return -1;

	kvlist = rte_kvargs_parse(input_args, pfe_vdev_valid_params);
	if (kvlist == NULL)
		return -1;

	ret = rte_kvargs_process(kvlist,
				PFE_VDEV_GEM_ID_ARG,
				&parse_integer_arg,
				&params->gem_id);
	rte_kvargs_free(kvlist);
	return ret;
}

static int
pmd_pfe_probe(struct rte_vdev_device *vdev)
{
	const u32 *prop;
	const struct device_node *np;
	const char *name;
	const uint32_t *addr;
	uint64_t cbus_addr, ddr_size, cbus_size;
	int rc = -1, fd = -1, gem_id;
	unsigned int ii, interface_count = 0;
	size_t size = 0;
	struct pfe_vdev_init_params init_params = {
		.gem_id = -1
	};

	name = rte_vdev_device_name(vdev);
	rc = pfe_parse_vdev_init_params(&init_params, vdev);
	if (rc < 0)
		return -EINVAL;

	PFE_PMD_LOG(INFO, "Initializing pmd_pfe for %s Given gem-id %d",
		name, init_params.gem_id);

	if (g_pfe) {
		if (g_pfe->nb_devs >= g_pfe->max_intf) {
			PFE_PMD_ERR("PFE %d dev already created Max is %d",
				g_pfe->nb_devs, g_pfe->max_intf);
			return -EINVAL;
		}
		goto eth_init;
	}

	g_pfe = rte_zmalloc(NULL, sizeof(*g_pfe), RTE_CACHE_LINE_SIZE);
	if (g_pfe == NULL)
		return  -EINVAL;

	/* Load the device-tree driver */
	rc = of_init();
	if (rc) {
		PFE_PMD_ERR("of_init failed with ret: %d", rc);
		goto err;
	}

	np = of_find_compatible_node(NULL, NULL, "fsl,pfe");
	if (!np) {
		PFE_PMD_ERR("Invalid device node");
		rc = -EINVAL;
		goto err;
	}

	addr = of_get_address(np, 0, &cbus_size, NULL);
	if (!addr) {
		PFE_PMD_ERR("of_get_address cannot return qman address\n");
		goto err;
	}
	cbus_addr = of_translate_address(np, addr);
	if (!cbus_addr) {
		PFE_PMD_ERR("of_translate_address failed\n");
		goto err;
	}

	addr = of_get_address(np, 1, &ddr_size, NULL);
	if (!addr) {
		PFE_PMD_ERR("of_get_address cannot return qman address\n");
		goto err;
	}

	g_pfe->ddr_phys_baseaddr = of_translate_address(np, addr);
	if (!g_pfe->ddr_phys_baseaddr) {
		PFE_PMD_ERR("of_translate_address failed\n");
		goto err;
	}

	g_pfe->ddr_baseaddr = pfe_mem_ptov(g_pfe->ddr_phys_baseaddr);
	g_pfe->ddr_size = ddr_size;
	g_pfe->cbus_size = cbus_size;

	fd = open("/dev/mem", O_RDWR);
	g_pfe->cbus_baseaddr = mmap(NULL, cbus_size, PROT_READ | PROT_WRITE,
					MAP_SHARED, fd, cbus_addr);
	close(fd);
	if (g_pfe->cbus_baseaddr == MAP_FAILED) {
		PFE_PMD_ERR("Can not map cbus base");
		rc = -EINVAL;
		goto err;
	}

	/* Read interface count */
	prop = of_get_property(np, "fsl,pfe-num-interfaces", &size);
	if (!prop) {
		PFE_PMD_ERR("Failed to read number of interfaces");
		rc = -ENXIO;
		goto err_prop;
	}

	interface_count = rte_be_to_cpu_32((unsigned int)*prop);
	if (interface_count <= 0) {
		PFE_PMD_ERR("No ethernet interface count : %d",
				interface_count);
		rc = -ENXIO;
		goto err_prop;
	}
	PFE_PMD_INFO("num interfaces = %d ", interface_count);

	g_pfe->max_intf  = interface_count;
	g_pfe->platform_data.ls1012a_mdio_pdata[0].phy_mask = 0xffffffff;

	for (ii = 0; ii < interface_count; ii++) {
		pfe_get_gemac_if_proprties(g_pfe, np, ii, interface_count,
					   &g_pfe->platform_data);
	}

	pfe_lib_init(g_pfe->cbus_baseaddr, g_pfe->ddr_baseaddr,
		     g_pfe->ddr_phys_baseaddr, g_pfe->ddr_size);

	PFE_PMD_INFO("CLASS version: %x", readl(CLASS_VERSION));
	PFE_PMD_INFO("TMU version: %x", readl(TMU_VERSION));

	PFE_PMD_INFO("BMU1 version: %x", readl(BMU1_BASE_ADDR + BMU_VERSION));
	PFE_PMD_INFO("BMU2 version: %x", readl(BMU2_BASE_ADDR + BMU_VERSION));

	PFE_PMD_INFO("EGPI1 version: %x", readl(EGPI1_BASE_ADDR + GPI_VERSION));
	PFE_PMD_INFO("EGPI2 version: %x", readl(EGPI2_BASE_ADDR + GPI_VERSION));
	PFE_PMD_INFO("HGPI version: %x", readl(HGPI_BASE_ADDR + GPI_VERSION));

	PFE_PMD_INFO("HIF version: %x", readl(HIF_VERSION));
	PFE_PMD_INFO("HIF NOPCY version: %x", readl(HIF_NOCPY_VERSION));

	cbus_emac_base[0] = EMAC1_BASE_ADDR;
	cbus_emac_base[1] = EMAC2_BASE_ADDR;

	cbus_gpi_base[0] = EGPI1_BASE_ADDR;
	cbus_gpi_base[1] = EGPI2_BASE_ADDR;

	rc = pfe_hif_lib_init(g_pfe);
	if (rc < 0)
		goto err_hif_lib;

	rc = pfe_hif_init(g_pfe);
	if (rc < 0)
		goto err_hif;
	pfe_soc_version_get();
eth_init:
	if (init_params.gem_id < 0)
		gem_id = g_pfe->nb_devs;
	else
		gem_id = init_params.gem_id;

	PFE_PMD_LOG(INFO, "Init pmd_pfe for %s gem-id %d(given =%d)",
		name, gem_id, init_params.gem_id);

	rc = pfe_eth_init(vdev, g_pfe, gem_id);
	if (rc < 0)
		goto err_eth;
	else
		g_pfe->nb_devs++;

	return 0;

err_eth:
	pfe_hif_exit(g_pfe);

err_hif:
	pfe_hif_lib_exit(g_pfe);

err_hif_lib:
err_prop:
	munmap(g_pfe->cbus_baseaddr, cbus_size);
err:
	rte_free(g_pfe);
	return rc;
}

static int
pmd_pfe_remove(struct rte_vdev_device *vdev)
{
	const char *name;
	struct rte_eth_dev *eth_dev = NULL;
	int ret = 0;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	PFE_PMD_INFO("Closing eventdev sw device %s", name);

	if (!g_pfe)
		return 0;

	eth_dev = rte_eth_dev_allocated(name);
	if (eth_dev) {
		pfe_eth_close(eth_dev);
		ret = rte_eth_dev_release_port(eth_dev);
	}

	return ret;
}

static
struct rte_vdev_driver pmd_pfe_drv = {
	.probe = pmd_pfe_probe,
	.remove = pmd_pfe_remove,
};

RTE_PMD_REGISTER_VDEV(PFE_NAME_PMD, pmd_pfe_drv);
RTE_PMD_REGISTER_PARAM_STRING(PFE_NAME_PMD, PFE_VDEV_GEM_ID_ARG "=<int> ");
RTE_LOG_REGISTER_DEFAULT(pfe_logtype_pmd, NOTICE);
