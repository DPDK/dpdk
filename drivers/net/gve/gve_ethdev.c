/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022-2023 Intel Corporation
 * Copyright(C) 2023 Google LLC
 */

#include "gve_ethdev.h"
#include "base/gve_adminq.h"
#include "base/gve_register.h"
#include "base/gve_osdep.h"
#include "gve_version.h"
#include "rte_ether.h"
#include "gve_rss.h"
#include <ethdev_driver.h>

static void
gve_write_version(uint8_t *driver_version_register)
{
	const char *c = gve_version_string();
	while (*c) {
		writeb(*c, driver_version_register);
		c++;
	}
	writeb('\n', driver_version_register);
}

static const struct rte_memzone *
gve_alloc_using_mz(const char *name, uint32_t num_pages)
{
	const struct rte_memzone *mz;
	mz = rte_memzone_reserve_aligned(name, num_pages * PAGE_SIZE,
					 rte_socket_id(),
					 RTE_MEMZONE_IOVA_CONTIG, PAGE_SIZE);
	if (mz == NULL)
		PMD_DRV_LOG(ERR, "Failed to alloc memzone %s.", name);
	return mz;
}

static int
gve_alloc_using_malloc(void **bufs, uint32_t num_entries)
{
	uint32_t i;

	for (i = 0; i < num_entries; i++) {
		bufs[i] = rte_malloc_socket(NULL, PAGE_SIZE, PAGE_SIZE, rte_socket_id());
		if (bufs[i] == NULL) {
			PMD_DRV_LOG(ERR, "Failed to malloc");
			goto free_bufs;
		}
	}
	return 0;

free_bufs:
	while (i > 0)
		rte_free(bufs[--i]);

	return -ENOMEM;
}

static struct gve_queue_page_list *
gve_alloc_queue_page_list(const char *name, uint32_t num_pages, bool is_rx)
{
	struct gve_queue_page_list *qpl;
	const struct rte_memzone *mz;
	uint32_t i;

	qpl = rte_zmalloc("qpl struct",	sizeof(struct gve_queue_page_list), 0);
	if (!qpl)
		return NULL;

	qpl->page_buses = rte_zmalloc("qpl page buses",
		num_pages * sizeof(dma_addr_t), 0);
	if (qpl->page_buses == NULL) {
		PMD_DRV_LOG(ERR, "Failed to alloc qpl page buses");
		goto free_qpl_struct;
	}

	if (is_rx) {
		/* RX QPL need not be IOVA contiguous.
		 * Allocate 4K size buffers using malloc
		 */
		qpl->qpl_bufs = rte_zmalloc("qpl bufs",
			num_pages * sizeof(void *), 0);
		if (qpl->qpl_bufs == NULL) {
			PMD_DRV_LOG(ERR, "Failed to alloc qpl bufs");
			goto free_qpl_page_buses;
		}

		if (gve_alloc_using_malloc(qpl->qpl_bufs, num_pages))
			goto free_qpl_page_bufs;

		/* Populate the IOVA addresses */
		for (i = 0; i < num_pages; i++)
			qpl->page_buses[i] =
				rte_malloc_virt2iova(qpl->qpl_bufs[i]);
	} else {
		/* TX QPL needs to be IOVA contiguous
		 * Allocate QPL using memzone
		 */
		mz = gve_alloc_using_mz(name, num_pages);
		if (!mz)
			goto free_qpl_page_buses;

		qpl->mz = mz;

		/* Populate the IOVA addresses */
		for (i = 0; i < num_pages; i++)
			qpl->page_buses[i] = mz->iova + i * PAGE_SIZE;
	}

	qpl->num_entries = num_pages;
	return qpl;

free_qpl_page_bufs:
	rte_free(qpl->qpl_bufs);
free_qpl_page_buses:
	rte_free(qpl->page_buses);
free_qpl_struct:
	rte_free(qpl);
	return NULL;
}

static void
gve_free_queue_page_list(struct gve_queue_page_list *qpl)
{
	if (qpl->mz) {
		rte_memzone_free(qpl->mz);
		qpl->mz = NULL;
	} else if (qpl->qpl_bufs) {
		uint32_t i;

		for (i = 0; i < qpl->num_entries; i++)
			rte_free(qpl->qpl_bufs[i]);
	}

	if (qpl->qpl_bufs) {
		rte_free(qpl->qpl_bufs);
		qpl->qpl_bufs = NULL;
	}

	if (qpl->page_buses) {
		rte_free(qpl->page_buses);
		qpl->page_buses = NULL;
	}
	rte_free(qpl);
}

struct gve_queue_page_list *
gve_setup_queue_page_list(struct gve_priv *priv, uint16_t queue_id, bool is_rx,
	uint32_t num_pages)
{
	const char *queue_type_string = is_rx ? "rx" : "tx";
	char qpl_name[RTE_MEMZONE_NAMESIZE];
	struct gve_queue_page_list *qpl;
	int err;

	/* Allocate a new QPL. */
	snprintf(qpl_name, sizeof(qpl_name), "gve_%s_%s_qpl%d",
		priv->pci_dev->device.name, queue_type_string, queue_id);
	qpl = gve_alloc_queue_page_list(qpl_name, num_pages, is_rx);
	if (!qpl) {
		PMD_DRV_LOG(ERR,
			    "Failed to alloc %s qpl for queue %hu.",
			    queue_type_string, queue_id);
		return NULL;
	}

	/* Assign the QPL an ID. */
	qpl->id = queue_id;
	if (is_rx)
		qpl->id += priv->max_nb_txq;

	/* Validate page registration limit and register QPLs. */
	if (priv->num_registered_pages + qpl->num_entries >
	    priv->max_registered_pages) {
		PMD_DRV_LOG(ERR, "Pages %" PRIu64 " > max registered pages %" PRIu64,
			    priv->num_registered_pages + qpl->num_entries,
			    priv->max_registered_pages);
		goto cleanup_qpl;
	}
	err = gve_adminq_register_page_list(priv, qpl);
	if (err) {
		PMD_DRV_LOG(ERR,
			    "Failed to register %s qpl for queue %hu.",
			    queue_type_string, queue_id);
		goto cleanup_qpl;
	}
	priv->num_registered_pages += qpl->num_entries;
	return qpl;

cleanup_qpl:
	gve_free_queue_page_list(qpl);
	return NULL;
}

