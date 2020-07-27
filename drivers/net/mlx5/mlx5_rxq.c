/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/queue.h>

#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_ethdev_driver.h>
#include <rte_common.h>
#include <rte_interrupts.h>
#include <rte_debug.h>
#include <rte_io.h>
#include <rte_eal_paging.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_malloc.h>

#include "mlx5_defs.h"
#include "mlx5.h"
#include "mlx5_common_os.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"
#include "mlx5_autoconf.h"
#include "mlx5_flow.h"


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
	uint32_t i;
	uint16_t n = 0;
	uint16_t n_ibv = 0;

	if (mlx5_check_mprq_support(dev) < 0)
		return 0;
	/* All the configured queues should be enabled. */
	for (i = 0; i < priv->rxqs_n; ++i) {
		struct mlx5_rxq_data *rxq = (*priv->rxqs)[i];
		struct mlx5_rxq_ctrl *rxq_ctrl = container_of
			(rxq, struct mlx5_rxq_ctrl, rxq);

		if (rxq == NULL || rxq_ctrl->type != MLX5_RXQ_TYPE_STANDARD)
			continue;
		n_ibv++;
		if (mlx5_rxq_mprq_enabled(rxq))
			++n;
	}
	/* Multi-Packet RQ can't be partially configured. */
	MLX5_ASSERT(n == 0 || n == n_ibv);
	return n == n_ibv;
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
		MLX5_ASSERT(DATA_OFF(buf) == RTE_PKTMBUF_HEADROOM);
		/* Buffer is supposed to be empty. */
		MLX5_ASSERT(rte_pktmbuf_data_len(buf) == 0);
		MLX5_ASSERT(rte_pktmbuf_pkt_len(buf) == 0);
		MLX5_ASSERT(!buf->next);
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
		struct rte_pktmbuf_pool_private *priv =
			(struct rte_pktmbuf_pool_private *)
				rte_mempool_get_priv(rxq_ctrl->rxq.mp);
		int j;

		/* Initialize default rearm_data for vPMD. */
		mbuf_init->data_off = RTE_PKTMBUF_HEADROOM;
		rte_mbuf_refcnt_set(mbuf_init, 1);
		mbuf_init->nb_segs = 1;
		mbuf_init->port = rxq->port_id;
		if (priv->flags & RTE_PKTMBUF_POOL_F_PINNED_EXT_BUF)
			mbuf_init->ol_flags = EXT_ATTACHED_MBUF;
		/*
		 * prevent compiler reordering:
		 * rearm_data covers previous fields.
		 */
		rte_compiler_barrier();
		rxq->mbuf_initializer =
			*(rte_xmm_t *)&mbuf_init->rearm_data;
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
	MLX5_ASSERT(mlx5_rxq_check_vec_support(rxq) < 0);
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
			     DEV_RX_OFFLOAD_JUMBO_FRAME |
			     DEV_RX_OFFLOAD_RSS_HASH);

	if (config->hw_fcs_strip)
		offloads |= DEV_RX_OFFLOAD_KEEP_CRC;

	if (config->hw_csum)
		offloads |= (DEV_RX_OFFLOAD_IPV4_CKSUM |
			     DEV_RX_OFFLOAD_UDP_CKSUM |
			     DEV_RX_OFFLOAD_TCP_CKSUM);
	if (config->hw_vlan_strip)
		offloads |= DEV_RX_OFFLOAD_VLAN_STRIP;
	if (MLX5_LRO_SUPPORTED(dev))
		offloads |= DEV_RX_OFFLOAD_TCP_LRO;
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
 * Verify if the queue can be released.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   RX queue index.
 *
 * @return
 *   1 if the queue can be released
 *   0 if the queue can not be released, there are references to it.
 *   Negative errno and rte_errno is set if queue doesn't exist.
 */
static int
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

/* Fetches and drops all SW-owned and error CQEs to synchronize CQ. */
static void
rxq_sync_cq(struct mlx5_rxq_data *rxq)
{
	const uint16_t cqe_n = 1 << rxq->cqe_n;
	const uint16_t cqe_mask = cqe_n - 1;
	volatile struct mlx5_cqe *cqe;
	int ret, i;

	i = cqe_n;
	do {
		cqe = &(*rxq->cqes)[rxq->cq_ci & cqe_mask];
		ret = check_cqe(cqe, cqe_n, rxq->cq_ci);
		if (ret == MLX5_CQE_STATUS_HW_OWN)
			break;
		if (ret == MLX5_CQE_STATUS_ERR) {
			rxq->cq_ci++;
			continue;
		}
		MLX5_ASSERT(ret == MLX5_CQE_STATUS_SW_OWN);
		if (MLX5_CQE_FORMAT(cqe->op_own) != MLX5_COMPRESSED) {
			rxq->cq_ci++;
			continue;
		}
		/* Compute the next non compressed CQE. */
		rxq->cq_ci += rte_be_to_cpu_32(cqe->byte_cnt);

	} while (--i);
	/* Move all CQEs to HW ownership, including possible MiniCQEs. */
	for (i = 0; i < cqe_n; i++) {
		cqe = &(*rxq->cqes)[i];
		cqe->op_own = MLX5_CQE_INVALIDATE;
	}
	/* Resync CQE and WQE (WQ in RESET state). */
	rte_cio_wmb();
	*rxq->cq_db = rte_cpu_to_be_32(rxq->cq_ci);
	rte_cio_wmb();
	*rxq->rq_db = rte_cpu_to_be_32(0);
	rte_cio_wmb();
}

/**
 * Rx queue stop. Device queue goes to the RESET state,
 * all involved mbufs are freed from WQ.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   RX queue index.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_rx_queue_stop_primary(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
			container_of(rxq, struct mlx5_rxq_ctrl, rxq);
	int ret;

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	if (rxq_ctrl->obj->type == MLX5_RXQ_OBJ_TYPE_IBV) {
		struct ibv_wq_attr mod = {
			.attr_mask = IBV_WQ_ATTR_STATE,
			.wq_state = IBV_WQS_RESET,
		};

		ret = mlx5_glue->modify_wq(rxq_ctrl->obj->wq, &mod);
	} else { /* rxq_ctrl->obj->type == MLX5_RXQ_OBJ_TYPE_DEVX_RQ. */
		struct mlx5_devx_modify_rq_attr rq_attr;

		memset(&rq_attr, 0, sizeof(rq_attr));
		rq_attr.rq_state = MLX5_RQC_STATE_RST;
		rq_attr.state = MLX5_RQC_STATE_RDY;
		ret = mlx5_devx_cmd_modify_rq(rxq_ctrl->obj->rq, &rq_attr);
	}
	if (ret) {
		DRV_LOG(ERR, "Cannot change Rx WQ state to RESET:  %s",
			strerror(errno));
		rte_errno = errno;
		return ret;
	}
	/* Remove all processes CQEs. */
	rxq_sync_cq(rxq);
	/* Free all involved mbufs. */
	rxq_free_elts(rxq_ctrl);
	/* Set the actual queue state. */
	dev->data->rx_queue_state[idx] = RTE_ETH_QUEUE_STATE_STOPPED;
	return 0;
}

/**
 * Rx queue stop. Device queue goes to the RESET state,
 * all involved mbufs are freed from WQ.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   RX queue index.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_rx_queue_stop(struct rte_eth_dev *dev, uint16_t idx)
{
	eth_rx_burst_t pkt_burst = dev->rx_pkt_burst;
	int ret;

	if (dev->data->rx_queue_state[idx] == RTE_ETH_QUEUE_STATE_HAIRPIN) {
		DRV_LOG(ERR, "Hairpin queue can't be stopped");
		rte_errno = EINVAL;
		return -EINVAL;
	}
	if (dev->data->rx_queue_state[idx] == RTE_ETH_QUEUE_STATE_STOPPED)
		return 0;
	/*
	 * Vectorized Rx burst requires the CQ and RQ indices
	 * synchronized, that might be broken on RQ restart
	 * and cause Rx malfunction, so queue stopping is
	 * not supported if vectorized Rx burst is engaged.
	 * The routine pointer depends on the process
	 * type, should perform check there.
	 */
	if (pkt_burst == mlx5_rx_burst) {
		DRV_LOG(ERR, "Rx queue stop is not supported "
			"for vectorized Rx");
		rte_errno = EINVAL;
		return -EINVAL;
	}
	if (rte_eal_process_type() ==  RTE_PROC_SECONDARY) {
		ret = mlx5_mp_os_req_queue_control(dev, idx,
						   MLX5_MP_REQ_QUEUE_RX_STOP);
	} else {
		ret = mlx5_rx_queue_stop_primary(dev, idx);
	}
	return ret;
}

/**
 * Rx queue start. Device queue goes to the ready state,
 * all required mbufs are allocated and WQ is replenished.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   RX queue index.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_rx_queue_start_primary(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
			container_of(rxq, struct mlx5_rxq_ctrl, rxq);
	int ret;

	MLX5_ASSERT(rte_eal_process_type() ==  RTE_PROC_PRIMARY);
	/* Allocate needed buffers. */
	ret = rxq_alloc_elts(rxq_ctrl);
	if (ret) {
		DRV_LOG(ERR, "Cannot reallocate buffers for Rx WQ");
		rte_errno = errno;
		return ret;
	}
	rte_cio_wmb();
	*rxq->cq_db = rte_cpu_to_be_32(rxq->cq_ci);
	rte_cio_wmb();
	/* Reset RQ consumer before moving queue ro READY state. */
	*rxq->rq_db = rte_cpu_to_be_32(0);
	rte_cio_wmb();
	if (rxq_ctrl->obj->type == MLX5_RXQ_OBJ_TYPE_IBV) {
		struct ibv_wq_attr mod = {
			.attr_mask = IBV_WQ_ATTR_STATE,
			.wq_state = IBV_WQS_RDY,
		};

		ret = mlx5_glue->modify_wq(rxq_ctrl->obj->wq, &mod);
	} else { /* rxq_ctrl->obj->type == MLX5_RXQ_OBJ_TYPE_DEVX_RQ. */
		struct mlx5_devx_modify_rq_attr rq_attr;

		memset(&rq_attr, 0, sizeof(rq_attr));
		rq_attr.rq_state = MLX5_RQC_STATE_RDY;
		rq_attr.state = MLX5_RQC_STATE_RST;
		ret = mlx5_devx_cmd_modify_rq(rxq_ctrl->obj->rq, &rq_attr);
	}
	if (ret) {
		DRV_LOG(ERR, "Cannot change Rx WQ state to READY:  %s",
			strerror(errno));
		rte_errno = errno;
		return ret;
	}
	/* Reinitialize RQ - set WQEs. */
	mlx5_rxq_initialize(rxq);
	rxq->err_state = MLX5_RXQ_ERR_STATE_NO_ERROR;
	/* Set actual queue state. */
	dev->data->rx_queue_state[idx] = RTE_ETH_QUEUE_STATE_STARTED;
	return 0;
}

