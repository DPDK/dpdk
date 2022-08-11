/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 6WIND S.A.
 * Copyright 2021 Mellanox Technologies, Ltd
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_prefetch.h>
#include <rte_common.h>
#include <rte_branch_prediction.h>
#include <rte_ether.h>
#include <rte_cycles.h>
#include <rte_flow.h>

#include <mlx5_prm.h>
#include <mlx5_common.h>
#include <mlx5_common_mr.h>

#include "mlx5_autoconf.h"
#include "mlx5_defs.h"
#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_rx.h"


static __rte_always_inline uint32_t
rxq_cq_to_pkt_type(struct mlx5_rxq_data *rxq, volatile struct mlx5_cqe *cqe,
		   volatile struct mlx5_mini_cqe8 *mcqe);

static __rte_always_inline int
mlx5_rx_poll_len(struct mlx5_rxq_data *rxq, volatile struct mlx5_cqe *cqe,
		 uint16_t cqe_cnt, volatile struct mlx5_mini_cqe8 **mcqe);

static __rte_always_inline uint32_t
rxq_cq_to_ol_flags(volatile struct mlx5_cqe *cqe);

static __rte_always_inline void
rxq_cq_to_mbuf(struct mlx5_rxq_data *rxq, struct rte_mbuf *pkt,
	       volatile struct mlx5_cqe *cqe,
	       volatile struct mlx5_mini_cqe8 *mcqe);

static inline void
mlx5_lro_update_tcp_hdr(struct rte_tcp_hdr *__rte_restrict tcp,
			volatile struct mlx5_cqe *__rte_restrict cqe,
			uint32_t phcsum, uint8_t l4_type);

static inline void
mlx5_lro_update_hdr(uint8_t *__rte_restrict padd,
		    volatile struct mlx5_cqe *__rte_restrict cqe,
		    volatile struct mlx5_mini_cqe8 *mcqe,
		    struct mlx5_rxq_data *rxq, uint32_t len);


/**
 * Internal function to compute the number of used descriptors in an RX queue.
 *
 * @param rxq
 *   The Rx queue.
 *
 * @return
 *   The number of used Rx descriptor.
 */
static uint32_t
rx_queue_count(struct mlx5_rxq_data *rxq)
{
	struct rxq_zip *zip = &rxq->zip;
	volatile struct mlx5_cqe *cqe;
	const unsigned int cqe_n = (1 << rxq->cqe_n);
	const unsigned int sges_n = (1 << rxq->sges_n);
	const unsigned int elts_n = (1 << rxq->elts_n);
	const unsigned int strd_n = RTE_BIT32(rxq->log_strd_num);
	const unsigned int cqe_cnt = cqe_n - 1;
	unsigned int cq_ci, used;

	/* if we are processing a compressed cqe */
	if (zip->ai) {
		used = zip->cqe_cnt - zip->ai;
		cq_ci = zip->cq_ci;
	} else {
		used = 0;
		cq_ci = rxq->cq_ci;
	}
	cqe = &(*rxq->cqes)[cq_ci & cqe_cnt];
	while (check_cqe(cqe, cqe_n, cq_ci) != MLX5_CQE_STATUS_HW_OWN) {
		int8_t op_own;
		unsigned int n;

		op_own = cqe->op_own;
		if (MLX5_CQE_FORMAT(op_own) == MLX5_COMPRESSED)
			n = rte_be_to_cpu_32(cqe->byte_cnt);
		else
			n = 1;
		cq_ci += n;
		used += n;
		cqe = &(*rxq->cqes)[cq_ci & cqe_cnt];
	}
	used = RTE_MIN(used * sges_n, elts_n * strd_n);
	return used;
}

/**
 * DPDK callback to check the status of a Rx descriptor.
 *
 * @param rx_queue
 *   The Rx queue.
 * @param[in] offset
 *   The index of the descriptor in the ring.
 *
 * @return
 *   The status of the Rx descriptor.
 */
int
mlx5_rx_descriptor_status(void *rx_queue, uint16_t offset)
{
	struct mlx5_rxq_data *rxq = rx_queue;

	if (offset >= (1 << rxq->cqe_n)) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (offset < rx_queue_count(rxq))
		return RTE_ETH_RX_DESC_DONE;
	return RTE_ETH_RX_DESC_AVAIL;
}

/**
 * DPDK callback to get the RX queue information.
 *
 * @param dev
 *   Pointer to the device structure.
 *
 * @param rx_queue_id
 *   Rx queue identificator.
 *
 * @param qinfo
 *   Pointer to the RX queue information structure.
 *
 * @return
 *   None.
 */

void
mlx5_rxq_info_get(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		  struct rte_eth_rxq_info *qinfo)
{
	struct mlx5_rxq_ctrl *rxq_ctrl = mlx5_rxq_ctrl_get(dev, rx_queue_id);
	struct mlx5_rxq_data *rxq = mlx5_rxq_data_get(dev, rx_queue_id);

	if (!rxq)
		return;
	qinfo->mp = mlx5_rxq_mprq_enabled(rxq) ?
					rxq->mprq_mp : rxq->mp;
	qinfo->conf.rx_thresh.pthresh = 0;
	qinfo->conf.rx_thresh.hthresh = 0;
	qinfo->conf.rx_thresh.wthresh = 0;
	qinfo->conf.rx_free_thresh = rxq->rq_repl_thresh;
	qinfo->conf.rx_drop_en = 1;
	if (rxq_ctrl == NULL || rxq_ctrl->obj == NULL)
		qinfo->conf.rx_deferred_start = 0;
	else
		qinfo->conf.rx_deferred_start = 1;
	qinfo->conf.offloads = dev->data->dev_conf.rxmode.offloads;
	qinfo->scattered_rx = dev->data->scattered_rx;
	qinfo->nb_desc = mlx5_rxq_mprq_enabled(rxq) ?
		RTE_BIT32(rxq->elts_n) * RTE_BIT32(rxq->log_strd_num) :
		RTE_BIT32(rxq->elts_n);
}