int
gve_teardown_queue_page_list(struct gve_priv *priv,
	struct gve_queue_page_list *qpl)
{
	int err = gve_adminq_unregister_page_list(priv, qpl->id);
	if (err) {
		PMD_DRV_LOG(CRIT, "Unable to unregister qpl %d!", qpl->id);
		return err;
	}
	priv->num_registered_pages -= qpl->num_entries;
	gve_free_queue_page_list(qpl);
	return 0;
}

static int
gve_dev_configure(struct rte_eth_dev *dev)
{
	struct gve_priv *priv = dev->data->dev_private;

	if (dev->data->dev_conf.rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG) {
		dev->data->dev_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;
		priv->rss_config.alg = GVE_RSS_HASH_TOEPLITZ;
	}

	if (dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO)
		priv->enable_rsc = 1;

	/* Reset RSS RETA in case number of queues changed. */
	if (priv->rss_config.indir) {
		struct gve_rss_config update_reta_config;
		gve_init_rss_config_from_priv(priv, &update_reta_config);
		gve_generate_rss_reta(dev, &update_reta_config);

		int err = gve_adminq_configure_rss(priv, &update_reta_config);
		if (err)
			PMD_DRV_LOG(ERR,
				"Could not reconfigure RSS redirection table.");
		else
			gve_update_priv_rss_config(priv, &update_reta_config);

		gve_free_rss_config(&update_reta_config);
		return err;
	}

	return 0;
}

static int
gve_link_update(struct rte_eth_dev *dev, __rte_unused int wait_to_complete)
{
	struct gve_priv *priv = dev->data->dev_private;
	struct rte_eth_link link;
	int err;

	memset(&link, 0, sizeof(link));
	link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	link.link_autoneg = RTE_ETH_LINK_AUTONEG;

	if (!dev->data->dev_started) {
		link.link_status = RTE_ETH_LINK_DOWN;
		link.link_speed = RTE_ETH_SPEED_NUM_NONE;
	} else {
		link.link_status = RTE_ETH_LINK_UP;
		PMD_DRV_LOG(DEBUG, "Get link status from hw");
		err = gve_adminq_report_link_speed(priv);
		if (err) {
			PMD_DRV_LOG(ERR, "Failed to get link speed.");
			priv->link_speed = RTE_ETH_SPEED_NUM_UNKNOWN;
		}
		link.link_speed = priv->link_speed;
	}

	return rte_eth_linkstatus_set(dev, &link);
}

static int
gve_alloc_stats_report(struct gve_priv *priv,
		uint16_t nb_tx_queues, uint16_t nb_rx_queues)
{
	char z_name[RTE_MEMZONE_NAMESIZE];
	int tx_stats_cnt;
	int rx_stats_cnt;

	tx_stats_cnt = (GVE_TX_STATS_REPORT_NUM + NIC_TX_STATS_REPORT_NUM) *
		nb_tx_queues;
	rx_stats_cnt = (GVE_RX_STATS_REPORT_NUM + NIC_RX_STATS_REPORT_NUM) *
		nb_rx_queues;
	priv->stats_report_len = sizeof(struct gve_stats_report) +
		sizeof(struct stats) * (tx_stats_cnt + rx_stats_cnt);

	snprintf(z_name, sizeof(z_name), "gve_stats_report_%s",
			priv->pci_dev->device.name);
	priv->stats_report_mem = rte_memzone_reserve_aligned(z_name,
			priv->stats_report_len,
			rte_socket_id(),
			RTE_MEMZONE_IOVA_CONTIG, PAGE_SIZE);

	if (!priv->stats_report_mem)
		return -ENOMEM;

	/* offset by skipping stats written by gve. */
	priv->stats_start_idx = (GVE_TX_STATS_REPORT_NUM * nb_tx_queues) +
		(GVE_RX_STATS_REPORT_NUM * nb_rx_queues);
	priv->stats_end_idx = priv->stats_start_idx +
		(NIC_TX_STATS_REPORT_NUM * nb_tx_queues) +
		(NIC_RX_STATS_REPORT_NUM * nb_rx_queues) - 1;

	return 0;
}

static void
gve_free_stats_report(struct rte_eth_dev *dev)
{
	struct gve_priv *priv = dev->data->dev_private;
	rte_memzone_free(priv->stats_report_mem);
	priv->stats_report_mem = NULL;
}

/* Read Rx NIC stats from shared region */
static void
gve_get_imissed_from_nic(struct rte_eth_dev *dev)
{
	struct gve_stats_report *stats_report;
	struct gve_rx_queue *rxq;
	struct gve_priv *priv;
	struct stats stat;
	int queue_id;
	int stat_id;
	int i;

	priv = dev->data->dev_private;
	if (!priv->stats_report_mem)
		return;
	stats_report = (struct gve_stats_report *)
		priv->stats_report_mem->addr;
	for (i = priv->stats_start_idx; i <= priv->stats_end_idx; i++) {
		stat = stats_report->stats[i];
		queue_id = cpu_to_be32(stat.queue_id);
		rxq = dev->data->rx_queues[queue_id];
		if (rxq == NULL)
			continue;
		stat_id = cpu_to_be32(stat.stat_name);
		/* Update imissed. */
		if (stat_id == RX_NO_BUFFERS_POSTED)
			rxq->stats.imissed = cpu_to_be64(stat.value);
	}
}

