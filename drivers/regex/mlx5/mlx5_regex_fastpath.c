/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <unistd.h>
#include <strings.h>
#include <stdint.h>
#include <sys/mman.h>

#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_errno.h>
#include <rte_bus_pci.h>
#include <rte_pci.h>
#include <rte_regexdev_driver.h>
#include <rte_mbuf.h>

#include <infiniband/mlx5dv.h>
#include <mlx5_glue.h>
#include <mlx5_common.h>
#include <mlx5_prm.h>

#include "mlx5_regex_utils.h"
#include "mlx5_rxp.h"
#include "mlx5_regex.h"

#define MLX5_REGEX_MAX_WQE_INDEX 0xffff
#define MLX5_REGEX_METADATA_SIZE ((size_t)64)
#define MLX5_REGEX_MAX_OUTPUT (((size_t)1) << 11)
#define MLX5_REGEX_WQE_CTRL_OFFSET 12
#define MLX5_REGEX_WQE_METADATA_OFFSET 16
#define MLX5_REGEX_WQE_GATHER_OFFSET 32
#define MLX5_REGEX_WQE_SCATTER_OFFSET 48
#define MLX5_REGEX_METADATA_OFF 32
#define MLX5_REGEX_UMR_WQE_SIZE 192
/* The maximum KLMs can be added to one UMR indirect mkey. */
#define MLX5_REGEX_MAX_KLM_NUM 128
/* The KLM array size for one job. */
#define MLX5_REGEX_KLMS_SIZE \
	((MLX5_REGEX_MAX_KLM_NUM) * sizeof(struct mlx5_klm))
/* In WQE set mode, the pi should be quarter of the MLX5_REGEX_MAX_WQE_INDEX. */
#define MLX5_REGEX_UMR_QP_PI_IDX(pi, ops) \
	(((pi) + (ops)) & (MLX5_REGEX_MAX_WQE_INDEX >> 2))

static inline uint32_t
qp_size_get(struct mlx5_regex_hw_qp *qp)
{
	return (1U << qp->log_nb_desc);
}

static inline uint32_t
cq_size_get(struct mlx5_regex_cq *cq)
{
	return (1U << cq->log_nb_desc);
}

struct mlx5_regex_job {
	uint64_t user_id;
	volatile uint8_t *output;
	volatile uint8_t *metadata;
	struct mlx5_klm *imkey_array; /* Indirect mkey's KLM array. */
	struct mlx5_devx_obj *imkey; /* UMR WQE's indirect meky. */
} __rte_cached_aligned;

static inline void
set_data_seg(struct mlx5_wqe_data_seg *seg,
	     uint32_t length, uint32_t lkey,
	     uintptr_t address)
{
	seg->byte_count = rte_cpu_to_be_32(length);
	seg->lkey = rte_cpu_to_be_32(lkey);
	seg->addr = rte_cpu_to_be_64(address);
}

static inline void
set_metadata_seg(struct mlx5_wqe_metadata_seg *seg,
		 uint32_t mmo_control_31_0, uint32_t lkey,
		 uintptr_t address)
{
	seg->mmo_control_31_0 = htobe32(mmo_control_31_0);
	seg->lkey = rte_cpu_to_be_32(lkey);
	seg->addr = rte_cpu_to_be_64(address);
}

static inline void
set_regex_ctrl_seg(void *seg, uint8_t le, uint16_t subset_id0,
		   uint16_t subset_id1, uint16_t subset_id2,
		   uint16_t subset_id3, uint8_t ctrl)
{
	MLX5_SET(regexp_mmo_control, seg, le, le);
	MLX5_SET(regexp_mmo_control, seg, ctrl, ctrl);
	MLX5_SET(regexp_mmo_control, seg, subset_id_0, subset_id0);
	MLX5_SET(regexp_mmo_control, seg, subset_id_1, subset_id1);
	MLX5_SET(regexp_mmo_control, seg, subset_id_2, subset_id2);
	MLX5_SET(regexp_mmo_control, seg, subset_id_3, subset_id3);
}

