/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2018 Microsoft Corporation
 * Copyright(c) 2013-2016 Brocade Communications Systems, Inc.
 * All rights reserved.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <rte_ethdev.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_memzone.h>
#include <rte_devargs.h>
#include <rte_malloc.h>
#include <rte_kvargs.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_dev.h>
#include <rte_bus_vmbus.h>

#include "hn_logs.h"
#include "hn_var.h"
#include "hn_rndis.h"
#include "hn_nvs.h"
#include "ndis.h"

#define HN_TX_OFFLOAD_CAPS (DEV_TX_OFFLOAD_IPV4_CKSUM | \
			    DEV_TX_OFFLOAD_TCP_CKSUM  | \
			    DEV_TX_OFFLOAD_UDP_CKSUM  | \
			    DEV_TX_OFFLOAD_TCP_TSO    | \
			    DEV_TX_OFFLOAD_MULTI_SEGS | \
			    DEV_TX_OFFLOAD_VLAN_INSERT)

#define HN_RX_OFFLOAD_CAPS (DEV_RX_OFFLOAD_CHECKSUM | \
			    DEV_RX_OFFLOAD_VLAN_STRIP | \
			    DEV_RX_OFFLOAD_RSS_HASH)

int hn_logtype_init;
int hn_logtype_driver;

struct hn_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned int offset;
};

static const struct hn_xstats_name_off hn_stat_strings[] = {
	{ "good_packets",           offsetof(struct hn_stats, packets) },
	{ "good_bytes",             offsetof(struct hn_stats, bytes) },
	{ "errors",                 offsetof(struct hn_stats, errors) },
	{ "ring full",              offsetof(struct hn_stats, ring_full) },
	{ "multicast_packets",      offsetof(struct hn_stats, multicast) },
	{ "broadcast_packets",      offsetof(struct hn_stats, broadcast) },
	{ "undersize_packets",      offsetof(struct hn_stats, size_bins[0]) },
	{ "size_64_packets",        offsetof(struct hn_stats, size_bins[1]) },
	{ "size_65_127_packets",    offsetof(struct hn_stats, size_bins[2]) },
	{ "size_128_255_packets",   offsetof(struct hn_stats, size_bins[3]) },
	{ "size_256_511_packets",   offsetof(struct hn_stats, size_bins[4]) },
	{ "size_512_1023_packets",  offsetof(struct hn_stats, size_bins[5]) },
	{ "size_1024_1518_packets", offsetof(struct hn_stats, size_bins[6]) },
	{ "size_1519_max_packets",  offsetof(struct hn_stats, size_bins[7]) },
};

/* The default RSS key.
 * This value is the same as MLX5 so that flows will be
 * received on same path for both VF and synthetic NIC.
 */
static const uint8_t rss_default_key[NDIS_HASH_KEYSIZE_TOEPLITZ] = {
	0x2c, 0xc6, 0x81, 0xd1,	0x5b, 0xdb, 0xf4, 0xf7,
	0xfc, 0xa2, 0x83, 0x19,	0xdb, 0x1a, 0x3e, 0x94,
	0x6b, 0x9e, 0x38, 0xd9,	0x2c, 0x9c, 0x03, 0xd1,
	0xad, 0x99, 0x44, 0xa7,	0xd9, 0x56, 0x3d, 0x59,
	0x06, 0x3c, 0x25, 0xf3,	0xfc, 0x1f, 0xdc, 0x2a,
};

static struct rte_eth_dev *
eth_dev_vmbus_allocate(struct rte_vmbus_device *dev, size_t private_data_size)
{
	struct rte_eth_dev *eth_dev;
	const char *name;

	if (!dev)
		return NULL;

	name = dev->device.name;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		eth_dev = rte_eth_dev_allocate(name);
		if (!eth_dev) {
			PMD_DRV_LOG(NOTICE, "can not allocate rte ethdev");
			return NULL;
		}

		if (private_data_size) {
			eth_dev->data->dev_private =
				rte_zmalloc_socket(name, private_data_size,
						     RTE_CACHE_LINE_SIZE, dev->device.numa_node);
			if (!eth_dev->data->dev_private) {
				PMD_DRV_LOG(NOTICE, "can not allocate driver data");
				rte_eth_dev_release_port(eth_dev);
				return NULL;
			}
		}
	} else {
		eth_dev = rte_eth_dev_attach_secondary(name);
		if (!eth_dev) {
			PMD_DRV_LOG(NOTICE, "can not attach secondary");
			return NULL;
		}
	}

	eth_dev->device = &dev->device;

	/* interrupt is simulated */
	dev->intr_handle.type = RTE_INTR_HANDLE_EXT;
	eth_dev->data->dev_flags |= RTE_ETH_DEV_INTR_LSC;
	eth_dev->intr_handle = &dev->intr_handle;

	/* allow ethdev to remove on close */
	eth_dev->data->dev_flags |= RTE_ETH_DEV_CLOSE_REMOVE;

	return eth_dev;
}

