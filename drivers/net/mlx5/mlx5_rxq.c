/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/queue.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#include <infiniband/mlx5dv.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_ethdev_driver.h>
#include <rte_common.h>
#include <rte_interrupts.h>
#include <rte_debug.h>
#include <rte_io.h>

#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"
#include "mlx5_autoconf.h"
#include "mlx5_defs.h"
#include "mlx5_glue.h"

/* Default RSS hash key also used for ConnectX-3. */
uint8_t rss_hash_default_key[] = {
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

/* Length of the default RSS hash key. */
static_assert(MLX5_RSS_HASH_KEY_LEN ==
	      (unsigned int)sizeof(rss_hash_default_key),
	      "wrong RSS default key size.");

/**
 * Check whether Multi-Packet RQ can be enabled for the device.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   1 if supported, negative errno value if not.
 */
inline int
mlx5_check_mprq_support(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (priv->config.mprq.enabled &&
	    priv->rxqs_n >= priv->config.mprq.min_rxqs_num)
		return 1;
	return -ENOTSUP;
}

/**
 * Check whether Multi-Packet RQ is enabled for the Rx queue.
 *
 *  @param rxq
 *     Pointer to receive queue structure.
 *
 * @return
 *   0 if disabled, otherwise enabled.
 */
inline int
mlx5_rxq_mprq_enabled(struct mlx5_rxq_data *rxq)
{
	return rxq->strd_num_n > 0;
}

/**
 * Check whether Multi-Packet RQ is enabled for the device.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 if disabled, otherwise enabled.
 */
inline int
mlx5_mprq_enabled(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint16_t i;
	uint16_t n = 0;

	if (mlx5_check_mprq_support(dev) < 0)
		return 0;
	/* All the configured queues should be enabled. */
	for (i = 0; i < priv->rxqs_n; ++i) {
		struct mlx5_rxq_data *rxq = (*priv->rxqs)[i];

		if (!rxq)
			continue;
		if (mlx5_rxq_mprq_enabled(rxq))
			++n;
	}
	/* Multi-Packet RQ can't be partially configured. */
	assert(n == 0 || n == priv->rxqs_n);
	return n == priv->rxqs_n;
}

/**
 * Allocate RX queue elements for Multi-Packet RQ.
 *
 * @param rxq_ctrl
 *   Pointer to RX queue structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
rxq_alloc_elts_mprq(struct mlx5_rxq_ctrl *rxq_ctrl)
{
	struct mlx5_rxq_data *rxq = &rxq_ctrl->rxq;
	unsigned int wqe_n = 1 << rxq->elts_n;
	unsigned int i;
	int err;

	/* Iterate on segments. */
	for (i = 0; i <= wqe_n; ++i) {
		struct mlx5_mprq_buf *buf;

		if (rte_mempool_get(rxq->mprq_mp, (void **)&buf) < 0) {
			DRV_LOG(ERR, "port %u empty mbuf pool", rxq->port_id);
			rte_errno = ENOMEM;
			goto error;
		}
		if (i < wqe_n)
			(*rxq->mprq_bufs)[i] = buf;
		else
			rxq->mprq_repl = buf;
	}
	DRV_LOG(DEBUG,
		"port %u Rx queue %u allocated and configured %u segments",
		rxq->port_id, rxq->idx, wqe_n);
	return 0;
error:
	err = rte_errno; /* Save rte_errno before cleanup. */
	wqe_n = i;
	for (i = 0; (i != wqe_n); ++i) {
		if ((*rxq->mprq_bufs)[i] != NULL)
			rte_mempool_put(rxq->mprq_mp,
					(*rxq->mprq_bufs)[i]);
		(*rxq->mprq_bufs)[i] = NULL;
	}
	DRV_LOG(DEBUG, "port %u Rx queue %u failed, freed everything",
		rxq->port_id, rxq->idx);
	rte_errno = err; /* Restore rte_errno. */
	return -rte_errno;
}

/**
 * Allocate RX queue elements for Single-Packet RQ.
 *
 * @param rxq_ctrl
 *   Pointer to RX queue structure.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
rxq_alloc_elts_sprq(struct mlx5_rxq_ctrl *rxq_ctrl)
{
	const unsigned int sges_n = 1 << rxq_ctrl->rxq.sges_n;
	unsigned int elts_n = 1 << rxq_ctrl->rxq.elts_n;
	unsigned int i;
	int err;

	/* Iterate on segments. */
	for (i = 0; (i != elts_n); ++i) {
		struct rte_mbuf *buf;

		buf = rte_pktmbuf_alloc(rxq_ctrl->rxq.mp);
		if (buf == NULL) {
			DRV_LOG(ERR, "port %u empty mbuf pool",
				PORT_ID(rxq_ctrl->priv));
			rte_errno = ENOMEM;
			goto error;
		}
		/* Headroom is reserved by rte_pktmbuf_alloc(). */
		assert(DATA_OFF(buf) == RTE_PKTMBUF_HEADROOM);
		/* Buffer is supposed to be empty. */
		assert(rte_pktmbuf_data_len(buf) == 0);
		assert(rte_pktmbuf_pkt_len(buf) == 0);
		assert(!buf->next);
		/* Only the first segment keeps headroom. */
		if (i % sges_n)
			SET_DATA_OFF(buf, 0);
		PORT(buf) = rxq_ctrl->rxq.port_id;
		DATA_LEN(buf) = rte_pktmbuf_tailroom(buf);
		PKT_LEN(buf) = DATA_LEN(buf);
		NB_SEGS(buf) = 1;
		(*rxq_ctrl->rxq.elts)[i] = buf;
	}
	/* If Rx vector is activated. */
	if (mlx5_rxq_check_vec_support(&rxq_ctrl->rxq) > 0) {
		struct mlx5_rxq_data *rxq = &rxq_ctrl->rxq;
		struct rte_mbuf *mbuf_init = &rxq->fake_mbuf;
		int j;

		/* Initialize default rearm_data for vPMD. */
		mbuf_init->data_off = RTE_PKTMBUF_HEADROOM;
		rte_mbuf_refcnt_set(mbuf_init, 1);
		mbuf_init->nb_segs = 1;
		mbuf_init->port = rxq->port_id;
		/*
		 * prevent compiler reordering:
		 * rearm_data covers previous fields.
		 */
		rte_compiler_barrier();
		rxq->mbuf_initializer =
			*(uint64_t *)&mbuf_init->rearm_data;
		/* Padding with a fake mbuf for vectorized Rx. */
		for (j = 0; j < MLX5_VPMD_DESCS_PER_LOOP; ++j)
			(*rxq->elts)[elts_n + j] = &rxq->fake_mbuf;
	}
	DRV_LOG(DEBUG,
		"port %u Rx queue %u allocated and configured %u segments"
		" (max %u packets)",
		PORT_ID(rxq_ctrl->priv), rxq_ctrl->rxq.idx, elts_n,
		elts_n / (1 << rxq_ctrl->rxq.sges_n));
	return 0;
error:
	err = rte_errno; /* Save rte_errno before cleanup. */
	elts_n = i;
	for (i = 0; (i != elts_n); ++i) {
		if ((*rxq_ctrl->rxq.elts)[i] != NULL)
			rte_pktmbuf_free_seg((*rxq_ctrl->rxq.elts)[i]);
		(*rxq_ctrl->rxq.elts)[i] = NULL;
	}
	DRV_LOG(DEBUG, "port %u Rx queue %u failed, freed everything",
		PORT_ID(rxq_ctrl->priv), rxq_ctrl->rxq.idx);
	rte_errno = err; /* Restore rte_errno. */
	return -rte_errno;
}

/**
 * Allocate RX queue elements.
 *
 * @param rxq_ctrl
 *   Pointer to RX queue structure.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
rxq_alloc_elts(struct mlx5_rxq_ctrl *rxq_ctrl)
{
	return mlx5_rxq_mprq_enabled(&rxq_ctrl->rxq) ?
	       rxq_alloc_elts_mprq(rxq_ctrl) : rxq_alloc_elts_sprq(rxq_ctrl);
}

/**
 * Free RX queue elements for Multi-Packet RQ.
 *
 * @param rxq_ctrl
 *   Pointer to RX queue structure.
 */
static void
rxq_free_elts_mprq(struct mlx5_rxq_ctrl *rxq_ctrl)
{
	struct mlx5_rxq_data *rxq = &rxq_ctrl->rxq;
	uint16_t i;

	DRV_LOG(DEBUG, "port %u Multi-Packet Rx queue %u freeing WRs",
		rxq->port_id, rxq->idx);
	if (rxq->mprq_bufs == NULL)
		return;
	assert(mlx5_rxq_check_vec_support(rxq) < 0);
	for (i = 0; (i != (1u << rxq->elts_n)); ++i) {
		if ((*rxq->mprq_bufs)[i] != NULL)
			mlx5_mprq_buf_free((*rxq->mprq_bufs)[i]);
		(*rxq->mprq_bufs)[i] = NULL;
	}
	if (rxq->mprq_repl != NULL) {
		mlx5_mprq_buf_free(rxq->mprq_repl);
		rxq->mprq_repl = NULL;
	}
}

/**
 * Free RX queue elements for Single-Packet RQ.
 *
 * @param rxq_ctrl
 *   Pointer to RX queue structure.
 */
