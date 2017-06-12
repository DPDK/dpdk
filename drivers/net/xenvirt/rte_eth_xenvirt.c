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

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/user.h>
#ifndef PAGE_SIZE
#define PAGE_SIZE sysconf(_SC_PAGE_SIZE)
#endif
#include <linux/binfmts.h>
#include <xen/xen-compat.h>
#if __XEN_LATEST_INTERFACE_VERSION__ < 0x00040200
#include <xs.h>
#else
#include <xenstore.h>
#endif
#include <linux/virtio_ring.h>

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_vdev.h>
#include <cmdline_parse.h>
#include <cmdline_parse_etheraddr.h>

#include "rte_xen_lib.h"
#include "virtqueue.h"
#include "rte_eth_xenvirt.h"

#define VQ_DESC_NUM 256
#define VIRTIO_MBUF_BURST_SZ 64

/* virtio_idx is increased after new device is created.*/
static int virtio_idx = 0;

static struct rte_eth_link pmd_link = {
		.link_speed = ETH_SPEED_NUM_10G,
		.link_duplex = ETH_LINK_FULL_DUPLEX,
		.link_status = ETH_LINK_DOWN,
		.link_autoneg = ETH_LINK_SPEED_FIXED
};

static void
eth_xenvirt_free_queues(struct rte_eth_dev *dev);

static uint16_t
eth_xenvirt_rx(void *q, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct virtqueue *rxvq = q;
	struct rte_mbuf *rxm, *new_mbuf;
	uint16_t nb_used, num;
	uint32_t len[VIRTIO_MBUF_BURST_SZ];
	uint32_t i;
	struct pmd_internals *pi = rxvq->internals;

	nb_used = VIRTQUEUE_NUSED(rxvq);

	rte_smp_rmb();
	num = (uint16_t)(likely(nb_used <= nb_pkts) ? nb_used : nb_pkts);
	num = (uint16_t)(likely(num <= VIRTIO_MBUF_BURST_SZ) ? num : VIRTIO_MBUF_BURST_SZ);
	if (unlikely(num == 0)) return 0;

	num = virtqueue_dequeue_burst(rxvq, rx_pkts, len, num);
	PMD_RX_LOG(DEBUG, "used:%d dequeue:%d\n", nb_used, num);
	for (i = 0; i < num ; i ++) {
		rxm = rx_pkts[i];
		PMD_RX_LOG(DEBUG, "packet len:%d\n", len[i]);
		rxm->next = NULL;
		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rxm->data_len = (uint16_t)(len[i] - sizeof(struct virtio_net_hdr));
		rxm->nb_segs = 1;
		rxm->port = pi->port_id;
		rxm->pkt_len  = (uint32_t)(len[i] - sizeof(struct virtio_net_hdr));
	}
	/* allocate new mbuf for the used descriptor */
	while (likely(!virtqueue_full(rxvq))) {
		new_mbuf = rte_mbuf_raw_alloc(rxvq->mpool);
		if (unlikely(new_mbuf == NULL)) {
			break;
		}
		if (unlikely(virtqueue_enqueue_recv_refill(rxvq, new_mbuf))) {
			rte_pktmbuf_free_seg(new_mbuf);
			break;
		}
	}
	pi->eth_stats.ipackets += num;
	return num;
}

