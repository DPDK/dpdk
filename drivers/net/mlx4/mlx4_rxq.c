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

#include <rte_byteorder.h>
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
 * Historical RSS hash key.
 *
 * This used to be the default for mlx4 in Linux before v3.19 switched to
 * generating random hash keys through netdev_rss_key_fill().
 *
 * It is used in this PMD for consistency with past DPDK releases but can
 * now be overridden through user configuration.
 *
 * Note: this is not const to work around API quirks.
 */
uint8_t
mlx4_rss_hash_key_default[MLX4_RSS_HASH_KEY_SIZE] = {
	0x2c, 0xc6, 0x81, 0xd1,
	0x5b, 0xdb, 0xf4, 0xf7,
	0xfc, 0xa2, 0x83, 0x19,
	0xdb, 0x1a, 0x3e, 0x94,
	0x6b, 0x9e, 0x38, 0xd9,
	0x2c, 0x9c, 0x03, 0xd1,
	0xad, 0x99, 0x44, 0xa7,
	0xd9, 0x56, 0x3d, 0x59,
	0x06, 0x3c, 0x25, 0xf3,
	0xfc, 0x1f, 0xdc, 0x2a,
};

/**
 * Obtain a RSS context with specified properties.
 *
 * Used when creating a flow rule targeting one or several Rx queues.
 *
 * If a matching RSS context already exists, it is returned with its
 * reference count incremented.
 *
 * @param priv
 *   Pointer to private structure.
 * @param fields
 *   Fields for RSS processing (Verbs format).
 * @param[in] key
 *   Hash key to use (whose size is exactly MLX4_RSS_HASH_KEY_SIZE).
 * @param queues
 *   Number of target queues.
 * @param[in] queue_id
 *   Target queues.
 *
 * @return
 *   Pointer to RSS context on success, NULL otherwise and rte_errno is set.
 */
struct mlx4_rss *
mlx4_rss_get(struct priv *priv, uint64_t fields,
	     uint8_t key[MLX4_RSS_HASH_KEY_SIZE],
	     uint16_t queues, const uint16_t queue_id[])
{
	struct mlx4_rss *rss;
	size_t queue_id_size = sizeof(queue_id[0]) * queues;

	LIST_FOREACH(rss, &priv->rss, next)
		if (fields == rss->fields &&
		    queues == rss->queues &&
		    !memcmp(key, rss->key, MLX4_RSS_HASH_KEY_SIZE) &&
		    !memcmp(queue_id, rss->queue_id, queue_id_size)) {
			++rss->refcnt;
			return rss;
		}
	rss = rte_malloc(__func__, offsetof(struct mlx4_rss, queue_id) +
			 queue_id_size, 0);
	if (!rss)
		goto error;
	*rss = (struct mlx4_rss){
		.priv = priv,
		.refcnt = 1,
		.usecnt = 0,
		.qp = NULL,
		.ind = NULL,
		.fields = fields,
		.queues = queues,
	};
	memcpy(rss->key, key, MLX4_RSS_HASH_KEY_SIZE);
	memcpy(rss->queue_id, queue_id, queue_id_size);
	LIST_INSERT_HEAD(&priv->rss, rss, next);
	return rss;
error:
	rte_errno = ENOMEM;
	return NULL;
}

/**
 * Release a RSS context instance.
 *
 * Used when destroying a flow rule targeting one or several Rx queues.
 *
 * This function decrements the reference count of the context and destroys
 * it after reaching 0. The context must have no users at this point; all
 * prior calls to mlx4_rss_attach() must have been followed by matching
 * calls to mlx4_rss_detach().
 *
 * @param rss
 *   RSS context to release.
 */
void mlx4_rss_put(struct mlx4_rss *rss)
{
	assert(rss->refcnt);
	if (--rss->refcnt)
		return;
	assert(!rss->usecnt);
	assert(!rss->qp);
	assert(!rss->ind);
	LIST_REMOVE(rss, next);
	rte_free(rss);
}

/**
 * Attach a user to a RSS context instance.
 *
 * Used when the RSS QP and indirection table objects must be instantiated,
 * that is, when a flow rule must be enabled.
 *
 * This function increments the usage count of the context.
 *
 * @param rss
 *   RSS context to attach to.
 */