static void
rxq_free_elts_sprq(struct mlx5_rxq_ctrl *rxq_ctrl)
{
	struct mlx5_rxq_data *rxq = &rxq_ctrl->rxq;
	const uint16_t q_n = (1 << rxq->elts_n);
	const uint16_t q_mask = q_n - 1;
	uint16_t used = q_n - (rxq->rq_ci - rxq->rq_pi);
	uint16_t i;

	DRV_LOG(DEBUG, "port %u Rx queue %u freeing WRs",
		PORT_ID(rxq_ctrl->priv), rxq->idx);
	if (rxq->elts == NULL)
		return;
	/**
	 * Some mbuf in the Ring belongs to the application.  They cannot be
	 * freed.
	 */
	if (mlx5_rxq_check_vec_support(rxq) > 0) {
		for (i = 0; i < used; ++i)
			(*rxq->elts)[(rxq->rq_ci + i) & q_mask] = NULL;
		rxq->rq_pi = rxq->rq_ci;
	}
	for (i = 0; (i != (1u << rxq->elts_n)); ++i) {
		if ((*rxq->elts)[i] != NULL)
			rte_pktmbuf_free_seg((*rxq->elts)[i]);
		(*rxq->elts)[i] = NULL;
	}
}

/**
 * Free RX queue elements.
 *
 * @param rxq_ctrl
 *   Pointer to RX queue structure.
 */
static void
rxq_free_elts(struct mlx5_rxq_ctrl *rxq_ctrl)
{
	if (mlx5_rxq_mprq_enabled(&rxq_ctrl->rxq))
		rxq_free_elts_mprq(rxq_ctrl);
	else
		rxq_free_elts_sprq(rxq_ctrl);
}

/**
 * Clean up a RX queue.
 *
 * Destroy objects, free allocated memory and reset the structure for reuse.
 *
 * @param rxq_ctrl
 *   Pointer to RX queue structure.
 */
void
mlx5_rxq_cleanup(struct mlx5_rxq_ctrl *rxq_ctrl)
{
	DRV_LOG(DEBUG, "port %u cleaning up Rx queue %u",
		PORT_ID(rxq_ctrl->priv), rxq_ctrl->rxq.idx);
	if (rxq_ctrl->ibv)
		mlx5_rxq_ibv_release(rxq_ctrl->ibv);
	memset(rxq_ctrl, 0, sizeof(*rxq_ctrl));
}

/**
 * Returns the per-queue supported offloads.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   Supported Rx offloads.
 */
uint64_t
mlx5_get_rx_queue_offloads(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_config *config = &priv->config;
	uint64_t offloads = (DEV_RX_OFFLOAD_SCATTER |
			     DEV_RX_OFFLOAD_TIMESTAMP |
			     DEV_RX_OFFLOAD_JUMBO_FRAME);

	if (config->hw_fcs_strip)
		offloads |= DEV_RX_OFFLOAD_KEEP_CRC;

	if (config->hw_csum)
		offloads |= (DEV_RX_OFFLOAD_IPV4_CKSUM |
			     DEV_RX_OFFLOAD_UDP_CKSUM |
			     DEV_RX_OFFLOAD_TCP_CKSUM);
	if (config->hw_vlan_strip)
		offloads |= DEV_RX_OFFLOAD_VLAN_STRIP;
	return offloads;
}


/**
 * Returns the per-port supported offloads.
 *
 * @return
 *   Supported Rx offloads.
 */
uint64_t
mlx5_get_rx_port_offloads(void)
{
	uint64_t offloads = DEV_RX_OFFLOAD_VLAN_FILTER;

	return offloads;
}

/**
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
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_rx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
		    unsigned int socket, const struct rte_eth_rxconf *conf,
		    struct rte_mempool *mp)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq, struct mlx5_rxq_ctrl, rxq);

	if (!rte_is_power_of_2(desc)) {
		desc = 1 << log2above(desc);
		DRV_LOG(WARNING,
			"port %u increased number of descriptors in Rx queue %u"
			" to the next power of two (%d)",
			dev->data->port_id, idx, desc);
	}
	DRV_LOG(DEBUG, "port %u configuring Rx queue %u for %u descriptors",
		dev->data->port_id, idx, desc);
	if (idx >= priv->rxqs_n) {
		DRV_LOG(ERR, "port %u Rx queue index out of range (%u >= %u)",
			dev->data->port_id, idx, priv->rxqs_n);
		rte_errno = EOVERFLOW;
		return -rte_errno;
	}
	if (!mlx5_rxq_releasable(dev, idx)) {
		DRV_LOG(ERR, "port %u unable to release queue index %u",
			dev->data->port_id, idx);
		rte_errno = EBUSY;
		return -rte_errno;
	}
	mlx5_rxq_release(dev, idx);
	rxq_ctrl = mlx5_rxq_new(dev, idx, desc, socket, conf, mp);
	if (!rxq_ctrl) {
		DRV_LOG(ERR, "port %u unable to allocate queue index %u",
			dev->data->port_id, idx);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	DRV_LOG(DEBUG, "port %u adding Rx queue %u to list",
		dev->data->port_id, idx);
	(*priv->rxqs)[idx] = &rxq_ctrl->rxq;
	return 0;
}

/**
 * DPDK callback to release a RX queue.
 *
 * @param dpdk_rxq
 *   Generic RX queue pointer.
 */
void
mlx5_rx_queue_release(void *dpdk_rxq)
{
	struct mlx5_rxq_data *rxq = (struct mlx5_rxq_data *)dpdk_rxq;
	struct mlx5_rxq_ctrl *rxq_ctrl;
	struct mlx5_priv *priv;

	if (rxq == NULL)
		return;
	rxq_ctrl = container_of(rxq, struct mlx5_rxq_ctrl, rxq);
	priv = rxq_ctrl->priv;
	if (!mlx5_rxq_releasable(ETH_DEV(priv), rxq_ctrl->rxq.idx))
		rte_panic("port %u Rx queue %u is still used by a flow and"
			  " cannot be removed\n",
			  PORT_ID(priv), rxq->idx);
	mlx5_rxq_release(ETH_DEV(priv), rxq_ctrl->rxq.idx);
}

/**
 * Allocate queue vector and fill epoll fd list for Rx interrupts.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_rx_intr_vec_enable(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;
	unsigned int rxqs_n = priv->rxqs_n;
	unsigned int n = RTE_MIN(rxqs_n, (uint32_t)RTE_MAX_RXTX_INTR_VEC_ID);
	unsigned int count = 0;
	struct rte_intr_handle *intr_handle = dev->intr_handle;

	if (!dev->data->dev_conf.intr_conf.rxq)
		return 0;
	mlx5_rx_intr_vec_disable(dev);
	intr_handle->intr_vec = malloc(n * sizeof(intr_handle->intr_vec[0]));
	if (intr_handle->intr_vec == NULL) {
		DRV_LOG(ERR,
			"port %u failed to allocate memory for interrupt"
			" vector, Rx interrupts will not be supported",
			dev->data->port_id);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	intr_handle->type = RTE_INTR_HANDLE_EXT;
	for (i = 0; i != n; ++i) {
		/* This rxq ibv must not be released in this function. */
		struct mlx5_rxq_ibv *rxq_ibv = mlx5_rxq_ibv_get(dev, i);
		int fd;
		int flags;
		int rc;

		/* Skip queues that cannot request interrupts. */
		if (!rxq_ibv || !rxq_ibv->channel) {
			/* Use invalid intr_vec[] index to disable entry. */
			intr_handle->intr_vec[i] =
				RTE_INTR_VEC_RXTX_OFFSET +
				RTE_MAX_RXTX_INTR_VEC_ID;
			continue;
		}
		if (count >= RTE_MAX_RXTX_INTR_VEC_ID) {
			DRV_LOG(ERR,
				"port %u too many Rx queues for interrupt"
				" vector size (%d), Rx interrupts cannot be"
				" enabled",
				dev->data->port_id, RTE_MAX_RXTX_INTR_VEC_ID);
			mlx5_rx_intr_vec_disable(dev);
			rte_errno = ENOMEM;
			return -rte_errno;
		}
		fd = rxq_ibv->channel->fd;
		flags = fcntl(fd, F_GETFL);
		rc = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
		if (rc < 0) {
			rte_errno = errno;
			DRV_LOG(ERR,
				"port %u failed to make Rx interrupt file"
				" descriptor %d non-blocking for queue index"
				" %d",
				dev->data->port_id, fd, i);
			mlx5_rx_intr_vec_disable(dev);
			return -rte_errno;
		}
		intr_handle->intr_vec[i] = RTE_INTR_VEC_RXTX_OFFSET + count;
		intr_handle->efds[count] = fd;
		count++;
	}
	if (!count)
		mlx5_rx_intr_vec_disable(dev);
	else
		intr_handle->nb_efd = count;
	return 0;
}

