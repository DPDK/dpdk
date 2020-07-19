/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */
#include <fcntl.h>
#include <stdint.h>

#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_interrupts.h>
#include <rte_alarm.h>
#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_eal_paging.h>

#include <mlx5_malloc.h>

#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_common_os.h"

static const char * const mlx5_txpp_stat_names[] = {
	"txpp_err_miss_int", /* Missed service interrupt. */
	"txpp_err_rearm_queue",	/* Rearm Queue errors. */
	"txpp_err_clock_queue", /* Clock Queue errors. */
	"txpp_err_ts_past", /* Timestamp in the past. */
	"txpp_err_ts_future", /* Timestamp in the distant future. */
	"txpp_jitter", /* Timestamp jitter (one Clock Queue completion). */
	"txpp_wander", /* Timestamp jitter (half of Clock Queue completions). */
	"txpp_sync_lost", /* Scheduling synchronization lost. */
};

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
mlx5_txpp_free_pp_index(struct mlx5_dev_ctx_shared *sh)
{
	if (sh->txpp.pp) {
		mlx5_glue->dv_free_pp(sh->txpp.pp);
		sh->txpp.pp = NULL;
		sh->txpp.pp_id = 0;
	}
}

/* Allocate Packet Pacing index from kernel via mlx5dv call. */
static int
mlx5_txpp_alloc_pp_index(struct mlx5_dev_ctx_shared *sh)
{
#ifdef HAVE_MLX5DV_PP_ALLOC
	uint32_t pp[MLX5_ST_SZ_DW(set_pp_rate_limit_context)];
	uint64_t rate;

	MLX5_ASSERT(!sh->txpp.pp);
	memset(&pp, 0, sizeof(pp));
	rate = NS_PER_S / sh->txpp.tick;
	if (rate * sh->txpp.tick != NS_PER_S)
		DRV_LOG(WARNING, "Packet pacing frequency is not precise.");
	if (sh->txpp.test) {
		uint32_t len;

		len = RTE_MAX(MLX5_TXPP_TEST_PKT_SIZE,
			      (size_t)RTE_ETHER_MIN_LEN);
		MLX5_SET(set_pp_rate_limit_context, &pp,
			 burst_upper_bound, len);
		MLX5_SET(set_pp_rate_limit_context, &pp,
			 typical_packet_size, len);
		/* Convert packets per second into kilobits. */
		rate = (rate * len) / (1000ul / CHAR_BIT);
		DRV_LOG(INFO, "Packet pacing rate set to %" PRIu64, rate);
	}
	MLX5_SET(set_pp_rate_limit_context, &pp, rate_limit, rate);
	MLX5_SET(set_pp_rate_limit_context, &pp, rate_mode,
		 sh->txpp.test ? MLX5_DATA_RATE : MLX5_WQE_RATE);
	sh->txpp.pp = mlx5_glue->dv_alloc_pp
				(sh->ctx, sizeof(pp), &pp,
				 MLX5DV_PP_ALLOC_FLAGS_DEDICATED_INDEX);
	if (sh->txpp.pp == NULL) {
		DRV_LOG(ERR, "Failed to allocate packet pacing index.");
		rte_errno = errno;
		return -errno;
	}
	if (!sh->txpp.pp->index) {
		DRV_LOG(ERR, "Zero packet pacing index allocated.");
		mlx5_txpp_free_pp_index(sh);
		rte_errno = ENOTSUP;
		return -ENOTSUP;
	}
	sh->txpp.pp_id = sh->txpp.pp->index;
	return 0;
#else
	RTE_SET_USED(sh);
	DRV_LOG(ERR, "Allocating pacing index is not supported.");
	rte_errno = ENOTSUP;
	return -ENOTSUP;
#endif
}

static void
mlx5_txpp_destroy_send_queue(struct mlx5_txpp_wq *wq)
{
	if (wq->sq)
		claim_zero(mlx5_devx_cmd_destroy(wq->sq));
	if (wq->sq_umem)
		claim_zero(mlx5_glue->devx_umem_dereg(wq->sq_umem));
	if (wq->sq_buf)
		mlx5_free((void *)(uintptr_t)wq->sq_buf);
	if (wq->cq)
		claim_zero(mlx5_devx_cmd_destroy(wq->cq));
	if (wq->cq_umem)
		claim_zero(mlx5_glue->devx_umem_dereg(wq->cq_umem));
	if (wq->cq_buf)
		mlx5_free((void *)(uintptr_t)wq->cq_buf);
	memset(wq, 0, sizeof(*wq));
}

static void
mlx5_txpp_destroy_rearm_queue(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_txpp_wq *wq = &sh->txpp.rearm_queue;

	mlx5_txpp_destroy_send_queue(wq);
}

static void
mlx5_txpp_destroy_clock_queue(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_txpp_wq *wq = &sh->txpp.clock_queue;

	mlx5_txpp_destroy_send_queue(wq);
	if (sh->txpp.tsa) {
		mlx5_free(sh->txpp.tsa);
		sh->txpp.tsa = NULL;
	}
}

