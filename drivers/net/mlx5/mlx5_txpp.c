/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_interrupts.h>
#include <rte_alarm.h>
#include <rte_malloc.h>

#include "mlx5.h"
#include "mlx5_rxtx.h"

/* Destroy Event Queue Notification Channel. */
static void
mlx5_txpp_destroy_eqn(struct mlx5_dev_ctx_shared *sh)
{
	if (sh->txpp.echan) {
		mlx5_glue->devx_destroy_event_channel(sh->txpp.echan);
		sh->txpp.echan = NULL;
	}
	sh->txpp.eqn = 0;
}

/* Create Event Queue Notification Channel. */
static int
mlx5_txpp_create_eqn(struct mlx5_dev_ctx_shared *sh)
{
	uint32_t lcore;

	MLX5_ASSERT(!sh->txpp.echan);
	lcore = (uint32_t)rte_lcore_to_cpu_id(-1);
	if (mlx5_glue->devx_query_eqn(sh->ctx, lcore, &sh->txpp.eqn)) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to query EQ number %d.", rte_errno);
		sh->txpp.eqn = 0;
		return -rte_errno;
	}
	sh->txpp.echan = mlx5_glue->devx_create_event_channel(sh->ctx,
			MLX5DV_DEVX_CREATE_EVENT_CHANNEL_FLAGS_OMIT_EV_DATA);
	if (!sh->txpp.echan) {
		sh->txpp.eqn = 0;
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create event channel %d.",
			rte_errno);
		return -rte_errno;
	}
	return 0;
}

static void
mlx5_txpp_destroy_clock_queue(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_txpp_wq *wq = &sh->txpp.clock_queue;

	if (wq->sq)
		claim_zero(mlx5_devx_cmd_destroy(wq->sq));
	if (wq->sq_umem)
		claim_zero(mlx5_glue->devx_umem_dereg(wq->sq_umem));
	if (wq->sq_buf)
		rte_free((void *)(uintptr_t)wq->sq_buf);
	if (wq->cq)
		claim_zero(mlx5_devx_cmd_destroy(wq->cq));
	if (wq->cq_umem)
		claim_zero(mlx5_glue->devx_umem_dereg(wq->cq_umem));
	if (wq->cq_buf)
		rte_free((void *)(uintptr_t)wq->cq_buf);
	memset(wq, 0, sizeof(*wq));
}