/**
 * Clean up Rx interrupts handler.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
void
mlx5_rx_intr_vec_disable(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_intr_handle *intr_handle = dev->intr_handle;
	unsigned int i;
	unsigned int rxqs_n = priv->rxqs_n;
	unsigned int n = RTE_MIN(rxqs_n, (uint32_t)RTE_MAX_RXTX_INTR_VEC_ID);

	if (!dev->data->dev_conf.intr_conf.rxq)
		return;
	if (!intr_handle->intr_vec)
		goto free;
	for (i = 0; i != n; ++i) {
		struct mlx5_rxq_ctrl *rxq_ctrl;
		struct mlx5_rxq_data *rxq_data;

		if (intr_handle->intr_vec[i] == RTE_INTR_VEC_RXTX_OFFSET +
		    RTE_MAX_RXTX_INTR_VEC_ID)
			continue;
		/**
		 * Need to access directly the queue to release the reference
		 * kept in priv_rx_intr_vec_enable().
		 */
		rxq_data = (*priv->rxqs)[i];
		rxq_ctrl = container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
		mlx5_rxq_ibv_release(rxq_ctrl->ibv);
	}
free:
	rte_intr_free_epoll_fd(intr_handle);
	if (intr_handle->intr_vec)
		free(intr_handle->intr_vec);
	intr_handle->nb_efd = 0;
	intr_handle->intr_vec = NULL;
}

/**
 *  MLX5 CQ notification .
 *
 *  @param rxq
 *     Pointer to receive queue structure.
 *  @param sq_n_rxq
 *     Sequence number per receive queue .
 */
static inline void
mlx5_arm_cq(struct mlx5_rxq_data *rxq, int sq_n_rxq)
{
	int sq_n = 0;
	uint32_t doorbell_hi;
	uint64_t doorbell;
	void *cq_db_reg = (char *)rxq->cq_uar + MLX5_CQ_DOORBELL;

	sq_n = sq_n_rxq & MLX5_CQ_SQN_MASK;
	doorbell_hi = sq_n << MLX5_CQ_SQN_OFFSET | (rxq->cq_ci & MLX5_CI_MASK);
	doorbell = (uint64_t)doorbell_hi << 32;
	doorbell |=  rxq->cqn;
	rxq->cq_db[MLX5_CQ_ARM_DB] = rte_cpu_to_be_32(doorbell_hi);
	mlx5_uar_write64(rte_cpu_to_be_64(doorbell),
			 cq_db_reg, rxq->uar_lock_cq);
}

/**
 * DPDK callback for Rx queue interrupt enable.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param rx_queue_id
 *   Rx queue number.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_rx_intr_enable(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq_data;
	struct mlx5_rxq_ctrl *rxq_ctrl;

	rxq_data = (*priv->rxqs)[rx_queue_id];
	if (!rxq_data) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	rxq_ctrl = container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	if (rxq_ctrl->irq) {
		struct mlx5_rxq_ibv *rxq_ibv;

		rxq_ibv = mlx5_rxq_ibv_get(dev, rx_queue_id);
		if (!rxq_ibv) {
			rte_errno = EINVAL;
			return -rte_errno;
		}
		mlx5_arm_cq(rxq_data, rxq_data->cq_arm_sn);
		mlx5_rxq_ibv_release(rxq_ibv);
	}
	return 0;
}

/**
 * DPDK callback for Rx queue interrupt disable.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param rx_queue_id
 *   Rx queue number.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_rx_intr_disable(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq_data;
	struct mlx5_rxq_ctrl *rxq_ctrl;
	struct mlx5_rxq_ibv *rxq_ibv = NULL;
	struct ibv_cq *ev_cq;
	void *ev_ctx;
	int ret;

	rxq_data = (*priv->rxqs)[rx_queue_id];
	if (!rxq_data) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	rxq_ctrl = container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	if (!rxq_ctrl->irq)
		return 0;
	rxq_ibv = mlx5_rxq_ibv_get(dev, rx_queue_id);
	if (!rxq_ibv) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	ret = mlx5_glue->get_cq_event(rxq_ibv->channel, &ev_cq, &ev_ctx);
	if (ret || ev_cq != rxq_ibv->cq) {
		rte_errno = EINVAL;
		goto exit;
	}
	rxq_data->cq_arm_sn++;
	mlx5_glue->ack_cq_events(rxq_ibv->cq, 1);
	mlx5_rxq_ibv_release(rxq_ibv);
	return 0;
exit:
	ret = rte_errno; /* Save rte_errno before cleanup. */
	if (rxq_ibv)
		mlx5_rxq_ibv_release(rxq_ibv);
	DRV_LOG(WARNING, "port %u unable to disable interrupt on Rx queue %d",
		dev->data->port_id, rx_queue_id);
	rte_errno = ret; /* Restore rte_errno. */
	return -rte_errno;
}

/**
 * Create the Rx queue Verbs object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Rx queue array
 *
 * @return
 *   The Verbs object initialised, NULL otherwise and rte_errno is set.
 */
