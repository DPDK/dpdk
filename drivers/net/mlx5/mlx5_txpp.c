/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */
#include <fcntl.h>
#include <stdint.h>

#include <rte_ether.h>
#include <ethdev_driver.h>
#include <rte_interrupts.h>
#include <rte_alarm.h>
#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_eal_paging.h>

#include <mlx5_malloc.h>
#include <mlx5_common_devx.h>

#include "mlx5.h"
#include "mlx5_rx.h"
#include "mlx5_tx.h"
#include "mlx5_common_os.h"

static_assert(sizeof(struct mlx5_cqe_ts) == sizeof(rte_int128_t),
		"Wrong timestamp CQE part size");

static const char * const mlx5_txpp_stat_names[] = {
	"tx_pp_missed_interrupt_errors", /* Missed service interrupt. */
	"tx_pp_rearm_queue_errors", /* Rearm Queue errors. */
	"tx_pp_clock_queue_errors", /* Clock Queue errors. */
	"tx_pp_timestamp_past_errors", /* Timestamp in the past. */
	"tx_pp_timestamp_future_errors", /* Timestamp in the distant future. */
	"tx_pp_timestamp_order_errors", /* Timestamp not in ascending order. */
	"tx_pp_jitter", /* Timestamp jitter (one Clock Queue completion). */
	"tx_pp_wander", /* Timestamp wander (half of Clock Queue CQEs). */
	"tx_pp_sync_lost", /* Scheduling synchronization lost. */
};

/* Destroy Event Queue Notification Channel. */
static void
mlx5_txpp_destroy_event_channel(struct mlx5_dev_ctx_shared *sh)
{
	if (sh->txpp.echan) {
		mlx5_os_devx_destroy_event_channel(sh->txpp.echan);
		sh->txpp.echan = NULL;
	}
}

/* Create Event Queue Notification Channel. */
static int
mlx5_txpp_create_event_channel(struct mlx5_dev_ctx_shared *sh)
{
	MLX5_ASSERT(!sh->txpp.echan);
	sh->txpp.echan = mlx5_os_devx_create_event_channel(sh->cdev->ctx,
			MLX5DV_DEVX_CREATE_EVENT_CHANNEL_FLAGS_OMIT_EV_DATA);
	if (!sh->txpp.echan) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create event channel %d.", rte_errno);
		return -rte_errno;
	}
	return 0;
}

static void
mlx5_txpp_free_pp_index(struct mlx5_dev_ctx_shared *sh)
{
#ifdef HAVE_MLX5DV_PP_ALLOC
	if (sh->txpp.pp) {
		mlx5_glue->dv_free_pp(sh->txpp.pp);
		sh->txpp.pp = NULL;
		sh->txpp.pp_id = 0;
	}
#else
	RTE_SET_USED(sh);
	DRV_LOG(ERR, "Freeing pacing index is not supported.");
#endif
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
				(sh->cdev->ctx, sizeof(pp), &pp,
				 MLX5DV_PP_ALLOC_FLAGS_DEDICATED_INDEX);
	if (sh->txpp.pp == NULL) {
		DRV_LOG(ERR, "Failed to allocate packet pacing index.");
		rte_errno = errno;
		return -errno;
	}
	if (!((struct mlx5dv_pp *)sh->txpp.pp)->index) {
		DRV_LOG(ERR, "Zero packet pacing index allocated.");
		mlx5_txpp_free_pp_index(sh);
		rte_errno = ENOTSUP;
		return -ENOTSUP;
	}
	sh->txpp.pp_id = ((struct mlx5dv_pp *)(sh->txpp.pp))->index;
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
	mlx5_devx_sq_destroy(&wq->sq_obj);
	mlx5_devx_cq_destroy(&wq->cq_obj);
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
	struct mlx5_wqe *wqe = (struct mlx5_wqe *)(uintptr_t)wq->sq_obj.wqes;
	union {
		uint32_t w32[2];
		uint64_t w64;
	} cs;

	wq->sq_ci = ci + 1;
	cs.w32[0] = rte_cpu_to_be_32(rte_be_to_cpu_32
			(wqe[ci & (wq->sq_size - 1)].ctrl[0]) | (ci - 1) << 8);
	cs.w32[1] = wqe[ci & (wq->sq_size - 1)].ctrl[1];
	/* Update SQ doorbell record with new SQ ci. */
	mlx5_doorbell_ring(&sh->tx_uar.bf_db, cs.w64, wq->sq_ci,
			   wq->sq_obj.db_rec, !sh->tx_uar.dbnc);
}

