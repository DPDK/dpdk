/*-
 *   BSD LICENSE
 *
 *   Copyright 2012 6WIND S.A.
 *   Copyright 2012 Mellanox
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

/* System headers. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ethdev_pci.h>
#include <rte_dev.h>
#include <rte_mbuf.h>
#include <rte_errno.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_flow.h>
#include <rte_kvargs.h>
#include <rte_interrupts.h>
#include <rte_common.h>

/* PMD headers. */
#include "mlx4.h"
#include "mlx4_flow.h"
#include "mlx4_rxtx.h"
#include "mlx4_utils.h"

/** Configuration structure for device arguments. */
struct mlx4_conf {
	struct {
		uint32_t present; /**< Bit-field for existing ports. */
		uint32_t enabled; /**< Bit-field for user-enabled ports. */
	} ports;
};

/* Available parameters list. */
const char *pmd_mlx4_init_params[] = {
	MLX4_PMD_PORT_KVARG,
	NULL,
};

/* Device configuration. */

static int
rxq_setup(struct rte_eth_dev *dev, struct rxq *rxq, uint16_t desc,
	  unsigned int socket, const struct rte_eth_rxconf *conf,
	  struct rte_mempool *mp);

static void
rxq_cleanup(struct rxq *rxq);

static void
priv_mac_addr_del(struct priv *priv);

/**
 * DPDK callback for Ethernet device configuration.
 *
 * Prepare the driver for a given number of TX and RX queues.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_dev_configure(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	unsigned int rxqs_n = dev->data->nb_rx_queues;
	unsigned int txqs_n = dev->data->nb_tx_queues;

	priv->rxqs = (void *)dev->data->rx_queues;
	priv->txqs = (void *)dev->data->tx_queues;
	if (txqs_n != priv->txqs_n) {
		INFO("%p: TX queues number update: %u -> %u",
		     (void *)dev, priv->txqs_n, txqs_n);
		priv->txqs_n = txqs_n;
	}
	if (rxqs_n != priv->rxqs_n) {
		INFO("%p: Rx queues number update: %u -> %u",
		     (void *)dev, priv->rxqs_n, rxqs_n);
		priv->rxqs_n = rxqs_n;
	}
	return 0;
}

struct mlx4_check_mempool_data {
	int ret;
	char *start;
	char *end;
};

/* Called by mlx4_check_mempool() when iterating the memory chunks. */
static void mlx4_check_mempool_cb(struct rte_mempool *mp,
	void *opaque, struct rte_mempool_memhdr *memhdr,
	unsigned mem_idx)
{
	struct mlx4_check_mempool_data *data = opaque;

	(void)mp;
	(void)mem_idx;
	/* It already failed, skip the next chunks. */
	if (data->ret != 0)
		return;
	/* It is the first chunk. */
	if (data->start == NULL && data->end == NULL) {
		data->start = memhdr->addr;
		data->end = data->start + memhdr->len;
		return;
	}
	if (data->end == memhdr->addr) {
		data->end += memhdr->len;
		return;
	}
	if (data->start == (char *)memhdr->addr + memhdr->len) {
		data->start -= memhdr->len;
		return;
	}
	/* Error, mempool is not virtually contigous. */
	data->ret = -1;
}

/**
 * Check if a mempool can be used: it must be virtually contiguous.
 *
 * @param[in] mp
 *   Pointer to memory pool.
 * @param[out] start
 *   Pointer to the start address of the mempool virtual memory area
 * @param[out] end
 *   Pointer to the end address of the mempool virtual memory area
 *
 * @return
 *   0 on success (mempool is virtually contiguous), -1 on error.
 */
static int mlx4_check_mempool(struct rte_mempool *mp, uintptr_t *start,
	uintptr_t *end)
{
	struct mlx4_check_mempool_data data;

	memset(&data, 0, sizeof(data));
	rte_mempool_mem_iter(mp, mlx4_check_mempool_cb, &data);
	*start = (uintptr_t)data.start;
	*end = (uintptr_t)data.end;
	return data.ret;
}

/**
 * Register mempool as a memory region.
 *
 * @param pd
 *   Pointer to protection domain.
 * @param mp
 *   Pointer to memory pool.
 *
 * @return
 *   Memory region pointer, NULL in case of error and rte_errno is set.
 */
struct ibv_mr *
mlx4_mp2mr(struct ibv_pd *pd, struct rte_mempool *mp)
{
	const struct rte_memseg *ms = rte_eal_get_physmem_layout();
	uintptr_t start;
	uintptr_t end;
	unsigned int i;
	struct ibv_mr *mr;

	if (mlx4_check_mempool(mp, &start, &end) != 0) {
		rte_errno = EINVAL;
		ERROR("mempool %p: not virtually contiguous",
			(void *)mp);
		return NULL;
	}
	DEBUG("mempool %p area start=%p end=%p size=%zu",
	      (void *)mp, (void *)start, (void *)end,
	      (size_t)(end - start));
	/* Round start and end to page boundary if found in memory segments. */
	for (i = 0; (i < RTE_MAX_MEMSEG) && (ms[i].addr != NULL); ++i) {
		uintptr_t addr = (uintptr_t)ms[i].addr;
		size_t len = ms[i].len;
		unsigned int align = ms[i].hugepage_sz;

		if ((start > addr) && (start < addr + len))
			start = RTE_ALIGN_FLOOR(start, align);
		if ((end > addr) && (end < addr + len))
			end = RTE_ALIGN_CEIL(end, align);
	}
	DEBUG("mempool %p using start=%p end=%p size=%zu for MR",
	      (void *)mp, (void *)start, (void *)end,
	      (size_t)(end - start));
	mr = ibv_reg_mr(pd,
			(void *)start,
			end - start,
			IBV_ACCESS_LOCAL_WRITE);
	if (!mr)
		rte_errno = errno ? errno : EINVAL;
	return mr;
}