struct mlx5_rxq_ibv *
mlx5_rxq_ibv_new(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	struct ibv_wq_attr mod;
	union {
		struct {
			struct ibv_cq_init_attr_ex ibv;
			struct mlx5dv_cq_init_attr mlx5;
		} cq;
		struct {
			struct ibv_wq_init_attr ibv;
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
			struct mlx5dv_wq_init_attr mlx5;
#endif
		} wq;
		struct ibv_cq_ex cq_attr;
	} attr;
	unsigned int cqe_n;
	unsigned int wqe_n = 1 << rxq_data->elts_n;
	struct mlx5_rxq_ibv *tmpl;
	struct mlx5dv_cq cq_info;
	struct mlx5dv_rwq rwq;
	unsigned int i;
	int ret = 0;
	struct mlx5dv_obj obj;
	struct mlx5_dev_config *config = &priv->config;
	const int mprq_en = mlx5_rxq_mprq_enabled(rxq_data);

	assert(rxq_data);
	assert(!rxq_ctrl->ibv);
	priv->verbs_alloc_ctx.type = MLX5_VERBS_ALLOC_TYPE_RX_QUEUE;
	priv->verbs_alloc_ctx.obj = rxq_ctrl;
	tmpl = rte_calloc_socket(__func__, 1, sizeof(*tmpl), 0,
				 rxq_ctrl->socket);
	if (!tmpl) {
		DRV_LOG(ERR,
			"port %u Rx queue %u cannot allocate verbs resources",
			dev->data->port_id, rxq_data->idx);
		rte_errno = ENOMEM;
		goto error;
	}
	tmpl->rxq_ctrl = rxq_ctrl;
	if (rxq_ctrl->irq) {
		tmpl->channel = mlx5_glue->create_comp_channel(priv->sh->ctx);
		if (!tmpl->channel) {
			DRV_LOG(ERR, "port %u: comp channel creation failure",
				dev->data->port_id);
			rte_errno = ENOMEM;
			goto error;
		}
	}
	if (mprq_en)
		cqe_n = wqe_n * (1 << rxq_data->strd_num_n) - 1;
	else
		cqe_n = wqe_n  - 1;
	attr.cq.ibv = (struct ibv_cq_init_attr_ex){
		.cqe = cqe_n,
		.channel = tmpl->channel,
		.comp_mask = 0,
	};
	attr.cq.mlx5 = (struct mlx5dv_cq_init_attr){
		.comp_mask = 0,
	};
	if (config->cqe_comp && !rxq_data->hw_timestamp) {
		attr.cq.mlx5.comp_mask |=
			MLX5DV_CQ_INIT_ATTR_MASK_COMPRESSED_CQE;
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
		attr.cq.mlx5.cqe_comp_res_format =
			mprq_en ? MLX5DV_CQE_RES_FORMAT_CSUM_STRIDX :
				  MLX5DV_CQE_RES_FORMAT_HASH;
#else
		attr.cq.mlx5.cqe_comp_res_format = MLX5DV_CQE_RES_FORMAT_HASH;
#endif
		/*
		 * For vectorized Rx, it must not be doubled in order to
		 * make cq_ci and rq_ci aligned.
		 */
		if (mlx5_rxq_check_vec_support(rxq_data) < 0)
			attr.cq.ibv.cqe *= 2;
	} else if (config->cqe_comp && rxq_data->hw_timestamp) {
		DRV_LOG(DEBUG,
			"port %u Rx CQE compression is disabled for HW"
			" timestamp",
			dev->data->port_id);
	}
#ifdef HAVE_IBV_MLX5_MOD_CQE_128B_PAD
	if (config->cqe_pad) {
		attr.cq.mlx5.comp_mask |= MLX5DV_CQ_INIT_ATTR_MASK_FLAGS;
		attr.cq.mlx5.flags |= MLX5DV_CQ_INIT_ATTR_FLAGS_CQE_PAD;
	}
#endif
	tmpl->cq = mlx5_glue->cq_ex_to_cq
		(mlx5_glue->dv_create_cq(priv->sh->ctx, &attr.cq.ibv,
					 &attr.cq.mlx5));
	if (tmpl->cq == NULL) {
		DRV_LOG(ERR, "port %u Rx queue %u CQ creation failure",
			dev->data->port_id, idx);
		rte_errno = ENOMEM;
		goto error;
	}
	DRV_LOG(DEBUG, "port %u device_attr.max_qp_wr is %d",
		dev->data->port_id, priv->sh->device_attr.orig_attr.max_qp_wr);
	DRV_LOG(DEBUG, "port %u device_attr.max_sge is %d",
		dev->data->port_id, priv->sh->device_attr.orig_attr.max_sge);
	attr.wq.ibv = (struct ibv_wq_init_attr){
		.wq_context = NULL, /* Could be useful in the future. */
		.wq_type = IBV_WQT_RQ,
		/* Max number of outstanding WRs. */
		.max_wr = wqe_n >> rxq_data->sges_n,
		/* Max number of scatter/gather elements in a WR. */
		.max_sge = 1 << rxq_data->sges_n,
		.pd = priv->sh->pd,
		.cq = tmpl->cq,
		.comp_mask =
			IBV_WQ_FLAGS_CVLAN_STRIPPING |
			0,
		.create_flags = (rxq_data->vlan_strip ?
				 IBV_WQ_FLAGS_CVLAN_STRIPPING :
				 0),
	};
	/* By default, FCS (CRC) is stripped by hardware. */
	if (rxq_data->crc_present) {
		attr.wq.ibv.create_flags |= IBV_WQ_FLAGS_SCATTER_FCS;
		attr.wq.ibv.comp_mask |= IBV_WQ_INIT_ATTR_FLAGS;
	}
	if (config->hw_padding) {
#if defined(HAVE_IBV_WQ_FLAG_RX_END_PADDING)
		attr.wq.ibv.create_flags |= IBV_WQ_FLAG_RX_END_PADDING;
		attr.wq.ibv.comp_mask |= IBV_WQ_INIT_ATTR_FLAGS;
#elif defined(HAVE_IBV_WQ_FLAGS_PCI_WRITE_END_PADDING)
		attr.wq.ibv.create_flags |= IBV_WQ_FLAGS_PCI_WRITE_END_PADDING;
		attr.wq.ibv.comp_mask |= IBV_WQ_INIT_ATTR_FLAGS;
#endif
	}
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
	attr.wq.mlx5 = (struct mlx5dv_wq_init_attr){
		.comp_mask = 0,
	};
	if (mprq_en) {
		struct mlx5dv_striding_rq_init_attr *mprq_attr =
			&attr.wq.mlx5.striding_rq_attrs;

		attr.wq.mlx5.comp_mask |= MLX5DV_WQ_INIT_ATTR_MASK_STRIDING_RQ;
		*mprq_attr = (struct mlx5dv_striding_rq_init_attr){
			.single_stride_log_num_of_bytes = rxq_data->strd_sz_n,
			.single_wqe_log_num_of_strides = rxq_data->strd_num_n,
			.two_byte_shift_en = MLX5_MPRQ_TWO_BYTE_SHIFT,
		};
	}
	tmpl->wq = mlx5_glue->dv_create_wq(priv->sh->ctx, &attr.wq.ibv,
					   &attr.wq.mlx5);
#else
	tmpl->wq = mlx5_glue->create_wq(priv->sh->ctx, &attr.wq.ibv);
#endif
	if (tmpl->wq == NULL) {
		DRV_LOG(ERR, "port %u Rx queue %u WQ creation failure",
			dev->data->port_id, idx);
		rte_errno = ENOMEM;
		goto error;
	}
	/*
	 * Make sure number of WRs*SGEs match expectations since a queue
	 * cannot allocate more than "desc" buffers.
	 */
	if (attr.wq.ibv.max_wr != (wqe_n >> rxq_data->sges_n) ||
	    attr.wq.ibv.max_sge != (1u << rxq_data->sges_n)) {
		DRV_LOG(ERR,
			"port %u Rx queue %u requested %u*%u but got %u*%u"
			" WRs*SGEs",
			dev->data->port_id, idx,
			wqe_n >> rxq_data->sges_n, (1 << rxq_data->sges_n),
			attr.wq.ibv.max_wr, attr.wq.ibv.max_sge);
		rte_errno = EINVAL;
		goto error;
	}
	/* Change queue state to ready. */
	mod = (struct ibv_wq_attr){
		.attr_mask = IBV_WQ_ATTR_STATE,
		.wq_state = IBV_WQS_RDY,
	};
	ret = mlx5_glue->modify_wq(tmpl->wq, &mod);
	if (ret) {
		DRV_LOG(ERR,
			"port %u Rx queue %u WQ state to IBV_WQS_RDY failed",
			dev->data->port_id, idx);
		rte_errno = ret;
		goto error;
	}
	obj.cq.in = tmpl->cq;
	obj.cq.out = &cq_info;
	obj.rwq.in = tmpl->wq;
	obj.rwq.out = &rwq;
	ret = mlx5_glue->dv_init_obj(&obj, MLX5DV_OBJ_CQ | MLX5DV_OBJ_RWQ);
	if (ret) {
		rte_errno = ret;
		goto error;
	}
	if (cq_info.cqe_size != RTE_CACHE_LINE_SIZE) {
		DRV_LOG(ERR,
			"port %u wrong MLX5_CQE_SIZE environment variable"
			" value: it should be set to %u",
			dev->data->port_id, RTE_CACHE_LINE_SIZE);
		rte_errno = EINVAL;
		goto error;
	}
	/* Fill the rings. */
	rxq_data->wqes = rwq.buf;
	for (i = 0; (i != wqe_n); ++i) {
		volatile struct mlx5_wqe_data_seg *scat;
		uintptr_t addr;
		uint32_t byte_count;

		if (mprq_en) {
			struct mlx5_mprq_buf *buf = (*rxq_data->mprq_bufs)[i];

			scat = &((volatile struct mlx5_wqe_mprq *)
				 rxq_data->wqes)[i].dseg;
			addr = (uintptr_t)mlx5_mprq_buf_addr(buf);
			byte_count = (1 << rxq_data->strd_sz_n) *
				     (1 << rxq_data->strd_num_n);
		} else {
			struct rte_mbuf *buf = (*rxq_data->elts)[i];

			scat = &((volatile struct mlx5_wqe_data_seg *)
				 rxq_data->wqes)[i];
			addr = rte_pktmbuf_mtod(buf, uintptr_t);
			byte_count = DATA_LEN(buf);
		}
		/* scat->addr must be able to store a pointer. */
		assert(sizeof(scat->addr) >= sizeof(uintptr_t));
		*scat = (struct mlx5_wqe_data_seg){
			.addr = rte_cpu_to_be_64(addr),
			.byte_count = rte_cpu_to_be_32(byte_count),
			.lkey = mlx5_rx_addr2mr(rxq_data, addr),
		};
	}
	rxq_data->rq_db = rwq.dbrec;
	rxq_data->cqe_n = log2above(cq_info.cqe_cnt);
	rxq_data->cq_ci = 0;
	rxq_data->consumed_strd = 0;
	rxq_data->rq_pi = 0;
	rxq_data->zip = (struct rxq_zip){
		.ai = 0,
	};
	rxq_data->cq_db = cq_info.dbrec;
	rxq_data->cqes = (volatile struct mlx5_cqe (*)[])(uintptr_t)cq_info.buf;
	rxq_data->cq_uar = cq_info.cq_uar;
	rxq_data->cqn = cq_info.cqn;
	rxq_data->cq_arm_sn = 0;
	/* Update doorbell counter. */
	rxq_data->rq_ci = wqe_n >> rxq_data->sges_n;
	rte_wmb();
	*rxq_data->rq_db = rte_cpu_to_be_32(rxq_data->rq_ci);
	DRV_LOG(DEBUG, "port %u rxq %u updated with %p", dev->data->port_id,
		idx, (void *)&tmpl);
	rte_atomic32_inc(&tmpl->refcnt);
	LIST_INSERT_HEAD(&priv->rxqsibv, tmpl, next);
	priv->verbs_alloc_ctx.type = MLX5_VERBS_ALLOC_TYPE_NONE;
	return tmpl;
error:
	ret = rte_errno; /* Save rte_errno before cleanup. */
	if (tmpl->wq)
		claim_zero(mlx5_glue->destroy_wq(tmpl->wq));
	if (tmpl->cq)
		claim_zero(mlx5_glue->destroy_cq(tmpl->cq));
	if (tmpl->channel)
		claim_zero(mlx5_glue->destroy_comp_channel(tmpl->channel));
	priv->verbs_alloc_ctx.type = MLX5_VERBS_ALLOC_TYPE_NONE;
	rte_errno = ret; /* Restore rte_errno. */
	return NULL;
}

/**
 * Get an Rx queue Verbs object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Rx queue array
 *
 * @return
 *   The Verbs object if it exists.
 */
struct mlx5_rxq_ibv *
mlx5_rxq_ibv_get(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl;

	if (idx >= priv->rxqs_n)
		return NULL;
	if (!rxq_data)
		return NULL;
	rxq_ctrl = container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	if (rxq_ctrl->ibv) {
		rte_atomic32_inc(&rxq_ctrl->ibv->refcnt);
	}
	return rxq_ctrl->ibv;
}

/**
 * Release an Rx verbs queue object.
 *
 * @param rxq_ibv
 *   Verbs Rx queue object.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
int
mlx5_rxq_ibv_release(struct mlx5_rxq_ibv *rxq_ibv)
{
	assert(rxq_ibv);
	assert(rxq_ibv->wq);
	assert(rxq_ibv->cq);
	if (rte_atomic32_dec_and_test(&rxq_ibv->refcnt)) {
		rxq_free_elts(rxq_ibv->rxq_ctrl);
		claim_zero(mlx5_glue->destroy_wq(rxq_ibv->wq));
		claim_zero(mlx5_glue->destroy_cq(rxq_ibv->cq));
		if (rxq_ibv->channel)
			claim_zero(mlx5_glue->destroy_comp_channel
				   (rxq_ibv->channel));
		LIST_REMOVE(rxq_ibv, next);
		rte_free(rxq_ibv);
		return 0;
	}
	return 1;
}

/**
 * Verify the Verbs Rx queue list is empty
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   The number of object not released.
 */
