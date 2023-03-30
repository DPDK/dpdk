/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef MLX5DR_SEND_H_
#define MLX5DR_SEND_H_

#define MLX5DR_NUM_SEND_RINGS 1

/* As a single operation requires at least two WQEBBS.
 * This means a maximum of 16 such operations per rule.
 */
#define MAX_WQES_PER_RULE 32

/* WQE Control segment. */
struct mlx5dr_wqe_ctrl_seg {
	__be32 opmod_idx_opcode;
	__be32 qpn_ds;
	__be32 flags;
	__be32 imm;
};

enum mlx5dr_wqe_opcode {
	MLX5DR_WQE_OPCODE_TBL_ACCESS = 0x2c,
};

enum mlx5dr_wqe_opmod {
	MLX5DR_WQE_OPMOD_GTA_STE = 0,
	MLX5DR_WQE_OPMOD_GTA_MOD_ARG = 1,
};

enum mlx5dr_wqe_gta_opcode {
	MLX5DR_WQE_GTA_OP_ACTIVATE = 0,
	MLX5DR_WQE_GTA_OP_DEACTIVATE = 1,
};

enum mlx5dr_wqe_gta_opmod {
	MLX5DR_WQE_GTA_OPMOD_STE = 0,
	MLX5DR_WQE_GTA_OPMOD_MOD_ARG = 1,
};

enum mlx5dr_wqe_gta_sz {
	MLX5DR_WQE_SZ_GTA_CTRL = 48,
	MLX5DR_WQE_SZ_GTA_DATA = 64,
};

struct mlx5dr_wqe_gta_ctrl_seg {
	__be32 op_dirix;
	__be32 stc_ix[5];
	__be32 rsvd0[6];
};

struct mlx5dr_wqe_gta_data_seg_ste {
	__be32 rsvd0_ctr_id;
	__be32 rsvd1_definer;
	__be32 rsvd2[3];
	union {
		struct {
			__be32 action[3];
			__be32 tag[8];
		};
		__be32 jumbo[11];
	};
};

struct mlx5dr_wqe_gta_data_seg_arg {
	__be32 action_args[8];
};

struct mlx5dr_wqe_gta {
	struct mlx5dr_wqe_gta_ctrl_seg gta_ctrl;
	union {
		struct mlx5dr_wqe_gta_data_seg_ste seg_ste;
		struct mlx5dr_wqe_gta_data_seg_arg seg_arg;
	};
};

struct mlx5dr_send_ring_cq {
	uint8_t *buf;
	uint32_t cons_index;
	uint32_t ncqe_mask;
	uint32_t buf_sz;
	uint32_t ncqe;
	uint32_t cqe_log_sz;
	__be32 *db;
	uint16_t poll_wqe;
	struct ibv_cq *ibv_cq;
	uint32_t cqn;
	uint32_t cqe_sz;
};

struct mlx5dr_send_ring_priv {
	struct mlx5dr_rule *rule;
	void *user_data;
	uint32_t num_wqebbs;
	uint32_t id;
	uint32_t retry_id;
	uint32_t *used_id;
};

struct mlx5dr_send_ring_dep_wqe {
	struct mlx5dr_wqe_gta_ctrl_seg wqe_ctrl;
	struct mlx5dr_wqe_gta_data_seg_ste wqe_data;
	struct mlx5dr_rule *rule;
	uint32_t rtc_0;
	uint32_t rtc_1;
	uint32_t retry_rtc_0;
	uint32_t retry_rtc_1;
	void *user_data;
};

struct mlx5dr_send_ring_sq {
	char *buf;
	uint32_t sqn;
	__be32 *db;
	void *reg_addr;
	uint16_t cur_post;
	uint16_t buf_mask;
	struct mlx5dr_send_ring_priv *wr_priv;
	unsigned int last_idx;
	struct mlx5dr_send_ring_dep_wqe *dep_wqe;
	unsigned int head_dep_idx;
	unsigned int tail_dep_idx;
	struct mlx5dr_devx_obj *obj;
	struct mlx5dv_devx_umem *buf_umem;
	struct mlx5dv_devx_umem *db_umem;
};

struct mlx5dr_send_ring {
	struct mlx5dr_send_ring_cq send_cq;
	struct mlx5dr_send_ring_sq send_sq;
};

struct mlx5dr_completed_poll_entry {
	void *user_data;
	enum rte_flow_op_status status;
};

struct mlx5dr_completed_poll {
	struct mlx5dr_completed_poll_entry *entries;
	uint16_t ci;
	uint16_t pi;
	uint16_t mask;
};

struct mlx5dr_send_engine {
	struct mlx5dr_send_ring send_ring[MLX5DR_NUM_SEND_RINGS]; /* For now 1:1 mapping */
	struct mlx5dv_devx_uar *uar; /* Uar is shared between rings of a queue */
	struct mlx5dr_completed_poll completed;
	uint16_t used_entries;
	uint16_t th_entries;
	uint16_t rings;
	uint16_t num_entries;
	bool err;
} __rte_cache_aligned;

struct mlx5dr_send_engine_post_ctrl {
	struct mlx5dr_send_engine *queue;
	struct mlx5dr_send_ring *send_ring;
	size_t num_wqebbs;
};

