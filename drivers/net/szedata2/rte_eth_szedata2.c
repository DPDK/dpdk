/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2015 CESNET
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
 *     * Neither the name of CESNET nor the names of its
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

#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <err.h>

#include <libsze2.h>

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_kvargs.h>
#include <rte_dev.h>

#include "rte_eth_szedata2.h"

#define RTE_ETH_SZEDATA2_DEV_PATH_ARG "dev_path"
#define RTE_ETH_SZEDATA2_RX_IFACES_ARG "rx_ifaces"
#define RTE_ETH_SZEDATA2_TX_IFACES_ARG "tx_ifaces"

#define RTE_ETH_SZEDATA2_MAX_RX_QUEUES 32
#define RTE_ETH_SZEDATA2_MAX_TX_QUEUES 32
#define RTE_ETH_SZEDATA2_TX_LOCK_SIZE (32 * 1024 * 1024)

/**
 * size of szedata2_packet header with alignment
 */
#define RTE_SZE2_PACKET_HEADER_SIZE_ALIGNED 8

struct szedata2_rx_queue {
	struct szedata *sze;
	uint8_t rx_channel;
	uint8_t in_port;
	struct rte_mempool *mb_pool;
	volatile uint64_t rx_pkts;
	volatile uint64_t rx_bytes;
	volatile uint64_t err_pkts;
};

struct szedata2_tx_queue {
	struct szedata *sze;
	uint8_t tx_channel;
	volatile uint64_t tx_pkts;
	volatile uint64_t err_pkts;
	volatile uint64_t tx_bytes;
};

struct rxtx_szedata2 {
	uint32_t num_of_rx;
	uint32_t num_of_tx;
	uint32_t sze_rx_mask_req;
	uint32_t sze_tx_mask_req;
	char *sze_dev;
};

struct pmd_internals {
	struct szedata2_rx_queue rx_queue[RTE_ETH_SZEDATA2_MAX_RX_QUEUES];
	struct szedata2_tx_queue tx_queue[RTE_ETH_SZEDATA2_MAX_TX_QUEUES];
	unsigned nb_rx_queues;
	unsigned nb_tx_queues;
	uint32_t num_of_rx;
	uint32_t num_of_tx;
	uint32_t sze_rx_req;
	uint32_t sze_tx_req;
	int if_index;
	char *sze_dev;
};

static const char *valid_arguments[] = {
	RTE_ETH_SZEDATA2_DEV_PATH_ARG,
	RTE_ETH_SZEDATA2_RX_IFACES_ARG,
	RTE_ETH_SZEDATA2_TX_IFACES_ARG,
	NULL
};

static struct ether_addr eth_addr = {
	.addr_bytes = { 0x00, 0x11, 0x17, 0x00, 0x00, 0x00 }
};
static const char *drivername = "SZEdata2 PMD";
static struct rte_eth_link pmd_link = {
		.link_speed = ETH_LINK_SPEED_10G,
		.link_duplex = ETH_LINK_FULL_DUPLEX,
		.link_status = 0
};


