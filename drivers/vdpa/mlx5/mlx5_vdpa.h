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
#include <rte_vdpa_dev.h>
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

#define MLX5_VDPA_DEFAULT_TIMER_DELAY_US 100u
#define MLX5_VDPA_DEFAULT_TIMER_STEP_US 1u

struct mlx5_vdpa_cq {
	uint16_t log_desc_n;
	uint32_t cq_ci:24;
	uint32_t arm_sn:2;
	uint32_t armed:1;
	int callfd;
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

enum {
	MLX5_VDPA_NOTIFIER_STATE_DISABLED,
	MLX5_VDPA_NOTIFIER_STATE_ENABLED,
	MLX5_VDPA_NOTIFIER_STATE_ERR
};

struct mlx5_vdpa_virtq {
	SLIST_ENTRY(mlx5_vdpa_virtq) next;
	uint8_t enable;
	uint16_t index;
	uint16_t vq_size;
	uint8_t notifier_state;
	bool stopped;
	uint32_t version;
	struct mlx5_vdpa_priv *priv;
	struct mlx5_devx_obj *virtq;
	struct mlx5_devx_obj *counters;
	struct mlx5_vdpa_event_qp eqp;
	struct {
		struct mlx5dv_devx_umem *obj;
		void *buf;
		uint32_t size;
	} umems[3];
	struct rte_intr_handle intr_handle;
	uint64_t err_time[3]; /* RDTSC time of recent errors. */
	uint32_t n_retry;
	struct mlx5_devx_virtio_q_couners_attr reset;
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

enum {
	MLX5_VDPA_EVENT_MODE_DYNAMIC_TIMER,
	MLX5_VDPA_EVENT_MODE_FIXED_TIMER,
	MLX5_VDPA_EVENT_MODE_ONLY_INTERRUPT
};

struct mlx5_vdpa_priv {
	TAILQ_ENTRY(mlx5_vdpa_priv) next;
	uint8_t configured;
	pthread_mutex_t vq_config_lock;
	uint64_t last_traffic_tic;
	pthread_t timer_tid;
	pthread_mutex_t timer_lock;
	pthread_cond_t timer_cond;
	volatile uint8_t timer_on;
	int event_mode;
	uint32_t event_us;
	uint32_t timer_delay_us;
	uint32_t no_traffic_time_s;
	struct rte_vdpa_device *vdev; /* vDPA device. */
	int vid; /* vhost device id. */
	struct ibv_context *ctx; /* Device context. */
	struct rte_pci_device *pci_dev;
	struct mlx5_hca_vdpa_attr caps;
	uint32_t pdn; /* Protection Domain number. */
	struct ibv_pd *pd;
	uint32_t gpa_mkey_index;
	struct ibv_mr *null_mr;
	struct rte_vhost_memory *vmem;
	uint32_t eqn;
	struct mlx5dv_devx_event_channel *eventc;
	struct mlx5dv_devx_event_channel *err_chnl;
	struct mlx5dv_devx_uar *uar;
	struct rte_intr_handle intr_handle;
	struct rte_intr_handle err_intr_handle;
	struct mlx5_devx_obj *td;
	struct mlx5_devx_obj *tiss[16]; /* TIS list for each LAG port. */
	uint16_t nr_virtqs;
	uint8_t num_lag_ports;
	uint8_t qp_ts_format;
	uint64_t features; /* Negotiated features. */
	uint16_t log_max_rqt_size;
	struct mlx5_vdpa_steer steer;
	struct mlx5dv_var *var;
	void *virtq_db_addr;
	SLIST_HEAD(mr_list, mlx5_vdpa_query_mr) mr_list;
	struct mlx5_vdpa_virtq virtqs[];
};

enum {
	MLX5_VDPA_STATS_RECEIVED_DESCRIPTORS,
	MLX5_VDPA_STATS_COMPLETED_DESCRIPTORS,
	MLX5_VDPA_STATS_BAD_DESCRIPTOR_ERRORS,
	MLX5_VDPA_STATS_EXCEED_MAX_CHAIN,
	MLX5_VDPA_STATS_INVALID_BUFFER,
	MLX5_VDPA_STATS_COMPLETION_ERRORS,
	MLX5_VDPA_STATS_MAX
};

/*
 * Check whether virtq is for traffic receive.
 * According to VIRTIO_NET Spec the virtqueues index identity its type by:
 * 0 receiveq1
 * 1 transmitq1
 * ...
 * 2(N-1) receiveqN
 * 2(N-1)+1 transmitqN
 * 2N controlq
 */
static inline uint8_t
is_virtq_recvq(int virtq_index, int nr_vring)
{
	if (virtq_index % 2 == 0 && virtq_index != nr_vring - 1)
		return 1;
	return 0;
}

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
 * Setup error interrupt handler.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int mlx5_vdpa_err_event_setup(struct mlx5_vdpa_priv *priv);

/**
 * Unset error event handler.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 */
void mlx5_vdpa_err_event_unset(struct mlx5_vdpa_priv *priv);

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
 * @param[in] priv
 *   The vdpa driver private structure.
 * @param[in] index
 *   The virtq index.
 * @param[in] enable
 *   Set to enable, otherwise disable.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int mlx5_vdpa_virtq_enable(struct mlx5_vdpa_priv *priv, int index, int enable);

/**
 * Unset steering and release all its related resources- stop traffic.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 */
void mlx5_vdpa_steer_unset(struct mlx5_vdpa_priv *priv);

/**
 * Update steering according to the received queues status.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int mlx5_vdpa_steer_update(struct mlx5_vdpa_priv *priv);

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

/**
 * Stop virtq before destroying it.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 * @param[in] index
 *   The virtq index.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int mlx5_vdpa_virtq_stop(struct mlx5_vdpa_priv *priv, int index);

/**
 * Query virtq information.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 * @param[in] index
 *   The virtq index.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int mlx5_vdpa_virtq_query(struct mlx5_vdpa_priv *priv, int index);

/**
 * Get virtq statistics.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 * @param[in] qid
 *   The virtq index.
 * @param stats
 *   The virtq statistics array to fill.
 * @param n
 *   The number of elements in @p stats array.
 *
 * @return
 *   A negative value on error, otherwise the number of entries filled in the
 *   @p stats array.
 */
int
mlx5_vdpa_virtq_stats_get(struct mlx5_vdpa_priv *priv, int qid,
			  struct rte_vdpa_stat *stats, unsigned int n);

/**
 * Reset virtq statistics.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 * @param[in] qid
 *   The virtq index.
 *
 * @return
 *   A negative value on error, otherwise 0.
 */
int
mlx5_vdpa_virtq_stats_reset(struct mlx5_vdpa_priv *priv, int qid);
#endif /* RTE_PMD_MLX5_VDPA_H_ */
