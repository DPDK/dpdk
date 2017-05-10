/*-
 *   BSD LICENSE
 *
 *   Copyright (C) IGEL Co.,Ltd.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of IGEL Co.,Ltd. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_ethdev_vdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_vdev.h>
#include <rte_kvargs.h>
#include <rte_spinlock.h>

#define ETH_NULL_PACKET_SIZE_ARG	"size"
#define ETH_NULL_PACKET_COPY_ARG	"copy"

static unsigned default_packet_size = 64;
static unsigned default_packet_copy;

static const char *valid_arguments[] = {
	ETH_NULL_PACKET_SIZE_ARG,
	ETH_NULL_PACKET_COPY_ARG,
	NULL
};

struct pmd_internals;

struct null_queue {
	struct pmd_internals *internals;

	struct rte_mempool *mb_pool;
	struct rte_mbuf *dummy_packet;

	rte_atomic64_t rx_pkts;
	rte_atomic64_t tx_pkts;
	rte_atomic64_t err_pkts;
};

struct pmd_internals {
	unsigned packet_size;
	unsigned packet_copy;
	uint8_t port_id;

	struct null_queue rx_null_queues[RTE_MAX_QUEUES_PER_PORT];
	struct null_queue tx_null_queues[RTE_MAX_QUEUES_PER_PORT];

	/** Bit mask of RSS offloads, the bit offset also means flow type */
	uint64_t flow_type_rss_offloads;

	rte_spinlock_t rss_lock;

	uint16_t reta_size;
	struct rte_eth_rss_reta_entry64 reta_conf[ETH_RSS_RETA_SIZE_128 /
			RTE_RETA_GROUP_SIZE];

	uint8_t rss_key[40];                /**< 40-byte hash key. */
};


static struct ether_addr eth_addr = { .addr_bytes = {0} };
static struct rte_eth_link pmd_link = {
	.link_speed = ETH_SPEED_NUM_10G,
	.link_duplex = ETH_LINK_FULL_DUPLEX,
	.link_status = ETH_LINK_DOWN,
	.link_autoneg = ETH_LINK_SPEED_AUTONEG,
};