static int
gve_start_queues(struct rte_eth_dev *dev)
{
	struct gve_priv *priv = dev->data->dev_private;
	uint16_t num_queues;
	uint16_t i;
	int ret;

	num_queues = dev->data->nb_tx_queues;
	priv->txqs = (struct gve_tx_queue **)dev->data->tx_queues;
	ret = gve_adminq_create_tx_queues(priv, num_queues);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to create %u tx queues.", num_queues);
		return ret;
	}
	for (i = 0; i < num_queues; i++) {
		if (gve_is_gqi(priv))
			ret = gve_tx_queue_start(dev, i);
		else
			ret = gve_tx_queue_start_dqo(dev, i);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "Fail to start Tx queue %d", i);
			goto err_tx;
		}
	}

	num_queues = dev->data->nb_rx_queues;
	priv->rxqs = (struct gve_rx_queue **)dev->data->rx_queues;
	ret = gve_adminq_create_rx_queues(priv, num_queues);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to create %u rx queues.", num_queues);
		goto err_tx;
	}
	for (i = 0; i < num_queues; i++) {
		if (gve_is_gqi(priv))
			ret = gve_rx_queue_start(dev, i);
		else
			ret = gve_rx_queue_start_dqo(dev, i);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "Fail to start Rx queue %d", i);
			goto err_rx;
		}
	}

	gve_set_device_rings_ok(priv);

	return 0;

err_rx:
	if (gve_is_gqi(priv))
		gve_stop_rx_queues(dev);
	else
		gve_stop_rx_queues_dqo(dev);
err_tx:
	if (gve_is_gqi(priv))
		gve_stop_tx_queues(dev);
	else
		gve_stop_tx_queues_dqo(dev);

	gve_clear_device_rings_ok(priv);
	return ret;
}

static int
gve_dev_start(struct rte_eth_dev *dev)
{
	struct gve_priv *priv;
	int ret;

	ret = gve_start_queues(dev);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to start queues");
		return ret;
	}

	dev->data->dev_started = 1;
	gve_link_update(dev, 0);

	priv = dev->data->dev_private;
	/* No stats available yet for Dqo. */
	if (gve_is_gqi(priv)) {
		ret = gve_alloc_stats_report(priv,
				dev->data->nb_tx_queues,
				dev->data->nb_rx_queues);
		if (ret != 0) {
			PMD_DRV_LOG(ERR,
				"Failed to allocate region for stats reporting.");
			return ret;
		}
		ret = gve_adminq_report_stats(priv, priv->stats_report_len,
				priv->stats_report_mem->iova,
				GVE_STATS_REPORT_TIMER_PERIOD);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "gve_adminq_report_stats command failed.");
			return ret;
		}
	}

	return 0;
}

static int
gve_dev_stop(struct rte_eth_dev *dev)
{
	struct gve_priv *priv = dev->data->dev_private;

	dev->data->dev_started = 0;
	dev->data->dev_link.link_status = RTE_ETH_LINK_DOWN;

	gve_clear_device_rings_ok(priv);
	if (gve_is_gqi(priv)) {
		gve_stop_tx_queues(dev);
		gve_stop_rx_queues(dev);
	} else {
		gve_stop_tx_queues_dqo(dev);
		gve_stop_rx_queues_dqo(dev);
	}

	if (gve_is_gqi(dev->data->dev_private))
		gve_free_stats_report(dev);

	return 0;
}

static void
gve_free_queues(struct rte_eth_dev *dev)
{
	struct gve_priv *priv = dev->data->dev_private;
	uint16_t i;

	if (gve_is_gqi(priv)) {
		for (i = 0; i < dev->data->nb_tx_queues; i++)
			gve_tx_queue_release(dev, i);

		for (i = 0; i < dev->data->nb_rx_queues; i++)
			gve_rx_queue_release(dev, i);
	} else {
		for (i = 0; i < dev->data->nb_tx_queues; i++)
			gve_tx_queue_release_dqo(dev, i);

		for (i = 0; i < dev->data->nb_rx_queues; i++)
			gve_rx_queue_release_dqo(dev, i);
	}
}

static void
gve_free_counter_array(struct gve_priv *priv)
{
	rte_memzone_free(priv->cnt_array_mz);
	priv->cnt_array = NULL;
}

static void
gve_free_irq_db(struct gve_priv *priv)
{
	rte_memzone_free(priv->irq_dbs_mz);
	priv->irq_dbs = NULL;
}

static void
gve_free_ptype_lut_dqo(struct gve_priv *priv)
{
	if (!gve_is_gqi(priv)) {
		rte_free(priv->ptype_lut_dqo);
		priv->ptype_lut_dqo = NULL;
	}
}

static void
gve_teardown_device_resources(struct gve_priv *priv)
{
	int err;

	/* Tell device its resources are being freed */
	if (gve_get_device_resources_ok(priv)) {
		err = gve_adminq_deconfigure_device_resources(priv);
		if (err)
			PMD_DRV_LOG(ERR, "Could not deconfigure device resources: err=%d", err);
	}

	gve_free_ptype_lut_dqo(priv);
	gve_free_counter_array(priv);
	gve_free_irq_db(priv);
	gve_clear_device_resources_ok(priv);
}

static int
gve_dev_close(struct rte_eth_dev *dev)
{
	struct gve_priv *priv = dev->data->dev_private;
	int err = 0;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (dev->data->dev_started) {
		err = gve_dev_stop(dev);
		if (err != 0)
			PMD_DRV_LOG(ERR, "Failed to stop dev.");
	}

	gve_free_queues(dev);
	gve_teardown_device_resources(priv);
	gve_adminq_free(priv);

	dev->data->mac_addrs = NULL;

	return err;
}