/**
 * Rx queue start. Device queue goes to the ready state,
 * all required mbufs are allocated and WQ is replenished.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   RX queue index.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_rx_queue_start(struct rte_eth_dev *dev, uint16_t idx)
{
	int ret;

	if (dev->data->rx_queue_state[idx] == RTE_ETH_QUEUE_STATE_HAIRPIN) {
		DRV_LOG(ERR, "Hairpin queue can't be started");
		rte_errno = EINVAL;
		return -EINVAL;
	}
	if (dev->data->rx_queue_state[idx] == RTE_ETH_QUEUE_STATE_STARTED)
		return 0;
	if (rte_eal_process_type() ==  RTE_PROC_SECONDARY) {
		ret = mlx5_mp_os_req_queue_control(dev, idx,
						   MLX5_MP_REQ_QUEUE_RX_START);
	} else {
		ret = mlx5_rx_queue_start_primary(dev, idx);
	}
	return ret;
}

/**
 * Rx queue presetup checks.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   RX queue index.
 * @param desc
 *   Number of descriptors to configure in queue.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_rx_queue_pre_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t *desc)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (!rte_is_power_of_2(*desc)) {
		*desc = 1 << log2above(*desc);
		DRV_LOG(WARNING,
			"port %u increased number of descriptors in Rx queue %u"
			" to the next power of two (%d)",
			dev->data->port_id, idx, *desc);
	}
	DRV_LOG(DEBUG, "port %u configuring Rx queue %u for %u descriptors",
		dev->data->port_id, idx, *desc);
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
	return 0;
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
	int res;

	res = mlx5_rx_queue_pre_setup(dev, idx, &desc);
	if (res)
		return res;
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
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   RX queue index.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param hairpin_conf
 *   Hairpin configuration parameters.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_rx_hairpin_queue_setup(struct rte_eth_dev *dev, uint16_t idx,
			    uint16_t desc,
			    const struct rte_eth_hairpin_conf *hairpin_conf)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq, struct mlx5_rxq_ctrl, rxq);
	int res;

	res = mlx5_rx_queue_pre_setup(dev, idx, &desc);
	if (res)
		return res;
	if (hairpin_conf->peer_count != 1 ||
	    hairpin_conf->peers[0].port != dev->data->port_id ||
	    hairpin_conf->peers[0].queue >= priv->txqs_n) {
		DRV_LOG(ERR, "port %u unable to setup hairpin queue index %u "
			" invalid hairpind configuration", dev->data->port_id,
			idx);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	rxq_ctrl = mlx5_rxq_hairpin_new(dev, idx, desc, hairpin_conf);
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
 * Get an Rx queue Verbs/DevX object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Rx queue array
 *
 * @return
 *   The Verbs/DevX object if it exists.
 */
static struct mlx5_rxq_obj *
mlx5_rxq_obj_get(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl;

	if (idx >= priv->rxqs_n)
		return NULL;
	if (!rxq_data)
		return NULL;
	rxq_ctrl = container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	if (rxq_ctrl->obj)
		rte_atomic32_inc(&rxq_ctrl->obj->refcnt);
	return rxq_ctrl->obj;
}

/**
 * Release the resources allocated for an RQ DevX object.
 *
 * @param rxq_ctrl
 *   DevX Rx queue object.
 */
static void
rxq_release_devx_rq_resources(struct mlx5_rxq_ctrl *rxq_ctrl)
{
	if (rxq_ctrl->rxq.wqes) {
		mlx5_free((void *)(uintptr_t)rxq_ctrl->rxq.wqes);
		rxq_ctrl->rxq.wqes = NULL;
	}
	if (rxq_ctrl->wq_umem) {
		mlx5_glue->devx_umem_dereg(rxq_ctrl->wq_umem);
		rxq_ctrl->wq_umem = NULL;
	}
}

/**
 * Release the resources allocated for the Rx CQ DevX object.
 *
 * @param rxq_ctrl
 *   DevX Rx queue object.
 */
static void
rxq_release_devx_cq_resources(struct mlx5_rxq_ctrl *rxq_ctrl)
{
	if (rxq_ctrl->rxq.cqes) {
		rte_free((void *)(uintptr_t)rxq_ctrl->rxq.cqes);
		rxq_ctrl->rxq.cqes = NULL;
	}
	if (rxq_ctrl->cq_umem) {
		mlx5_glue->devx_umem_dereg(rxq_ctrl->cq_umem);
		rxq_ctrl->cq_umem = NULL;
	}
}

/**
 * Release an Rx hairpin related resources.
 *
 * @param rxq_obj
 *   Hairpin Rx queue object.
 */
static void
rxq_obj_hairpin_release(struct mlx5_rxq_obj *rxq_obj)
{
	struct mlx5_devx_modify_rq_attr rq_attr = { 0 };

	MLX5_ASSERT(rxq_obj);
	rq_attr.state = MLX5_RQC_STATE_RST;
	rq_attr.rq_state = MLX5_RQC_STATE_RDY;
	mlx5_devx_cmd_modify_rq(rxq_obj->rq, &rq_attr);
	claim_zero(mlx5_devx_cmd_destroy(rxq_obj->rq));
}

/**
 * Release an Rx verbs/DevX queue object.
 *
 * @param rxq_obj
 *   Verbs/DevX Rx queue object.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
static int
mlx5_rxq_obj_release(struct mlx5_rxq_obj *rxq_obj)
{
	MLX5_ASSERT(rxq_obj);
	if (rte_atomic32_dec_and_test(&rxq_obj->refcnt)) {
		switch (rxq_obj->type) {
		case MLX5_RXQ_OBJ_TYPE_IBV:
			MLX5_ASSERT(rxq_obj->wq);
			MLX5_ASSERT(rxq_obj->ibv_cq);
			rxq_free_elts(rxq_obj->rxq_ctrl);
			claim_zero(mlx5_glue->destroy_wq(rxq_obj->wq));
			claim_zero(mlx5_glue->destroy_cq(rxq_obj->ibv_cq));
			if (rxq_obj->ibv_channel)
				claim_zero(mlx5_glue->destroy_comp_channel
					   (rxq_obj->ibv_channel));
			break;
		case MLX5_RXQ_OBJ_TYPE_DEVX_RQ:
			MLX5_ASSERT(rxq_obj->rq);
			MLX5_ASSERT(rxq_obj->devx_cq);
			rxq_free_elts(rxq_obj->rxq_ctrl);
			claim_zero(mlx5_devx_cmd_destroy(rxq_obj->rq));
			claim_zero(mlx5_devx_cmd_destroy(rxq_obj->devx_cq));
			if (rxq_obj->devx_channel)
				mlx5_glue->devx_destroy_event_channel
							(rxq_obj->devx_channel);
			rxq_release_devx_rq_resources(rxq_obj->rxq_ctrl);
			rxq_release_devx_cq_resources(rxq_obj->rxq_ctrl);
			break;
		case MLX5_RXQ_OBJ_TYPE_DEVX_HAIRPIN:
			MLX5_ASSERT(rxq_obj->rq);
			rxq_obj_hairpin_release(rxq_obj);
			break;
		}
		LIST_REMOVE(rxq_obj, next);
		mlx5_free(rxq_obj);
		return 0;
	}
	return 1;
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
	intr_handle->intr_vec = mlx5_malloc(0,
				n * sizeof(intr_handle->intr_vec[0]),
				0, SOCKET_ID_ANY);
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
		/* This rxq obj must not be released in this function. */
		struct mlx5_rxq_obj *rxq_obj = mlx5_rxq_obj_get(dev, i);
		int rc;

		/* Skip queues that cannot request interrupts. */
		if (!rxq_obj || (!rxq_obj->ibv_channel &&
				 !rxq_obj->devx_channel)) {
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
		rc = mlx5_os_set_nonblock_channel_fd(rxq_obj->fd);
		if (rc < 0) {
			rte_errno = errno;
			DRV_LOG(ERR,
				"port %u failed to make Rx interrupt file"
				" descriptor %d non-blocking for queue index"
				" %d",
				dev->data->port_id, rxq_obj->fd, i);
			mlx5_rx_intr_vec_disable(dev);
			return -rte_errno;
		}
		intr_handle->intr_vec[i] = RTE_INTR_VEC_RXTX_OFFSET + count;
		intr_handle->efds[count] = rxq_obj->fd;
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
		 * kept in mlx5_rx_intr_vec_enable().
		 */
		rxq_data = (*priv->rxqs)[i];
		rxq_ctrl = container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
		if (rxq_ctrl->obj)
			mlx5_rxq_obj_release(rxq_ctrl->obj);
	}
free:
	rte_intr_free_epoll_fd(intr_handle);
	if (intr_handle->intr_vec)
		mlx5_free(intr_handle->intr_vec);
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
	doorbell |= rxq->cqn;
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
		struct mlx5_rxq_obj *rxq_obj;

		rxq_obj = mlx5_rxq_obj_get(dev, rx_queue_id);
		if (!rxq_obj) {
			rte_errno = EINVAL;
			return -rte_errno;
		}
		mlx5_arm_cq(rxq_data, rxq_data->cq_arm_sn);
		mlx5_rxq_obj_release(rxq_obj);
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
	struct mlx5_rxq_obj *rxq_obj = NULL;
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
	rxq_obj = mlx5_rxq_obj_get(dev, rx_queue_id);
	if (!rxq_obj) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (rxq_obj->type == MLX5_RXQ_OBJ_TYPE_IBV) {
		ret = mlx5_glue->get_cq_event(rxq_obj->ibv_channel, &ev_cq,
					      &ev_ctx);
		if (ret < 0 || ev_cq != rxq_obj->ibv_cq)
			goto exit;
		mlx5_glue->ack_cq_events(rxq_obj->ibv_cq, 1);
	} else if (rxq_obj->type == MLX5_RXQ_OBJ_TYPE_DEVX_RQ) {
#ifdef HAVE_IBV_DEVX_EVENT
		union {
			struct mlx5dv_devx_async_event_hdr event_resp;
			uint8_t buf[sizeof(struct mlx5dv_devx_async_event_hdr)
				    + 128];
		} out;

		ret = mlx5_glue->devx_get_event
				(rxq_obj->devx_channel, &out.event_resp,
				 sizeof(out.buf));
		if (ret < 0 || out.event_resp.cookie !=
				(uint64_t)(uintptr_t)rxq_obj->devx_cq)
			goto exit;
#endif /* HAVE_IBV_DEVX_EVENT */
	}
	rxq_data->cq_arm_sn++;
	mlx5_rxq_obj_release(rxq_obj);
	return 0;
exit:
	/**
	 * For ret < 0 save the errno (may be EAGAIN which means the get_event
	 * function was called before receiving one).
	 */
	if (ret < 0)
		rte_errno = errno;
	else
		rte_errno = EINVAL;
	ret = rte_errno; /* Save rte_errno before cleanup. */
	if (rxq_obj)
		mlx5_rxq_obj_release(rxq_obj);
	if (ret != EAGAIN)
		DRV_LOG(WARNING, "port %u unable to disable interrupt on Rx queue %d",
			dev->data->port_id, rx_queue_id);
	rte_errno = ret; /* Restore rte_errno. */
	return -rte_errno;
}

/**
 * Create a CQ Verbs object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param priv
 *   Pointer to device private data.
 * @param rxq_data
 *   Pointer to Rx queue data.
 * @param cqe_n
 *   Number of CQEs in CQ.
 * @param rxq_obj
 *   Pointer to Rx queue object data.
 *
 * @return
 *   The Verbs object initialised, NULL otherwise and rte_errno is set.
 */