static void
mlx5_txpp_doorbell_rearm_queue(struct mlx5_dev_ctx_shared *sh, uint16_t ci)
{
	struct mlx5_txpp_wq *wq = &sh->txpp.rearm_queue;
	union {
		uint32_t w32[2];
		uint64_t w64;
	} cs;

	wq->sq_ci = ci + 1;
	cs.w32[0] = rte_cpu_to_be_32(rte_be_to_cpu_32
		   (wq->wqes[ci & (wq->sq_size - 1)].ctrl[0]) | (ci - 1) << 8);
	cs.w32[1] = wq->wqes[ci & (wq->sq_size - 1)].ctrl[1];
	/* Update SQ doorbell record with new SQ ci. */
	rte_compiler_barrier();
	*wq->sq_dbrec = rte_cpu_to_be_32(wq->sq_ci);
	/* Make sure the doorbell record is updated. */
	rte_wmb();
	/* Write to doorbel register to start processing. */
	__mlx5_uar_write64_relaxed(cs.w64, sh->tx_uar->reg_addr, NULL);
	rte_wmb();
}

static void
mlx5_txpp_fill_cqe_rearm_queue(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_txpp_wq *wq = &sh->txpp.rearm_queue;
	struct mlx5_cqe *cqe = (struct mlx5_cqe *)(uintptr_t)wq->cqes;
	uint32_t i;

	for (i = 0; i < MLX5_TXPP_REARM_CQ_SIZE; i++) {
		cqe->op_own = (MLX5_CQE_INVALID << 4) | MLX5_CQE_OWNER_MASK;
		++cqe;
	}
}

static void
mlx5_txpp_fill_wqe_rearm_queue(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_txpp_wq *wq = &sh->txpp.rearm_queue;
	struct mlx5_wqe *wqe = (struct mlx5_wqe *)(uintptr_t)wq->wqes;
	uint32_t i;

	for (i = 0; i < wq->sq_size; i += 2) {
		struct mlx5_wqe_cseg *cs;
		struct mlx5_wqe_qseg *qs;
		uint32_t index;

		/* Build SEND_EN request with slave WQE index. */
		cs = &wqe[i + 0].cseg;
		cs->opcode = RTE_BE32(MLX5_OPCODE_SEND_EN | 0);
		cs->sq_ds = rte_cpu_to_be_32((wq->sq->id << 8) | 2);
		cs->flags = RTE_BE32(MLX5_COMP_ALWAYS <<
				     MLX5_COMP_MODE_OFFSET);
		cs->misc = RTE_BE32(0);
		qs = RTE_PTR_ADD(cs, sizeof(struct mlx5_wqe_cseg));
		index = (i * MLX5_TXPP_REARM / 2 + MLX5_TXPP_REARM) &
			((1 << MLX5_WQ_INDEX_WIDTH) - 1);
		qs->max_index = rte_cpu_to_be_32(index);
		qs->qpn_cqn = rte_cpu_to_be_32(sh->txpp.clock_queue.sq->id);
		/* Build WAIT request with slave CQE index. */
		cs = &wqe[i + 1].cseg;
		cs->opcode = RTE_BE32(MLX5_OPCODE_WAIT | 0);
		cs->sq_ds = rte_cpu_to_be_32((wq->sq->id << 8) | 2);
		cs->flags = RTE_BE32(MLX5_COMP_ONLY_ERR <<
				     MLX5_COMP_MODE_OFFSET);
		cs->misc = RTE_BE32(0);
		qs = RTE_PTR_ADD(cs, sizeof(struct mlx5_wqe_cseg));
		index = (i * MLX5_TXPP_REARM / 2 + MLX5_TXPP_REARM / 2) &
			((1 << MLX5_CQ_INDEX_WIDTH) - 1);
		qs->max_index = rte_cpu_to_be_32(index);
		qs->qpn_cqn = rte_cpu_to_be_32(sh->txpp.clock_queue.cq->id);
	}
}