static int
gve_verify_driver_compatibility(struct gve_priv *priv)
{
	const struct rte_memzone *driver_info_mem;
	struct gve_driver_info *driver_info;
	int err;

	driver_info_mem = rte_memzone_reserve_aligned("verify_driver_compatibility",
			sizeof(struct gve_driver_info),
			rte_socket_id(),
			RTE_MEMZONE_IOVA_CONTIG, PAGE_SIZE);

	if (driver_info_mem == NULL) {
		PMD_DRV_LOG(ERR,
		    "Could not alloc memzone for driver compatibility");
		return -ENOMEM;
	}
	driver_info = (struct gve_driver_info *)driver_info_mem->addr;

	*driver_info = (struct gve_driver_info) {
		.os_type = 5, /* DPDK */
		.driver_major = GVE_VERSION_MAJOR,
		.driver_minor = GVE_VERSION_MINOR,
		.driver_sub = GVE_VERSION_SUB,
		.os_version_major = cpu_to_be32(DPDK_VERSION_MAJOR),
		.os_version_minor = cpu_to_be32(DPDK_VERSION_MINOR),
		.os_version_sub = cpu_to_be32(DPDK_VERSION_SUB),
		.driver_capability_flags = {
			cpu_to_be64(GVE_DRIVER_CAPABILITY_FLAGS1),
			cpu_to_be64(GVE_DRIVER_CAPABILITY_FLAGS2),
			cpu_to_be64(GVE_DRIVER_CAPABILITY_FLAGS3),
			cpu_to_be64(GVE_DRIVER_CAPABILITY_FLAGS4),
		},
	};

	populate_driver_version_strings((char *)driver_info->os_version_str1,
			(char *)driver_info->os_version_str2);

	err = gve_adminq_verify_driver_compatibility(priv,
		sizeof(struct gve_driver_info),
		(dma_addr_t)driver_info_mem->iova);
	/* It's ok if the device doesn't support this */
	if (err == -EOPNOTSUPP)
		err = 0;

	rte_memzone_free(driver_info_mem);
	return err;
}

static int
gve_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct gve_priv *priv = dev->data->dev_private;

	dev_info->device = dev->device;
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_queues = priv->max_nb_rxq;
	dev_info->max_tx_queues = priv->max_nb_txq;
	if (gve_is_gqi(priv)) {
		dev_info->min_rx_bufsize = GVE_RX_MIN_BUF_SIZE_GQI;
		dev_info->max_rx_bufsize = GVE_RX_MAX_BUF_SIZE_GQI;
	} else {
		dev_info->min_rx_bufsize = GVE_RX_MIN_BUF_SIZE_DQO;
		dev_info->max_rx_bufsize = GVE_RX_MAX_BUF_SIZE_DQO;
	}

	dev_info->max_rx_pktlen = priv->max_mtu + RTE_ETHER_HDR_LEN;
	dev_info->max_mtu = priv->max_mtu;
	dev_info->min_mtu = RTE_ETHER_MIN_MTU;

	dev_info->rx_offload_capa = RTE_ETH_RX_OFFLOAD_RSS_HASH;
	dev_info->tx_offload_capa =
		RTE_ETH_TX_OFFLOAD_MULTI_SEGS	|
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM	|
		RTE_ETH_TX_OFFLOAD_TCP_CKSUM	|
		RTE_ETH_TX_OFFLOAD_SCTP_CKSUM	|
		RTE_ETH_TX_OFFLOAD_TCP_TSO;

	if (!gve_is_gqi(priv)) {
		dev_info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_IPV4_CKSUM;
		dev_info->rx_offload_capa |=
				RTE_ETH_RX_OFFLOAD_IPV4_CKSUM   |
				RTE_ETH_RX_OFFLOAD_UDP_CKSUM	|
				RTE_ETH_RX_OFFLOAD_TCP_CKSUM	|
				RTE_ETH_RX_OFFLOAD_TCP_LRO;
	}

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_free_thresh = GVE_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_free_thresh = GVE_DEFAULT_TX_FREE_THRESH,
		.tx_rs_thresh = GVE_DEFAULT_TX_RS_THRESH,
		.offloads = 0,
	};

	dev_info->default_rxportconf.ring_size = priv->default_rx_desc_cnt;
	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = priv->max_rx_desc_cnt,
		.nb_min = priv->min_rx_desc_cnt,
		.nb_align = 1,
	};

	dev_info->default_txportconf.ring_size = priv->default_tx_desc_cnt;
	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = priv->max_tx_desc_cnt,
		.nb_min = priv->min_tx_desc_cnt,
		.nb_align = 1,
		.nb_mtu_seg_max = GVE_TX_MAX_DATA_DESCS - 1,
	};

	dev_info->flow_type_rss_offloads = GVE_RTE_RSS_OFFLOAD_ALL;
	dev_info->hash_key_size = GVE_RSS_HASH_KEY_SIZE;
	dev_info->reta_size = GVE_RSS_INDIR_SIZE;

	return 0;
}

static int
gve_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats,
		  struct eth_queue_stats *qstats __rte_unused)
{
	uint16_t i;
	if (gve_is_gqi(dev->data->dev_private))
		gve_get_imissed_from_nic(dev);

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct gve_tx_queue *txq = dev->data->tx_queues[i];
		if (txq == NULL)
			continue;

		stats->opackets += txq->stats.packets;
		stats->obytes += txq->stats.bytes;
		stats->oerrors += txq->stats.errors;
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct gve_rx_queue *rxq = dev->data->rx_queues[i];
		if (rxq == NULL)
			continue;

		stats->ipackets += rxq->stats.packets;
		stats->ibytes += rxq->stats.bytes;
		stats->ierrors += rxq->stats.errors;
		stats->rx_nombuf += rxq->stats.no_mbufs;
		stats->imissed += rxq->stats.imissed;
	}

	return 0;
}

static int
gve_dev_stats_reset(struct rte_eth_dev *dev)
{
	uint16_t i;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct gve_tx_queue *txq = dev->data->tx_queues[i];
		if (txq == NULL)
			continue;

		memset(&txq->stats, 0, sizeof(txq->stats));
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct gve_rx_queue *rxq = dev->data->rx_queues[i];
		if (rxq == NULL)
			continue;

		memset(&rxq->stats, 0, sizeof(rxq->stats));
	}

	return 0;
}