static inline void
set_wqe_ctrl_seg(struct mlx5_wqe_ctrl_seg *seg, uint16_t pi, uint8_t opcode,
		 uint8_t opmod, uint32_t qp_num, uint8_t fm_ce_se, uint8_t ds,
		 uint8_t signature, uint32_t imm)
{
	seg->opmod_idx_opcode = rte_cpu_to_be_32(((uint32_t)opmod << 24) |
						 ((uint32_t)pi << 8) |
						 opcode);
	seg->qpn_ds = rte_cpu_to_be_32((qp_num << 8) | ds);
	seg->fm_ce_se = fm_ce_se;
	seg->signature = signature;
	seg->imm = imm;
}

static inline void
__prep_one(struct mlx5_regex_priv *priv, struct mlx5_regex_hw_qp *qp_obj,
	   struct rte_regex_ops *op, struct mlx5_regex_job *job,
	   size_t pi, struct mlx5_klm *klm)
{
	size_t wqe_offset = (pi & (qp_size_get(qp_obj) - 1)) *
			    (MLX5_SEND_WQE_BB << (priv->has_umr ? 2 : 0)) +
			    (priv->has_umr ? MLX5_REGEX_UMR_WQE_SIZE : 0);
	uint16_t group0 = op->req_flags & RTE_REGEX_OPS_REQ_GROUP_ID0_VALID_F ?
				op->group_id0 : 0;
	uint16_t group1 = op->req_flags & RTE_REGEX_OPS_REQ_GROUP_ID1_VALID_F ?
				op->group_id1 : 0;
	uint16_t group2 = op->req_flags & RTE_REGEX_OPS_REQ_GROUP_ID2_VALID_F ?
				op->group_id2 : 0;
	uint16_t group3 = op->req_flags & RTE_REGEX_OPS_REQ_GROUP_ID3_VALID_F ?
				op->group_id3 : 0;
	uint8_t control = op->req_flags &
				RTE_REGEX_OPS_REQ_MATCH_HIGH_PRIORITY_F ? 1 : 0;

	/* For backward compatibility. */
	if (!(op->req_flags & (RTE_REGEX_OPS_REQ_GROUP_ID0_VALID_F |
			       RTE_REGEX_OPS_REQ_GROUP_ID1_VALID_F |
			       RTE_REGEX_OPS_REQ_GROUP_ID2_VALID_F |
			       RTE_REGEX_OPS_REQ_GROUP_ID3_VALID_F)))
		group0 = op->group_id0;
	uint8_t *wqe = (uint8_t *)(uintptr_t)qp_obj->qp_obj.wqes + wqe_offset;
	int ds = 4; /*  ctrl + meta + input + output */

	set_wqe_ctrl_seg((struct mlx5_wqe_ctrl_seg *)wqe,
			 (priv->has_umr ? (pi * 4 + 3) : pi),
			 MLX5_OPCODE_MMO, MLX5_OPC_MOD_MMO_REGEX,
			 qp_obj->qp_obj.qp->id, 0, ds, 0, 0);
	set_regex_ctrl_seg(wqe + 12, 0, group0, group1, group2, group3,
			   control);
	struct mlx5_wqe_data_seg *input_seg =
		(struct mlx5_wqe_data_seg *)(wqe +
					     MLX5_REGEX_WQE_GATHER_OFFSET);
	input_seg->byte_count = rte_cpu_to_be_32(klm->byte_count);
	input_seg->addr = rte_cpu_to_be_64(klm->address);
	input_seg->lkey = klm->mkey;
	job->user_id = op->user_id;
}

static inline void
prep_one(struct mlx5_regex_priv *priv, struct mlx5_regex_qp *qp,
	 struct mlx5_regex_hw_qp *qp_obj, struct rte_regex_ops *op,
	 struct mlx5_regex_job *job)
{
	struct mlx5_klm klm;

	klm.byte_count = rte_pktmbuf_data_len(op->mbuf);
	klm.mkey = mlx5_mr_mb2mr(&qp->mr_ctrl, op->mbuf);
	klm.address = rte_pktmbuf_mtod(op->mbuf, uintptr_t);
	__prep_one(priv, qp_obj, op, job, qp_obj->pi, &klm);
	qp_obj->db_pi = qp_obj->pi;
	qp_obj->pi = (qp_obj->pi + 1) & MLX5_REGEX_MAX_WQE_INDEX;
}