/**
 * DPDK callback to get the RX packet burst mode information.
 *
 * @param dev
 *   Pointer to the device structure.
 *
 * @param rx_queue_id
 *   Rx queue identification.
 *
 * @param mode
 *   Pointer to the burts mode information.
 *
 * @return
 *   0 as success, -EINVAL as failure.
 */
int
mlx5_rx_burst_mode_get(struct rte_eth_dev *dev,
		       uint16_t rx_queue_id __rte_unused,
		       struct rte_eth_burst_mode *mode)
{
	eth_rx_burst_t pkt_burst = dev->rx_pkt_burst;
	struct mlx5_rxq_priv *rxq = mlx5_rxq_get(dev, rx_queue_id);

	if (!rxq) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (pkt_burst == mlx5_rx_burst) {
		snprintf(mode->info, sizeof(mode->info), "%s", "Scalar");
	} else if (pkt_burst == mlx5_rx_burst_mprq) {
		snprintf(mode->info, sizeof(mode->info), "%s", "Multi-Packet RQ");
	} else if (pkt_burst == mlx5_rx_burst_vec) {
#if defined RTE_ARCH_X86_64
		snprintf(mode->info, sizeof(mode->info), "%s", "Vector SSE");
#elif defined RTE_ARCH_ARM64
		snprintf(mode->info, sizeof(mode->info), "%s", "Vector Neon");
#elif defined RTE_ARCH_PPC_64
		snprintf(mode->info, sizeof(mode->info), "%s", "Vector AltiVec");
#else
		return -EINVAL;
#endif
	} else if (pkt_burst == mlx5_rx_burst_mprq_vec) {
#if defined RTE_ARCH_X86_64
		snprintf(mode->info, sizeof(mode->info), "%s", "MPRQ Vector SSE");
#elif defined RTE_ARCH_ARM64
		snprintf(mode->info, sizeof(mode->info), "%s", "MPRQ Vector Neon");
#elif defined RTE_ARCH_PPC_64
		snprintf(mode->info, sizeof(mode->info), "%s", "MPRQ Vector AltiVec");
#else
		return -EINVAL;
#endif
	} else {
		return -EINVAL;
	}
	return 0;
}

/**
 * DPDK callback to get the number of used descriptors in a RX queue.
 *
 * @param rx_queue
 *   The Rx queue pointer.
 *
 * @return
 *   The number of used rx descriptor.
 *   -EINVAL if the queue is invalid
 */
uint32_t
mlx5_rx_queue_count(void *rx_queue)
{
	struct mlx5_rxq_data *rxq = rx_queue;
	struct rte_eth_dev *dev;

	if (!rxq) {
		rte_errno = EINVAL;
		return -rte_errno;
	}

	dev = &rte_eth_devices[rxq->port_id];

	if (dev->rx_pkt_burst == NULL ||
	    dev->rx_pkt_burst == removed_rx_burst) {
		rte_errno = ENOTSUP;
		return -rte_errno;
	}

	return rx_queue_count(rxq);
}

#define CLB_VAL_IDX 0
#define CLB_MSK_IDX 1
static int
mlx5_monitor_callback(const uint64_t value,
		const uint64_t opaque[RTE_POWER_MONITOR_OPAQUE_SZ])
{
	const uint64_t m = opaque[CLB_MSK_IDX];
	const uint64_t v = opaque[CLB_VAL_IDX];

	return (value & m) == v ? -1 : 0;
}

int mlx5_get_monitor_addr(void *rx_queue, struct rte_power_monitor_cond *pmc)
{
	struct mlx5_rxq_data *rxq = rx_queue;
	const unsigned int cqe_num = 1 << rxq->cqe_n;
	const unsigned int cqe_mask = cqe_num - 1;
	const uint16_t idx = rxq->cq_ci & cqe_num;
	volatile struct mlx5_cqe *cqe = &(*rxq->cqes)[rxq->cq_ci & cqe_mask];

	if (unlikely(rxq->cqes == NULL)) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	pmc->addr = &cqe->op_own;
	pmc->opaque[CLB_VAL_IDX] = !!idx;
	pmc->opaque[CLB_MSK_IDX] = MLX5_CQE_OWNER_MASK;
	pmc->fn = mlx5_monitor_callback;
	pmc->size = sizeof(uint8_t);
	return 0;
}

/**
 * Translate RX completion flags to packet type.
 *
 * @param[in] rxq
 *   Pointer to RX queue structure.
 * @param[in] cqe
 *   Pointer to CQE.
 *
 * @note: fix mlx5_dev_supported_ptypes_get() if any change here.
 *
 * @return
 *   Packet type for struct rte_mbuf.
 */
static inline uint32_t
rxq_cq_to_pkt_type(struct mlx5_rxq_data *rxq, volatile struct mlx5_cqe *cqe,
				   volatile struct mlx5_mini_cqe8 *mcqe)
{
	uint8_t idx;
	uint8_t ptype;
	uint8_t pinfo = (cqe->pkt_info & 0x3) << 6;

	/* Get l3/l4 header from mini-CQE in case L3/L4 format*/
	if (mcqe == NULL ||
	    rxq->mcqe_format != MLX5_CQE_RESP_FORMAT_L34H_STRIDX)
		ptype = (cqe->hdr_type_etc & 0xfc00) >> 10;
	else
		ptype = mcqe->hdr_type >> 2;
	/*
	 * The index to the array should have:
	 * bit[1:0] = l3_hdr_type
	 * bit[4:2] = l4_hdr_type
	 * bit[5] = ip_frag
	 * bit[6] = tunneled
	 * bit[7] = outer_l3_type
	 */
	idx = pinfo | ptype;
	return mlx5_ptype_table[idx] | rxq->tunnel * !!(idx & (1 << 6));
}

/**
 * Initialize Rx WQ and indexes.
 *
 * @param[in] rxq
 *   Pointer to RX queue structure.
 */