static uint32_t
count_ones(uint32_t num)
{
	num = num - ((num >> 1) & 0x55555555); /* reuse input as temporary */
	num = (num & 0x33333333) + ((num >> 2) & 0x33333333);        /* temp */
	return (((num + (num >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24; /* count */
}

static int
init_rx_channels(struct rte_eth_dev *dev, int v)
{
	struct pmd_internals *internals = dev->data->dev_private;
	int ret;
	uint32_t i;
	uint32_t count = internals->num_of_rx;
	uint32_t num_sub = 0;
	uint32_t x;
	uint32_t rx;
	uint32_t tx;

	rx = internals->sze_rx_req;
	tx = 0;

	for (i = 0; i < count; i++) {
		/*
		 * Open, subscribe rx,tx channels and start device
		 */
		if (v)
			RTE_LOG(INFO, PMD, "Opening SZE device %u. time\n", i);

		internals->rx_queue[num_sub].sze =
			szedata_open(internals->sze_dev);
		if (internals->rx_queue[num_sub].sze == NULL)
			return -1;

		/* separate least significant non-zero bit */
		x = rx & ((~rx) + 1);

		if (v)
			RTE_LOG(INFO, PMD, "Subscribing rx channel: 0x%x "
				"tx channel: 0x%x\n", x, tx);

		ret = szedata_subscribe3(internals->rx_queue[num_sub].sze,
				&x, &tx);
		if (ret) {
			szedata_close(internals->rx_queue[num_sub].sze);
			internals->rx_queue[num_sub].sze = NULL;
			return -1;
		}

		if (v)
			RTE_LOG(INFO, PMD, "Subscribed rx channel: 0x%x "
				"tx channel: 0x%x\n", x, tx);

		if (x) {
			if (v)
				RTE_LOG(INFO, PMD, "Starting SZE device for "
					"rx queue: %u\n", num_sub);

			ret = szedata_start(internals->rx_queue[num_sub].sze);
			if (ret) {
				szedata_close(internals->rx_queue[num_sub].sze);
				internals->rx_queue[num_sub].sze = NULL;
				return -1;
			}

			/*
			 * set to 1 all bits lower than bit set to 1
			 * and that bit to 0
			 */
			x -= 1;
			internals->rx_queue[num_sub].rx_channel =
				count_ones(x);

			if (v)
				RTE_LOG(INFO, PMD, "Subscribed rx channel "
					"no: %u\n",
					internals->rx_queue[num_sub].rx_channel
					);

			num_sub++;
			internals->nb_rx_queues = num_sub;
		} else {
			if (v)
				RTE_LOG(INFO, PMD,
					"Could not subscribe any rx channel. "
					"Closing SZE device\n");

			szedata_close(internals->rx_queue[num_sub].sze);
			internals->rx_queue[num_sub].sze = NULL;
		}

		/* set least significant non-zero bit to zero */
		rx = rx & (rx - 1);
	}

	dev->data->nb_rx_queues = (uint16_t)num_sub;

	if (v)
		RTE_LOG(INFO, PMD, "Successfully opened rx channels: %u\n",
			num_sub);

	return 0;
}

static int
init_tx_channels(struct rte_eth_dev *dev, int v)
{
	struct pmd_internals *internals = dev->data->dev_private;
	int ret;
	uint32_t i;
	uint32_t count = internals->num_of_tx;
	uint32_t num_sub = 0;
	uint32_t x;
	uint32_t rx;
	uint32_t tx;

	rx = 0;
	tx = internals->sze_tx_req;

	for (i = 0; i < count; i++) {
		/*
		 * Open, subscribe rx,tx channels and start device
		 */
		if (v)
			RTE_LOG(INFO, PMD, "Opening SZE device %u. time\n",
				i + internals->num_of_rx);

		internals->tx_queue[num_sub].sze =
			szedata_open(internals->sze_dev);
		if (internals->tx_queue[num_sub].sze == NULL)
			return -1;

		/* separate least significant non-zero bit */
		x = tx & ((~tx) + 1);

		if (v)
			RTE_LOG(INFO, PMD, "Subscribing rx channel: 0x%x "
				"tx channel: 0x%x\n", rx, x);

		ret = szedata_subscribe3(internals->tx_queue[num_sub].sze,
				&rx, &x);
		if (ret) {
			szedata_close(internals->tx_queue[num_sub].sze);
			internals->tx_queue[num_sub].sze = NULL;
			return -1;
		}

		if (v)
			RTE_LOG(INFO, PMD, "Subscribed rx channel: 0x%x "
				"tx channel: 0x%x\n", rx, x);

		if (x) {
			if (v)
				RTE_LOG(INFO, PMD, "Starting SZE device for "
					"tx queue: %u\n", num_sub);

			ret = szedata_start(internals->tx_queue[num_sub].sze);
			if (ret) {
				szedata_close(internals->tx_queue[num_sub].sze);
				internals->tx_queue[num_sub].sze = NULL;
				return -1;
			}

			/*
			 * set to 1 all bits lower than bit set to 1
			 * and that bit to 0
			 */
			x -= 1;
			internals->tx_queue[num_sub].tx_channel =
				count_ones(x);

			if (v)
				RTE_LOG(INFO, PMD, "Subscribed tx channel "
					"no: %u\n",
					internals->tx_queue[num_sub].tx_channel
					);

			num_sub++;
			internals->nb_tx_queues = num_sub;
		} else {
			if (v)
				RTE_LOG(INFO, PMD,
					"Could not subscribe any tx channel. "
					"Closing SZE device\n");

			szedata_close(internals->tx_queue[num_sub].sze);
			internals->tx_queue[num_sub].sze = NULL;
		}

		/* set least significant non-zero bit to zero */
		tx = tx & (tx - 1);
	}

	dev->data->nb_tx_queues = (uint16_t)num_sub;

	if (v)
		RTE_LOG(INFO, PMD, "Successfully opened tx channels: %u\n",
			num_sub);

	return 0;
}

static void
close_rx_channels(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = dev->data->dev_private;
	uint32_t i;
	uint32_t num_sub = internals->nb_rx_queues;

	for (i = 0; i < num_sub; i++) {
		if (internals->rx_queue[i].sze != NULL) {
			szedata_close(internals->rx_queue[i].sze);
			internals->rx_queue[i].sze = NULL;
		}
	}
	/* set number of rx queues to zero */
	internals->nb_rx_queues = 0;
	dev->data->nb_rx_queues = (uint16_t)0;
}

static void
close_tx_channels(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = dev->data->dev_private;
	uint32_t i;
	uint32_t num_sub = internals->nb_tx_queues;

	for (i = 0; i < num_sub; i++) {
		if (internals->tx_queue[i].sze != NULL) {
			szedata_close(internals->tx_queue[i].sze);
			internals->tx_queue[i].sze = NULL;
		}
	}
	/* set number of rx queues to zero */
	internals->nb_tx_queues = 0;
	dev->data->nb_tx_queues = (uint16_t)0;
}

static int
eth_dev_start(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = dev->data->dev_private;
	int ret;

	if (internals->nb_rx_queues == 0) {
		ret = init_rx_channels(dev, 0);
		if (ret != 0) {
			close_rx_channels(dev);
			return -1;
		}
	}

	if (internals->nb_tx_queues == 0) {
		ret = init_tx_channels(dev, 0);
		if (ret != 0) {
			close_tx_channels(dev);
			close_rx_channels(dev);
			return -1;
		}
	}

	dev->data->dev_link.link_status = 1;
	return 0;
}

static void
eth_dev_stop(struct rte_eth_dev *dev)
{
	unsigned i;
	struct pmd_internals *internals = dev->data->dev_private;

	for (i = 0; i < internals->nb_rx_queues; i++) {
		if (internals->rx_queue[i].sze != NULL) {
			szedata_close(internals->rx_queue[i].sze);
			internals->rx_queue[i].sze = NULL;
		}
	}

	for (i = 0; i < internals->nb_tx_queues; i++) {
		if (internals->tx_queue[i].sze != NULL) {
			szedata_close(internals->tx_queue[i].sze);
			internals->tx_queue[i].sze = NULL;
		}
	}

	internals->nb_rx_queues = 0;
	internals->nb_tx_queues = 0;

	dev->data->nb_rx_queues = (uint16_t)0;
	dev->data->nb_tx_queues = (uint16_t)0;

	dev->data->dev_link.link_status = 0;
}

static int
eth_dev_configure(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

static void
eth_dev_info(struct rte_eth_dev *dev,
		struct rte_eth_dev_info *dev_info)
{
	struct pmd_internals *internals = dev->data->dev_private;
	dev_info->driver_name = drivername;
	dev_info->if_index = internals->if_index;
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = (uint32_t)-1;
	dev_info->max_rx_queues = (uint16_t)internals->nb_rx_queues;
	dev_info->max_tx_queues = (uint16_t)internals->nb_tx_queues;
	dev_info->min_rx_bufsize = 0;
	dev_info->pci_dev = NULL;
}

static void
eth_stats_get(struct rte_eth_dev *dev,
		struct rte_eth_stats *stats)
{
	unsigned i;
	uint64_t rx_total = 0;
	uint64_t tx_total = 0;
	uint64_t tx_err_total = 0;
	uint64_t rx_total_bytes = 0;
	uint64_t tx_total_bytes = 0;
	const struct pmd_internals *internal = dev->data->dev_private;

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS &&
			i < internal->nb_rx_queues; i++) {
		stats->q_ipackets[i] = internal->rx_queue[i].rx_pkts;
		stats->q_ibytes[i] = internal->rx_queue[i].rx_bytes;
		rx_total += stats->q_ipackets[i];
		rx_total_bytes += stats->q_ibytes[i];
	}

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS &&
			i < internal->nb_tx_queues; i++) {
		stats->q_opackets[i] = internal->tx_queue[i].tx_pkts;
		stats->q_errors[i] = internal->tx_queue[i].err_pkts;
		stats->q_obytes[i] = internal->tx_queue[i].tx_bytes;
		tx_total += stats->q_opackets[i];
		tx_err_total += stats->q_errors[i];
		tx_total_bytes += stats->q_obytes[i];
	}

	stats->ipackets = rx_total;
	stats->opackets = tx_total;
	stats->ibytes = rx_total_bytes;
	stats->obytes = tx_total_bytes;
	stats->oerrors = tx_err_total;
}

static void
eth_stats_reset(struct rte_eth_dev *dev)
{
	unsigned i;
	struct pmd_internals *internal = dev->data->dev_private;
	for (i = 0; i < internal->nb_rx_queues; i++) {
		internal->rx_queue[i].rx_pkts = 0;
		internal->rx_queue[i].rx_bytes = 0;
	}
	for (i = 0; i < internal->nb_tx_queues; i++) {
		internal->tx_queue[i].tx_pkts = 0;
		internal->tx_queue[i].err_pkts = 0;
		internal->tx_queue[i].tx_bytes = 0;
	}
}

static void
eth_dev_close(struct rte_eth_dev *dev)
{
	unsigned i;
	struct pmd_internals *internals = dev->data->dev_private;

	for (i = 0; i < internals->nb_rx_queues; i++) {
		if (internals->rx_queue[i].sze != NULL) {
			szedata_close(internals->rx_queue[i].sze);
			internals->rx_queue[i].sze = NULL;
		}
	}

	for (i = 0; i < internals->nb_tx_queues; i++) {
		if (internals->tx_queue[i].sze != NULL) {
			szedata_close(internals->tx_queue[i].sze);
			internals->tx_queue[i].sze = NULL;
		}
	}

	internals->nb_rx_queues = 0;
	internals->nb_tx_queues = 0;

	dev->data->nb_rx_queues = (uint16_t)0;
	dev->data->nb_tx_queues = (uint16_t)0;
}

static void
eth_queue_release(void *q __rte_unused)
{
}

static int
eth_link_update(struct rte_eth_dev *dev __rte_unused,
		int wait_to_complete __rte_unused)
{
	return 0;
}

static int
eth_rx_queue_setup(struct rte_eth_dev *dev,
		uint16_t rx_queue_id,
		uint16_t nb_rx_desc __rte_unused,
		unsigned int socket_id __rte_unused,
		const struct rte_eth_rxconf *rx_conf __rte_unused,
		struct rte_mempool *mb_pool)
{
	struct pmd_internals *internals = dev->data->dev_private;
	struct szedata2_rx_queue *szedata2_q =
		&internals->rx_queue[rx_queue_id];
	szedata2_q->mb_pool = mb_pool;
	dev->data->rx_queues[rx_queue_id] = szedata2_q;
	szedata2_q->in_port = dev->data->port_id;
	return 0;
}

static int
eth_tx_queue_setup(struct rte_eth_dev *dev,
		uint16_t tx_queue_id,
		uint16_t nb_tx_desc __rte_unused,
		unsigned int socket_id __rte_unused,
		const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct pmd_internals *internals = dev->data->dev_private;
	dev->data->tx_queues[tx_queue_id] = &internals->tx_queue[tx_queue_id];
	return 0;
}

static void
eth_mac_addr_set(struct rte_eth_dev *dev __rte_unused,
		struct ether_addr *mac_addr __rte_unused)
{
}

static struct eth_dev_ops ops = {
		.dev_start          = eth_dev_start,
		.dev_stop           = eth_dev_stop,
		.dev_close          = eth_dev_close,
		.dev_configure      = eth_dev_configure,
		.dev_infos_get      = eth_dev_info,
		.rx_queue_setup     = eth_rx_queue_setup,
		.tx_queue_setup     = eth_tx_queue_setup,
		.rx_queue_release   = eth_queue_release,
		.tx_queue_release   = eth_queue_release,
		.link_update        = eth_link_update,
		.stats_get          = eth_stats_get,
		.stats_reset        = eth_stats_reset,
		.mac_addr_set       = eth_mac_addr_set,
};

static int
parse_mask(const char *mask_str, uint32_t *mask_num)
{
	char *endptr;
	long int value;

	value = strtol(mask_str, &endptr, 0);
	if (*endptr != '\0' || value > UINT32_MAX || value < 0)
		return -1;

	*mask_num = (uint32_t)value;
	return 0;
}

static int
add_rx_mask(const char *key __rte_unused, const char *value, void *extra_args)
{
	struct rxtx_szedata2 *szedata2 = extra_args;
	uint32_t mask;

	if (parse_mask(value, &mask) != 0)
		return -1;

	szedata2->sze_rx_mask_req |= mask;
	return 0;
}

static int
add_tx_mask(const char *key __rte_unused, const char *value, void *extra_args)
{
	struct rxtx_szedata2 *szedata2 = extra_args;
	uint32_t mask;

	if (parse_mask(value, &mask) != 0)
		return -1;

	szedata2->sze_tx_mask_req |= mask;
	return 0;
}

static int
rte_pmd_init_internals(const char *name, const unsigned nb_rx_queues,
		const unsigned nb_tx_queues,
		const unsigned numa_node,
		struct pmd_internals **internals,
		struct rte_eth_dev **eth_dev)
{
	struct rte_eth_dev_data *data = NULL;

	RTE_LOG(INFO, PMD,
			"Creating szedata2-backed ethdev on numa socket %u\n",
			numa_node);

	/*
	 * now do all data allocation - for eth_dev structure
	 * and internal (private) data
	 */
	data = rte_zmalloc_socket(name, sizeof(*data), 0, numa_node);
	if (data == NULL)
		goto error;

	*internals = rte_zmalloc_socket(name, sizeof(**internals), 0,
			numa_node);
	if (*internals == NULL)
		goto error;

	/* reserve an ethdev entry */
	*eth_dev = rte_eth_dev_allocate(name, RTE_ETH_DEV_VIRTUAL);
	if (*eth_dev == NULL)
		goto error;

	/*
	 * now put it all together
	 * - store queue data in internals,
	 * - store numa_node info in pci_driver
	 * - point eth_dev_data to internals
	 * - and point eth_dev structure to new eth_dev_data structure
	 *
	 * NOTE: we'll replace the data element, of originally allocated eth_dev
	 * so the rings are local per-process
	 */

	(*internals)->nb_rx_queues = nb_rx_queues;
	(*internals)->nb_tx_queues = nb_tx_queues;

	(*internals)->if_index = 0;

	data->dev_private = *internals;
	data->port_id = (*eth_dev)->data->port_id;
	snprintf(data->name, sizeof(data->name), "%s", (*eth_dev)->data->name);
	data->nb_rx_queues = (uint16_t)nb_rx_queues;
	data->nb_tx_queues = (uint16_t)nb_tx_queues;
	data->dev_link = pmd_link;
	data->mac_addrs = &eth_addr;

	(*eth_dev)->data = data;
	(*eth_dev)->dev_ops = &ops;
	(*eth_dev)->data->dev_flags = RTE_ETH_DEV_DETACHABLE;
	(*eth_dev)->driver = NULL;
	(*eth_dev)->data->kdrv = RTE_KDRV_NONE;
	(*eth_dev)->data->drv_name = drivername;
	(*eth_dev)->data->numa_node = numa_node;

	return 0;

error:
	rte_free(data);
	rte_free(*internals);
	return -1;
}

static int
rte_eth_from_szedata2(const char *name,
		struct rxtx_szedata2 *szedata2,
		const unsigned numa_node)
{
	struct pmd_internals *internals = NULL;
	struct rte_eth_dev *eth_dev = NULL;
	int ret;

	if (rte_pmd_init_internals(name, 0, 0, numa_node,
			&internals, &eth_dev) < 0)
		return -1;

	internals->sze_dev = szedata2->sze_dev;
	internals->sze_rx_req = szedata2->sze_rx_mask_req;
	internals->sze_tx_req = szedata2->sze_tx_mask_req;
	internals->num_of_rx = szedata2->num_of_rx;
	internals->num_of_tx = szedata2->num_of_tx;

	RTE_LOG(INFO, PMD, "Number of rx channels to open: %u mask: 0x%x\n",
			internals->num_of_rx, internals->sze_rx_req);
	RTE_LOG(INFO, PMD, "Number of tx channels to open: %u mask: 0x%x\n",
			internals->num_of_tx, internals->sze_tx_req);

	ret = init_rx_channels(eth_dev, 1);
	if (ret != 0) {
		close_rx_channels(eth_dev);
		return -1;
	}

	ret = init_tx_channels(eth_dev, 1);
	if (ret != 0) {
		close_tx_channels(eth_dev);
		close_rx_channels(eth_dev);
		return -1;
	}

	eth_dev->rx_pkt_burst = NULL;
	eth_dev->tx_pkt_burst = NULL;

	return 0;
}


static int
rte_pmd_szedata2_devinit(const char *name, const char *params)
{
	unsigned numa_node;
	int ret;
	struct rte_kvargs *kvlist;
	unsigned k_idx;
	struct rte_kvargs_pair *pair = NULL;
	struct rxtx_szedata2 szedata2 = { 0, 0, 0, 0, NULL };
	bool dev_path_missing = true;

	RTE_LOG(INFO, PMD, "Initializing pmd_szedata2 for %s\n", name);

	numa_node = rte_socket_id();

	kvlist = rte_kvargs_parse(params, valid_arguments);
	if (kvlist == NULL)
		return -1;

	/*
	 * Get szedata2 device path and rx,tx channels from passed arguments.
	 */

	if (rte_kvargs_count(kvlist, RTE_ETH_SZEDATA2_DEV_PATH_ARG) != 1)
		goto err;

	if (rte_kvargs_count(kvlist, RTE_ETH_SZEDATA2_RX_IFACES_ARG) < 1)
		goto err;

	if (rte_kvargs_count(kvlist, RTE_ETH_SZEDATA2_TX_IFACES_ARG) < 1)
		goto err;

	for (k_idx = 0; k_idx < kvlist->count; k_idx++) {
		pair = &kvlist->pairs[k_idx];
		if (strstr(pair->key, RTE_ETH_SZEDATA2_DEV_PATH_ARG) != NULL) {
			szedata2.sze_dev = pair->value;
			dev_path_missing = false;
			break;
		}
	}

	if (dev_path_missing)
		goto err;

	ret = rte_kvargs_process(kvlist, RTE_ETH_SZEDATA2_RX_IFACES_ARG,
			&add_rx_mask, &szedata2);
	if (ret < 0)
		goto err;

	ret = rte_kvargs_process(kvlist, RTE_ETH_SZEDATA2_TX_IFACES_ARG,
			&add_tx_mask, &szedata2);
	if (ret < 0)
		goto err;

	szedata2.num_of_rx = count_ones(szedata2.sze_rx_mask_req);
	szedata2.num_of_tx = count_ones(szedata2.sze_tx_mask_req);

	RTE_LOG(INFO, PMD, "SZE device found at path %s\n", szedata2.sze_dev);

	return rte_eth_from_szedata2(name, &szedata2, numa_node);
err:
	rte_kvargs_free(kvlist);
	return -1;
}

static int
rte_pmd_szedata2_devuninit(const char *name)
{
	struct rte_eth_dev *dev = NULL;

	RTE_LOG(INFO, PMD, "Uninitializing pmd_szedata2 for %s "
			"on numa socket %u\n", name, rte_socket_id());

	if (name == NULL)
		return -1;

	dev = rte_eth_dev_allocated(name);
	if (dev == NULL)
		return -1;

	rte_free(dev->data->dev_private);
	rte_free(dev->data);
	rte_eth_dev_release_port(dev);
	return 0;
}

static struct rte_driver pmd_szedata2_drv = {
	.name = "eth_szedata2",
	.type = PMD_VDEV,
	.init = rte_pmd_szedata2_devinit,
	.uninit = rte_pmd_szedata2_devuninit,
};

PMD_REGISTER_DRIVER(pmd_szedata2_drv);