int
mlx5_rxq_ibv_verify(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret = 0;
	struct mlx5_rxq_ibv *rxq_ibv;

	LIST_FOREACH(rxq_ibv, &priv->rxqsibv, next) {
		DRV_LOG(DEBUG, "port %u Verbs Rx queue %u still referenced",
			dev->data->port_id, rxq_ibv->rxq_ctrl->rxq.idx);
		++ret;
	}
	return ret;
}

/**
 * Return true if a single reference exists on the object.
 *
 * @param rxq_ibv
 *   Verbs Rx queue object.
 */
int
mlx5_rxq_ibv_releasable(struct mlx5_rxq_ibv *rxq_ibv)
{
	assert(rxq_ibv);
	return (rte_atomic32_read(&rxq_ibv->refcnt) == 1);
}

/**
 * Callback function to initialize mbufs for Multi-Packet RQ.
 */
static inline void
mlx5_mprq_buf_init(struct rte_mempool *mp, void *opaque_arg __rte_unused,
		    void *_m, unsigned int i __rte_unused)
{
	struct mlx5_mprq_buf *buf = _m;

	memset(_m, 0, sizeof(*buf));
	buf->mp = mp;
	rte_atomic16_set(&buf->refcnt, 1);
}

/**
 * Free mempool of Multi-Packet RQ.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
int
mlx5_mprq_free_mp(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_mempool *mp = priv->mprq_mp;
	unsigned int i;

	if (mp == NULL)
		return 0;
	DRV_LOG(DEBUG, "port %u freeing mempool (%s) for Multi-Packet RQ",
		dev->data->port_id, mp->name);
	/*
	 * If a buffer in the pool has been externally attached to a mbuf and it
	 * is still in use by application, destroying the Rx qeueue can spoil
	 * the packet. It is unlikely to happen but if application dynamically
	 * creates and destroys with holding Rx packets, this can happen.
	 *
	 * TODO: It is unavoidable for now because the mempool for Multi-Packet
	 * RQ isn't provided by application but managed by PMD.
	 */
	if (!rte_mempool_full(mp)) {
		DRV_LOG(ERR,
			"port %u mempool for Multi-Packet RQ is still in use",
			dev->data->port_id);
		rte_errno = EBUSY;
		return -rte_errno;
	}
	rte_mempool_free(mp);
	/* Unset mempool for each Rx queue. */
	for (i = 0; i != priv->rxqs_n; ++i) {
		struct mlx5_rxq_data *rxq = (*priv->rxqs)[i];

		if (rxq == NULL)
			continue;
		rxq->mprq_mp = NULL;
	}
	priv->mprq_mp = NULL;
	return 0;
}

/**
 * Allocate a mempool for Multi-Packet RQ. All configured Rx queues share the
 * mempool. If already allocated, reuse it if there're enough elements.
 * Otherwise, resize it.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
int
mlx5_mprq_alloc_mp(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_mempool *mp = priv->mprq_mp;
	char name[RTE_MEMPOOL_NAMESIZE];
	unsigned int desc = 0;
	unsigned int buf_len;
	unsigned int obj_num;
	unsigned int obj_size;
	unsigned int strd_num_n = 0;
	unsigned int strd_sz_n = 0;
	unsigned int i;

	if (!mlx5_mprq_enabled(dev))
		return 0;
	/* Count the total number of descriptors configured. */
	for (i = 0; i != priv->rxqs_n; ++i) {
		struct mlx5_rxq_data *rxq = (*priv->rxqs)[i];

		if (rxq == NULL)
			continue;
		desc += 1 << rxq->elts_n;
		/* Get the max number of strides. */
		if (strd_num_n < rxq->strd_num_n)
			strd_num_n = rxq->strd_num_n;
		/* Get the max size of a stride. */
		if (strd_sz_n < rxq->strd_sz_n)
			strd_sz_n = rxq->strd_sz_n;
	}
	assert(strd_num_n && strd_sz_n);
	buf_len = (1 << strd_num_n) * (1 << strd_sz_n);
	obj_size = buf_len + sizeof(struct mlx5_mprq_buf);
	/*
	 * Received packets can be either memcpy'd or externally referenced. In
	 * case that the packet is attached to an mbuf as an external buffer, as
	 * it isn't possible to predict how the buffers will be queued by
	 * application, there's no option to exactly pre-allocate needed buffers
	 * in advance but to speculatively prepares enough buffers.
	 *
	 * In the data path, if this Mempool is depleted, PMD will try to memcpy
	 * received packets to buffers provided by application (rxq->mp) until
	 * this Mempool gets available again.
	 */
	desc *= 4;
	obj_num = desc + MLX5_MPRQ_MP_CACHE_SZ * priv->rxqs_n;
	/*
	 * rte_mempool_create_empty() has sanity check to refuse large cache
	 * size compared to the number of elements.
	 * CACHE_FLUSHTHRESH_MULTIPLIER is defined in a C file, so using a
	 * constant number 2 instead.
	 */
	obj_num = RTE_MAX(obj_num, MLX5_MPRQ_MP_CACHE_SZ * 2);
	/* Check a mempool is already allocated and if it can be resued. */
	if (mp != NULL && mp->elt_size >= obj_size && mp->size >= obj_num) {
		DRV_LOG(DEBUG, "port %u mempool %s is being reused",
			dev->data->port_id, mp->name);
		/* Reuse. */
		goto exit;
	} else if (mp != NULL) {
		DRV_LOG(DEBUG, "port %u mempool %s should be resized, freeing it",
			dev->data->port_id, mp->name);
		/*
		 * If failed to free, which means it may be still in use, no way
		 * but to keep using the existing one. On buffer underrun,
		 * packets will be memcpy'd instead of external buffer
		 * attachment.
		 */
		if (mlx5_mprq_free_mp(dev)) {
			if (mp->elt_size >= obj_size)
				goto exit;
			else
				return -rte_errno;
		}
	}
	snprintf(name, sizeof(name), "port-%u-mprq", dev->data->port_id);
	mp = rte_mempool_create(name, obj_num, obj_size, MLX5_MPRQ_MP_CACHE_SZ,
				0, NULL, NULL, mlx5_mprq_buf_init, NULL,
				dev->device->numa_node, 0);
	if (mp == NULL) {
		DRV_LOG(ERR,
			"port %u failed to allocate a mempool for"
			" Multi-Packet RQ, count=%u, size=%u",
			dev->data->port_id, obj_num, obj_size);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	priv->mprq_mp = mp;
exit:
	/* Set mempool for each Rx queue. */
	for (i = 0; i != priv->rxqs_n; ++i) {
		struct mlx5_rxq_data *rxq = (*priv->rxqs)[i];

		if (rxq == NULL)
			continue;
		rxq->mprq_mp = mp;
	}
	DRV_LOG(INFO, "port %u Multi-Packet RQ is configured",
		dev->data->port_id);
	return 0;
}

/**
 * Create a DPDK Rx queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   RX queue index.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 *
 * @return
 *   A DPDK queue object on success, NULL otherwise and rte_errno is set.
 */