static struct ibv_cq *
mlx5_ibv_cq_new(struct rte_eth_dev *dev, struct mlx5_priv *priv,
		struct mlx5_rxq_data *rxq_data,
		unsigned int cqe_n, struct mlx5_rxq_obj *rxq_obj)
{
	struct {
		struct ibv_cq_init_attr_ex ibv;
		struct mlx5dv_cq_init_attr mlx5;
	} cq_attr;

	cq_attr.ibv = (struct ibv_cq_init_attr_ex){
		.cqe = cqe_n,
		.channel = rxq_obj->ibv_channel,
		.comp_mask = 0,
	};
	cq_attr.mlx5 = (struct mlx5dv_cq_init_attr){
		.comp_mask = 0,
	};
	if (priv->config.cqe_comp && !rxq_data->hw_timestamp &&
	    !rxq_data->lro) {
		cq_attr.mlx5.comp_mask |=
				MLX5DV_CQ_INIT_ATTR_MASK_COMPRESSED_CQE;
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
		cq_attr.mlx5.cqe_comp_res_format =
				mlx5_rxq_mprq_enabled(rxq_data) ?
				MLX5DV_CQE_RES_FORMAT_CSUM_STRIDX :
				MLX5DV_CQE_RES_FORMAT_HASH;
#else
		cq_attr.mlx5.cqe_comp_res_format = MLX5DV_CQE_RES_FORMAT_HASH;
#endif
		/*
		 * For vectorized Rx, it must not be doubled in order to
		 * make cq_ci and rq_ci aligned.
		 */
		if (mlx5_rxq_check_vec_support(rxq_data) < 0)
			cq_attr.ibv.cqe *= 2;
	} else if (priv->config.cqe_comp && rxq_data->hw_timestamp) {
		DRV_LOG(DEBUG,
			"port %u Rx CQE compression is disabled for HW"
			" timestamp",
			dev->data->port_id);
	} else if (priv->config.cqe_comp && rxq_data->lro) {
		DRV_LOG(DEBUG,
			"port %u Rx CQE compression is disabled for LRO",
			dev->data->port_id);
	}
#ifdef HAVE_IBV_MLX5_MOD_CQE_128B_PAD
	if (priv->config.cqe_pad) {
		cq_attr.mlx5.comp_mask |= MLX5DV_CQ_INIT_ATTR_MASK_FLAGS;
		cq_attr.mlx5.flags |= MLX5DV_CQ_INIT_ATTR_FLAGS_CQE_PAD;
	}
#endif
	return mlx5_glue->cq_ex_to_cq(mlx5_glue->dv_create_cq(priv->sh->ctx,
							      &cq_attr.ibv,
							      &cq_attr.mlx5));
}

/**
 * Create a WQ Verbs object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param priv
 *   Pointer to device private data.
 * @param rxq_data
 *   Pointer to Rx queue data.
 * @param idx
 *   Queue index in DPDK Rx queue array
 * @param wqe_n
 *   Number of WQEs in WQ.
 * @param rxq_obj
 *   Pointer to Rx queue object data.
 *
 * @return
 *   The Verbs object initialised, NULL otherwise and rte_errno is set.
 */
static struct ibv_wq *
mlx5_ibv_wq_new(struct rte_eth_dev *dev, struct mlx5_priv *priv,
		struct mlx5_rxq_data *rxq_data, uint16_t idx,
		unsigned int wqe_n, struct mlx5_rxq_obj *rxq_obj)
{
	struct {
		struct ibv_wq_init_attr ibv;
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
		struct mlx5dv_wq_init_attr mlx5;
#endif
	} wq_attr;

	wq_attr.ibv = (struct ibv_wq_init_attr){
		.wq_context = NULL, /* Could be useful in the future. */
		.wq_type = IBV_WQT_RQ,
		/* Max number of outstanding WRs. */
		.max_wr = wqe_n >> rxq_data->sges_n,
		/* Max number of scatter/gather elements in a WR. */
		.max_sge = 1 << rxq_data->sges_n,
		.pd = priv->sh->pd,
		.cq = rxq_obj->ibv_cq,
		.comp_mask = IBV_WQ_FLAGS_CVLAN_STRIPPING | 0,
		.create_flags = (rxq_data->vlan_strip ?
				 IBV_WQ_FLAGS_CVLAN_STRIPPING : 0),
	};
	/* By default, FCS (CRC) is stripped by hardware. */
	if (rxq_data->crc_present) {
		wq_attr.ibv.create_flags |= IBV_WQ_FLAGS_SCATTER_FCS;
		wq_attr.ibv.comp_mask |= IBV_WQ_INIT_ATTR_FLAGS;
	}
	if (priv->config.hw_padding) {
#if defined(HAVE_IBV_WQ_FLAG_RX_END_PADDING)
		wq_attr.ibv.create_flags |= IBV_WQ_FLAG_RX_END_PADDING;
		wq_attr.ibv.comp_mask |= IBV_WQ_INIT_ATTR_FLAGS;
#elif defined(HAVE_IBV_WQ_FLAGS_PCI_WRITE_END_PADDING)
		wq_attr.ibv.create_flags |= IBV_WQ_FLAGS_PCI_WRITE_END_PADDING;
		wq_attr.ibv.comp_mask |= IBV_WQ_INIT_ATTR_FLAGS;
#endif
	}
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
	wq_attr.mlx5 = (struct mlx5dv_wq_init_attr){
		.comp_mask = 0,
	};
	if (mlx5_rxq_mprq_enabled(rxq_data)) {
		struct mlx5dv_striding_rq_init_attr *mprq_attr =
						&wq_attr.mlx5.striding_rq_attrs;

		wq_attr.mlx5.comp_mask |= MLX5DV_WQ_INIT_ATTR_MASK_STRIDING_RQ;
		*mprq_attr = (struct mlx5dv_striding_rq_init_attr){
			.single_stride_log_num_of_bytes = rxq_data->strd_sz_n,
			.single_wqe_log_num_of_strides = rxq_data->strd_num_n,
			.two_byte_shift_en = MLX5_MPRQ_TWO_BYTE_SHIFT,
		};
	}
	rxq_obj->wq = mlx5_glue->dv_create_wq(priv->sh->ctx, &wq_attr.ibv,
					      &wq_attr.mlx5);
#else
	rxq_obj->wq = mlx5_glue->create_wq(priv->sh->ctx, &wq_attr.ibv);
#endif
	if (rxq_obj->wq) {
		/*
		 * Make sure number of WRs*SGEs match expectations since a queue
		 * cannot allocate more than "desc" buffers.
		 */
		if (wq_attr.ibv.max_wr != (wqe_n >> rxq_data->sges_n) ||
		    wq_attr.ibv.max_sge != (1u << rxq_data->sges_n)) {
			DRV_LOG(ERR,
				"port %u Rx queue %u requested %u*%u but got"
				" %u*%u WRs*SGEs",
				dev->data->port_id, idx,
				wqe_n >> rxq_data->sges_n,
				(1 << rxq_data->sges_n),
				wq_attr.ibv.max_wr, wq_attr.ibv.max_sge);
			claim_zero(mlx5_glue->destroy_wq(rxq_obj->wq));
			rxq_obj->wq = NULL;
			rte_errno = EINVAL;
		}
	}
	return rxq_obj->wq;
}

/**
 * Fill common fields of create RQ attributes structure.
 *
 * @param rxq_data
 *   Pointer to Rx queue data.
 * @param cqn
 *   CQ number to use with this RQ.
 * @param rq_attr
 *   RQ attributes structure to fill..
 */
static void
mlx5_devx_create_rq_attr_fill(struct mlx5_rxq_data *rxq_data, uint32_t cqn,
			      struct mlx5_devx_create_rq_attr *rq_attr)
{
	rq_attr->state = MLX5_RQC_STATE_RST;
	rq_attr->vsd = (rxq_data->vlan_strip) ? 0 : 1;
	rq_attr->cqn = cqn;
	rq_attr->scatter_fcs = (rxq_data->crc_present) ? 1 : 0;
}

/**
 * Fill common fields of DevX WQ attributes structure.
 *
 * @param priv
 *   Pointer to device private data.
 * @param rxq_ctrl
 *   Pointer to Rx queue control structure.
 * @param wq_attr
 *   WQ attributes structure to fill..
 */
static void
mlx5_devx_wq_attr_fill(struct mlx5_priv *priv, struct mlx5_rxq_ctrl *rxq_ctrl,
		       struct mlx5_devx_wq_attr *wq_attr)
{
	wq_attr->end_padding_mode = priv->config.cqe_pad ?
					MLX5_WQ_END_PAD_MODE_ALIGN :
					MLX5_WQ_END_PAD_MODE_NONE;
	wq_attr->pd = priv->sh->pdn;
	wq_attr->dbr_addr = rxq_ctrl->rq_dbr_offset;
	wq_attr->dbr_umem_id = rxq_ctrl->rq_dbr_umem_id;
	wq_attr->dbr_umem_valid = 1;
	wq_attr->wq_umem_id = rxq_ctrl->wq_umem->umem_id;
	wq_attr->wq_umem_valid = 1;
}

/**
 * Create a RQ object using DevX.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Rx queue array
 * @param cqn
 *   CQ number to use with this RQ.
 *
 * @return
 *   The DevX object initialised, NULL otherwise and rte_errno is set.
 */
static struct mlx5_devx_obj *
mlx5_devx_rq_new(struct rte_eth_dev *dev, uint16_t idx, uint32_t cqn)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	struct mlx5_devx_create_rq_attr rq_attr = { 0 };
	uint32_t wqe_n = 1 << (rxq_data->elts_n - rxq_data->sges_n);
	uint32_t wq_size = 0;
	uint32_t wqe_size = 0;
	uint32_t log_wqe_size = 0;
	void *buf = NULL;
	struct mlx5_devx_obj *rq;

	/* Fill RQ attributes. */
	rq_attr.mem_rq_type = MLX5_RQC_MEM_RQ_TYPE_MEMORY_RQ_INLINE;
	rq_attr.flush_in_error_en = 1;
	mlx5_devx_create_rq_attr_fill(rxq_data, cqn, &rq_attr);
	/* Fill WQ attributes for this RQ. */
	if (mlx5_rxq_mprq_enabled(rxq_data)) {
		rq_attr.wq_attr.wq_type = MLX5_WQ_TYPE_CYCLIC_STRIDING_RQ;
		/*
		 * Number of strides in each WQE:
		 * 512*2^single_wqe_log_num_of_strides.
		 */
		rq_attr.wq_attr.single_wqe_log_num_of_strides =
				rxq_data->strd_num_n -
				MLX5_MIN_SINGLE_WQE_LOG_NUM_STRIDES;
		/* Stride size = (2^single_stride_log_num_of_bytes)*64B. */
		rq_attr.wq_attr.single_stride_log_num_of_bytes =
				rxq_data->strd_sz_n -
				MLX5_MIN_SINGLE_STRIDE_LOG_NUM_BYTES;
		wqe_size = sizeof(struct mlx5_wqe_mprq);
	} else {
		rq_attr.wq_attr.wq_type = MLX5_WQ_TYPE_CYCLIC;
		wqe_size = sizeof(struct mlx5_wqe_data_seg);
	}
	log_wqe_size = log2above(wqe_size) + rxq_data->sges_n;
	rq_attr.wq_attr.log_wq_stride = log_wqe_size;
	rq_attr.wq_attr.log_wq_sz = rxq_data->elts_n - rxq_data->sges_n;
	/* Calculate and allocate WQ memory space. */
	wqe_size = 1 << log_wqe_size; /* round up power of two.*/
	wq_size = wqe_n * wqe_size;
	size_t alignment = MLX5_WQE_BUF_ALIGNMENT;
	if (alignment == (size_t)-1) {
		DRV_LOG(ERR, "Failed to get mem page size");
		rte_errno = ENOMEM;
		return NULL;
	}
	buf = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO, wq_size,
			  alignment, rxq_ctrl->socket);
	if (!buf)
		return NULL;
	rxq_data->wqes = buf;
	rxq_ctrl->wq_umem = mlx5_glue->devx_umem_reg(priv->sh->ctx,
						     buf, wq_size, 0);
	if (!rxq_ctrl->wq_umem) {
		mlx5_free(buf);
		return NULL;
	}
	mlx5_devx_wq_attr_fill(priv, rxq_ctrl, &rq_attr.wq_attr);
	rq = mlx5_devx_cmd_create_rq(priv->sh->ctx, &rq_attr, rxq_ctrl->socket);
	if (!rq)
		rxq_release_devx_rq_resources(rxq_ctrl);
	return rq;
}