void
mlx5_rxq_initialize(struct mlx5_rxq_data *rxq)
{
	const unsigned int wqe_n = 1 << rxq->elts_n;
	unsigned int i;

	for (i = 0; (i != wqe_n); ++i) {
		volatile struct mlx5_wqe_data_seg *scat;
		uintptr_t addr;
		uint32_t byte_count;
		uint32_t lkey;

		if (mlx5_rxq_mprq_enabled(rxq)) {
			struct mlx5_mprq_buf *buf = (*rxq->mprq_bufs)[i];

			scat = &((volatile struct mlx5_wqe_mprq *)
				rxq->wqes)[i].dseg;
			addr = (uintptr_t)mlx5_mprq_buf_addr
					(buf, RTE_BIT32(rxq->log_strd_num));
			byte_count = RTE_BIT32(rxq->log_strd_sz) *
				     RTE_BIT32(rxq->log_strd_num);
			lkey = mlx5_rx_addr2mr(rxq, addr);
		} else {
			struct rte_mbuf *buf = (*rxq->elts)[i];

			scat = &((volatile struct mlx5_wqe_data_seg *)
					rxq->wqes)[i];
			addr = rte_pktmbuf_mtod(buf, uintptr_t);
			byte_count = DATA_LEN(buf);
			lkey = mlx5_rx_mb2mr(rxq, buf);
		}
		/* scat->addr must be able to store a pointer. */
		MLX5_ASSERT(sizeof(scat->addr) >= sizeof(uintptr_t));
		*scat = (struct mlx5_wqe_data_seg){
			.addr = rte_cpu_to_be_64(addr),
			.byte_count = rte_cpu_to_be_32(byte_count),
			.lkey = lkey,
		};
	}
	rxq->consumed_strd = 0;
	rxq->decompressed = 0;
	rxq->rq_pi = 0;
	rxq->zip = (struct rxq_zip){
		.ai = 0,
	};
	rxq->elts_ci = mlx5_rxq_mprq_enabled(rxq) ?
		(wqe_n >> rxq->sges_n) * RTE_BIT32(rxq->log_strd_num) : 0;
	/* Update doorbell counter. */
	rxq->rq_ci = wqe_n >> rxq->sges_n;
	rte_io_wmb();
	*rxq->rq_db = rte_cpu_to_be_32(rxq->rq_ci);
}

/* Must be negative. */
#define MLX5_ERROR_CQE_RET (-1)
/* Must not be negative. */
#define MLX5_RECOVERY_ERROR_RET 0

/**
 * Handle a Rx error.
 * The function inserts the RQ state to reset when the first error CQE is
 * shown, then drains the CQ by the caller function loop. When the CQ is empty,
 * it moves the RQ state to ready and initializes the RQ.
 * Next CQE identification and error counting are in the caller responsibility.
 *
 * @param[in] rxq
 *   Pointer to RX queue structure.
 * @param[in] vec
 *   1 when called from vectorized Rx burst, need to prepare mbufs for the RQ.
 *   0 when called from non-vectorized Rx burst.
 *
 * @return
 *   MLX5_RECOVERY_ERROR_RET in case of recovery error, otherwise the CQE status.
 */
int
mlx5_rx_err_handle(struct mlx5_rxq_data *rxq, uint8_t vec)
{
	const uint16_t cqe_n = 1 << rxq->cqe_n;
	const uint16_t cqe_mask = cqe_n - 1;
	const uint16_t wqe_n = 1 << rxq->elts_n;
	const uint16_t strd_n = RTE_BIT32(rxq->log_strd_num);
	struct mlx5_rxq_ctrl *rxq_ctrl =
			container_of(rxq, struct mlx5_rxq_ctrl, rxq);
	union {
		volatile struct mlx5_cqe *cqe;
		volatile struct mlx5_err_cqe *err_cqe;
	} u = {
		.cqe = &(*rxq->cqes)[rxq->cq_ci & cqe_mask],
	};
	struct mlx5_mp_arg_queue_state_modify sm;
	int ret;

	switch (rxq->err_state) {
	case MLX5_RXQ_ERR_STATE_NO_ERROR:
		rxq->err_state = MLX5_RXQ_ERR_STATE_NEED_RESET;
		/* Fall-through */
	case MLX5_RXQ_ERR_STATE_NEED_RESET:
		sm.is_wq = 1;
		sm.queue_id = rxq->idx;
		sm.state = IBV_WQS_RESET;
		if (mlx5_queue_state_modify(RXQ_DEV(rxq_ctrl), &sm))
			return MLX5_RECOVERY_ERROR_RET;
		if (rxq_ctrl->dump_file_n <
		    RXQ_PORT(rxq_ctrl)->config.max_dump_files_num) {
			MKSTR(err_str, "Unexpected CQE error syndrome "
			      "0x%02x CQN = %u RQN = %u wqe_counter = %u"
			      " rq_ci = %u cq_ci = %u", u.err_cqe->syndrome,
			      rxq->cqn, rxq_ctrl->wqn,
			      rte_be_to_cpu_16(u.err_cqe->wqe_counter),
			      rxq->rq_ci << rxq->sges_n, rxq->cq_ci);
			MKSTR(name, "dpdk_mlx5_port_%u_rxq_%u_%u",
			      rxq->port_id, rxq->idx, (uint32_t)rte_rdtsc());
			mlx5_dump_debug_information(name, NULL, err_str, 0);
			mlx5_dump_debug_information(name, "MLX5 Error CQ:",
						    (const void *)((uintptr_t)
								    rxq->cqes),
						    sizeof(*u.cqe) * cqe_n);
			mlx5_dump_debug_information(name, "MLX5 Error RQ:",
						    (const void *)((uintptr_t)
								    rxq->wqes),
						    16 * wqe_n);
			rxq_ctrl->dump_file_n++;
		}
		rxq->err_state = MLX5_RXQ_ERR_STATE_NEED_READY;
		/* Fall-through */
	case MLX5_RXQ_ERR_STATE_NEED_READY:
		ret = check_cqe(u.cqe, cqe_n, rxq->cq_ci);
		if (ret == MLX5_CQE_STATUS_HW_OWN) {
			rte_io_wmb();
			*rxq->cq_db = rte_cpu_to_be_32(rxq->cq_ci);
			rte_io_wmb();
			/*
			 * The RQ consumer index must be zeroed while moving
			 * from RESET state to RDY state.
			 */
			*rxq->rq_db = rte_cpu_to_be_32(0);
			rte_io_wmb();
			sm.is_wq = 1;
			sm.queue_id = rxq->idx;
			sm.state = IBV_WQS_RDY;
			if (mlx5_queue_state_modify(RXQ_DEV(rxq_ctrl), &sm))
				return MLX5_RECOVERY_ERROR_RET;
			if (vec) {
				const uint32_t elts_n =
					mlx5_rxq_mprq_enabled(rxq) ?
					wqe_n * strd_n : wqe_n;
				const uint32_t e_mask = elts_n - 1;
				uint32_t elts_ci =
					mlx5_rxq_mprq_enabled(rxq) ?
					rxq->elts_ci : rxq->rq_ci;
				uint32_t elt_idx;
				struct rte_mbuf **elt;
				int i;
				unsigned int n = elts_n - (elts_ci -
							  rxq->rq_pi);

				for (i = 0; i < (int)n; ++i) {
					elt_idx = (elts_ci + i) & e_mask;
					elt = &(*rxq->elts)[elt_idx];
					*elt = rte_mbuf_raw_alloc(rxq->mp);
					if (!*elt) {
						for (i--; i >= 0; --i) {
							elt_idx = (elts_ci +
								   i) & elts_n;
							elt = &(*rxq->elts)
								[elt_idx];
							rte_pktmbuf_free_seg
								(*elt);
						}
						return MLX5_RECOVERY_ERROR_RET;
					}
				}
				for (i = 0; i < (int)elts_n; ++i) {
					elt = &(*rxq->elts)[i];
					DATA_LEN(*elt) =
						(uint16_t)((*elt)->buf_len -
						rte_pktmbuf_headroom(*elt));
				}
				/* Padding with a fake mbuf for vec Rx. */
				for (i = 0; i < MLX5_VPMD_DESCS_PER_LOOP; ++i)
					(*rxq->elts)[elts_n + i] =
								&rxq->fake_mbuf;
			}
			mlx5_rxq_initialize(rxq);
			rxq->err_state = MLX5_RXQ_ERR_STATE_NO_ERROR;
		}
		return ret;
	default:
		return MLX5_RECOVERY_ERROR_RET;
	}
}

