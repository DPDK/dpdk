/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_ethdev.h>
#include <rte_ethdev_vdev.h>
#include <rte_malloc.h>
#include <rte_vdev.h>
#include <rte_kvargs.h>
#include <rte_errno.h>
#include <rte_ring.h>

#include "rte_eth_softnic.h"
#include "rte_eth_softnic_internals.h"

#define DEV_HARD(p)					\
	(&rte_eth_devices[p->hard.port_id])

#define PMD_PARAM_HARD_NAME					"hard_name"
#define PMD_PARAM_HARD_TX_QUEUE_ID			"hard_tx_queue_id"

static const char *pmd_valid_args[] = {
	PMD_PARAM_HARD_NAME,
	PMD_PARAM_HARD_TX_QUEUE_ID,
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
};

static void
pmd_dev_infos_get(struct rte_eth_dev *dev __rte_unused,
	struct rte_eth_dev_info *dev_info)
{
	memcpy(dev_info, &pmd_dev_info, sizeof(*dev_info));
}

static int
pmd_dev_configure(struct rte_eth_dev *dev)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct rte_eth_dev *hard_dev = DEV_HARD(p);

	if (dev->data->nb_rx_queues > hard_dev->data->nb_rx_queues)
		return -1;

	if (p->params.hard.tx_queue_id >= hard_dev->data->nb_tx_queues)
		return -1;

	return 0;
}

static int
pmd_rx_queue_setup(struct rte_eth_dev *dev,
	uint16_t rx_queue_id,
	uint16_t nb_rx_desc __rte_unused,
	unsigned int socket_id,
	const struct rte_eth_rxconf *rx_conf __rte_unused,
	struct rte_mempool *mb_pool __rte_unused)
{
	struct pmd_internals *p = dev->data->dev_private;

	if (p->params.soft.intrusive == 0) {
		struct pmd_rx_queue *rxq;

		rxq = rte_zmalloc_socket(p->params.soft.name,
			sizeof(struct pmd_rx_queue), 0, socket_id);
		if (rxq == NULL)
			return -ENOMEM;

		rxq->hard.port_id = p->hard.port_id;
		rxq->hard.rx_queue_id = rx_queue_id;
		dev->data->rx_queues[rx_queue_id] = rxq;
	} else {
		struct rte_eth_dev *hard_dev = DEV_HARD(p);
		void *rxq = hard_dev->data->rx_queues[rx_queue_id];

		if (rxq == NULL)
			return -1;

		dev->data->rx_queues[rx_queue_id] = rxq;
	}
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
		dev->data->name, tx_queue_id);
	r = rte_ring_create(name, nb_tx_desc, socket_id,
		RING_F_SP_ENQ | RING_F_SC_DEQ);
	if (r == NULL)
		return -1;

	dev->data->tx_queues[tx_queue_id] = r;
	return 0;
}