static void
eth_dev_vmbus_release(struct rte_eth_dev *eth_dev)
{
	/* free ether device */
	rte_eth_dev_release_port(eth_dev);

	eth_dev->device = NULL;
	eth_dev->intr_handle = NULL;
}

/* handle "latency=X" from devargs */
static int hn_set_latency(const char *key, const char *value, void *opaque)
{
	struct hn_data *hv = opaque;
	char *endp = NULL;
	unsigned long lat;

	errno = 0;
	lat = strtoul(value, &endp, 0);

	if (*value == '\0' || *endp != '\0') {
		PMD_DRV_LOG(ERR, "invalid parameter %s=%s", key, value);
		return -EINVAL;
	}

	PMD_DRV_LOG(DEBUG, "set latency %lu usec", lat);

	hv->latency = lat * 1000;	/* usec to nsec */
	return 0;
}

/* Parse device arguments */
static int hn_parse_args(const struct rte_eth_dev *dev)
{
	struct hn_data *hv = dev->data->dev_private;
	struct rte_devargs *devargs = dev->device->devargs;
	static const char * const valid_keys[] = {
		"latency",
		NULL
	};
	struct rte_kvargs *kvlist;
	int ret;

	if (!devargs)
		return 0;

	PMD_INIT_LOG(DEBUG, "device args %s %s",
		     devargs->name, devargs->args);

	kvlist = rte_kvargs_parse(devargs->args, valid_keys);
	if (!kvlist) {
		PMD_DRV_LOG(NOTICE, "invalid parameters");
		return -EINVAL;
	}

	ret = rte_kvargs_process(kvlist, "latency", hn_set_latency, hv);
	if (ret)
		PMD_DRV_LOG(ERR, "Unable to process latency arg\n");

	rte_kvargs_free(kvlist);
	return ret;
}

/* Update link status.
 * Note: the DPDK definition of "wait_to_complete"
 *   means block this call until link is up.
 *   which is not worth supporting.
 */
int
hn_dev_link_update(struct rte_eth_dev *dev,
		   int wait_to_complete)
{
	struct hn_data *hv = dev->data->dev_private;
	struct rte_eth_link link, old;
	int error;

	old = dev->data->dev_link;

	error = hn_rndis_get_linkstatus(hv);
	if (error)
		return error;

	hn_rndis_get_linkspeed(hv);

	hn_vf_link_update(dev, wait_to_complete);

	link = (struct rte_eth_link) {
		.link_duplex = ETH_LINK_FULL_DUPLEX,
		.link_autoneg = ETH_LINK_SPEED_FIXED,
		.link_speed = hv->link_speed / 10000,
	};

	if (hv->link_status == NDIS_MEDIA_STATE_CONNECTED)
		link.link_status = ETH_LINK_UP;
	else
		link.link_status = ETH_LINK_DOWN;

	if (old.link_status == link.link_status)
		return 0;

	PMD_INIT_LOG(DEBUG, "Port %d is %s", dev->data->port_id,
		     (link.link_status == ETH_LINK_UP) ? "up" : "down");

	return rte_eth_linkstatus_set(dev, &link);
}

static int hn_dev_info_get(struct rte_eth_dev *dev,
			   struct rte_eth_dev_info *dev_info)
{
	struct hn_data *hv = dev->data->dev_private;
	int rc;

	dev_info->speed_capa = ETH_LINK_SPEED_10G;
	dev_info->min_rx_bufsize = HN_MIN_RX_BUF_SIZE;
	dev_info->max_rx_pktlen  = HN_MAX_XFER_LEN;
	dev_info->max_mac_addrs  = 1;

	dev_info->hash_key_size = NDIS_HASH_KEYSIZE_TOEPLITZ;
	dev_info->flow_type_rss_offloads = hv->rss_offloads;
	dev_info->reta_size = ETH_RSS_RETA_SIZE_128;

	dev_info->max_rx_queues = hv->max_queues;
	dev_info->max_tx_queues = hv->max_queues;