/**
 * Create a DevX CQ object for an Rx queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param cqe_n
 *   Number of CQEs in CQ.
 * @param idx
 *   Queue index in DPDK Rx queue array
 * @param rxq_obj
 *   Pointer to Rx queue object data.
 *
 * @return
 *   The DevX object initialised, NULL otherwise and rte_errno is set.
 */
static struct mlx5_devx_obj *
mlx5_devx_cq_new(struct rte_eth_dev *dev, unsigned int cqe_n, uint16_t idx,
		 struct mlx5_rxq_obj *rxq_obj)
{
	struct mlx5_devx_obj *cq_obj = 0;
	struct mlx5_devx_cq_attr cq_attr = { 0 };
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	size_t page_size = rte_mem_page_size();
	uint32_t lcore = (uint32_t)rte_lcore_to_cpu_id(-1);
	uint32_t eqn = 0;
	void *buf = NULL;
	uint16_t event_nums[1] = {0};
	uint32_t log_cqe_n;
	uint32_t cq_size;
	int ret = 0;

	if (page_size == (size_t)-1) {
		DRV_LOG(ERR, "Failed to get page_size.");
		goto error;
	}
	if (priv->config.cqe_comp && !rxq_data->hw_timestamp &&
	    !rxq_data->lro) {
		cq_attr.cqe_comp_en = MLX5DV_CQ_INIT_ATTR_MASK_COMPRESSED_CQE;
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
		cq_attr.mini_cqe_res_format =
				mlx5_rxq_mprq_enabled(rxq_data) ?
				MLX5DV_CQE_RES_FORMAT_CSUM_STRIDX :
				MLX5DV_CQE_RES_FORMAT_HASH;
#else
		cq_attr.mini_cqe_res_format = MLX5DV_CQE_RES_FORMAT_HASH;
#endif
		/*
		 * For vectorized Rx, it must not be doubled in order to
		 * make cq_ci and rq_ci aligned.
		 */
		if (mlx5_rxq_check_vec_support(rxq_data) < 0)
			cqe_n *= 2;
	} else if (priv->config.cqe_comp && rxq_data->hw_timestamp) {
		DRV_LOG(DEBUG,
			"port %u Rx CQE compression is disabled for HW"
			" timestamp",
			dev->data->port_id);
	} else if (priv->config.cqe_comp && rxq_data->lro) {
		DRV_LOG(DEBUG,
			"port %u Rx CQE compression is disabled for LRO",
			dev->data->port_id);
	}
#ifdef HAVE_IBV_MLX5_MOD_CQE_128B_PAD
	if (priv->config.cqe_pad)
		cq_attr.cqe_size = MLX5DV_CQ_INIT_ATTR_FLAGS_CQE_PAD;
#endif
	log_cqe_n = log2above(cqe_n);
	cq_size = sizeof(struct mlx5_cqe) * (1 << log_cqe_n);
	/* Query the EQN for this core. */
	if (mlx5_glue->devx_query_eqn(priv->sh->ctx, lcore, &eqn)) {
		DRV_LOG(ERR, "Failed to query EQN for CQ.");
		goto error;
	}
	cq_attr.eqn = eqn;
	buf = rte_calloc_socket(__func__, 1, cq_size, page_size,
				rxq_ctrl->socket);
	if (!buf) {
		DRV_LOG(ERR, "Failed to allocate memory for CQ.");
		goto error;
	}
	rxq_data->cqes = (volatile struct mlx5_cqe (*)[])(uintptr_t)buf;
	rxq_ctrl->cq_umem = mlx5_glue->devx_umem_reg(priv->sh->ctx, buf,
						     cq_size,
						     IBV_ACCESS_LOCAL_WRITE);
	if (!rxq_ctrl->cq_umem) {
		DRV_LOG(ERR, "Failed to register umem for CQ.");
		goto error;
	}
	cq_attr.uar_page_id = priv->sh->devx_rx_uar->page_id;
	cq_attr.q_umem_id = rxq_ctrl->cq_umem->umem_id;
	cq_attr.q_umem_valid = 1;
	cq_attr.log_cq_size = log_cqe_n;
	cq_attr.log_page_size = rte_log2_u32(page_size);
	cq_attr.db_umem_offset = rxq_ctrl->cq_dbr_offset;
	cq_attr.db_umem_id = rxq_ctrl->cq_dbr_umem_id;
	cq_attr.db_umem_valid = rxq_ctrl->cq_dbr_umem_id_valid;
	cq_obj = mlx5_devx_cmd_create_cq(priv->sh->ctx, &cq_attr);
	if (!cq_obj)
		goto error;
	rxq_data->cqe_n = log_cqe_n;
	rxq_data->cqn = cq_obj->id;
	if (rxq_obj->devx_channel) {
		ret = mlx5_glue->devx_subscribe_devx_event
						(rxq_obj->devx_channel,
						 cq_obj->obj,
						 sizeof(event_nums),
						 event_nums,
						 (uint64_t)(uintptr_t)cq_obj);
		if (ret) {
			DRV_LOG(ERR, "Fail to subscribe CQ to event channel.");
			rte_errno = errno;
			goto error;
		}
	}
	/* Initialise CQ to 1's to mark HW ownership for all CQEs. */
	memset((void *)(uintptr_t)rxq_data->cqes, 0xFF, cq_size);
	return cq_obj;
error:
	if (cq_obj)
		mlx5_devx_cmd_destroy(cq_obj);
	rxq_release_devx_cq_resources(rxq_ctrl);
	return NULL;
}

/**
 * Create the Rx hairpin queue object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Rx queue array
 *
 * @return
 *   The hairpin DevX object initialised, NULL otherwise and rte_errno is set.
 */
static struct mlx5_rxq_obj *
mlx5_rxq_obj_hairpin_new(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	struct mlx5_devx_create_rq_attr attr = { 0 };
	struct mlx5_rxq_obj *tmpl = NULL;
	uint32_t max_wq_data;

	MLX5_ASSERT(rxq_data);
	MLX5_ASSERT(!rxq_ctrl->obj);
	tmpl = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO, sizeof(*tmpl), 0,
			   rxq_ctrl->socket);
	if (!tmpl) {
		DRV_LOG(ERR,
			"port %u Rx queue %u cannot allocate verbs resources",
			dev->data->port_id, rxq_data->idx);
		rte_errno = ENOMEM;
		return NULL;
	}
	tmpl->type = MLX5_RXQ_OBJ_TYPE_DEVX_HAIRPIN;
	tmpl->rxq_ctrl = rxq_ctrl;
	attr.hairpin = 1;
	max_wq_data = priv->config.hca_attr.log_max_hairpin_wq_data_sz;
	/* Jumbo frames > 9KB should be supported, and more packets. */
	if (priv->config.log_hp_size != (uint32_t)MLX5_ARG_UNSET) {
		if (priv->config.log_hp_size > max_wq_data) {
			DRV_LOG(ERR, "total data size %u power of 2 is "
				"too large for hairpin",
				priv->config.log_hp_size);
			mlx5_free(tmpl);
			rte_errno = ERANGE;
			return NULL;
		}
		attr.wq_attr.log_hairpin_data_sz = priv->config.log_hp_size;
	} else {
		attr.wq_attr.log_hairpin_data_sz =
				(max_wq_data < MLX5_HAIRPIN_JUMBO_LOG_SIZE) ?
				 max_wq_data : MLX5_HAIRPIN_JUMBO_LOG_SIZE;
	}
	/* Set the packets number to the maximum value for performance. */
	attr.wq_attr.log_hairpin_num_packets =
			attr.wq_attr.log_hairpin_data_sz -
			MLX5_HAIRPIN_QUEUE_STRIDE;
	tmpl->rq = mlx5_devx_cmd_create_rq(priv->sh->ctx, &attr,
					   rxq_ctrl->socket);
	if (!tmpl->rq) {
		DRV_LOG(ERR,
			"port %u Rx hairpin queue %u can't create rq object",
			dev->data->port_id, idx);
		mlx5_free(tmpl);
		rte_errno = errno;
		return NULL;
	}
	DRV_LOG(DEBUG, "port %u rxq %u updated with %p", dev->data->port_id,
		idx, (void *)&tmpl);
	rte_atomic32_inc(&tmpl->refcnt);
	LIST_INSERT_HEAD(&priv->rxqsobj, tmpl, next);
	priv->verbs_alloc_ctx.type = MLX5_VERBS_ALLOC_TYPE_NONE;
	dev->data->rx_queue_state[idx] = RTE_ETH_QUEUE_STATE_HAIRPIN;
	return tmpl;
}

/**
 * Create the Rx queue Verbs/DevX object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Rx queue array
 * @param type
 *   Type of Rx queue object to create.
 *
 * @return
 *   The Verbs/DevX object initialised, NULL otherwise and rte_errno is set.
 */