/* RX queues handling. */

/**
 * Allocate RX queue elements.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 * @param elts_n
 *   Number of elements to allocate.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
rxq_alloc_elts(struct rxq *rxq, unsigned int elts_n)
{
	unsigned int i;
	struct rxq_elt (*elts)[elts_n] =
		rte_calloc_socket("RXQ elements", 1, sizeof(*elts), 0,
				  rxq->socket);

	if (elts == NULL) {
		rte_errno = ENOMEM;
		ERROR("%p: can't allocate packets array", (void *)rxq);
		goto error;
	}
	/* For each WR (packet). */
	for (i = 0; (i != elts_n); ++i) {
		struct rxq_elt *elt = &(*elts)[i];
		struct ibv_recv_wr *wr = &elt->wr;
		struct ibv_sge *sge = &(*elts)[i].sge;
		struct rte_mbuf *buf = rte_pktmbuf_alloc(rxq->mp);

		if (buf == NULL) {
			rte_errno = ENOMEM;
			ERROR("%p: empty mbuf pool", (void *)rxq);
			goto error;
		}
		elt->buf = buf;
		wr->next = &(*elts)[(i + 1)].wr;
		wr->sg_list = sge;
		wr->num_sge = 1;
		/* Headroom is reserved by rte_pktmbuf_alloc(). */
		assert(buf->data_off == RTE_PKTMBUF_HEADROOM);
		/* Buffer is supposed to be empty. */
		assert(rte_pktmbuf_data_len(buf) == 0);
		assert(rte_pktmbuf_pkt_len(buf) == 0);
		/* sge->addr must be able to store a pointer. */
		assert(sizeof(sge->addr) >= sizeof(uintptr_t));
		/* SGE keeps its headroom. */
		sge->addr = (uintptr_t)
			((uint8_t *)buf->buf_addr + RTE_PKTMBUF_HEADROOM);
		sge->length = (buf->buf_len - RTE_PKTMBUF_HEADROOM);
		sge->lkey = rxq->mr->lkey;
		/* Redundant check for tailroom. */
		assert(sge->length == rte_pktmbuf_tailroom(buf));
	}
	/* The last WR pointer must be NULL. */
	(*elts)[(i - 1)].wr.next = NULL;
	DEBUG("%p: allocated and configured %u single-segment WRs",
	      (void *)rxq, elts_n);
	rxq->elts_n = elts_n;
	rxq->elts_head = 0;
	rxq->elts = elts;
	return 0;
error:
	if (elts != NULL) {
		for (i = 0; (i != RTE_DIM(*elts)); ++i)
			rte_pktmbuf_free_seg((*elts)[i].buf);
		rte_free(elts);
	}
	DEBUG("%p: failed, freed everything", (void *)rxq);
	assert(rte_errno > 0);
	return -rte_errno;
}

/**
 * Free RX queue elements.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 */
static void
rxq_free_elts(struct rxq *rxq)
{
	unsigned int i;
	unsigned int elts_n = rxq->elts_n;
	struct rxq_elt (*elts)[elts_n] = rxq->elts;

	DEBUG("%p: freeing WRs", (void *)rxq);
	rxq->elts_n = 0;
	rxq->elts = NULL;
	if (elts == NULL)
		return;
	for (i = 0; (i != RTE_DIM(*elts)); ++i)
		rte_pktmbuf_free_seg((*elts)[i].buf);
	rte_free(elts);
}

/**
 * Unregister a MAC address.
 *
 * @param priv
 *   Pointer to private structure.
 */
static void
priv_mac_addr_del(struct priv *priv)
{
#ifndef NDEBUG
	uint8_t (*mac)[ETHER_ADDR_LEN] = &priv->mac.addr_bytes;
#endif

	if (!priv->mac_flow)
		return;
	DEBUG("%p: removing MAC address %02x:%02x:%02x:%02x:%02x:%02x",
	      (void *)priv,
	      (*mac)[0], (*mac)[1], (*mac)[2], (*mac)[3], (*mac)[4], (*mac)[5]);
	claim_zero(ibv_destroy_flow(priv->mac_flow));
	priv->mac_flow = NULL;
}