int mlx4_rss_attach(struct mlx4_rss *rss)
{
	assert(rss->refcnt);
	if (rss->usecnt++) {
		assert(rss->qp);
		assert(rss->ind);
		return 0;
	}

	struct ibv_wq *ind_tbl[rss->queues];
	struct priv *priv = rss->priv;
	const char *msg;
	unsigned int i;
	int ret;

	if (!rte_is_power_of_2(RTE_DIM(ind_tbl))) {
		msg = "number of RSS queues must be a power of two";
		goto error;
	}
	for (i = 0; i != RTE_DIM(ind_tbl); ++i) {
		uint16_t id = rss->queue_id[i];
		struct rxq *rxq = NULL;

		if (id < priv->dev->data->nb_rx_queues)
			rxq = priv->dev->data->rx_queues[id];
		if (!rxq) {
			msg = "RSS target queue is not configured";
			goto error;
		}
		ind_tbl[i] = rxq->wq;
	}
	rss->ind = ibv_create_rwq_ind_table
		(priv->ctx,
		 &(struct ibv_rwq_ind_table_init_attr){
			.log_ind_tbl_size = rte_log2_u32(RTE_DIM(ind_tbl)),
			.ind_tbl = ind_tbl,
			.comp_mask = 0,
		 });
	if (!rss->ind) {
		msg = "RSS indirection table creation failure";
		goto error;
	}
	rss->qp = ibv_create_qp_ex
		(priv->ctx,
		 &(struct ibv_qp_init_attr_ex){
			.comp_mask = (IBV_QP_INIT_ATTR_PD |
				      IBV_QP_INIT_ATTR_RX_HASH |
				      IBV_QP_INIT_ATTR_IND_TABLE),
			.qp_type = IBV_QPT_RAW_PACKET,
			.pd = priv->pd,
			.rwq_ind_tbl = rss->ind,
			.rx_hash_conf = {
				.rx_hash_function = IBV_RX_HASH_FUNC_TOEPLITZ,
				.rx_hash_key_len = MLX4_RSS_HASH_KEY_SIZE,
				.rx_hash_key = rss->key,
				.rx_hash_fields_mask = rss->fields,
			},
		 });
	if (!rss->qp) {
		msg = "RSS hash QP creation failure";
		goto error;
	}
	ret = ibv_modify_qp
		(rss->qp,
		 &(struct ibv_qp_attr){
			.qp_state = IBV_QPS_INIT,
			.port_num = priv->port,
		 },
		 IBV_QP_STATE | IBV_QP_PORT);
	if (ret) {
		msg = "failed to switch RSS hash QP to INIT state";
		goto error;
	}
	ret = ibv_modify_qp
		(rss->qp,
		 &(struct ibv_qp_attr){
			.qp_state = IBV_QPS_RTR,
		 },
		 IBV_QP_STATE);
	if (ret) {
		msg = "failed to switch RSS hash QP to RTR state";
		goto error;
	}
	return 0;
error:
	ERROR("mlx4: %s", msg);
	--rss->usecnt;
	rte_errno = EINVAL;
	return -rte_errno;
}

/**
 * Detach a user from a RSS context instance.
 *
 * Used when disabling (not destroying) a flow rule.
 *
 * This function decrements the usage count of the context and destroys
 * usage resources after reaching 0.
 *
 * @param rss
 *   RSS context to detach from.
 */