struct mlx5_rxq_obj *
mlx5_rxq_obj_new(struct rte_eth_dev *dev, uint16_t idx,
		 enum mlx5_rxq_obj_type type)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	struct ibv_wq_attr mod;
	unsigned int cqe_n;
	unsigned int wqe_n = 1 << rxq_data->elts_n;
	struct mlx5_rxq_obj *tmpl = NULL;
	struct mlx5dv_cq cq_info;
	struct mlx5dv_rwq rwq;
	int ret = 0;
	struct mlx5dv_obj obj;

	MLX5_ASSERT(rxq_data);
	MLX5_ASSERT(!rxq_ctrl->obj);
	if (type == MLX5_RXQ_OBJ_TYPE_DEVX_HAIRPIN)
		return mlx5_rxq_obj_hairpin_new(dev, idx);
	priv->verbs_alloc_ctx.type = MLX5_VERBS_ALLOC_TYPE_RX_QUEUE;
	priv->verbs_alloc_ctx.obj = rxq_ctrl;
	tmpl = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO, sizeof(*tmpl), 0,
			   rxq_ctrl->socket);
	if (!tmpl) {
		DRV_LOG(ERR,
			"port %u Rx queue %u cannot allocate resources",
			dev->data->port_id, rxq_data->idx);
		rte_errno = ENOMEM;
		goto error;
	}
	tmpl->type = type;
	tmpl->rxq_ctrl = rxq_ctrl;
	if (rxq_ctrl->irq) {
		if (tmpl->type == MLX5_RXQ_OBJ_TYPE_IBV) {
			tmpl->ibv_channel =
				mlx5_glue->create_comp_channel(priv->sh->ctx);
			if (!tmpl->ibv_channel) {
				DRV_LOG(ERR, "port %u: comp channel creation "
					"failure", dev->data->port_id);
				rte_errno = ENOMEM;
				goto error;
			}
			tmpl->fd = tmpl->ibv_channel->fd;
		} else if (tmpl->type == MLX5_RXQ_OBJ_TYPE_DEVX_RQ) {
			int devx_ev_flag =
			  MLX5DV_DEVX_CREATE_EVENT_CHANNEL_FLAGS_OMIT_EV_DATA;

			tmpl->devx_channel =
				mlx5_glue->devx_create_event_channel
								(priv->sh->ctx,
								 devx_ev_flag);
			if (!tmpl->devx_channel) {
				rte_errno = errno;
				DRV_LOG(ERR,
					"Failed to create event channel %d.",
					rte_errno);
				goto error;
			}
			tmpl->fd = tmpl->devx_channel->fd;
		}
	}
	if (mlx5_rxq_mprq_enabled(rxq_data))
		cqe_n = wqe_n * (1 << rxq_data->strd_num_n) - 1;
	else
		cqe_n = wqe_n - 1;
	DRV_LOG(DEBUG, "port %u device_attr.max_qp_wr is %d",
		dev->data->port_id, priv->sh->device_attr.max_qp_wr);
	DRV_LOG(DEBUG, "port %u device_attr.max_sge is %d",
		dev->data->port_id, priv->sh->device_attr.max_sge);
	if (tmpl->type == MLX5_RXQ_OBJ_TYPE_IBV) {
		/* Create CQ using Verbs API. */
		tmpl->ibv_cq = mlx5_ibv_cq_new(dev, priv, rxq_data, cqe_n,
					       tmpl);
		if (!tmpl->ibv_cq) {
			DRV_LOG(ERR, "port %u Rx queue %u CQ creation failure",
				dev->data->port_id, idx);
			rte_errno = ENOMEM;
			goto error;
		}
		obj.cq.in = tmpl->ibv_cq;
		obj.cq.out = &cq_info;
		ret = mlx5_glue->dv_init_obj(&obj, MLX5DV_OBJ_CQ);
		if (ret) {
			rte_errno = ret;
			goto error;
		}
		if (cq_info.cqe_size != RTE_CACHE_LINE_SIZE) {
			DRV_LOG(ERR,
				"port %u wrong MLX5_CQE_SIZE environment "
				"variable value: it should be set to %u",
				dev->data->port_id, RTE_CACHE_LINE_SIZE);
			rte_errno = EINVAL;
			goto error;
		}
		/* Fill the rings. */
		rxq_data->cqe_n = log2above(cq_info.cqe_cnt);
		rxq_data->cq_db = cq_info.dbrec;
		rxq_data->cqes =
			(volatile struct mlx5_cqe (*)[])(uintptr_t)cq_info.buf;
		rxq_data->cq_uar = cq_info.cq_uar;
		rxq_data->cqn = cq_info.cqn;
		/* Create WQ (RQ) using Verbs API. */
		tmpl->wq = mlx5_ibv_wq_new(dev, priv, rxq_data, idx, wqe_n,
					   tmpl);
		if (!tmpl->wq) {
			DRV_LOG(ERR, "port %u Rx queue %u WQ creation failure",
				dev->data->port_id, idx);
			rte_errno = ENOMEM;
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
				"port %u Rx queue %u WQ state to IBV_WQS_RDY"
				" failed", dev->data->port_id, idx);
			rte_errno = ret;
			goto error;
		}
		obj.rwq.in = tmpl->wq;
		obj.rwq.out = &rwq;
		ret = mlx5_glue->dv_init_obj(&obj, MLX5DV_OBJ_RWQ);
		if (ret) {
			rte_errno = ret;
			goto error;
		}
		rxq_data->wqes = rwq.buf;
		rxq_data->rq_db = rwq.dbrec;
	} else if (tmpl->type == MLX5_RXQ_OBJ_TYPE_DEVX_RQ) {
		struct mlx5_devx_modify_rq_attr rq_attr = { 0 };
		struct mlx5_devx_dbr_page *dbr_page;
		int64_t dbr_offset;

		/* Allocate CQ door-bell. */
		dbr_offset = mlx5_get_dbr(priv->sh->ctx, &priv->dbrpgs,
					  &dbr_page);
		if (dbr_offset < 0) {
			DRV_LOG(ERR, "Failed to allocate CQ door-bell.");
			goto error;
		}
		rxq_ctrl->cq_dbr_offset = dbr_offset;
		rxq_ctrl->cq_dbr_umem_id = mlx5_os_get_umem_id(dbr_page->umem);
		rxq_ctrl->cq_dbr_umem_id_valid = 1;
		rxq_data->cq_db =
			(uint32_t *)((uintptr_t)dbr_page->dbrs +
				     (uintptr_t)rxq_ctrl->cq_dbr_offset);
		rxq_data->cq_uar = priv->sh->devx_rx_uar->base_addr;
		/* Create CQ using DevX API. */
		tmpl->devx_cq = mlx5_devx_cq_new(dev, cqe_n, idx, tmpl);
		if (!tmpl->devx_cq) {
			DRV_LOG(ERR, "Failed to create CQ.");
			goto error;
		}
		/* Allocate RQ door-bell. */
		dbr_offset = mlx5_get_dbr(priv->sh->ctx, &priv->dbrpgs,
					  &dbr_page);
		if (dbr_offset < 0) {
			DRV_LOG(ERR, "Failed to allocate RQ door-bell.");
			goto error;
		}
		rxq_ctrl->rq_dbr_offset = dbr_offset;
		rxq_ctrl->rq_dbr_umem_id = mlx5_os_get_umem_id(dbr_page->umem);
		rxq_ctrl->rq_dbr_umem_id_valid = 1;
		rxq_data->rq_db =
			(uint32_t *)((uintptr_t)dbr_page->dbrs +
				     (uintptr_t)rxq_ctrl->rq_dbr_offset);
		/* Create RQ using DevX API. */
		tmpl->rq = mlx5_devx_rq_new(dev, idx, tmpl->devx_cq->id);
		if (!tmpl->rq) {
			DRV_LOG(ERR, "port %u Rx queue %u RQ creation failure",
				dev->data->port_id, idx);
			rte_errno = ENOMEM;
			goto error;
		}
		/* Change queue state to ready. */
		rq_attr.rq_state = MLX5_RQC_STATE_RST;
		rq_attr.state = MLX5_RQC_STATE_RDY;
		ret = mlx5_devx_cmd_modify_rq(tmpl->rq, &rq_attr);
		if (ret)
			goto error;
	}
	rxq_data->cq_arm_sn = 0;
	mlx5_rxq_initialize(rxq_data);
	rxq_data->cq_ci = 0;
	DRV_LOG(DEBUG, "port %u rxq %u updated with %p", dev->data->port_id,
		idx, (void *)&tmpl);
	rte_atomic32_inc(&tmpl->refcnt);
	LIST_INSERT_HEAD(&priv->rxqsobj, tmpl, next);
	priv->verbs_alloc_ctx.type = MLX5_VERBS_ALLOC_TYPE_NONE;
	dev->data->rx_queue_state[idx] = RTE_ETH_QUEUE_STATE_STARTED;
	return tmpl;
error:
	if (tmpl) {
		ret = rte_errno; /* Save rte_errno before cleanup. */
		if (tmpl->type == MLX5_RXQ_OBJ_TYPE_IBV) {
			if (tmpl->wq)
				claim_zero(mlx5_glue->destroy_wq(tmpl->wq));
			if (tmpl->ibv_cq)
				claim_zero(mlx5_glue->destroy_cq(tmpl->ibv_cq));
			if (tmpl->ibv_channel)
				claim_zero(mlx5_glue->destroy_comp_channel
							(tmpl->ibv_channel));
		} else if (tmpl->type == MLX5_RXQ_OBJ_TYPE_DEVX_RQ) {
			if (tmpl->rq)
				claim_zero(mlx5_devx_cmd_destroy(tmpl->rq));
			if (tmpl->devx_cq)
				claim_zero(mlx5_devx_cmd_destroy
							(tmpl->devx_cq));
			if (tmpl->devx_channel)
				mlx5_glue->devx_destroy_event_channel
							(tmpl->devx_channel);
		}
		mlx5_free(tmpl);
		rte_errno = ret; /* Restore rte_errno. */
	}
	if (type == MLX5_RXQ_OBJ_TYPE_DEVX_RQ) {
		rxq_release_devx_rq_resources(rxq_ctrl);
		rxq_release_devx_cq_resources(rxq_ctrl);
	}
	priv->verbs_alloc_ctx.type = MLX5_VERBS_ALLOC_TYPE_NONE;
	return NULL;
}

/**
 * Verify the Rx queue objects list is empty
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   The number of objects not released.
 */
int
mlx5_rxq_obj_verify(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret = 0;
	struct mlx5_rxq_obj *rxq_obj;

	LIST_FOREACH(rxq_obj, &priv->rxqsobj, next) {
		DRV_LOG(DEBUG, "port %u Rx queue %u still referenced",
			dev->data->port_id, rxq_obj->rxq_ctrl->rxq.idx);
		++ret;
	}
	return ret;
}

/**
 * Callback function to initialize mbufs for Multi-Packet RQ.
 */
static inline void
mlx5_mprq_buf_init(struct rte_mempool *mp, void *opaque_arg,
		    void *_m, unsigned int i __rte_unused)
{
	struct mlx5_mprq_buf *buf = _m;
	struct rte_mbuf_ext_shared_info *shinfo;
	unsigned int strd_n = (unsigned int)(uintptr_t)opaque_arg;
	unsigned int j;

	memset(_m, 0, sizeof(*buf));
	buf->mp = mp;
	rte_atomic16_set(&buf->refcnt, 1);
	for (j = 0; j != strd_n; ++j) {
		shinfo = &buf->shinfos[j];
		shinfo->free_cb = mlx5_mprq_buf_free_cb;
		shinfo->fcb_opaque = buf;
	}
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
	 * is still in use by application, destroying the Rx queue can spoil
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
	unsigned int n_ibv = 0;

	if (!mlx5_mprq_enabled(dev))
		return 0;
	/* Count the total number of descriptors configured. */
	for (i = 0; i != priv->rxqs_n; ++i) {
		struct mlx5_rxq_data *rxq = (*priv->rxqs)[i];
		struct mlx5_rxq_ctrl *rxq_ctrl = container_of
			(rxq, struct mlx5_rxq_ctrl, rxq);

		if (rxq == NULL || rxq_ctrl->type != MLX5_RXQ_TYPE_STANDARD)
			continue;
		n_ibv++;
		desc += 1 << rxq->elts_n;
		/* Get the max number of strides. */
		if (strd_num_n < rxq->strd_num_n)
			strd_num_n = rxq->strd_num_n;
		/* Get the max size of a stride. */
		if (strd_sz_n < rxq->strd_sz_n)
			strd_sz_n = rxq->strd_sz_n;
	}
	MLX5_ASSERT(strd_num_n && strd_sz_n);
	buf_len = (1 << strd_num_n) * (1 << strd_sz_n);
	obj_size = sizeof(struct mlx5_mprq_buf) + buf_len + (1 << strd_num_n) *
		sizeof(struct rte_mbuf_ext_shared_info) + RTE_PKTMBUF_HEADROOM;
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
	obj_num = desc + MLX5_MPRQ_MP_CACHE_SZ * n_ibv;
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
				0, NULL, NULL, mlx5_mprq_buf_init,
				(void *)(uintptr_t)(1 << strd_num_n),
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
		struct mlx5_rxq_ctrl *rxq_ctrl = container_of
			(rxq, struct mlx5_rxq_ctrl, rxq);

		if (rxq == NULL || rxq_ctrl->type != MLX5_RXQ_TYPE_STANDARD)
			continue;
		rxq->mprq_mp = mp;
	}
	DRV_LOG(INFO, "port %u Multi-Packet RQ is configured",
		dev->data->port_id);
	return 0;
}