/**
 * Register a MAC address.
 *
 * The MAC address is registered in queue 0.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
priv_mac_addr_add(struct priv *priv)
{
	uint8_t (*mac)[ETHER_ADDR_LEN] = &priv->mac.addr_bytes;
	struct rxq *rxq;
	struct ibv_flow *flow;

	/* If device isn't started, this is all we need to do. */
	if (!priv->started)
		return 0;
	if (priv->isolated)
		return 0;
	if (*priv->rxqs && (*priv->rxqs)[0])
		rxq = (*priv->rxqs)[0];
	else
		return 0;

	/* Allocate flow specification on the stack. */
	struct __attribute__((packed)) {
		struct ibv_flow_attr attr;
		struct ibv_flow_spec_eth spec;
	} data;
	struct ibv_flow_attr *attr = &data.attr;
	struct ibv_flow_spec_eth *spec = &data.spec;

	if (priv->mac_flow)
		priv_mac_addr_del(priv);
	/*
	 * No padding must be inserted by the compiler between attr and spec.
	 * This layout is expected by libibverbs.
	 */
	assert(((uint8_t *)attr + sizeof(*attr)) == (uint8_t *)spec);
	*attr = (struct ibv_flow_attr){
		.type = IBV_FLOW_ATTR_NORMAL,
		.priority = 3,
		.num_of_specs = 1,
		.port = priv->port,
		.flags = 0
	};
	*spec = (struct ibv_flow_spec_eth){
		.type = IBV_FLOW_SPEC_ETH,
		.size = sizeof(*spec),
		.val = {
			.dst_mac = {
				(*mac)[0], (*mac)[1], (*mac)[2],
				(*mac)[3], (*mac)[4], (*mac)[5]
			},
		},
		.mask = {
			.dst_mac = "\xff\xff\xff\xff\xff\xff",
		}
	};
	DEBUG("%p: adding MAC address %02x:%02x:%02x:%02x:%02x:%02x",
	      (void *)priv,
	      (*mac)[0], (*mac)[1], (*mac)[2], (*mac)[3], (*mac)[4], (*mac)[5]);
	/* Create related flow. */
	flow = ibv_create_flow(rxq->qp, attr);
	if (flow == NULL) {
		rte_errno = errno ? errno : EINVAL;
		ERROR("%p: flow configuration failed, errno=%d: %s",
		      (void *)rxq, rte_errno, strerror(errno));
		return -rte_errno;
	}
	assert(priv->mac_flow == NULL);
	priv->mac_flow = flow;
	return 0;
}

/**
 * Clean up a RX queue.
 *
 * Destroy objects, free allocated memory and reset the structure for reuse.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 */
static void
rxq_cleanup(struct rxq *rxq)
{
	DEBUG("cleaning up %p", (void *)rxq);
	rxq_free_elts(rxq);
	if (rxq->qp != NULL)
		claim_zero(ibv_destroy_qp(rxq->qp));
	if (rxq->cq != NULL)
		claim_zero(ibv_destroy_cq(rxq->cq));
	if (rxq->channel != NULL)
		claim_zero(ibv_destroy_comp_channel(rxq->channel));
	if (rxq->mr != NULL)
		claim_zero(ibv_dereg_mr(rxq->mr));
	memset(rxq, 0, sizeof(*rxq));
}

/**
 * Allocate a Queue Pair.
 * Optionally setup inline receive if supported.
 *
 * @param priv
 *   Pointer to private structure.
 * @param cq
 *   Completion queue to associate with QP.
 * @param desc
 *   Number of descriptors in QP (hint only).
 *
 * @return
 *   QP pointer or NULL in case of error and rte_errno is set.
 */
static struct ibv_qp *
rxq_setup_qp(struct priv *priv, struct ibv_cq *cq, uint16_t desc)
{
	struct ibv_qp *qp;
	struct ibv_qp_init_attr attr = {
		/* CQ to be associated with the send queue. */
		.send_cq = cq,
		/* CQ to be associated with the receive queue. */
		.recv_cq = cq,
		.cap = {
			/* Max number of outstanding WRs. */
			.max_recv_wr = ((priv->device_attr.max_qp_wr < desc) ?
					priv->device_attr.max_qp_wr :
					desc),
			/* Max number of scatter/gather elements in a WR. */
			.max_recv_sge = 1,
		},
		.qp_type = IBV_QPT_RAW_PACKET,
	};

	qp = ibv_create_qp(priv->pd, &attr);
	if (!qp)
		rte_errno = errno ? errno : EINVAL;
	return qp;
}