/**
 * Get size of the next packet for a given CQE. For compressed CQEs, the
 * consumer index is updated only once all packets of the current one have
 * been processed.
 *
 * @param rxq
 *   Pointer to RX queue.
 * @param cqe
 *   CQE to process.
 * @param[out] mcqe
 *   Store pointer to mini-CQE if compressed. Otherwise, the pointer is not
 *   written.
 *
 * @return
 *   0 in case of empty CQE, MLX5_ERROR_CQE_RET in case of error CQE,
 *   otherwise the packet size in regular RxQ, and striding byte
 *   count format in mprq case.
 */
static inline int
mlx5_rx_poll_len(struct mlx5_rxq_data *rxq, volatile struct mlx5_cqe *cqe,
		 uint16_t cqe_cnt, volatile struct mlx5_mini_cqe8 **mcqe)
{
	struct rxq_zip *zip = &rxq->zip;
	uint16_t cqe_n = cqe_cnt + 1;
	int len;
	uint16_t idx, end;

	do {
		len = 0;
		/* Process compressed data in the CQE and mini arrays. */
		if (zip->ai) {
			volatile struct mlx5_mini_cqe8 (*mc)[8] =
				(volatile struct mlx5_mini_cqe8 (*)[8])
				(uintptr_t)(&(*rxq->cqes)[zip->ca &
							  cqe_cnt].pkt_info);
			len = rte_be_to_cpu_32((*mc)[zip->ai & 7].byte_cnt &
					       rxq->byte_mask);
			*mcqe = &(*mc)[zip->ai & 7];
			if ((++zip->ai & 7) == 0) {
				/* Invalidate consumed CQEs */
				idx = zip->ca;
				end = zip->na;
				while (idx != end) {
					(*rxq->cqes)[idx & cqe_cnt].op_own =
						MLX5_CQE_INVALIDATE;
					++idx;
				}
				/*
				 * Increment consumer index to skip the number
				 * of CQEs consumed. Hardware leaves holes in
				 * the CQ ring for software use.
				 */
				zip->ca = zip->na;
				zip->na += 8;
			}
			if (unlikely(rxq->zip.ai == rxq->zip.cqe_cnt)) {
				/* Invalidate the rest */
				idx = zip->ca;
				end = zip->cq_ci;

				while (idx != end) {
					(*rxq->cqes)[idx & cqe_cnt].op_own =
						MLX5_CQE_INVALIDATE;
					++idx;
				}
				rxq->cq_ci = zip->cq_ci;
				zip->ai = 0;
			}
		/*
		 * No compressed data, get next CQE and verify if it is
		 * compressed.
		 */
		} else {
			int ret;
			int8_t op_own;
			uint32_t cq_ci;

			ret = check_cqe(cqe, cqe_n, rxq->cq_ci);
			if (unlikely(ret != MLX5_CQE_STATUS_SW_OWN)) {
				if (unlikely(ret == MLX5_CQE_STATUS_ERR ||
					     rxq->err_state)) {
					ret = mlx5_rx_err_handle(rxq, 0);
					if (ret == MLX5_CQE_STATUS_HW_OWN ||
					    ret == MLX5_RECOVERY_ERROR_RET)
						return MLX5_ERROR_CQE_RET;
				} else {
					return 0;
				}
			}
			/*
			 * Introduce the local variable to have queue cq_ci
			 * index in queue structure always consistent with
			 * actual CQE boundary (not pointing to the middle
			 * of compressed CQE session).
			 */
			cq_ci = rxq->cq_ci + 1;
			op_own = cqe->op_own;
			if (MLX5_CQE_FORMAT(op_own) == MLX5_COMPRESSED) {
				volatile struct mlx5_mini_cqe8 (*mc)[8] =
					(volatile struct mlx5_mini_cqe8 (*)[8])
					(uintptr_t)(&(*rxq->cqes)
						[cq_ci & cqe_cnt].pkt_info);

				/* Fix endianness. */
				zip->cqe_cnt = rte_be_to_cpu_32(cqe->byte_cnt);
				/*
				 * Current mini array position is the one
				 * returned by check_cqe64().
				 *
				 * If completion comprises several mini arrays,
				 * as a special case the second one is located
				 * 7 CQEs after the initial CQE instead of 8
				 * for subsequent ones.
				 */
				zip->ca = cq_ci;
				zip->na = zip->ca + 7;
				/* Compute the next non compressed CQE. */
				zip->cq_ci = rxq->cq_ci + zip->cqe_cnt;
				/* Get packet size to return. */
				len = rte_be_to_cpu_32((*mc)[0].byte_cnt &
						       rxq->byte_mask);
				*mcqe = &(*mc)[0];
				zip->ai = 1;
				/* Prefetch all to be invalidated */
				idx = zip->ca;
				end = zip->cq_ci;
				while (idx != end) {
					rte_prefetch0(&(*rxq->cqes)[(idx) &
								    cqe_cnt]);
					++idx;
				}
			} else {
				rxq->cq_ci = cq_ci;
				len = rte_be_to_cpu_32(cqe->byte_cnt);
			}
		}
		if (unlikely(rxq->err_state)) {
			cqe = &(*rxq->cqes)[rxq->cq_ci & cqe_cnt];
			++rxq->stats.idropped;
		} else {
			return len;
		}
	} while (1);
}

