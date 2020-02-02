/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_VDPA_H_
#define RTE_PMD_MLX5_VDPA_H_

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

struct mlx5_vdpa_priv {
	TAILQ_ENTRY(mlx5_vdpa_priv) next;
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

#endif /* RTE_PMD_MLX5_VDPA_H_ */