/**
 * Configure a RX queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param rxq
 *   Pointer to RX queue structure.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 * @param[in] conf
 *   Thresholds parameters.
 * @param mp
 *   Memory pool for buffer allocations.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
rxq_setup(struct rte_eth_dev *dev, struct rxq *rxq, uint16_t desc,
	  unsigned int socket, const struct rte_eth_rxconf *conf,
	  struct rte_mempool *mp)
{
	struct priv *priv = dev->data->dev_private;
	struct rxq tmpl = {
		.priv = priv,
		.mp = mp,
		.socket = socket
	};
	struct ibv_qp_attr mod;
	struct ibv_recv_wr *bad_wr;
	unsigned int mb_len;
	int ret;

	(void)conf; /* Thresholds configuration (ignored). */
	mb_len = rte_pktmbuf_data_room_size(mp);
	if (desc == 0) {
		rte_errno = EINVAL;
		ERROR("%p: invalid number of Rx descriptors", (void *)dev);
		goto error;
	}
	/* Enable scattered packets support for this queue if necessary. */
	assert(mb_len >= RTE_PKTMBUF_HEADROOM);
	if (dev->data->dev_conf.rxmode.max_rx_pkt_len <=
	    (mb_len - RTE_PKTMBUF_HEADROOM)) {
		;
	} else if (dev->data->dev_conf.rxmode.enable_scatter) {
		WARN("%p: scattered mode has been requested but is"
		     " not supported, this may lead to packet loss",
		     (void *)dev);
	} else {
		WARN("%p: the requested maximum Rx packet size (%u) is"
		     " larger than a single mbuf (%u) and scattered"
		     " mode has not been requested",
		     (void *)dev,
		     dev->data->dev_conf.rxmode.max_rx_pkt_len,
		     mb_len - RTE_PKTMBUF_HEADROOM);
	}
	/* Use the entire RX mempool as the memory region. */
	tmpl.mr = mlx4_mp2mr(priv->pd, mp);
	if (tmpl.mr == NULL) {
		rte_errno = EINVAL;
		ERROR("%p: MR creation failure: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	if (dev->data->dev_conf.intr_conf.rxq) {
		tmpl.channel = ibv_create_comp_channel(priv->ctx);
		if (tmpl.channel == NULL) {
			rte_errno = ENOMEM;
			ERROR("%p: Rx interrupt completion channel creation"
			      " failure: %s",
			      (void *)dev, strerror(rte_errno));
			goto error;
		}
		if (mlx4_fd_set_non_blocking(tmpl.channel->fd) < 0) {
			ERROR("%p: unable to make Rx interrupt completion"
			      " channel non-blocking: %s",
			      (void *)dev, strerror(rte_errno));
			goto error;
		}
	}
	tmpl.cq = ibv_create_cq(priv->ctx, desc, NULL, tmpl.channel, 0);
	if (tmpl.cq == NULL) {
		rte_errno = ENOMEM;
		ERROR("%p: CQ creation failure: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	DEBUG("priv->device_attr.max_qp_wr is %d",
	      priv->device_attr.max_qp_wr);
	DEBUG("priv->device_attr.max_sge is %d",
	      priv->device_attr.max_sge);
	tmpl.qp = rxq_setup_qp(priv, tmpl.cq, desc);
	if (tmpl.qp == NULL) {
		ERROR("%p: QP creation failure: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	mod = (struct ibv_qp_attr){
		/* Move the QP to this state. */
		.qp_state = IBV_QPS_INIT,
		/* Primary port number. */
		.port_num = priv->port
	};
	ret = ibv_modify_qp(tmpl.qp, &mod, IBV_QP_STATE | IBV_QP_PORT);
	if (ret) {
		rte_errno = ret;
		ERROR("%p: QP state to IBV_QPS_INIT failed: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	ret = rxq_alloc_elts(&tmpl, desc);
	if (ret) {
		ERROR("%p: RXQ allocation failed: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	ret = ibv_post_recv(tmpl.qp, &(*tmpl.elts)[0].wr, &bad_wr);
	if (ret) {
		rte_errno = ret;
		ERROR("%p: ibv_post_recv() failed for WR %p: %s",
		      (void *)dev,
		      (void *)bad_wr,
		      strerror(rte_errno));
		goto error;
	}
	mod = (struct ibv_qp_attr){
		.qp_state = IBV_QPS_RTR
	};
	ret = ibv_modify_qp(tmpl.qp, &mod, IBV_QP_STATE);
	if (ret) {
		rte_errno = ret;
		ERROR("%p: QP state to IBV_QPS_RTR failed: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	/* Save port ID. */
	tmpl.port_id = dev->data->port_id;
	DEBUG("%p: RTE port ID: %u", (void *)rxq, tmpl.port_id);
	/* Clean up rxq in case we're reinitializing it. */
	DEBUG("%p: cleaning-up old rxq just in case", (void *)rxq);
	rxq_cleanup(rxq);
	*rxq = tmpl;
	DEBUG("%p: rxq updated with %p", (void *)rxq, (void *)&tmpl);
	return 0;
error:
	ret = rte_errno;
	rxq_cleanup(&tmpl);
	rte_errno = ret;
	assert(rte_errno > 0);
	return -rte_errno;
}

/**
 * DPDK callback to configure a RX queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   RX queue index.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 * @param[in] conf
 *   Thresholds parameters.
 * @param mp
 *   Memory pool for buffer allocations.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_rx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
		    unsigned int socket, const struct rte_eth_rxconf *conf,
		    struct rte_mempool *mp)
{
	struct priv *priv = dev->data->dev_private;
	struct rxq *rxq = (*priv->rxqs)[idx];
	int ret;

	DEBUG("%p: configuring queue %u for %u descriptors",
	      (void *)dev, idx, desc);
	if (idx >= priv->rxqs_n) {
		rte_errno = EOVERFLOW;
		ERROR("%p: queue index out of range (%u >= %u)",
		      (void *)dev, idx, priv->rxqs_n);
		return -rte_errno;
	}
	if (rxq != NULL) {
		DEBUG("%p: reusing already allocated queue index %u (%p)",
		      (void *)dev, idx, (void *)rxq);
		if (priv->started) {
			rte_errno = EEXIST;
			return -rte_errno;
		}
		(*priv->rxqs)[idx] = NULL;
		if (idx == 0)
			priv_mac_addr_del(priv);
		rxq_cleanup(rxq);
	} else {
		rxq = rte_calloc_socket("RXQ", 1, sizeof(*rxq), 0, socket);
		if (rxq == NULL) {
			rte_errno = ENOMEM;
			ERROR("%p: unable to allocate queue index %u",
			      (void *)dev, idx);
			return -rte_errno;
		}
	}
	ret = rxq_setup(dev, rxq, desc, socket, conf, mp);
	if (ret)
		rte_free(rxq);
	else {
		rxq->stats.idx = idx;
		DEBUG("%p: adding RX queue %p to list",
		      (void *)dev, (void *)rxq);
		(*priv->rxqs)[idx] = rxq;
		/* Update receive callback. */
		dev->rx_pkt_burst = mlx4_rx_burst;
	}
	return ret;
}

/**
 * DPDK callback to release a RX queue.
 *
 * @param dpdk_rxq
 *   Generic RX queue pointer.
 */
static void
mlx4_rx_queue_release(void *dpdk_rxq)
{
	struct rxq *rxq = (struct rxq *)dpdk_rxq;
	struct priv *priv;
	unsigned int i;

	if (rxq == NULL)
		return;
	priv = rxq->priv;
	for (i = 0; (i != priv->rxqs_n); ++i)
		if ((*priv->rxqs)[i] == rxq) {
			DEBUG("%p: removing RX queue %p from list",
			      (void *)priv->dev, (void *)rxq);
			(*priv->rxqs)[i] = NULL;
			if (i == 0)
				priv_mac_addr_del(priv);
			break;
		}
	rxq_cleanup(rxq);
	rte_free(rxq);
}

/**
 * DPDK callback to start the device.
 *
 * Simulate device start by attaching all configured flows.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_dev_start(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	int ret;

	if (priv->started)
		return 0;
	DEBUG("%p: attaching configured flows to all RX queues", (void *)dev);
	priv->started = 1;
	ret = priv_mac_addr_add(priv);
	if (ret)
		goto err;
	ret = mlx4_intr_install(priv);
	if (ret) {
		ERROR("%p: interrupt handler installation failed",
		     (void *)dev);
		goto err;
	}
	ret = mlx4_priv_flow_start(priv);
	if (ret) {
		ERROR("%p: flow start failed: %s",
		      (void *)dev, strerror(ret));
		goto err;
	}
	return 0;
err:
	/* Rollback. */
	priv_mac_addr_del(priv);
	priv->started = 0;
	return ret;
}

/**
 * DPDK callback to stop the device.
 *
 * Simulate device stop by detaching all configured flows.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mlx4_dev_stop(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;

	if (!priv->started)
		return;
	DEBUG("%p: detaching flows from all RX queues", (void *)dev);
	priv->started = 0;
	mlx4_priv_flow_stop(priv);
	mlx4_intr_uninstall(priv);
	priv_mac_addr_del(priv);
}

/**
 * DPDK callback to close the device.
 *
 * Destroy all queues and objects, free memory.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mlx4_dev_close(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	void *tmp;
	unsigned int i;

	if (priv == NULL)
		return;
	DEBUG("%p: closing device \"%s\"",
	      (void *)dev,
	      ((priv->ctx != NULL) ? priv->ctx->device->name : ""));
	priv_mac_addr_del(priv);
	/*
	 * Prevent crashes when queues are still in use. This is unfortunately
	 * still required for DPDK 1.3 because some programs (such as testpmd)
	 * never release them before closing the device.
	 */
	dev->rx_pkt_burst = mlx4_rx_burst_removed;
	dev->tx_pkt_burst = mlx4_tx_burst_removed;
	if (priv->rxqs != NULL) {
		/* XXX race condition if mlx4_rx_burst() is still running. */
		usleep(1000);
		for (i = 0; (i != priv->rxqs_n); ++i) {
			tmp = (*priv->rxqs)[i];
			if (tmp == NULL)
				continue;
			(*priv->rxqs)[i] = NULL;
			rxq_cleanup(tmp);
			rte_free(tmp);
		}
		priv->rxqs_n = 0;
		priv->rxqs = NULL;
	}
	if (priv->txqs != NULL) {
		/* XXX race condition if mlx4_tx_burst() is still running. */
		usleep(1000);
		for (i = 0; (i != priv->txqs_n); ++i) {
			tmp = (*priv->txqs)[i];
			if (tmp == NULL)
				continue;
			(*priv->txqs)[i] = NULL;
			mlx4_txq_cleanup(tmp);
			rte_free(tmp);
		}
		priv->txqs_n = 0;
		priv->txqs = NULL;
	}
	if (priv->pd != NULL) {
		assert(priv->ctx != NULL);
		claim_zero(ibv_dealloc_pd(priv->pd));
		claim_zero(ibv_close_device(priv->ctx));
	} else
		assert(priv->ctx == NULL);
	mlx4_intr_uninstall(priv);
	memset(priv, 0, sizeof(*priv));
}

const struct rte_flow_ops mlx4_flow_ops = {
	.validate = mlx4_flow_validate,
	.create = mlx4_flow_create,
	.destroy = mlx4_flow_destroy,
	.flush = mlx4_flow_flush,
	.query = NULL,
	.isolate = mlx4_flow_isolate,
};

/**
 * Manage filter operations.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param filter_type
 *   Filter type.
 * @param filter_op
 *   Operation to perform.
 * @param arg
 *   Pointer to operation-specific structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_dev_filter_ctrl(struct rte_eth_dev *dev,
		     enum rte_filter_type filter_type,
		     enum rte_filter_op filter_op,
		     void *arg)
{
	switch (filter_type) {
	case RTE_ETH_FILTER_GENERIC:
		if (filter_op != RTE_ETH_FILTER_GET)
			break;
		*(const void **)arg = &mlx4_flow_ops;
		return 0;
	default:
		ERROR("%p: filter type (%d) not supported",
		      (void *)dev, filter_type);
		break;
	}
	rte_errno = ENOTSUP;
	return -rte_errno;
}

static const struct eth_dev_ops mlx4_dev_ops = {
	.dev_configure = mlx4_dev_configure,
	.dev_start = mlx4_dev_start,
	.dev_stop = mlx4_dev_stop,
	.dev_set_link_down = mlx4_dev_set_link_down,
	.dev_set_link_up = mlx4_dev_set_link_up,
	.dev_close = mlx4_dev_close,
	.link_update = mlx4_link_update,
	.stats_get = mlx4_stats_get,
	.stats_reset = mlx4_stats_reset,
	.dev_infos_get = mlx4_dev_infos_get,
	.rx_queue_setup = mlx4_rx_queue_setup,
	.tx_queue_setup = mlx4_tx_queue_setup,
	.rx_queue_release = mlx4_rx_queue_release,
	.tx_queue_release = mlx4_tx_queue_release,
	.flow_ctrl_get = mlx4_flow_ctrl_get,
	.flow_ctrl_set = mlx4_flow_ctrl_set,
	.mtu_set = mlx4_mtu_set,
	.filter_ctrl = mlx4_dev_filter_ctrl,
	.rx_queue_intr_enable = mlx4_rx_intr_enable,
	.rx_queue_intr_disable = mlx4_rx_intr_disable,
};

/**
 * Get PCI information from struct ibv_device.
 *
 * @param device
 *   Pointer to Ethernet device structure.
 * @param[out] pci_addr
 *   PCI bus address output buffer.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_ibv_device_to_pci_addr(const struct ibv_device *device,
			    struct rte_pci_addr *pci_addr)
{
	FILE *file;
	char line[32];
	MKSTR(path, "%s/device/uevent", device->ibdev_path);

	file = fopen(path, "rb");
	if (file == NULL) {
		rte_errno = errno;
		return -rte_errno;
	}
	while (fgets(line, sizeof(line), file) == line) {
		size_t len = strlen(line);
		int ret;

		/* Truncate long lines. */
		if (len == (sizeof(line) - 1))
			while (line[(len - 1)] != '\n') {
				ret = fgetc(file);
				if (ret == EOF)
					break;
				line[(len - 1)] = ret;
			}
		/* Extract information. */
		if (sscanf(line,
			   "PCI_SLOT_NAME="
			   "%" SCNx32 ":%" SCNx8 ":%" SCNx8 ".%" SCNx8 "\n",
			   &pci_addr->domain,
			   &pci_addr->bus,
			   &pci_addr->devid,
			   &pci_addr->function) == 4) {
			ret = 0;
			break;
		}
	}
	fclose(file);
	return 0;
}

/**
 * Verify and store value for device argument.
 *
 * @param[in] key
 *   Key argument to verify.
 * @param[in] val
 *   Value associated with key.
 * @param[in, out] conf
 *   Shared configuration data.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_arg_parse(const char *key, const char *val, struct mlx4_conf *conf)
{
	unsigned long tmp;

	errno = 0;
	tmp = strtoul(val, NULL, 0);
	if (errno) {
		rte_errno = errno;
		WARN("%s: \"%s\" is not a valid integer", key, val);
		return -rte_errno;
	}
	if (strcmp(MLX4_PMD_PORT_KVARG, key) == 0) {
		uint32_t ports = rte_log2_u32(conf->ports.present);

		if (tmp >= ports) {
			ERROR("port index %lu outside range [0,%" PRIu32 ")",
			      tmp, ports);
			return -EINVAL;
		}
		if (!(conf->ports.present & (1 << tmp))) {
			rte_errno = EINVAL;
			ERROR("invalid port index %lu", tmp);
			return -rte_errno;
		}
		conf->ports.enabled |= 1 << tmp;
	} else {
		rte_errno = EINVAL;
		WARN("%s: unknown parameter", key);
		return -rte_errno;
	}
	return 0;
}

/**
 * Parse device parameters.
 *
 * @param devargs
 *   Device arguments structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_args(struct rte_devargs *devargs, struct mlx4_conf *conf)
{
	struct rte_kvargs *kvlist;
	unsigned int arg_count;
	int ret = 0;
	int i;

	if (devargs == NULL)
		return 0;
	kvlist = rte_kvargs_parse(devargs->args, pmd_mlx4_init_params);
	if (kvlist == NULL) {
		rte_errno = EINVAL;
		ERROR("failed to parse kvargs");
		return -rte_errno;
	}
	/* Process parameters. */
	for (i = 0; pmd_mlx4_init_params[i]; ++i) {
		arg_count = rte_kvargs_count(kvlist, MLX4_PMD_PORT_KVARG);
		while (arg_count-- > 0) {
			ret = rte_kvargs_process(kvlist,
						 MLX4_PMD_PORT_KVARG,
						 (int (*)(const char *,
							  const char *,
							  void *))
						 mlx4_arg_parse,
						 conf);
			if (ret != 0)
				goto free_kvlist;
		}
	}
free_kvlist:
	rte_kvargs_free(kvlist);
	return ret;
}

static struct rte_pci_driver mlx4_driver;

/**
 * DPDK callback to register a PCI device.
 *
 * This function creates an Ethernet device for each port of a given
 * PCI device.
 *
 * @param[in] pci_drv
 *   PCI driver structure (mlx4_driver).
 * @param[in] pci_dev
 *   PCI device information.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_pci_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	struct ibv_device **list;
	struct ibv_device *ibv_dev;
	int err = 0;
	struct ibv_context *attr_ctx = NULL;
	struct ibv_device_attr device_attr;
	struct mlx4_conf conf = {
		.ports.present = 0,
	};
	unsigned int vf;
	int i;

	(void)pci_drv;
	assert(pci_drv == &mlx4_driver);
	list = ibv_get_device_list(&i);
	if (list == NULL) {
		rte_errno = errno;
		assert(rte_errno);
		if (rte_errno == ENOSYS)
			ERROR("cannot list devices, is ib_uverbs loaded?");
		return -rte_errno;
	}
	assert(i >= 0);
	/*
	 * For each listed device, check related sysfs entry against
	 * the provided PCI ID.
	 */
	while (i != 0) {
		struct rte_pci_addr pci_addr;

		--i;
		DEBUG("checking device \"%s\"", list[i]->name);
		if (mlx4_ibv_device_to_pci_addr(list[i], &pci_addr))
			continue;
		if ((pci_dev->addr.domain != pci_addr.domain) ||
		    (pci_dev->addr.bus != pci_addr.bus) ||
		    (pci_dev->addr.devid != pci_addr.devid) ||
		    (pci_dev->addr.function != pci_addr.function))
			continue;
		vf = (pci_dev->id.device_id ==
		      PCI_DEVICE_ID_MELLANOX_CONNECTX3VF);
		INFO("PCI information matches, using device \"%s\" (VF: %s)",
		     list[i]->name, (vf ? "true" : "false"));
		attr_ctx = ibv_open_device(list[i]);
		err = errno;
		break;
	}
	if (attr_ctx == NULL) {
		ibv_free_device_list(list);
		switch (err) {
		case 0:
			rte_errno = ENODEV;
			ERROR("cannot access device, is mlx4_ib loaded?");
			return -rte_errno;
		case EINVAL:
			rte_errno = EINVAL;
			ERROR("cannot use device, are drivers up to date?");
			return -rte_errno;
		}
		assert(err > 0);
		rte_errno = err;
		return -rte_errno;
	}
	ibv_dev = list[i];
	DEBUG("device opened");
	if (ibv_query_device(attr_ctx, &device_attr)) {
		rte_errno = ENODEV;
		goto error;
	}
	INFO("%u port(s) detected", device_attr.phys_port_cnt);
	conf.ports.present |= (UINT64_C(1) << device_attr.phys_port_cnt) - 1;
	if (mlx4_args(pci_dev->device.devargs, &conf)) {
		ERROR("failed to process device arguments");
		rte_errno = EINVAL;
		goto error;
	}
	/* Use all ports when none are defined */
	if (!conf.ports.enabled)
		conf.ports.enabled = conf.ports.present;
	for (i = 0; i < device_attr.phys_port_cnt; i++) {
		uint32_t port = i + 1; /* ports are indexed from one */
		struct ibv_context *ctx = NULL;
		struct ibv_port_attr port_attr;
		struct ibv_pd *pd = NULL;
		struct priv *priv = NULL;
		struct rte_eth_dev *eth_dev = NULL;
		struct ether_addr mac;

		/* If port is not enabled, skip. */
		if (!(conf.ports.enabled & (1 << i)))
			continue;
		DEBUG("using port %u", port);
		ctx = ibv_open_device(ibv_dev);
		if (ctx == NULL) {
			rte_errno = ENODEV;
			goto port_error;
		}
		/* Check port status. */
		err = ibv_query_port(ctx, port, &port_attr);
		if (err) {
			rte_errno = err;
			ERROR("port query failed: %s", strerror(rte_errno));
			goto port_error;
		}
		if (port_attr.link_layer != IBV_LINK_LAYER_ETHERNET) {
			rte_errno = ENOTSUP;
			ERROR("port %d is not configured in Ethernet mode",
			      port);
			goto port_error;
		}
		if (port_attr.state != IBV_PORT_ACTIVE)
			DEBUG("port %d is not active: \"%s\" (%d)",
			      port, ibv_port_state_str(port_attr.state),
			      port_attr.state);
		/* Make asynchronous FD non-blocking to handle interrupts. */
		if (mlx4_fd_set_non_blocking(ctx->async_fd) < 0) {
			ERROR("cannot make asynchronous FD non-blocking: %s",
			      strerror(rte_errno));
			goto port_error;
		}
		/* Allocate protection domain. */
		pd = ibv_alloc_pd(ctx);
		if (pd == NULL) {
			rte_errno = ENOMEM;
			ERROR("PD allocation failure");
			goto port_error;
		}
		/* from rte_ethdev.c */
		priv = rte_zmalloc("ethdev private structure",
				   sizeof(*priv),
				   RTE_CACHE_LINE_SIZE);
		if (priv == NULL) {
			rte_errno = ENOMEM;
			ERROR("priv allocation failure");
			goto port_error;
		}
		priv->ctx = ctx;
		priv->device_attr = device_attr;
		priv->port = port;
		priv->pd = pd;
		priv->mtu = ETHER_MTU;
		priv->vf = vf;
		/* Configure the first MAC address by default. */
		if (mlx4_get_mac(priv, &mac.addr_bytes)) {
			ERROR("cannot get MAC address, is mlx4_en loaded?"
			      " (rte_errno: %s)", strerror(rte_errno));
			goto port_error;
		}
		INFO("port %u MAC address is %02x:%02x:%02x:%02x:%02x:%02x",
		     priv->port,
		     mac.addr_bytes[0], mac.addr_bytes[1],
		     mac.addr_bytes[2], mac.addr_bytes[3],
		     mac.addr_bytes[4], mac.addr_bytes[5]);
		/* Register MAC address. */
		priv->mac = mac;
		if (priv_mac_addr_add(priv))
			goto port_error;
#ifndef NDEBUG
		{
			char ifname[IF_NAMESIZE];

			if (mlx4_get_ifname(priv, &ifname) == 0)
				DEBUG("port %u ifname is \"%s\"",
				      priv->port, ifname);
			else
				DEBUG("port %u ifname is unknown", priv->port);
		}
#endif
		/* Get actual MTU if possible. */
		mlx4_mtu_get(priv, &priv->mtu);
		DEBUG("port %u MTU is %u", priv->port, priv->mtu);
		/* from rte_ethdev.c */
		{
			char name[RTE_ETH_NAME_MAX_LEN];

			snprintf(name, sizeof(name), "%s port %u",
				 ibv_get_device_name(ibv_dev), port);
			eth_dev = rte_eth_dev_allocate(name);
		}
		if (eth_dev == NULL) {
			ERROR("can not allocate rte ethdev");
			rte_errno = ENOMEM;
			goto port_error;
		}
		eth_dev->data->dev_private = priv;
		eth_dev->data->mac_addrs = &priv->mac;
		eth_dev->device = &pci_dev->device;
		rte_eth_copy_pci_info(eth_dev, pci_dev);
		eth_dev->device->driver = &mlx4_driver.driver;
		/* Initialize local interrupt handle for current port. */
		priv->intr_handle = (struct rte_intr_handle){
			.fd = -1,
			.type = RTE_INTR_HANDLE_EXT,
		};
		/*
		 * Override ethdev interrupt handle pointer with private
		 * handle instead of that of the parent PCI device used by
		 * default. This prevents it from being shared between all
		 * ports of the same PCI device since each of them is
		 * associated its own Verbs context.
		 *
		 * Rx interrupts in particular require this as the PMD has
		 * no control over the registration of queue interrupts
		 * besides setting up eth_dev->intr_handle, the rest is
		 * handled by rte_intr_rx_ctl().
		 */
		eth_dev->intr_handle = &priv->intr_handle;
		priv->dev = eth_dev;
		eth_dev->dev_ops = &mlx4_dev_ops;
		eth_dev->data->dev_flags |= RTE_ETH_DEV_DETACHABLE;
		/* Bring Ethernet device up. */
		DEBUG("forcing Ethernet interface up");
		mlx4_dev_set_link_up(priv->dev);
		/* Update link status once if waiting for LSC. */
		if (eth_dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC)
			mlx4_link_update(eth_dev, 0);
		continue;
port_error:
		rte_free(priv);
		if (pd)
			claim_zero(ibv_dealloc_pd(pd));
		if (ctx)
			claim_zero(ibv_close_device(ctx));
		if (eth_dev)
			rte_eth_dev_release_port(eth_dev);
		break;
	}
	if (i == device_attr.phys_port_cnt)
		return 0;
	/*
	 * XXX if something went wrong in the loop above, there is a resource
	 * leak (ctx, pd, priv, dpdk ethdev) but we can do nothing about it as
	 * long as the dpdk does not provide a way to deallocate a ethdev and a
	 * way to enumerate the registered ethdevs to free the previous ones.
	 */
error:
	if (attr_ctx)
		claim_zero(ibv_close_device(attr_ctx));
	if (list)
		ibv_free_device_list(list);
	assert(rte_errno >= 0);
	return -rte_errno;
}