	dev_info->tx_desc_lim.nb_min = 1;
	dev_info->tx_desc_lim.nb_max = 4096;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* fills in rx and tx offload capability */
	rc = hn_rndis_get_offload(hv, dev_info);
	if (rc != 0)
		return rc;

	/* merges the offload and queues of vf */
	return hn_vf_info_get(hv, dev_info);
}

static int hn_rss_reta_update(struct rte_eth_dev *dev,
			      struct rte_eth_rss_reta_entry64 *reta_conf,
			      uint16_t reta_size)
{
	struct hn_data *hv = dev->data->dev_private;
	unsigned int i;
	int err;

	PMD_INIT_FUNC_TRACE();

	if (reta_size != NDIS_HASH_INDCNT) {
		PMD_DRV_LOG(ERR, "Hash lookup table size does not match NDIS");
		return -EINVAL;
	}

	for (i = 0; i < NDIS_HASH_INDCNT; i++) {
		uint16_t idx = i / RTE_RETA_GROUP_SIZE;
		uint16_t shift = i % RTE_RETA_GROUP_SIZE;
		uint64_t mask = (uint64_t)1 << shift;

		if (reta_conf[idx].mask & mask)
			hv->rss_ind[i] = reta_conf[idx].reta[shift];
	}

	err = hn_rndis_conf_rss(hv, NDIS_RSS_FLAG_DISABLE);
	if (err) {
		PMD_DRV_LOG(NOTICE,
			"rss disable failed");
		return err;
	}

	err = hn_rndis_conf_rss(hv, 0);
	if (err) {
		PMD_DRV_LOG(NOTICE,
			    "reta reconfig failed");
		return err;
	}

	return hn_vf_reta_hash_update(dev, reta_conf, reta_size);
}

static int hn_rss_reta_query(struct rte_eth_dev *dev,
			     struct rte_eth_rss_reta_entry64 *reta_conf,
			     uint16_t reta_size)
{
	struct hn_data *hv = dev->data->dev_private;
	unsigned int i;

	PMD_INIT_FUNC_TRACE();

	if (reta_size != NDIS_HASH_INDCNT) {
		PMD_DRV_LOG(ERR, "Hash lookup table size does not match NDIS");
		return -EINVAL;
	}

	for (i = 0; i < NDIS_HASH_INDCNT; i++) {
		uint16_t idx = i / RTE_RETA_GROUP_SIZE;
		uint16_t shift = i % RTE_RETA_GROUP_SIZE;
		uint64_t mask = (uint64_t)1 << shift;

		if (reta_conf[idx].mask & mask)
			reta_conf[idx].reta[shift] = hv->rss_ind[i];
	}
	return 0;
}

static void hn_rss_hash_init(struct hn_data *hv,
			     const struct rte_eth_rss_conf *rss_conf)
{
	/* Convert from DPDK RSS hash flags to NDIS hash flags */
	hv->rss_hash = NDIS_HASH_FUNCTION_TOEPLITZ;

	if (rss_conf->rss_hf & ETH_RSS_IPV4)
		hv->rss_hash |= NDIS_HASH_IPV4;
	if (rss_conf->rss_hf & ETH_RSS_NONFRAG_IPV4_TCP)
		hv->rss_hash |= NDIS_HASH_TCP_IPV4;
	if (rss_conf->rss_hf & ETH_RSS_IPV6)
		hv->rss_hash |=  NDIS_HASH_IPV6;
	if (rss_conf->rss_hf & ETH_RSS_IPV6_EX)
		hv->rss_hash |=  NDIS_HASH_IPV6_EX;
	if (rss_conf->rss_hf & ETH_RSS_NONFRAG_IPV6_TCP)
		hv->rss_hash |= NDIS_HASH_TCP_IPV6;
	if (rss_conf->rss_hf & ETH_RSS_IPV6_TCP_EX)
		hv->rss_hash |= NDIS_HASH_TCP_IPV6_EX;

	memcpy(hv->rss_key, rss_conf->rss_key ? : rss_default_key,
	       NDIS_HASH_KEYSIZE_TOEPLITZ);
}

static int hn_rss_hash_update(struct rte_eth_dev *dev,
			      struct rte_eth_rss_conf *rss_conf)
{
	struct hn_data *hv = dev->data->dev_private;
	int err;

	PMD_INIT_FUNC_TRACE();

	err = hn_rndis_conf_rss(hv, NDIS_RSS_FLAG_DISABLE);
	if (err) {
		PMD_DRV_LOG(NOTICE,
			    "rss disable failed");
		return err;
	}

	hn_rss_hash_init(hv, rss_conf);