static int
gve_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct gve_priv *priv = dev->data->dev_private;
	int err;

	if (mtu < RTE_ETHER_MIN_MTU || mtu > priv->max_mtu) {
		PMD_DRV_LOG(ERR, "MIN MTU is %u, MAX MTU is %u",
			    RTE_ETHER_MIN_MTU, priv->max_mtu);
		return -EINVAL;
	}

	/* mtu setting is forbidden if port is start */
	if (dev->data->dev_started) {
		PMD_DRV_LOG(ERR, "Port must be stopped before configuration");
		return -EBUSY;
	}

	err = gve_adminq_set_mtu(priv, mtu);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to set mtu as %u err = %d", mtu, err);
		return err;
	}

	return 0;
}

#define TX_QUEUE_STATS_OFFSET(x) offsetof(struct gve_tx_stats, x)
#define RX_QUEUE_STATS_OFFSET(x) offsetof(struct gve_rx_stats, x)

static const struct gve_xstats_name_offset tx_xstats_name_offset[] = {
	{ "packets", TX_QUEUE_STATS_OFFSET(packets) },
	{ "bytes",   TX_QUEUE_STATS_OFFSET(bytes) },
	{ "errors",  TX_QUEUE_STATS_OFFSET(errors) },
};

static const struct gve_xstats_name_offset rx_xstats_name_offset[] = {
	{ "packets",                RX_QUEUE_STATS_OFFSET(packets) },
	{ "bytes",                  RX_QUEUE_STATS_OFFSET(bytes) },
	{ "errors",                 RX_QUEUE_STATS_OFFSET(errors) },
	{ "mbuf_alloc_errors",      RX_QUEUE_STATS_OFFSET(no_mbufs) },
	{ "mbuf_alloc_errors_bulk", RX_QUEUE_STATS_OFFSET(no_mbufs_bulk) },
	{ "imissed",                RX_QUEUE_STATS_OFFSET(imissed) },
};

static int
gve_xstats_count(struct rte_eth_dev *dev)
{
	uint16_t i, count = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		if (dev->data->tx_queues[i])
			count += RTE_DIM(tx_xstats_name_offset);
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (dev->data->rx_queues[i])
			count += RTE_DIM(rx_xstats_name_offset);
	}

	return count;
}

static int
gve_xstats_get(struct rte_eth_dev *dev,
			struct rte_eth_xstat *xstats,
			unsigned int size)
{
	uint16_t i, j, count = gve_xstats_count(dev);
	const char *stats;

	if (gve_is_gqi(dev->data->dev_private))
		gve_get_imissed_from_nic(dev);

	if (xstats == NULL || size < count)
		return count;

	count = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		const struct gve_tx_queue *txq = dev->data->tx_queues[i];
		if (txq == NULL)
			continue;

		stats = (const char *)&txq->stats;
		for (j = 0; j < RTE_DIM(tx_xstats_name_offset); j++, count++) {
			xstats[count].id = count;
			xstats[count].value = *(const uint64_t *)
				(stats + tx_xstats_name_offset[j].offset);
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		const struct gve_rx_queue *rxq = dev->data->rx_queues[i];
		if (rxq == NULL)
			continue;

		stats = (const char *)&rxq->stats;
		for (j = 0; j < RTE_DIM(rx_xstats_name_offset); j++, count++) {
			xstats[count].id = count;
			xstats[count].value = *(const uint64_t *)
				(stats + rx_xstats_name_offset[j].offset);
		}
	}

	return count;
}

static int
gve_xstats_get_names(struct rte_eth_dev *dev,
			struct rte_eth_xstat_name *xstats_names,
			unsigned int size)
{
	uint16_t i, j, count = gve_xstats_count(dev);

	if (xstats_names == NULL || size < count)
		return count;

	count = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		if (dev->data->tx_queues[i] == NULL)
			continue;

		for (j = 0; j < RTE_DIM(tx_xstats_name_offset); j++)
			snprintf(xstats_names[count++].name,
				 RTE_ETH_XSTATS_NAME_SIZE,
				 "tx_q%u_%s", i, tx_xstats_name_offset[j].name);
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (dev->data->rx_queues[i] == NULL)
			continue;

		for (j = 0; j < RTE_DIM(rx_xstats_name_offset); j++)
			snprintf(xstats_names[count++].name,
				 RTE_ETH_XSTATS_NAME_SIZE,
				 "rx_q%u_%s", i, rx_xstats_name_offset[j].name);
	}

	return count;
}


static int
gve_rss_hash_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_conf *rss_conf)
{
	struct gve_priv *priv = dev->data->dev_private;
	struct gve_rss_config gve_rss_conf;
	int rss_reta_size;
	int err;

	if (gve_validate_rss_hf(rss_conf->rss_hf)) {
		PMD_DRV_LOG(ERR, "Unsupported hash function.");
		return -EINVAL;
	}

	if (rss_conf->algorithm != RTE_ETH_HASH_FUNCTION_TOEPLITZ &&
		rss_conf->algorithm != RTE_ETH_HASH_FUNCTION_DEFAULT) {
		PMD_DRV_LOG(ERR, "Device only supports Toeplitz algorithm.");
		return -EINVAL;
	}

	if (rss_conf->rss_key_len) {
		if (rss_conf->rss_key_len != GVE_RSS_HASH_KEY_SIZE) {
			PMD_DRV_LOG(ERR,
				"Invalid hash key size. Only RSS hash key size "
				"of %u supported", GVE_RSS_HASH_KEY_SIZE);
			return -EINVAL;
		}

		if (!rss_conf->rss_key) {
			PMD_DRV_LOG(ERR, "RSS key must be non-null.");
			return -EINVAL;
		}
	} else {
		if (!priv->rss_config.key_size) {
			PMD_DRV_LOG(ERR, "RSS key must be initialized before "
				"any other configuration.");
			return -EINVAL;
		}
		rss_conf->rss_key_len = priv->rss_config.key_size;
	}

	rss_reta_size = priv->rss_config.indir ?
			priv->rss_config.indir_size :
			GVE_RSS_INDIR_SIZE;
	err = gve_init_rss_config(&gve_rss_conf, rss_conf->rss_key_len,
		rss_reta_size);
	if (err)
		return err;

	gve_rss_conf.alg = GVE_RSS_HASH_TOEPLITZ;
	err = gve_update_rss_hash_types(priv, &gve_rss_conf, rss_conf);
	if (err)
		goto err;
	err = gve_update_rss_key(priv, &gve_rss_conf, rss_conf);
	if (err)
		goto err;

	/* Set redirection table to default or preexisting. */
	if (!priv->rss_config.indir)
		gve_generate_rss_reta(dev, &gve_rss_conf);
	else
		memcpy(gve_rss_conf.indir, priv->rss_config.indir,
			gve_rss_conf.indir_size * sizeof(*priv->rss_config.indir));

	err = gve_adminq_configure_rss(priv, &gve_rss_conf);
	if (!err)
		gve_update_priv_rss_config(priv, &gve_rss_conf);

err:
	gve_free_rss_config(&gve_rss_conf);
	return err;
}