struct mlx5dr_send_engine_post_attr {
	uint8_t opcode;
	uint8_t opmod;
	uint8_t notify_hw;
	uint8_t fence;
	uint8_t match_definer_id;
	uint8_t range_definer_id;
	size_t len;
	struct mlx5dr_rule *rule;
	uint32_t id;
	uint32_t retry_id;
	uint32_t *used_id;
	void *user_data;
};

struct mlx5dr_send_ste_attr {
	/* rtc / retry_rtc / used_id_rtc override send_attr */
	uint32_t rtc_0;
	uint32_t rtc_1;
	uint32_t retry_rtc_0;
	uint32_t retry_rtc_1;
	uint32_t *used_id_rtc_0;
	uint32_t *used_id_rtc_1;
	bool wqe_tag_is_jumbo;
	uint8_t gta_opcode;
	uint32_t direct_index;
	struct mlx5dr_send_engine_post_attr send_attr;
	struct mlx5dr_rule_match_tag *wqe_tag;
	struct mlx5dr_rule_match_tag *range_wqe_tag;
	struct mlx5dr_wqe_gta_ctrl_seg *wqe_ctrl;
	struct mlx5dr_wqe_gta_data_seg_ste *wqe_data;
	struct mlx5dr_wqe_gta_data_seg_ste *range_wqe_data;
};

/**
 * Provide safe 64bit store operation to mlx5 UAR region for
 * both 32bit and 64bit architectures.
 *
 * @param val
 *   value to write in CPU endian format.
 * @param addr
 *   Address to write to.
 * @param lock
 *   Address of the lock to use for that UAR access.
 */
static __rte_always_inline void
mlx5dr_uar_write64_relaxed(uint64_t val, void *addr)
{
#ifdef RTE_ARCH_64
	*(uint64_t *)addr = val;
#else /* !RTE_ARCH_64 */
	*(uint32_t *)addr = val;
	rte_io_wmb();
	*((uint32_t *)addr + 1) = val >> 32;
#endif
}

struct mlx5dr_send_ring_dep_wqe *
mlx5dr_send_add_new_dep_wqe(struct mlx5dr_send_engine *queue);

void mlx5dr_send_abort_new_dep_wqe(struct mlx5dr_send_engine *queue);

void mlx5dr_send_all_dep_wqe(struct mlx5dr_send_engine *queue);

void mlx5dr_send_queue_close(struct mlx5dr_send_engine *queue);

int mlx5dr_send_queue_open(struct mlx5dr_context *ctx,
			   struct mlx5dr_send_engine *queue,
			   uint16_t queue_size);

void mlx5dr_send_queues_close(struct mlx5dr_context *ctx);

int mlx5dr_send_queues_open(struct mlx5dr_context *ctx,
			    uint16_t queues,
			    uint16_t queue_size);

struct mlx5dr_send_engine_post_ctrl
mlx5dr_send_engine_post_start(struct mlx5dr_send_engine *queue);

void mlx5dr_send_engine_post_req_wqe(struct mlx5dr_send_engine_post_ctrl *ctrl,
				     char **buf, size_t *len);

void mlx5dr_send_engine_post_end(struct mlx5dr_send_engine_post_ctrl *ctrl,
				 struct mlx5dr_send_engine_post_attr *attr);

void mlx5dr_send_ste(struct mlx5dr_send_engine *queue,
		     struct mlx5dr_send_ste_attr *ste_attr);

void mlx5dr_send_stes_fw(struct mlx5dr_send_engine *queue,
			 struct mlx5dr_send_ste_attr *ste_attr);

void mlx5dr_send_engine_flush_queue(struct mlx5dr_send_engine *queue);

static inline bool mlx5dr_send_engine_empty(struct mlx5dr_send_engine *queue)
{
	struct mlx5dr_send_ring_sq *send_sq = &queue->send_ring->send_sq;
	struct mlx5dr_send_ring_cq *send_cq = &queue->send_ring->send_cq;

	return ((send_sq->cur_post & send_sq->buf_mask) == send_cq->poll_wqe);
}

static inline bool mlx5dr_send_engine_full(struct mlx5dr_send_engine *queue)
{
	return queue->used_entries >= queue->th_entries;
}

static inline void mlx5dr_send_engine_inc_rule(struct mlx5dr_send_engine *queue)
{
	queue->used_entries++;
}

static inline void mlx5dr_send_engine_dec_rule(struct mlx5dr_send_engine *queue)
{
	queue->used_entries--;
}

static inline void mlx5dr_send_engine_gen_comp(struct mlx5dr_send_engine *queue,
					       void *user_data,
					       int comp_status)
{
	struct mlx5dr_completed_poll *comp = &queue->completed;

	comp->entries[comp->pi].status = comp_status;
	comp->entries[comp->pi].user_data = user_data;

	comp->pi = (comp->pi + 1) & comp->mask;
}

static inline bool mlx5dr_send_engine_err(struct mlx5dr_send_engine *queue)
{
	return queue->err;
}

#endif /* MLX5DR_SEND_H_ */