static inline void
send_doorbell(struct mlx5_regex_priv *priv, struct mlx5_regex_hw_qp *qp)
{
	size_t wqe_offset = (qp->db_pi & (qp_size_get(qp) - 1)) *
			    (MLX5_SEND_WQE_BB << (priv->has_umr ? 2 : 0)) +
			    (priv->has_umr ? MLX5_REGEX_UMR_WQE_SIZE : 0);
	uint8_t *wqe = (uint8_t *)(uintptr_t)qp->qp_obj.wqes + wqe_offset;
	uint32_t actual_pi = (priv->has_umr ? (qp->db_pi * 4 + 3) : qp->db_pi) &
			     MLX5_REGEX_MAX_WQE_INDEX;

	/* Or the fm_ce_se instead of set, avoid the fence be cleared. */
	((struct mlx5_wqe_ctrl_seg *)wqe)->fm_ce_se |= MLX5_WQE_CTRL_CQ_UPDATE;
	mlx5_doorbell_ring(&priv->uar.bf_db, *(volatile uint64_t *)wqe,
			   actual_pi, &qp->qp_obj.db_rec[MLX5_SND_DBR],
			   !priv->uar.dbnc);
}

static inline int
get_free(struct mlx5_regex_hw_qp *qp, uint8_t has_umr) {
	return (qp_size_get(qp) - ((qp->pi - qp->ci) &
			(has_umr ? (MLX5_REGEX_MAX_WQE_INDEX >> 2) :
			MLX5_REGEX_MAX_WQE_INDEX)));
}

static inline uint32_t
job_id_get(uint32_t qid, size_t qp_size, size_t index) {
	return qid * qp_size + (index & (qp_size - 1));
}

#ifdef HAVE_MLX5_UMR_IMKEY
static inline int
mkey_klm_available(struct mlx5_klm *klm, uint32_t pos, uint32_t new)
{
	return (klm && ((pos + new) <= MLX5_REGEX_MAX_KLM_NUM));
}

static inline void
complete_umr_wqe(struct mlx5_regex_qp *qp, struct mlx5_regex_hw_qp *qp_obj,
		 struct mlx5_regex_job *mkey_job,
		 size_t umr_index, uint32_t klm_size, uint32_t total_len)
{
	size_t wqe_offset = (umr_index & (qp_size_get(qp_obj) - 1)) *
		(MLX5_SEND_WQE_BB * 4);
	struct mlx5_wqe_ctrl_seg *wqe = (struct mlx5_wqe_ctrl_seg *)((uint8_t *)
				   (uintptr_t)qp_obj->qp_obj.wqes + wqe_offset);
	struct mlx5_wqe_umr_ctrl_seg *ucseg =
				(struct mlx5_wqe_umr_ctrl_seg *)(wqe + 1);
	struct mlx5_wqe_mkey_context_seg *mkc =
				(struct mlx5_wqe_mkey_context_seg *)(ucseg + 1);
	struct mlx5_klm *iklm = (struct mlx5_klm *)(mkc + 1);
	uint16_t klm_align = RTE_ALIGN(klm_size, 4);

	memset(wqe, 0, MLX5_REGEX_UMR_WQE_SIZE);
	/* Set WQE control seg. Non-inline KLM UMR WQE size must be 9 WQE_DS. */
	set_wqe_ctrl_seg(wqe, (umr_index * 4), MLX5_OPCODE_UMR,
			 0, qp_obj->qp_obj.qp->id, 0, 9, 0,
			 rte_cpu_to_be_32(mkey_job->imkey->id));
	/* Set UMR WQE control seg. */
	ucseg->mkey_mask |= rte_cpu_to_be_64(MLX5_WQE_UMR_CTRL_MKEY_MASK_LEN |
				MLX5_WQE_UMR_CTRL_FLAG_TRNSLATION_OFFSET |
				MLX5_WQE_UMR_CTRL_MKEY_MASK_ACCESS_LOCAL_WRITE);
	ucseg->klm_octowords = rte_cpu_to_be_16(klm_align);
	/* Set mkey context seg. */
	mkc->len = rte_cpu_to_be_64(total_len);
	mkc->qpn_mkey = rte_cpu_to_be_32(0xffffff00 |
					(mkey_job->imkey->id & 0xff));
	/* Set UMR pointer to data seg. */
	iklm->address = rte_cpu_to_be_64
				((uintptr_t)((char *)mkey_job->imkey_array));
	iklm->mkey = rte_cpu_to_be_32(qp->imkey_addr->lkey);
	iklm->byte_count = rte_cpu_to_be_32(klm_align);
	/* Clear the padding memory. */
	memset((uint8_t *)&mkey_job->imkey_array[klm_size], 0,
	       sizeof(struct mlx5_klm) * (klm_align - klm_size));

	/* Add the following RegEx WQE with fence. */
	wqe = (struct mlx5_wqe_ctrl_seg *)
				(((uint8_t *)wqe) + MLX5_REGEX_UMR_WQE_SIZE);
	wqe->fm_ce_se |= MLX5_WQE_CTRL_INITIATOR_SMALL_FENCE;
}

