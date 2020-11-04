/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include "rte_eth_ring.h"
#include <rte_mbuf.h>
#include <rte_ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_bus_vdev.h>
#include <rte_kvargs.h>
#include <rte_errno.h>

#define ETH_RING_NUMA_NODE_ACTION_ARG	"nodeaction"
#define ETH_RING_ACTION_CREATE		"CREATE"
#define ETH_RING_ACTION_ATTACH		"ATTACH"
#define ETH_RING_INTERNAL_ARG		"internal"
#define ETH_RING_INTERNAL_ARG_MAX_LEN	19 /* "0x..16chars..\0" */

static const char *valid_arguments[] = {
	ETH_RING_NUMA_NODE_ACTION_ARG,
	ETH_RING_INTERNAL_ARG,
	NULL
};

struct ring_internal_args {
	struct rte_ring * const *rx_queues;
	const unsigned int nb_rx_queues;
	struct rte_ring * const *tx_queues;
	const unsigned int nb_tx_queues;
	const unsigned int numa_node;
	void *addr; /* self addr for sanity check */
};

enum dev_action {
	DEV_CREATE,
	DEV_ATTACH
};

struct ring_queue {
	struct rte_ring *rng;
	rte_atomic64_t rx_pkts;
	rte_atomic64_t tx_pkts;
};

struct pmd_internals {
	unsigned int max_rx_queues;
	unsigned int max_tx_queues;

	struct ring_queue rx_ring_queues[RTE_PMD_RING_MAX_RX_RINGS];
	struct ring_queue tx_ring_queues[RTE_PMD_RING_MAX_TX_RINGS];

	struct rte_ether_addr address;
	enum dev_action action;
};

static struct rte_eth_link pmd_link = {
	.link_speed = ETH_SPEED_NUM_10G,
	.link_duplex = ETH_LINK_FULL_DUPLEX,
	.link_status = ETH_LINK_DOWN,
	.link_autoneg = ETH_LINK_FIXED,
};

RTE_LOG_REGISTER(eth_ring_logtype, pmd.net.ring, NOTICE);

#define PMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, eth_ring_logtype, \
		"%s(): " fmt "\n", __func__, ##args)

static uint16_t
eth_ring_rx(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs)
{
	void **ptrs = (void *)&bufs[0];
	struct ring_queue *r = q;
	const uint16_t nb_rx = (uint16_t)rte_ring_dequeue_burst(r->rng,
			ptrs, nb_bufs, NULL);
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
			ptrs, nb_bufs, NULL);
	if (r->rng->flags & RING_F_SP_ENQ)
		r->tx_pkts.cnt += nb_tx;
	else
		rte_atomic64_add(&(r->tx_pkts), nb_tx);
	return nb_tx;
}

static int
eth_dev_configure(struct rte_eth_dev *dev __rte_unused) { return 0; }

static int
eth_dev_start(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = ETH_LINK_UP;
	return 0;
}

static int
eth_dev_stop(struct rte_eth_dev *dev)
{
	dev->data->dev_started = 0;
	dev->data->dev_link.link_status = ETH_LINK_DOWN;
	return 0;
}

static int
eth_dev_set_link_down(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = ETH_LINK_DOWN;
	return 0;
}

static int
eth_dev_set_link_up(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = ETH_LINK_UP;
	return 0;
}

static int
eth_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
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


static int
eth_dev_info(struct rte_eth_dev *dev,
	     struct rte_eth_dev_info *dev_info)
{
	struct pmd_internals *internals = dev->data->dev_private;

	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = (uint32_t)-1;
	dev_info->max_rx_queues = (uint16_t)internals->max_rx_queues;
	dev_info->rx_offload_capa = DEV_RX_OFFLOAD_SCATTER;
	dev_info->tx_offload_capa = DEV_TX_OFFLOAD_MULTI_SEGS;
	dev_info->max_tx_queues = (uint16_t)internals->max_tx_queues;
	dev_info->min_rx_bufsize = 0;

