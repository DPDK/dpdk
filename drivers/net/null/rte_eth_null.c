/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) IGEL Co.,Ltd.
 *  All rights reserved.
 */

#include <rte_mbuf.h>
#include <ethdev_driver.h>
#include <ethdev_vdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_bus_vdev.h>
#include <rte_kvargs.h>
#include <rte_spinlock.h>

#define ETH_NULL_PACKET_SIZE_ARG	"size"
#define ETH_NULL_PACKET_COPY_ARG	"copy"
#define ETH_NULL_PACKET_NO_RX_ARG	"no-rx"

static unsigned int default_packet_size = 64;
static unsigned int default_packet_copy;
static unsigned int default_no_rx;

static const char *valid_arguments[] = {
	ETH_NULL_PACKET_SIZE_ARG,
	ETH_NULL_PACKET_COPY_ARG,
	ETH_NULL_PACKET_NO_RX_ARG,
	NULL
};

struct pmd_internals;

struct null_queue {
	struct pmd_internals *internals;

	struct rte_mempool *mb_pool;
	struct rte_mbuf *dummy_packet;

	rte_atomic64_t rx_pkts;
	rte_atomic64_t tx_pkts;
};

struct pmd_options {
	unsigned int packet_copy;
	unsigned int packet_size;
	unsigned int no_rx;
};

struct pmd_internals {
	unsigned int packet_size;
	unsigned int packet_copy;
	unsigned int no_rx;
	uint16_t port_id;

	struct null_queue rx_null_queues[RTE_MAX_QUEUES_PER_PORT];
	struct null_queue tx_null_queues[RTE_MAX_QUEUES_PER_PORT];

	struct rte_ether_addr eth_addr;
	/** Bit mask of RSS offloads, the bit offset also means flow type */
	uint64_t flow_type_rss_offloads;

	rte_spinlock_t rss_lock;

	uint16_t reta_size;
	struct rte_eth_rss_reta_entry64 reta_conf[RTE_ETH_RSS_RETA_SIZE_128 /
			RTE_ETH_RETA_GROUP_SIZE];

	uint8_t rss_key[40];                /**< 40-byte hash key. */
};
static struct rte_eth_link pmd_link = {
	.link_speed = RTE_ETH_SPEED_NUM_10G,
	.link_duplex = RTE_ETH_LINK_FULL_DUPLEX,
	.link_status = RTE_ETH_LINK_DOWN,
	.link_autoneg = RTE_ETH_LINK_FIXED,
};

RTE_LOG_REGISTER_DEFAULT(eth_null_logtype, NOTICE);

#define PMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, eth_null_logtype, \
		"%s(): " fmt "\n", __func__, ##args)

static uint16_t
eth_null_rx(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs)
{
	int i;
	struct null_queue *h = q;
	unsigned int packet_size;

	if ((q == NULL) || (bufs == NULL))
		return 0;

	packet_size = h->internals->packet_size;
	if (rte_pktmbuf_alloc_bulk(h->mb_pool, bufs, nb_bufs) != 0)
		return 0;

	for (i = 0; i < nb_bufs; i++) {
		bufs[i]->data_len = (uint16_t)packet_size;
		bufs[i]->pkt_len = packet_size;
		bufs[i]->port = h->internals->port_id;
	}

	rte_atomic64_add(&(h->rx_pkts), i);

	return i;
}

static uint16_t
eth_null_copy_rx(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs)
{
	int i;
	struct null_queue *h = q;
	unsigned int packet_size;

	if ((q == NULL) || (bufs == NULL))
		return 0;

	packet_size = h->internals->packet_size;
	if (rte_pktmbuf_alloc_bulk(h->mb_pool, bufs, nb_bufs) != 0)
		return 0;

	for (i = 0; i < nb_bufs; i++) {
		rte_memcpy(rte_pktmbuf_mtod(bufs[i], void *), h->dummy_packet,
					packet_size);
		bufs[i]->data_len = (uint16_t)packet_size;
		bufs[i]->pkt_len = packet_size;
		bufs[i]->port = h->internals->port_id;
	}

	rte_atomic64_add(&(h->rx_pkts), i);

	return i;
}