static uint16_t
eth_xenvirt_tx(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct virtqueue *txvq = tx_queue;
	struct rte_mbuf *txm;
	uint16_t nb_used, nb_tx, num, i;
	int error;
	uint32_t len[VIRTIO_MBUF_BURST_SZ];
	struct rte_mbuf *snd_pkts[VIRTIO_MBUF_BURST_SZ];
	struct pmd_internals *pi = txvq->internals;

	nb_tx = 0;

	if (unlikely(nb_pkts == 0))
		return 0;

	PMD_TX_LOG(DEBUG, "%d packets to xmit", nb_pkts);
	nb_used = VIRTQUEUE_NUSED(txvq);

	rte_smp_rmb();

	num = (uint16_t)(likely(nb_used <= VIRTIO_MBUF_BURST_SZ) ? nb_used : VIRTIO_MBUF_BURST_SZ);
	num = virtqueue_dequeue_burst(txvq, snd_pkts, len, num);

	for (i = 0; i < num ; i ++) {
		/* mergable not supported, one segment only */
		rte_pktmbuf_free_seg(snd_pkts[i]);
	}

	while (nb_tx < nb_pkts) {
		if (likely(!virtqueue_full(txvq))) {
		/* TODO drop tx_pkts if it contains multiple segments */
			txm = tx_pkts[nb_tx];
			error = virtqueue_enqueue_xmit(txvq, txm);
			if (unlikely(error)) {
				if (error == ENOSPC)
					PMD_TX_LOG(ERR, "virtqueue_enqueue Free count = 0\n");
				else if (error == EMSGSIZE)
					PMD_TX_LOG(ERR, "virtqueue_enqueue Free count < 1\n");
				else
					PMD_TX_LOG(ERR, "virtqueue_enqueue error: %d\n", error);
				break;
			}
			nb_tx++;
		} else {
			PMD_TX_LOG(ERR, "No free tx descriptors to transmit\n");
			/* virtqueue_notify not needed in our para-virt solution */
			break;
		}
	}
	pi->eth_stats.opackets += nb_tx;
	return nb_tx;
}

static int
eth_dev_configure(struct rte_eth_dev *dev __rte_unused)
{
	RTE_LOG(ERR, PMD, "%s\n", __func__);
	return 0;
}

/*
 * Create a shared page between guest and host.
 * Host monitors this page if it is cleared on unmap, and then
 * do necessary clean up.
 */
static void
gntalloc_vring_flag(int vtidx)
{
	char key_str[PATH_MAX];
	char val_str[PATH_MAX];
	uint32_t gref_tmp;
	void *ptr;

	if (grefwatch_from_alloc(&gref_tmp, &ptr)) {
		RTE_LOG(ERR, PMD, "grefwatch_from_alloc error\n");
		exit(0);
	}

	*(uint8_t *)ptr = MAP_FLAG;
	snprintf(val_str, sizeof(val_str), "%u", gref_tmp);
	snprintf(key_str, sizeof(key_str),
		DPDK_XENSTORE_PATH"%d"VRING_FLAG_STR, vtidx);
	xenstore_write(key_str, val_str);
}

/*
 * Notify host this virtio device is started.
 * Host could start polling this device.
 */
static void
dev_start_notify(int vtidx)
{
	char key_str[PATH_MAX];
	char val_str[PATH_MAX];

	RTE_LOG(INFO, PMD, "%s: virtio %d is started\n", __func__, vtidx);
	gntalloc_vring_flag(vtidx);

	snprintf(key_str, sizeof(key_str), "%s%s%d",
		DPDK_XENSTORE_PATH, EVENT_TYPE_START_STR,
			vtidx);
	snprintf(val_str, sizeof(val_str), "1");
	xenstore_write(key_str, val_str);
}

/*
 * Notify host this virtio device is stopped.
 * Host could stop polling this device.
 */
static void
dev_stop_notify(int vtidx)
{
	RTE_SET_USED(vtidx);
}


static int
update_mac_address(struct ether_addr *mac_addrs, int vtidx)
{
	char key_str[PATH_MAX];
	char val_str[PATH_MAX];
	int rv;

	if (mac_addrs == NULL) {
		RTE_LOG(ERR, PMD, "%s: NULL pointer mac specified\n", __func__);
		return -1;
	}
	rv = snprintf(key_str, sizeof(key_str),
			DPDK_XENSTORE_PATH"%d_ether_addr", vtidx);
	if (rv == -1)
		return rv;
	rv = snprintf(val_str, sizeof(val_str), "%02x:%02x:%02x:%02x:%02x:%02x",
			mac_addrs->addr_bytes[0],
			mac_addrs->addr_bytes[1],
			mac_addrs->addr_bytes[2],
			mac_addrs->addr_bytes[3],
			mac_addrs->addr_bytes[4],
			mac_addrs->addr_bytes[5]);
	if (rv == -1)
		return rv;
	if (xenstore_write(key_str, val_str))
		return rv;
	return 0;
}