#define MLX5_MAX_TCP_HDR_OFFSET ((unsigned int)(sizeof(struct rte_ether_hdr) + \
					sizeof(struct rte_vlan_hdr) * 2 + \
					sizeof(struct rte_ipv6_hdr)))
#define MAX_TCP_OPTION_SIZE 40u
#define MLX5_MAX_LRO_HEADER_FIX ((unsigned int)(MLX5_MAX_TCP_HDR_OFFSET + \
				 sizeof(struct rte_tcp_hdr) + \
				 MAX_TCP_OPTION_SIZE))

/**
 * Adjust the maximum LRO massage size.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   RX queue index.
 * @param max_lro_size
 *   The maximum size for LRO packet.
 */
static void
mlx5_max_lro_msg_size_adjust(struct rte_eth_dev *dev, uint16_t idx,
			     uint32_t max_lro_size)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (priv->config.hca_attr.lro_max_msg_sz_mode ==
	    MLX5_LRO_MAX_MSG_SIZE_START_FROM_L4 && max_lro_size >
	    MLX5_MAX_TCP_HDR_OFFSET)
		max_lro_size -= MLX5_MAX_TCP_HDR_OFFSET;
	max_lro_size = RTE_MIN(max_lro_size, MLX5_MAX_LRO_SIZE);
	MLX5_ASSERT(max_lro_size >= MLX5_LRO_SEG_CHUNK_SIZE);
	max_lro_size /= MLX5_LRO_SEG_CHUNK_SIZE;
	if (priv->max_lro_msg_size)
		priv->max_lro_msg_size =
			RTE_MIN((uint32_t)priv->max_lro_msg_size, max_lro_size);
	else
		priv->max_lro_msg_size = max_lro_size;
	DRV_LOG(DEBUG,
		"port %u Rx Queue %u max LRO message size adjusted to %u bytes",
		dev->data->port_id, idx,
		priv->max_lro_msg_size * MLX5_LRO_SEG_CHUNK_SIZE);
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
	unsigned int mprq_stride_nums;
	unsigned int mprq_stride_size;
	unsigned int mprq_stride_cap;
	struct mlx5_dev_config *config = &priv->config;
	/*
	 * Always allocate extra slots, even if eventually
	 * the vector Rx will not be used.
	 */
	uint16_t desc_n =
		desc + config->rx_vec_en * MLX5_VPMD_DESCS_PER_LOOP;
	uint64_t offloads = conf->offloads |
			   dev->data->dev_conf.rxmode.offloads;
	unsigned int lro_on_queue = !!(offloads & DEV_RX_OFFLOAD_TCP_LRO);
	const int mprq_en = mlx5_check_mprq_support(dev) > 0;
	unsigned int max_rx_pkt_len = lro_on_queue ?
			dev->data->dev_conf.rxmode.max_lro_pkt_size :
			dev->data->dev_conf.rxmode.max_rx_pkt_len;
	unsigned int non_scatter_min_mbuf_size = max_rx_pkt_len +
							RTE_PKTMBUF_HEADROOM;
	unsigned int max_lro_size = 0;
	unsigned int first_mb_free_size = mb_len - RTE_PKTMBUF_HEADROOM;

	if (non_scatter_min_mbuf_size > mb_len && !(offloads &
						    DEV_RX_OFFLOAD_SCATTER)) {
		DRV_LOG(ERR, "port %u Rx queue %u: Scatter offload is not"
			" configured and no enough mbuf space(%u) to contain "
			"the maximum RX packet length(%u) with head-room(%u)",
			dev->data->port_id, idx, mb_len, max_rx_pkt_len,
			RTE_PKTMBUF_HEADROOM);
		rte_errno = ENOSPC;
		return NULL;
	}
	tmpl = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO, sizeof(*tmpl) +
			   desc_n * sizeof(struct rte_mbuf *), 0, socket);
	if (!tmpl) {
		rte_errno = ENOMEM;
		return NULL;
	}
	tmpl->type = MLX5_RXQ_TYPE_STANDARD;
	if (mlx5_mr_btree_init(&tmpl->rxq.mr_ctrl.cache_bh,
			       MLX5_MR_BTREE_CACHE_N, socket)) {
		/* rte_errno is already set. */
		goto error;
	}
	tmpl->socket = socket;
	if (dev->data->dev_conf.intr_conf.rxq)
		tmpl->irq = 1;
	mprq_stride_nums = config->mprq.stride_num_n ?
		config->mprq.stride_num_n : MLX5_MPRQ_STRIDE_NUM_N;
	mprq_stride_size = non_scatter_min_mbuf_size <=
		(1U << config->mprq.max_stride_size_n) ?
		log2above(non_scatter_min_mbuf_size) : MLX5_MPRQ_STRIDE_SIZE_N;
	mprq_stride_cap = (config->mprq.stride_num_n ?
		(1U << config->mprq.stride_num_n) : (1U << mprq_stride_nums)) *
			(config->mprq.stride_size_n ?
		(1U << config->mprq.stride_size_n) : (1U << mprq_stride_size));
	/*
	 * This Rx queue can be configured as a Multi-Packet RQ if all of the
	 * following conditions are met:
	 *  - MPRQ is enabled.
	 *  - The number of descs is more than the number of strides.
	 *  - max_rx_pkt_len plus overhead is less than the max size
	 *    of a stride or mprq_stride_size is specified by a user.
	 *    Need to nake sure that there are enough stides to encap
	 *    the maximum packet size in case mprq_stride_size is set.
	 *  Otherwise, enable Rx scatter if necessary.
	 */
	if (mprq_en && desc > (1U << mprq_stride_nums) &&
	    (non_scatter_min_mbuf_size <=
	     (1U << config->mprq.max_stride_size_n) ||
	     (config->mprq.stride_size_n &&
	      non_scatter_min_mbuf_size <= mprq_stride_cap))) {
		/* TODO: Rx scatter isn't supported yet. */
		tmpl->rxq.sges_n = 0;
		/* Trim the number of descs needed. */
		desc >>= mprq_stride_nums;
		tmpl->rxq.strd_num_n = config->mprq.stride_num_n ?
			config->mprq.stride_num_n : mprq_stride_nums;
		tmpl->rxq.strd_sz_n = config->mprq.stride_size_n ?
			config->mprq.stride_size_n : mprq_stride_size;
		tmpl->rxq.strd_shift_en = MLX5_MPRQ_TWO_BYTE_SHIFT;
		tmpl->rxq.strd_scatter_en =
				!!(offloads & DEV_RX_OFFLOAD_SCATTER);
		tmpl->rxq.mprq_max_memcpy_len = RTE_MIN(first_mb_free_size,
				config->mprq.max_memcpy_len);
		max_lro_size = RTE_MIN(max_rx_pkt_len,
				       (1u << tmpl->rxq.strd_num_n) *
				       (1u << tmpl->rxq.strd_sz_n));
		DRV_LOG(DEBUG,
			"port %u Rx queue %u: Multi-Packet RQ is enabled"
			" strd_num_n = %u, strd_sz_n = %u",
			dev->data->port_id, idx,
			tmpl->rxq.strd_num_n, tmpl->rxq.strd_sz_n);
	} else if (max_rx_pkt_len <= first_mb_free_size) {
		tmpl->rxq.sges_n = 0;
		max_lro_size = max_rx_pkt_len;
	} else if (offloads & DEV_RX_OFFLOAD_SCATTER) {
		unsigned int size = non_scatter_min_mbuf_size;
		unsigned int sges_n;

		if (lro_on_queue && first_mb_free_size <
		    MLX5_MAX_LRO_HEADER_FIX) {
			DRV_LOG(ERR, "Not enough space in the first segment(%u)"
				" to include the max header size(%u) for LRO",
				first_mb_free_size, MLX5_MAX_LRO_HEADER_FIX);
			rte_errno = ENOTSUP;
			goto error;
		}
		/*
		 * Determine the number of SGEs needed for a full packet
		 * and round it to the next power of two.
		 */
		sges_n = log2above((size / mb_len) + !!(size % mb_len));
		if (sges_n > MLX5_MAX_LOG_RQ_SEGS) {
			DRV_LOG(ERR,
				"port %u too many SGEs (%u) needed to handle"
				" requested maximum packet size %u, the maximum"
				" supported are %u", dev->data->port_id,
				1 << sges_n, max_rx_pkt_len,
				1u << MLX5_MAX_LOG_RQ_SEGS);
			rte_errno = ENOTSUP;
			goto error;
		}
		tmpl->rxq.sges_n = sges_n;
		max_lro_size = max_rx_pkt_len;
	}
	if (config->mprq.enabled && !mlx5_rxq_mprq_enabled(&tmpl->rxq))
		DRV_LOG(WARNING,
			"port %u MPRQ is requested but cannot be enabled\n"
			" (requested: pkt_sz = %u, desc_num = %u,"
			" rxq_num = %u, stride_sz = %u, stride_num = %u\n"
			"  supported: min_rxqs_num = %u,"
			" min_stride_sz = %u, max_stride_sz = %u).",
			dev->data->port_id, non_scatter_min_mbuf_size,
			desc, priv->rxqs_n,
			config->mprq.stride_size_n ?
				(1U << config->mprq.stride_size_n) :
				(1U << mprq_stride_size),
			config->mprq.stride_num_n ?
				(1U << config->mprq.stride_num_n) :
				(1U << mprq_stride_nums),
			config->mprq.min_rxqs_num,
			(1U << config->mprq.min_stride_size_n),
			(1U << config->mprq.max_stride_size_n));
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
	mlx5_max_lro_msg_size_adjust(dev, idx, max_lro_size);
	/* Toggle RX checksum offload if hardware supports it. */
	tmpl->rxq.csum = !!(offloads & DEV_RX_OFFLOAD_CHECKSUM);
	tmpl->rxq.hw_timestamp = !!(offloads & DEV_RX_OFFLOAD_TIMESTAMP);
	/* Configure VLAN stripping. */
	tmpl->rxq.vlan_strip = !!(offloads & DEV_RX_OFFLOAD_VLAN_STRIP);
	/* By default, FCS (CRC) is stripped by hardware. */
	tmpl->rxq.crc_present = 0;
	tmpl->rxq.lro = lro_on_queue;
	if (offloads & DEV_RX_OFFLOAD_KEEP_CRC) {
		if (config->hw_fcs_strip) {
			/*
			 * RQs used for LRO-enabled TIRs should not be
			 * configured to scatter the FCS.
			 */
			if (lro_on_queue)
				DRV_LOG(WARNING,
					"port %u CRC stripping has been "
					"disabled but will still be performed "
					"by hardware, because LRO is enabled",
					dev->data->port_id);
			else
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
	tmpl->rxq.uar_lock_cq = &priv->sh->uar_lock_cq;
#endif
	tmpl->rxq.idx = idx;
	rte_atomic32_inc(&tmpl->refcnt);
	LIST_INSERT_HEAD(&priv->rxqsctrl, tmpl, next);
	return tmpl;
error:
	mlx5_free(tmpl);
	return NULL;
}

/**
 * Create a DPDK Rx hairpin queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   RX queue index.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param hairpin_conf
 *   The hairpin binding configuration.
 *
 * @return
 *   A DPDK queue object on success, NULL otherwise and rte_errno is set.
 */
struct mlx5_rxq_ctrl *
mlx5_rxq_hairpin_new(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
		     const struct rte_eth_hairpin_conf *hairpin_conf)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_ctrl *tmpl;

	tmpl = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO, sizeof(*tmpl), 0,
			   SOCKET_ID_ANY);
	if (!tmpl) {
		rte_errno = ENOMEM;
		return NULL;
	}
	tmpl->type = MLX5_RXQ_TYPE_HAIRPIN;
	tmpl->socket = SOCKET_ID_ANY;
	tmpl->rxq.rss_hash = 0;
	tmpl->rxq.port_id = dev->data->port_id;
	tmpl->priv = priv;
	tmpl->rxq.mp = NULL;
	tmpl->rxq.elts_n = log2above(desc);
	tmpl->rxq.elts = NULL;
	tmpl->rxq.mr_ctrl.cache_bh = (struct mlx5_mr_btree) { 0 };
	tmpl->hairpin_conf = *hairpin_conf;
	tmpl->rxq.idx = idx;
	rte_atomic32_inc(&tmpl->refcnt);
	LIST_INSERT_HEAD(&priv->rxqsctrl, tmpl, next);
	return tmpl;
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
		mlx5_rxq_obj_get(dev, idx);
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
	MLX5_ASSERT(rxq_ctrl->priv);
	if (rxq_ctrl->obj && !mlx5_rxq_obj_release(rxq_ctrl->obj))
		rxq_ctrl->obj = NULL;
	if (rte_atomic32_dec_and_test(&rxq_ctrl->refcnt)) {
		if (rxq_ctrl->rq_dbr_umem_id_valid)
			claim_zero(mlx5_release_dbr(&priv->dbrpgs,
						    rxq_ctrl->rq_dbr_umem_id,
						    rxq_ctrl->rq_dbr_offset));
		if (rxq_ctrl->cq_dbr_umem_id_valid)
			claim_zero(mlx5_release_dbr(&priv->dbrpgs,
						    rxq_ctrl->cq_dbr_umem_id,
						    rxq_ctrl->cq_dbr_offset));
		if (rxq_ctrl->type == MLX5_RXQ_TYPE_STANDARD)
			mlx5_mr_btree_free(&rxq_ctrl->rxq.mr_ctrl.cache_bh);
		LIST_REMOVE(rxq_ctrl, next);
		mlx5_free(rxq_ctrl);
		(*priv->rxqs)[idx] = NULL;
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
 * Get a Rx queue type.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Rx queue index.
 *
 * @return
 *   The Rx queue type.
 */
enum mlx5_rxq_type
mlx5_rxq_get_type(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_ctrl *rxq_ctrl = NULL;

	if (idx < priv->rxqs_n && (*priv->rxqs)[idx]) {
		rxq_ctrl = container_of((*priv->rxqs)[idx],
					struct mlx5_rxq_ctrl,
					rxq);
		return rxq_ctrl->type;
	}
	return MLX5_RXQ_TYPE_UNDEFINED;
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
 *   The Verbs/DevX object initialised, NULL otherwise and rte_errno is set.
 */
static struct mlx5_ind_table_obj *
mlx5_ind_table_obj_new(struct rte_eth_dev *dev, const uint16_t *queues,
		       uint32_t queues_n, enum mlx5_ind_tbl_type type)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ind_table_obj *ind_tbl;
	unsigned int i = 0, j = 0, k = 0;

	ind_tbl = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*ind_tbl) +
			      queues_n * sizeof(uint16_t), 0, SOCKET_ID_ANY);
	if (!ind_tbl) {
		rte_errno = ENOMEM;
		return NULL;
	}
	ind_tbl->type = type;
	if (ind_tbl->type == MLX5_IND_TBL_TYPE_IBV) {
		const unsigned int wq_n = rte_is_power_of_2(queues_n) ?
			log2above(queues_n) :
			log2above(priv->config.ind_table_max_size);
		struct ibv_wq *wq[1 << wq_n];

		for (i = 0; i != queues_n; ++i) {
			struct mlx5_rxq_ctrl *rxq = mlx5_rxq_get(dev,
								 queues[i]);
			if (!rxq)
				goto error;
			wq[i] = rxq->obj->wq;
			ind_tbl->queues[i] = queues[i];
		}
		ind_tbl->queues_n = queues_n;
		/* Finalise indirection table. */
		k = i; /* Retain value of i for use in error case. */
		for (j = 0; k != (unsigned int)(1 << wq_n); ++k, ++j)
			wq[k] = wq[j];
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
	} else { /* ind_tbl->type == MLX5_IND_TBL_TYPE_DEVX */
		struct mlx5_devx_rqt_attr *rqt_attr = NULL;
		const unsigned int rqt_n =
			1 << (rte_is_power_of_2(queues_n) ?
			      log2above(queues_n) :
			      log2above(priv->config.ind_table_max_size));

		rqt_attr = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*rqt_attr) +
				      rqt_n * sizeof(uint32_t), 0,
				      SOCKET_ID_ANY);
		if (!rqt_attr) {
			DRV_LOG(ERR, "port %u cannot allocate RQT resources",
				dev->data->port_id);
			rte_errno = ENOMEM;
			goto error;
		}
		rqt_attr->rqt_max_size = priv->config.ind_table_max_size;
		rqt_attr->rqt_actual_size = rqt_n;
		for (i = 0; i != queues_n; ++i) {
			struct mlx5_rxq_ctrl *rxq = mlx5_rxq_get(dev,
								 queues[i]);
			if (!rxq)
				goto error;
			rqt_attr->rq_list[i] = rxq->obj->rq->id;
			ind_tbl->queues[i] = queues[i];
		}
		k = i; /* Retain value of i for use in error case. */
		for (j = 0; k != rqt_n; ++k, ++j)
			rqt_attr->rq_list[k] = rqt_attr->rq_list[j];
		ind_tbl->rqt = mlx5_devx_cmd_create_rqt(priv->sh->ctx,
							rqt_attr);
		mlx5_free(rqt_attr);
		if (!ind_tbl->rqt) {
			DRV_LOG(ERR, "port %u cannot create DevX RQT",
				dev->data->port_id);
			rte_errno = errno;
			goto error;
		}
		ind_tbl->queues_n = queues_n;
	}
	rte_atomic32_inc(&ind_tbl->refcnt);
	LIST_INSERT_HEAD(&priv->ind_tbls, ind_tbl, next);
	return ind_tbl;