static uint16_t
eth_null_rx(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs)
{
	int i;
	struct null_queue *h = q;
	unsigned packet_size;

	if ((q == NULL) || (bufs == NULL))
		return 0;

	packet_size = h->internals->packet_size;
	for (i = 0; i < nb_bufs; i++) {
		bufs[i] = rte_pktmbuf_alloc(h->mb_pool);
		if (!bufs[i])
			break;
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
	unsigned packet_size;

	if ((q == NULL) || (bufs == NULL))
		return 0;

	packet_size = h->internals->packet_size;
	for (i = 0; i < nb_bufs; i++) {
		bufs[i] = rte_pktmbuf_alloc(h->mb_pool);
		if (!bufs[i])
			break;
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
	unsigned packet_size;

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

	dev->data->dev_link.link_status = ETH_LINK_UP;
	return 0;
}

static void
eth_dev_stop(struct rte_eth_dev *dev)
{
	if (dev == NULL)
		return;

	dev->data->dev_link.link_status = ETH_LINK_DOWN;
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
	unsigned packet_size;

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
	unsigned packet_size;

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


static void
eth_dev_info(struct rte_eth_dev *dev,
		struct rte_eth_dev_info *dev_info)
{
	struct pmd_internals *internals;

	if ((dev == NULL) || (dev_info == NULL))
		return;

	internals = dev->data->dev_private;
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = (uint32_t)-1;
	dev_info->max_rx_queues = RTE_DIM(internals->rx_null_queues);
	dev_info->max_tx_queues = RTE_DIM(internals->tx_null_queues);
	dev_info->min_rx_bufsize = 0;
	dev_info->reta_size = internals->reta_size;
	dev_info->flow_type_rss_offloads = internals->flow_type_rss_offloads;
}

static void
eth_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *igb_stats)
{
	unsigned i, num_stats;
	unsigned long rx_total = 0, tx_total = 0, tx_err_total = 0;
	const struct pmd_internals *internal;

	if ((dev == NULL) || (igb_stats == NULL))
		return;

	internal = dev->data->dev_private;
	num_stats = RTE_MIN((unsigned)RTE_ETHDEV_QUEUE_STAT_CNTRS,
			RTE_MIN(dev->data->nb_rx_queues,
				RTE_DIM(internal->rx_null_queues)));
	for (i = 0; i < num_stats; i++) {
		igb_stats->q_ipackets[i] =
			internal->rx_null_queues[i].rx_pkts.cnt;
		rx_total += igb_stats->q_ipackets[i];
	}

	num_stats = RTE_MIN((unsigned)RTE_ETHDEV_QUEUE_STAT_CNTRS,
			RTE_MIN(dev->data->nb_tx_queues,
				RTE_DIM(internal->tx_null_queues)));
	for (i = 0; i < num_stats; i++) {
		igb_stats->q_opackets[i] =
			internal->tx_null_queues[i].tx_pkts.cnt;
		igb_stats->q_errors[i] =
			internal->tx_null_queues[i].err_pkts.cnt;
		tx_total += igb_stats->q_opackets[i];
		tx_err_total += igb_stats->q_errors[i];
	}

	igb_stats->ipackets = rx_total;
	igb_stats->opackets = tx_total;
	igb_stats->oerrors = tx_err_total;
}

static void
eth_stats_reset(struct rte_eth_dev *dev)
{
	unsigned i;
	struct pmd_internals *internal;

	if (dev == NULL)
		return;

	internal = dev->data->dev_private;
	for (i = 0; i < RTE_DIM(internal->rx_null_queues); i++)
		internal->rx_null_queues[i].rx_pkts.cnt = 0;
	for (i = 0; i < RTE_DIM(internal->tx_null_queues); i++) {
		internal->tx_null_queues[i].tx_pkts.cnt = 0;
		internal->tx_null_queues[i].err_pkts.cnt = 0;
	}
}

static void
eth_queue_release(void *q)
{
	struct null_queue *nq;

	if (q == NULL)
		return;

	nq = q;
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
	for (i = 0; i < (internal->reta_size / RTE_RETA_GROUP_SIZE); i++) {
		internal->reta_conf[i].mask = reta_conf[i].mask;
		for (j = 0; j < RTE_RETA_GROUP_SIZE; j++)
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
	for (i = 0; i < (internal->reta_size / RTE_RETA_GROUP_SIZE); i++) {
		for (j = 0; j < RTE_RETA_GROUP_SIZE; j++)
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

static const struct eth_dev_ops ops = {
	.dev_start = eth_dev_start,
	.dev_stop = eth_dev_stop,
	.dev_configure = eth_dev_configure,
	.dev_infos_get = eth_dev_info,
	.rx_queue_setup = eth_rx_queue_setup,
	.tx_queue_setup = eth_tx_queue_setup,
	.rx_queue_release = eth_queue_release,
	.tx_queue_release = eth_queue_release,
	.link_update = eth_link_update,
	.stats_get = eth_stats_get,
	.stats_reset = eth_stats_reset,
	.reta_update = eth_rss_reta_update,
	.reta_query = eth_rss_reta_query,
	.rss_hash_update = eth_rss_hash_update,
	.rss_hash_conf_get = eth_rss_hash_conf_get
};

static struct rte_vdev_driver pmd_null_drv;

static int
eth_dev_null_create(struct rte_vdev_device *dev,
		unsigned packet_size,
		unsigned packet_copy)
{
	const unsigned nb_rx_queues = 1;
	const unsigned nb_tx_queues = 1;
	struct rte_eth_dev_data *data = NULL;
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

	RTE_LOG(INFO, PMD, "Creating null ethdev on numa socket %u\n",
		dev->device.numa_node);

	/* now do all data allocation - for eth_dev structure, dummy pci driver
	 * and internal (private) data
	 */
	data = rte_zmalloc_socket(rte_vdev_device_name(dev), sizeof(*data), 0,
		dev->device.numa_node);
	if (!data)
		return -ENOMEM;

	eth_dev = rte_eth_vdev_allocate(dev, sizeof(*internals));
	if (!eth_dev) {
		rte_free(data);
		return -ENOMEM;
	}

	/* now put it all together
	 * - store queue data in internals,
	 * - store numa_node info in ethdev data
	 * - point eth_dev_data to internals
	 * - and point eth_dev structure to new eth_dev_data structure
	 */
	/* NOTE: we'll replace the data element, of originally allocated eth_dev
	 * so the nulls are local per-process */

	internals = eth_dev->data->dev_private;
	internals->packet_size = packet_size;
	internals->packet_copy = packet_copy;
	internals->port_id = eth_dev->data->port_id;

	internals->flow_type_rss_offloads =  ETH_RSS_PROTO_MASK;
	internals->reta_size = RTE_DIM(internals->reta_conf) * RTE_RETA_GROUP_SIZE;

	rte_memcpy(internals->rss_key, default_rss_key, 40);

	rte_memcpy(data, eth_dev->data, sizeof(*data));
	data->nb_rx_queues = (uint16_t)nb_rx_queues;
	data->nb_tx_queues = (uint16_t)nb_tx_queues;
	data->dev_link = pmd_link;
	data->mac_addrs = &eth_addr;

	eth_dev->data = data;
	eth_dev->dev_ops = &ops;

	data->dev_flags = RTE_ETH_DEV_DETACHABLE;

	/* finally assign rx and tx ops */
	if (packet_copy) {
		eth_dev->rx_pkt_burst = eth_null_copy_rx;
		eth_dev->tx_pkt_burst = eth_null_copy_tx;
	} else {
		eth_dev->rx_pkt_burst = eth_null_rx;
		eth_dev->tx_pkt_burst = eth_null_tx;
	}

	return 0;
}

static inline int
get_packet_size_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	const char *a = value;
	unsigned *packet_size = extra_args;

	if ((value == NULL) || (extra_args == NULL))
		return -EINVAL;

	*packet_size = (unsigned)strtoul(a, NULL, 0);
	if (*packet_size == UINT_MAX)
		return -1;

	return 0;
}

static inline int
get_packet_copy_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	const char *a = value;
	unsigned *packet_copy = extra_args;

	if ((value == NULL) || (extra_args == NULL))
		return -EINVAL;

	*packet_copy = (unsigned)strtoul(a, NULL, 0);
	if (*packet_copy == UINT_MAX)
		return -1;

	return 0;
}

static int
rte_pmd_null_probe(struct rte_vdev_device *dev)
{
	const char *name, *params;
	unsigned packet_size = default_packet_size;
	unsigned packet_copy = default_packet_copy;
	struct rte_kvargs *kvlist = NULL;
	int ret;

	if (!dev)
		return -EINVAL;

	name = rte_vdev_device_name(dev);
	params = rte_vdev_device_args(dev);
	RTE_LOG(INFO, PMD, "Initializing pmd_null for %s\n", name);

	if (params != NULL) {
		kvlist = rte_kvargs_parse(params, valid_arguments);
		if (kvlist == NULL)
			return -1;

		if (rte_kvargs_count(kvlist, ETH_NULL_PACKET_SIZE_ARG) == 1) {

			ret = rte_kvargs_process(kvlist,
					ETH_NULL_PACKET_SIZE_ARG,
					&get_packet_size_arg, &packet_size);
			if (ret < 0)
				goto free_kvlist;
		}

		if (rte_kvargs_count(kvlist, ETH_NULL_PACKET_COPY_ARG) == 1) {

			ret = rte_kvargs_process(kvlist,
					ETH_NULL_PACKET_COPY_ARG,
					&get_packet_copy_arg, &packet_copy);
			if (ret < 0)
				goto free_kvlist;
		}
	}

	RTE_LOG(INFO, PMD, "Configure pmd_null: packet size is %d, "
			"packet copy is %s\n", packet_size,
			packet_copy ? "enabled" : "disabled");

	ret = eth_dev_null_create(dev, packet_size, packet_copy);

free_kvlist:
	if (kvlist)
		rte_kvargs_free(kvlist);
	return ret;
}

static int
rte_pmd_null_remove(struct rte_vdev_device *dev)
{
	struct rte_eth_dev *eth_dev = NULL;

	if (!dev)
		return -EINVAL;

	RTE_LOG(INFO, PMD, "Closing null ethdev on numa socket %u\n",
			rte_socket_id());

	/* find the ethdev entry */
	eth_dev = rte_eth_dev_allocated(rte_vdev_device_name(dev));
	if (eth_dev == NULL)
		return -1;

	rte_free(eth_dev->data->dev_private);
	rte_free(eth_dev->data);

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
	"copy=<int>");