static const struct rte_pci_id mlx4_pci_id_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX3)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX3PRO)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX3VF)
	},
	{
		.vendor_id = 0
	}
};

static struct rte_pci_driver mlx4_driver = {
	.driver = {
		.name = MLX4_DRIVER_NAME
	},
	.id_table = mlx4_pci_id_map,
	.probe = mlx4_pci_probe,
	.drv_flags = RTE_PCI_DRV_INTR_LSC |
		     RTE_PCI_DRV_INTR_RMV,
};

/**
 * Driver initialization routine.
 */
RTE_INIT(rte_mlx4_pmd_init);
static void
rte_mlx4_pmd_init(void)
{
	/*
	 * RDMAV_HUGEPAGES_SAFE tells ibv_fork_init() we intend to use
	 * huge pages. Calling ibv_fork_init() during init allows
	 * applications to use fork() safely for purposes other than
	 * using this PMD, which is not supported in forked processes.
	 */
	setenv("RDMAV_HUGEPAGES_SAFE", "1", 1);
	ibv_fork_init();
	rte_pci_register(&mlx4_driver);
}

RTE_PMD_EXPORT_NAME(net_mlx4, __COUNTER__);
RTE_PMD_REGISTER_PCI_TABLE(net_mlx4, mlx4_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(net_mlx4,
	"* ib_uverbs & mlx4_en & mlx4_core & mlx4_ib");