static uint16_t
eth_null_no_rx(void *q __rte_unused, struct rte_mbuf **bufs __rte_unused,
		uint16_t nb_bufs __rte_unused)
{
	return 0;
}

static uint16_t
eth_null_tx(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs)
{
	int i;
	struct null_queue *h = q;

	if ((q == NULL) || (bufs == NULL))
		return 0;

	for (i = 0; i < nb_bufs; i++)
		rte_pktmbuf_free(bufs[i]);

	rte_atomic64_add(&(h->tx_pkts), i);

	return i;
}

static uint16_t
eth_null_copy_tx(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs)
{
	int i;
	struct null_queue *h = q;
	unsigned int packet_size;

	if ((q == NULL) || (bufs == NULL))
		return 0;

	packet_size = h->internals->packet_size;
	for (i = 0; i < nb_bufs; i++) {
		rte_memcpy(h->dummy_packet, rte_pktmbuf_mtod(bufs[i], void *),
					packet_size);
		rte_pktmbuf_free(bufs[i]);
	}

	rte_atomic64_add(&(h->tx_pkts), i);

	return i;
}

static int
eth_dev_configure(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

static int
eth_dev_start(struct rte_eth_dev *dev)
{
	if (dev == NULL)
		return -EINVAL;

	dev->data->dev_link.link_status = RTE_ETH_LINK_UP;
	return 0;
}

static int
eth_dev_stop(struct rte_eth_dev *dev)
{
	if (dev == NULL)
		return 0;

	dev->data->dev_link.link_status = RTE_ETH_LINK_DOWN;

	return 0;
}

static int
eth_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		uint16_t nb_rx_desc __rte_unused,
		unsigned int socket_id __rte_unused,
		const struct rte_eth_rxconf *rx_conf __rte_unused,
		struct rte_mempool *mb_pool)
{
	struct rte_mbuf *dummy_packet;
	struct pmd_internals *internals;
	unsigned int packet_size;

	if ((dev == NULL) || (mb_pool == NULL))
		return -EINVAL;

	internals = dev->data->dev_private;

	if (rx_queue_id >= dev->data->nb_rx_queues)
		return -ENODEV;

	packet_size = internals->packet_size;

	internals->rx_null_queues[rx_queue_id].mb_pool = mb_pool;
	dev->data->rx_queues[rx_queue_id] =
		&internals->rx_null_queues[rx_queue_id];
	dummy_packet = rte_zmalloc_socket(NULL,
			packet_size, 0, dev->data->numa_node);
	if (dummy_packet == NULL)
		return -ENOMEM;

	internals->rx_null_queues[rx_queue_id].internals = internals;
	internals->rx_null_queues[rx_queue_id].dummy_packet = dummy_packet;

	return 0;
}

static int
eth_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		uint16_t nb_tx_desc __rte_unused,
		unsigned int socket_id __rte_unused,
		const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct rte_mbuf *dummy_packet;
	struct pmd_internals *internals;
	unsigned int packet_size;

	if (dev == NULL)
		return -EINVAL;

	internals = dev->data->dev_private;

	if (tx_queue_id >= dev->data->nb_tx_queues)
		return -ENODEV;

	packet_size = internals->packet_size;

	dev->data->tx_queues[tx_queue_id] =
		&internals->tx_null_queues[tx_queue_id];
	dummy_packet = rte_zmalloc_socket(NULL,
			packet_size, 0, dev->data->numa_node);
	if (dummy_packet == NULL)
		return -ENOMEM;

	internals->tx_null_queues[tx_queue_id].internals = internals;
	internals->tx_null_queues[tx_queue_id].dummy_packet = dummy_packet;

	return 0;
}

