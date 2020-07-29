/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <unistd.h>
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
#include <strings.h>

#include "mlx5_regex_utils.h"
#include "mlx5_rxp.h"
#include "mlx5_regex.h"

#define MLX5_REGEX_MAX_WQE_INDEX 0xffff
#define MLX5_REGEX_METADATA_SIZE 64
#define MLX5_REGEX_MAX_INPUT (1 << 14)
#define MLX5_REGEX_MAX_OUTPUT (1 << 11)
#define MLX5_REGEX_WQE_CTRL_OFFSET 12
#define MLX5_REGEX_WQE_METADATA_OFFSET 16
#define MLX5_REGEX_WQE_GATHER_OFFSET 32
#define MLX5_REGEX_WQE_SCATTER_OFFSET 48
#define MLX5_REGEX_METADATA_OFF 32

static inline uint32_t
sq_size_get(struct mlx5_regex_sq *sq)
{
	return (1U << sq->log_nb_desc);
}

static inline uint32_t
cq_size_get(struct mlx5_regex_cq *cq)
{
	return (1U << cq->log_nb_desc);
}

struct mlx5_regex_job {
	uint64_t user_id;
	uint8_t *input;
	volatile uint8_t *output;
	volatile uint8_t *metadata;
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
prep_one(struct mlx5_regex_sq *sq, struct rte_regex_ops *op,
	 struct mlx5_regex_job *job)
{
	size_t wqe_offset = (sq->pi & (sq_size_get(sq) - 1)) * MLX5_SEND_WQE_BB;
	uint8_t *wqe = (uint8_t *)sq->wqe + wqe_offset;
	int ds = 4; /*  ctrl + meta + input + output */

	memcpy(job->input,
		rte_pktmbuf_mtod(op->mbuf, void *),
		rte_pktmbuf_data_len(op->mbuf));
	set_wqe_ctrl_seg((struct mlx5_wqe_ctrl_seg *)wqe, sq->pi,
			 MLX5_OPCODE_MMO, MLX5_OPC_MOD_MMO_REGEX, sq->obj->id,
			 0, ds, 0, 0);
	set_regex_ctrl_seg(wqe + 12, 0, op->group_id0, op->group_id1,
			   op->group_id2,
			   op->group_id3, 0);
	struct mlx5_wqe_data_seg *input_seg =
		(struct mlx5_wqe_data_seg *)(wqe +
					     MLX5_REGEX_WQE_GATHER_OFFSET);
	input_seg->byte_count =
		rte_cpu_to_be_32(rte_pktmbuf_data_len(op->mbuf));
	job->user_id = op->user_id;
	sq->db_pi = sq->pi;
	sq->pi = (sq->pi + 1) & MLX5_REGEX_MAX_WQE_INDEX;
}

static inline void
send_doorbell(struct mlx5dv_devx_uar *uar, struct mlx5_regex_sq *sq)
{
	size_t wqe_offset = (sq->db_pi & (sq_size_get(sq) - 1)) *
		MLX5_SEND_WQE_BB;
	uint8_t *wqe = (uint8_t *)sq->wqe + wqe_offset;
	((struct mlx5_wqe_ctrl_seg *)wqe)->fm_ce_se = MLX5_WQE_CTRL_CQ_UPDATE;
	uint64_t *doorbell_addr =
		(uint64_t *)((uint8_t *)uar->base_addr + 0x800);
	rte_cio_wmb();
	sq->dbr[MLX5_SND_DBR] = rte_cpu_to_be_32((sq->db_pi + 1) &
						 MLX5_REGEX_MAX_WQE_INDEX);
	rte_wmb();
	*doorbell_addr = *(volatile uint64_t *)wqe;
	rte_wmb();
}

static inline int
can_send(struct mlx5_regex_sq *sq) {
	return ((uint16_t)(sq->pi - sq->ci) < sq_size_get(sq));
}

static inline uint32_t
job_id_get(uint32_t qid, size_t sq_size, size_t index) {
	return qid * sq_size + (index & (sq_size - 1));
}

uint16_t
mlx5_regexdev_enqueue(struct rte_regexdev *dev, uint16_t qp_id,
		      struct rte_regex_ops **ops, uint16_t nb_ops)
{
	struct mlx5_regex_priv *priv = dev->data->dev_private;
	struct mlx5_regex_qp *queue = &priv->qps[qp_id];
	struct mlx5_regex_sq *sq;
	size_t sqid, job_id, i = 0;