/* Creates the Rearm Queue to fire the requests to Clock Queue in realtime. */
static int
mlx5_txpp_create_rearm_queue(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_devx_create_sq_attr sq_attr = { 0 };
	struct mlx5_devx_modify_sq_attr msq_attr = { 0 };
	struct mlx5_devx_cq_attr cq_attr = { 0 };
	struct mlx5_txpp_wq *wq = &sh->txpp.rearm_queue;
	size_t page_size;
	uint32_t umem_size, umem_dbrec;
	int ret;

	page_size = rte_mem_page_size();
	if (page_size == (size_t)-1) {
		DRV_LOG(ERR, "Failed to get mem page size");
		return -ENOMEM;
	}
	/* Allocate memory buffer for CQEs and doorbell record. */
	umem_size = sizeof(struct mlx5_cqe) * MLX5_TXPP_REARM_CQ_SIZE;
	umem_dbrec = RTE_ALIGN(umem_size, MLX5_DBR_SIZE);
	umem_size += MLX5_DBR_SIZE;
	wq->cq_buf = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO, umem_size,
				 page_size, sh->numa_node);
	if (!wq->cq_buf) {
		DRV_LOG(ERR, "Failed to allocate memory for Rearm Queue.");
		return -ENOMEM;
	}
	/* Register allocated buffer in user space with DevX. */
	wq->cq_umem = mlx5_glue->devx_umem_reg(sh->ctx,
					       (void *)(uintptr_t)wq->cq_buf,
					       umem_size,
					       IBV_ACCESS_LOCAL_WRITE);
	if (!wq->cq_umem) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to register umem for Rearm Queue.");
		goto error;
	}
	/* Create completion queue object for Rearm Queue. */
	cq_attr.cqe_size = (sizeof(struct mlx5_cqe) == 128) ?
			    MLX5_CQE_SIZE_128B : MLX5_CQE_SIZE_64B;
	cq_attr.uar_page_id = sh->tx_uar->page_id;
	cq_attr.eqn = sh->txpp.eqn;
	cq_attr.q_umem_valid = 1;
	cq_attr.q_umem_offset = 0;
	cq_attr.q_umem_id = mlx5_os_get_umem_id(wq->cq_umem);
	cq_attr.db_umem_valid = 1;
	cq_attr.db_umem_offset = umem_dbrec;
	cq_attr.db_umem_id = mlx5_os_get_umem_id(wq->cq_umem);
	cq_attr.log_cq_size = rte_log2_u32(MLX5_TXPP_REARM_CQ_SIZE);
	cq_attr.log_page_size = rte_log2_u32(page_size);
	wq->cq = mlx5_devx_cmd_create_cq(sh->ctx, &cq_attr);
	if (!wq->cq) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create CQ for Rearm Queue.");
		goto error;
	}
	wq->cq_dbrec = RTE_PTR_ADD(wq->cq_buf, umem_dbrec);
	wq->cq_ci = 0;
	wq->arm_sn = 0;
	/* Mark all CQEs initially as invalid. */
	mlx5_txpp_fill_cqe_rearm_queue(sh);
	/*
	 * Allocate memory buffer for Send Queue WQEs.
	 * There should be no WQE leftovers in the cyclic queue.
	 */
	wq->sq_size = MLX5_TXPP_REARM_SQ_SIZE;
	MLX5_ASSERT(wq->sq_size == (1 << log2above(wq->sq_size)));
	umem_size =  MLX5_WQE_SIZE * wq->sq_size;
	umem_dbrec = RTE_ALIGN(umem_size, MLX5_DBR_SIZE);
	umem_size += MLX5_DBR_SIZE;
	wq->sq_buf = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO, umem_size,
				 page_size, sh->numa_node);
	if (!wq->sq_buf) {
		DRV_LOG(ERR, "Failed to allocate memory for Rearm Queue.");
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
		DRV_LOG(ERR, "Failed to register umem for Rearm Queue.");
		goto error;
	}
	/* Create send queue object for Rearm Queue. */
	sq_attr.state = MLX5_SQC_STATE_RST;
	sq_attr.tis_lst_sz = 1;
	sq_attr.tis_num = sh->tis->id;
	sq_attr.cqn = wq->cq->id;
	sq_attr.cd_master = 1;
	sq_attr.wq_attr.uar_page = sh->tx_uar->page_id;
	sq_attr.wq_attr.wq_type = MLX5_WQ_TYPE_CYCLIC;
	sq_attr.wq_attr.pd = sh->pdn;
	sq_attr.wq_attr.log_wq_stride = rte_log2_u32(MLX5_WQE_SIZE);
	sq_attr.wq_attr.log_wq_sz = rte_log2_u32(wq->sq_size);
	sq_attr.wq_attr.dbr_umem_valid = 1;
	sq_attr.wq_attr.dbr_addr = umem_dbrec;
	sq_attr.wq_attr.dbr_umem_id = mlx5_os_get_umem_id(wq->sq_umem);
	sq_attr.wq_attr.wq_umem_valid = 1;
	sq_attr.wq_attr.wq_umem_id = mlx5_os_get_umem_id(wq->sq_umem);
	sq_attr.wq_attr.wq_umem_offset = 0;
	wq->sq = mlx5_devx_cmd_create_sq(sh->ctx, &sq_attr);
	if (!wq->sq) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create SQ for Rearm Queue.");
		goto error;
	}
	wq->sq_dbrec = RTE_PTR_ADD(wq->sq_buf, umem_dbrec +
				   MLX5_SND_DBR * sizeof(uint32_t));
	/* Build the WQEs in the Send Queue before goto Ready state. */
	mlx5_txpp_fill_wqe_rearm_queue(sh);
	/* Change queue state to ready. */
	msq_attr.sq_state = MLX5_SQC_STATE_RST;
	msq_attr.state = MLX5_SQC_STATE_RDY;
	ret = mlx5_devx_cmd_modify_sq(wq->sq, &msq_attr);
	if (ret) {
		DRV_LOG(ERR, "Failed to set SQ ready state Rearm Queue.");
		goto error;
	}
	return 0;