static int
pmd_dev_start(struct rte_eth_dev *dev)
{
	struct pmd_internals *p = dev->data->dev_private;

	dev->data->dev_link.link_status = ETH_LINK_UP;

	if (p->params.soft.intrusive) {
		struct rte_eth_dev *hard_dev = DEV_HARD(p);

		/* The hard_dev->rx_pkt_burst should be stable by now */
		dev->rx_pkt_burst = hard_dev->rx_pkt_burst;
	}

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

static const struct eth_dev_ops pmd_ops = {
	.dev_configure = pmd_dev_configure,
	.dev_start = pmd_dev_start,
	.dev_stop = pmd_dev_stop,
	.dev_close = pmd_dev_close,
	.link_update = pmd_link_update,
	.dev_infos_get = pmd_dev_infos_get,
	.rx_queue_setup = pmd_rx_queue_setup,
	.tx_queue_setup = pmd_tx_queue_setup,
	.tm_ops_get = NULL,
};

static uint16_t
pmd_rx_pkt_burst(void *rxq,
	struct rte_mbuf **rx_pkts,
	uint16_t nb_pkts)
{
	struct pmd_rx_queue *rx_queue = rxq;

	return rte_eth_rx_burst(rx_queue->hard.port_id,
		rx_queue->hard.rx_queue_id,
		rx_pkts,
		nb_pkts);
}

static uint16_t
pmd_tx_pkt_burst(void *txq,
	struct rte_mbuf **tx_pkts,
	uint16_t nb_pkts)
{
	return (uint16_t)rte_ring_enqueue_burst(txq,
		(void **)tx_pkts,
		nb_pkts,
		NULL);
}

static __rte_always_inline int
run_default(struct rte_eth_dev *dev)
{
	struct pmd_internals *p = dev->data->dev_private;

	/* Persistent context: Read Only (update not required) */
	struct rte_mbuf **pkts = p->soft.def.pkts;
	uint16_t nb_tx_queues = dev->data->nb_tx_queues;

	/* Persistent context: Read - Write (update required) */
	uint32_t txq_pos = p->soft.def.txq_pos;
	uint32_t pkts_len = p->soft.def.pkts_len;
	uint32_t flush_count = p->soft.def.flush_count;

	/* Not part of the persistent context */
	uint32_t pos;
	uint16_t i;

	/* Soft device TXQ read, Hard device TXQ write */
	for (i = 0; i < nb_tx_queues; i++) {
		struct rte_ring *txq = dev->data->tx_queues[txq_pos];

		/* Read soft device TXQ burst to packet enqueue buffer */
		pkts_len += rte_ring_sc_dequeue_burst(txq,
			(void **)&pkts[pkts_len],
			DEFAULT_BURST_SIZE,
			NULL);

		/* Increment soft device TXQ */
		txq_pos++;
		if (txq_pos >= nb_tx_queues)
			txq_pos = 0;

		/* Hard device TXQ write when complete burst is available */
		if (pkts_len >= DEFAULT_BURST_SIZE) {
			for (pos = 0; pos < pkts_len; )
				pos += rte_eth_tx_burst(p->hard.port_id,
					p->params.hard.tx_queue_id,
					&pkts[pos],
					(uint16_t)(pkts_len - pos));

			pkts_len = 0;
			flush_count = 0;
			break;
		}
	}

	if (flush_count >= FLUSH_COUNT_THRESHOLD) {
		for (pos = 0; pos < pkts_len; )
			pos += rte_eth_tx_burst(p->hard.port_id,
				p->params.hard.tx_queue_id,
				&pkts[pos],
				(uint16_t)(pkts_len - pos));

		pkts_len = 0;
		flush_count = 0;
	}

	p->soft.def.txq_pos = txq_pos;
	p->soft.def.pkts_len = pkts_len;
	p->soft.def.flush_count = flush_count + 1;

	return 0;
}

int
rte_pmd_softnic_run(uint16_t port_id)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, 0);
#endif

	return run_default(dev);
}

static struct ether_addr eth_addr = { .addr_bytes = {0} };

static uint32_t
eth_dev_speed_max_mbps(uint32_t speed_capa)
{
	uint32_t rate_mbps[32] = {
		ETH_SPEED_NUM_NONE,
		ETH_SPEED_NUM_10M,
		ETH_SPEED_NUM_10M,
		ETH_SPEED_NUM_100M,
		ETH_SPEED_NUM_100M,
		ETH_SPEED_NUM_1G,
		ETH_SPEED_NUM_2_5G,
		ETH_SPEED_NUM_5G,
		ETH_SPEED_NUM_10G,
		ETH_SPEED_NUM_20G,
		ETH_SPEED_NUM_25G,
		ETH_SPEED_NUM_40G,
		ETH_SPEED_NUM_50G,
		ETH_SPEED_NUM_56G,
		ETH_SPEED_NUM_100G,
	};

	uint32_t pos = (speed_capa) ? (31 - __builtin_clz(speed_capa)) : 0;
	return rate_mbps[pos];
}

static int
default_init(struct pmd_internals *p,
	struct pmd_params *params,
	int numa_node)
{
	p->soft.def.pkts = rte_zmalloc_socket(params->soft.name,
		2 * DEFAULT_BURST_SIZE * sizeof(struct rte_mbuf *),
		0,
		numa_node);

	if (p->soft.def.pkts == NULL)
		return -ENOMEM;

	return 0;
}

static void
default_free(struct pmd_internals *p)
{
	rte_free(p->soft.def.pkts);
}

static void *
pmd_init(struct pmd_params *params, int numa_node)
{
	struct pmd_internals *p;
	int status;

	p = rte_zmalloc_socket(params->soft.name,
		sizeof(struct pmd_internals),
		0,
		numa_node);
	if (p == NULL)
		return NULL;

	memcpy(&p->params, params, sizeof(p->params));
	rte_eth_dev_get_port_by_name(params->hard.name, &p->hard.port_id);

	/* Default */
	status = default_init(p, params, numa_node);
	if (status) {
		free(p->params.hard.name);
		rte_free(p);
		return NULL;
	}

	return p;
}

static void
pmd_free(struct pmd_internals *p)
{
	default_free(p);

	free(p->params.hard.name);
	rte_free(p);
}