static void
mlx5_txpp_fill_wqe_clock_queue(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_txpp_wq *wq = &sh->txpp.clock_queue;
	struct mlx5_wqe *wqe = (struct mlx5_wqe *)(uintptr_t)wq->wqes;
	struct mlx5_wqe_cseg *cs = &wqe->cseg;
	uint32_t wqe_size, opcode, i;
	uint8_t *dst;

	/* For test purposes fill the WQ with SEND inline packet. */
	if (sh->txpp.test) {
		wqe_size = RTE_ALIGN(MLX5_TXPP_TEST_PKT_SIZE +
				     MLX5_WQE_CSEG_SIZE +
				     2 * MLX5_WQE_ESEG_SIZE -
				     MLX5_ESEG_MIN_INLINE_SIZE,
				     MLX5_WSEG_SIZE);
		opcode = MLX5_OPCODE_SEND;
	} else {
		wqe_size = MLX5_WSEG_SIZE;
		opcode = MLX5_OPCODE_NOP;
	}
	cs->opcode = rte_cpu_to_be_32(opcode | 0); /* Index is ignored. */
	cs->sq_ds = rte_cpu_to_be_32((wq->sq->id << 8) |
				     (wqe_size / MLX5_WSEG_SIZE));
	cs->flags = RTE_BE32(MLX5_COMP_ALWAYS << MLX5_COMP_MODE_OFFSET);
	cs->misc = RTE_BE32(0);
	wqe_size = RTE_ALIGN(wqe_size, MLX5_WQE_SIZE);
	if (sh->txpp.test) {
		struct mlx5_wqe_eseg *es = &wqe->eseg;
		struct rte_ether_hdr *eth_hdr;
		struct rte_ipv4_hdr *ip_hdr;
		struct rte_udp_hdr *udp_hdr;

		/* Build the inline test packet pattern. */
		MLX5_ASSERT(wqe_size <= MLX5_WQE_SIZE_MAX);
		MLX5_ASSERT(MLX5_TXPP_TEST_PKT_SIZE >=
				(sizeof(struct rte_ether_hdr) +
				 sizeof(struct rte_ipv4_hdr)));
		es->flags = 0;
		es->cs_flags = MLX5_ETH_WQE_L3_CSUM | MLX5_ETH_WQE_L4_CSUM;
		es->swp_offs = 0;
		es->metadata = 0;
		es->swp_flags = 0;
		es->mss = 0;
		es->inline_hdr_sz = RTE_BE16(MLX5_TXPP_TEST_PKT_SIZE);
		/* Build test packet L2 header (Ethernet). */
		dst = (uint8_t *)&es->inline_data;
		eth_hdr = (struct rte_ether_hdr *)dst;
		rte_eth_random_addr(&eth_hdr->d_addr.addr_bytes[0]);
		rte_eth_random_addr(&eth_hdr->s_addr.addr_bytes[0]);
		eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
		/* Build test packet L3 header (IP v4). */
		dst += sizeof(struct rte_ether_hdr);
		ip_hdr = (struct rte_ipv4_hdr *)dst;
		ip_hdr->version_ihl = RTE_IPV4_VHL_DEF;
		ip_hdr->type_of_service = 0;
		ip_hdr->fragment_offset = 0;
		ip_hdr->time_to_live = 64;
		ip_hdr->next_proto_id = IPPROTO_UDP;
		ip_hdr->packet_id = 0;
		ip_hdr->total_length = RTE_BE16(MLX5_TXPP_TEST_PKT_SIZE -
						sizeof(struct rte_ether_hdr));
		/* use RFC5735 / RFC2544 reserved network test addresses */
		ip_hdr->src_addr = RTE_BE32((198U << 24) | (18 << 16) |
					    (0 << 8) | 1);
		ip_hdr->dst_addr = RTE_BE32((198U << 24) | (18 << 16) |
					    (0 << 8) | 2);
		if (MLX5_TXPP_TEST_PKT_SIZE <
					(sizeof(struct rte_ether_hdr) +
					 sizeof(struct rte_ipv4_hdr) +
					 sizeof(struct rte_udp_hdr)))
			goto wcopy;
		/* Build test packet L4 header (UDP). */
		dst += sizeof(struct rte_ipv4_hdr);
		udp_hdr = (struct rte_udp_hdr *)dst;
		udp_hdr->src_port = RTE_BE16(9); /* RFC863 Discard. */
		udp_hdr->dst_port = RTE_BE16(9);
		udp_hdr->dgram_len = RTE_BE16(MLX5_TXPP_TEST_PKT_SIZE -
					      sizeof(struct rte_ether_hdr) -
					      sizeof(struct rte_ipv4_hdr));
		udp_hdr->dgram_cksum = 0;
		/* Fill the test packet data. */
		dst += sizeof(struct rte_udp_hdr);
		for (i = sizeof(struct rte_ether_hdr) +
			sizeof(struct rte_ipv4_hdr) +
			sizeof(struct rte_udp_hdr);
				i < MLX5_TXPP_TEST_PKT_SIZE; i++)
			*dst++ = (uint8_t)(i & 0xFF);
	}
wcopy:
	/* Duplicate the pattern to the next WQEs. */
	dst = (uint8_t *)(uintptr_t)wq->sq_buf;
	for (i = 1; i < MLX5_TXPP_CLKQ_SIZE; i++) {
		dst += wqe_size;
		rte_memcpy(dst, (void *)(uintptr_t)wq->sq_buf, wqe_size);
	}
}