error:
	ret = -rte_errno;
	mlx5_txpp_destroy_rearm_queue(sh);
	rte_errno = -ret;
	return ret;
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
	size_t page_size;
	uint32_t umem_size, umem_dbrec;
	int ret;

	page_size = rte_mem_page_size();
	if (page_size == (size_t)-1) {
		DRV_LOG(ERR, "Failed to get mem page size");
		return -ENOMEM;
	}
	sh->txpp.tsa = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO,
				   MLX5_TXPP_REARM_SQ_SIZE *
				   sizeof(struct mlx5_txpp_ts),
				   0, sh->numa_node);
	if (!sh->txpp.tsa) {
		DRV_LOG(ERR, "Failed to allocate memory for CQ stats.");
		return -ENOMEM;
	}
	sh->txpp.ts_p = 0;
	sh->txpp.ts_n = 0;
	/* Allocate memory buffer for CQEs and doorbell record. */
	umem_size = sizeof(struct mlx5_cqe) * MLX5_TXPP_CLKQ_SIZE;
	umem_dbrec = RTE_ALIGN(umem_size, MLX5_DBR_SIZE);
	umem_size += MLX5_DBR_SIZE;
	wq->cq_buf = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO, umem_size,
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
	wq->sq_buf = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO, umem_size,
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
	sq_attr.packet_pacing_rate_limit_index = sh->txpp.pp_id;
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

/* Enable notification from the Rearm Queue CQ. */
static inline void
mlx5_txpp_cq_arm(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_txpp_wq *aq = &sh->txpp.rearm_queue;
	uint32_t arm_sn = aq->arm_sn << MLX5_CQ_SQN_OFFSET;
	uint32_t db_hi = arm_sn | MLX5_CQ_DBR_CMD_ALL | aq->cq_ci;
	uint64_t db_be = rte_cpu_to_be_64(((uint64_t)db_hi << 32) | aq->cq->id);
	uint32_t *addr = RTE_PTR_ADD(sh->tx_uar->base_addr, MLX5_CQ_DOORBELL);

	rte_compiler_barrier();
	aq->cq_dbrec[MLX5_CQ_ARM_DB] = rte_cpu_to_be_32(db_hi);
	rte_wmb();
#ifdef RTE_ARCH_64
	*(uint64_t *)addr = db_be;
#else
	*(uint32_t *)addr = db_be;
	rte_io_wmb();
	*((uint32_t *)addr + 1) = db_be >> 32;
#endif
	aq->arm_sn++;
}

static inline void
mlx5_atomic_read_cqe(rte_int128_t *from, rte_int128_t *ts)
{
	/*
	 * The only CQE of Clock Queue is being continuously
	 * update by hardware with soecified rate. We have to
	 * read timestump and WQE completion index atomically.
	 */
#if defined(RTE_ARCH_X86_64) || defined(RTE_ARCH_ARM64)
	rte_int128_t src;

	memset(&src, 0, sizeof(src));
	*ts = src;
	/* if (*from == *ts) *from = *src else *ts = *from; */
	rte_atomic128_cmp_exchange(from, ts, &src, 0,
				   __ATOMIC_RELAXED, __ATOMIC_RELAXED);
#else
	rte_atomic64_t *cqe = (rte_atomic64_t *)from;

	/* Power architecture does not support 16B compare-and-swap. */
	for (;;) {
		int64_t tm, op;
		int64_t *ps;

		rte_compiler_barrier();
		tm = rte_atomic64_read(cqe + 0);
		op = rte_atomic64_read(cqe + 1);
		rte_compiler_barrier();
		if (tm != rte_atomic64_read(cqe + 0))
			continue;
		if (op != rte_atomic64_read(cqe + 1))
			continue;
		ps = (int64_t *)ts;
		ps[0] = tm;
		ps[1] = op;
		return;
	}
#endif
}

/* Stores timestamp in the cache structure to share data with datapath. */
static inline void
mlx5_txpp_cache_timestamp(struct mlx5_dev_ctx_shared *sh,
			   uint64_t ts, uint64_t ci)
{
	ci = ci << (64 - MLX5_CQ_INDEX_WIDTH);
	ci |= (ts << MLX5_CQ_INDEX_WIDTH) >> MLX5_CQ_INDEX_WIDTH;
	rte_compiler_barrier();
	rte_atomic64_set(&sh->txpp.ts.ts, ts);
	rte_atomic64_set(&sh->txpp.ts.ci_ts, ci);
	rte_wmb();
}