struct mlx5_rxq_ctrl *
mlx5_rxq_new(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
	     unsigned int socket, const struct rte_eth_rxconf *conf,
	     struct rte_mempool *mp)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_ctrl *tmpl;
	unsigned int mb_len = rte_pktmbuf_data_room_size(mp);
	unsigned int mprq_stride_size;
	struct mlx5_dev_config *config = &priv->config;
	/*
	 * Always allocate extra slots, even if eventually
	 * the vector Rx will not be used.
	 */
	uint16_t desc_n =
		desc + config->rx_vec_en * MLX5_VPMD_DESCS_PER_LOOP;
	uint64_t offloads = conf->offloads |
			   dev->data->dev_conf.rxmode.offloads;
	const int mprq_en = mlx5_check_mprq_support(dev) > 0;

	tmpl = rte_calloc_socket("RXQ", 1,
				 sizeof(*tmpl) +
				 desc_n * sizeof(struct rte_mbuf *),
				 0, socket);
	if (!tmpl) {
		rte_errno = ENOMEM;
		return NULL;
	}
	if (mlx5_mr_btree_init(&tmpl->rxq.mr_ctrl.cache_bh,
			       MLX5_MR_BTREE_CACHE_N, socket)) {
		/* rte_errno is already set. */
		goto error;
	}
	tmpl->socket = socket;
	if (dev->data->dev_conf.intr_conf.rxq)
		tmpl->irq = 1;
	/*
	 * This Rx queue can be configured as a Multi-Packet RQ if all of the
	 * following conditions are met:
	 *  - MPRQ is enabled.
	 *  - The number of descs is more than the number of strides.
	 *  - max_rx_pkt_len plus overhead is less than the max size of a
	 *    stride.
	 *  Otherwise, enable Rx scatter if necessary.
	 */
	assert(mb_len >= RTE_PKTMBUF_HEADROOM);
	mprq_stride_size =
		dev->data->dev_conf.rxmode.max_rx_pkt_len +
		sizeof(struct rte_mbuf_ext_shared_info) +
		RTE_PKTMBUF_HEADROOM;
	if (mprq_en &&
	    desc > (1U << config->mprq.stride_num_n) &&
	    mprq_stride_size <= (1U << config->mprq.max_stride_size_n)) {
		/* TODO: Rx scatter isn't supported yet. */
		tmpl->rxq.sges_n = 0;
		/* Trim the number of descs needed. */
		desc >>= config->mprq.stride_num_n;
		tmpl->rxq.strd_num_n = config->mprq.stride_num_n;
		tmpl->rxq.strd_sz_n = RTE_MAX(log2above(mprq_stride_size),
					      config->mprq.min_stride_size_n);
		tmpl->rxq.strd_shift_en = MLX5_MPRQ_TWO_BYTE_SHIFT;
		tmpl->rxq.mprq_max_memcpy_len =
			RTE_MIN(mb_len - RTE_PKTMBUF_HEADROOM,
				config->mprq.max_memcpy_len);
		DRV_LOG(DEBUG,
			"port %u Rx queue %u: Multi-Packet RQ is enabled"
			" strd_num_n = %u, strd_sz_n = %u",
			dev->data->port_id, idx,
			tmpl->rxq.strd_num_n, tmpl->rxq.strd_sz_n);
	} else if (dev->data->dev_conf.rxmode.max_rx_pkt_len <=
		   (mb_len - RTE_PKTMBUF_HEADROOM)) {
		tmpl->rxq.sges_n = 0;
	} else if (offloads & DEV_RX_OFFLOAD_SCATTER) {
		unsigned int size =
			RTE_PKTMBUF_HEADROOM +
			dev->data->dev_conf.rxmode.max_rx_pkt_len;
		unsigned int sges_n;

		/*
		 * Determine the number of SGEs needed for a full packet
		 * and round it to the next power of two.
		 */
		sges_n = log2above((size / mb_len) + !!(size % mb_len));
		tmpl->rxq.sges_n = sges_n;
		/* Make sure rxq.sges_n did not overflow. */
		size = mb_len * (1 << tmpl->rxq.sges_n);
		size -= RTE_PKTMBUF_HEADROOM;
		if (size < dev->data->dev_conf.rxmode.max_rx_pkt_len) {
			DRV_LOG(ERR,
				"port %u too many SGEs (%u) needed to handle"
				" requested maximum packet size %u",
				dev->data->port_id,
				1 << sges_n,
				dev->data->dev_conf.rxmode.max_rx_pkt_len);
			rte_errno = EOVERFLOW;
			goto error;
		}
	} else {
		DRV_LOG(WARNING,
			"port %u the requested maximum Rx packet size (%u) is"
			" larger than a single mbuf (%u) and scattered mode has"
			" not been requested",
			dev->data->port_id,
			dev->data->dev_conf.rxmode.max_rx_pkt_len,
			mb_len - RTE_PKTMBUF_HEADROOM);
	}
	if (mprq_en && !mlx5_rxq_mprq_enabled(&tmpl->rxq))
		DRV_LOG(WARNING,
			"port %u MPRQ is requested but cannot be enabled"
			" (requested: desc = %u, stride_sz = %u,"
			" supported: min_stride_num = %u, max_stride_sz = %u).",
			dev->data->port_id, desc, mprq_stride_size,
			(1 << config->mprq.stride_num_n),
			(1 << config->mprq.max_stride_size_n));
	DRV_LOG(DEBUG, "port %u maximum number of segments per packet: %u",
		dev->data->port_id, 1 << tmpl->rxq.sges_n);
	if (desc % (1 << tmpl->rxq.sges_n)) {
		DRV_LOG(ERR,
			"port %u number of Rx queue descriptors (%u) is not a"
			" multiple of SGEs per packet (%u)",
			dev->data->port_id,
			desc,
			1 << tmpl->rxq.sges_n);
		rte_errno = EINVAL;
		goto error;
	}
	/* Toggle RX checksum offload if hardware supports it. */
	tmpl->rxq.csum = !!(offloads & DEV_RX_OFFLOAD_CHECKSUM);
	tmpl->rxq.hw_timestamp = !!(offloads & DEV_RX_OFFLOAD_TIMESTAMP);
	/* Configure VLAN stripping. */
	tmpl->rxq.vlan_strip = !!(offloads & DEV_RX_OFFLOAD_VLAN_STRIP);
	/* By default, FCS (CRC) is stripped by hardware. */
	tmpl->rxq.crc_present = 0;
	if (offloads & DEV_RX_OFFLOAD_KEEP_CRC) {
		if (config->hw_fcs_strip) {
			tmpl->rxq.crc_present = 1;
		} else {
			DRV_LOG(WARNING,
				"port %u CRC stripping has been disabled but will"
				" still be performed by hardware, make sure MLNX_OFED"
				" and firmware are up to date",
				dev->data->port_id);
		}
	}
	DRV_LOG(DEBUG,
		"port %u CRC stripping is %s, %u bytes will be subtracted from"
		" incoming frames to hide it",
		dev->data->port_id,
		tmpl->rxq.crc_present ? "disabled" : "enabled",
		tmpl->rxq.crc_present << 2);
	/* Save port ID. */
	tmpl->rxq.rss_hash = !!priv->rss_conf.rss_hf &&
		(!!(dev->data->dev_conf.rxmode.mq_mode & ETH_MQ_RX_RSS));
	tmpl->rxq.port_id = dev->data->port_id;
	tmpl->priv = priv;
	tmpl->rxq.mp = mp;
	tmpl->rxq.elts_n = log2above(desc);
	tmpl->rxq.rq_repl_thresh =
		MLX5_VPMD_RXQ_RPLNSH_THRESH(1 << tmpl->rxq.elts_n);
	tmpl->rxq.elts =
		(struct rte_mbuf *(*)[1 << tmpl->rxq.elts_n])(tmpl + 1);
#ifndef RTE_ARCH_64
	tmpl->rxq.uar_lock_cq = &priv->uar_lock_cq;
#endif
	tmpl->rxq.idx = idx;
	rte_atomic32_inc(&tmpl->refcnt);
	LIST_INSERT_HEAD(&priv->rxqsctrl, tmpl, next);
	return tmpl;
error:
	rte_free(tmpl);
	return NULL;
}

/**
 * Get a Rx queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   RX queue index.
 *
 * @return
 *   A pointer to the queue if it exists, NULL otherwise.
 */
struct mlx5_rxq_ctrl *
mlx5_rxq_get(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_ctrl *rxq_ctrl = NULL;

	if ((*priv->rxqs)[idx]) {
		rxq_ctrl = container_of((*priv->rxqs)[idx],
					struct mlx5_rxq_ctrl,
					rxq);
		mlx5_rxq_ibv_get(dev, idx);
		rte_atomic32_inc(&rxq_ctrl->refcnt);
	}
	return rxq_ctrl;
}

/**
 * Release a Rx queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   RX queue index.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
int
mlx5_rxq_release(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_ctrl *rxq_ctrl;

	if (!(*priv->rxqs)[idx])
		return 0;
	rxq_ctrl = container_of((*priv->rxqs)[idx], struct mlx5_rxq_ctrl, rxq);
	assert(rxq_ctrl->priv);
	if (rxq_ctrl->ibv && !mlx5_rxq_ibv_release(rxq_ctrl->ibv))
		rxq_ctrl->ibv = NULL;
	if (rte_atomic32_dec_and_test(&rxq_ctrl->refcnt)) {
		mlx5_mr_btree_free(&rxq_ctrl->rxq.mr_ctrl.cache_bh);
		LIST_REMOVE(rxq_ctrl, next);
		rte_free(rxq_ctrl);
		(*priv->rxqs)[idx] = NULL;
		return 0;
	}
	return 1;
}

/**
 * Verify if the queue can be released.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   RX queue index.
 *
 * @return
 *   1 if the queue can be released, negative errno otherwise and rte_errno is
 *   set.
 */
int
mlx5_rxq_releasable(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_ctrl *rxq_ctrl;

	if (!(*priv->rxqs)[idx]) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	rxq_ctrl = container_of((*priv->rxqs)[idx], struct mlx5_rxq_ctrl, rxq);
	return (rte_atomic32_read(&rxq_ctrl->refcnt) == 1);
}

/**
 * Verify the Rx Queue list is empty
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   The number of object not released.
 */
int
mlx5_rxq_verify(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_ctrl *rxq_ctrl;
	int ret = 0;

	LIST_FOREACH(rxq_ctrl, &priv->rxqsctrl, next) {
		DRV_LOG(DEBUG, "port %u Rx Queue %u still referenced",
			dev->data->port_id, rxq_ctrl->rxq.idx);
		++ret;
	}
	return ret;
}

/**
 * Create an indirection table.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param queues
 *   Queues entering in the indirection table.
 * @param queues_n
 *   Number of queues in the array.
 *
 * @return
 *   The Verbs object initialised, NULL otherwise and rte_errno is set.
 */
