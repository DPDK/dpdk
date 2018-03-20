/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 6WIND S.A.
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#ifndef MLX4_GLUE_H_
#define MLX4_GLUE_H_

#include <stddef.h>
#include <stdint.h>

/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/mlx4dv.h>
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#ifndef MLX4_GLUE_VERSION
#define MLX4_GLUE_VERSION ""
#endif

/* LIB_GLUE_VERSION must be updated every time this structure is modified. */
struct mlx4_glue {
	const char *version;
	int (*fork_init)(void);
	int (*get_async_event)(struct ibv_context *context,
			       struct ibv_async_event *event);
	void (*ack_async_event)(struct ibv_async_event *event);
	struct ibv_pd *(*alloc_pd)(struct ibv_context *context);
	int (*dealloc_pd)(struct ibv_pd *pd);
	struct ibv_device **(*get_device_list)(int *num_devices);
	void (*free_device_list)(struct ibv_device **list);
	struct ibv_context *(*open_device)(struct ibv_device *device);
	int (*close_device)(struct ibv_context *context);
	const char *(*get_device_name)(struct ibv_device *device);
	int (*query_device)(struct ibv_context *context,
			    struct ibv_device_attr *device_attr);
	int (*query_device_ex)(struct ibv_context *context,
			       const struct ibv_query_device_ex_input *input,
			       struct ibv_device_attr_ex *attr);
	int (*query_port)(struct ibv_context *context, uint8_t port_num,
			  struct ibv_port_attr *port_attr);
	const char *(*port_state_str)(enum ibv_port_state port_state);
	struct ibv_comp_channel *(*create_comp_channel)
		(struct ibv_context *context);
	int (*destroy_comp_channel)(struct ibv_comp_channel *channel);
	struct ibv_cq *(*create_cq)(struct ibv_context *context, int cqe,
				    void *cq_context,
				    struct ibv_comp_channel *channel,
				    int comp_vector);
	int (*destroy_cq)(struct ibv_cq *cq);
	int (*get_cq_event)(struct ibv_comp_channel *channel,
			    struct ibv_cq **cq, void **cq_context);
	void (*ack_cq_events)(struct ibv_cq *cq, unsigned int nevents);
	struct ibv_flow *(*create_flow)(struct ibv_qp *qp,
					struct ibv_flow_attr *flow);
	int (*destroy_flow)(struct ibv_flow *flow_id);
	struct ibv_qp *(*create_qp)(struct ibv_pd *pd,
				    struct ibv_qp_init_attr *qp_init_attr);
	struct ibv_qp *(*create_qp_ex)
		(struct ibv_context *context,
		 struct ibv_qp_init_attr_ex *qp_init_attr_ex);
	int (*destroy_qp)(struct ibv_qp *qp);
	int (*modify_qp)(struct ibv_qp *qp, struct ibv_qp_attr *attr,
			 int attr_mask);
	struct ibv_mr *(*reg_mr)(struct ibv_pd *pd, void *addr,
				 size_t length, int access);
	int (*dereg_mr)(struct ibv_mr *mr);
	struct ibv_rwq_ind_table *(*create_rwq_ind_table)
		(struct ibv_context *context,
		 struct ibv_rwq_ind_table_init_attr *init_attr);
	int (*destroy_rwq_ind_table)(struct ibv_rwq_ind_table *rwq_ind_table);
	struct ibv_wq *(*create_wq)(struct ibv_context *context,
				    struct ibv_wq_init_attr *wq_init_attr);
	int (*destroy_wq)(struct ibv_wq *wq);
	int (*modify_wq)(struct ibv_wq *wq, struct ibv_wq_attr *wq_attr);
	int (*dv_init_obj)(struct mlx4dv_obj *obj, uint64_t obj_type);
	int (*dv_set_context_attr)(struct ibv_context *context,
				   enum mlx4dv_set_ctx_attr_type attr_type,
				   void *attr);
};

const struct mlx4_glue *mlx4_glue;

#endif /* MLX4_GLUE_H_ */