/* Reads timestamp from Clock Queue CQE and stores in the cache. */
static inline void
mlx5_txpp_update_timestamp(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_txpp_wq *wq = &sh->txpp.clock_queue;
	struct mlx5_cqe *cqe = (struct mlx5_cqe *)(uintptr_t)wq->cqes;
	union {
		rte_int128_t u128;
		struct mlx5_cqe_ts cts;
	} to;
	uint64_t ts;
	uint16_t ci;

	static_assert(sizeof(struct mlx5_cqe_ts) == sizeof(rte_int128_t),
		      "Wrong timestamp CQE part size");
	mlx5_atomic_read_cqe((rte_int128_t *)&cqe->timestamp, &to.u128);
	if (to.cts.op_own >> 4) {
		DRV_LOG(DEBUG, "Clock Queue error sync lost.");
		rte_atomic32_inc(&sh->txpp.err_clock_queue);
		sh->txpp.sync_lost = 1;
		return;
	}
	ci = rte_be_to_cpu_16(to.cts.wqe_counter);
	ts = rte_be_to_cpu_64(to.cts.timestamp);
	ts = mlx5_txpp_convert_rx_ts(sh, ts);
	wq->cq_ci += (ci - wq->sq_ci) & UINT16_MAX;
	wq->sq_ci = ci;
	mlx5_txpp_cache_timestamp(sh, ts, wq->cq_ci);
}

/* Waits for the first completion on Clock Queue to init timestamp. */
static inline void
mlx5_txpp_init_timestamp(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_txpp_wq *wq = &sh->txpp.clock_queue;
	uint32_t wait;

	sh->txpp.ts_p = 0;
	sh->txpp.ts_n = 0;
	for (wait = 0; wait < MLX5_TXPP_WAIT_INIT_TS; wait++) {
		struct timespec onems;

		mlx5_txpp_update_timestamp(sh);
		if (wq->sq_ci)
			return;
		/* Wait one millisecond and try again. */
		onems.tv_sec = 0;
		onems.tv_nsec = NS_PER_S / MS_PER_S;
		nanosleep(&onems, 0);
	}
	DRV_LOG(ERR, "Unable to initialize timestamp.");
	sh->txpp.sync_lost = 1;
}

#ifdef HAVE_IBV_DEVX_EVENT
/* Gather statistics for timestamp from Clock Queue CQE. */
static inline void
mlx5_txpp_gather_timestamp(struct mlx5_dev_ctx_shared *sh)
{
	/* Check whether we have a valid timestamp. */
	if (!sh->txpp.clock_queue.sq_ci && !sh->txpp.ts_n)
		return;
	MLX5_ASSERT(sh->txpp.ts_p < MLX5_TXPP_REARM_SQ_SIZE);
	sh->txpp.tsa[sh->txpp.ts_p] = sh->txpp.ts;
	if (++sh->txpp.ts_p >= MLX5_TXPP_REARM_SQ_SIZE)
		sh->txpp.ts_p = 0;
	if (sh->txpp.ts_n < MLX5_TXPP_REARM_SQ_SIZE)
		++sh->txpp.ts_n;
}

/* Handles Rearm Queue completions in periodic service. */
static __rte_always_inline void
mlx5_txpp_handle_rearm_queue(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_txpp_wq *wq = &sh->txpp.rearm_queue;
	uint32_t cq_ci = wq->cq_ci;
	bool error = false;
	int ret;

	do {
		volatile struct mlx5_cqe *cqe;

		cqe = &wq->cqes[cq_ci & (MLX5_TXPP_REARM_CQ_SIZE - 1)];
		ret = check_cqe(cqe, MLX5_TXPP_REARM_CQ_SIZE, cq_ci);
		switch (ret) {
		case MLX5_CQE_STATUS_ERR:
			error = true;
			++cq_ci;
			break;
		case MLX5_CQE_STATUS_SW_OWN:
			wq->sq_ci += 2;
			++cq_ci;
			break;
		case MLX5_CQE_STATUS_HW_OWN:
			break;
		default:
			MLX5_ASSERT(false);
			break;
		}
	} while (ret != MLX5_CQE_STATUS_HW_OWN);
	if (likely(cq_ci != wq->cq_ci)) {
		/* Check whether we have missed interrupts. */
		if (cq_ci - wq->cq_ci != 1) {
			DRV_LOG(DEBUG, "Rearm Queue missed interrupt.");
			rte_atomic32_inc(&sh->txpp.err_miss_int);
			/* Check sync lost on wqe index. */
			if (cq_ci - wq->cq_ci >=
				(((1UL << MLX5_WQ_INDEX_WIDTH) /
				  MLX5_TXPP_REARM) - 1))
				error = 1;
		}
		/* Update doorbell record to notify hardware. */
		rte_compiler_barrier();
		*wq->cq_dbrec = rte_cpu_to_be_32(cq_ci);
		rte_wmb();
		wq->cq_ci = cq_ci;
		/* Fire new requests to Rearm Queue. */
		if (error) {
			DRV_LOG(DEBUG, "Rearm Queue error sync lost.");
			rte_atomic32_inc(&sh->txpp.err_rearm_queue);
			sh->txpp.sync_lost = 1;
		}
	}
}

/* Handles Clock Queue completions in periodic service. */
static __rte_always_inline void
mlx5_txpp_handle_clock_queue(struct mlx5_dev_ctx_shared *sh)
{
	mlx5_txpp_update_timestamp(sh);
	mlx5_txpp_gather_timestamp(sh);
}
#endif