/**
 * Translate RX completion flags to offload flags.
 *
 * @param[in] cqe
 *   Pointer to CQE.
 *
 * @return
 *   Offload flags (ol_flags) for struct rte_mbuf.
 */
static inline uint32_t
rxq_cq_to_ol_flags(volatile struct mlx5_cqe *cqe)
{
	uint32_t ol_flags = 0;
	uint16_t flags = rte_be_to_cpu_16(cqe->hdr_type_etc);

	ol_flags =
		TRANSPOSE(flags,
			  MLX5_CQE_RX_L3_HDR_VALID,
			  RTE_MBUF_F_RX_IP_CKSUM_GOOD) |
		TRANSPOSE(flags,
			  MLX5_CQE_RX_L4_HDR_VALID,
			  RTE_MBUF_F_RX_L4_CKSUM_GOOD);
	return ol_flags;
}

/**
 * Fill in mbuf fields from RX completion flags.
 * Note that pkt->ol_flags should be initialized outside of this function.
 *
 * @param rxq
 *   Pointer to RX queue.
 * @param pkt
 *   mbuf to fill.
 * @param cqe
 *   CQE to process.
 * @param rss_hash_res
 *   Packet RSS Hash result.
 */
static inline void
rxq_cq_to_mbuf(struct mlx5_rxq_data *rxq, struct rte_mbuf *pkt,
	       volatile struct mlx5_cqe *cqe,
	       volatile struct mlx5_mini_cqe8 *mcqe)
{
	/* Update packet information. */
	pkt->packet_type = rxq_cq_to_pkt_type(rxq, cqe, mcqe);
	pkt->port = unlikely(rxq->shared) ? cqe->user_index_low : rxq->port_id;

	if (rxq->rss_hash) {
		uint32_t rss_hash_res = 0;

		/* If compressed, take hash result from mini-CQE. */
		if (mcqe == NULL ||
		    rxq->mcqe_format != MLX5_CQE_RESP_FORMAT_HASH)
			rss_hash_res = rte_be_to_cpu_32(cqe->rx_hash_res);
		else
			rss_hash_res = rte_be_to_cpu_32(mcqe->rx_hash_result);
		if (rss_hash_res) {
			pkt->hash.rss = rss_hash_res;
			pkt->ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
		}
	}
	if (rxq->mark) {
		uint32_t mark = 0;

		/* If compressed, take flow tag from mini-CQE. */
		if (mcqe == NULL ||
		    rxq->mcqe_format != MLX5_CQE_RESP_FORMAT_FTAG_STRIDX)
			mark = cqe->sop_drop_qpn;
		else
			mark = ((mcqe->byte_cnt_flow & 0xff) << 8) |
				(mcqe->flow_tag_high << 16);
		if (MLX5_FLOW_MARK_IS_VALID(mark)) {
			pkt->ol_flags |= RTE_MBUF_F_RX_FDIR;
			if (mark != RTE_BE32(MLX5_FLOW_MARK_DEFAULT)) {
				pkt->ol_flags |= RTE_MBUF_F_RX_FDIR_ID;
				pkt->hash.fdir.hi = mlx5_flow_mark_get(mark);
			}
		}
	}
	if (rxq->dynf_meta) {
		uint32_t meta = rte_be_to_cpu_32(cqe->flow_table_metadata) &
			rxq->flow_meta_port_mask;

		if (meta) {
			pkt->ol_flags |= rxq->flow_meta_mask;
			*RTE_MBUF_DYNFIELD(pkt, rxq->flow_meta_offset,
						uint32_t *) = meta;
		}
	}
	if (rxq->csum)
		pkt->ol_flags |= rxq_cq_to_ol_flags(cqe);
	if (rxq->vlan_strip) {
		bool vlan_strip;

		if (mcqe == NULL ||
		    rxq->mcqe_format != MLX5_CQE_RESP_FORMAT_L34H_STRIDX)
			vlan_strip = cqe->hdr_type_etc &
				     RTE_BE16(MLX5_CQE_VLAN_STRIPPED);
		else
			vlan_strip = mcqe->hdr_type &
				     RTE_BE16(MLX5_CQE_VLAN_STRIPPED);
		if (vlan_strip) {
			pkt->ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
			pkt->vlan_tci = rte_be_to_cpu_16(cqe->vlan_info);
		}
	}
	if (rxq->hw_timestamp) {
		uint64_t ts = rte_be_to_cpu_64(cqe->timestamp);

		if (rxq->rt_timestamp)
			ts = mlx5_txpp_convert_rx_ts(rxq->sh, ts);
		mlx5_timestamp_set(pkt, rxq->timestamp_offset, ts);
		pkt->ol_flags |= rxq->timestamp_rx_flag;
	}
}