static int
eth_mtu_set(struct rte_eth_dev *dev __rte_unused, uint16_t mtu __rte_unused)
{
	return 0;
}

static int
eth_dev_info(struct rte_eth_dev *dev,
		struct rte_eth_dev_info *dev_info)
{
	struct pmd_internals *internals;

	if ((dev == NULL) || (dev_info == NULL))
		return -EINVAL;

	internals = dev->data->dev_private;
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = (uint32_t)-1;
	dev_info->max_rx_queues = RTE_DIM(internals->rx_null_queues);
	dev_info->max_tx_queues = RTE_DIM(internals->tx_null_queues);
	dev_info->min_rx_bufsize = 0;
	dev_info->reta_size = internals->reta_size;
	dev_info->flow_type_rss_offloads = internals->flow_type_rss_offloads;

	return 0;
}

static int
eth_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *igb_stats)
{
	unsigned int i, num_stats;
	unsigned long rx_total = 0, tx_total = 0;
	const struct pmd_internals *internal;

	if ((dev == NULL) || (igb_stats == NULL))
		return -EINVAL;

	internal = dev->data->dev_private;
	num_stats = RTE_MIN((unsigned int)RTE_ETHDEV_QUEUE_STAT_CNTRS,
			RTE_MIN(dev->data->nb_rx_queues,
				RTE_DIM(internal->rx_null_queues)));
	for (i = 0; i < num_stats; i++) {
		igb_stats->q_ipackets[i] =
			internal->rx_null_queues[i].rx_pkts.cnt;
		rx_total += igb_stats->q_ipackets[i];
	}

	num_stats = RTE_MIN((unsigned int)RTE_ETHDEV_QUEUE_STAT_CNTRS,
			RTE_MIN(dev->data->nb_tx_queues,
				RTE_DIM(internal->tx_null_queues)));
	for (i = 0; i < num_stats; i++) {
		igb_stats->q_opackets[i] =
			internal->tx_null_queues[i].tx_pkts.cnt;
		tx_total += igb_stats->q_opackets[i];
	}

	igb_stats->ipackets = rx_total;
	igb_stats->opackets = tx_total;

	return 0;
}

static int
eth_stats_reset(struct rte_eth_dev *dev)
{
	unsigned int i;
	struct pmd_internals *internal;

	if (dev == NULL)
		return -EINVAL;

	internal = dev->data->dev_private;
	for (i = 0; i < RTE_DIM(internal->rx_null_queues); i++)
		internal->rx_null_queues[i].rx_pkts.cnt = 0;
	for (i = 0; i < RTE_DIM(internal->tx_null_queues); i++)
		internal->tx_null_queues[i].tx_pkts.cnt = 0;

	return 0;
}

static void
eth_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	struct null_queue *nq = dev->data->rx_queues[qid];

	if (nq == NULL)
		return;

	rte_free(nq->dummy_packet);
}

static void
eth_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	struct null_queue *nq = dev->data->tx_queues[qid];

	if (nq == NULL)
		return;

	rte_free(nq->dummy_packet);
}

static int
eth_link_update(struct rte_eth_dev *dev __rte_unused,
		int wait_to_complete __rte_unused) { return 0; }

static int
eth_rss_reta_update(struct rte_eth_dev *dev,
		struct rte_eth_rss_reta_entry64 *reta_conf, uint16_t reta_size)
{
	int i, j;
	struct pmd_internals *internal = dev->data->dev_private;

	if (reta_size != internal->reta_size)
		return -EINVAL;

	rte_spinlock_lock(&internal->rss_lock);

	/* Copy RETA table */
	for (i = 0; i < (internal->reta_size / RTE_ETH_RETA_GROUP_SIZE); i++) {
		internal->reta_conf[i].mask = reta_conf[i].mask;
		for (j = 0; j < RTE_ETH_RETA_GROUP_SIZE; j++)
			if ((reta_conf[i].mask >> j) & 0x01)
				internal->reta_conf[i].reta[j] = reta_conf[i].reta[j];
	}