	err = hn_rndis_conf_rss(hv, 0);
	if (err) {
		PMD_DRV_LOG(NOTICE,
			    "rss reconfig failed (RSS disabled)");
		return err;
	}


	return hn_vf_rss_hash_update(dev, rss_conf);
}

static int hn_rss_hash_conf_get(struct rte_eth_dev *dev,
				struct rte_eth_rss_conf *rss_conf)
{
	struct hn_data *hv = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	if (hv->ndis_ver < NDIS_VERSION_6_20) {
		PMD_DRV_LOG(DEBUG, "RSS not supported on this host");
		return -EOPNOTSUPP;
	}

	rss_conf->rss_key_len = NDIS_HASH_KEYSIZE_TOEPLITZ;
	if (rss_conf->rss_key)
		memcpy(rss_conf->rss_key, hv->rss_key,
		       NDIS_HASH_KEYSIZE_TOEPLITZ);

	rss_conf->rss_hf = 0;
	if (hv->rss_hash & NDIS_HASH_IPV4)
		rss_conf->rss_hf |= ETH_RSS_IPV4;

	if (hv->rss_hash & NDIS_HASH_TCP_IPV4)
		rss_conf->rss_hf |= ETH_RSS_NONFRAG_IPV4_TCP;

	if (hv->rss_hash & NDIS_HASH_IPV6)
		rss_conf->rss_hf |= ETH_RSS_IPV6;

	if (hv->rss_hash & NDIS_HASH_IPV6_EX)
		rss_conf->rss_hf |= ETH_RSS_IPV6_EX;

	if (hv->rss_hash & NDIS_HASH_TCP_IPV6)
		rss_conf->rss_hf |= ETH_RSS_NONFRAG_IPV6_TCP;

	if (hv->rss_hash & NDIS_HASH_TCP_IPV6_EX)
		rss_conf->rss_hf |= ETH_RSS_IPV6_TCP_EX;

	return 0;
}

static int
hn_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct hn_data *hv = dev->data->dev_private;

	hn_rndis_set_rxfilter(hv, NDIS_PACKET_TYPE_PROMISCUOUS);
	return hn_vf_promiscuous_enable(dev);
}

static int
hn_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct hn_data *hv = dev->data->dev_private;
	uint32_t filter;

	filter = NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_BROADCAST;
	if (dev->data->all_multicast)
		filter |= NDIS_PACKET_TYPE_ALL_MULTICAST;
	hn_rndis_set_rxfilter(hv, filter);
	return hn_vf_promiscuous_disable(dev);
}

static int
hn_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct hn_data *hv = dev->data->dev_private;

	hn_rndis_set_rxfilter(hv, NDIS_PACKET_TYPE_DIRECTED |
			      NDIS_PACKET_TYPE_ALL_MULTICAST |
			NDIS_PACKET_TYPE_BROADCAST);
	return hn_vf_allmulticast_enable(dev);
}

static int
hn_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct hn_data *hv = dev->data->dev_private;

	hn_rndis_set_rxfilter(hv, NDIS_PACKET_TYPE_DIRECTED |
			     NDIS_PACKET_TYPE_BROADCAST);
	return hn_vf_allmulticast_disable(dev);
}

static int
hn_dev_mc_addr_list(struct rte_eth_dev *dev,
		     struct rte_ether_addr *mc_addr_set,
		     uint32_t nb_mc_addr)
{
	/* No filtering on the synthetic path, but can do it on VF */
	return hn_vf_mc_addr_list(dev, mc_addr_set, nb_mc_addr);
}

/* Setup shared rx/tx queue data */
static int hn_subchan_configure(struct hn_data *hv,
				uint32_t subchan)
{
	struct vmbus_channel *primary = hn_primary_chan(hv);
	int err;
	unsigned int retry = 0;

	PMD_DRV_LOG(DEBUG,
		    "open %u subchannels", subchan);

	/* Send create sub channels command */
	err = hn_nvs_alloc_subchans(hv, &subchan);
	if (err)
		return  err;

	while (subchan > 0) {
		struct vmbus_channel *new_sc;
		uint16_t chn_index;

		err = rte_vmbus_subchan_open(primary, &new_sc);
		if (err == -ENOENT && ++retry < 1000) {
			/* This can happen if not ready yet */
			rte_delay_ms(10);
			continue;
		}

		if (err) {
			PMD_DRV_LOG(ERR,
				    "open subchannel failed: %d", err);
			return err;
		}

		rte_vmbus_set_latency(hv->vmbus, new_sc, hv->latency);

		retry = 0;
		chn_index = rte_vmbus_sub_channel_index(new_sc);
		if (chn_index == 0 || chn_index > hv->max_queues) {
			PMD_DRV_LOG(ERR,
				    "Invalid subchannel offermsg channel %u",
				    chn_index);
			return -EIO;
		}

		PMD_DRV_LOG(DEBUG, "new sub channel %u", chn_index);
		hv->channels[chn_index] = new_sc;
		--subchan;
	}

	return err;
}