static int
gve_rss_hash_conf_get(struct rte_eth_dev *dev,
			struct rte_eth_rss_conf *rss_conf)
{
	struct gve_priv *priv = dev->data->dev_private;

	if (!(dev->data->dev_conf.rxmode.offloads &
			RTE_ETH_RX_OFFLOAD_RSS_HASH)) {
		PMD_DRV_LOG(ERR, "RSS not configured.");
		return -ENOTSUP;
	}


	gve_to_rte_rss_hf(priv->rss_config.hash_types, rss_conf);
	rss_conf->rss_key_len = priv->rss_config.key_size;
	if (rss_conf->rss_key) {
		if (!priv->rss_config.key) {
			PMD_DRV_LOG(ERR, "Unable to retrieve default RSS hash key.");
			return -ENOTSUP;
		}
		memcpy(rss_conf->rss_key, priv->rss_config.key,
			rss_conf->rss_key_len * sizeof(*rss_conf->rss_key));
	}

	return 0;
}

static int
gve_rss_reta_update(struct rte_eth_dev *dev,
	struct rte_eth_rss_reta_entry64 *reta_conf, uint16_t reta_size)
{
	struct gve_priv *priv = dev->data->dev_private;
	struct gve_rss_config gve_rss_conf;
	int table_id;
	int err;
	int i;

	/* RSS key must be set before the redirection table can be set. */
	if (!priv->rss_config.key || priv->rss_config.key_size == 0) {
		PMD_DRV_LOG(ERR, "RSS hash key msut be set before the "
			"redirection table can be updated.");
		return -ENOTSUP;
	}

	if (reta_size != GVE_RSS_INDIR_SIZE) {
		PMD_DRV_LOG(ERR, "Redirection table must have %hu elements",
			(uint16_t)GVE_RSS_INDIR_SIZE);
		return -EINVAL;
	}

	err = gve_init_rss_config_from_priv(priv, &gve_rss_conf);
	if (err) {
		PMD_DRV_LOG(ERR, "Error allocating new RSS config.");
		return err;
	}

	table_id = 0;
	for (i = 0; i < priv->rss_config.indir_size; i++) {
		int table_entry = i % RTE_ETH_RETA_GROUP_SIZE;
		if (reta_conf[table_id].mask & (1ULL << table_entry))
			gve_rss_conf.indir[i] =
				reta_conf[table_id].reta[table_entry];

		if (table_entry == RTE_ETH_RETA_GROUP_SIZE - 1)
			table_id++;
	}

	err = gve_adminq_configure_rss(priv, &gve_rss_conf);
	if (err)
		PMD_DRV_LOG(ERR, "Problem configuring RSS with device.");
	else
		gve_update_priv_rss_config(priv, &gve_rss_conf);

	gve_free_rss_config(&gve_rss_conf);
	return err;
}

static int
gve_rss_reta_query(struct rte_eth_dev *dev,
	struct rte_eth_rss_reta_entry64 *reta_conf, uint16_t reta_size)
{
	struct gve_priv *priv = dev->data->dev_private;
	int table_id;
	int i;

	if (!(dev->data->dev_conf.rxmode.offloads &
		RTE_ETH_RX_OFFLOAD_RSS_HASH)) {
		PMD_DRV_LOG(ERR, "RSS not configured.");
		return -ENOTSUP;
	}

	/* RSS key must be set before the redirection table can be queried. */
	if (!priv->rss_config.key) {
		PMD_DRV_LOG(ERR, "RSS hash key must be set before the "
			"redirection table can be initialized.");
		return -ENOTSUP;
	}

	if (reta_size != priv->rss_config.indir_size) {
		PMD_DRV_LOG(ERR, "RSS redirection table must have %d entries.",
			priv->rss_config.indir_size);
		return -EINVAL;
	}

	table_id = 0;
	for (i = 0; i < priv->rss_config.indir_size; i++) {
		int table_entry = i % RTE_ETH_RETA_GROUP_SIZE;
		if (reta_conf[table_id].mask & (1ULL << table_entry))
			reta_conf[table_id].reta[table_entry] =
				priv->rss_config.indir[i];

		if (table_entry == RTE_ETH_RETA_GROUP_SIZE - 1)
			table_id++;
	}

	return 0;
}

