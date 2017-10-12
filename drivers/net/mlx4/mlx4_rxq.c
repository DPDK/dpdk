/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox
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

/**
 * @file
 * Rx queues configuration for mlx4 driver.
 */

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_common.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_flow.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>

#include "mlx4.h"
#include "mlx4_flow.h"
#include "mlx4_rxtx.h"
#include "mlx4_utils.h"

/**
 * Allocate Rx queue elements.
 *
 * @param rxq
 *   Pointer to Rx queue structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_rxq_alloc_elts(struct rxq *rxq)
{
	struct rxq_elt (*elts)[rxq->elts_n] = rxq->elts;
	unsigned int i;

	/* For each WR (packet). */
	for (i = 0; i != RTE_DIM(*elts); ++i) {
		struct rxq_elt *elt = &(*elts)[i];
		struct ibv_recv_wr *wr = &elt->wr;
		struct ibv_sge *sge = &(*elts)[i].sge;
		struct rte_mbuf *buf = rte_pktmbuf_alloc(rxq->mp);

		if (buf == NULL) {
			while (i--) {
				rte_pktmbuf_free_seg((*elts)[i].buf);
				(*elts)[i].buf = NULL;
			}
			rte_errno = ENOMEM;
			return -rte_errno;
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
	return 0;
}

/**
 * Free Rx queue elements.
 *
 * @param rxq
 *   Pointer to Rx queue structure.
 */
static void
mlx4_rxq_free_elts(struct rxq *rxq)
{
	unsigned int i;
	struct rxq_elt (*elts)[rxq->elts_n] = rxq->elts;

	DEBUG("%p: freeing WRs", (void *)rxq);
	for (i = 0; (i != RTE_DIM(*elts)); ++i) {
		if (!(*elts)[i].buf)
			continue;
		rte_pktmbuf_free_seg((*elts)[i].buf);
		(*elts)[i].buf = NULL;
	}
}

/**
 * DPDK callback to configure a Rx queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   Rx queue index.
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
int
mlx4_rx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
		    unsigned int socket, const struct rte_eth_rxconf *conf,
		    struct rte_mempool *mp)
{
	struct priv *priv = dev->data->dev_private;
	uint32_t mb_len = rte_pktmbuf_data_room_size(mp);
	struct rxq_elt (*elts)[desc];
	struct rte_flow_error error;
	struct rxq *rxq;
	struct mlx4_malloc_vec vec[] = {
		{
			.align = RTE_CACHE_LINE_SIZE,
			.size = sizeof(*rxq),
			.addr = (void **)&rxq,
		},
		{
			.align = RTE_CACHE_LINE_SIZE,
			.size = sizeof(*elts),
			.addr = (void **)&elts,
		},
	};
	int ret;

	(void)conf; /* Thresholds configuration (ignored). */
	DEBUG("%p: configuring queue %u for %u descriptors",
	      (void *)dev, idx, desc);
	if (idx >= dev->data->nb_rx_queues) {
		rte_errno = EOVERFLOW;
		ERROR("%p: queue index out of range (%u >= %u)",
		      (void *)dev, idx, dev->data->nb_rx_queues);
		return -rte_errno;
	}
	rxq = dev->data->rx_queues[idx];
	if (rxq) {
		rte_errno = EEXIST;
		ERROR("%p: Rx queue %u already configured, release it first",
		      (void *)dev, idx);
		return -rte_errno;
	}
	if (!desc) {
		rte_errno = EINVAL;
		ERROR("%p: invalid number of Rx descriptors", (void *)dev);
		return -rte_errno;
	}
	/* Allocate and initialize Rx queue. */
	mlx4_zmallocv_socket("RXQ", vec, RTE_DIM(vec), socket);
	if (!rxq) {
		ERROR("%p: unable to allocate queue index %u",
		      (void *)dev, idx);
		return -rte_errno;
	}
	*rxq = (struct rxq){
		.priv = priv,
		.mp = mp,
		.port_id = dev->data->port_id,
		.elts_n = desc,
		.elts_head = 0,
		.elts = elts,
		.stats.idx = idx,
		.socket = socket,
	};
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
	/* Use the entire Rx mempool as the memory region. */
	rxq->mr = mlx4_mp2mr(priv->pd, mp);
	if (!rxq->mr) {
		rte_errno = EINVAL;
		ERROR("%p: MR creation failure: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	if (dev->data->dev_conf.intr_conf.rxq) {
		rxq->channel = ibv_create_comp_channel(priv->ctx);
		if (rxq->channel == NULL) {
			rte_errno = ENOMEM;
			ERROR("%p: Rx interrupt completion channel creation"
			      " failure: %s",
			      (void *)dev, strerror(rte_errno));
			goto error;
		}
		if (mlx4_fd_set_non_blocking(rxq->channel->fd) < 0) {
			ERROR("%p: unable to make Rx interrupt completion"
			      " channel non-blocking: %s",
			      (void *)dev, strerror(rte_errno));
			goto error;
		}
	}
	rxq->cq = ibv_create_cq(priv->ctx, desc, NULL, rxq->channel, 0);
	if (!rxq->cq) {
		rte_errno = ENOMEM;
		ERROR("%p: CQ creation failure: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	rxq->qp = ibv_create_qp
		(priv->pd,
		 &(struct ibv_qp_init_attr){
			.send_cq = rxq->cq,
			.recv_cq = rxq->cq,
			.cap = {
				.max_recv_wr =
					RTE_MIN(priv->device_attr.max_qp_wr,
						desc),
				.max_recv_sge = 1,
			},
			.qp_type = IBV_QPT_RAW_PACKET,
		 });
	if (!rxq->qp) {
		rte_errno = errno ? errno : EINVAL;
		ERROR("%p: QP creation failure: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	ret = ibv_modify_qp
		(rxq->qp,
		 &(struct ibv_qp_attr){
			.qp_state = IBV_QPS_INIT,
			.port_num = priv->port,
		 },
		 IBV_QP_STATE | IBV_QP_PORT);
	if (ret) {
		rte_errno = ret;
		ERROR("%p: QP state to IBV_QPS_INIT failed: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	ret = mlx4_rxq_alloc_elts(rxq);
	if (ret) {
		ERROR("%p: RXQ allocation failed: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	ret = ibv_post_recv(rxq->qp, &(*rxq->elts)[0].wr,
			    &(struct ibv_recv_wr *){ NULL });
	if (ret) {
		rte_errno = ret;
		ERROR("%p: ibv_post_recv() failed: %s",
		      (void *)dev,
		      strerror(rte_errno));
		goto error;
	}
	ret = ibv_modify_qp
		(rxq->qp,
		 &(struct ibv_qp_attr){
			.qp_state = IBV_QPS_RTR,
		 },
		 IBV_QP_STATE);
	if (ret) {
		rte_errno = ret;
		ERROR("%p: QP state to IBV_QPS_RTR failed: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	DEBUG("%p: adding Rx queue %p to list", (void *)dev, (void *)rxq);
	dev->data->rx_queues[idx] = rxq;
	/* Enable associated flows. */
	ret = mlx4_flow_sync(priv, &error);
	if (!ret)
		return 0;
	ERROR("cannot re-attach flow rules to queue %u"
	      " (code %d, \"%s\"), flow error type %d, cause %p, message: %s",
	      idx, -ret, strerror(-ret), error.type, error.cause,
	      error.message ? error.message : "(unspecified)");
error:
	dev->data->rx_queues[idx] = NULL;
	ret = rte_errno;
	mlx4_rx_queue_release(rxq);
	rte_errno = ret;
	assert(rte_errno > 0);
	return -rte_errno;
}

/**
 * DPDK callback to release a Rx queue.
 *
 * @param dpdk_rxq
 *   Generic Rx queue pointer.
 */
void
mlx4_rx_queue_release(void *dpdk_rxq)
{
	struct rxq *rxq = (struct rxq *)dpdk_rxq;
	struct priv *priv;
	unsigned int i;

	if (rxq == NULL)
		return;
	priv = rxq->priv;
	for (i = 0; i != priv->dev->data->nb_rx_queues; ++i)
		if (priv->dev->data->rx_queues[i] == rxq) {
			DEBUG("%p: removing Rx queue %p from list",
			      (void *)priv->dev, (void *)rxq);
			priv->dev->data->rx_queues[i] = NULL;
			break;
		}
	mlx4_flow_sync(priv, NULL);
	mlx4_rxq_free_elts(rxq);
	if (rxq->qp)
		claim_zero(ibv_destroy_qp(rxq->qp));
	if (rxq->cq)
		claim_zero(ibv_destroy_cq(rxq->cq));
	if (rxq->channel)
		claim_zero(ibv_destroy_comp_channel(rxq->channel));
	if (rxq->mr)
		claim_zero(ibv_dereg_mr(rxq->mr));
	rte_free(rxq);
}
