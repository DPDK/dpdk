/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include <rte_string_fns.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_vdev.h>
#include <rte_kni.h>
#include <rte_kvargs.h>
#include <rte_malloc.h>
#include <rte_bus_vdev.h>

/* Only single queue supported */
#define KNI_MAX_QUEUE_PER_PORT 1

#define MAX_KNI_PORTS 8

#define KNI_ETHER_MTU(mbuf_size)       \
	((mbuf_size) - RTE_ETHER_HDR_LEN) /**< Ethernet MTU. */

#define ETH_KNI_NO_REQUEST_THREAD_ARG	"no_request_thread"
static const char * const valid_arguments[] = {
	ETH_KNI_NO_REQUEST_THREAD_ARG,
	NULL
};

struct eth_kni_args {
	int no_request_thread;
};

struct pmd_queue_stats {
	uint64_t pkts;
	uint64_t bytes;
};

struct pmd_queue {
	struct pmd_internals *internals;
	struct rte_mempool *mb_pool;

	struct pmd_queue_stats rx;
	struct pmd_queue_stats tx;
};

struct pmd_internals {
	struct rte_kni *kni;
	uint16_t port_id;
	int is_kni_started;

	pthread_t thread;
	int stop_thread;
	int no_request_thread;

	struct rte_ether_addr eth_addr;

	struct pmd_queue rx_queues[KNI_MAX_QUEUE_PER_PORT];
	struct pmd_queue tx_queues[KNI_MAX_QUEUE_PER_PORT];
};

static const struct rte_eth_link pmd_link = {
		.link_speed = ETH_SPEED_NUM_10G,
		.link_duplex = ETH_LINK_FULL_DUPLEX,
		.link_status = ETH_LINK_DOWN,
		.link_autoneg = ETH_LINK_FIXED,
};
static int is_kni_initialized;

RTE_LOG_REGISTER(eth_kni_logtype, pmd.net.kni, NOTICE);

#define PMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, eth_kni_logtype, \
		"%s(): " fmt "\n", __func__, ##args)
static uint16_t
eth_kni_rx(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs)
{
	struct pmd_queue *kni_q = q;
	struct rte_kni *kni = kni_q->internals->kni;
	uint16_t nb_pkts;
	int i;

	nb_pkts = rte_kni_rx_burst(kni, bufs, nb_bufs);
	for (i = 0; i < nb_pkts; i++)
		bufs[i]->port = kni_q->internals->port_id;

	kni_q->rx.pkts += nb_pkts;

	return nb_pkts;
}

static uint16_t
eth_kni_tx(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs)
{
	struct pmd_queue *kni_q = q;
	struct rte_kni *kni = kni_q->internals->kni;
	uint16_t nb_pkts;

	nb_pkts =  rte_kni_tx_burst(kni, bufs, nb_bufs);

	kni_q->tx.pkts += nb_pkts;

	return nb_pkts;
}

static void *
kni_handle_request(void *param)
{
	struct pmd_internals *internals = param;
#define MS 1000

	while (!internals->stop_thread) {
		rte_kni_handle_request(internals->kni);
		usleep(500 * MS);
	}

	return param;
}

static int
eth_kni_start(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = dev->data->dev_private;
	uint16_t port_id = dev->data->port_id;
	struct rte_mempool *mb_pool;
	struct rte_kni_conf conf;
	const char *name = dev->device->name + 4; /* remove net_ */

	mb_pool = internals->rx_queues[0].mb_pool;
	strlcpy(conf.name, name, RTE_KNI_NAMESIZE);
	conf.force_bind = 0;
	conf.group_id = port_id;
	conf.mbuf_size =
		rte_pktmbuf_data_room_size(mb_pool) - RTE_PKTMBUF_HEADROOM;
	conf.mtu = KNI_ETHER_MTU(conf.mbuf_size);

	internals->kni = rte_kni_alloc(mb_pool, &conf, NULL);
	if (internals->kni == NULL) {
		PMD_LOG(ERR,
			"Fail to create kni interface for port: %d",
			port_id);
		return -1;
	}

	return 0;
}

static int
eth_kni_dev_start(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = dev->data->dev_private;
	int ret;

	if (internals->is_kni_started == 0) {
		ret = eth_kni_start(dev);
		if (ret)
			return -1;
		internals->is_kni_started = 1;
	}

	if (internals->no_request_thread == 0) {
		internals->stop_thread = 0;

		ret = rte_ctrl_thread_create(&internals->thread,
			"kni_handle_req", NULL,
			kni_handle_request, internals);
		if (ret) {
			PMD_LOG(ERR,
				"Fail to create kni request thread");
			return -1;
		}
	}

	dev->data->dev_link.link_status = 1;

	return 0;
}

