/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#include "rte_eth_ring.h"
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_dev.h>
#include <rte_kvargs.h>

#define ETH_RING_NUMA_NODE_ACTION_ARG	"nodeaction"
#define ETH_RING_ACTION_CREATE		"CREATE"
#define ETH_RING_ACTION_ATTACH		"ATTACH"

static const char *ring_ethdev_driver_name = "Ring PMD";

static const char *valid_arguments[] = {
	ETH_RING_NUMA_NODE_ACTION_ARG,
	NULL
};

struct ring_queue {
	struct rte_ring *rng;
	rte_atomic64_t rx_pkts;
	rte_atomic64_t tx_pkts;
	rte_atomic64_t err_pkts;
};

struct pmd_internals {
	unsigned nb_rx_queues;
	unsigned nb_tx_queues;

	struct ring_queue rx_ring_queues[RTE_PMD_RING_MAX_RX_RINGS];
	struct ring_queue tx_ring_queues[RTE_PMD_RING_MAX_TX_RINGS];

	struct ether_addr address;
};


static const char *drivername = "Rings PMD";
static struct rte_eth_link pmd_link = {
		.link_speed = 10000,
		.link_duplex = ETH_LINK_FULL_DUPLEX,
		.link_status = 0
};

static uint16_t
eth_ring_rx(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs)
{
	void **ptrs = (void *)&bufs[0];
	struct ring_queue *r = q;
	const uint16_t nb_rx = (uint16_t)rte_ring_dequeue_burst(r->rng,
			ptrs, nb_bufs);
	if (r->rng->flags & RING_F_SC_DEQ)
		r->rx_pkts.cnt += nb_rx;
	else
		rte_atomic64_add(&(r->rx_pkts), nb_rx);
	return nb_rx;
}

static uint16_t
eth_ring_tx(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs)
{
	void **ptrs = (void *)&bufs[0];
	struct ring_queue *r = q;
	const uint16_t nb_tx = (uint16_t)rte_ring_enqueue_burst(r->rng,
			ptrs, nb_bufs);
	if (r->rng->flags & RING_F_SP_ENQ) {
		r->tx_pkts.cnt += nb_tx;
		r->err_pkts.cnt += nb_bufs - nb_tx;
	} else {
		rte_atomic64_add(&(r->tx_pkts), nb_tx);
		rte_atomic64_add(&(r->err_pkts), nb_bufs - nb_tx);
	}
	return nb_tx;
}

static int
eth_dev_configure(struct rte_eth_dev *dev __rte_unused) { return 0; }

static int
eth_dev_start(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = 1;
	return 0;
}

static void
eth_dev_stop(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = 0;
}

static int
eth_dev_set_link_down(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = 0;
	return 0;
}

static int
eth_dev_set_link_up(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = 1;
	return 0;
}

static int
eth_rx_queue_setup(struct rte_eth_dev *dev,uint16_t rx_queue_id,
				    uint16_t nb_rx_desc __rte_unused,
				    unsigned int socket_id __rte_unused,
				    const struct rte_eth_rxconf *rx_conf __rte_unused,
				    struct rte_mempool *mb_pool __rte_unused)
{
	struct pmd_internals *internals = dev->data->dev_private;
	dev->data->rx_queues[rx_queue_id] = &internals->rx_ring_queues[rx_queue_id];
	return 0;
}

static int
eth_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
				    uint16_t nb_tx_desc __rte_unused,
				    unsigned int socket_id __rte_unused,
				    const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct pmd_internals *internals = dev->data->dev_private;
	dev->data->tx_queues[tx_queue_id] = &internals->tx_ring_queues[tx_queue_id];
	return 0;
}


static void
eth_dev_info(struct rte_eth_dev *dev,
		struct rte_eth_dev_info *dev_info)
{
	struct pmd_internals *internals = dev->data->dev_private;
	dev_info->driver_name = drivername;
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = (uint32_t)-1;
	dev_info->max_rx_queues = (uint16_t)internals->nb_rx_queues;
	dev_info->max_tx_queues = (uint16_t)internals->nb_tx_queues;
	dev_info->min_rx_bufsize = 0;
	dev_info->pci_dev = NULL;
}

static void
eth_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *igb_stats)
{
	unsigned i;
	unsigned long rx_total = 0, tx_total = 0, tx_err_total = 0;
	const struct pmd_internals *internal = dev->data->dev_private;

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS &&
			i < internal->nb_rx_queues; i++) {
		igb_stats->q_ipackets[i] = internal->rx_ring_queues[i].rx_pkts.cnt;
		rx_total += igb_stats->q_ipackets[i];
	}

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS &&
			i < internal->nb_tx_queues; i++) {
		igb_stats->q_opackets[i] = internal->tx_ring_queues[i].tx_pkts.cnt;
		igb_stats->q_errors[i] = internal->tx_ring_queues[i].err_pkts.cnt;
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
	struct pmd_internals *internal = dev->data->dev_private;
	for (i = 0; i < internal->nb_rx_queues; i++)
		internal->rx_ring_queues[i].rx_pkts.cnt = 0;
	for (i = 0; i < internal->nb_tx_queues; i++) {
		internal->tx_ring_queues[i].tx_pkts.cnt = 0;
		internal->tx_ring_queues[i].err_pkts.cnt = 0;
	}
}