	rte_spinlock_unlock(&internal->rss_lock);

	return 0;
}

static int
eth_rss_reta_query(struct rte_eth_dev *dev,
		struct rte_eth_rss_reta_entry64 *reta_conf, uint16_t reta_size)
{
	int i, j;
	struct pmd_internals *internal = dev->data->dev_private;

	if (reta_size != internal->reta_size)
		return -EINVAL;

	rte_spinlock_lock(&internal->rss_lock);

	/* Copy RETA table */
	for (i = 0; i < (internal->reta_size / RTE_ETH_RETA_GROUP_SIZE); i++) {
		for (j = 0; j < RTE_ETH_RETA_GROUP_SIZE; j++)
			if ((reta_conf[i].mask >> j) & 0x01)
				reta_conf[i].reta[j] = internal->reta_conf[i].reta[j];
	}

	rte_spinlock_unlock(&internal->rss_lock);

	return 0;
}

static int
eth_rss_hash_update(struct rte_eth_dev *dev, struct rte_eth_rss_conf *rss_conf)
{
	struct pmd_internals *internal = dev->data->dev_private;

	rte_spinlock_lock(&internal->rss_lock);

	if ((rss_conf->rss_hf & internal->flow_type_rss_offloads) != 0)
		dev->data->dev_conf.rx_adv_conf.rss_conf.rss_hf =
				rss_conf->rss_hf & internal->flow_type_rss_offloads;

	if (rss_conf->rss_key)
		rte_memcpy(internal->rss_key, rss_conf->rss_key, 40);

	rte_spinlock_unlock(&internal->rss_lock);

	return 0;
}

static int
eth_rss_hash_conf_get(struct rte_eth_dev *dev,
		struct rte_eth_rss_conf *rss_conf)
{
	struct pmd_internals *internal = dev->data->dev_private;

	rte_spinlock_lock(&internal->rss_lock);

	rss_conf->rss_hf = dev->data->dev_conf.rx_adv_conf.rss_conf.rss_hf;
	if (rss_conf->rss_key)
		rte_memcpy(rss_conf->rss_key, internal->rss_key, 40);

	rte_spinlock_unlock(&internal->rss_lock);

	return 0;
}

static int
eth_mac_address_set(__rte_unused struct rte_eth_dev *dev,
		    __rte_unused struct rte_ether_addr *addr)
{
	return 0;
}

static int
eth_dev_close(struct rte_eth_dev *dev)
{
	PMD_LOG(INFO, "Closing null ethdev on NUMA socket %u",
			rte_socket_id());

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* mac_addrs must not be freed alone because part of dev_private */
	dev->data->mac_addrs = NULL;

	return 0;
}

static const struct eth_dev_ops ops = {
	.dev_close = eth_dev_close,
	.dev_start = eth_dev_start,
	.dev_stop = eth_dev_stop,
	.dev_configure = eth_dev_configure,
	.dev_infos_get = eth_dev_info,
	.rx_queue_setup = eth_rx_queue_setup,
	.tx_queue_setup = eth_tx_queue_setup,
	.rx_queue_release = eth_rx_queue_release,
	.tx_queue_release = eth_tx_queue_release,
	.mtu_set = eth_mtu_set,
	.link_update = eth_link_update,
	.mac_addr_set = eth_mac_address_set,
	.stats_get = eth_stats_get,
	.stats_reset = eth_stats_reset,
	.reta_update = eth_rss_reta_update,
	.reta_query = eth_rss_reta_query,
	.rss_hash_update = eth_rss_hash_update,
	.rss_hash_conf_get = eth_rss_hash_conf_get
};