static const struct eth_dev_ops gve_eth_dev_ops = {
	.dev_configure        = gve_dev_configure,
	.dev_start            = gve_dev_start,
	.dev_stop             = gve_dev_stop,
	.dev_close            = gve_dev_close,
	.dev_infos_get        = gve_dev_info_get,
	.rx_queue_setup       = gve_rx_queue_setup,
	.tx_queue_setup       = gve_tx_queue_setup,
	.rx_queue_release     = gve_rx_queue_release,
	.tx_queue_release     = gve_tx_queue_release,
	.rx_queue_start       = gve_rx_queue_start,
	.tx_queue_start       = gve_tx_queue_start,
	.rx_queue_stop        = gve_rx_queue_stop,
	.tx_queue_stop        = gve_tx_queue_stop,
	.link_update          = gve_link_update,
	.stats_get            = gve_dev_stats_get,
	.stats_reset          = gve_dev_stats_reset,
	.mtu_set              = gve_dev_mtu_set,
	.xstats_get           = gve_xstats_get,
	.xstats_get_names     = gve_xstats_get_names,
	.rss_hash_update      = gve_rss_hash_update,
	.rss_hash_conf_get    = gve_rss_hash_conf_get,
	.reta_update          = gve_rss_reta_update,
	.reta_query           = gve_rss_reta_query,
};

static const struct eth_dev_ops gve_eth_dev_ops_dqo = {
	.dev_configure        = gve_dev_configure,
	.dev_start            = gve_dev_start,
	.dev_stop             = gve_dev_stop,
	.dev_close            = gve_dev_close,
	.dev_infos_get        = gve_dev_info_get,
	.rx_queue_setup       = gve_rx_queue_setup_dqo,
	.tx_queue_setup       = gve_tx_queue_setup_dqo,
	.rx_queue_release     = gve_rx_queue_release_dqo,
	.tx_queue_release     = gve_tx_queue_release_dqo,
	.rx_queue_start       = gve_rx_queue_start_dqo,
	.tx_queue_start       = gve_tx_queue_start_dqo,
	.rx_queue_stop        = gve_rx_queue_stop_dqo,
	.tx_queue_stop        = gve_tx_queue_stop_dqo,
	.link_update          = gve_link_update,
	.stats_get            = gve_dev_stats_get,
	.stats_reset          = gve_dev_stats_reset,
	.mtu_set              = gve_dev_mtu_set,
	.xstats_get           = gve_xstats_get,
	.xstats_get_names     = gve_xstats_get_names,
	.rss_hash_update      = gve_rss_hash_update,
	.rss_hash_conf_get    = gve_rss_hash_conf_get,
	.reta_update          = gve_rss_reta_update,
	.reta_query           = gve_rss_reta_query,
};

static int
pci_dev_msix_vec_count(struct rte_pci_device *pdev)
{
	off_t msix_pos = rte_pci_find_capability(pdev, RTE_PCI_CAP_ID_MSIX);
	uint16_t control;

	if (msix_pos > 0 && rte_pci_read_config(pdev, &control, sizeof(control),
			msix_pos + RTE_PCI_MSIX_FLAGS) == sizeof(control))
		return (control & RTE_PCI_MSIX_FLAGS_QSIZE) + 1;

	return 0;
}

static int
gve_setup_device_resources(struct gve_priv *priv)
{
	char z_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;
	int err = 0;

	snprintf(z_name, sizeof(z_name), "gve_%s_cnt_arr", priv->pci_dev->device.name);
	mz = rte_memzone_reserve_aligned(z_name,
					 priv->num_event_counters * sizeof(*priv->cnt_array),
					 rte_socket_id(), RTE_MEMZONE_IOVA_CONTIG,
					 PAGE_SIZE);
	if (mz == NULL) {
		PMD_DRV_LOG(ERR, "Could not alloc memzone for count array");
		return -ENOMEM;
	}
	priv->cnt_array = (rte_be32_t *)mz->addr;
	priv->cnt_array_mz = mz;

	snprintf(z_name, sizeof(z_name), "gve_%s_irqmz", priv->pci_dev->device.name);
	mz = rte_memzone_reserve_aligned(z_name,
					 sizeof(*priv->irq_dbs) * (priv->num_ntfy_blks),
					 rte_socket_id(), RTE_MEMZONE_IOVA_CONTIG,
					 PAGE_SIZE);
	if (mz == NULL) {
		PMD_DRV_LOG(ERR, "Could not alloc memzone for irq_dbs");
		err = -ENOMEM;
		goto free_cnt_array;
	}
	priv->irq_dbs = (struct gve_irq_db *)mz->addr;
	priv->irq_dbs_mz = mz;

	err = gve_adminq_configure_device_resources(priv,
						    priv->cnt_array_mz->iova,
						    priv->num_event_counters,
						    priv->irq_dbs_mz->iova,
						    priv->num_ntfy_blks);
	if (unlikely(err)) {
		PMD_DRV_LOG(ERR, "Could not config device resources: err=%d", err);
		goto free_irq_dbs;
	}
	if (!gve_is_gqi(priv)) {
		priv->ptype_lut_dqo = rte_zmalloc("gve_ptype_lut_dqo",
			sizeof(struct gve_ptype_lut), 0);
		if (priv->ptype_lut_dqo == NULL) {
			PMD_DRV_LOG(ERR, "Failed to alloc ptype lut.");
			err = -ENOMEM;
			goto free_irq_dbs;
		}
		err = gve_adminq_get_ptype_map_dqo(priv, priv->ptype_lut_dqo);
		if (unlikely(err)) {
			PMD_DRV_LOG(ERR, "Failed to get ptype map: err=%d", err);
			goto free_ptype_lut;
		}
	}

	gve_set_device_resources_ok(priv);

	return 0;
free_ptype_lut:
	rte_free(priv->ptype_lut_dqo);
	priv->ptype_lut_dqo = NULL;
free_irq_dbs:
	gve_free_irq_db(priv);
free_cnt_array:
	gve_free_counter_array(priv);

	return err;
}

static void
gve_set_default_ring_size_bounds(struct gve_priv *priv)
{
	priv->max_tx_desc_cnt = GVE_DEFAULT_MAX_RING_SIZE;
	priv->max_rx_desc_cnt = GVE_DEFAULT_MAX_RING_SIZE;
	priv->min_tx_desc_cnt = GVE_DEFAULT_MIN_TX_RING_SIZE;
	priv->min_rx_desc_cnt = GVE_DEFAULT_MIN_RX_RING_SIZE;
}