static int
pmd_ethdev_register(struct rte_vdev_device *vdev,
	struct pmd_params *params,
	void *dev_private)
{
	struct rte_eth_dev_info hard_info;
	struct rte_eth_dev *soft_dev;
	uint32_t hard_speed;
	int numa_node;
	uint16_t hard_port_id;

	rte_eth_dev_get_port_by_name(params->hard.name, &hard_port_id);
	rte_eth_dev_info_get(hard_port_id, &hard_info);
	hard_speed = eth_dev_speed_max_mbps(hard_info.speed_capa);
	numa_node = rte_eth_dev_socket_id(hard_port_id);

	/* Ethdev entry allocation */
	soft_dev = rte_eth_dev_allocate(params->soft.name);
	if (!soft_dev)
		return -ENOMEM;

	/* dev */
	soft_dev->rx_pkt_burst = (params->soft.intrusive) ?
		NULL : /* set up later */
		pmd_rx_pkt_burst;
	soft_dev->tx_pkt_burst = pmd_tx_pkt_burst;
	soft_dev->tx_pkt_prepare = NULL;
	soft_dev->dev_ops = &pmd_ops;
	soft_dev->device = &vdev->device;

	/* dev->data */
	soft_dev->data->dev_private = dev_private;
	soft_dev->data->dev_link.link_speed = hard_speed;
	soft_dev->data->dev_link.link_duplex = ETH_LINK_FULL_DUPLEX;
	soft_dev->data->dev_link.link_autoneg = ETH_LINK_SPEED_FIXED;
	soft_dev->data->dev_link.link_status = ETH_LINK_DOWN;
	soft_dev->data->mac_addrs = &eth_addr;
	soft_dev->data->promiscuous = 1;
	soft_dev->data->kdrv = RTE_KDRV_NONE;
	soft_dev->data->numa_node = numa_node;
	soft_dev->data->dev_flags = RTE_ETH_DEV_DETACHABLE;

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
pmd_parse_args(struct pmd_params *p, const char *name, const char *params)
{
	struct rte_kvargs *kvlist;
	int ret;

	kvlist = rte_kvargs_parse(params, pmd_valid_args);
	if (kvlist == NULL)
		return -EINVAL;

	/* Set default values */
	memset(p, 0, sizeof(*p));
	p->soft.name = name;
	p->soft.intrusive = INTRUSIVE;
	p->hard.tx_queue_id = SOFTNIC_HARD_TX_QUEUE_ID;

	/* HARD: name (mandatory) */
	if (rte_kvargs_count(kvlist, PMD_PARAM_HARD_NAME) == 1) {
		ret = rte_kvargs_process(kvlist, PMD_PARAM_HARD_NAME,
			&get_string, &p->hard.name);
		if (ret < 0)
			goto out_free;
	} else {
		ret = -EINVAL;
		goto out_free;
	}

	/* HARD: tx_queue_id (optional) */
	if (rte_kvargs_count(kvlist, PMD_PARAM_HARD_TX_QUEUE_ID) == 1) {
		ret = rte_kvargs_process(kvlist, PMD_PARAM_HARD_TX_QUEUE_ID,
			&get_uint32, &p->hard.tx_queue_id);
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
	int status;

	struct rte_eth_dev_info hard_info;
	uint16_t hard_port_id;
	int numa_node;
	void *dev_private;

	RTE_LOG(INFO, PMD,
		"Probing device \"%s\"\n",
		rte_vdev_device_name(vdev));

	/* Parse input arguments */
	params = rte_vdev_device_args(vdev);
	if (!params)
		return -EINVAL;

	status = pmd_parse_args(&p, rte_vdev_device_name(vdev), params);
	if (status)
		return status;

	/* Check input arguments */
	if (rte_eth_dev_get_port_by_name(p.hard.name, &hard_port_id))
		return -EINVAL;

	rte_eth_dev_info_get(hard_port_id, &hard_info);
	numa_node = rte_eth_dev_socket_id(hard_port_id);

	if (p.hard.tx_queue_id >= hard_info.max_tx_queues)
		return -EINVAL;

	/* Allocate and initialize soft ethdev private data */
	dev_private = pmd_init(&p, numa_node);
	if (dev_private == NULL)
		return -ENOMEM;

	/* Register soft ethdev */
	RTE_LOG(INFO, PMD,
		"Creating soft ethdev \"%s\" for hard ethdev \"%s\"\n",
		p.soft.name, p.hard.name);

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

	RTE_LOG(INFO, PMD, "Removing device \"%s\"\n",
		rte_vdev_device_name(vdev));

	/* Find the ethdev entry */
	dev = rte_eth_dev_allocated(rte_vdev_device_name(vdev));
	if (dev == NULL)
		return -ENODEV;
	p = dev->data->dev_private;

	/* Free device data structures*/
	pmd_free(p);
	rte_free(dev->data);
	rte_eth_dev_release_port(dev);

	return 0;
}

static struct rte_vdev_driver pmd_softnic_drv = {
	.probe = pmd_probe,
	.remove = pmd_remove,
};

RTE_PMD_REGISTER_VDEV(net_softnic, pmd_softnic_drv);
RTE_PMD_REGISTER_PARAM_STRING(net_softnic,
	PMD_PARAM_HARD_NAME "=<string> "
	PMD_PARAM_HARD_TX_QUEUE_ID "=<int>");