static int
eth_dev_start(struct rte_eth_dev *dev)
{
	struct virtqueue *rxvq = dev->data->rx_queues[0];
	struct virtqueue *txvq = dev->data->tx_queues[0];
	struct rte_mbuf *m;
	struct pmd_internals *pi = (struct pmd_internals *)dev->data->dev_private;
	int rv;

	dev->data->dev_link.link_status = ETH_LINK_UP;
	while (!virtqueue_full(rxvq)) {
		m = rte_mbuf_raw_alloc(rxvq->mpool);
		if (m == NULL)
			break;
		/* Enqueue allocated buffers. */
		if (virtqueue_enqueue_recv_refill(rxvq, m)) {
			rte_pktmbuf_free_seg(m);
			break;
		}
	}

	rxvq->internals = pi;
	txvq->internals = pi;

	rv = update_mac_address(dev->data->mac_addrs, pi->virtio_idx);
	if (rv)
		return -1;
	dev_start_notify(pi->virtio_idx);

	return 0;
}

static void
eth_dev_stop(struct rte_eth_dev *dev)
{
	struct pmd_internals *pi = (struct pmd_internals *)dev->data->dev_private;

	dev->data->dev_link.link_status = ETH_LINK_DOWN;
	dev_stop_notify(pi->virtio_idx);
}

/*
 * Notify host this virtio device is closed.
 * Host could do necessary clean up to this device.
 */
static void
eth_dev_close(struct rte_eth_dev *dev)
{
	eth_xenvirt_free_queues(dev);
}

static void
eth_dev_info(struct rte_eth_dev *dev,
		struct rte_eth_dev_info *dev_info)
{
	struct pmd_internals *internals = dev->data->dev_private;

	RTE_SET_USED(internals);
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = (uint32_t)2048;
	dev_info->max_rx_queues = (uint16_t)1;
	dev_info->max_tx_queues = (uint16_t)1;
	dev_info->min_rx_bufsize = 0;
}

static void
eth_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct pmd_internals *internals = dev->data->dev_private;
	if(stats)
		rte_memcpy(stats, &internals->eth_stats, sizeof(*stats));
}

static void
eth_stats_reset(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = dev->data->dev_private;
	/* Reset software totals */
	memset(&internals->eth_stats, 0, sizeof(internals->eth_stats));
}

static void
eth_queue_release(void *q)
{
	rte_free(q);
}

static int
eth_link_update(struct rte_eth_dev *dev __rte_unused,
		int wait_to_complete __rte_unused)
{
	return 0;
}

/*
 * Create shared vring between guest and host.
 * Memory is allocated through grant alloc driver, so it is not physical continuous.
 */
static void *
gntalloc_vring_create(int queue_type, uint32_t size, int vtidx)
{
	char key_str[PATH_MAX] = {0};
	char val_str[PATH_MAX] = {0};
	void *va = NULL;
	int pg_size;
	uint32_t pg_num;
	uint32_t *gref_arr = NULL;
	phys_addr_t *pa_arr = NULL;
	uint64_t start_index;
	int rv;

	pg_size = getpagesize();
	size    = RTE_ALIGN_CEIL(size, pg_size);
	pg_num  = size / pg_size;

	gref_arr = calloc(pg_num, sizeof(gref_arr[0]));
	pa_arr  = calloc(pg_num, sizeof(pa_arr[0]));

	if (gref_arr == NULL || pa_arr == NULL) {
		RTE_LOG(ERR, PMD, "%s: calloc failed\n", __func__);
		goto out;
	}

	va  = gntalloc(size, gref_arr, &start_index);
	if (va == NULL) {
		RTE_LOG(ERR, PMD, "%s: gntalloc failed\n", __func__);
		goto out;
	}

	if (get_phys_map(va, pa_arr, pg_num, pg_size))
		goto out;

	/* write in xenstore gref and pfn for each page of vring */
	if (grant_node_create(pg_num, gref_arr, pa_arr, val_str, sizeof(val_str))) {
		gntfree(va, size, start_index);
		va = NULL;
		goto out;
	}

	if (queue_type == VTNET_RQ)
		rv = snprintf(key_str, sizeof(key_str), DPDK_XENSTORE_PATH"%d"RXVRING_XENSTORE_STR, vtidx);
	else
		rv = snprintf(key_str, sizeof(key_str), DPDK_XENSTORE_PATH"%d"TXVRING_XENSTORE_STR, vtidx);
	if (rv == -1 || xenstore_write(key_str, val_str) == -1) {
		gntfree(va, size, start_index);
		va = NULL;
	}
out:
	free(pa_arr);
	free(gref_arr);

	return va;
}



