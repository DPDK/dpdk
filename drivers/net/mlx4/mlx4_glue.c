/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 6WIND S.A.
 * Copyright 2018 Mellanox Technologies, Ltd
 */

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

#include "mlx4_glue.h"

static int
mlx4_glue_fork_init(void)
{
	return ibv_fork_init();
}

static int
mlx4_glue_get_async_event(struct ibv_context *context,
			  struct ibv_async_event *event)
{
	return ibv_get_async_event(context, event);
}

static void
mlx4_glue_ack_async_event(struct ibv_async_event *event)
{
	ibv_ack_async_event(event);
}

static struct ibv_pd *
mlx4_glue_alloc_pd(struct ibv_context *context)
{
	return ibv_alloc_pd(context);
}

static int
mlx4_glue_dealloc_pd(struct ibv_pd *pd)
{
	return ibv_dealloc_pd(pd);
}

static struct ibv_device **
mlx4_glue_get_device_list(int *num_devices)
{
	return ibv_get_device_list(num_devices);
}

static void
mlx4_glue_free_device_list(struct ibv_device **list)
{
	ibv_free_device_list(list);
}

static struct ibv_context *
mlx4_glue_open_device(struct ibv_device *device)
{
	return ibv_open_device(device);
}

static int
mlx4_glue_close_device(struct ibv_context *context)
{
	return ibv_close_device(context);
}

static const char *
mlx4_glue_get_device_name(struct ibv_device *device)
{
	return ibv_get_device_name(device);
}

static int
mlx4_glue_query_device(struct ibv_context *context,
		       struct ibv_device_attr *device_attr)
{
	return ibv_query_device(context, device_attr);
}

static int
mlx4_glue_query_device_ex(struct ibv_context *context,
			  const struct ibv_query_device_ex_input *input,
			  struct ibv_device_attr_ex *attr)
{
	return ibv_query_device_ex(context, input, attr);
}

static int
mlx4_glue_query_port(struct ibv_context *context, uint8_t port_num,
		     struct ibv_port_attr *port_attr)
{
	return ibv_query_port(context, port_num, port_attr);
}

static const char *
mlx4_glue_port_state_str(enum ibv_port_state port_state)
{
	return ibv_port_state_str(port_state);
}

static struct ibv_comp_channel *
mlx4_glue_create_comp_channel(struct ibv_context *context)
{
	return ibv_create_comp_channel(context);
}

static int
mlx4_glue_destroy_comp_channel(struct ibv_comp_channel *channel)
{
	return ibv_destroy_comp_channel(channel);
}

static struct ibv_cq *
mlx4_glue_create_cq(struct ibv_context *context, int cqe, void *cq_context,
		    struct ibv_comp_channel *channel, int comp_vector)
{
	return ibv_create_cq(context, cqe, cq_context, channel, comp_vector);
}

static int
mlx4_glue_destroy_cq(struct ibv_cq *cq)
{
	return ibv_destroy_cq(cq);
}

static int
mlx4_glue_get_cq_event(struct ibv_comp_channel *channel, struct ibv_cq **cq,
		       void **cq_context)
{
	return ibv_get_cq_event(channel, cq, cq_context);
}

static void
mlx4_glue_ack_cq_events(struct ibv_cq *cq, unsigned int nevents)
{
	ibv_ack_cq_events(cq, nevents);
}

static struct ibv_flow *
mlx4_glue_create_flow(struct ibv_qp *qp, struct ibv_flow_attr *flow)
{
	return ibv_create_flow(qp, flow);
}

static int
mlx4_glue_destroy_flow(struct ibv_flow *flow_id)
{
	return ibv_destroy_flow(flow_id);
}

static struct ibv_qp *
mlx4_glue_create_qp(struct ibv_pd *pd, struct ibv_qp_init_attr *qp_init_attr)
{
	return ibv_create_qp(pd, qp_init_attr);
}

static struct ibv_qp *
mlx4_glue_create_qp_ex(struct ibv_context *context,
		       struct ibv_qp_init_attr_ex *qp_init_attr_ex)
{
	return ibv_create_qp_ex(context, qp_init_attr_ex);
}

static int
mlx4_glue_destroy_qp(struct ibv_qp *qp)
{
	return ibv_destroy_qp(qp);
}