static inline void
prep_nop_regex_wqe_set(struct mlx5_regex_priv *priv,
		struct mlx5_regex_hw_qp *qp, struct rte_regex_ops *op,
		struct mlx5_regex_job *job, size_t pi, struct mlx5_klm *klm)
{
	size_t wqe_offset = (pi & (qp_size_get(qp) - 1)) *
			    (MLX5_SEND_WQE_BB << 2);
	struct mlx5_wqe_ctrl_seg *wqe = (struct mlx5_wqe_ctrl_seg *)((uint8_t *)
				   (uintptr_t)qp->qp_obj.wqes + wqe_offset);

	/* Clear the WQE memory used as UMR WQE previously. */
	if ((rte_be_to_cpu_32(wqe->opmod_idx_opcode) & 0xff) != MLX5_OPCODE_NOP)
		memset(wqe, 0, MLX5_REGEX_UMR_WQE_SIZE);
	/* UMR WQE size is 9 DS, align nop WQE to 3 WQEBBS(12 DS). */
	set_wqe_ctrl_seg(wqe, pi * 4, MLX5_OPCODE_NOP, 0, qp->qp_obj.qp->id,
			 0, 12, 0, 0);
	__prep_one(priv, qp, op, job, pi, klm);
}

static inline void
prep_regex_umr_wqe_set(struct mlx5_regex_priv *priv, struct mlx5_regex_qp *qp,
		struct mlx5_regex_hw_qp *qp_obj, struct rte_regex_ops **op,
		size_t nb_ops)
{
	struct mlx5_regex_job *job = NULL;
	size_t hw_qpid = qp_obj->qpn, mkey_job_id = 0;
	size_t left_ops = nb_ops;
	uint32_t klm_num = 0;
	uint32_t len = 0;
	struct mlx5_klm *mkey_klm = NULL;
	struct mlx5_klm klm;
	uintptr_t addr;

	while (left_ops--)
		rte_prefetch0(op[left_ops]);
	left_ops = nb_ops;
	/*
	 * Build the WQE set by reverse. In case the burst may consume
	 * multiple mkeys, build the WQE set as normal will hard to
	 * address the last mkey index, since we will only know the last
	 * RegEx WQE's index when finishes building.
	 */
	while (left_ops--) {
		struct rte_mbuf *mbuf = op[left_ops]->mbuf;
		size_t pi = MLX5_REGEX_UMR_QP_PI_IDX(qp_obj->pi, left_ops);

		if (mbuf->nb_segs > 1) {
			size_t scatter_size = 0;

			if (!mkey_klm_available(mkey_klm, klm_num,
						mbuf->nb_segs)) {
				/*
				 * The mkey's KLM is full, create the UMR
				 * WQE in the next WQE set.
				 */
				if (mkey_klm)
					complete_umr_wqe(qp, qp_obj,
						&qp->jobs[mkey_job_id],
						MLX5_REGEX_UMR_QP_PI_IDX(pi, 1),
						klm_num, len);
				/*
				 * Get the indircet mkey and KLM array index
				 * from the last WQE set.
				 */
				mkey_job_id = job_id_get(hw_qpid,
						qp_size_get(qp_obj), pi);
				mkey_klm = qp->jobs[mkey_job_id].imkey_array;
				klm_num = 0;
				len = 0;
			}
			/* Build RegEx WQE's data segment KLM. */
			klm.address = len;
			klm.mkey = rte_cpu_to_be_32
					(qp->jobs[mkey_job_id].imkey->id);
			while (mbuf) {
				addr = rte_pktmbuf_mtod(mbuf, uintptr_t);
				/* Build indirect mkey seg's KLM. */
				mkey_klm->mkey = mlx5_mr_mb2mr(&qp->mr_ctrl,
							       mbuf);
				mkey_klm->address = rte_cpu_to_be_64(addr);
				mkey_klm->byte_count = rte_cpu_to_be_32
						(rte_pktmbuf_data_len(mbuf));
				/*
				 * Save the mbuf's total size for RegEx data
				 * segment.
				 */
				scatter_size += rte_pktmbuf_data_len(mbuf);
				mkey_klm++;
				klm_num++;
				mbuf = mbuf->next;
			}
			len += scatter_size;
			klm.byte_count = scatter_size;
		} else {
			/* The single mubf case. Build the KLM directly. */
			klm.mkey = mlx5_mr_mb2mr(&qp->mr_ctrl, mbuf);
			klm.address = rte_pktmbuf_mtod(mbuf, uintptr_t);
			klm.byte_count = rte_pktmbuf_data_len(mbuf);
		}
		job = &qp->jobs[job_id_get(hw_qpid, qp_size_get(qp_obj), pi)];
		/*
		 * Build the nop + RegEx WQE set by default. The fist nop WQE
		 * will be updated later as UMR WQE if scattered mubf exist.
		 */
		prep_nop_regex_wqe_set(priv, qp_obj, op[left_ops], job, pi,
					&klm);
	}
	/*
	 * Scattered mbuf have been added to the KLM array. Complete the build
	 * of UMR WQE, update the first nop WQE as UMR WQE.
	 */
	if (mkey_klm)
		complete_umr_wqe(qp, qp_obj, &qp->jobs[mkey_job_id], qp_obj->pi,
				 klm_num, len);
	qp_obj->db_pi = MLX5_REGEX_UMR_QP_PI_IDX(qp_obj->pi, nb_ops - 1);
	qp_obj->pi = MLX5_REGEX_UMR_QP_PI_IDX(qp_obj->pi, nb_ops);
}