static struct virtqueue *
virtio_queue_setup(struct rte_eth_dev *dev, int queue_type)
{
	struct virtqueue *vq = NULL;
	uint16_t vq_size = VQ_DESC_NUM;
	int i = 0;
	char vq_name[VIRTQUEUE_MAX_NAME_SZ];
	size_t size;
	struct vring *vr;

	/* Allocate memory for virtqueue. */
	if (queue_type == VTNET_RQ) {
		snprintf(vq_name, sizeof(vq_name), "port%d_rvq",
				dev->data->port_id);
		vq = rte_zmalloc(vq_name, sizeof(struct virtqueue) +
			vq_size * sizeof(struct vq_desc_extra), RTE_CACHE_LINE_SIZE);
		if (vq == NULL) {
			RTE_LOG(ERR, PMD, "%s: unabled to allocate virtqueue\n", __func__);
			return NULL;
		}
		memcpy(vq->vq_name, vq_name, sizeof(vq->vq_name));
	} else if(queue_type == VTNET_TQ) {
		snprintf(vq_name, sizeof(vq_name), "port%d_tvq",
			dev->data->port_id);
		vq = rte_zmalloc(vq_name, sizeof(struct virtqueue) +
			vq_size * sizeof(struct vq_desc_extra), RTE_CACHE_LINE_SIZE);
		if (vq == NULL) {
			RTE_LOG(ERR, PMD, "%s: unabled to allocate virtqueue\n", __func__);
			return NULL;
		}
		memcpy(vq->vq_name, vq_name, sizeof(vq->vq_name));
	}

	memcpy(vq->vq_name, vq_name, sizeof(vq->vq_name));

	vq->vq_alignment = VIRTIO_PCI_VRING_ALIGN;
	vq->vq_nentries = vq_size;
	vq->vq_free_cnt = vq_size;
	/* Calcuate vring size according to virtio spec */
	size = vring_size(vq_size, VIRTIO_PCI_VRING_ALIGN);
	vq->vq_ring_size = RTE_ALIGN_CEIL(size, VIRTIO_PCI_VRING_ALIGN);
	/* Allocate memory for virtio vring through gntalloc driver*/
	vq->vq_ring_virt_mem = gntalloc_vring_create(queue_type, vq->vq_ring_size,
		((struct pmd_internals *)dev->data->dev_private)->virtio_idx);
	memset(vq->vq_ring_virt_mem, 0, vq->vq_ring_size);
	vr = &vq->vq_ring;
	vring_init(vr, vq_size, vq->vq_ring_virt_mem, vq->vq_alignment);
	/*
	 * Locally maintained last consumed index, this idex trails
	 * vq_ring.used->idx.
	 */
	vq->vq_used_cons_idx = 0;
	vq->vq_desc_head_idx = 0;
	vq->vq_free_cnt = vq->vq_nentries;
	memset(vq->vq_descx, 0, sizeof(struct vq_desc_extra) * vq->vq_nentries);

	/* Chain all the descriptors in the ring with an END */
	for (i = 0; i < vq_size - 1; i++)
		vr->desc[i].next = (uint16_t)(i + 1);
	vr->desc[i].next = VQ_RING_DESC_CHAIN_END;

	return vq;
}

static int
eth_rx_queue_setup(struct rte_eth_dev *dev,uint16_t rx_queue_id,
				uint16_t nb_rx_desc __rte_unused,
				unsigned int socket_id __rte_unused,
				const struct rte_eth_rxconf *rx_conf __rte_unused,
				struct rte_mempool *mb_pool)
{
	struct virtqueue *vq;
	vq = dev->data->rx_queues[rx_queue_id] = virtio_queue_setup(dev, VTNET_RQ);
	vq->mpool = mb_pool;
	return 0;
}

