/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_VDPA_H_
#define RTE_PMD_MLX5_VDPA_H_

#include <linux/virtio_net.h>
#include <sys/queue.h>

#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <rte_vdpa.h>
#include <rte_vhost.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif
#include <rte_spinlock.h>
#include <rte_interrupts.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_prm.h>


#define MLX5_VDPA_INTR_RETRIES 256
#define MLX5_VDPA_INTR_RETRIES_USEC 1000

#ifndef VIRTIO_F_ORDER_PLATFORM
#define VIRTIO_F_ORDER_PLATFORM 36
#endif

#ifndef VIRTIO_F_RING_PACKED
#define VIRTIO_F_RING_PACKED 34
#endif

struct mlx5_vdpa_cq {
	uint16_t log_desc_n;
	uint32_t cq_ci:24;
	uint32_t arm_sn:2;
	rte_spinlock_t sl;
	struct mlx5_devx_obj *cq;
	struct mlx5dv_devx_umem *umem_obj;
	union {
		volatile void *umem_buf;
		volatile struct mlx5_cqe *cqes;
	};
	volatile uint32_t *db_rec;
	uint64_t errors;
};

struct mlx5_vdpa_event_qp {
	struct mlx5_vdpa_cq cq;
	struct mlx5_devx_obj *fw_qp;
	struct mlx5_devx_obj *sw_qp;
	struct mlx5dv_devx_umem *umem_obj;
	void *umem_buf;
	volatile uint32_t *db_rec;
};

struct mlx5_vdpa_query_mr {
	SLIST_ENTRY(mlx5_vdpa_query_mr) next;
	void *addr;
	uint64_t length;
	struct mlx5dv_devx_umem *umem;
	struct mlx5_devx_obj *mkey;
	int is_indirect;
};

struct mlx5_vdpa_virtq {
	SLIST_ENTRY(mlx5_vdpa_virtq) next;
	uint8_t enable;
	uint16_t index;
	uint16_t vq_size;
	struct mlx5_vdpa_priv *priv;
	struct mlx5_devx_obj *virtq;
	struct mlx5_vdpa_event_qp eqp;
	struct {
		struct mlx5dv_devx_umem *obj;
		void *buf;
		uint32_t size;
	} umems[3];
	struct rte_intr_handle intr_handle;
};

struct mlx5_vdpa_steer {
	struct mlx5_devx_obj *rqt;
	void *domain;
	void *tbl;
	struct {
		struct mlx5dv_flow_matcher *matcher;
		struct mlx5_devx_obj *tir;
		void *tir_action;
		void *flow;
	} rss[7];
};

struct mlx5_vdpa_priv {
	TAILQ_ENTRY(mlx5_vdpa_priv) next;
	uint8_t configured;
	int id; /* vDPA device id. */
	int vid; /* vhost device id. */
	struct ibv_context *ctx; /* Device context. */
	struct rte_vdpa_dev_addr dev_addr;
	struct mlx5_hca_vdpa_attr caps;
	uint32_t pdn; /* Protection Domain number. */
	struct ibv_pd *pd;
	uint32_t gpa_mkey_index;
	struct ibv_mr *null_mr;
	struct rte_vhost_memory *vmem;
	uint32_t eqn;
	struct mlx5dv_devx_event_channel *eventc;
	struct mlx5dv_devx_uar *uar;
	struct rte_intr_handle intr_handle;
	struct mlx5_devx_obj *td;
	struct mlx5_devx_obj *tis;
	uint16_t nr_virtqs;
	uint64_t features; /* Negotiated features. */
	uint16_t log_max_rqt_size;
	SLIST_HEAD(virtq_list, mlx5_vdpa_virtq) virtq_list;
	struct mlx5_vdpa_steer steer;
	struct mlx5dv_var *var;
	void *virtq_db_addr;
	SLIST_HEAD(mr_list, mlx5_vdpa_query_mr) mr_list;
};

/**
 * Release all the prepared memory regions and all their related resources.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 */
void mlx5_vdpa_mem_dereg(struct mlx5_vdpa_priv *priv);

/**
 * Register all the memory regions of the virtio device to the HW and allocate
 * all their related resources.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int mlx5_vdpa_mem_register(struct mlx5_vdpa_priv *priv);


/**
 * Create an event QP and all its related resources.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 * @param[in] desc_n
 *   Number of descriptors.
 * @param[in] callfd
 *   The guest notification file descriptor.
 * @param[in/out] eqp
 *   Pointer to the event QP structure.
 *
 * @return
 *   0 on success, -1 otherwise and rte_errno is set.
 */
int mlx5_vdpa_event_qp_create(struct mlx5_vdpa_priv *priv, uint16_t desc_n,
			      int callfd, struct mlx5_vdpa_event_qp *eqp);

/**
 * Destroy an event QP and all its related resources.
 *
 * @param[in/out] eqp
 *   Pointer to the event QP structure.
 */
void mlx5_vdpa_event_qp_destroy(struct mlx5_vdpa_event_qp *eqp);

/**
 * Release all the event global resources.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 */
void mlx5_vdpa_event_qp_global_release(struct mlx5_vdpa_priv *priv);

/**
 * Setup CQE event.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int mlx5_vdpa_cqe_event_setup(struct mlx5_vdpa_priv *priv);

/**
 * Unset CQE event .
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 */
void mlx5_vdpa_cqe_event_unset(struct mlx5_vdpa_priv *priv);

/**
 * Release a virtq and all its related resources.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 */
void mlx5_vdpa_virtqs_release(struct mlx5_vdpa_priv *priv);

/**
 * Create all the HW virtqs resources and all their related resources.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int mlx5_vdpa_virtqs_prepare(struct mlx5_vdpa_priv *priv);

/**
 * Enable\Disable virtq..
 *
 * @param[in] virtq
 *   The vdpa driver private virtq structure.
 * @param[in] enable
 *   Set to enable, otherwise disable.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int mlx5_vdpa_virtq_enable(struct mlx5_vdpa_virtq *virtq, int enable);

/**
 * Unset steering and release all its related resources- stop traffic.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 */
int mlx5_vdpa_steer_unset(struct mlx5_vdpa_priv *priv);

/**
 * Setup steering and all its related resources to enable RSS traffic from the
 * device to all the Rx host queues.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int mlx5_vdpa_steer_setup(struct mlx5_vdpa_priv *priv);

/**
 * Enable\Disable live migration logging.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 * @param[in] enable
 *   Set for enable, unset for disable.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int mlx5_vdpa_logging_enable(struct mlx5_vdpa_priv *priv, int enable);

/**
 * Set dirty bitmap logging to allow live migration.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 * @param[in] log_base
 *   Vhost log base.
 * @param[in] log_size
 *   Vhost log size.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int mlx5_vdpa_dirty_bitmap_set(struct mlx5_vdpa_priv *priv, uint64_t log_base,
			       uint64_t log_size);

/**
 * Log all virtqs information for live migration.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 * @param[in] enable
 *   Set for enable, unset for disable.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int mlx5_vdpa_lm_log(struct mlx5_vdpa_priv *priv);

/**
 * Modify virtq state to be ready or suspend.
 *
 * @param[in] virtq
 *   The vdpa driver private virtq structure.
 * @param[in] state
 *   Set for ready, otherwise suspend.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int mlx5_vdpa_virtq_modify(struct mlx5_vdpa_virtq *virtq, int state);

#endif /* RTE_PMD_MLX5_VDPA_H_ */