static int hn_dev_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_conf *dev_conf = &dev->data->dev_conf;
	struct rte_eth_rss_conf *rss_conf = &dev_conf->rx_adv_conf.rss_conf;
	const struct rte_eth_rxmode *rxmode = &dev_conf->rxmode;
	const struct rte_eth_txmode *txmode = &dev_conf->txmode;
	struct hn_data *hv = dev->data->dev_private;
	uint64_t unsupported;
	int i, err, subchan;

	PMD_INIT_FUNC_TRACE();

	if (dev_conf->rxmode.mq_mode & ETH_MQ_RX_RSS_FLAG)
		dev_conf->rxmode.offloads |= DEV_RX_OFFLOAD_RSS_HASH;

	unsupported = txmode->offloads & ~HN_TX_OFFLOAD_CAPS;
	if (unsupported) {
		PMD_DRV_LOG(NOTICE,
			    "unsupported TX offload: %#" PRIx64,
			    unsupported);
		return -EINVAL;
	}

	unsupported = rxmode->offloads & ~HN_RX_OFFLOAD_CAPS;
	if (unsupported) {
		PMD_DRV_LOG(NOTICE,
			    "unsupported RX offload: %#" PRIx64,
			    rxmode->offloads);
		return -EINVAL;
	}

	hv->vlan_strip = !!(rxmode->offloads & DEV_RX_OFFLOAD_VLAN_STRIP);

	err = hn_rndis_conf_offload(hv, txmode->offloads,
				    rxmode->offloads);
	if (err) {
		PMD_DRV_LOG(NOTICE,
			    "offload configure failed");
		return err;
	}

	hv->num_queues = RTE_MAX(dev->data->nb_rx_queues,
				 dev->data->nb_tx_queues);

	for (i = 0; i < NDIS_HASH_INDCNT; i++)
		hv->rss_ind[i] = i % hv->num_queues;

	hn_rss_hash_init(hv, rss_conf);

	subchan = hv->num_queues - 1;
	if (subchan > 0) {
		err = hn_subchan_configure(hv, subchan);
		if (err) {
			PMD_DRV_LOG(NOTICE,
				    "subchannel configuration failed");
			return err;
		}

		err = hn_rndis_conf_rss(hv, NDIS_RSS_FLAG_DISABLE);
		if (err) {
			PMD_DRV_LOG(NOTICE,
				"rss disable failed");
			return err;
		}

		err = hn_rndis_conf_rss(hv, 0);
		if (err) {
			PMD_DRV_LOG(NOTICE,
				    "initial RSS config failed");
			return err;
		}
	}

	return hn_vf_configure(dev, dev_conf);
}

static int hn_dev_stats_get(struct rte_eth_dev *dev,
			    struct rte_eth_stats *stats)
{
	unsigned int i;

	hn_vf_stats_get(dev, stats);

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		const struct hn_tx_queue *txq = dev->data->tx_queues[i];

		if (!txq)
			continue;

		stats->opackets += txq->stats.packets;
		stats->obytes += txq->stats.bytes;
		stats->oerrors += txq->stats.errors;

		if (i < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			stats->q_opackets[i] = txq->stats.packets;
			stats->q_obytes[i] = txq->stats.bytes;
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		const struct hn_rx_queue *rxq = dev->data->rx_queues[i];

		if (!rxq)
			continue;

		stats->ipackets += rxq->stats.packets;
		stats->ibytes += rxq->stats.bytes;
		stats->ierrors += rxq->stats.errors;
		stats->imissed += rxq->stats.ring_full;

		if (i < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			stats->q_ipackets[i] = rxq->stats.packets;
			stats->q_ibytes[i] = rxq->stats.bytes;
		}
	}

	stats->rx_nombuf = dev->data->rx_mbuf_alloc_failed;
	return 0;
}

static int
hn_dev_stats_reset(struct rte_eth_dev *dev)
{
	unsigned int i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct hn_tx_queue *txq = dev->data->tx_queues[i];

		if (!txq)
			continue;
		memset(&txq->stats, 0, sizeof(struct hn_stats));
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct hn_rx_queue *rxq = dev->data->rx_queues[i];

		if (!rxq)
			continue;

		memset(&rxq->stats, 0, sizeof(struct hn_stats));
	}

	return 0;
}