static int
eth_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
				uint16_t nb_tx_desc __rte_unused,
				unsigned int socket_id __rte_unused,
				const struct rte_eth_txconf *tx_conf __rte_unused)
{
	dev->data->tx_queues[tx_queue_id] = virtio_queue_setup(dev, VTNET_TQ);
	return 0;
}

static void
eth_xenvirt_free_queues(struct rte_eth_dev *dev)
{
	int i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		eth_queue_release(dev->data->rx_queues[i]);
		dev->data->rx_queues[i] = NULL;
	}
	dev->data->nb_rx_queues = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		eth_queue_release(dev->data->tx_queues[i]);
		dev->data->tx_queues[i] = NULL;
	}
	dev->data->nb_tx_queues = 0;
}

static const struct eth_dev_ops ops = {
	.dev_start = eth_dev_start,
	.dev_stop = eth_dev_stop,
	.dev_close = eth_dev_close,
	.dev_configure = eth_dev_configure,
	.dev_infos_get = eth_dev_info,
	.rx_queue_setup = eth_rx_queue_setup,
	.tx_queue_setup = eth_tx_queue_setup,
	.rx_queue_release = eth_queue_release,
	.tx_queue_release = eth_queue_release,
	.link_update = eth_link_update,
	.stats_get = eth_stats_get,
	.stats_reset = eth_stats_reset,
};


static int
rte_eth_xenvirt_parse_args(struct xenvirt_dict *dict,
			const char *name, const char *params)
{
	int i;
	char *pairs[RTE_ETH_XENVIRT_MAX_ARGS];
	int num_of_pairs;
	char *pair[2];
	char *args;
	int ret = -1;

	if (params == NULL)
		return 0;

	args = rte_zmalloc(NULL, strlen(params) + 1, RTE_CACHE_LINE_SIZE);
	if (args == NULL) {
		RTE_LOG(ERR, PMD, "Couldn't parse %s device \n", name);
		return -1;
	}
	rte_memcpy(args, params, strlen(params));

	num_of_pairs = rte_strsplit(args, strnlen(args, MAX_ARG_STRLEN),
					pairs,
					RTE_ETH_XENVIRT_MAX_ARGS ,
					RTE_ETH_XENVIRT_PAIRS_DELIM);

	for (i = 0; i < num_of_pairs; i++) {
		pair[0] = NULL;
		pair[1] = NULL;
		rte_strsplit(pairs[i], strnlen(pairs[i], MAX_ARG_STRLEN),
					pair, 2,
					RTE_ETH_XENVIRT_KEY_VALUE_DELIM);

		if (pair[0] == NULL || pair[1] == NULL || pair[0][0] == 0
			|| pair[1][0] == 0) {
			RTE_LOG(ERR, PMD,
				"Couldn't parse %s device,"
				"wrong key or value \n", name);
			goto err;
		}

		if (!strncmp(pair[0], RTE_ETH_XENVIRT_MAC_PARAM,
				sizeof(RTE_ETH_XENVIRT_MAC_PARAM))) {
			if (cmdline_parse_etheraddr(NULL,
						    pair[1],
						    &dict->addr,
						    sizeof(dict->addr)) < 0) {
				RTE_LOG(ERR, PMD,
					"Invalid %s device ether address\n",
					name);
				goto err;
			}

			dict->addr_valid = 1;
		}
	}

	ret = 0;
err:
	rte_free(args);
	return ret;
}

enum dev_action {
	DEV_CREATE,
	DEV_ATTACH
};

static struct rte_vdev_driver pmd_xenvirt_drv;

static int
eth_dev_xenvirt_create(const char *name, const char *params,
		const unsigned numa_node,
                enum dev_action action)
{
	struct rte_eth_dev_data *data = NULL;
	struct pmd_internals *internals = NULL;
	struct rte_eth_dev *eth_dev = NULL;
	struct xenvirt_dict dict;

	memset(&dict, 0, sizeof(struct xenvirt_dict));

	RTE_LOG(INFO, PMD, "Creating virtio rings backed ethdev on numa socket %u\n",
			numa_node);
	RTE_SET_USED(action);

	if (rte_eth_xenvirt_parse_args(&dict, name, params) < 0) {
		RTE_LOG(ERR, PMD, "%s: Failed to parse ethdev parameters\n", __func__);
		return -1;
	}