uint16_t
mlx5_regexdev_enqueue_gga(struct rte_regexdev *dev, uint16_t qp_id,
			  struct rte_regex_ops **ops, uint16_t nb_ops)
{
	struct mlx5_regex_priv *priv = dev->data->dev_private;
	struct mlx5_regex_qp *queue = &priv->qps[qp_id];
	struct mlx5_regex_hw_qp *qp_obj;
	size_t hw_qpid, nb_left = nb_ops, nb_desc;

	while ((hw_qpid = ffs(queue->free_qps))) {
		hw_qpid--; /* ffs returns 1 for bit 0 */
		qp_obj = &queue->qps[hw_qpid];
		nb_desc = get_free(qp_obj, priv->has_umr);
		if (nb_desc) {
			/* The ops be handled can't exceed nb_ops. */
			if (nb_desc > nb_left)
				nb_desc = nb_left;
			else
				queue->free_qps &= ~(1 << hw_qpid);
			prep_regex_umr_wqe_set(priv, queue, qp_obj, ops,
				nb_desc);
			send_doorbell(priv, qp_obj);
			nb_left -= nb_desc;
		}
		if (!nb_left)
			break;
		ops += nb_desc;
	}
	nb_ops -= nb_left;
	queue->pi += nb_ops;
	return nb_ops;
}
#endif

uint16_t
mlx5_regexdev_enqueue(struct rte_regexdev *dev, uint16_t qp_id,
		      struct rte_regex_ops **ops, uint16_t nb_ops)
{
	struct mlx5_regex_priv *priv = dev->data->dev_private;
	struct mlx5_regex_qp *queue = &priv->qps[qp_id];
	struct mlx5_regex_hw_qp *qp_obj;
	size_t hw_qpid, job_id, i = 0;

	while ((hw_qpid = ffs(queue->free_qps))) {
		hw_qpid--; /* ffs returns 1 for bit 0 */
		qp_obj = &queue->qps[hw_qpid];
		while (get_free(qp_obj, priv->has_umr)) {
			job_id = job_id_get(hw_qpid, qp_size_get(qp_obj),
				qp_obj->pi);
			prep_one(priv, queue, qp_obj, ops[i],
				&queue->jobs[job_id]);
			i++;
			if (unlikely(i == nb_ops)) {
				send_doorbell(priv, qp_obj);
				goto out;
			}
		}
		queue->free_qps &= ~(1 << hw_qpid);
		send_doorbell(priv, qp_obj);
	}

out:
	queue->pi += i;
	return i;
}

#define MLX5_REGEX_RESP_SZ 8