	return 0;
}

static int
eth_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	unsigned int i;
	unsigned long rx_total = 0, tx_total = 0;
	const struct pmd_internals *internal = dev->data->dev_private;

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS &&
			i < dev->data->nb_rx_queues; i++) {
		stats->q_ipackets[i] = internal->rx_ring_queues[i].rx_pkts.cnt;
		rx_total += stats->q_ipackets[i];
	}

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS &&
			i < dev->data->nb_tx_queues; i++) {
		stats->q_opackets[i] = internal->tx_ring_queues[i].tx_pkts.cnt;
		tx_total += stats->q_opackets[i];
	}

	stats->ipackets = rx_total;
	stats->opackets = tx_total;

	return 0;
}

static int
eth_stats_reset(struct rte_eth_dev *dev)
{
	unsigned int i;
	struct pmd_internals *internal = dev->data->dev_private;

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		internal->rx_ring_queues[i].rx_pkts.cnt = 0;
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		internal->tx_ring_queues[i].tx_pkts.cnt = 0;

	return 0;
}

static void
eth_mac_addr_remove(struct rte_eth_dev *dev __rte_unused,
	uint32_t index __rte_unused)
{
}

static int
eth_mac_addr_add(struct rte_eth_dev *dev __rte_unused,
	struct rte_ether_addr *mac_addr __rte_unused,
	uint32_t index __rte_unused,
	uint32_t vmdq __rte_unused)
{
	return 0;
}

static void
eth_queue_release(void *q __rte_unused) { ; }
static int
eth_link_update(struct rte_eth_dev *dev __rte_unused,
		int wait_to_complete __rte_unused) { return 0; }

static int
eth_dev_close(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = NULL;
	struct ring_queue *r = NULL;
	uint16_t i;
	int ret;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	ret = eth_dev_stop(dev);

	internals = dev->data->dev_private;
	if (internals->action == DEV_CREATE) {
		/*
		 * it is only necessary to delete the rings in rx_queues because
		 * they are the same used in tx_queues
		 */
		for (i = 0; i < dev->data->nb_rx_queues; i++) {
			r = dev->data->rx_queues[i];
			rte_ring_free(r->rng);
		}
	}

	/* mac_addrs must not be freed alone because part of dev_private */
	dev->data->mac_addrs = NULL;

	return ret;
}