static int
hn_dev_xstats_reset(struct rte_eth_dev *dev)
{
	int ret;

	ret = hn_dev_stats_reset(dev);
	if (ret != 0)
		return 0;

	return hn_vf_xstats_reset(dev);
}

static int
hn_dev_xstats_count(struct rte_eth_dev *dev)
{
	int ret, count;

	count = dev->data->nb_tx_queues * RTE_DIM(hn_stat_strings);
	count += dev->data->nb_rx_queues * RTE_DIM(hn_stat_strings);

	ret = hn_vf_xstats_get_names(dev, NULL, 0);
	if (ret < 0)
		return ret;

	return count + ret;
}

static int
hn_dev_xstats_get_names(struct rte_eth_dev *dev,
			struct rte_eth_xstat_name *xstats_names,
			unsigned int limit)
{
	unsigned int i, t, count = 0;
	int ret;

	if (!xstats_names)
		return hn_dev_xstats_count(dev);

	/* Note: limit checked in rte_eth_xstats_names() */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		const struct hn_tx_queue *txq = dev->data->tx_queues[i];

		if (!txq)
			continue;

		if (count >= limit)
			break;

		for (t = 0; t < RTE_DIM(hn_stat_strings); t++)
			snprintf(xstats_names[count++].name,
				 RTE_ETH_XSTATS_NAME_SIZE,
				 "tx_q%u_%s", i, hn_stat_strings[t].name);
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++)  {
		const struct hn_rx_queue *rxq = dev->data->rx_queues[i];

		if (!rxq)
			continue;

		if (count >= limit)
			break;

		for (t = 0; t < RTE_DIM(hn_stat_strings); t++)
			snprintf(xstats_names[count++].name,
				 RTE_ETH_XSTATS_NAME_SIZE,
				 "rx_q%u_%s", i,
				 hn_stat_strings[t].name);
	}

	ret = hn_vf_xstats_get_names(dev, xstats_names + count,
				     limit - count);
	if (ret < 0)
		return ret;

	return count + ret;
}

static int
hn_dev_xstats_get(struct rte_eth_dev *dev,
		  struct rte_eth_xstat *xstats,
		  unsigned int n)
{
	unsigned int i, t, count = 0;
	const unsigned int nstats = hn_dev_xstats_count(dev);
	const char *stats;
	int ret;

	PMD_INIT_FUNC_TRACE();

	if (n < nstats)
		return nstats;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		const struct hn_tx_queue *txq = dev->data->tx_queues[i];

		if (!txq)
			continue;

		stats = (const char *)&txq->stats;
		for (t = 0; t < RTE_DIM(hn_stat_strings); t++, count++) {
			xstats[count].id = count;
			xstats[count].value = *(const uint64_t *)
				(stats + hn_stat_strings[t].offset);
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		const struct hn_rx_queue *rxq = dev->data->rx_queues[i];

		if (!rxq)
			continue;

		stats = (const char *)&rxq->stats;
		for (t = 0; t < RTE_DIM(hn_stat_strings); t++, count++) {
			xstats[count].id = count;
			xstats[count].value = *(const uint64_t *)
				(stats + hn_stat_strings[t].offset);
		}
	}

	ret = hn_vf_xstats_get(dev, xstats, count, n);
	if (ret < 0)
		return ret;

	return count + ret;
}

static int
hn_dev_start(struct rte_eth_dev *dev)
{
	struct hn_data *hv = dev->data->dev_private;
	int error;

	PMD_INIT_FUNC_TRACE();

	error = hn_rndis_set_rxfilter(hv,
				      NDIS_PACKET_TYPE_BROADCAST |
				      NDIS_PACKET_TYPE_ALL_MULTICAST |
				      NDIS_PACKET_TYPE_DIRECTED);
	if (error)
		return error;

	error = hn_vf_start(dev);
	if (error)
		hn_rndis_set_rxfilter(hv, 0);

	/* Initialize Link state */
	if (error == 0)
		hn_dev_link_update(dev, 0);

	return error;
}

static void
hn_dev_stop(struct rte_eth_dev *dev)
{
	struct hn_data *hv = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	hn_rndis_set_rxfilter(hv, 0);
	hn_vf_stop(dev);
}

static void
hn_dev_close(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();

	hn_vf_close(dev);
	hn_dev_free_queues(dev);
}