/* Invoked periodically on Rearm Queue completions. */
void
mlx5_txpp_interrupt_handler(void *cb_arg)
{
#ifndef HAVE_IBV_DEVX_EVENT
	RTE_SET_USED(cb_arg);
	return;
#else
	struct mlx5_dev_ctx_shared *sh = cb_arg;
	union {
		struct mlx5dv_devx_async_event_hdr event_resp;
		uint8_t buf[sizeof(struct mlx5dv_devx_async_event_hdr) + 128];
	} out;

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	/* Process events in the loop. Only rearm completions are expected. */
	while (mlx5_glue->devx_get_event
				(sh->txpp.echan,
				 &out.event_resp,
				 sizeof(out.buf)) >=
				 (ssize_t)sizeof(out.event_resp.cookie)) {
		mlx5_txpp_handle_rearm_queue(sh);
		mlx5_txpp_handle_clock_queue(sh);
		mlx5_txpp_cq_arm(sh);
		mlx5_txpp_doorbell_rearm_queue
					(sh, sh->txpp.rearm_queue.sq_ci - 1);
	}
#endif /* HAVE_IBV_DEVX_ASYNC */
}

static void
mlx5_txpp_stop_service(struct mlx5_dev_ctx_shared *sh)
{
	if (!sh->txpp.intr_handle.fd)
		return;
	mlx5_intr_callback_unregister(&sh->txpp.intr_handle,
				      mlx5_txpp_interrupt_handler, sh);
	sh->txpp.intr_handle.fd = 0;
}

/* Attach interrupt handler and fires first request to Rearm Queue. */
static int
mlx5_txpp_start_service(struct mlx5_dev_ctx_shared *sh)
{
	uint16_t event_nums[1] = {0};
	int flags;
	int ret;

	rte_atomic32_set(&sh->txpp.err_miss_int, 0);
	rte_atomic32_set(&sh->txpp.err_rearm_queue, 0);
	rte_atomic32_set(&sh->txpp.err_clock_queue, 0);
	rte_atomic32_set(&sh->txpp.err_ts_past, 0);
	rte_atomic32_set(&sh->txpp.err_ts_future, 0);
	/* Attach interrupt handler to process Rearm Queue completions. */
	flags = fcntl(sh->txpp.echan->fd, F_GETFL);
	ret = fcntl(sh->txpp.echan->fd, F_SETFL, flags | O_NONBLOCK);
	if (ret) {
		DRV_LOG(ERR, "Failed to change event channel FD.");
		rte_errno = errno;
		return -rte_errno;
	}
	memset(&sh->txpp.intr_handle, 0, sizeof(sh->txpp.intr_handle));
	sh->txpp.intr_handle.fd = sh->txpp.echan->fd;
	sh->txpp.intr_handle.type = RTE_INTR_HANDLE_EXT;
	if (rte_intr_callback_register(&sh->txpp.intr_handle,
				       mlx5_txpp_interrupt_handler, sh)) {
		sh->txpp.intr_handle.fd = 0;
		DRV_LOG(ERR, "Failed to register CQE interrupt %d.", rte_errno);
		return -rte_errno;
	}
	/* Subscribe CQ event to the event channel controlled by the driver. */
	ret = mlx5_glue->devx_subscribe_devx_event(sh->txpp.echan,
						   sh->txpp.rearm_queue.cq->obj,
						   sizeof(event_nums),
						   event_nums, 0);
	if (ret) {
		DRV_LOG(ERR, "Failed to subscribe CQE event.");
		rte_errno = errno;
		return -errno;
	}
	/* Enable interrupts in the CQ. */
	mlx5_txpp_cq_arm(sh);
	/* Fire the first request on Rearm Queue. */
	mlx5_txpp_doorbell_rearm_queue(sh, sh->txpp.rearm_queue.sq_size - 1);
	mlx5_txpp_init_timestamp(sh);
	return 0;
}

/*
 * The routine initializes the packet pacing infrastructure:
 * - allocates PP context
 * - Clock CQ/SQ
 * - Rearm CQ/SQ
 * - attaches rearm interrupt handler
 * - starts Clock Queue
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
	ret = mlx5_txpp_alloc_pp_index(sh);
	if (ret)
		goto exit;
	ret = mlx5_txpp_create_clock_queue(sh);
	if (ret)
		goto exit;
	ret = mlx5_txpp_create_rearm_queue(sh);
	if (ret)
		goto exit;
	ret = mlx5_txpp_start_service(sh);
	if (ret)
		goto exit;
exit:
	if (ret) {
		mlx5_txpp_stop_service(sh);
		mlx5_txpp_destroy_rearm_queue(sh);
		mlx5_txpp_destroy_clock_queue(sh);
		mlx5_txpp_free_pp_index(sh);
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
	mlx5_txpp_stop_service(sh);
	mlx5_txpp_destroy_rearm_queue(sh);
	mlx5_txpp_destroy_clock_queue(sh);
	mlx5_txpp_free_pp_index(sh);
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

/*
 * Read the current clock counter of an Ethernet device
 *
 * This returns the current raw clock value of an Ethernet device. It is
 * a raw amount of ticks, with no given time reference.
 * The value returned here is from the same clock than the one
 * filling timestamp field of Rx/Tx packets when using hardware timestamp
 * offload. Therefore it can be used to compute a precise conversion of
 * the device clock to the real time.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param clock
 *   Pointer to the uint64_t that holds the raw clock value.
 *
 * @return
 *   - 0: Success.
 *   - -ENOTSUP: The function is not supported in this mode. Requires
 *     packet pacing module configured and started (tx_pp devarg)
 */