static void
mlx5_txpp_fill_wqe_rearm_queue(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_txpp_wq *wq = &sh->txpp.rearm_queue;
	struct mlx5_wqe *wqe = (struct mlx5_wqe *)(uintptr_t)wq->sq_obj.wqes;
	uint32_t i;

	for (i = 0; i < wq->sq_size; i += 2) {
		struct mlx5_wqe_cseg *cs;
		struct mlx5_wqe_qseg *qs;
		uint32_t index;

		/* Build SEND_EN request with slave WQE index. */
		cs = &wqe[i + 0].cseg;
		cs->opcode = RTE_BE32(MLX5_OPCODE_SEND_EN | 0);
		cs->sq_ds = rte_cpu_to_be_32((wq->sq_obj.sq->id << 8) | 2);
		cs->flags = RTE_BE32(MLX5_COMP_ALWAYS <<
				     MLX5_COMP_MODE_OFFSET);
		cs->misc = RTE_BE32(0);
		qs = RTE_PTR_ADD(cs, sizeof(struct mlx5_wqe_cseg));
		index = (i * MLX5_TXPP_REARM / 2 + MLX5_TXPP_REARM) &
			((1 << MLX5_WQ_INDEX_WIDTH) - 1);
		qs->max_index = rte_cpu_to_be_32(index);
		qs->qpn_cqn =
			   rte_cpu_to_be_32(sh->txpp.clock_queue.sq_obj.sq->id);
		/* Build WAIT request with slave CQE index. */
		cs = &wqe[i + 1].cseg;
		cs->opcode = RTE_BE32(MLX5_OPCODE_WAIT | 0);
		cs->sq_ds = rte_cpu_to_be_32((wq->sq_obj.sq->id << 8) | 2);
		cs->flags = RTE_BE32(MLX5_COMP_ONLY_ERR <<
				     MLX5_COMP_MODE_OFFSET);
		cs->misc = RTE_BE32(0);
		qs = RTE_PTR_ADD(cs, sizeof(struct mlx5_wqe_cseg));
		index = (i * MLX5_TXPP_REARM / 2 + MLX5_TXPP_REARM / 2) &
			((1 << MLX5_CQ_INDEX_WIDTH) - 1);
		qs->max_index = rte_cpu_to_be_32(index);
		qs->qpn_cqn =
			   rte_cpu_to_be_32(sh->txpp.clock_queue.cq_obj.cq->id);
	}
}