static void
eth_mac_addr_remove(struct rte_eth_dev *dev __rte_unused,
	uint32_t index __rte_unused)
{
}

static void
eth_mac_addr_add(struct rte_eth_dev *dev __rte_unused,
	struct ether_addr *mac_addr __rte_unused,
	uint32_t index __rte_unused,
	uint32_t vmdq __rte_unused)
{
}

static void
eth_queue_release(void *q __rte_unused) { ; }
static int
eth_link_update(struct rte_eth_dev *dev __rte_unused,
		int wait_to_complete __rte_unused) { return 0; }

static const struct eth_dev_ops ops = {
	.dev_start = eth_dev_start,
	.dev_stop = eth_dev_stop,
	.dev_set_link_up = eth_dev_set_link_up,
	.dev_set_link_down = eth_dev_set_link_down,
	.dev_configure = eth_dev_configure,
	.dev_infos_get = eth_dev_info,
	.rx_queue_setup = eth_rx_queue_setup,
	.tx_queue_setup = eth_tx_queue_setup,
	.rx_queue_release = eth_queue_release,
	.tx_queue_release = eth_queue_release,
	.link_update = eth_link_update,
	.stats_get = eth_stats_get,
	.stats_reset = eth_stats_reset,
	.mac_addr_remove = eth_mac_addr_remove,
	.mac_addr_add = eth_mac_addr_add,
};

static struct eth_driver rte_ring_pmd = {
	.pci_drv = {
		.name = "rte_ring_pmd",
		.drv_flags = RTE_PCI_DRV_DETACHABLE,
	},
};

static struct rte_pci_id id_table;

int
rte_eth_from_rings(const char *name, struct rte_ring *const rx_queues[],
		const unsigned nb_rx_queues,
		struct rte_ring *const tx_queues[],
		const unsigned nb_tx_queues,
		const unsigned numa_node)
{
	struct rte_eth_dev_data *data = NULL;
	struct rte_pci_device *pci_dev = NULL;
	struct pmd_internals *internals = NULL;
	struct rte_eth_dev *eth_dev = NULL;

	unsigned i;

	/* do some parameter checking */
	if (rx_queues == NULL && nb_rx_queues > 0)
		goto error;
	if (tx_queues == NULL && nb_tx_queues > 0)
		goto error;

	RTE_LOG(INFO, PMD, "Creating rings-backed ethdev on numa socket %u\n",
			numa_node);

	/* now do all data allocation - for eth_dev structure, dummy pci driver
	 * and internal (private) data
	 */
	data = rte_zmalloc_socket(name, sizeof(*data), 0, numa_node);
	if (data == NULL)
		goto error;

	pci_dev = rte_zmalloc_socket(name, sizeof(*pci_dev), 0, numa_node);
	if (pci_dev == NULL)
		goto error;

	internals = rte_zmalloc_socket(name, sizeof(*internals), 0, numa_node);
	if (internals == NULL)
		goto error;

	/* reserve an ethdev entry */
	eth_dev = rte_eth_dev_allocate(name, RTE_ETH_DEV_VIRTUAL);
	if (eth_dev == NULL)
		goto error;


	/* now put it all together
	 * - store queue data in internals,
	 * - store numa_node info in pci_driver
	 * - point eth_dev_data to internals and pci_driver
	 * - and point eth_dev structure to new eth_dev_data structure
	 */
	/* NOTE: we'll replace the data element, of originally allocated eth_dev
	 * so the rings are local per-process */

	internals->nb_rx_queues = nb_rx_queues;
	internals->nb_tx_queues = nb_tx_queues;
	for (i = 0; i < nb_rx_queues; i++) {
		internals->rx_ring_queues[i].rng = rx_queues[i];
	}
	for (i = 0; i < nb_tx_queues; i++) {
		internals->tx_ring_queues[i].rng = tx_queues[i];
	}

	rte_ring_pmd.pci_drv.name = ring_ethdev_driver_name;
	rte_ring_pmd.pci_drv.id_table = &id_table;

	pci_dev->numa_node = numa_node;
	pci_dev->driver = &rte_ring_pmd.pci_drv;

	data->dev_private = internals;
	data->port_id = eth_dev->data->port_id;
	memmove(data->name, eth_dev->data->name, sizeof(data->name));
	data->nb_rx_queues = (uint16_t)nb_rx_queues;
	data->nb_tx_queues = (uint16_t)nb_tx_queues;
	data->dev_link = pmd_link;
	data->mac_addrs = &internals->address;

	eth_dev->data = data;
	eth_dev->driver = &rte_ring_pmd;
	eth_dev->dev_ops = &ops;
	eth_dev->pci_dev = pci_dev;
	TAILQ_INIT(&(eth_dev->link_intr_cbs));

	/* finally assign rx and tx ops */
	eth_dev->rx_pkt_burst = eth_ring_rx;
	eth_dev->tx_pkt_burst = eth_ring_tx;

	return data->port_id;

error:
	rte_free(data);
	rte_free(pci_dev);
	rte_free(internals);

	return -1;
}