error:
	for (j = 0; j < i; j++)
		mlx5_rxq_release(dev, ind_tbl->queues[j]);
	mlx5_free(ind_tbl);
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
static struct mlx5_ind_table_obj *
mlx5_ind_table_obj_get(struct rte_eth_dev *dev, const uint16_t *queues,
		       uint32_t queues_n)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ind_table_obj *ind_tbl;

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
static int
mlx5_ind_table_obj_release(struct rte_eth_dev *dev,
			   struct mlx5_ind_table_obj *ind_tbl)
{
	unsigned int i;

	if (rte_atomic32_dec_and_test(&ind_tbl->refcnt)) {
		if (ind_tbl->type == MLX5_IND_TBL_TYPE_IBV)
			claim_zero(mlx5_glue->destroy_rwq_ind_table
							(ind_tbl->ind_table));
		else if (ind_tbl->type == MLX5_IND_TBL_TYPE_DEVX)
			claim_zero(mlx5_devx_cmd_destroy(ind_tbl->rqt));
	}
	for (i = 0; i != ind_tbl->queues_n; ++i)
		claim_nonzero(mlx5_rxq_release(dev, ind_tbl->queues[i]));
	if (!rte_atomic32_read(&ind_tbl->refcnt)) {
		LIST_REMOVE(ind_tbl, next);
		mlx5_free(ind_tbl);
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
mlx5_ind_table_obj_verify(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ind_table_obj *ind_tbl;
	int ret = 0;

	LIST_FOREACH(ind_tbl, &priv->ind_tbls, next) {
		DRV_LOG(DEBUG,
			"port %u indirection table obj %p still referenced",
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
 *   The Verbs/DevX object initialised index, 0 otherwise and rte_errno is set.
 */
uint32_t
mlx5_hrxq_new(struct rte_eth_dev *dev,
	      const uint8_t *rss_key, uint32_t rss_key_len,
	      uint64_t hash_fields,
	      const uint16_t *queues, uint32_t queues_n,
	      int tunnel __rte_unused)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hrxq *hrxq;
	uint32_t hrxq_idx = 0;
	struct ibv_qp *qp = NULL;
	struct mlx5_ind_table_obj *ind_tbl;
	int err;
	struct mlx5_devx_obj *tir = NULL;
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[queues[0]];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);

	queues_n = hash_fields ? queues_n : 1;
	ind_tbl = mlx5_ind_table_obj_get(dev, queues, queues_n);
	if (!ind_tbl) {
		enum mlx5_ind_tbl_type type;

		type = rxq_ctrl->obj->type == MLX5_RXQ_OBJ_TYPE_IBV ?
				MLX5_IND_TBL_TYPE_IBV : MLX5_IND_TBL_TYPE_DEVX;
		ind_tbl = mlx5_ind_table_obj_new(dev, queues, queues_n, type);
	}
	if (!ind_tbl) {
		rte_errno = ENOMEM;
		return 0;
	}
	if (ind_tbl->type == MLX5_IND_TBL_TYPE_IBV) {
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
		struct mlx5dv_qp_init_attr qp_init_attr;

		memset(&qp_init_attr, 0, sizeof(qp_init_attr));
		if (tunnel) {
			qp_init_attr.comp_mask =
				MLX5DV_QP_INIT_ATTR_MASK_QP_CREATE_FLAGS;
			qp_init_attr.create_flags =
				MLX5DV_QP_CREATE_TUNNEL_OFFLOADS;
		}
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
		if (dev->data->dev_conf.lpbk_mode) {
			/*
			 * Allow packet sent from NIC loop back
			 * w/o source MAC check.
			 */
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
					.rx_hash_function =
						IBV_RX_HASH_FUNC_TOEPLITZ,
					.rx_hash_key_len = rss_key_len,
					.rx_hash_key =
						(void *)(uintptr_t)rss_key,
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
					.rx_hash_function =
						IBV_RX_HASH_FUNC_TOEPLITZ,
					.rx_hash_key_len = rss_key_len,
					.rx_hash_key =
						(void *)(uintptr_t)rss_key,
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
	} else { /* ind_tbl->type == MLX5_IND_TBL_TYPE_DEVX */
		struct mlx5_devx_tir_attr tir_attr;
		uint32_t i;
		uint32_t lro = 1;

		/* Enable TIR LRO only if all the queues were configured for. */
		for (i = 0; i < queues_n; ++i) {
			if (!(*priv->rxqs)[queues[i]]->lro) {
				lro = 0;
				break;
			}
		}
		memset(&tir_attr, 0, sizeof(tir_attr));
		tir_attr.disp_type = MLX5_TIRC_DISP_TYPE_INDIRECT;
		tir_attr.rx_hash_fn = MLX5_RX_HASH_FN_TOEPLITZ;
		tir_attr.tunneled_offload_en = !!tunnel;
		/* If needed, translate hash_fields bitmap to PRM format. */
		if (hash_fields) {
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
			struct mlx5_rx_hash_field_select *rx_hash_field_select =
					hash_fields & IBV_RX_HASH_INNER ?
					&tir_attr.rx_hash_field_selector_inner :
					&tir_attr.rx_hash_field_selector_outer;
#else
			struct mlx5_rx_hash_field_select *rx_hash_field_select =
					&tir_attr.rx_hash_field_selector_outer;
#endif

			/* 1 bit: 0: IPv4, 1: IPv6. */
			rx_hash_field_select->l3_prot_type =
				!!(hash_fields & MLX5_IPV6_IBV_RX_HASH);
			/* 1 bit: 0: TCP, 1: UDP. */
			rx_hash_field_select->l4_prot_type =
				!!(hash_fields & MLX5_UDP_IBV_RX_HASH);
			/* Bitmask which sets which fields to use in RX Hash. */
			rx_hash_field_select->selected_fields =
			((!!(hash_fields & MLX5_L3_SRC_IBV_RX_HASH)) <<
			 MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_SRC_IP) |
			(!!(hash_fields & MLX5_L3_DST_IBV_RX_HASH)) <<
			 MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_DST_IP |
			(!!(hash_fields & MLX5_L4_SRC_IBV_RX_HASH)) <<
			 MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_L4_SPORT |
			(!!(hash_fields & MLX5_L4_DST_IBV_RX_HASH)) <<
			 MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_L4_DPORT;
		}
		if (rxq_ctrl->obj->type == MLX5_RXQ_OBJ_TYPE_DEVX_HAIRPIN)
			tir_attr.transport_domain = priv->sh->td->id;
		else
			tir_attr.transport_domain = priv->sh->tdn;
		memcpy(tir_attr.rx_hash_toeplitz_key, rss_key,
		       MLX5_RSS_HASH_KEY_LEN);
		tir_attr.indirect_table = ind_tbl->rqt->id;
		if (dev->data->dev_conf.lpbk_mode)
			tir_attr.self_lb_block =
					MLX5_TIRC_SELF_LB_BLOCK_BLOCK_UNICAST;
		if (lro) {
			tir_attr.lro_timeout_period_usecs =
					priv->config.lro.timeout;
			tir_attr.lro_max_msg_sz = priv->max_lro_msg_size;
			tir_attr.lro_enable_mask =
					MLX5_TIRC_LRO_ENABLE_MASK_IPV4_LRO |
					MLX5_TIRC_LRO_ENABLE_MASK_IPV6_LRO;
		}
		tir = mlx5_devx_cmd_create_tir(priv->sh->ctx, &tir_attr);
		if (!tir) {
			DRV_LOG(ERR, "port %u cannot create DevX TIR",
				dev->data->port_id);
			rte_errno = errno;
			goto error;
		}
	}
	hrxq = mlx5_ipool_zmalloc(priv->sh->ipool[MLX5_IPOOL_HRXQ], &hrxq_idx);
	if (!hrxq)
		goto error;
	hrxq->ind_table = ind_tbl;
	if (ind_tbl->type == MLX5_IND_TBL_TYPE_IBV) {
		hrxq->qp = qp;
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
		hrxq->action =
			mlx5_glue->dv_create_flow_action_dest_ibv_qp(hrxq->qp);
		if (!hrxq->action) {
			rte_errno = errno;
			goto error;
		}
#endif
	} else { /* ind_tbl->type == MLX5_IND_TBL_TYPE_DEVX */
		hrxq->tir = tir;
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
		hrxq->action = mlx5_glue->dv_create_flow_action_dest_devx_tir
							(hrxq->tir->obj);
		if (!hrxq->action) {
			rte_errno = errno;
			goto error;
		}
#endif
	}
	hrxq->rss_key_len = rss_key_len;
	hrxq->hash_fields = hash_fields;
	memcpy(hrxq->rss_key, rss_key, rss_key_len);
	rte_atomic32_inc(&hrxq->refcnt);
	ILIST_INSERT(priv->sh->ipool[MLX5_IPOOL_HRXQ], &priv->hrxqs, hrxq_idx,
		     hrxq, next);
	return hrxq_idx;
error:
	err = rte_errno; /* Save rte_errno before cleanup. */
	mlx5_ind_table_obj_release(dev, ind_tbl);
	if (qp)
		claim_zero(mlx5_glue->destroy_qp(qp));
	else if (tir)
		claim_zero(mlx5_devx_cmd_destroy(tir));
	rte_errno = err; /* Restore rte_errno. */
	return 0;
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
 *   An hash Rx queue index on success.
 */
uint32_t
mlx5_hrxq_get(struct rte_eth_dev *dev,
	      const uint8_t *rss_key, uint32_t rss_key_len,
	      uint64_t hash_fields,
	      const uint16_t *queues, uint32_t queues_n)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hrxq *hrxq;
	uint32_t idx;

	queues_n = hash_fields ? queues_n : 1;
	ILIST_FOREACH(priv->sh->ipool[MLX5_IPOOL_HRXQ], priv->hrxqs, idx,
		      hrxq, next) {
		struct mlx5_ind_table_obj *ind_tbl;

		if (hrxq->rss_key_len != rss_key_len)
			continue;
		if (memcmp(hrxq->rss_key, rss_key, rss_key_len))
			continue;
		if (hrxq->hash_fields != hash_fields)
			continue;
		ind_tbl = mlx5_ind_table_obj_get(dev, queues, queues_n);
		if (!ind_tbl)
			continue;
		if (ind_tbl != hrxq->ind_table) {
			mlx5_ind_table_obj_release(dev, ind_tbl);
			continue;
		}
		rte_atomic32_inc(&hrxq->refcnt);
		return idx;
	}
	return 0;
}

/**
 * Release the hash Rx queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param hrxq
 *   Index to Hash Rx queue to release.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
int
mlx5_hrxq_release(struct rte_eth_dev *dev, uint32_t hrxq_idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hrxq *hrxq;

	hrxq = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_HRXQ], hrxq_idx);
	if (!hrxq)
		return 0;
	if (rte_atomic32_dec_and_test(&hrxq->refcnt)) {
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
		mlx5_glue->destroy_flow_action(hrxq->action);
#endif
		if (hrxq->ind_table->type == MLX5_IND_TBL_TYPE_IBV)
			claim_zero(mlx5_glue->destroy_qp(hrxq->qp));
		else /* hrxq->ind_table->type == MLX5_IND_TBL_TYPE_DEVX */
			claim_zero(mlx5_devx_cmd_destroy(hrxq->tir));
		mlx5_ind_table_obj_release(dev, hrxq->ind_table);
		ILIST_REMOVE(priv->sh->ipool[MLX5_IPOOL_HRXQ], &priv->hrxqs,
			     hrxq_idx, hrxq, next);
		mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_HRXQ], hrxq_idx);
		return 0;
	}
	claim_nonzero(mlx5_ind_table_obj_release(dev, hrxq->ind_table));
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
mlx5_hrxq_verify(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hrxq *hrxq;
	uint32_t idx;
	int ret = 0;

	ILIST_FOREACH(priv->sh->ipool[MLX5_IPOOL_HRXQ], priv->hrxqs, idx,
		      hrxq, next) {
		DRV_LOG(DEBUG,
			"port %u hash Rx queue %p still referenced",
			dev->data->port_id, (void *)hrxq);
		++ret;
	}
	return ret;
}

/**
 * Create a drop Rx queue Verbs/DevX object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   The Verbs/DevX object initialised, NULL otherwise and rte_errno is set.
 */
static struct mlx5_rxq_obj *
mlx5_rxq_obj_drop_new(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct ibv_context *ctx = priv->sh->ctx;
	struct ibv_cq *cq;
	struct ibv_wq *wq = NULL;
	struct mlx5_rxq_obj *rxq;

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
	rxq = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*rxq), 0, SOCKET_ID_ANY);
	if (!rxq) {
		DEBUG("port %u cannot allocate drop Rx queue memory",
		      dev->data->port_id);
		rte_errno = ENOMEM;
		goto error;
	}
	rxq->ibv_cq = cq;
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
 * Release a drop Rx queue Verbs/DevX object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   The Verbs/DevX object initialised, NULL otherwise and rte_errno is set.
 */