static inline void
extract_result(struct rte_regex_ops *op, struct mlx5_regex_job *job)
{
	size_t j;
	size_t offset;
	uint16_t status;

	op->user_id = job->user_id;
	op->nb_matches = MLX5_GET_VOLATILE(regexp_metadata, job->metadata +
					   MLX5_REGEX_METADATA_OFF,
					   match_count);
	op->nb_actual_matches = MLX5_GET_VOLATILE(regexp_metadata,
						  job->metadata +
						  MLX5_REGEX_METADATA_OFF,
						  detected_match_count);
	for (j = 0; j < op->nb_matches; j++) {
		offset = MLX5_REGEX_RESP_SZ * j;
		op->matches[j].rule_id =
			MLX5_GET_VOLATILE(regexp_match_tuple,
					  (job->output + offset), rule_id);
		op->matches[j].start_offset =
			MLX5_GET_VOLATILE(regexp_match_tuple,
					  (job->output +  offset), start_ptr);
		op->matches[j].len =
			MLX5_GET_VOLATILE(regexp_match_tuple,
					  (job->output +  offset), length);
	}
	status = MLX5_GET_VOLATILE(regexp_metadata, job->metadata +
				   MLX5_REGEX_METADATA_OFF,
				   status);
	op->rsp_flags = 0;
	if (status & MLX5_RXP_RESP_STATUS_PMI_SOJ)
		op->rsp_flags |= RTE_REGEX_OPS_RSP_PMI_SOJ_F;
	if (status & MLX5_RXP_RESP_STATUS_PMI_EOJ)
		op->rsp_flags |= RTE_REGEX_OPS_RSP_PMI_EOJ_F;
	if (status & MLX5_RXP_RESP_STATUS_MAX_LATENCY)
		op->rsp_flags |= RTE_REGEX_OPS_RSP_MAX_SCAN_TIMEOUT_F;
	if (status & MLX5_RXP_RESP_STATUS_MAX_MATCH)
		op->rsp_flags |= RTE_REGEX_OPS_RSP_MAX_MATCH_F;
	if (status & MLX5_RXP_RESP_STATUS_MAX_PREFIX)
		op->rsp_flags |= RTE_REGEX_OPS_RSP_MAX_PREFIX_F;
	if (status & MLX5_RXP_RESP_STATUS_MAX_PRI_THREADS)
		op->rsp_flags |= RTE_REGEX_OPS_RSP_RESOURCE_LIMIT_REACHED_F;
	if (status & MLX5_RXP_RESP_STATUS_MAX_SEC_THREADS)
		op->rsp_flags |= RTE_REGEX_OPS_RSP_RESOURCE_LIMIT_REACHED_F;
}

static inline volatile struct mlx5_cqe *
poll_one(struct mlx5_regex_cq *cq)
{
	volatile struct mlx5_cqe *cqe;
	size_t next_cqe_offset;

	next_cqe_offset =  (cq->ci & (cq_size_get(cq) - 1));
	cqe = (volatile struct mlx5_cqe *)(cq->cq_obj.cqes + next_cqe_offset);
	rte_io_wmb();

	int ret = check_cqe(cqe, cq_size_get(cq), cq->ci);

	if (unlikely(ret == MLX5_CQE_STATUS_ERR)) {
		DRV_LOG(ERR, "Completion with error on qp 0x%x",  0);
		return NULL;
	}

	if (unlikely(ret != MLX5_CQE_STATUS_SW_OWN))
		return NULL;

	return cqe;
}


/**
 * DPDK callback for dequeue.
 *
 * @param dev
 *   Pointer to the regex dev structure.
 * @param qp_id
 *   The queue to enqueue the traffic to.
 * @param ops
 *   List of regex ops to dequeue.
 * @param nb_ops
 *   Number of ops in ops parameter.
 *
 * @return
 *   Number of packets successfully dequeued (<= pkts_n).
 */
uint16_t
mlx5_regexdev_dequeue(struct rte_regexdev *dev, uint16_t qp_id,
		      struct rte_regex_ops **ops, uint16_t nb_ops)
{
	struct mlx5_regex_priv *priv = dev->data->dev_private;
	struct mlx5_regex_qp *queue = &priv->qps[qp_id];
	struct mlx5_regex_cq *cq = &queue->cq;
	volatile struct mlx5_cqe *cqe;
	size_t i = 0;