/* Creates the Rearm Queue to fire the requests to Clock Queue in realtime. */
static int
mlx5_txpp_create_rearm_queue(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_devx_create_sq_attr sq_attr = {
		.cd_master = 1,
		.state = MLX5_SQC_STATE_RST,
		.tis_lst_sz = 1,
		.tis_num = sh->tis[0]->id,
		.wq_attr = (struct mlx5_devx_wq_attr){
			.pd = sh->cdev->pdn,
			.uar_page =
				mlx5_os_get_devx_uar_page_id(sh->tx_uar.obj),
		},
		.ts_format = mlx5_ts_format_conv
				       (sh->cdev->config.hca_attr.sq_ts_format),
	};
	struct mlx5_devx_modify_sq_attr msq_attr = { 0 };
	struct mlx5_devx_cq_attr cq_attr = {
		.uar_page_id = mlx5_os_get_devx_uar_page_id(sh->tx_uar.obj),
	};
	struct mlx5_txpp_wq *wq = &sh->txpp.rearm_queue;
	int ret;

	/* Create completion queue object for Rearm Queue. */
	ret = mlx5_devx_cq_create(sh->cdev->ctx, &wq->cq_obj,
				  log2above(MLX5_TXPP_REARM_CQ_SIZE), &cq_attr,
				  sh->numa_node);
	if (ret) {
		DRV_LOG(ERR, "Failed to create CQ for Rearm Queue.");
		return ret;
	}
	wq->cq_ci = 0;
	wq->arm_sn = 0;
	wq->sq_size = MLX5_TXPP_REARM_SQ_SIZE;
	MLX5_ASSERT(wq->sq_size == (1 << log2above(wq->sq_size)));
	/* Create send queue object for Rearm Queue. */
	sq_attr.cqn = wq->cq_obj.cq->id;
	/* There should be no WQE leftovers in the cyclic queue. */
	ret = mlx5_devx_sq_create(sh->cdev->ctx, &wq->sq_obj,
				  log2above(MLX5_TXPP_REARM_SQ_SIZE), &sq_attr,
				  sh->numa_node);
	if (ret) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create SQ for Rearm Queue.");
		goto error;
	}
	/* Build the WQEs in the Send Queue before goto Ready state. */
	mlx5_txpp_fill_wqe_rearm_queue(sh);
	/* Change queue state to ready. */
	msq_attr.sq_state = MLX5_SQC_STATE_RST;
	msq_attr.state = MLX5_SQC_STATE_RDY;
	ret = mlx5_devx_cmd_modify_sq(wq->sq_obj.sq, &msq_attr);
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
	struct mlx5_wqe *wqe = (struct mlx5_wqe *)(uintptr_t)wq->sq_obj.wqes;
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
	cs->sq_ds = rte_cpu_to_be_32((wq->sq_obj.sq->id << 8) |
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
		rte_eth_random_addr(&eth_hdr->dst_addr.addr_bytes[0]);
		rte_eth_random_addr(&eth_hdr->src_addr.addr_bytes[0]);
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
	dst = (uint8_t *)(uintptr_t)wq->sq_obj.umem_buf;
	for (i = 1; i < MLX5_TXPP_CLKQ_SIZE; i++) {
		dst += wqe_size;
		rte_memcpy(dst, (void *)(uintptr_t)wq->sq_obj.umem_buf,
			   wqe_size);
	}
}

/* Creates the Clock Queue for packet pacing, returns zero on success. */
static int
mlx5_txpp_create_clock_queue(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_devx_create_sq_attr sq_attr = { 0 };
	struct mlx5_devx_modify_sq_attr msq_attr = { 0 };
	struct mlx5_devx_cq_attr cq_attr = {
		.use_first_only = 1,
		.overrun_ignore = 1,
		.uar_page_id = mlx5_os_get_devx_uar_page_id(sh->tx_uar.obj),
	};
	struct mlx5_txpp_wq *wq = &sh->txpp.clock_queue;
	int ret;

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
	/* Create completion queue object for Clock Queue. */
	ret = mlx5_devx_cq_create(sh->cdev->ctx, &wq->cq_obj,
				  log2above(MLX5_TXPP_CLKQ_SIZE), &cq_attr,
				  sh->numa_node);
	if (ret) {
		DRV_LOG(ERR, "Failed to create CQ for Clock Queue.");
		goto error;
	}
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
	/* Create send queue object for Clock Queue. */
	if (sh->txpp.test) {
		sq_attr.tis_lst_sz = 1;
		sq_attr.tis_num = sh->tis[0]->id;
		sq_attr.non_wire = 0;
		sq_attr.static_sq_wq = 1;
	} else {
		sq_attr.non_wire = 1;
		sq_attr.static_sq_wq = 1;
	}
	sq_attr.cqn = wq->cq_obj.cq->id;
	sq_attr.packet_pacing_rate_limit_index = sh->txpp.pp_id;
	sq_attr.wq_attr.cd_slave = 1;
	sq_attr.wq_attr.uar_page = mlx5_os_get_devx_uar_page_id(sh->tx_uar.obj);
	sq_attr.wq_attr.pd = sh->cdev->pdn;
	sq_attr.ts_format =
		mlx5_ts_format_conv(sh->cdev->config.hca_attr.sq_ts_format);
	ret = mlx5_devx_sq_create(sh->cdev->ctx, &wq->sq_obj,
				  log2above(wq->sq_size),
				  &sq_attr, sh->numa_node);
	if (ret) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create SQ for Clock Queue.");
		goto error;
	}
	/* Build the WQEs in the Send Queue before goto Ready state. */
	mlx5_txpp_fill_wqe_clock_queue(sh);
	/* Change queue state to ready. */
	msq_attr.sq_state = MLX5_SQC_STATE_RST;
	msq_attr.state = MLX5_SQC_STATE_RDY;
	wq->sq_ci = 0;
	ret = mlx5_devx_cmd_modify_sq(wq->sq_obj.sq, &msq_attr);
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
	uint64_t db_be =
		rte_cpu_to_be_64(((uint64_t)db_hi << 32) | aq->cq_obj.cq->id);

	mlx5_doorbell_ring(&sh->tx_uar.cq_db, db_be, db_hi,
			   &aq->cq_obj.db_rec[MLX5_CQ_ARM_DB], 0);
	aq->arm_sn++;
}