/* Creates the Clock Queue for packet pacing, returns zero on success. */
static int
mlx5_txpp_create_clock_queue(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_devx_create_sq_attr sq_attr = { 0 };
	struct mlx5_devx_modify_sq_attr msq_attr = { 0 };
	struct mlx5_devx_cq_attr cq_attr = { 0 };
	struct mlx5_txpp_wq *wq = &sh->txpp.clock_queue;
	size_t page_size = sysconf(_SC_PAGESIZE);
	uint32_t umem_size, umem_dbrec;
	int ret;

	/* Allocate memory buffer for CQEs and doorbell record. */
	umem_size = sizeof(struct mlx5_cqe) * MLX5_TXPP_CLKQ_SIZE;
	umem_dbrec = RTE_ALIGN(umem_size, MLX5_DBR_SIZE);
	umem_size += MLX5_DBR_SIZE;
	wq->cq_buf = rte_zmalloc_socket(__func__, umem_size,
					page_size, sh->numa_node);
	if (!wq->cq_buf) {
		DRV_LOG(ERR, "Failed to allocate memory for Clock Queue.");
		return -ENOMEM;
	}
	/* Register allocated buffer in user space with DevX. */
	wq->cq_umem = mlx5_glue->devx_umem_reg(sh->ctx,
					       (void *)(uintptr_t)wq->cq_buf,
					       umem_size,
					       IBV_ACCESS_LOCAL_WRITE);
	if (!wq->cq_umem) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to register umem for Clock Queue.");
		goto error;
	}
	/* Create completion queue object for Clock Queue. */
	cq_attr.cqe_size = (sizeof(struct mlx5_cqe) == 128) ?
			    MLX5_CQE_SIZE_128B : MLX5_CQE_SIZE_64B;
	cq_attr.use_first_only = 1;
	cq_attr.overrun_ignore = 1;
	cq_attr.uar_page_id = sh->tx_uar->page_id;
	cq_attr.eqn = sh->txpp.eqn;
	cq_attr.q_umem_valid = 1;
	cq_attr.q_umem_offset = 0;
	cq_attr.q_umem_id = wq->cq_umem->umem_id;
	cq_attr.db_umem_valid = 1;
	cq_attr.db_umem_offset = umem_dbrec;
	cq_attr.db_umem_id = wq->cq_umem->umem_id;
	cq_attr.log_cq_size = rte_log2_u32(MLX5_TXPP_CLKQ_SIZE);
	cq_attr.log_page_size = rte_log2_u32(page_size);
	wq->cq = mlx5_devx_cmd_create_cq(sh->ctx, &cq_attr);
	if (!wq->cq) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create CQ for Clock Queue.");
		goto error;
	}
	wq->cq_dbrec = RTE_PTR_ADD(wq->cq_buf, umem_dbrec);
	wq->cq_ci = 0;
	/* Allocate memory buffer for Send Queue WQEs. */
	if (sh->txpp.test) {
		wq->sq_size = RTE_ALIGN(MLX5_TXPP_TEST_PKT_SIZE +
					MLX5_WQE_CSEG_SIZE +
					2 * MLX5_WQE_ESEG_SIZE -
					MLX5_ESEG_MIN_INLINE_SIZE,
					MLX5_WQE_SIZE) / MLX5_WQE_SIZE;
		wq->sq_size *= MLX5_TXPP_CLKQ_SIZE;
	} else {
		wq->sq_size = MLX5_TXPP_CLKQ_SIZE;
	}
	/* There should not be WQE leftovers in the cyclic queue. */
	MLX5_ASSERT(wq->sq_size == (1 << log2above(wq->sq_size)));
	umem_size =  MLX5_WQE_SIZE * wq->sq_size;
	umem_dbrec = RTE_ALIGN(umem_size, MLX5_DBR_SIZE);
	umem_size += MLX5_DBR_SIZE;
	wq->sq_buf = rte_zmalloc_socket(__func__, umem_size,
					page_size, sh->numa_node);
	if (!wq->sq_buf) {
		DRV_LOG(ERR, "Failed to allocate memory for Clock Queue.");
		rte_errno = ENOMEM;
		goto error;
	}
	/* Register allocated buffer in user space with DevX. */
	wq->sq_umem = mlx5_glue->devx_umem_reg(sh->ctx,
					       (void *)(uintptr_t)wq->sq_buf,
					       umem_size,
					       IBV_ACCESS_LOCAL_WRITE);
	if (!wq->sq_umem) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to register umem for Clock Queue.");
		goto error;
	}
	/* Create send queue object for Clock Queue. */
	if (sh->txpp.test) {
		sq_attr.tis_lst_sz = 1;
		sq_attr.tis_num = sh->tis->id;
		sq_attr.non_wire = 0;
		sq_attr.static_sq_wq = 1;
	} else {
		sq_attr.non_wire = 1;
		sq_attr.static_sq_wq = 1;
	}
	sq_attr.state = MLX5_SQC_STATE_RST;
	sq_attr.cqn = wq->cq->id;
	sq_attr.wq_attr.cd_slave = 1;
	sq_attr.wq_attr.uar_page = sh->tx_uar->page_id;
	sq_attr.wq_attr.wq_type = MLX5_WQ_TYPE_CYCLIC;
	sq_attr.wq_attr.pd = sh->pdn;
	sq_attr.wq_attr.log_wq_stride = rte_log2_u32(MLX5_WQE_SIZE);
	sq_attr.wq_attr.log_wq_sz = rte_log2_u32(wq->sq_size);
	sq_attr.wq_attr.dbr_umem_valid = 1;
	sq_attr.wq_attr.dbr_addr = umem_dbrec;
	sq_attr.wq_attr.dbr_umem_id = wq->sq_umem->umem_id;
	sq_attr.wq_attr.wq_umem_valid = 1;
	sq_attr.wq_attr.wq_umem_id = wq->sq_umem->umem_id;
	/* umem_offset must be zero for static_sq_wq queue. */
	sq_attr.wq_attr.wq_umem_offset = 0;
	wq->sq = mlx5_devx_cmd_create_sq(sh->ctx, &sq_attr);
	if (!wq->sq) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create SQ for Clock Queue.");
		goto error;
	}
	wq->sq_dbrec = RTE_PTR_ADD(wq->sq_buf, umem_dbrec +
				   MLX5_SND_DBR * sizeof(uint32_t));
	/* Build the WQEs in the Send Queue before goto Ready state. */
	mlx5_txpp_fill_wqe_clock_queue(sh);
	/* Change queue state to ready. */
	msq_attr.sq_state = MLX5_SQC_STATE_RST;
	msq_attr.state = MLX5_SQC_STATE_RDY;
	wq->sq_ci = 0;
	ret = mlx5_devx_cmd_modify_sq(wq->sq, &msq_attr);
	if (ret) {
		DRV_LOG(ERR, "Failed to set SQ ready state Clock Queue.");
		goto error;
	}
	return 0;
