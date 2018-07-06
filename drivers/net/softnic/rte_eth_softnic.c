/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_ethdev_driver.h>
#include <rte_ethdev_vdev.h>
#include <rte_malloc.h>
#include <rte_bus_vdev.h>
#include <rte_kvargs.h>
#include <rte_errno.h>
#include <rte_ring.h>
#include <rte_tm_driver.h>

#include "rte_eth_softnic.h"
#include "rte_eth_softnic_internals.h"

#define PMD_PARAM_FIRMWARE                                 "firmware"
#define PMD_PARAM_CPU_ID                                   "cpu_id"

static const char *pmd_valid_args[] = {
	PMD_PARAM_FIRMWARE,
	PMD_PARAM_CPU_ID,
	NULL
};

static const struct rte_eth_dev_info pmd_dev_info = {
	.min_rx_bufsize = 0,
	.max_rx_pktlen = UINT32_MAX,
	.max_rx_queues = UINT16_MAX,
	.max_tx_queues = UINT16_MAX,
	.rx_desc_lim = {
		.nb_max = UINT16_MAX,
		.nb_min = 0,
		.nb_align = 1,
	},
	.tx_desc_lim = {
		.nb_max = UINT16_MAX,
		.nb_min = 0,
		.nb_align = 1,
	},
	.rx_offload_capa = DEV_RX_OFFLOAD_CRC_STRIP,
};

static int pmd_softnic_logtype;

#define PMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, pmd_softnic_logtype, \
		"%s(): " fmt "\n", __func__, ##args)

static void
pmd_dev_infos_get(struct rte_eth_dev *dev __rte_unused,
	struct rte_eth_dev_info *dev_info)
{
	memcpy(dev_info, &pmd_dev_info, sizeof(*dev_info));
}