static int
eth_kni_dev_stop(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = dev->data->dev_private;
	int ret;

	if (internals->no_request_thread == 0 && internals->stop_thread == 0) {
		internals->stop_thread = 1;

		ret = pthread_cancel(internals->thread);
		if (ret)
			PMD_LOG(ERR, "Can't cancel the thread");

		ret = pthread_join(internals->thread, NULL);
		if (ret)
			PMD_LOG(ERR, "Can't join the thread");
	}

	dev->data->dev_link.link_status = 0;
	dev->data->dev_started = 0;

	return 0;
}

static int
eth_kni_close(struct rte_eth_dev *eth_dev)
{
	struct pmd_internals *internals;
	int ret;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	ret = eth_kni_dev_stop(eth_dev);
	if (ret)
		PMD_LOG(WARNING, "Not able to stop kni for %s",
			eth_dev->data->name);

	/* mac_addrs must not be freed alone because part of dev_private */
	eth_dev->data->mac_addrs = NULL;

	internals = eth_dev->data->dev_private;
	ret = rte_kni_release(internals->kni);
	if (ret)
		PMD_LOG(WARNING, "Not able to release kni for %s",
			eth_dev->data->name);

	return ret;
}

static int
eth_kni_dev_configure(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

static int
eth_kni_dev_info(struct rte_eth_dev *dev __rte_unused,
		struct rte_eth_dev_info *dev_info)
{
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = UINT32_MAX;
	dev_info->max_rx_queues = KNI_MAX_QUEUE_PER_PORT;
	dev_info->max_tx_queues = KNI_MAX_QUEUE_PER_PORT;
	dev_info->min_rx_bufsize = 0;

	return 0;
}

static int
eth_kni_rx_queue_setup(struct rte_eth_dev *dev,
		uint16_t rx_queue_id,
		uint16_t nb_rx_desc __rte_unused,
		unsigned int socket_id __rte_unused,
		const struct rte_eth_rxconf *rx_conf __rte_unused,
		struct rte_mempool *mb_pool)
{
	struct pmd_internals *internals = dev->data->dev_private;
	struct pmd_queue *q;

	q = &internals->rx_queues[rx_queue_id];
	q->internals = internals;
	q->mb_pool = mb_pool;

	dev->data->rx_queues[rx_queue_id] = q;

	return 0;
}

static int
eth_kni_tx_queue_setup(struct rte_eth_dev *dev,
		uint16_t tx_queue_id,
		uint16_t nb_tx_desc __rte_unused,
		unsigned int socket_id __rte_unused,
		const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct pmd_internals *internals = dev->data->dev_private;
	struct pmd_queue *q;

	q = &internals->tx_queues[tx_queue_id];
	q->internals = internals;

	dev->data->tx_queues[tx_queue_id] = q;

	return 0;
}

static void
eth_kni_queue_release(void *q __rte_unused)
{
}

static int
eth_kni_link_update(struct rte_eth_dev *dev __rte_unused,
		int wait_to_complete __rte_unused)
{
	return 0;
}

static int
eth_kni_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	unsigned long rx_packets_total = 0, rx_bytes_total = 0;
	unsigned long tx_packets_total = 0, tx_bytes_total = 0;
	struct rte_eth_dev_data *data = dev->data;
	unsigned int i, num_stats;
	struct pmd_queue *q;

	num_stats = RTE_MIN((unsigned int)RTE_ETHDEV_QUEUE_STAT_CNTRS,
			data->nb_rx_queues);
	for (i = 0; i < num_stats; i++) {
		q = data->rx_queues[i];
		stats->q_ipackets[i] = q->rx.pkts;
		stats->q_ibytes[i] = q->rx.bytes;
		rx_packets_total += stats->q_ipackets[i];
		rx_bytes_total += stats->q_ibytes[i];
	}

	num_stats = RTE_MIN((unsigned int)RTE_ETHDEV_QUEUE_STAT_CNTRS,
			data->nb_tx_queues);
	for (i = 0; i < num_stats; i++) {
		q = data->tx_queues[i];
		stats->q_opackets[i] = q->tx.pkts;
		stats->q_obytes[i] = q->tx.bytes;
		tx_packets_total += stats->q_opackets[i];
		tx_bytes_total += stats->q_obytes[i];
	}

	stats->ipackets = rx_packets_total;
	stats->ibytes = rx_bytes_total;
	stats->opackets = tx_packets_total;
	stats->obytes = tx_bytes_total;

	return 0;
}

static int
eth_kni_stats_reset(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	struct pmd_queue *q;
	unsigned int i;

	for (i = 0; i < data->nb_rx_queues; i++) {
		q = data->rx_queues[i];
		q->rx.pkts = 0;
		q->rx.bytes = 0;
	}
	for (i = 0; i < data->nb_tx_queues; i++) {
		q = data->tx_queues[i];
		q->tx.pkts = 0;
		q->tx.bytes = 0;
	}

	return 0;
}