enum dev_action{
	DEV_CREATE,
	DEV_ATTACH
};

static int
eth_dev_ring_create(const char *name, const unsigned numa_node,
		enum dev_action action)
{
	/* rx and tx are so-called from point of view of first port.
	 * They are inverted from the point of view of second port
	 */
	struct rte_ring *rxtx[RTE_PMD_RING_MAX_RX_RINGS];
	unsigned i;
	char rng_name[RTE_RING_NAMESIZE];
	unsigned num_rings = RTE_MIN(RTE_PMD_RING_MAX_RX_RINGS,
			RTE_PMD_RING_MAX_TX_RINGS);

	for (i = 0; i < num_rings; i++) {
		snprintf(rng_name, sizeof(rng_name), "ETH_RXTX%u_%s", i, name);
		rxtx[i] = (action == DEV_CREATE) ?
				rte_ring_create(rng_name, 1024, numa_node,
						RING_F_SP_ENQ|RING_F_SC_DEQ) :
				rte_ring_lookup(rng_name);
		if (rxtx[i] == NULL)
			return -1;
	}

	if (rte_eth_from_rings(name, rxtx, num_rings, rxtx, num_rings, numa_node) < 0)
		return -1;

	return 0;
}


static int
eth_dev_ring_pair_create(const char *name, const unsigned numa_node,
		enum dev_action action)
{
	/* rx and tx are so-called from point of view of first port.
	 * They are inverted from the point of view of second port
	 */
	struct rte_ring *rx[RTE_PMD_RING_MAX_RX_RINGS];
	struct rte_ring *tx[RTE_PMD_RING_MAX_TX_RINGS];
	unsigned i;
	char rx_rng_name[RTE_RING_NAMESIZE];
	char tx_rng_name[RTE_RING_NAMESIZE];
	unsigned num_rings = RTE_MIN(RTE_PMD_RING_MAX_RX_RINGS,
			RTE_PMD_RING_MAX_TX_RINGS);

	for (i = 0; i < num_rings; i++) {
		snprintf(rx_rng_name, sizeof(rx_rng_name), "ETH_RX%u_%s", i, name);
		rx[i] = (action == DEV_CREATE) ?
				rte_ring_create(rx_rng_name, 1024, numa_node,
						RING_F_SP_ENQ|RING_F_SC_DEQ) :
				rte_ring_lookup(rx_rng_name);
		if (rx[i] == NULL)
			return -1;
		snprintf(tx_rng_name, sizeof(tx_rng_name), "ETH_TX%u_%s", i, name);
		tx[i] = (action == DEV_CREATE) ?
				rte_ring_create(tx_rng_name, 1024, numa_node,
						RING_F_SP_ENQ|RING_F_SC_DEQ):
				rte_ring_lookup(tx_rng_name);
		if (tx[i] == NULL)
			return -1;
	}

	if (rte_eth_from_rings(rx_rng_name, rx, num_rings, tx, num_rings,
				numa_node) < 0 ||
			rte_eth_from_rings(tx_rng_name, tx, num_rings, rx,
				num_rings, numa_node) < 0)
		return -1;

	return 0;
}

int
rte_eth_ring_pair_create(const char *name, const unsigned numa_node)
{
	RTE_LOG(WARNING, PMD, "rte_eth_ring_pair_create is deprecated\n");
	return eth_dev_ring_pair_create(name, numa_node, DEV_CREATE);
}

int
rte_eth_ring_pair_attach(const char *name, const unsigned numa_node)
{
	RTE_LOG(WARNING, PMD, "rte_eth_ring_pair_attach is deprecated\n");
	return eth_dev_ring_pair_create(name, numa_node, DEV_ATTACH);
}

struct node_action_pair {
	char name[PATH_MAX];
	unsigned node;
	enum dev_action action;
};

struct node_action_list {
	unsigned total;
	unsigned count;
	struct node_action_pair *list;
};