int
mlx5_txpp_read_clock(struct rte_eth_dev *dev, uint64_t *timestamp)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	int ret;

	if (sh->txpp.refcnt) {
		struct mlx5_txpp_wq *wq = &sh->txpp.clock_queue;
		struct mlx5_cqe *cqe = (struct mlx5_cqe *)(uintptr_t)wq->cqes;
		union {
			rte_int128_t u128;
			struct mlx5_cqe_ts cts;
		} to;
		uint64_t ts;

		mlx5_atomic_read_cqe((rte_int128_t *)&cqe->timestamp, &to.u128);
		if (to.cts.op_own >> 4) {
			DRV_LOG(DEBUG, "Clock Queue error sync lost.");
			rte_atomic32_inc(&sh->txpp.err_clock_queue);
			sh->txpp.sync_lost = 1;
			return -EIO;
		}
		ts = rte_be_to_cpu_64(to.cts.timestamp);
		ts = mlx5_txpp_convert_rx_ts(sh, ts);
		*timestamp = ts;
		return 0;
	}
	/* Not supported in isolated mode - kernel does not see the CQEs. */
	if (priv->isolated || rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -ENOTSUP;
	ret = mlx5_read_clock(dev, timestamp);
	return ret;
}

/**
 * DPDK callback to clear device extended statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success and stats is reset, negative errno value otherwise and
 *   rte_errno is set.
 */
int mlx5_txpp_xstats_reset(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;

	rte_atomic32_set(&sh->txpp.err_miss_int, 0);
	rte_atomic32_set(&sh->txpp.err_rearm_queue, 0);
	rte_atomic32_set(&sh->txpp.err_clock_queue, 0);
	rte_atomic32_set(&sh->txpp.err_ts_past, 0);
	rte_atomic32_set(&sh->txpp.err_ts_future, 0);
	return 0;
}

/**
 * Routine to retrieve names of extended device statistics
 * for packet send scheduling. It appends the specific stats names
 * after the parts filled by preceding modules (eth stats, etc.)
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] xstats_names
 *   Buffer to insert names into.
 * @param n
 *   Number of names.
 * @param n_used
 *   Number of names filled by preceding statistics modules.
 *
 * @return
 *   Number of xstats names.
 */
int mlx5_txpp_xstats_get_names(struct rte_eth_dev *dev __rte_unused,
			       struct rte_eth_xstat_name *xstats_names,
			       unsigned int n, unsigned int n_used)
{
	unsigned int n_txpp = RTE_DIM(mlx5_txpp_stat_names);
	unsigned int i;

	if (n >= n_used + n_txpp && xstats_names) {
		for (i = 0; i < n_txpp; ++i) {
			strncpy(xstats_names[i + n_used].name,
				mlx5_txpp_stat_names[i],
				RTE_ETH_XSTATS_NAME_SIZE);
			xstats_names[i + n_used].name
					[RTE_ETH_XSTATS_NAME_SIZE - 1] = 0;
		}
	}
	return n_used + n_txpp;
}

static inline void
mlx5_txpp_read_tsa(struct mlx5_dev_txpp *txpp,
		   struct mlx5_txpp_ts *tsa, uint16_t idx)
{
	do {
		int64_t ts, ci;

		ts = rte_atomic64_read(&txpp->tsa[idx].ts);
		ci = rte_atomic64_read(&txpp->tsa[idx].ci_ts);
		rte_compiler_barrier();
		if ((ci ^ ts) << MLX5_CQ_INDEX_WIDTH != 0)
			continue;
		if (rte_atomic64_read(&txpp->tsa[idx].ts) != ts)
			continue;
		if (rte_atomic64_read(&txpp->tsa[idx].ci_ts) != ci)
			continue;
		rte_atomic64_set(&tsa->ts, ts);
		rte_atomic64_set(&tsa->ci_ts, ci);
		return;
	} while (true);
}

/*
 * Jitter reflects the clock change between
 * neighbours Clock Queue completions.
 */