/**
 * DPDK callback for RX.
 *
 * @param dpdk_rxq
 *   Generic pointer to RX queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
uint16_t
mlx5_rx_burst(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct mlx5_rxq_data *rxq = dpdk_rxq;
	const unsigned int wqe_cnt = (1 << rxq->elts_n) - 1;
	const unsigned int cqe_cnt = (1 << rxq->cqe_n) - 1;
	const unsigned int sges_n = rxq->sges_n;
	struct rte_mbuf *pkt = NULL;
	struct rte_mbuf *seg = NULL;
	volatile struct mlx5_cqe *cqe =
		&(*rxq->cqes)[rxq->cq_ci & cqe_cnt];
	unsigned int i = 0;
	unsigned int rq_ci = rxq->rq_ci << sges_n;
	int len = 0; /* keep its value across iterations. */

	while (pkts_n) {
		unsigned int idx = rq_ci & wqe_cnt;
		volatile struct mlx5_wqe_data_seg *wqe =
			&((volatile struct mlx5_wqe_data_seg *)rxq->wqes)[idx];
		struct rte_mbuf *rep = (*rxq->elts)[idx];
		volatile struct mlx5_mini_cqe8 *mcqe = NULL;

		if (pkt)
			NEXT(seg) = rep;
		seg = rep;
		rte_prefetch0(seg);
		rte_prefetch0(cqe);
		rte_prefetch0(wqe);
		/* Allocate the buf from the same pool. */
		rep = rte_mbuf_raw_alloc(seg->pool);
		if (unlikely(rep == NULL)) {
			++rxq->stats.rx_nombuf;
			if (!pkt) {
				/*
				 * no buffers before we even started,
				 * bail out silently.
				 */
				break;
			}
			while (pkt != seg) {
				MLX5_ASSERT(pkt != (*rxq->elts)[idx]);
				rep = NEXT(pkt);
				NEXT(pkt) = NULL;
				NB_SEGS(pkt) = 1;
				rte_mbuf_raw_free(pkt);
				pkt = rep;
			}
			rq_ci >>= sges_n;
			++rq_ci;
			rq_ci <<= sges_n;
			break;
		}
		if (!pkt) {
			cqe = &(*rxq->cqes)[rxq->cq_ci & cqe_cnt];
			len = mlx5_rx_poll_len(rxq, cqe, cqe_cnt, &mcqe);
			if (len <= 0) {
				rte_mbuf_raw_free(rep);
				if (unlikely(len == MLX5_ERROR_CQE_RET))
					rq_ci = rxq->rq_ci << sges_n;
				break;
			}
			pkt = seg;
			MLX5_ASSERT(len >= (rxq->crc_present << 2));
			pkt->ol_flags &= RTE_MBUF_F_EXTERNAL;
			rxq_cq_to_mbuf(rxq, pkt, cqe, mcqe);
			if (rxq->crc_present)
				len -= RTE_ETHER_CRC_LEN;
			PKT_LEN(pkt) = len;
			if (cqe->lro_num_seg > 1) {
				mlx5_lro_update_hdr
					(rte_pktmbuf_mtod(pkt, uint8_t *), cqe,
					 mcqe, rxq, len);
				pkt->ol_flags |= RTE_MBUF_F_RX_LRO;
				pkt->tso_segsz = len / cqe->lro_num_seg;
			}
		}
		DATA_LEN(rep) = DATA_LEN(seg);
		PKT_LEN(rep) = PKT_LEN(seg);
		SET_DATA_OFF(rep, DATA_OFF(seg));
		PORT(rep) = PORT(seg);
		(*rxq->elts)[idx] = rep;
		/*
		 * Fill NIC descriptor with the new buffer. The lkey and size
		 * of the buffers are already known, only the buffer address
		 * changes.
		 */
		wqe->addr = rte_cpu_to_be_64(rte_pktmbuf_mtod(rep, uintptr_t));
		/* If there's only one MR, no need to replace LKey in WQE. */
		if (unlikely(mlx5_mr_btree_len(&rxq->mr_ctrl.cache_bh) > 1))
			wqe->lkey = mlx5_rx_mb2mr(rxq, rep);
		if (len > DATA_LEN(seg)) {
			len -= DATA_LEN(seg);
			++NB_SEGS(pkt);
			++rq_ci;
			continue;
		}
		DATA_LEN(seg) = len;
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Increment bytes counter. */
		rxq->stats.ibytes += PKT_LEN(pkt);
#endif
		/* Return packet. */
		*(pkts++) = pkt;
		pkt = NULL;
		--pkts_n;
		++i;
		/* Align consumer index to the next stride. */
		rq_ci >>= sges_n;
		++rq_ci;
		rq_ci <<= sges_n;
	}
	if (unlikely(i == 0 && ((rq_ci >> sges_n) == rxq->rq_ci)))
		return 0;
	/* Update the consumer index. */
	rxq->rq_ci = rq_ci >> sges_n;
	rte_io_wmb();
	*rxq->cq_db = rte_cpu_to_be_32(rxq->cq_ci);
	rte_io_wmb();
	*rxq->rq_db = rte_cpu_to_be_32(rxq->rq_ci);
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Increment packets counter. */
	rxq->stats.ipackets += i;
