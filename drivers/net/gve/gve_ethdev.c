/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Intel Corporation
 */

#include "gve_ethdev.h"
#include "base/gve_adminq.h"
#include "base/gve_register.h"
#include "base/gve_osdep.h"
#include "gve_version.h"

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

static int
gve_alloc_queue_page_list(struct gve_priv *priv, uint32_t id, uint32_t pages)
{
	char z_name[RTE_MEMZONE_NAMESIZE];
	struct gve_queue_page_list *qpl;
	const struct rte_memzone *mz;
	dma_addr_t page_bus;
	uint32_t i;

	if (priv->num_registered_pages + pages >
	    priv->max_registered_pages) {
		PMD_DRV_LOG(ERR, "Pages %" PRIu64 " > max registered pages %" PRIu64,
			    priv->num_registered_pages + pages,
			    priv->max_registered_pages);
		return -EINVAL;
	}
	qpl = &priv->qpl[id];
	snprintf(z_name, sizeof(z_name), "gve_%s_qpl%d", priv->pci_dev->device.name, id);
	mz = rte_memzone_reserve_aligned(z_name, pages * PAGE_SIZE,
					 rte_socket_id(),
					 RTE_MEMZONE_IOVA_CONTIG, PAGE_SIZE);
	if (mz == NULL) {
		PMD_DRV_LOG(ERR, "Failed to alloc %s.", z_name);
		return -ENOMEM;
	}
	qpl->page_buses = rte_zmalloc("qpl page buses", pages * sizeof(dma_addr_t), 0);
	if (qpl->page_buses == NULL) {
		PMD_DRV_LOG(ERR, "Failed to alloc qpl %u page buses", id);
		return -ENOMEM;
	}
	page_bus = mz->iova;
	for (i = 0; i < pages; i++) {
		qpl->page_buses[i] = page_bus;
		page_bus += PAGE_SIZE;
	}
	qpl->id = id;
	qpl->mz = mz;
	qpl->num_entries = pages;

	priv->num_registered_pages += pages;

	return 0;
}

static void
gve_free_qpls(struct gve_priv *priv)
{
	uint16_t nb_txqs = priv->max_nb_txq;
	uint16_t nb_rxqs = priv->max_nb_rxq;
	uint32_t i;

	if (priv->queue_format != GVE_GQI_QPL_FORMAT)
		return;

	for (i = 0; i < nb_txqs + nb_rxqs; i++) {
		if (priv->qpl[i].mz != NULL)
			rte_memzone_free(priv->qpl[i].mz);
		rte_free(priv->qpl[i].page_buses);
	}

	rte_free(priv->qpl);
}

static int
gve_dev_configure(struct rte_eth_dev *dev)
{
	struct gve_priv *priv = dev->data->dev_private;

	if (dev->data->dev_conf.rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG)
		dev->data->dev_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;

	if (dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO)
		priv->enable_rsc = 1;

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
	for (i = 0; i < num_queues; i++)
		if (gve_tx_queue_start(dev, i) != 0) {
			PMD_DRV_LOG(ERR, "Fail to start Tx queue %d", i);
			goto err_tx;
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

	return 0;

err_rx:
	gve_stop_rx_queues(dev);
err_tx:
	gve_stop_tx_queues(dev);
	return ret;
}

static int
gve_dev_start(struct rte_eth_dev *dev)
{
	int ret;

	ret = gve_start_queues(dev);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to start queues");
		return ret;
	}

	dev->data->dev_started = 1;
	gve_link_update(dev, 0);

	return 0;
}

static int
gve_dev_stop(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = RTE_ETH_LINK_DOWN;

	gve_stop_tx_queues(dev);
	gve_stop_rx_queues(dev);

	dev->data->dev_started = 0;

	return 0;
}

static int
gve_dev_close(struct rte_eth_dev *dev)
{
	struct gve_priv *priv = dev->data->dev_private;
	int err = 0;
	uint16_t i;

	if (dev->data->dev_started) {
		err = gve_dev_stop(dev);
		if (err != 0)
			PMD_DRV_LOG(ERR, "Failed to stop dev.");
	}

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

	gve_free_qpls(priv);
	rte_free(priv->adminq);

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
	dev_info->min_rx_bufsize = GVE_MIN_BUF_SIZE;
	dev_info->max_rx_pktlen = GVE_MAX_RX_PKTLEN;
	dev_info->max_mtu = GVE_MAX_MTU;
	dev_info->min_mtu = GVE_MIN_MTU;

	dev_info->rx_offload_capa = 0;
	dev_info->tx_offload_capa =
		RTE_ETH_TX_OFFLOAD_MULTI_SEGS	|
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM	|
		RTE_ETH_TX_OFFLOAD_TCP_CKSUM	|
		RTE_ETH_TX_OFFLOAD_SCTP_CKSUM	|
		RTE_ETH_TX_OFFLOAD_TCP_TSO;

	if (priv->queue_format == GVE_DQO_RDA_FORMAT)
		dev_info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_TCP_LRO;

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

	dev_info->default_rxportconf.ring_size = priv->rx_desc_cnt;
	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = priv->rx_desc_cnt,
		.nb_min = priv->rx_desc_cnt,
		.nb_align = 1,
	};

	dev_info->default_txportconf.ring_size = priv->tx_desc_cnt;
	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = priv->tx_desc_cnt,
		.nb_min = priv->tx_desc_cnt,
		.nb_align = 1,
	};

	return 0;
}