	while ((sqid = ffs(queue->free_sqs))) {
		sqid--; /* ffs returns 1 for bit 0 */
		sq = &queue->sqs[sqid];
		while (can_send(sq)) {
			job_id = job_id_get(sqid, sq_size_get(sq), sq->pi);
			prep_one(sq, ops[i], &queue->jobs[job_id]);
			i++;
			if (unlikely(i == nb_ops)) {
				send_doorbell(priv->uar, sq);
				goto out;
			}
		}
		queue->free_sqs &= ~(1 << sqid);
		send_doorbell(priv->uar, sq);
	}

out:
	queue->pi += i;
	return i;
}

#define MLX5_REGEX_RESP_SZ 8

static inline void
extract_result(struct rte_regex_ops *op, struct mlx5_regex_job *job)
{
	size_t j, offset;
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
}

static inline volatile struct mlx5_cqe *
poll_one(struct mlx5_regex_cq *cq)
{
	volatile struct mlx5_cqe *cqe;
	size_t next_cqe_offset;

	next_cqe_offset =  (cq->ci & (cq_size_get(cq) - 1));
	cqe = (volatile struct mlx5_cqe *)(cq->cqe + next_cqe_offset);
	rte_cio_wmb();

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
		size_t sqid = cqe->rsvd3[2];
		struct mlx5_regex_sq *sq = &queue->sqs[sqid];
		while (sq->ci != wq_counter) {
			if (unlikely(i == nb_ops)) {
				/* Return without updating cq->ci */
				goto out;
			}
			uint32_t job_id = job_id_get(sqid, sq_size_get(sq),
						     sq->ci);
			extract_result(ops[i], &queue->jobs[job_id]);
			sq->ci = (sq->ci + 1) & MLX5_REGEX_MAX_WQE_INDEX;
			i++;
		}
		cq->ci = (cq->ci + 1) & 0xffffff;
		rte_wmb();
		cq->dbr[0] = rte_cpu_to_be_32(cq->ci);
		queue->free_sqs |= (1 << sqid);
	}

out:
	queue->ci += i;
	return i;
}

static void
setup_sqs(struct mlx5_regex_qp *queue)
{
	size_t sqid, entry;
	uint32_t job_id;
	for (sqid = 0; sqid < queue->nb_obj; sqid++) {
		struct mlx5_regex_sq *sq = &queue->sqs[sqid];
		uint8_t *wqe = (uint8_t *)sq->wqe;
		for (entry = 0 ; entry < sq_size_get(sq); entry++) {
			job_id = sqid * sq_size_get(sq) + entry;
			struct mlx5_regex_job *job = &queue->jobs[job_id];

			set_metadata_seg((struct mlx5_wqe_metadata_seg *)
					 (wqe + MLX5_REGEX_WQE_METADATA_OFFSET),
					 0, queue->metadata->lkey,
					 (uintptr_t)job->metadata);
			set_data_seg((struct mlx5_wqe_data_seg *)
				     (wqe + MLX5_REGEX_WQE_GATHER_OFFSET),
				     0, queue->inputs->lkey,
				     (uintptr_t)job->input);
			set_data_seg((struct mlx5_wqe_data_seg *)
				     (wqe + MLX5_REGEX_WQE_SCATTER_OFFSET),
				     MLX5_REGEX_MAX_OUTPUT,
				     queue->outputs->lkey,
				     (uintptr_t)job->output);
			wqe += 64;
		}
		queue->free_sqs |= 1 << sqid;
	}
}

static int
setup_buffers(struct mlx5_regex_qp *qp, struct ibv_pd *pd)
{
	uint32_t i;
	int err;

	void *ptr = rte_calloc(__func__, qp->nb_desc,
			       MLX5_REGEX_METADATA_SIZE,
			       MLX5_REGEX_METADATA_SIZE);
	if (!ptr)
		return -ENOMEM;

	qp->metadata = mlx5_glue->reg_mr(pd, ptr,
					 MLX5_REGEX_METADATA_SIZE*qp->nb_desc,
					 IBV_ACCESS_LOCAL_WRITE);
	if (!qp->metadata) {
		rte_free(ptr);
		return -EINVAL;
	}
	ptr = rte_calloc(__func__, qp->nb_desc,
			 MLX5_REGEX_MAX_INPUT,
			 MLX5_REGEX_MAX_INPUT);

	if (!ptr) {
		err = -ENOMEM;
		goto err_input;
	}
	qp->inputs = mlx5_glue->reg_mr(pd, ptr,
				       MLX5_REGEX_MAX_INPUT*qp->nb_desc,
				       IBV_ACCESS_LOCAL_WRITE);
	if (!qp->inputs) {
		rte_free(ptr);
		err = -EINVAL;
		goto err_input;
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
		err = -EINVAL;
		goto err_output;
	}

	/* distribute buffers to jobs */
	for (i = 0; i < qp->nb_desc; i++) {
		qp->jobs[i].input =
			(uint8_t *)qp->inputs->addr +
			(i % qp->nb_desc) * MLX5_REGEX_MAX_INPUT;
		qp->jobs[i].output =
			(uint8_t *)qp->outputs->addr +
			(i % qp->nb_desc) * MLX5_REGEX_MAX_OUTPUT;
		qp->jobs[i].metadata =
			(uint8_t *)qp->metadata->addr +
			(i % qp->nb_desc) * MLX5_REGEX_METADATA_SIZE;
	}
	return 0;

err_output:
	ptr = qp->inputs->addr;
	rte_free(ptr);
	mlx5_glue->dereg_mr(qp->inputs);
err_input:
	ptr = qp->metadata->addr;
	rte_free(ptr);
	mlx5_glue->dereg_mr(qp->metadata);
	return err;
}

int
mlx5_regexdev_setup_fastpath(struct mlx5_regex_priv *priv, uint32_t qp_id)
{
	struct mlx5_regex_qp *qp = &priv->qps[qp_id];
	int err;

	qp->jobs = rte_calloc(__func__, qp->nb_desc, sizeof(*qp->jobs),
			      sizeof(*qp->jobs));
	if (!qp->jobs)
		return -ENOMEM;
	err = setup_buffers(qp, priv->pd);
	if (err)
		return err;
	setup_sqs(qp);
	return 0;
}