	while ((cqe = poll_one(cq))) {
		uint16_t wq_counter
			= (rte_be_to_cpu_16(cqe->wqe_counter) + 1) &
			  MLX5_REGEX_MAX_WQE_INDEX;
		size_t hw_qpid = cqe->user_index_bytes[2];
		struct mlx5_regex_hw_qp *qp_obj = &queue->qps[hw_qpid];

		/* UMR mode WQE counter move as WQE set(4 WQEBBS).*/
		if (priv->has_umr)
			wq_counter >>= 2;
		while (qp_obj->ci != wq_counter) {
			if (unlikely(i == nb_ops)) {
				/* Return without updating cq->ci */
				goto out;
			}
			uint32_t job_id = job_id_get(hw_qpid,
					qp_size_get(qp_obj), qp_obj->ci);
			extract_result(ops[i], &queue->jobs[job_id]);
			qp_obj->ci = (qp_obj->ci + 1) & (priv->has_umr ?
				 (MLX5_REGEX_MAX_WQE_INDEX >> 2) :
				  MLX5_REGEX_MAX_WQE_INDEX);
			i++;
		}
		cq->ci = (cq->ci + 1) & 0xffffff;
		rte_wmb();
		cq->cq_obj.db_rec[0] = rte_cpu_to_be_32(cq->ci);
		queue->free_qps |= (1 << hw_qpid);
	}

out:
	queue->ci += i;
	return i;
}

static void
setup_qps(struct mlx5_regex_priv *priv, struct mlx5_regex_qp *queue)
{
	size_t hw_qpid, entry;
	uint32_t job_id;
	for (hw_qpid = 0; hw_qpid < queue->nb_obj; hw_qpid++) {
		struct mlx5_regex_hw_qp *qp_obj = &queue->qps[hw_qpid];
		uint8_t *wqe = (uint8_t *)(uintptr_t)qp_obj->qp_obj.wqes;
		for (entry = 0 ; entry < qp_size_get(qp_obj); entry++) {
			job_id = hw_qpid * qp_size_get(qp_obj) + entry;
			struct mlx5_regex_job *job = &queue->jobs[job_id];

			/* Fill UMR WQE with NOP in advanced. */
			if (priv->has_umr) {
				set_wqe_ctrl_seg
					((struct mlx5_wqe_ctrl_seg *)wqe,
					 entry * 2, MLX5_OPCODE_NOP, 0,
					 qp_obj->qp_obj.qp->id, 0, 12, 0, 0);
				wqe += MLX5_REGEX_UMR_WQE_SIZE;
			}
			set_metadata_seg((struct mlx5_wqe_metadata_seg *)
					 (wqe + MLX5_REGEX_WQE_METADATA_OFFSET),
					 0, queue->metadata->lkey,
					 (uintptr_t)job->metadata);
			set_data_seg((struct mlx5_wqe_data_seg *)
				     (wqe + MLX5_REGEX_WQE_SCATTER_OFFSET),
				     MLX5_REGEX_MAX_OUTPUT,
				     queue->outputs->lkey,
				     (uintptr_t)job->output);
			wqe += 64;
		}
		queue->free_qps |= 1 << hw_qpid;
	}
}