	/* now do all data allocation - for eth_dev structure, dummy pci driver
	 * and internal (private) data
	 */
	data = rte_zmalloc_socket(name, sizeof(*data), 0, numa_node);
	if (data == NULL)
		goto err;

	internals = rte_zmalloc_socket(name, sizeof(*internals), 0, numa_node);
	if (internals == NULL)
		goto err;

	/* reserve an ethdev entry */
	eth_dev = rte_eth_dev_allocate(name);
	if (eth_dev == NULL)
		goto err;

	data->dev_private = internals;
	data->port_id = eth_dev->data->port_id;
	data->nb_rx_queues = (uint16_t)1;
	data->nb_tx_queues = (uint16_t)1;
	data->dev_link = pmd_link;
	data->mac_addrs = rte_zmalloc("xen_virtio", ETHER_ADDR_LEN, 0);

	if(dict.addr_valid)
		memcpy(&data->mac_addrs->addr_bytes, &dict.addr, sizeof(struct ether_addr));
	else
		eth_random_addr(&data->mac_addrs->addr_bytes[0]);

	eth_dev->data = data;
	eth_dev->dev_ops = &ops;

	eth_dev->data->dev_flags = RTE_ETH_DEV_DETACHABLE;
	eth_dev->data->kdrv = RTE_KDRV_NONE;
	eth_dev->data->numa_node = numa_node;

	eth_dev->rx_pkt_burst = eth_xenvirt_rx;
	eth_dev->tx_pkt_burst = eth_xenvirt_tx;

	internals->virtio_idx = virtio_idx++;
	internals->port_id = eth_dev->data->port_id;

	return 0;

err:
	rte_free(data);
	rte_free(internals);

	return -1;
}


static int
eth_dev_xenvirt_free(const char *name, const unsigned numa_node)
{
	struct rte_eth_dev *eth_dev = NULL;

	RTE_LOG(DEBUG, PMD,
		"Free virtio rings backed ethdev on numa socket %u\n",
		numa_node);

	/* find an ethdev entry */
	eth_dev = rte_eth_dev_allocated(name);
	if (eth_dev == NULL)
		return -1;

	if (eth_dev->data->dev_started == 1) {
		eth_dev_stop(eth_dev);
		eth_dev_close(eth_dev);
	}

	eth_dev->rx_pkt_burst = NULL;
	eth_dev->tx_pkt_burst = NULL;
	eth_dev->dev_ops = NULL;

	rte_free(eth_dev->data);
	rte_free(eth_dev->data->dev_private);
	rte_free(eth_dev->data->mac_addrs);

	virtio_idx--;

	return 0;
}

/*TODO: Support multiple process model */
static int
rte_pmd_xenvirt_probe(struct rte_vdev_device *dev)
{
	if (virtio_idx == 0) {
		if (xenstore_init() != 0) {
			RTE_LOG(ERR, PMD, "%s: xenstore init failed\n", __func__);
			return -1;
		}
		if (gntalloc_open() != 0) {
			RTE_LOG(ERR, PMD, "%s: grant init failed\n", __func__);
			return -1;
		}
	}
	eth_dev_xenvirt_create(rte_vdev_device_name(dev),
		rte_vdev_device_args(dev), rte_socket_id(), DEV_CREATE);
	return 0;
}

static int
rte_pmd_xenvirt_remove(struct rte_vdev_device *dev)
{
	eth_dev_xenvirt_free(rte_vdev_device_name(dev), rte_socket_id());

	if (virtio_idx == 0) {
		if (xenstore_uninit() != 0)
			RTE_LOG(ERR, PMD, "%s: xenstore uninit failed\n", __func__);

		gntalloc_close();
	}
	return 0;
}

static struct rte_vdev_driver pmd_xenvirt_drv = {
	.probe = rte_pmd_xenvirt_probe,
	.remove = rte_pmd_xenvirt_remove,
};

RTE_PMD_REGISTER_VDEV(net_xenvirt, pmd_xenvirt_drv);
RTE_PMD_REGISTER_ALIAS(net_xenvirt, eth_xenvirt);
RTE_PMD_REGISTER_PARAM_STRING(net_xenvirt,
	"mac=<mac addr>");