static int
gve_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	uint16_t i;

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
};

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
gve_teardown_device_resources(struct gve_priv *priv)
{
	int err;

	/* Tell device its resources are being freed */
	if (gve_get_device_resources_ok(priv)) {
		err = gve_adminq_deconfigure_device_resources(priv);
		if (err)
			PMD_DRV_LOG(ERR, "Could not deconfigure device resources: err=%d", err);
	}
	gve_free_counter_array(priv);
	gve_free_irq_db(priv);
	gve_clear_device_resources_ok(priv);
}

static uint8_t
pci_dev_find_capability(struct rte_pci_device *pdev, int cap)
{
	uint8_t pos, id;
	uint16_t ent;
	int loops;
	int ret;

	ret = rte_pci_read_config(pdev, &pos, sizeof(pos), PCI_CAPABILITY_LIST);
	if (ret != sizeof(pos))
		return 0;

	loops = (PCI_CFG_SPACE_SIZE - PCI_STD_HEADER_SIZEOF) / PCI_CAP_SIZEOF;

	while (pos && loops--) {
		ret = rte_pci_read_config(pdev, &ent, sizeof(ent), pos);
		if (ret != sizeof(ent))
			return 0;

		id = ent & 0xff;
		if (id == 0xff)
			break;

		if (id == cap)
			return pos;

		pos = (ent >> 8);
	}

	return 0;
}

static int
pci_dev_msix_vec_count(struct rte_pci_device *pdev)
{
	uint8_t msix_cap = pci_dev_find_capability(pdev, PCI_CAP_ID_MSIX);
	uint16_t control;
	int ret;

	if (!msix_cap)
		return 0;

	ret = rte_pci_read_config(pdev, &control, sizeof(control), msix_cap + PCI_MSIX_FLAGS);
	if (ret != sizeof(control))
		return 0;

	return (control & PCI_MSIX_FLAGS_QSIZE) + 1;
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
	return 0;

free_irq_dbs:
	gve_free_irq_db(priv);
free_cnt_array:
	gve_free_counter_array(priv);

	return err;
}

static int
gve_init_priv(struct gve_priv *priv, bool skip_describe_device)
{
	uint16_t pages;
	int num_ntfy;
	uint32_t i;
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

	/* In GQI_QPL queue format:
	 * Allocate queue page lists according to max queue number
	 * tx qpl id should start from 0 while rx qpl id should start
	 * from priv->max_nb_txq
	 */
	if (priv->queue_format == GVE_GQI_QPL_FORMAT) {
		priv->qpl = rte_zmalloc("gve_qpl",
					(priv->max_nb_txq + priv->max_nb_rxq) *
					sizeof(struct gve_queue_page_list), 0);
		if (priv->qpl == NULL) {
			PMD_DRV_LOG(ERR, "Failed to alloc qpl.");
			err = -ENOMEM;
			goto free_adminq;
		}

		for (i = 0; i < priv->max_nb_txq + priv->max_nb_rxq; i++) {
			if (i < priv->max_nb_txq)
				pages = priv->tx_pages_per_qpl;
			else
				pages = priv->rx_data_slot_cnt;
			err = gve_alloc_queue_page_list(priv, i, pages);
			if (err != 0) {
				PMD_DRV_LOG(ERR, "Failed to alloc qpl %u.", i);
				goto err_qpl;
			}
		}
	}

setup_device:
	err = gve_setup_device_resources(priv);
	if (!err)
		return 0;
err_qpl:
	gve_free_qpls(priv);
free_adminq:
	gve_adminq_free(priv);
	return err;
}

static void
gve_teardown_priv_resources(struct gve_priv *priv)
{
	gve_teardown_device_resources(priv);
	gve_adminq_free(priv);
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

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

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
gve_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct gve_priv *priv = eth_dev->data->dev_private;

	gve_teardown_priv_resources(priv);

	eth_dev->data->mac_addrs = NULL;

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
	return rte_eth_dev_pci_generic_remove(pci_dev, gve_dev_uninit);
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
RTE_PMD_REGISTER_KMOD_DEP(net_gve, "* igb_uio | vfio-pci");
RTE_LOG_REGISTER_SUFFIX(gve_logtype_driver, driver, NOTICE);