static int
gve_init_priv(struct gve_priv *priv, bool skip_describe_device)
{
	int num_ntfy;
	int err;

	/* Set up the adminq */
	err = gve_adminq_alloc(priv);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to alloc admin queue: err=%d", err);
		return err;
	}
	err = gve_verify_driver_compatibility(priv);
	if (err) {
		PMD_DRV_LOG(ERR, "Could not verify driver compatibility: err=%d", err);
		goto free_adminq;
	}

	/* Set default descriptor counts */
	gve_set_default_ring_size_bounds(priv);

	if (skip_describe_device)
		goto setup_device;

	/* Get the initial information we need from the device */
	err = gve_adminq_describe_device(priv);
	if (err) {
		PMD_DRV_LOG(ERR, "Could not get device information: err=%d", err);
		goto free_adminq;
	}

	num_ntfy = pci_dev_msix_vec_count(priv->pci_dev);
	if (num_ntfy <= 0) {
		PMD_DRV_LOG(ERR, "Could not count MSI-x vectors");
		err = -EIO;
		goto free_adminq;
	} else if (num_ntfy < GVE_MIN_MSIX) {
		PMD_DRV_LOG(ERR, "GVE needs at least %d MSI-x vectors, but only has %d",
			    GVE_MIN_MSIX, num_ntfy);
		err = -EINVAL;
		goto free_adminq;
	}

	priv->num_registered_pages = 0;

	/* gvnic has one Notification Block per MSI-x vector, except for the
	 * management vector
	 */
	priv->num_ntfy_blks = (num_ntfy - 1) & ~0x1;
	priv->mgmt_msix_idx = priv->num_ntfy_blks;

	priv->max_nb_txq = RTE_MIN(priv->max_nb_txq, priv->num_ntfy_blks / 2);
	priv->max_nb_rxq = RTE_MIN(priv->max_nb_rxq, priv->num_ntfy_blks / 2);

	if (priv->default_num_queues > 0) {
		priv->max_nb_txq = RTE_MIN(priv->default_num_queues, priv->max_nb_txq);
		priv->max_nb_rxq = RTE_MIN(priv->default_num_queues, priv->max_nb_rxq);
	}

	PMD_DRV_LOG(INFO, "Max TX queues %d, Max RX queues %d",
		    priv->max_nb_txq, priv->max_nb_rxq);

setup_device:
	err = gve_setup_device_resources(priv);
	if (!err)
		return 0;
free_adminq:
	gve_adminq_free(priv);
	return err;
}

static int
gve_dev_init(struct rte_eth_dev *eth_dev)
{
	struct gve_priv *priv = eth_dev->data->dev_private;
	int max_tx_queues, max_rx_queues;
	struct rte_pci_device *pci_dev;
	struct gve_registers *reg_bar;
	rte_be32_t *db_bar;
	int err;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		if (gve_is_gqi(priv)) {
			gve_set_rx_function(eth_dev);
			gve_set_tx_function(eth_dev);
			eth_dev->dev_ops = &gve_eth_dev_ops;
		} else {
			gve_set_rx_function_dqo(eth_dev);
			gve_set_tx_function_dqo(eth_dev);
			eth_dev->dev_ops = &gve_eth_dev_ops_dqo;
		}
		return 0;
	}

	pci_dev = RTE_DEV_TO_PCI(eth_dev->device);

	reg_bar = pci_dev->mem_resource[GVE_REG_BAR].addr;
	if (!reg_bar) {
		PMD_DRV_LOG(ERR, "Failed to map pci bar!");
		return -ENOMEM;
	}

	db_bar = pci_dev->mem_resource[GVE_DB_BAR].addr;
	if (!db_bar) {
		PMD_DRV_LOG(ERR, "Failed to map doorbell bar!");
		return -ENOMEM;
	}

	gve_write_version(&reg_bar->driver_version);
	/* Get max queues to alloc etherdev */
	max_tx_queues = ioread32be(&reg_bar->max_tx_queues);
	max_rx_queues = ioread32be(&reg_bar->max_rx_queues);

	priv->reg_bar0 = reg_bar;
	priv->db_bar2 = db_bar;
	priv->pci_dev = pci_dev;
	priv->state_flags = 0x0;

	priv->max_nb_txq = max_tx_queues;
	priv->max_nb_rxq = max_rx_queues;

	err = gve_init_priv(priv, false);
	if (err)
		return err;

	if (gve_is_gqi(priv)) {
		eth_dev->dev_ops = &gve_eth_dev_ops;
		gve_set_rx_function(eth_dev);
		gve_set_tx_function(eth_dev);
	} else {
		eth_dev->dev_ops = &gve_eth_dev_ops_dqo;
		gve_set_rx_function_dqo(eth_dev);
		gve_set_tx_function_dqo(eth_dev);
	}

	eth_dev->data->mac_addrs = &priv->dev_addr;

	return 0;
}

static int
gve_pci_probe(__rte_unused struct rte_pci_driver *pci_drv,
	      struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev, sizeof(struct gve_priv), gve_dev_init);
}

static int
gve_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, gve_dev_close);
}

static const struct rte_pci_id pci_id_gve_map[] = {
	{ RTE_PCI_DEVICE(GOOGLE_VENDOR_ID, GVE_DEV_ID) },
	{ .device_id = 0 },
};

static struct rte_pci_driver rte_gve_pmd = {
	.id_table = pci_id_gve_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = gve_pci_probe,
	.remove = gve_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_gve, rte_gve_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_gve, pci_id_gve_map);
RTE_PMD_REGISTER_KMOD_DEP(net_gve, "* igb_uio | vfio-pci | nic_uio");
RTE_LOG_REGISTER_SUFFIX(gve_logtype_driver, driver, NOTICE);