static uint64_t
mlx5_txpp_xstats_jitter(struct mlx5_dev_txpp *txpp)
{
	struct mlx5_txpp_ts tsa0, tsa1;
	int64_t dts, dci;
	uint16_t ts_p;

	if (txpp->ts_n < 2) {
		/* No gathered enough reports yet. */
		return 0;
	}
	do {
		int ts_0, ts_1;

		ts_p = txpp->ts_p;
		rte_compiler_barrier();
		ts_0 = ts_p - 2;
		if (ts_0 < 0)
			ts_0 += MLX5_TXPP_REARM_SQ_SIZE;
		ts_1 = ts_p - 1;
		if (ts_1 < 0)
			ts_1 += MLX5_TXPP_REARM_SQ_SIZE;
		mlx5_txpp_read_tsa(txpp, &tsa0, ts_0);
		mlx5_txpp_read_tsa(txpp, &tsa1, ts_1);
		rte_compiler_barrier();
	} while (ts_p != txpp->ts_p);
	/* We have two neighbor reports, calculate the jitter. */
	dts = rte_atomic64_read(&tsa1.ts) - rte_atomic64_read(&tsa0.ts);
	dci = (rte_atomic64_read(&tsa1.ci_ts) >> (64 - MLX5_CQ_INDEX_WIDTH)) -
	      (rte_atomic64_read(&tsa0.ci_ts) >> (64 - MLX5_CQ_INDEX_WIDTH));
	if (dci < 0)
		dci += 1 << MLX5_CQ_INDEX_WIDTH;
	dci *= txpp->tick;
	return (dts > dci) ? dts - dci : dci - dts;
}

/*
 * Wander reflects the long-term clock change
 * over the entire length of all Clock Queue completions.
 */
static uint64_t
mlx5_txpp_xstats_wander(struct mlx5_dev_txpp *txpp)
{
	struct mlx5_txpp_ts tsa0, tsa1;
	int64_t dts, dci;
	uint16_t ts_p;

	if (txpp->ts_n < MLX5_TXPP_REARM_SQ_SIZE) {
		/* No gathered enough reports yet. */
		return 0;
	}
	do {
		int ts_0, ts_1;

		ts_p = txpp->ts_p;
		rte_compiler_barrier();
		ts_0 = ts_p - MLX5_TXPP_REARM_SQ_SIZE / 2 - 1;
		if (ts_0 < 0)
			ts_0 += MLX5_TXPP_REARM_SQ_SIZE;
		ts_1 = ts_p - 1;
		if (ts_1 < 0)
			ts_1 += MLX5_TXPP_REARM_SQ_SIZE;
		mlx5_txpp_read_tsa(txpp, &tsa0, ts_0);
		mlx5_txpp_read_tsa(txpp, &tsa1, ts_1);
		rte_compiler_barrier();
	} while (ts_p != txpp->ts_p);
	/* We have two neighbor reports, calculate the jitter. */
	dts = rte_atomic64_read(&tsa1.ts) - rte_atomic64_read(&tsa0.ts);
	dci = (rte_atomic64_read(&tsa1.ci_ts) >> (64 - MLX5_CQ_INDEX_WIDTH)) -
	      (rte_atomic64_read(&tsa0.ci_ts) >> (64 - MLX5_CQ_INDEX_WIDTH));
	dci += 1 << MLX5_CQ_INDEX_WIDTH;
	dci *= txpp->tick;
	return (dts > dci) ? dts - dci : dci - dts;
}

/**
 * Routine to retrieve extended device statistics
 * for packet send scheduling. It appends the specific statistics
 * after the parts filled by preceding modules (eth stats, etc.)
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[out] stats
 *   Pointer to rte extended stats table.
 * @param n
 *   The size of the stats table.
 * @param n_used
 *   Number of stats filled by preceding statistics modules.
 *
 * @return
 *   Number of extended stats on success and stats is filled,
 *   negative on error and rte_errno is set.
 */
int
mlx5_txpp_xstats_get(struct rte_eth_dev *dev,
		     struct rte_eth_xstat *stats,
		     unsigned int n, unsigned int n_used)
{
	unsigned int n_txpp = RTE_DIM(mlx5_txpp_stat_names);

	if (n >= n_used + n_txpp && stats) {
		struct mlx5_priv *priv = dev->data->dev_private;
		struct mlx5_dev_ctx_shared *sh = priv->sh;
		unsigned int i;

		for (i = 0; i < n_txpp; ++i)
			stats[n_used + i].id = n_used + i;
		stats[n_used + 0].value =
				rte_atomic32_read(&sh->txpp.err_miss_int);
		stats[n_used + 1].value =
				rte_atomic32_read(&sh->txpp.err_rearm_queue);
		stats[n_used + 2].value =
				rte_atomic32_read(&sh->txpp.err_clock_queue);
		stats[n_used + 3].value =
				rte_atomic32_read(&sh->txpp.err_ts_past);
		stats[n_used + 4].value =
				rte_atomic32_read(&sh->txpp.err_ts_future);
		stats[n_used + 5].value = mlx5_txpp_xstats_jitter(&sh->txpp);
		stats[n_used + 6].value = mlx5_txpp_xstats_wander(&sh->txpp);
		stats[n_used + 7].value = sh->txpp.sync_lost;
	}
	return n_used + n_txpp;
}