static int
eth_dev_null_create(struct rte_vdev_device *dev, struct pmd_options *args)
{
	const unsigned int nb_rx_queues = 1;
	const unsigned int nb_tx_queues = 1;
	struct rte_eth_dev_data *data;
	struct pmd_internals *internals = NULL;
	struct rte_eth_dev *eth_dev = NULL;

	static const uint8_t default_rss_key[40] = {
		0x6D, 0x5A, 0x56, 0xDA, 0x25, 0x5B, 0x0E, 0xC2, 0x41, 0x67, 0x25, 0x3D,
		0x43, 0xA3, 0x8F, 0xB0, 0xD0, 0xCA, 0x2B, 0xCB, 0xAE, 0x7B, 0x30, 0xB4,
		0x77, 0xCB, 0x2D, 0xA3, 0x80, 0x30, 0xF2, 0x0C, 0x6A, 0x42, 0xB7, 0x3B,
		0xBE, 0xAC, 0x01, 0xFA
	};

	if (dev->device.numa_node == SOCKET_ID_ANY)
		dev->device.numa_node = rte_socket_id();

	PMD_LOG(INFO, "Creating null ethdev on numa socket %u",
		dev->device.numa_node);

	eth_dev = rte_eth_vdev_allocate(dev, sizeof(*internals));
	if (!eth_dev)
		return -ENOMEM;

	/* now put it all together
	 * - store queue data in internals,
	 * - store numa_node info in ethdev data
	 * - point eth_dev_data to internals
	 * - and point eth_dev structure to new eth_dev_data structure
	 */
	/* NOTE: we'll replace the data element, of originally allocated eth_dev
	 * so the nulls are local per-process */

	internals = eth_dev->data->dev_private;
	internals->packet_size = args->packet_size;
	internals->packet_copy = args->packet_copy;
	internals->no_rx = args->no_rx;
	internals->port_id = eth_dev->data->port_id;
	rte_eth_random_addr(internals->eth_addr.addr_bytes);

	internals->flow_type_rss_offloads =  RTE_ETH_RSS_PROTO_MASK;
	internals->reta_size = RTE_DIM(internals->reta_conf) * RTE_ETH_RETA_GROUP_SIZE;

	rte_memcpy(internals->rss_key, default_rss_key, 40);

	data = eth_dev->data;
	data->nb_rx_queues = (uint16_t)nb_rx_queues;
	data->nb_tx_queues = (uint16_t)nb_tx_queues;
	data->dev_link = pmd_link;
	data->mac_addrs = &internals->eth_addr;
	data->promiscuous = 1;
	data->all_multicast = 1;
	data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	eth_dev->dev_ops = &ops;

	/* finally assign rx and tx ops */
	if (internals->packet_copy) {
		eth_dev->rx_pkt_burst = eth_null_copy_rx;
		eth_dev->tx_pkt_burst = eth_null_copy_tx;
	} else if (internals->no_rx) {
		eth_dev->rx_pkt_burst = eth_null_no_rx;
		eth_dev->tx_pkt_burst = eth_null_tx;
	} else {
		eth_dev->rx_pkt_burst = eth_null_rx;
		eth_dev->tx_pkt_burst = eth_null_tx;
	}

	rte_eth_dev_probing_finish(eth_dev);
	return 0;
}

static inline int
get_packet_size_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	const char *a = value;
	unsigned int *packet_size = extra_args;

	if ((value == NULL) || (extra_args == NULL))
		return -EINVAL;

	*packet_size = (unsigned int)strtoul(a, NULL, 0);
	if (*packet_size == UINT_MAX)
		return -1;

	return 0;
}

static inline int
get_packet_copy_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	const char *a = value;
	unsigned int *packet_copy = extra_args;

	if ((value == NULL) || (extra_args == NULL))
		return -EINVAL;

	*packet_copy = (unsigned int)strtoul(a, NULL, 0);
	if (*packet_copy == UINT_MAX)
		return -1;

	return 0;
}

static int
get_packet_no_rx_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	const char *a = value;
	unsigned int no_rx;

	if (value == NULL || extra_args == NULL)
		return -EINVAL;

	no_rx = (unsigned int)strtoul(a, NULL, 0);
	if (no_rx != 0 && no_rx != 1)
		return -1;

	*(unsigned int *)extra_args = no_rx;
	return 0;
}