void mlx4_rss_detach(struct mlx4_rss *rss)
{
	assert(rss->refcnt);
	assert(rss->qp);
	assert(rss->ind);
	if (--rss->usecnt)
		return;
	claim_zero(ibv_destroy_qp(rss->qp));
	rss->qp = NULL;
	claim_zero(ibv_destroy_rwq_ind_table(rss->ind));
	rss->ind = NULL;
}

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
	const uint32_t elts_n = 1 << rxq->elts_n;
	const uint32_t sges_n = 1 << rxq->sges_n;
	struct rte_mbuf *(*elts)[elts_n] = rxq->elts;
	unsigned int i;

	assert(rte_is_power_of_2(elts_n));
	for (i = 0; i != RTE_DIM(*elts); ++i) {
		volatile struct mlx4_wqe_data_seg *scat = &(*rxq->wqes)[i];
		struct rte_mbuf *buf = rte_pktmbuf_alloc(rxq->mp);

		if (buf == NULL) {
			while (i--) {
				rte_pktmbuf_free_seg((*elts)[i]);
				(*elts)[i] = NULL;
			}
			rte_errno = ENOMEM;
			return -rte_errno;
		}
		/* Headroom is reserved by rte_pktmbuf_alloc(). */
		assert(buf->data_off == RTE_PKTMBUF_HEADROOM);
		/* Buffer is supposed to be empty. */
		assert(rte_pktmbuf_data_len(buf) == 0);
		assert(rte_pktmbuf_pkt_len(buf) == 0);
		/* Only the first segment keeps headroom. */
		if (i % sges_n)
			buf->data_off = 0;
		buf->port = rxq->port_id;
		buf->data_len = rte_pktmbuf_tailroom(buf);
		buf->pkt_len = rte_pktmbuf_tailroom(buf);
		buf->nb_segs = 1;
		*scat = (struct mlx4_wqe_data_seg){
			.addr = rte_cpu_to_be_64(rte_pktmbuf_mtod(buf,
								  uintptr_t)),
			.byte_count = rte_cpu_to_be_32(buf->data_len),
			.lkey = rte_cpu_to_be_32(rxq->mr->lkey),
		};
		(*elts)[i] = buf;
	}
	DEBUG("%p: allocated and configured %u segments (max %u packets)",
	      (void *)rxq, elts_n, elts_n / sges_n);
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
	struct rte_mbuf *(*elts)[1 << rxq->elts_n] = rxq->elts;

	DEBUG("%p: freeing Rx queue elements", (void *)rxq);
	for (i = 0; (i != RTE_DIM(*elts)); ++i) {
		if (!(*elts)[i])
			continue;
		rte_pktmbuf_free_seg((*elts)[i]);
		(*elts)[i] = NULL;
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
	struct mlx4dv_obj mlxdv;
	struct mlx4dv_rwq dv_rwq;
	struct mlx4dv_cq dv_cq;
	uint32_t mb_len = rte_pktmbuf_data_room_size(mp);
	struct rte_mbuf *(*elts)[rte_align32pow2(desc)];
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
	if (desc != RTE_DIM(*elts)) {
		desc = RTE_DIM(*elts);
		WARN("%p: increased number of descriptors in Rx queue %u"
		     " to the next power of two (%u)",
		     (void *)dev, idx, desc);
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
		.sges_n = 0,
		.elts_n = rte_log2_u32(desc),
		.elts = elts,
		/* Toggle Rx checksum offload if hardware supports it. */
		.csum = (priv->hw_csum &&
			 dev->data->dev_conf.rxmode.hw_ip_checksum),
		.csum_l2tun = (priv->hw_csum_l2tun &&
			       dev->data->dev_conf.rxmode.hw_ip_checksum),
		.stats = {
			.idx = idx,
		},
		.socket = socket,
	};
	/* Enable scattered packets support for this queue if necessary. */
	assert(mb_len >= RTE_PKTMBUF_HEADROOM);
	if (dev->data->dev_conf.rxmode.max_rx_pkt_len <=
	    (mb_len - RTE_PKTMBUF_HEADROOM)) {
		;
	} else if (dev->data->dev_conf.rxmode.enable_scatter) {
		uint32_t size =
			RTE_PKTMBUF_HEADROOM +
			dev->data->dev_conf.rxmode.max_rx_pkt_len;
		uint32_t sges_n;

		/*
		 * Determine the number of SGEs needed for a full packet
		 * and round it to the next power of two.
		 */
		sges_n = rte_log2_u32((size / mb_len) + !!(size % mb_len));
		rxq->sges_n = sges_n;
		/* Make sure sges_n did not overflow. */
		size = mb_len * (1 << rxq->sges_n);
		size -= RTE_PKTMBUF_HEADROOM;
		if (size < dev->data->dev_conf.rxmode.max_rx_pkt_len) {
			rte_errno = EOVERFLOW;
			ERROR("%p: too many SGEs (%u) needed to handle"
			      " requested maximum packet size %u",
			      (void *)dev,
			      1 << sges_n,
			      dev->data->dev_conf.rxmode.max_rx_pkt_len);
			goto error;
		}
	} else {
		WARN("%p: the requested maximum Rx packet size (%u) is"
		     " larger than a single mbuf (%u) and scattered"
		     " mode has not been requested",
		     (void *)dev,
		     dev->data->dev_conf.rxmode.max_rx_pkt_len,
		     mb_len - RTE_PKTMBUF_HEADROOM);
	}
	DEBUG("%p: maximum number of segments per packet: %u",
	      (void *)dev, 1 << rxq->sges_n);
	if (desc % (1 << rxq->sges_n)) {
		rte_errno = EINVAL;
		ERROR("%p: number of Rx queue descriptors (%u) is not a"
		      " multiple of maximum segments per packet (%u)",
		      (void *)dev,
		      desc,
		      1 << rxq->sges_n);
		goto error;
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
	rxq->cq = ibv_create_cq(priv->ctx, desc >> rxq->sges_n, NULL,
				rxq->channel, 0);
	if (!rxq->cq) {
		rte_errno = ENOMEM;
		ERROR("%p: CQ creation failure: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	rxq->wq = ibv_create_wq
		(priv->ctx,
		 &(struct ibv_wq_init_attr){
			.wq_type = IBV_WQT_RQ,
			.max_wr = desc >> rxq->sges_n,
			.max_sge = 1 << rxq->sges_n,
			.pd = priv->pd,
			.cq = rxq->cq,
		 });
	if (!rxq->wq) {
		rte_errno = errno ? errno : EINVAL;
		ERROR("%p: WQ creation failure: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	ret = ibv_modify_wq
		(rxq->wq,
		 &(struct ibv_wq_attr){
			.attr_mask = IBV_WQ_ATTR_STATE,
			.wq_state = IBV_WQS_RDY,
		 });
	if (ret) {
		rte_errno = ret;
		ERROR("%p: WQ state to IBV_WPS_RDY failed: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	/* Retrieve device queue information. */
	mlxdv.cq.in = rxq->cq;
	mlxdv.cq.out = &dv_cq;
	mlxdv.rwq.in = rxq->wq;
	mlxdv.rwq.out = &dv_rwq;
	ret = mlx4dv_init_obj(&mlxdv, MLX4DV_OBJ_RWQ | MLX4DV_OBJ_CQ);
	if (ret) {
		rte_errno = EINVAL;
		ERROR("%p: failed to obtain device information", (void *)dev);
		goto error;
	}
	rxq->wqes =
		(volatile struct mlx4_wqe_data_seg (*)[])
		((uintptr_t)dv_rwq.buf.buf + dv_rwq.rq.offset);
	rxq->rq_db = dv_rwq.rdb;
	rxq->rq_ci = 0;
	rxq->mcq.buf = dv_cq.buf.buf;
	rxq->mcq.cqe_cnt = dv_cq.cqe_cnt;
	rxq->mcq.set_ci_db = dv_cq.set_ci_db;
	rxq->mcq.cqe_64 = (dv_cq.cqe_size & 64) ? 1 : 0;
	ret = mlx4_rxq_alloc_elts(rxq);
	if (ret) {
		ERROR("%p: RXQ allocation failed: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	DEBUG("%p: adding Rx queue %p to list", (void *)dev, (void *)rxq);
	dev->data->rx_queues[idx] = rxq;
	/* Enable associated flows. */
	ret = mlx4_flow_sync(priv, &error);
	if (!ret) {
		/* Update doorbell counter. */
		rxq->rq_ci = desc >> rxq->sges_n;
		rte_wmb();
		*rxq->rq_db = rte_cpu_to_be_32(rxq->rq_ci);
		return 0;
	}
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
	if (rxq->wq)
		claim_zero(ibv_destroy_wq(rxq->wq));
	if (rxq->cq)
		claim_zero(ibv_destroy_cq(rxq->cq));
	if (rxq->channel)
		claim_zero(ibv_destroy_comp_channel(rxq->channel));
	if (rxq->mr)
		claim_zero(ibv_dereg_mr(rxq->mr));
	rte_free(rxq);
}