static const struct eth_dev_ops eth_kni_ops = {
	.dev_start = eth_kni_dev_start,
	.dev_stop = eth_kni_dev_stop,
	.dev_close = eth_kni_close,
	.dev_configure = eth_kni_dev_configure,
	.dev_infos_get = eth_kni_dev_info,
	.rx_queue_setup = eth_kni_rx_queue_setup,
	.tx_queue_setup = eth_kni_tx_queue_setup,
	.rx_queue_release = eth_kni_queue_release,
	.tx_queue_release = eth_kni_queue_release,
	.link_update = eth_kni_link_update,
	.stats_get = eth_kni_stats_get,
	.stats_reset = eth_kni_stats_reset,
};

static struct rte_eth_dev *
eth_kni_create(struct rte_vdev_device *vdev,
		struct eth_kni_args *args,
		unsigned int numa_node)
{
	struct pmd_internals *internals;
	struct rte_eth_dev_data *data;
	struct rte_eth_dev *eth_dev;

	PMD_LOG(INFO, "Creating kni ethdev on numa socket %u",
			numa_node);

	/* reserve an ethdev entry */
	eth_dev = rte_eth_vdev_allocate(vdev, sizeof(*internals));
	if (!eth_dev)
		return NULL;

	internals = eth_dev->data->dev_private;
	internals->port_id = eth_dev->data->port_id;
	data = eth_dev->data;
	data->nb_rx_queues = 1;
	data->nb_tx_queues = 1;
	data->dev_link = pmd_link;
	data->mac_addrs = &internals->eth_addr;
	data->promiscuous = 1;
	data->all_multicast = 1;
	data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	rte_eth_random_addr(internals->eth_addr.addr_bytes);

	eth_dev->dev_ops = &eth_kni_ops;

	internals->no_request_thread = args->no_request_thread;

	return eth_dev;
}

static int
kni_init(void)
{
	int ret;

	if (is_kni_initialized == 0) {
		ret = rte_kni_init(MAX_KNI_PORTS);
		if (ret < 0)
			return ret;
	}

	is_kni_initialized++;

	return 0;
}

static int
eth_kni_kvargs_process(struct eth_kni_args *args, const char *params)
{
	struct rte_kvargs *kvlist;

	kvlist = rte_kvargs_parse(params, valid_arguments);
	if (kvlist == NULL)
		return -1;

	memset(args, 0, sizeof(struct eth_kni_args));

	if (rte_kvargs_count(kvlist, ETH_KNI_NO_REQUEST_THREAD_ARG) == 1)
		args->no_request_thread = 1;

	rte_kvargs_free(kvlist);

	return 0;
}

static int
eth_kni_probe(struct rte_vdev_device *vdev)
{
	struct rte_eth_dev *eth_dev;
	struct eth_kni_args args;
	const char *name;
	const char *params;
	int ret;

	name = rte_vdev_device_name(vdev);
	params = rte_vdev_device_args(vdev);
	PMD_LOG(INFO, "Initializing eth_kni for %s", name);

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		eth_dev = rte_eth_dev_attach_secondary(name);
		if (!eth_dev) {
			PMD_LOG(ERR, "Failed to probe %s", name);
			return -1;
		}
		/* TODO: request info from primary to set up Rx and Tx */
		eth_dev->dev_ops = &eth_kni_ops;
		eth_dev->device = &vdev->device;
		rte_eth_dev_probing_finish(eth_dev);
		return 0;
	}

	ret = eth_kni_kvargs_process(&args, params);
	if (ret < 0)
		return ret;

	ret = kni_init();
	if (ret < 0)
		return ret;

	eth_dev = eth_kni_create(vdev, &args, rte_socket_id());
	if (eth_dev == NULL)
		goto kni_uninit;

	eth_dev->rx_pkt_burst = eth_kni_rx;
	eth_dev->tx_pkt_burst = eth_kni_tx;

	rte_eth_dev_probing_finish(eth_dev);
	return 0;

kni_uninit:
	is_kni_initialized--;
	if (is_kni_initialized == 0)
		rte_kni_close();
	return -1;
}

static int
eth_kni_remove(struct rte_vdev_device *vdev)
{
	struct rte_eth_dev *eth_dev;
	const char *name;
	int ret;

	name = rte_vdev_device_name(vdev);
	PMD_LOG(INFO, "Un-Initializing eth_kni for %s", name);

	/* find the ethdev entry */
	eth_dev = rte_eth_dev_allocated(name);
	if (eth_dev != NULL) {
		if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
			ret = eth_kni_dev_stop(eth_dev);
			if (ret != 0)
				return ret;
			return rte_eth_dev_release_port(eth_dev);
		}
		eth_kni_close(eth_dev);
		rte_eth_dev_release_port(eth_dev);
	}

	is_kni_initialized--;
	if (is_kni_initialized == 0)
		rte_kni_close();

	return 0;
}

static struct rte_vdev_driver eth_kni_drv = {
	.probe = eth_kni_probe,
	.remove = eth_kni_remove,
};

RTE_PMD_REGISTER_VDEV(net_kni, eth_kni_drv);
RTE_PMD_REGISTER_PARAM_STRING(net_kni, ETH_KNI_NO_REQUEST_THREAD_ARG "=<int>");