static int
rte_pmd_null_probe(struct rte_vdev_device *dev)
{
	const char *name, *params;
	struct pmd_options args = {
		.packet_copy = default_packet_copy,
		.packet_size = default_packet_size,
		.no_rx = default_no_rx,
	};
	struct rte_kvargs *kvlist = NULL;
	struct rte_eth_dev *eth_dev;
	int ret;

	if (!dev)
		return -EINVAL;

	name = rte_vdev_device_name(dev);
	params = rte_vdev_device_args(dev);
	PMD_LOG(INFO, "Initializing pmd_null for %s", name);

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		struct pmd_internals *internals;
		eth_dev = rte_eth_dev_attach_secondary(name);
		if (!eth_dev) {
			PMD_LOG(ERR, "Failed to probe %s", name);
			return -1;
		}
		/* TODO: request info from primary to set up Rx and Tx */
		eth_dev->dev_ops = &ops;
		eth_dev->device = &dev->device;
		internals = eth_dev->data->dev_private;
		if (internals->packet_copy) {
			eth_dev->rx_pkt_burst = eth_null_copy_rx;
			eth_dev->tx_pkt_burst = eth_null_copy_tx;
		} else if (internals->no_rx) {
			eth_dev->rx_pkt_burst = eth_null_no_rx;
			eth_dev->tx_pkt_burst = eth_null_tx;
		} else {
			eth_dev->rx_pkt_burst = eth_null_rx;
			eth_dev->tx_pkt_burst = eth_null_tx;
		}
		rte_eth_dev_probing_finish(eth_dev);
		return 0;
	}

	if (params != NULL) {
		kvlist = rte_kvargs_parse(params, valid_arguments);
		if (kvlist == NULL)
			return -1;

		ret = rte_kvargs_process(kvlist,
				ETH_NULL_PACKET_SIZE_ARG,
				&get_packet_size_arg, &args.packet_size);
		if (ret < 0)
			goto free_kvlist;


		ret = rte_kvargs_process(kvlist,
				ETH_NULL_PACKET_COPY_ARG,
				&get_packet_copy_arg, &args.packet_copy);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist,
				ETH_NULL_PACKET_NO_RX_ARG,
				&get_packet_no_rx_arg, &args.no_rx);
		if (ret < 0)
			goto free_kvlist;

		if (args.no_rx && args.packet_copy) {
			PMD_LOG(ERR,
				"Both %s and %s arguments at the same time not supported",
				ETH_NULL_PACKET_COPY_ARG,
				ETH_NULL_PACKET_NO_RX_ARG);
			goto free_kvlist;
		}
	}

	PMD_LOG(INFO, "Configure pmd_null: packet size is %d, "
			"packet copy is %s", args.packet_size,
			args.packet_copy ? "enabled" : "disabled");

	ret = eth_dev_null_create(dev, &args);

free_kvlist:
	rte_kvargs_free(kvlist);
	return ret;
}

static int
rte_pmd_null_remove(struct rte_vdev_device *dev)
{
	struct rte_eth_dev *eth_dev = NULL;

	if (!dev)
		return -EINVAL;

	/* find the ethdev entry */
	eth_dev = rte_eth_dev_allocated(rte_vdev_device_name(dev));
	if (eth_dev == NULL)
		return 0; /* port already released */

	eth_dev_close(eth_dev);
	rte_eth_dev_release_port(eth_dev);

	return 0;
}

static struct rte_vdev_driver pmd_null_drv = {
	.probe = rte_pmd_null_probe,
	.remove = rte_pmd_null_remove,
};

RTE_PMD_REGISTER_VDEV(net_null, pmd_null_drv);
RTE_PMD_REGISTER_ALIAS(net_null, eth_null);
RTE_PMD_REGISTER_PARAM_STRING(net_null,
	"size=<int> "
	"copy=<int> "
	ETH_NULL_PACKET_NO_RX_ARG "=0|1");