static const struct eth_dev_ops ops = {
	.dev_close = eth_dev_close,
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

static int
do_eth_dev_ring_create(const char *name,
		struct rte_vdev_device *vdev,
		struct rte_ring * const rx_queues[],
		const unsigned int nb_rx_queues,
		struct rte_ring *const tx_queues[],
		const unsigned int nb_tx_queues,
		const unsigned int numa_node, enum dev_action action,
		struct rte_eth_dev **eth_dev_p)
{
	struct rte_eth_dev_data *data = NULL;
	struct pmd_internals *internals = NULL;
	struct rte_eth_dev *eth_dev = NULL;
	void **rx_queues_local = NULL;
	void **tx_queues_local = NULL;
	unsigned int i;

	PMD_LOG(INFO, "Creating rings-backed ethdev on numa socket %u",
			numa_node);

	rx_queues_local = rte_calloc_socket(name, nb_rx_queues,
					    sizeof(void *), 0, numa_node);
	if (rx_queues_local == NULL) {
		rte_errno = ENOMEM;
		goto error;
	}

	tx_queues_local = rte_calloc_socket(name, nb_tx_queues,
					    sizeof(void *), 0, numa_node);
	if (tx_queues_local == NULL) {
		rte_errno = ENOMEM;
		goto error;
	}

	internals = rte_zmalloc_socket(name, sizeof(*internals), 0, numa_node);
	if (internals == NULL) {
		rte_errno = ENOMEM;
		goto error;
	}

	/* reserve an ethdev entry */
	eth_dev = rte_eth_dev_allocate(name);
	if (eth_dev == NULL) {
		rte_errno = ENOSPC;
		goto error;
	}

	/* now put it all together
	 * - store EAL device in eth_dev,
	 * - store queue data in internals,
	 * - store numa_node info in eth_dev_data
	 * - point eth_dev_data to internals
	 * - and point eth_dev structure to new eth_dev_data structure
	 */

	eth_dev->device = &vdev->device;

	data = eth_dev->data;
	data->rx_queues = rx_queues_local;
	data->tx_queues = tx_queues_local;

	internals->action = action;
	internals->max_rx_queues = nb_rx_queues;
	internals->max_tx_queues = nb_tx_queues;
	for (i = 0; i < nb_rx_queues; i++) {
		internals->rx_ring_queues[i].rng = rx_queues[i];
		data->rx_queues[i] = &internals->rx_ring_queues[i];
	}
	for (i = 0; i < nb_tx_queues; i++) {
		internals->tx_ring_queues[i].rng = tx_queues[i];
		data->tx_queues[i] = &internals->tx_ring_queues[i];
	}

	data->dev_private = internals;
	data->nb_rx_queues = (uint16_t)nb_rx_queues;
	data->nb_tx_queues = (uint16_t)nb_tx_queues;
	data->dev_link = pmd_link;
	data->mac_addrs = &internals->address;
	data->promiscuous = 1;
	data->all_multicast = 1;
	data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	eth_dev->dev_ops = &ops;
	data->numa_node = numa_node;

	/* finally assign rx and tx ops */
	eth_dev->rx_pkt_burst = eth_ring_rx;
	eth_dev->tx_pkt_burst = eth_ring_tx;

	rte_eth_dev_probing_finish(eth_dev);
	*eth_dev_p = eth_dev;

	return data->port_id;

error:
	rte_free(rx_queues_local);
	rte_free(tx_queues_local);
	rte_free(internals);

	return -1;
}

int
rte_eth_from_rings(const char *name, struct rte_ring *const rx_queues[],
		const unsigned int nb_rx_queues,
		struct rte_ring *const tx_queues[],
		const unsigned int nb_tx_queues,
		const unsigned int numa_node)
{
	struct ring_internal_args args = {
		.rx_queues = rx_queues,
		.nb_rx_queues = nb_rx_queues,
		.tx_queues = tx_queues,
		.nb_tx_queues = nb_tx_queues,
		.numa_node = numa_node,
		.addr = &args,
	};
	char args_str[32];
	char ring_name[RTE_RING_NAMESIZE];
	uint16_t port_id = RTE_MAX_ETHPORTS;
	int ret;

	/* do some parameter checking */
	if (rx_queues == NULL && nb_rx_queues > 0) {
		rte_errno = EINVAL;
		return -1;
	}
	if (tx_queues == NULL && nb_tx_queues > 0) {
		rte_errno = EINVAL;
		return -1;
	}
	if (nb_rx_queues > RTE_PMD_RING_MAX_RX_RINGS) {
		rte_errno = EINVAL;
		return -1;
	}

	snprintf(args_str, sizeof(args_str), "%s=%p",
		 ETH_RING_INTERNAL_ARG, &args);

	ret = snprintf(ring_name, sizeof(ring_name), "net_ring_%s", name);
	if (ret >= (int)sizeof(ring_name)) {
		rte_errno = ENAMETOOLONG;
		return -1;
	}

	ret = rte_vdev_init(ring_name, args_str);
	if (ret) {
		rte_errno = EINVAL;
		return -1;
	}

	ret = rte_eth_dev_get_port_by_name(ring_name, &port_id);
	if (ret) {
		rte_errno = ENODEV;
		return -1;
	}

	return port_id;
}

int
rte_eth_from_ring(struct rte_ring *r)
{
	return rte_eth_from_rings(r->name, &r, 1, &r, 1,
			r->memzone ? r->memzone->socket_id : SOCKET_ID_ANY);
}

static int
eth_dev_ring_create(const char *name,
		struct rte_vdev_device *vdev,
		const unsigned int numa_node,
		enum dev_action action, struct rte_eth_dev **eth_dev)
{
	/* rx and tx are so-called from point of view of first port.
	 * They are inverted from the point of view of second port
	 */
	struct rte_ring *rxtx[RTE_PMD_RING_MAX_RX_RINGS];
	unsigned int i;
	char rng_name[RTE_RING_NAMESIZE];
	unsigned int num_rings = RTE_MIN(RTE_PMD_RING_MAX_RX_RINGS,
			RTE_PMD_RING_MAX_TX_RINGS);

	for (i = 0; i < num_rings; i++) {
		int cc;

		cc = snprintf(rng_name, sizeof(rng_name),
			      "ETH_RXTX%u_%s", i, name);
		if (cc >= (int)sizeof(rng_name)) {
			rte_errno = ENAMETOOLONG;
			return -1;
		}

		rxtx[i] = (action == DEV_CREATE) ?
				rte_ring_create(rng_name, 1024, numa_node,
						RING_F_SP_ENQ|RING_F_SC_DEQ) :
				rte_ring_lookup(rng_name);
		if (rxtx[i] == NULL)
			return -1;
	}

	if (do_eth_dev_ring_create(name, vdev, rxtx, num_rings, rxtx, num_rings,
		numa_node, action, eth_dev) < 0)
		return -1;

	return 0;
}

struct node_action_pair {
	char name[PATH_MAX];
	unsigned int node;
	enum dev_action action;
};

struct node_action_list {
	unsigned int total;
	unsigned int count;
	struct node_action_pair *list;
};

static int parse_kvlist(const char *key __rte_unused,
			const char *value, void *data)
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
		PMD_LOG(WARNING, "command line parameter is empty for ring pmd!");
		goto out;
	}

	node = strchr(name, ':');
	if (!node) {
		PMD_LOG(WARNING, "could not parse node value from %s",
			name);
		goto out;
	}

	*node = '\0';
	node++;

	action = strchr(node, ':');
	if (!action) {
		PMD_LOG(WARNING, "could not parse action value from %s",
			node);
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
		PMD_LOG(WARNING,
			"node value %s is unparseable as a number", node);
		goto out;
	}

	strlcpy(info->list[info->count].name, name,
		sizeof(info->list[info->count].name));

	info->count++;

	ret = 0;