static const struct eth_dev_ops hn_eth_dev_ops = {
	.dev_configure		= hn_dev_configure,
	.dev_start		= hn_dev_start,
	.dev_stop		= hn_dev_stop,
	.dev_close		= hn_dev_close,
	.dev_infos_get		= hn_dev_info_get,
	.dev_supported_ptypes_get = hn_vf_supported_ptypes,
	.promiscuous_enable     = hn_dev_promiscuous_enable,
	.promiscuous_disable    = hn_dev_promiscuous_disable,
	.allmulticast_enable    = hn_dev_allmulticast_enable,
	.allmulticast_disable   = hn_dev_allmulticast_disable,
	.set_mc_addr_list	= hn_dev_mc_addr_list,
	.reta_update		= hn_rss_reta_update,
	.reta_query             = hn_rss_reta_query,
	.rss_hash_update	= hn_rss_hash_update,
	.rss_hash_conf_get      = hn_rss_hash_conf_get,
	.tx_queue_setup		= hn_dev_tx_queue_setup,
	.tx_queue_release	= hn_dev_tx_queue_release,
	.tx_done_cleanup        = hn_dev_tx_done_cleanup,
	.rx_queue_setup		= hn_dev_rx_queue_setup,
	.rx_queue_release	= hn_dev_rx_queue_release,
	.link_update		= hn_dev_link_update,
	.stats_get		= hn_dev_stats_get,
	.stats_reset            = hn_dev_stats_reset,
	.xstats_get		= hn_dev_xstats_get,
	.xstats_get_names	= hn_dev_xstats_get_names,
	.xstats_reset		= hn_dev_xstats_reset,
};

/*
 * Setup connection between PMD and kernel.
 */
static int
hn_attach(struct hn_data *hv, unsigned int mtu)
{
	int error;

	/* Attach NVS */
	error = hn_nvs_attach(hv, mtu);
	if (error)
		goto failed_nvs;

	/* Attach RNDIS */
	error = hn_rndis_attach(hv);
	if (error)
		goto failed_rndis;

	/*
	 * NOTE:
	 * Under certain conditions on certain versions of Hyper-V,
	 * the RNDIS rxfilter is _not_ zero on the hypervisor side
	 * after the successful RNDIS initialization.
	 */
	hn_rndis_set_rxfilter(hv, NDIS_PACKET_TYPE_NONE);
	return 0;
failed_rndis:
	hn_nvs_detach(hv);
failed_nvs:
	return error;
}

static void
hn_detach(struct hn_data *hv)
{
	hn_nvs_detach(hv);
	hn_rndis_detach(hv);
}

static int
eth_hn_dev_init(struct rte_eth_dev *eth_dev)
{
	struct hn_data *hv = eth_dev->data->dev_private;
	struct rte_device *device = eth_dev->device;
	struct rte_vmbus_device *vmbus;
	unsigned int rxr_cnt;
	int err, max_chan;

	PMD_INIT_FUNC_TRACE();

	vmbus = container_of(device, struct rte_vmbus_device, device);
	eth_dev->dev_ops = &hn_eth_dev_ops;
	eth_dev->tx_pkt_burst = &hn_xmit_pkts;
	eth_dev->rx_pkt_burst = &hn_recv_pkts;

	/*
	 * for secondary processes, we don't initialize any further as primary
	 * has already done this work.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* Since Hyper-V only supports one MAC address */
	eth_dev->data->mac_addrs = rte_calloc("hv_mac", HN_MAX_MAC_ADDRS,
					      sizeof(struct rte_ether_addr), 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate memory store MAC addresses");
		return -ENOMEM;
	}

	hv->vmbus = vmbus;
	hv->rxbuf_res = &vmbus->resource[HV_RECV_BUF_MAP];
	hv->chim_res  = &vmbus->resource[HV_SEND_BUF_MAP];
	hv->port_id = eth_dev->data->port_id;
	hv->latency = HN_CHAN_LATENCY_NS;
	hv->max_queues = 1;
	rte_spinlock_init(&hv->vf_lock);
	hv->vf_port = HN_INVALID_PORT;

	err = hn_parse_args(eth_dev);
	if (err)
		return err;

	strlcpy(hv->owner.name, eth_dev->device->name,
		RTE_ETH_MAX_OWNER_NAME_LEN);
	err = rte_eth_dev_owner_new(&hv->owner.id);
	if (err) {
		PMD_INIT_LOG(ERR, "Can not get owner id");
		return err;
	}

	/* Initialize primary channel input for control operations */
	err = rte_vmbus_chan_open(vmbus, &hv->channels[0]);
	if (err)
		return err;

	rte_vmbus_set_latency(hv->vmbus, hv->channels[0], hv->latency);

	hv->primary = hn_rx_queue_alloc(hv, 0,
					eth_dev->device->numa_node);

	if (!hv->primary)
		return -ENOMEM;

	err = hn_attach(hv, RTE_ETHER_MTU);
	if  (err)
		goto failed;

	err = hn_chim_init(eth_dev);
	if (err)
		goto failed;

	err = hn_rndis_get_eaddr(hv, eth_dev->data->mac_addrs->addr_bytes);
	if (err)
		goto failed;

	/* Multi queue requires later versions of windows server */
	if (hv->nvs_ver < NVS_VERSION_5)
		return 0;

	max_chan = rte_vmbus_max_channels(vmbus);
	PMD_INIT_LOG(DEBUG, "VMBus max channels %d", max_chan);
	if (max_chan <= 0)
		goto failed;

	if (hn_rndis_query_rsscaps(hv, &rxr_cnt) != 0)
		rxr_cnt = 1;

	hv->max_queues = RTE_MIN(rxr_cnt, (unsigned int)max_chan);

	/* If VF was reported but not added, do it now */
	if (hv->vf_present && !hn_vf_attached(hv)) {
		PMD_INIT_LOG(DEBUG, "Adding VF device");

		err = hn_vf_add(eth_dev, hv);
		if (err)
			hv->vf_present = 0;
	}

	return 0;

failed:
	PMD_INIT_LOG(NOTICE, "device init failed");

	hn_chim_uninit(eth_dev);
	hn_detach(hv);
	return err;
}