#endif
	return i;
}

/**
 * Update LRO packet TCP header.
 * The HW LRO feature doesn't update the TCP header after coalescing the
 * TCP segments but supplies information in CQE to fill it by SW.
 *
 * @param tcp
 *   Pointer to the TCP header.
 * @param cqe
 *   Pointer to the completion entry.
 * @param phcsum
 *   The L3 pseudo-header checksum.
 */
static inline void
mlx5_lro_update_tcp_hdr(struct rte_tcp_hdr *__rte_restrict tcp,
			volatile struct mlx5_cqe *__rte_restrict cqe,
			uint32_t phcsum, uint8_t l4_type)
{
	/*
	 * The HW calculates only the TCP payload checksum, need to complete
	 * the TCP header checksum and the L3 pseudo-header checksum.
	 */
	uint32_t csum = phcsum + cqe->csum;

	if (l4_type == MLX5_L4_HDR_TYPE_TCP_EMPTY_ACK ||
	    l4_type == MLX5_L4_HDR_TYPE_TCP_WITH_ACL) {
		tcp->tcp_flags |= RTE_TCP_ACK_FLAG;
		tcp->recv_ack = cqe->lro_ack_seq_num;
		tcp->rx_win = cqe->lro_tcp_win;
	}
	if (cqe->lro_tcppsh_abort_dupack & MLX5_CQE_LRO_PUSH_MASK)
		tcp->tcp_flags |= RTE_TCP_PSH_FLAG;
	tcp->cksum = 0;
	csum += rte_raw_cksum(tcp, (tcp->data_off >> 4) * 4);
	csum = ((csum & 0xffff0000) >> 16) + (csum & 0xffff);
	csum = (~csum) & 0xffff;
	if (csum == 0)
		csum = 0xffff;
	tcp->cksum = csum;
}

/**
 * Update LRO packet headers.
 * The HW LRO feature doesn't update the L3/TCP headers after coalescing the
 * TCP segments but supply information in CQE to fill it by SW.
 *
 * @param padd
 *   The packet address.
 * @param cqe
 *   Pointer to the completion entry.
 * @param len
 *   The packet length.
 */
static inline void
mlx5_lro_update_hdr(uint8_t *__rte_restrict padd,
		    volatile struct mlx5_cqe *__rte_restrict cqe,
		    volatile struct mlx5_mini_cqe8 *mcqe,
		    struct mlx5_rxq_data *rxq, uint32_t len)
{
	union {
		struct rte_ether_hdr *eth;
		struct rte_vlan_hdr *vlan;
		struct rte_ipv4_hdr *ipv4;
		struct rte_ipv6_hdr *ipv6;
		struct rte_tcp_hdr *tcp;
		uint8_t *hdr;
	} h = {
		.hdr = padd,
	};
	uint16_t proto = h.eth->ether_type;
	uint32_t phcsum;
	uint8_t l4_type;

	h.eth++;
	while (proto == RTE_BE16(RTE_ETHER_TYPE_VLAN) ||
	       proto == RTE_BE16(RTE_ETHER_TYPE_QINQ)) {
		proto = h.vlan->eth_proto;
		h.vlan++;
	}
	if (proto == RTE_BE16(RTE_ETHER_TYPE_IPV4)) {
		h.ipv4->time_to_live = cqe->lro_min_ttl;
		h.ipv4->total_length = rte_cpu_to_be_16(len - (h.hdr - padd));
		h.ipv4->hdr_checksum = 0;
		h.ipv4->hdr_checksum = rte_ipv4_cksum(h.ipv4);
		phcsum = rte_ipv4_phdr_cksum(h.ipv4, 0);
		h.ipv4++;
	} else {
		h.ipv6->hop_limits = cqe->lro_min_ttl;
		h.ipv6->payload_len = rte_cpu_to_be_16(len - (h.hdr - padd) -
						       sizeof(*h.ipv6));
		phcsum = rte_ipv6_phdr_cksum(h.ipv6, 0);
		h.ipv6++;
	}
	if (mcqe == NULL ||
	    rxq->mcqe_format != MLX5_CQE_RESP_FORMAT_L34H_STRIDX)
		l4_type = (rte_be_to_cpu_16(cqe->hdr_type_etc) &
			   MLX5_CQE_L4_TYPE_MASK) >> MLX5_CQE_L4_TYPE_SHIFT;
	else
		l4_type = (rte_be_to_cpu_16(mcqe->hdr_type) &
			   MLX5_CQE_L4_TYPE_MASK) >> MLX5_CQE_L4_TYPE_SHIFT;
	mlx5_lro_update_tcp_hdr(h.tcp, cqe, phcsum, l4_type);
}

void
mlx5_mprq_buf_free(struct mlx5_mprq_buf *buf)
{
	mlx5_mprq_buf_free_cb(NULL, buf);
}