static void
mlx5_rxq_obj_drop_release(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_obj *rxq = priv->drop_queue.rxq;

	if (rxq->wq)
		claim_zero(mlx5_glue->destroy_wq(rxq->wq));
	if (rxq->ibv_cq)
		claim_zero(mlx5_glue->destroy_cq(rxq->ibv_cq));
	mlx5_free(rxq);
	priv->drop_queue.rxq = NULL;
}

/**
 * Create a drop indirection table.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   The Verbs/DevX object initialised, NULL otherwise and rte_errno is set.
 */
static struct mlx5_ind_table_obj *
mlx5_ind_table_obj_drop_new(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ind_table_obj *ind_tbl;
	struct mlx5_rxq_obj *rxq;
	struct mlx5_ind_table_obj tmpl;

	rxq = mlx5_rxq_obj_drop_new(dev);
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
	ind_tbl = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*ind_tbl), 0,
			      SOCKET_ID_ANY);
	if (!ind_tbl) {
		rte_errno = ENOMEM;
		goto error;
	}
	ind_tbl->ind_table = tmpl.ind_table;
	return ind_tbl;
error:
	mlx5_rxq_obj_drop_release(dev);
	return NULL;
}

/**
 * Release a drop indirection table.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
static void
mlx5_ind_table_obj_drop_release(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ind_table_obj *ind_tbl = priv->drop_queue.hrxq->ind_table;

	claim_zero(mlx5_glue->destroy_rwq_ind_table(ind_tbl->ind_table));
	mlx5_rxq_obj_drop_release(dev);
	mlx5_free(ind_tbl);
	priv->drop_queue.hrxq->ind_table = NULL;
}

/**
 * Create a drop Rx Hash queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   The Verbs/DevX object initialised, NULL otherwise and rte_errno is set.
 */
struct mlx5_hrxq *
mlx5_hrxq_drop_new(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ind_table_obj *ind_tbl = NULL;
	struct ibv_qp *qp = NULL;
	struct mlx5_hrxq *hrxq = NULL;

	if (priv->drop_queue.hrxq) {
		rte_atomic32_inc(&priv->drop_queue.hrxq->refcnt);
		return priv->drop_queue.hrxq;
	}
	hrxq = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*hrxq), 0, SOCKET_ID_ANY);
	if (!hrxq) {
		DRV_LOG(WARNING,
			"port %u cannot allocate memory for drop queue",
			dev->data->port_id);
		rte_errno = ENOMEM;
		goto error;
	}
	priv->drop_queue.hrxq = hrxq;
	ind_tbl = mlx5_ind_table_obj_drop_new(dev);
	if (!ind_tbl)
		goto error;
	hrxq->ind_table = ind_tbl;
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
	hrxq->qp = qp;
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	hrxq->action = mlx5_glue->dv_create_flow_action_dest_ibv_qp(hrxq->qp);
	if (!hrxq->action) {
		rte_errno = errno;
		goto error;
	}
#endif
	rte_atomic32_set(&hrxq->refcnt, 1);
	return hrxq;
error:
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	if (hrxq && hrxq->action)
		mlx5_glue->destroy_flow_action(hrxq->action);
#endif
	if (qp)
		claim_zero(mlx5_glue->destroy_qp(hrxq->qp));
	if (ind_tbl)
		mlx5_ind_table_obj_drop_release(dev);
	if (hrxq) {
		priv->drop_queue.hrxq = NULL;
		mlx5_free(hrxq);
	}
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
		mlx5_ind_table_obj_drop_release(dev);
		mlx5_free(hrxq);
		priv->drop_queue.hrxq = NULL;
	}
}


/**
 * Set the Rx queue timestamp conversion parameters
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 */
void
mlx5_rxq_timestamp_set(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_rxq_data *data;
	unsigned int i;

	for (i = 0; i != priv->rxqs_n; ++i) {
		if (!(*priv->rxqs)[i])
			continue;
		data = (*priv->rxqs)[i];
		data->sh = sh;
		data->rt_timestamp = priv->config.rt_timestamp;
	}
}