static int
eth_hn_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct hn_data *hv = eth_dev->data->dev_private;
	int ret;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	hn_dev_stop(eth_dev);
	hn_dev_close(eth_dev);

	eth_dev->dev_ops = NULL;
	eth_dev->tx_pkt_burst = NULL;
	eth_dev->rx_pkt_burst = NULL;

	hn_detach(hv);
	hn_chim_uninit(eth_dev);
	rte_vmbus_chan_close(hv->primary->chan);
	rte_free(hv->primary);
	ret = rte_eth_dev_owner_delete(hv->owner.id);
	if (ret != 0)
		return ret;

	return 0;
}

static int eth_hn_probe(struct rte_vmbus_driver *drv __rte_unused,
			struct rte_vmbus_device *dev)
{
	struct rte_eth_dev *eth_dev;
	int ret;

	PMD_INIT_FUNC_TRACE();

	eth_dev = eth_dev_vmbus_allocate(dev, sizeof(struct hn_data));
	if (!eth_dev)
		return -ENOMEM;

	ret = eth_hn_dev_init(eth_dev);
	if (ret)
		eth_dev_vmbus_release(eth_dev);
	else
		rte_eth_dev_probing_finish(eth_dev);

	return ret;
}

static int eth_hn_remove(struct rte_vmbus_device *dev)
{
	struct rte_eth_dev *eth_dev;
	int ret;

	PMD_INIT_FUNC_TRACE();

	eth_dev = rte_eth_dev_allocated(dev->device.name);
	if (!eth_dev)
		return -ENODEV;

	ret = eth_hn_dev_uninit(eth_dev);
	if (ret)
		return ret;

	eth_dev_vmbus_release(eth_dev);
	return 0;
}

/* Network device GUID */
static const rte_uuid_t hn_net_ids[] = {
	/*  f8615163-df3e-46c5-913f-f2d2f965ed0e */
	RTE_UUID_INIT(0xf8615163, 0xdf3e, 0x46c5, 0x913f, 0xf2d2f965ed0eULL),
	{ 0 }
};

static struct rte_vmbus_driver rte_netvsc_pmd = {
	.id_table = hn_net_ids,
	.probe = eth_hn_probe,
	.remove = eth_hn_remove,
};

RTE_PMD_REGISTER_VMBUS(net_netvsc, rte_netvsc_pmd);
RTE_PMD_REGISTER_KMOD_DEP(net_netvsc, "* uio_hv_generic");

RTE_INIT(hn_init_log)
{
	hn_logtype_init = rte_log_register("pmd.net.netvsc.init");
	if (hn_logtype_init >= 0)
		rte_log_set_level(hn_logtype_init, RTE_LOG_NOTICE);
	hn_logtype_driver = rte_log_register("pmd.net.netvsc.driver");
	if (hn_logtype_driver >= 0)
		rte_log_set_level(hn_logtype_driver, RTE_LOG_NOTICE);
}