static int
mlx4_glue_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask)
{
	return ibv_modify_qp(qp, attr, attr_mask);
}

static struct ibv_mr *
mlx4_glue_reg_mr(struct ibv_pd *pd, void *addr, size_t length, int access)
{
	return ibv_reg_mr(pd, addr, length, access);
}

static int
mlx4_glue_dereg_mr(struct ibv_mr *mr)
{
	return ibv_dereg_mr(mr);
}

static struct ibv_rwq_ind_table *
mlx4_glue_create_rwq_ind_table(struct ibv_context *context,
			       struct ibv_rwq_ind_table_init_attr *init_attr)
{
	return ibv_create_rwq_ind_table(context, init_attr);
}

static int
mlx4_glue_destroy_rwq_ind_table(struct ibv_rwq_ind_table *rwq_ind_table)
{
	return ibv_destroy_rwq_ind_table(rwq_ind_table);
}

static struct ibv_wq *
mlx4_glue_create_wq(struct ibv_context *context,
		    struct ibv_wq_init_attr *wq_init_attr)
{
	return ibv_create_wq(context, wq_init_attr);
}

static int
mlx4_glue_destroy_wq(struct ibv_wq *wq)
{
	return ibv_destroy_wq(wq);
}
static int
mlx4_glue_modify_wq(struct ibv_wq *wq, struct ibv_wq_attr *wq_attr)
{
	return ibv_modify_wq(wq, wq_attr);
}

static int
mlx4_glue_dv_init_obj(struct mlx4dv_obj *obj, uint64_t obj_type)
{
	return mlx4dv_init_obj(obj, obj_type);
}

static int
mlx4_glue_dv_set_context_attr(struct ibv_context *context,
			      enum mlx4dv_set_ctx_attr_type attr_type,
			      void *attr)
{
	return mlx4dv_set_context_attr(context, attr_type, attr);
}

const struct mlx4_glue *mlx4_glue = &(const struct mlx4_glue){
	.version = MLX4_GLUE_VERSION,
	.fork_init = mlx4_glue_fork_init,
	.get_async_event = mlx4_glue_get_async_event,
	.ack_async_event = mlx4_glue_ack_async_event,
	.alloc_pd = mlx4_glue_alloc_pd,
	.dealloc_pd = mlx4_glue_dealloc_pd,
	.get_device_list = mlx4_glue_get_device_list,
	.free_device_list = mlx4_glue_free_device_list,
	.open_device = mlx4_glue_open_device,
	.close_device = mlx4_glue_close_device,
	.get_device_name = mlx4_glue_get_device_name,
	.query_device = mlx4_glue_query_device,
	.query_device_ex = mlx4_glue_query_device_ex,
	.query_port = mlx4_glue_query_port,
	.port_state_str = mlx4_glue_port_state_str,
	.create_comp_channel = mlx4_glue_create_comp_channel,
	.destroy_comp_channel = mlx4_glue_destroy_comp_channel,
	.create_cq = mlx4_glue_create_cq,
	.destroy_cq = mlx4_glue_destroy_cq,
	.get_cq_event = mlx4_glue_get_cq_event,
	.ack_cq_events = mlx4_glue_ack_cq_events,
	.create_flow = mlx4_glue_create_flow,
	.destroy_flow = mlx4_glue_destroy_flow,
	.create_qp = mlx4_glue_create_qp,
	.create_qp_ex = mlx4_glue_create_qp_ex,
	.destroy_qp = mlx4_glue_destroy_qp,
	.modify_qp = mlx4_glue_modify_qp,
	.reg_mr = mlx4_glue_reg_mr,
	.dereg_mr = mlx4_glue_dereg_mr,
	.create_rwq_ind_table = mlx4_glue_create_rwq_ind_table,
	.destroy_rwq_ind_table = mlx4_glue_destroy_rwq_ind_table,
	.create_wq = mlx4_glue_create_wq,
	.destroy_wq = mlx4_glue_destroy_wq,
	.modify_wq = mlx4_glue_modify_wq,
	.dv_init_obj = mlx4_glue_dv_init_obj,
	.dv_set_context_attr = mlx4_glue_dv_set_context_attr,
};