out:
	free(name);
	return ret;
}

static int
parse_internal_args(const char *key __rte_unused, const char *value,
		void *data)
{
	struct ring_internal_args **internal_args = data;
	void *args;
	int ret, n;

	/* make sure 'value' is valid pointer length */
	if (strnlen(value, ETH_RING_INTERNAL_ARG_MAX_LEN) >=
			ETH_RING_INTERNAL_ARG_MAX_LEN) {
		PMD_LOG(ERR, "Error parsing internal args, argument is too long");
		return -1;
	}

	ret = sscanf(value, "%p%n", &args, &n);
	if (ret == 0 || (size_t)n != strlen(value)) {
		PMD_LOG(ERR, "Error parsing internal args");

		return -1;
	}

	*internal_args = args;

	if ((*internal_args)->addr != args)
		return -1;

	return 0;
}

static int
rte_pmd_ring_probe(struct rte_vdev_device *dev)
{
	const char *name, *params;
	struct rte_kvargs *kvlist = NULL;
	int ret = 0;
	struct node_action_list *info = NULL;
	struct rte_eth_dev *eth_dev = NULL;
	struct ring_internal_args *internal_args;

	name = rte_vdev_device_name(dev);
	params = rte_vdev_device_args(dev);

	PMD_LOG(INFO, "Initializing pmd_ring for %s", name);

	if (params == NULL || params[0] == '\0') {
		ret = eth_dev_ring_create(name, dev, rte_socket_id(), DEV_CREATE,
				&eth_dev);
		if (ret == -1) {
			PMD_LOG(INFO,
				"Attach to pmd_ring for %s", name);
			ret = eth_dev_ring_create(name, dev, rte_socket_id(),
						  DEV_ATTACH, &eth_dev);
		}
	} else {
		kvlist = rte_kvargs_parse(params, valid_arguments);

		if (!kvlist) {
			PMD_LOG(INFO,
				"Ignoring unsupported parameters when creating rings-backed ethernet device");
			ret = eth_dev_ring_create(name, dev, rte_socket_id(),
						  DEV_CREATE, &eth_dev);
			if (ret == -1) {
				PMD_LOG(INFO,
					"Attach to pmd_ring for %s",
					name);
				ret = eth_dev_ring_create(name, dev, rte_socket_id(),
							  DEV_ATTACH, &eth_dev);
			}

			return ret;
		}

		if (rte_kvargs_count(kvlist, ETH_RING_INTERNAL_ARG) == 1) {
			ret = rte_kvargs_process(kvlist, ETH_RING_INTERNAL_ARG,
						 parse_internal_args,
						 &internal_args);
			if (ret < 0)
				goto out_free;

			ret = do_eth_dev_ring_create(name, dev,
				internal_args->rx_queues,
				internal_args->nb_rx_queues,
				internal_args->tx_queues,
				internal_args->nb_tx_queues,
				internal_args->numa_node,
				DEV_ATTACH,
				&eth_dev);
			if (ret >= 0)
				ret = 0;
		} else {
			ret = rte_kvargs_count(kvlist, ETH_RING_NUMA_NODE_ACTION_ARG);
			info = rte_zmalloc("struct node_action_list",
					   sizeof(struct node_action_list) +
					   (sizeof(struct node_action_pair) * ret),
					   0);
			if (!info)
				goto out_free;

			info->total = ret;
			info->list = (struct node_action_pair *)(info + 1);

			ret = rte_kvargs_process(kvlist, ETH_RING_NUMA_NODE_ACTION_ARG,
						 parse_kvlist, info);

			if (ret < 0)
				goto out_free;

			for (info->count = 0; info->count < info->total; info->count++) {
				ret = eth_dev_ring_create(info->list[info->count].name,
							  dev,
							  info->list[info->count].node,
							  info->list[info->count].action,
							  &eth_dev);
				if ((ret == -1) &&
				    (info->list[info->count].action == DEV_CREATE)) {
					PMD_LOG(INFO,
						"Attach to pmd_ring for %s",
						name);
					ret = eth_dev_ring_create(name, dev,
							info->list[info->count].node,
							DEV_ATTACH,
							&eth_dev);
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
rte_pmd_ring_remove(struct rte_vdev_device *dev)
{
	const char *name = rte_vdev_device_name(dev);
	struct rte_eth_dev *eth_dev = NULL;

	PMD_LOG(INFO, "Un-Initializing pmd_ring for %s", name);

	if (name == NULL)
		return -EINVAL;

	/* find an ethdev entry */
	eth_dev = rte_eth_dev_allocated(name);
	if (eth_dev == NULL)
		return 0; /* port already released */

	eth_dev_close(eth_dev);
	rte_eth_dev_release_port(eth_dev);
	return 0;
}

static struct rte_vdev_driver pmd_ring_drv = {
	.probe = rte_pmd_ring_probe,
	.remove = rte_pmd_ring_remove,
};

RTE_PMD_REGISTER_VDEV(net_ring, pmd_ring_drv);
RTE_PMD_REGISTER_ALIAS(net_ring, eth_ring);
RTE_PMD_REGISTER_PARAM_STRING(net_ring,
	ETH_RING_NUMA_NODE_ACTION_ARG "=name:node:action(ATTACH|CREATE)");