error:
	ret = -rte_errno;
	mlx5_txpp_destroy_clock_queue(sh);
	rte_errno = -ret;
	return ret;
}

/*
 * The routine initializes the packet pacing infrastructure:
 * - allocates PP context
 * - Clock CQ/SQ
 * - Rearm CQ/SQ
 * - attaches rearm interrupt handler
 *
 * Returns 0 on success, negative otherwise
 */
static int
mlx5_txpp_create(struct mlx5_dev_ctx_shared *sh, struct mlx5_priv *priv)
{
	int tx_pp = priv->config.tx_pp;
	int ret;

	/* Store the requested pacing parameters. */
	sh->txpp.tick = tx_pp >= 0 ? tx_pp : -tx_pp;
	sh->txpp.test = !!(tx_pp < 0);
	sh->txpp.skew = priv->config.tx_skew;
	sh->txpp.freq = priv->config.hca_attr.dev_freq_khz;
	ret = mlx5_txpp_create_eqn(sh);
	if (ret)
		goto exit;
	ret = mlx5_txpp_create_clock_queue(sh);
	if (ret)
		goto exit;
exit:
	if (ret) {
		mlx5_txpp_destroy_clock_queue(sh);
		mlx5_txpp_destroy_eqn(sh);
		sh->txpp.tick = 0;
		sh->txpp.test = 0;
		sh->txpp.skew = 0;
	}
	return ret;
}

/*
 * The routine destroys the packet pacing infrastructure:
 * - detaches rearm interrupt handler
 * - Rearm CQ/SQ
 * - Clock CQ/SQ
 * - PP context
 */
static void
mlx5_txpp_destroy(struct mlx5_dev_ctx_shared *sh)
{
	mlx5_txpp_destroy_clock_queue(sh);
	mlx5_txpp_destroy_eqn(sh);
	sh->txpp.tick = 0;
	sh->txpp.test = 0;
	sh->txpp.skew = 0;
}

/**
 * Creates and starts packet pacing infrastructure on specified device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_txpp_start(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	int err = 0;
	int ret;

	if (!priv->config.tx_pp) {
		/* Packet pacing is not requested for the device. */
		MLX5_ASSERT(priv->txpp_en == 0);
		return 0;
	}
	if (priv->txpp_en) {
		/* Packet pacing is already enabled for the device. */
		MLX5_ASSERT(sh->txpp.refcnt);
		return 0;
	}
	if (priv->config.tx_pp > 0) {
		ret = rte_mbuf_dynflag_lookup
				(RTE_MBUF_DYNFLAG_TX_TIMESTAMP_NAME, NULL);
		if (ret < 0)
			return 0;
	}
	ret = pthread_mutex_lock(&sh->txpp.mutex);
	MLX5_ASSERT(!ret);
	RTE_SET_USED(ret);
	if (sh->txpp.refcnt) {
		priv->txpp_en = 1;
		++sh->txpp.refcnt;
	} else {
		err = mlx5_txpp_create(sh, priv);
		if (!err) {
			MLX5_ASSERT(sh->txpp.tick);
			priv->txpp_en = 1;
			sh->txpp.refcnt = 1;
		} else {
			rte_errno = -err;
		}
	}
	ret = pthread_mutex_unlock(&sh->txpp.mutex);
	MLX5_ASSERT(!ret);
	RTE_SET_USED(ret);
	return err;
}

/**
 * Stops and destroys packet pacing infrastructure on specified device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
void
mlx5_txpp_stop(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	int ret;

	if (!priv->txpp_en) {
		/* Packet pacing is already disabled for the device. */
		return;
	}
	priv->txpp_en = 0;
	ret = pthread_mutex_lock(&sh->txpp.mutex);
	MLX5_ASSERT(!ret);
	RTE_SET_USED(ret);
	MLX5_ASSERT(sh->txpp.refcnt);
	if (!sh->txpp.refcnt || --sh->txpp.refcnt)
		return;
	/* No references any more, do actual destroy. */
	mlx5_txpp_destroy(sh);
	ret = pthread_mutex_unlock(&sh->txpp.mutex);
	MLX5_ASSERT(!ret);
	RTE_SET_USED(ret);
}