struct mlx5_ind_table_ibv *
mlx5_ind_table_ibv_new(struct rte_eth_dev *dev, const uint16_t *queues,
		       uint32_t queues_n)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ind_table_ibv *ind_tbl;
	const unsigned int wq_n = rte_is_power_of_2(queues_n) ?
		log2above(queues_n) :
		log2above(priv->config.ind_table_max_size);
	struct ibv_wq *wq[1 << wq_n];
	unsigned int i;
	unsigned int j;

	ind_tbl = rte_calloc(__func__, 1, sizeof(*ind_tbl) +
			     queues_n * sizeof(uint16_t), 0);
	if (!ind_tbl) {
		rte_errno = ENOMEM;
		return NULL;
	}
	for (i = 0; i != queues_n; ++i) {
		struct mlx5_rxq_ctrl *rxq = mlx5_rxq_get(dev, queues[i]);

		if (!rxq)
			goto error;
		wq[i] = rxq->ibv->wq;
		ind_tbl->queues[i] = queues[i];
	}
	ind_tbl->queues_n = queues_n;
	/* Finalise indirection table. */
	for (j = 0; i != (unsigned int)(1 << wq_n); ++i, ++j)
		wq[i] = wq[j];
	ind_tbl->ind_table = mlx5_glue->create_rwq_ind_table
		(priv->sh->ctx,
		 &(struct ibv_rwq_ind_table_init_attr){
			.log_ind_tbl_size = wq_n,
			.ind_tbl = wq,
			.comp_mask = 0,
		 });
	if (!ind_tbl->ind_table) {
		rte_errno = errno;
		goto error;
	}
	rte_atomic32_inc(&ind_tbl->refcnt);
	LIST_INSERT_HEAD(&priv->ind_tbls, ind_tbl, next);
	return ind_tbl;
error:
	rte_free(ind_tbl);
	DEBUG("port %u cannot create indirection table", dev->data->port_id);
	return NULL;
}

/**
 * Get an indirection table.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param queues
 *   Queues entering in the indirection table.
 * @param queues_n
 *   Number of queues in the array.
 *
 * @return
 *   An indirection table if found.
 */
struct mlx5_ind_table_ibv *
mlx5_ind_table_ibv_get(struct rte_eth_dev *dev, const uint16_t *queues,
		       uint32_t queues_n)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ind_table_ibv *ind_tbl;

	LIST_FOREACH(ind_tbl, &priv->ind_tbls, next) {
		if ((ind_tbl->queues_n == queues_n) &&
		    (memcmp(ind_tbl->queues, queues,
			    ind_tbl->queues_n * sizeof(ind_tbl->queues[0]))
		     == 0))
			break;
	}
	if (ind_tbl) {
		unsigned int i;

		rte_atomic32_inc(&ind_tbl->refcnt);
		for (i = 0; i != ind_tbl->queues_n; ++i)
			mlx5_rxq_get(dev, ind_tbl->queues[i]);
	}
	return ind_tbl;
}

/**
 * Release an indirection table.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param ind_table
 *   Indirection table to release.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
int
mlx5_ind_table_ibv_release(struct rte_eth_dev *dev,
			   struct mlx5_ind_table_ibv *ind_tbl)
{
	unsigned int i;

	if (rte_atomic32_dec_and_test(&ind_tbl->refcnt))
		claim_zero(mlx5_glue->destroy_rwq_ind_table
			   (ind_tbl->ind_table));
	for (i = 0; i != ind_tbl->queues_n; ++i)
		claim_nonzero(mlx5_rxq_release(dev, ind_tbl->queues[i]));
	if (!rte_atomic32_read(&ind_tbl->refcnt)) {
		LIST_REMOVE(ind_tbl, next);
		rte_free(ind_tbl);
		return 0;
	}
	return 1;
}

/**
 * Verify the Rx Queue list is empty
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   The number of object not released.
 */
int
mlx5_ind_table_ibv_verify(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ind_table_ibv *ind_tbl;
	int ret = 0;

	LIST_FOREACH(ind_tbl, &priv->ind_tbls, next) {
		DRV_LOG(DEBUG,
			"port %u Verbs indirection table %p still referenced",
			dev->data->port_id, (void *)ind_tbl);
		++ret;
	}
	return ret;
}

/**
 * Create an Rx Hash queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param rss_key
 *   RSS key for the Rx hash queue.
 * @param rss_key_len
 *   RSS key length.
 * @param hash_fields
 *   Verbs protocol hash field to make the RSS on.
 * @param queues
 *   Queues entering in hash queue. In case of empty hash_fields only the
 *   first queue index will be taken for the indirection table.
 * @param queues_n
 *   Number of queues.
 * @param tunnel
 *   Tunnel type.
 *
 * @return
 *   The Verbs object initialised, NULL otherwise and rte_errno is set.
 */
struct mlx5_hrxq *
mlx5_hrxq_new(struct rte_eth_dev *dev,
	      const uint8_t *rss_key, uint32_t rss_key_len,
	      uint64_t hash_fields,
	      const uint16_t *queues, uint32_t queues_n,
	      int tunnel __rte_unused)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hrxq *hrxq;
	struct mlx5_ind_table_ibv *ind_tbl;
	struct ibv_qp *qp;
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	struct mlx5dv_qp_init_attr qp_init_attr;
#endif
	int err;

	queues_n = hash_fields ? queues_n : 1;
	ind_tbl = mlx5_ind_table_ibv_get(dev, queues, queues_n);
	if (!ind_tbl)
		ind_tbl = mlx5_ind_table_ibv_new(dev, queues, queues_n);
	if (!ind_tbl) {
		rte_errno = ENOMEM;
		return NULL;
	}
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	memset(&qp_init_attr, 0, sizeof(qp_init_attr));
	if (tunnel) {
		qp_init_attr.comp_mask =
				MLX5DV_QP_INIT_ATTR_MASK_QP_CREATE_FLAGS;
		qp_init_attr.create_flags = MLX5DV_QP_CREATE_TUNNEL_OFFLOADS;
	}
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	if (dev->data->dev_conf.lpbk_mode) {
		/* Allow packet sent from NIC loop back w/o source MAC check. */
		qp_init_attr.comp_mask |=
				MLX5DV_QP_INIT_ATTR_MASK_QP_CREATE_FLAGS;
		qp_init_attr.create_flags |=
				MLX5DV_QP_CREATE_TIR_ALLOW_SELF_LOOPBACK_UC;
	}
#endif
	qp = mlx5_glue->dv_create_qp
		(priv->sh->ctx,
		 &(struct ibv_qp_init_attr_ex){
			.qp_type = IBV_QPT_RAW_PACKET,
			.comp_mask =
				IBV_QP_INIT_ATTR_PD |
				IBV_QP_INIT_ATTR_IND_TABLE |
				IBV_QP_INIT_ATTR_RX_HASH,
			.rx_hash_conf = (struct ibv_rx_hash_conf){
				.rx_hash_function = IBV_RX_HASH_FUNC_TOEPLITZ,
				.rx_hash_key_len = rss_key_len,
				.rx_hash_key = (void *)(uintptr_t)rss_key,
				.rx_hash_fields_mask = hash_fields,
			},
			.rwq_ind_tbl = ind_tbl->ind_table,
			.pd = priv->sh->pd,
		 },
		 &qp_init_attr);
#else
	qp = mlx5_glue->create_qp_ex
		(priv->sh->ctx,
		 &(struct ibv_qp_init_attr_ex){
			.qp_type = IBV_QPT_RAW_PACKET,
			.comp_mask =
				IBV_QP_INIT_ATTR_PD |
				IBV_QP_INIT_ATTR_IND_TABLE |
				IBV_QP_INIT_ATTR_RX_HASH,
			.rx_hash_conf = (struct ibv_rx_hash_conf){
				.rx_hash_function = IBV_RX_HASH_FUNC_TOEPLITZ,
				.rx_hash_key_len = rss_key_len,
				.rx_hash_key = (void *)(uintptr_t)rss_key,
				.rx_hash_fields_mask = hash_fields,
			},
			.rwq_ind_tbl = ind_tbl->ind_table,
			.pd = priv->sh->pd,
		 });
#endif
	if (!qp) {
		rte_errno = errno;
		goto error;
	}
	hrxq = rte_calloc(__func__, 1, sizeof(*hrxq) + rss_key_len, 0);
	if (!hrxq)
		goto error;
	hrxq->ind_table = ind_tbl;
	hrxq->qp = qp;
	hrxq->rss_key_len = rss_key_len;
	hrxq->hash_fields = hash_fields;
	memcpy(hrxq->rss_key, rss_key, rss_key_len);
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	hrxq->action = mlx5_glue->dv_create_flow_action_dest_ibv_qp(hrxq->qp);
	if (!hrxq->action) {
		rte_errno = errno;
		goto error;
	}
#endif
	rte_atomic32_inc(&hrxq->refcnt);
	LIST_INSERT_HEAD(&priv->hrxqs, hrxq, next);
	return hrxq;
error:
	err = rte_errno; /* Save rte_errno before cleanup. */
	mlx5_ind_table_ibv_release(dev, ind_tbl);
	if (qp)
		claim_zero(mlx5_glue->destroy_qp(qp));
	rte_errno = err; /* Restore rte_errno. */
	return NULL;
}

/**
 * Get an Rx Hash queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param rss_conf
 *   RSS configuration for the Rx hash queue.
 * @param queues
 *   Queues entering in hash queue. In case of empty hash_fields only the
 *   first queue index will be taken for the indirection table.
 * @param queues_n
 *   Number of queues.
 *
 * @return
 *   An hash Rx queue on success.
 */