static int
setup_buffers(struct mlx5_regex_priv *priv, struct mlx5_regex_qp *qp)
{
	struct ibv_pd *pd = priv->cdev->pd;
	uint32_t i;
	int err;

	void *ptr = rte_calloc(__func__, qp->nb_desc,
			       MLX5_REGEX_METADATA_SIZE,
			       MLX5_REGEX_METADATA_SIZE);
	if (!ptr)
		return -ENOMEM;

	qp->metadata = mlx5_glue->reg_mr(pd, ptr,
					 MLX5_REGEX_METADATA_SIZE * qp->nb_desc,
					 IBV_ACCESS_LOCAL_WRITE);
	if (!qp->metadata) {
		DRV_LOG(ERR, "Failed to register metadata");
		rte_free(ptr);
		return -EINVAL;
	}

	ptr = rte_calloc(__func__, qp->nb_desc,
			 MLX5_REGEX_MAX_OUTPUT,
			 MLX5_REGEX_MAX_OUTPUT);
	if (!ptr) {
		err = -ENOMEM;
		goto err_output;
	}
	qp->outputs = mlx5_glue->reg_mr(pd, ptr,
					MLX5_REGEX_MAX_OUTPUT * qp->nb_desc,
					IBV_ACCESS_LOCAL_WRITE);
	if (!qp->outputs) {
		rte_free(ptr);
		DRV_LOG(ERR, "Failed to register output");
		err = -EINVAL;
		goto err_output;
	}

	if (priv->has_umr) {
		ptr = rte_calloc(__func__, qp->nb_desc, MLX5_REGEX_KLMS_SIZE,
				 MLX5_REGEX_KLMS_SIZE);
		if (!ptr) {
			err = -ENOMEM;
			goto err_imkey;
		}
		qp->imkey_addr = mlx5_glue->reg_mr(pd, ptr,
					MLX5_REGEX_KLMS_SIZE * qp->nb_desc,
					IBV_ACCESS_LOCAL_WRITE);
		if (!qp->imkey_addr) {
			rte_free(ptr);
			DRV_LOG(ERR, "Failed to register output");
			err = -EINVAL;
			goto err_imkey;
		}
	}

	/* distribute buffers to jobs */
	for (i = 0; i < qp->nb_desc; i++) {
		qp->jobs[i].output =
			(uint8_t *)qp->outputs->addr +
			(i % qp->nb_desc) * MLX5_REGEX_MAX_OUTPUT;
		qp->jobs[i].metadata =
			(uint8_t *)qp->metadata->addr +
			(i % qp->nb_desc) * MLX5_REGEX_METADATA_SIZE;
		if (qp->imkey_addr)
			qp->jobs[i].imkey_array = (struct mlx5_klm *)
				qp->imkey_addr->addr +
				(i % qp->nb_desc) * MLX5_REGEX_MAX_KLM_NUM;
	}

	return 0;

err_imkey:
	ptr = qp->outputs->addr;
	rte_free(ptr);
	mlx5_glue->dereg_mr(qp->outputs);
err_output:
	ptr = qp->metadata->addr;
	rte_free(ptr);
	mlx5_glue->dereg_mr(qp->metadata);
	return err;
}

int
mlx5_regexdev_setup_fastpath(struct mlx5_regex_priv *priv, uint32_t qp_id)
{
	struct mlx5_regex_qp *qp = &priv->qps[qp_id];
	struct mlx5_klm klm = { 0 };
	struct mlx5_devx_mkey_attr attr = {
		.klm_array = &klm,
		.klm_num = 1,
		.umr_en = 1,
	};
	uint32_t i;
	int err = 0;

	qp->jobs = rte_calloc(__func__, qp->nb_desc, sizeof(*qp->jobs), 64);
	if (!qp->jobs)
		return -ENOMEM;
	err = setup_buffers(priv, qp);
	if (err) {
		rte_free(qp->jobs);
		qp->jobs = NULL;
		return err;
	}

	setup_qps(priv, qp);

	if (priv->has_umr) {
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
		attr.pd = priv->cdev->pdn;
#endif
		for (i = 0; i < qp->nb_desc; i++) {
			attr.klm_num = MLX5_REGEX_MAX_KLM_NUM;
			attr.klm_array = qp->jobs[i].imkey_array;
			qp->jobs[i].imkey = mlx5_devx_cmd_mkey_create
						       (priv->cdev->ctx, &attr);
			if (!qp->jobs[i].imkey) {
				err = -rte_errno;
				DRV_LOG(ERR, "Failed to allocate imkey.");
				mlx5_regexdev_teardown_fastpath(priv, qp_id);
			}
		}
	}
	return err;
}

static void
free_buffers(struct mlx5_regex_qp *qp)
{
	if (qp->imkey_addr) {
		mlx5_glue->dereg_mr(qp->imkey_addr);
		rte_free(qp->imkey_addr->addr);
	}
	if (qp->metadata) {
		mlx5_glue->dereg_mr(qp->metadata);
		rte_free(qp->metadata->addr);
	}
	if (qp->outputs) {
		mlx5_glue->dereg_mr(qp->outputs);
		rte_free(qp->outputs->addr);
	}
}

void
mlx5_regexdev_teardown_fastpath(struct mlx5_regex_priv *priv, uint32_t qp_id)
{
	struct mlx5_regex_qp *qp = &priv->qps[qp_id];
	uint32_t i;

	if (qp->jobs) {
		for (i = 0; i < qp->nb_desc; i++) {
			if (qp->jobs[i].imkey)
				claim_zero(mlx5_devx_cmd_destroy
							(qp->jobs[i].imkey));
		}
		free_buffers(qp);
		rte_free(qp->jobs);
		qp->jobs = NULL;
	}
}