/**
 * DPDK callback for RX with Multi-Packet RQ support.
 *
 * @param dpdk_rxq
 *   Generic pointer to RX queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
uint16_t
mlx5_rx_burst_mprq(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct mlx5_rxq_data *rxq = dpdk_rxq;
	const uint32_t strd_n = RTE_BIT32(rxq->log_strd_num);
	const uint32_t strd_sz = RTE_BIT32(rxq->log_strd_sz);
	const uint32_t cq_mask = (1 << rxq->cqe_n) - 1;
	const uint32_t wq_mask = (1 << rxq->elts_n) - 1;
	volatile struct mlx5_cqe *cqe = &(*rxq->cqes)[rxq->cq_ci & cq_mask];
	unsigned int i = 0;
	uint32_t rq_ci = rxq->rq_ci;
	uint16_t consumed_strd = rxq->consumed_strd;
	struct mlx5_mprq_buf *buf = (*rxq->mprq_bufs)[rq_ci & wq_mask];

	while (i < pkts_n) {
		struct rte_mbuf *pkt;
		int ret;
		uint32_t len;
		uint16_t strd_cnt;
		uint16_t strd_idx;
		uint32_t byte_cnt;
		volatile struct mlx5_mini_cqe8 *mcqe = NULL;
		enum mlx5_rqx_code rxq_code;

		if (consumed_strd == strd_n) {
			/* Replace WQE if the buffer is still in use. */
			mprq_buf_replace(rxq, rq_ci & wq_mask);
			/* Advance to the next WQE. */
			consumed_strd = 0;
			++rq_ci;
			buf = (*rxq->mprq_bufs)[rq_ci & wq_mask];
		}
		cqe = &(*rxq->cqes)[rxq->cq_ci & cq_mask];
		ret = mlx5_rx_poll_len(rxq, cqe, cq_mask, &mcqe);
		if (ret == 0)
			break;
		if (unlikely(ret == MLX5_ERROR_CQE_RET)) {
			rq_ci = rxq->rq_ci;
			consumed_strd = rxq->consumed_strd;
			break;
		}
		byte_cnt = ret;
		len = (byte_cnt & MLX5_MPRQ_LEN_MASK) >> MLX5_MPRQ_LEN_SHIFT;
		MLX5_ASSERT((int)len >= (rxq->crc_present << 2));
		if (rxq->crc_present)
			len -= RTE_ETHER_CRC_LEN;
		if (mcqe &&
		    rxq->mcqe_format == MLX5_CQE_RESP_FORMAT_FTAG_STRIDX)
			strd_cnt = (len / strd_sz) + !!(len % strd_sz);
		else
			strd_cnt = (byte_cnt & MLX5_MPRQ_STRIDE_NUM_MASK) >>
				   MLX5_MPRQ_STRIDE_NUM_SHIFT;
		MLX5_ASSERT(strd_cnt);
		consumed_strd += strd_cnt;
		if (byte_cnt & MLX5_MPRQ_FILLER_MASK)
			continue;
		strd_idx = rte_be_to_cpu_16(mcqe == NULL ?
					cqe->wqe_counter :
					mcqe->stride_idx);
		MLX5_ASSERT(strd_idx < strd_n);
		MLX5_ASSERT(!((rte_be_to_cpu_16(cqe->wqe_id) ^ rq_ci) &
			    wq_mask));
		pkt = rte_pktmbuf_alloc(rxq->mp);
		if (unlikely(pkt == NULL)) {
			++rxq->stats.rx_nombuf;
			break;
		}
		len = (byte_cnt & MLX5_MPRQ_LEN_MASK) >> MLX5_MPRQ_LEN_SHIFT;
		MLX5_ASSERT((int)len >= (rxq->crc_present << 2));
		if (rxq->crc_present)
			len -= RTE_ETHER_CRC_LEN;
		rxq_code = mprq_buf_to_pkt(rxq, pkt, len, buf,
					   strd_idx, strd_cnt);
		if (unlikely(rxq_code != MLX5_RXQ_CODE_EXIT)) {
			rte_pktmbuf_free_seg(pkt);
			if (rxq_code == MLX5_RXQ_CODE_DROPPED) {
				++rxq->stats.idropped;
				continue;
			}
			if (rxq_code == MLX5_RXQ_CODE_NOMBUF) {
				++rxq->stats.rx_nombuf;
				break;
			}
		}
		rxq_cq_to_mbuf(rxq, pkt, cqe, mcqe);
		if (cqe->lro_num_seg > 1) {
			mlx5_lro_update_hdr(rte_pktmbuf_mtod(pkt, uint8_t *),
					    cqe, mcqe, rxq, len);
			pkt->ol_flags |= RTE_MBUF_F_RX_LRO;
			pkt->tso_segsz = len / cqe->lro_num_seg;
		}
		PKT_LEN(pkt) = len;
		PORT(pkt) = rxq->port_id;
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Increment bytes counter. */
		rxq->stats.ibytes += PKT_LEN(pkt);
#endif
		/* Return packet. */
		*(pkts++) = pkt;
		++i;
	}
	/* Update the consumer indexes. */
	rxq->consumed_strd = consumed_strd;
	rte_io_wmb();
	*rxq->cq_db = rte_cpu_to_be_32(rxq->cq_ci);
	if (rq_ci != rxq->rq_ci) {
		rxq->rq_ci = rq_ci;
		rte_io_wmb();
		*rxq->rq_db = rte_cpu_to_be_32(rxq->rq_ci);
	}
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Increment packets counter. */
	rxq->stats.ipackets += i;
#endif
	return i;
}

/**
 * Dummy DPDK callback for RX.
 *
 * This function is used to temporarily replace the real callback during
 * unsafe control operations on the queue, or in case of error.
 *
 * @param dpdk_rxq
 *   Generic pointer to RX queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
uint16_t
removed_rx_burst(void *dpdk_rxq __rte_unused,
		 struct rte_mbuf **pkts __rte_unused,
		 uint16_t pkts_n __rte_unused)
{
	rte_mb();
	return 0;
}

/*
 * Vectorized Rx routines are not compiled in when required vector instructions
 * are not supported on a target architecture.
 * The following null stubs are needed for linkage when those are not included
 * outside of this file (e.g. mlx5_rxtx_vec_sse.c for x86).
 */

__rte_weak uint16_t
mlx5_rx_burst_vec(void *dpdk_rxq __rte_unused,
		  struct rte_mbuf **pkts __rte_unused,
		  uint16_t pkts_n __rte_unused)
{
	return 0;
}

__rte_weak uint16_t
mlx5_rx_burst_mprq_vec(void *dpdk_rxq __rte_unused,
		       struct rte_mbuf **pkts __rte_unused,
		       uint16_t pkts_n __rte_unused)
{
	return 0;
}

__rte_weak int
mlx5_rxq_check_vec_support(struct mlx5_rxq_data *rxq __rte_unused)
{
	return -ENOTSUP;
}

__rte_weak int
mlx5_check_vec_rx_support(struct rte_eth_dev *dev __rte_unused)
{
	return -ENOTSUP;
}