static int
pmd_dev_configure(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

static int
pmd_rx_queue_setup(struct rte_eth_dev *dev,
	uint16_t rx_queue_id,
	uint16_t nb_rx_desc,
	unsigned int socket_id,
	const struct rte_eth_rxconf *rx_conf __rte_unused,
	struct rte_mempool *mb_pool __rte_unused)
{
	uint32_t size = RTE_ETH_NAME_MAX_LEN + strlen("_rxq") + 4;
	char name[size];
	struct rte_ring *r;

	snprintf(name, sizeof(name), "%s_rxq%04x",
		dev->data->name,
		rx_queue_id);

	r = rte_ring_create(name,
		nb_rx_desc,
		socket_id,
		RING_F_SP_ENQ | RING_F_SC_DEQ);
	if (r == NULL)
		return -1;

	dev->data->rx_queues[rx_queue_id] = r;
	return 0;
}

static int
pmd_tx_queue_setup(struct rte_eth_dev *dev,
	uint16_t tx_queue_id,
	uint16_t nb_tx_desc,
	unsigned int socket_id,
	const struct rte_eth_txconf *tx_conf __rte_unused)
{
	uint32_t size = RTE_ETH_NAME_MAX_LEN + strlen("_txq") + 4;
	char name[size];
	struct rte_ring *r;

	snprintf(name, sizeof(name), "%s_txq%04x",
		dev->data->name,
		tx_queue_id);

	r = rte_ring_create(name,
		nb_tx_desc,
		socket_id,
		RING_F_SP_ENQ | RING_F_SC_DEQ);
	if (r == NULL)
		return -1;

	dev->data->tx_queues[tx_queue_id] = r;
	return 0;
}

static int
pmd_dev_start(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = ETH_LINK_UP;

	return 0;
}

static void
pmd_dev_stop(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = ETH_LINK_DOWN;
}

static void
pmd_dev_close(struct rte_eth_dev *dev)
{
	uint32_t i;

	/* RX queues */
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		rte_ring_free((struct rte_ring *)dev->data->rx_queues[i]);

	/* TX queues */
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		rte_ring_free((struct rte_ring *)dev->data->tx_queues[i]);
}

static int
pmd_link_update(struct rte_eth_dev *dev __rte_unused,
	int wait_to_complete __rte_unused)
{
	return 0;
}

static int
pmd_tm_ops_get(struct rte_eth_dev *dev __rte_unused, void *arg)
{
	*(const struct rte_tm_ops **)arg = NULL;

	return 0;
}

static const struct eth_dev_ops pmd_ops = {
	.dev_configure = pmd_dev_configure,
	.dev_start = pmd_dev_start,
	.dev_stop = pmd_dev_stop,
	.dev_close = pmd_dev_close,
	.link_update = pmd_link_update,
	.dev_infos_get = pmd_dev_infos_get,
	.rx_queue_setup = pmd_rx_queue_setup,
	.tx_queue_setup = pmd_tx_queue_setup,
	.tm_ops_get = pmd_tm_ops_get,
};

static uint16_t
pmd_rx_pkt_burst(void *rxq,
	struct rte_mbuf **rx_pkts,
	uint16_t nb_pkts)
{
	return (uint16_t)rte_ring_sc_dequeue_burst(rxq,
		(void **)rx_pkts,
		nb_pkts,
		NULL);
}

static uint16_t
pmd_tx_pkt_burst(void *txq,
	struct rte_mbuf **tx_pkts,
	uint16_t nb_pkts)
{
	return (uint16_t)rte_ring_sp_enqueue_burst(txq,
		(void **)tx_pkts,
		nb_pkts,
		NULL);
}

int
rte_pmd_softnic_run(uint16_t port_id __rte_unused)
{
	return 0;
}

static void *
pmd_init(struct pmd_params *params)
{
	struct pmd_internals *p;

	p = rte_zmalloc_socket(params->name,
		sizeof(struct pmd_internals),
		0,
		params->cpu_id);
	if (p == NULL)
		return NULL;

	memcpy(&p->params, params, sizeof(p->params));

	return p;
}

static void
pmd_free(struct pmd_internals *p)
{
	rte_free(p);
}

static struct ether_addr eth_addr = {
	.addr_bytes = {0},
};

static int
pmd_ethdev_register(struct rte_vdev_device *vdev,
	struct pmd_params *params,
	void *dev_private)
{
	struct rte_eth_dev *dev;

	/* Ethdev entry allocation */
	dev = rte_eth_dev_allocate(params->name);
	if (!dev)
		return -ENOMEM;

	/* dev */
	dev->rx_pkt_burst = pmd_rx_pkt_burst;
	dev->tx_pkt_burst = pmd_tx_pkt_burst;
	dev->tx_pkt_prepare = NULL;
	dev->dev_ops = &pmd_ops;
	dev->device = &vdev->device;

	/* dev->data */
	dev->data->dev_private = dev_private;
	dev->data->dev_link.link_speed = ETH_SPEED_NUM_100G;
	dev->data->dev_link.link_duplex = ETH_LINK_FULL_DUPLEX;
	dev->data->dev_link.link_autoneg = ETH_LINK_FIXED;
	dev->data->dev_link.link_status = ETH_LINK_DOWN;
	dev->data->mac_addrs = &eth_addr;
	dev->data->promiscuous = 1;
	dev->data->kdrv = RTE_KDRV_NONE;
	dev->data->numa_node = params->cpu_id;

	rte_eth_dev_probing_finish(dev);

	return 0;
}

static int
get_string(const char *key __rte_unused, const char *value, void *extra_args)
{
	if (!value || !extra_args)
		return -EINVAL;

	*(char **)extra_args = strdup(value);

	if (!*(char **)extra_args)
		return -ENOMEM;

	return 0;
}

static int
get_uint32(const char *key __rte_unused, const char *value, void *extra_args)
{
	if (!value || !extra_args)
		return -EINVAL;

	*(uint32_t *)extra_args = strtoull(value, NULL, 0);

	return 0;
}

static int
pmd_parse_args(struct pmd_params *p, const char *params)
{
	struct rte_kvargs *kvlist;
	int ret = 0;

	kvlist = rte_kvargs_parse(params, pmd_valid_args);
	if (kvlist == NULL)
		return -EINVAL;

	/* Set default values */
	memset(p, 0, sizeof(*p));
	p->firmware = SOFTNIC_FIRMWARE;
	p->cpu_id = SOFTNIC_CPU_ID;

	/* Firmware script (optional) */
	if (rte_kvargs_count(kvlist, PMD_PARAM_FIRMWARE) == 1) {
		ret = rte_kvargs_process(kvlist, PMD_PARAM_FIRMWARE,
			&get_string, &p->firmware);
		if (ret < 0)
			goto out_free;
	}

	/* CPU ID (optional) */
	if (rte_kvargs_count(kvlist, PMD_PARAM_CPU_ID) == 1) {
		ret = rte_kvargs_process(kvlist, PMD_PARAM_CPU_ID,
			&get_uint32, &p->cpu_id);
		if (ret < 0)
			goto out_free;
	}

out_free:
	rte_kvargs_free(kvlist);
	return ret;
}

static int
pmd_probe(struct rte_vdev_device *vdev)
{
	struct pmd_params p;
	const char *params;
	int status = 0;

	void *dev_private;
	const char *name = rte_vdev_device_name(vdev);

	PMD_LOG(INFO, "Probing device \"%s\"", name);

	/* Parse input arguments */
	params = rte_vdev_device_args(vdev);
	if (!params)
		return -EINVAL;

	status = pmd_parse_args(&p, params);
	if (status)
		return status;

	p.name = name;

	/* Allocate and initialize soft ethdev private data */
	dev_private = pmd_init(&p);
	if (dev_private == NULL)
		return -ENOMEM;

	/* Register soft ethdev */
	PMD_LOG(INFO, "Creating soft ethdev \"%s\"", p.name);

	status = pmd_ethdev_register(vdev, &p, dev_private);
	if (status) {
		pmd_free(dev_private);
		return status;
	}

	return 0;
}

static int
pmd_remove(struct rte_vdev_device *vdev)
{
	struct rte_eth_dev *dev = NULL;
	struct pmd_internals *p;

	if (!vdev)
		return -EINVAL;

	PMD_LOG(INFO, "Removing device \"%s\"", rte_vdev_device_name(vdev));

	/* Find the ethdev entry */
	dev = rte_eth_dev_allocated(rte_vdev_device_name(vdev));
	if (dev == NULL)
		return -ENODEV;
	p = dev->data->dev_private;

	/* Free device data structures*/
	rte_free(dev->data);
	rte_eth_dev_release_port(dev);
	pmd_free(p);

	return 0;
}

static struct rte_vdev_driver pmd_softnic_drv = {
	.probe = pmd_probe,
	.remove = pmd_remove,
};

RTE_PMD_REGISTER_VDEV(net_softnic, pmd_softnic_drv);
RTE_PMD_REGISTER_PARAM_STRING(net_softnic,
	PMD_PARAM_FIRMWARE "=<string> "
	PMD_PARAM_CPU_ID "=<uint32>");

RTE_INIT(pmd_softnic_init_log)
{
	pmd_softnic_logtype = rte_log_register("pmd.net.softnic");
	if (pmd_softnic_logtype >= 0)
		rte_log_set_level(pmd_softnic_logtype, RTE_LOG_NOTICE);
}