static int parse_kvlist (const char *key __rte_unused, const char *value, void *data)
{
	struct node_action_list *info = data;
	int ret;
	char *name;
	char *action;
	char *node;
	char *end;

	name = strdup(value);

	ret = -EINVAL;

	if (!name) {
		RTE_LOG(WARNING, PMD, "command line paramter is empty for ring pmd!\n");
		goto out;
	}

	node = strchr(name, ':');
	if (!node) {
		RTE_LOG(WARNING, PMD, "could not parse node value from %s", name);
		goto out;
	}

	*node = '\0';
	node++;

	action = strchr(node, ':');
	if (!action) {
		RTE_LOG(WARNING, PMD, "could not action value from %s", node);
		goto out;
	}

	*action = '\0';
	action++;

	/*
	 * Need to do some sanity checking here
	 */

	if (strcmp(action, ETH_RING_ACTION_ATTACH) == 0)
		info->list[info->count].action = DEV_ATTACH;
	else if (strcmp(action, ETH_RING_ACTION_CREATE) == 0)
		info->list[info->count].action = DEV_CREATE;
	else
		goto out;

	errno = 0;
	info->list[info->count].node = strtol(node, &end, 10);

	if ((errno != 0) || (*end != '\0')) {
		RTE_LOG(WARNING, PMD, "node value %s is unparseable as a number\n", node);
		goto out;
	}

	snprintf(info->list[info->count].name, sizeof(info->list[info->count].name), "%s", name);

	info->count++;

	ret = 0;
out:
	free(name);
	return ret;
}

static int
rte_pmd_ring_devinit(const char *name, const char *params)
{
	struct rte_kvargs *kvlist = NULL;
	int ret = 0;
	struct node_action_list *info = NULL;

	RTE_LOG(INFO, PMD, "Initializing pmd_ring for %s\n", name);

	if (params == NULL || params[0] == '\0') {
		ret = eth_dev_ring_create(name, rte_socket_id(), DEV_CREATE);
		if (ret == -1) {
			RTE_LOG(INFO, PMD,
				"Attach to pmd_ring for %s\n", name);
			ret = eth_dev_ring_create(name, rte_socket_id(),
						  DEV_ATTACH);
		}
	}
	else {
		kvlist = rte_kvargs_parse(params, valid_arguments);

		if (!kvlist) {
			RTE_LOG(INFO, PMD, "Ignoring unsupported parameters when creating"
					" rings-backed ethernet device\n");
			ret = eth_dev_ring_create(name, rte_socket_id(),
						  DEV_CREATE);
			if (ret == -1) {
				RTE_LOG(INFO, PMD,
					"Attach to pmd_ring for %s\n",
					name);
				ret = eth_dev_ring_create(name, rte_socket_id(),
							  DEV_ATTACH);
			}
			return ret;
		} else {
			ret = rte_kvargs_count(kvlist, ETH_RING_NUMA_NODE_ACTION_ARG);
			info = rte_zmalloc("struct node_action_list",
					   sizeof(struct node_action_list) +
					   (sizeof(struct node_action_pair) * ret),
					   0);
			if (!info)
				goto out_free;

			info->total = ret;
			info->list = (struct node_action_pair*)(info + 1);

			ret = rte_kvargs_process(kvlist, ETH_RING_NUMA_NODE_ACTION_ARG,
						 parse_kvlist, info);

			if (ret < 0)
				goto out_free;

			for (info->count = 0; info->count < info->total; info->count++) {
				ret = eth_dev_ring_create(name,
							  info->list[info->count].node,
							  info->list[info->count].action);
				if ((ret == -1) &&
				    (info->list[info->count].action == DEV_CREATE)) {
					RTE_LOG(INFO, PMD,
						"Attach to pmd_ring for %s\n",
						name);
					ret = eth_dev_ring_create(name,
							info->list[info->count].node,
							DEV_ATTACH);
				}
			}
		}
	}

out_free:
	rte_kvargs_free(kvlist);
	rte_free(info);
	return ret;
}

static int
rte_pmd_ring_devuninit(const char *name)
{
	struct rte_eth_dev *eth_dev = NULL;

	RTE_LOG(INFO, PMD, "Un-Initializing pmd_ring for %s\n", name);

	if (name == NULL)
		return -EINVAL;

	/* find an ethdev entry */
	eth_dev = rte_eth_dev_allocated(name);
	if (eth_dev == NULL)
		return -ENODEV;

	eth_dev_stop(eth_dev);
	rte_free(eth_dev->data->dev_private);
	rte_free(eth_dev->data);
	rte_free(eth_dev->pci_dev);

	rte_eth_dev_release_port(eth_dev);
	return 0;
}

static struct rte_driver pmd_ring_drv = {
	.name = "eth_ring",
	.type = PMD_VDEV,
	.init = rte_pmd_ring_devinit,
	.uninit = rte_pmd_ring_devuninit,
};

PMD_REGISTER_DRIVER(pmd_ring_drv);