#if defined(RTE_ARCH_X86_64)
static inline int
mlx5_atomic128_compare_exchange(rte_int128_t *dst,
				rte_int128_t *exp,
				const rte_int128_t *src)
{
	uint8_t res;

	asm volatile (MPLOCKED
		      "cmpxchg16b %[dst];"
		      " sete %[res]"
		      : [dst] "=m" (dst->val[0]),
			"=a" (exp->val[0]),
			"=d" (exp->val[1]),
			[res] "=r" (res)
		      : "b" (src->val[0]),
			"c" (src->val[1]),
			"a" (exp->val[0]),
			"d" (exp->val[1]),
			"m" (dst->val[0])
		      : "memory");

	return res;
}
#endif

static inline void
mlx5_atomic_read_cqe(rte_int128_t *from, rte_int128_t *ts)
{
	/*
	 * The only CQE of Clock Queue is being continuously
	 * updated by hardware with specified rate. We must
	 * read timestamp and WQE completion index atomically.
	 */
#if defined(RTE_ARCH_X86_64)
	rte_int128_t src;

	memset(&src, 0, sizeof(src));
	*ts = src;
	/* if (*from == *ts) *from = *src else *ts = *from; */
	mlx5_atomic128_compare_exchange(from, ts, &src);
#else
	uint64_t *cqe = (uint64_t *)from;

	/*
	 * Power architecture does not support 16B compare-and-swap.
	 * ARM implements it in software, code below is more relevant.
	 */
	for (;;) {
		uint64_t tm, op;
		uint64_t *ps;

		rte_compiler_barrier();
		tm = __atomic_load_n(cqe + 0, __ATOMIC_RELAXED);
		op = __atomic_load_n(cqe + 1, __ATOMIC_RELAXED);
		rte_compiler_barrier();
		if (tm != __atomic_load_n(cqe + 0, __ATOMIC_RELAXED))
			continue;
		if (op != __atomic_load_n(cqe + 1, __ATOMIC_RELAXED))
			continue;
		ps = (uint64_t *)ts;
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
	__atomic_store_n(&sh->txpp.ts.ts, ts, __ATOMIC_RELAXED);
	__atomic_store_n(&sh->txpp.ts.ci_ts, ci, __ATOMIC_RELAXED);
	rte_wmb();
}

/* Reads timestamp from Clock Queue CQE and stores in the cache. */
static inline void
mlx5_txpp_update_timestamp(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_txpp_wq *wq = &sh->txpp.clock_queue;
	struct mlx5_cqe *cqe = (struct mlx5_cqe *)(uintptr_t)wq->cq_obj.cqes;
	union {
		rte_int128_t u128;
		struct mlx5_cqe_ts cts;
	} to;
	uint64_t ts;
	uint16_t ci;
	uint8_t opcode;

	mlx5_atomic_read_cqe((rte_int128_t *)&cqe->timestamp, &to.u128);
	opcode = MLX5_CQE_OPCODE(to.cts.op_own);
	if (opcode) {
		if (opcode != MLX5_CQE_INVALID) {
			/*
			 * Commit the error state if and only if
			 * we have got at least one actual completion.
			 */
			DRV_LOG(DEBUG,
				"Clock Queue error sync lost (%X).", opcode);
				__atomic_fetch_add(&sh->txpp.err_clock_queue,
				   1, __ATOMIC_RELAXED);
			sh->txpp.sync_lost = 1;
		}
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
		mlx5_txpp_update_timestamp(sh);
		if (wq->sq_ci)
			return;
		/* Wait one millisecond and try again. */
		rte_delay_us_sleep(US_PER_S / MS_PER_S);
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
	__atomic_store_n(&sh->txpp.tsa[sh->txpp.ts_p].ts,
			 sh->txpp.ts.ts, __ATOMIC_RELAXED);
	__atomic_store_n(&sh->txpp.tsa[sh->txpp.ts_p].ci_ts,
			 sh->txpp.ts.ci_ts, __ATOMIC_RELAXED);
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

		cqe = &wq->cq_obj.cqes[cq_ci & (MLX5_TXPP_REARM_CQ_SIZE - 1)];
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
			__atomic_fetch_add(&sh->txpp.err_miss_int,
					   1, __ATOMIC_RELAXED);
			/* Check sync lost on wqe index. */
			if (cq_ci - wq->cq_ci >=
				(((1UL << MLX5_WQ_INDEX_WIDTH) /
				  MLX5_TXPP_REARM) - 1))
				error = 1;
		}
		/* Update doorbell record to notify hardware. */
		rte_compiler_barrier();
		*wq->cq_obj.db_rec = rte_cpu_to_be_32(cq_ci);
		rte_wmb();
		wq->cq_ci = cq_ci;
		/* Fire new requests to Rearm Queue. */
		if (error) {
			DRV_LOG(DEBUG, "Rearm Queue error sync lost.");
			__atomic_fetch_add(&sh->txpp.err_rearm_queue,
					   1, __ATOMIC_RELAXED);
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
	mlx5_os_interrupt_handler_destroy(sh->txpp.intr_handle,
					  mlx5_txpp_interrupt_handler, sh);
}

/* Attach interrupt handler and fires first request to Rearm Queue. */
static int
mlx5_txpp_start_service(struct mlx5_dev_ctx_shared *sh)
{
	uint16_t event_nums[1] = {0};
	int ret;
	int fd;

	sh->txpp.err_miss_int = 0;
	sh->txpp.err_rearm_queue = 0;
	sh->txpp.err_clock_queue = 0;
	sh->txpp.err_ts_past = 0;
	sh->txpp.err_ts_future = 0;
	sh->txpp.err_ts_order = 0;
	/* Attach interrupt handler to process Rearm Queue completions. */
	fd = mlx5_os_get_devx_channel_fd(sh->txpp.echan);
	ret = mlx5_os_set_nonblock_channel_fd(fd);
	if (ret) {
		DRV_LOG(ERR, "Failed to change event channel FD.");
		rte_errno = errno;
		return -rte_errno;
	}
	fd = mlx5_os_get_devx_channel_fd(sh->txpp.echan);
	sh->txpp.intr_handle = mlx5_os_interrupt_handler_create
		(RTE_INTR_INSTANCE_F_SHARED, false,
		 fd, mlx5_txpp_interrupt_handler, sh);
	if (!sh->txpp.intr_handle) {
		DRV_LOG(ERR, "Fail to allocate intr_handle");
		return -rte_errno;
	}
	/* Subscribe CQ event to the event channel controlled by the driver. */
	ret = mlx5_os_devx_subscribe_devx_event(sh->txpp.echan,
					    sh->txpp.rearm_queue.cq_obj.cq->obj,
					     sizeof(event_nums), event_nums, 0);
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
mlx5_txpp_create(struct mlx5_dev_ctx_shared *sh)
{
	int tx_pp = sh->config.tx_pp;
	int ret;

	/* Store the requested pacing parameters. */
	sh->txpp.tick = tx_pp >= 0 ? tx_pp : -tx_pp;
	sh->txpp.test = !!(tx_pp < 0);
	sh->txpp.skew = sh->config.tx_skew;
	sh->txpp.freq = sh->cdev->config.hca_attr.dev_freq_khz;
	ret = mlx5_txpp_create_event_channel(sh);
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
		mlx5_txpp_destroy_event_channel(sh);
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
	mlx5_txpp_destroy_event_channel(sh);
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

	if (!sh->config.tx_pp) {
		/* Packet pacing is not requested for the device. */
		MLX5_ASSERT(priv->txpp_en == 0);
		return 0;
	}
	if (priv->txpp_en) {
		/* Packet pacing is already enabled for the device. */
		MLX5_ASSERT(sh->txpp.refcnt);
		return 0;
	}
	if (sh->config.tx_pp > 0) {
		err = rte_mbuf_dynflag_lookup
			(RTE_MBUF_DYNFLAG_TX_TIMESTAMP_NAME, NULL);
		/* No flag registered means no service needed. */
		if (err < 0)
			return 0;
		err = 0;
	}
	claim_zero(pthread_mutex_lock(&sh->txpp.mutex));
	if (sh->txpp.refcnt) {
		priv->txpp_en = 1;
		++sh->txpp.refcnt;
	} else {
		err = mlx5_txpp_create(sh);
		if (!err) {
			MLX5_ASSERT(sh->txpp.tick);
			priv->txpp_en = 1;
			sh->txpp.refcnt = 1;
		} else {
			rte_errno = -err;
		}
	}
	claim_zero(pthread_mutex_unlock(&sh->txpp.mutex));
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

	if (!priv->txpp_en) {
		/* Packet pacing is already disabled for the device. */
		return;
	}
	priv->txpp_en = 0;
	claim_zero(pthread_mutex_lock(&sh->txpp.mutex));
	MLX5_ASSERT(sh->txpp.refcnt);
	if (!sh->txpp.refcnt || --sh->txpp.refcnt) {
		claim_zero(pthread_mutex_unlock(&sh->txpp.mutex));
		return;
	}
	/* No references any more, do actual destroy. */
	mlx5_txpp_destroy(sh);
	claim_zero(pthread_mutex_unlock(&sh->txpp.mutex));
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
	struct mlx5_proc_priv *ppriv;
	uint64_t ts;
	int ret;

	if (sh->txpp.refcnt) {
		struct mlx5_txpp_wq *wq = &sh->txpp.clock_queue;
		struct mlx5_cqe *cqe =
				(struct mlx5_cqe *)(uintptr_t)wq->cq_obj.cqes;
		union {
			rte_int128_t u128;
			struct mlx5_cqe_ts cts;
		} to;

		mlx5_atomic_read_cqe((rte_int128_t *)&cqe->timestamp, &to.u128);
		if (to.cts.op_own >> 4) {
			DRV_LOG(DEBUG, "Clock Queue error sync lost.");
			__atomic_fetch_add(&sh->txpp.err_clock_queue,
					   1, __ATOMIC_RELAXED);
			sh->txpp.sync_lost = 1;
			return -EIO;
		}
		ts = rte_be_to_cpu_64(to.cts.timestamp);
		ts = mlx5_txpp_convert_rx_ts(sh, ts);
		*timestamp = ts;
		return 0;
	}
	/* Check and try to map HCA PIC BAR to allow reading real time. */
	ppriv = dev->process_private;
	if (ppriv && !ppriv->hca_bar &&
	    sh->dev_cap.rt_timestamp && mlx5_dev_is_pci(dev->device))
		mlx5_txpp_map_hca_bar(dev);
	/* Check if we can read timestamp directly from hardware. */
	if (ppriv && ppriv->hca_bar) {
		ts = MLX5_GET64(initial_seg, ppriv->hca_bar, real_time);
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

	__atomic_store_n(&sh->txpp.err_miss_int, 0, __ATOMIC_RELAXED);
	__atomic_store_n(&sh->txpp.err_rearm_queue, 0, __ATOMIC_RELAXED);
	__atomic_store_n(&sh->txpp.err_clock_queue, 0, __ATOMIC_RELAXED);
	__atomic_store_n(&sh->txpp.err_ts_past, 0, __ATOMIC_RELAXED);
	__atomic_store_n(&sh->txpp.err_ts_future, 0, __ATOMIC_RELAXED);
	__atomic_store_n(&sh->txpp.err_ts_order, 0, __ATOMIC_RELAXED);
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
			strlcpy(xstats_names[i + n_used].name,
				mlx5_txpp_stat_names[i],
				RTE_ETH_XSTATS_NAME_SIZE);
		}
	}
	return n_used + n_txpp;
}

static inline void
mlx5_txpp_read_tsa(struct mlx5_dev_txpp *txpp,
		   struct mlx5_txpp_ts *tsa, uint16_t idx)
{
	do {
		uint64_t ts, ci;

		ts = __atomic_load_n(&txpp->tsa[idx].ts, __ATOMIC_RELAXED);
		ci = __atomic_load_n(&txpp->tsa[idx].ci_ts, __ATOMIC_RELAXED);
		rte_compiler_barrier();
		if ((ci ^ ts) << MLX5_CQ_INDEX_WIDTH != 0)
			continue;
		if (__atomic_load_n(&txpp->tsa[idx].ts,
				    __ATOMIC_RELAXED) != ts)
			continue;
		if (__atomic_load_n(&txpp->tsa[idx].ci_ts,
				    __ATOMIC_RELAXED) != ci)
			continue;
		tsa->ts = ts;
		tsa->ci_ts = ci;
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
	dts = tsa1.ts - tsa0.ts;
	dci = (tsa1.ci_ts >> (64 - MLX5_CQ_INDEX_WIDTH)) -
	      (tsa0.ci_ts >> (64 - MLX5_CQ_INDEX_WIDTH));
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
	dts = tsa1.ts - tsa0.ts;
	dci = (tsa1.ci_ts >> (64 - MLX5_CQ_INDEX_WIDTH)) -
	      (tsa0.ci_ts >> (64 - MLX5_CQ_INDEX_WIDTH));
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
				__atomic_load_n(&sh->txpp.err_miss_int,
						__ATOMIC_RELAXED);
		stats[n_used + 1].value =
				__atomic_load_n(&sh->txpp.err_rearm_queue,
						__ATOMIC_RELAXED);
		stats[n_used + 2].value =
				__atomic_load_n(&sh->txpp.err_clock_queue,
						__ATOMIC_RELAXED);
		stats[n_used + 3].value =
				__atomic_load_n(&sh->txpp.err_ts_past,
						__ATOMIC_RELAXED);
		stats[n_used + 4].value =
				__atomic_load_n(&sh->txpp.err_ts_future,
						__ATOMIC_RELAXED);
		stats[n_used + 5].value =
				__atomic_load_n(&sh->txpp.err_ts_order,
						__ATOMIC_RELAXED);
		stats[n_used + 6].value = mlx5_txpp_xstats_jitter(&sh->txpp);
		stats[n_used + 7].value = mlx5_txpp_xstats_wander(&sh->txpp);
		stats[n_used + 8].value = sh->txpp.sync_lost;
	}
	return n_used + n_txpp;
}