struct mlx5_hrxq *
mlx5_hrxq_get(struct rte_eth_dev *dev,
	      const uint8_t *rss_key, uint32_t rss_key_len,
	      uint64_t hash_fields,
	      const uint16_t *queues, uint32_t queues_n)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hrxq *hrxq;

	queues_n = hash_fields ? queues_n : 1;
	LIST_FOREACH(hrxq, &priv->hrxqs, next) {
		struct mlx5_ind_table_ibv *ind_tbl;

		if (hrxq->rss_key_len != rss_key_len)
			continue;
		if (memcmp(hrxq->rss_key, rss_key, rss_key_len))
			continue;
		if (hrxq->hash_fields != hash_fields)
			continue;
		ind_tbl = mlx5_ind_table_ibv_get(dev, queues, queues_n);
		if (!ind_tbl)
			continue;
		if (ind_tbl != hrxq->ind_table) {
			mlx5_ind_table_ibv_release(dev, ind_tbl);
			continue;
		}
		rte_atomic32_inc(&hrxq->refcnt);
		return hrxq;
	}
	return NULL;
}

/**
 * Release the hash Rx queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param hrxq
 *   Pointer to Hash Rx queue to release.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
int
mlx5_hrxq_release(struct rte_eth_dev *dev, struct mlx5_hrxq *hrxq)
{
	if (rte_atomic32_dec_and_test(&hrxq->refcnt)) {
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
		mlx5_glue->destroy_flow_action(hrxq->action);
#endif
		claim_zero(mlx5_glue->destroy_qp(hrxq->qp));
		mlx5_ind_table_ibv_release(dev, hrxq->ind_table);
		LIST_REMOVE(hrxq, next);
		rte_free(hrxq);
		return 0;
	}
	claim_nonzero(mlx5_ind_table_ibv_release(dev, hrxq->ind_table));
	return 1;
}

/**
 * Verify the Rx Queue list is empty
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   The number of object not released.
 */
int
mlx5_hrxq_ibv_verify(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hrxq *hrxq;
	int ret = 0;

	LIST_FOREACH(hrxq, &priv->hrxqs, next) {
		DRV_LOG(DEBUG,
			"port %u Verbs hash Rx queue %p still referenced",
			dev->data->port_id, (void *)hrxq);
		++ret;
	}
	return ret;
}

/**
 * Create a drop Rx queue Verbs object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   The Verbs object initialised, NULL otherwise and rte_errno is set.
 */
struct mlx5_rxq_ibv *
mlx5_rxq_ibv_drop_new(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct ibv_context *ctx = priv->sh->ctx;
	struct ibv_cq *cq;
	struct ibv_wq *wq = NULL;
	struct mlx5_rxq_ibv *rxq;

	if (priv->drop_queue.rxq)
		return priv->drop_queue.rxq;
	cq = mlx5_glue->create_cq(ctx, 1, NULL, NULL, 0);
	if (!cq) {
		DEBUG("port %u cannot allocate CQ for drop queue",
		      dev->data->port_id);
		rte_errno = errno;
		goto error;
	}
	wq = mlx5_glue->create_wq(ctx,
		 &(struct ibv_wq_init_attr){
			.wq_type = IBV_WQT_RQ,
			.max_wr = 1,
			.max_sge = 1,
			.pd = priv->sh->pd,
			.cq = cq,
		 });
	if (!wq) {
		DEBUG("port %u cannot allocate WQ for drop queue",
		      dev->data->port_id);
		rte_errno = errno;
		goto error;
	}
	rxq = rte_calloc(__func__, 1, sizeof(*rxq), 0);
	if (!rxq) {
		DEBUG("port %u cannot allocate drop Rx queue memory",
		      dev->data->port_id);
		rte_errno = ENOMEM;
		goto error;
	}
	rxq->cq = cq;
	rxq->wq = wq;
	priv->drop_queue.rxq = rxq;
	return rxq;
error:
	if (wq)
		claim_zero(mlx5_glue->destroy_wq(wq));
	if (cq)
		claim_zero(mlx5_glue->destroy_cq(cq));
	return NULL;
}

/**
 * Release a drop Rx queue Verbs object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   The Verbs object initialised, NULL otherwise and rte_errno is set.
 */
void
mlx5_rxq_ibv_drop_release(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_ibv *rxq = priv->drop_queue.rxq;

	if (rxq->wq)
		claim_zero(mlx5_glue->destroy_wq(rxq->wq));
	if (rxq->cq)
		claim_zero(mlx5_glue->destroy_cq(rxq->cq));
	rte_free(rxq);
	priv->drop_queue.rxq = NULL;
}

/**
 * Create a drop indirection table.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   The Verbs object initialised, NULL otherwise and rte_errno is set.
 */
struct mlx5_ind_table_ibv *
mlx5_ind_table_ibv_drop_new(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ind_table_ibv *ind_tbl;
	struct mlx5_rxq_ibv *rxq;
	struct mlx5_ind_table_ibv tmpl;

	rxq = mlx5_rxq_ibv_drop_new(dev);
	if (!rxq)
		return NULL;
	tmpl.ind_table = mlx5_glue->create_rwq_ind_table
		(priv->sh->ctx,
		 &(struct ibv_rwq_ind_table_init_attr){
			.log_ind_tbl_size = 0,
			.ind_tbl = &rxq->wq,
			.comp_mask = 0,
		 });
	if (!tmpl.ind_table) {
		DEBUG("port %u cannot allocate indirection table for drop"
		      " queue",
		      dev->data->port_id);
		rte_errno = errno;
		goto error;
	}
	ind_tbl = rte_calloc(__func__, 1, sizeof(*ind_tbl), 0);
	if (!ind_tbl) {
		rte_errno = ENOMEM;
		goto error;
	}
	ind_tbl->ind_table = tmpl.ind_table;
	return ind_tbl;
error:
	mlx5_rxq_ibv_drop_release(dev);
	return NULL;
}

/**
 * Release a drop indirection table.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
void
mlx5_ind_table_ibv_drop_release(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ind_table_ibv *ind_tbl = priv->drop_queue.hrxq->ind_table;

	claim_zero(mlx5_glue->destroy_rwq_ind_table(ind_tbl->ind_table));
	mlx5_rxq_ibv_drop_release(dev);
	rte_free(ind_tbl);
	priv->drop_queue.hrxq->ind_table = NULL;
}

/**
 * Create a drop Rx Hash queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   The Verbs object initialised, NULL otherwise and rte_errno is set.
 */
struct mlx5_hrxq *
mlx5_hrxq_drop_new(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ind_table_ibv *ind_tbl;
	struct ibv_qp *qp;
	struct mlx5_hrxq *hrxq;

	if (priv->drop_queue.hrxq) {
		rte_atomic32_inc(&priv->drop_queue.hrxq->refcnt);
		return priv->drop_queue.hrxq;
	}
	ind_tbl = mlx5_ind_table_ibv_drop_new(dev);
	if (!ind_tbl)
		return NULL;
	qp = mlx5_glue->create_qp_ex(priv->sh->ctx,
		 &(struct ibv_qp_init_attr_ex){
			.qp_type = IBV_QPT_RAW_PACKET,
			.comp_mask =
				IBV_QP_INIT_ATTR_PD |
				IBV_QP_INIT_ATTR_IND_TABLE |
				IBV_QP_INIT_ATTR_RX_HASH,
			.rx_hash_conf = (struct ibv_rx_hash_conf){
				.rx_hash_function =
					IBV_RX_HASH_FUNC_TOEPLITZ,
				.rx_hash_key_len = MLX5_RSS_HASH_KEY_LEN,
				.rx_hash_key = rss_hash_default_key,
				.rx_hash_fields_mask = 0,
				},
			.rwq_ind_tbl = ind_tbl->ind_table,
			.pd = priv->sh->pd
		 });
	if (!qp) {
		DEBUG("port %u cannot allocate QP for drop queue",
		      dev->data->port_id);
		rte_errno = errno;
		goto error;
	}
	hrxq = rte_calloc(__func__, 1, sizeof(*hrxq), 0);
	if (!hrxq) {
		DRV_LOG(WARNING,
			"port %u cannot allocate memory for drop queue",
			dev->data->port_id);
		rte_errno = ENOMEM;
		goto error;
	}
	hrxq->ind_table = ind_tbl;
	hrxq->qp = qp;
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	hrxq->action = mlx5_glue->dv_create_flow_action_dest_ibv_qp(hrxq->qp);
	if (!hrxq->action) {
		rte_errno = errno;
		goto error;
	}
#endif
	priv->drop_queue.hrxq = hrxq;
	rte_atomic32_set(&hrxq->refcnt, 1);
	return hrxq;
error:
	if (ind_tbl)
		mlx5_ind_table_ibv_drop_release(dev);
	return NULL;
}

/**
 * Release a drop hash Rx queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
void
mlx5_hrxq_drop_release(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hrxq *hrxq = priv->drop_queue.hrxq;

	if (rte_atomic32_dec_and_test(&hrxq->refcnt)) {
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
		mlx5_glue->destroy_flow_action(hrxq->action);
#endif
		claim_zero(mlx5_glue->destroy_qp(hrxq->qp));
		mlx5_ind_table_ibv_drop_release(dev);
		rte_free(hrxq);
		priv->drop_queue.hrxq = NULL;
	}
}
