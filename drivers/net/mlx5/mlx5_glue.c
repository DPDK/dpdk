/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 6WIND S.A.
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Not needed by this file; included to work around the lack of off_t
 * definition for mlx5dv.h with unpatched rdma-core versions.
 */
#include <sys/types.h>

/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/mlx5dv.h>
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include "mlx5_autoconf.h"
#include "mlx5_glue.h"

static int
mlx5_glue_fork_init(void)
{
	return ibv_fork_init();
}

static struct ibv_pd *
mlx5_glue_alloc_pd(struct ibv_context *context)
{
	return ibv_alloc_pd(context);
}

static int
mlx5_glue_dealloc_pd(struct ibv_pd *pd)
{
	return ibv_dealloc_pd(pd);
}

static struct ibv_device **
mlx5_glue_get_device_list(int *num_devices)
{
	return ibv_get_device_list(num_devices);
}

static void
mlx5_glue_free_device_list(struct ibv_device **list)
{
	ibv_free_device_list(list);
}

static struct ibv_context *
mlx5_glue_open_device(struct ibv_device *device)
{
	return ibv_open_device(device);
}

static int
mlx5_glue_close_device(struct ibv_context *context)
{
	return ibv_close_device(context);
}

static int
mlx5_glue_query_device(struct ibv_context *context,
		       struct ibv_device_attr *device_attr)
{
	return ibv_query_device(context, device_attr);
}

static int
mlx5_glue_query_device_ex(struct ibv_context *context,
			  const struct ibv_query_device_ex_input *input,
			  struct ibv_device_attr_ex *attr)
{
	return ibv_query_device_ex(context, input, attr);
}

static int
mlx5_glue_query_port(struct ibv_context *context, uint8_t port_num,
		     struct ibv_port_attr *port_attr)
{
	return ibv_query_port(context, port_num, port_attr);
}

static struct ibv_comp_channel *
mlx5_glue_create_comp_channel(struct ibv_context *context)
{
	return ibv_create_comp_channel(context);
}

static int
mlx5_glue_destroy_comp_channel(struct ibv_comp_channel *channel)
{
	return ibv_destroy_comp_channel(channel);
}

static struct ibv_cq *
mlx5_glue_create_cq(struct ibv_context *context, int cqe, void *cq_context,
		    struct ibv_comp_channel *channel, int comp_vector)
{
	return ibv_create_cq(context, cqe, cq_context, channel, comp_vector);
}

static int
mlx5_glue_destroy_cq(struct ibv_cq *cq)
{
	return ibv_destroy_cq(cq);
}

static int
mlx5_glue_get_cq_event(struct ibv_comp_channel *channel, struct ibv_cq **cq,
		       void **cq_context)
{
	return ibv_get_cq_event(channel, cq, cq_context);
}

static void
mlx5_glue_ack_cq_events(struct ibv_cq *cq, unsigned int nevents)
{
	ibv_ack_cq_events(cq, nevents);
}

static struct ibv_rwq_ind_table *
mlx5_glue_create_rwq_ind_table(struct ibv_context *context,
			       struct ibv_rwq_ind_table_init_attr *init_attr)
{
	return ibv_create_rwq_ind_table(context, init_attr);
}

static int
mlx5_glue_destroy_rwq_ind_table(struct ibv_rwq_ind_table *rwq_ind_table)
{
	return ibv_destroy_rwq_ind_table(rwq_ind_table);
}

static struct ibv_wq *
mlx5_glue_create_wq(struct ibv_context *context,
		    struct ibv_wq_init_attr *wq_init_attr)
{
	return ibv_create_wq(context, wq_init_attr);
}

static int
mlx5_glue_destroy_wq(struct ibv_wq *wq)
{
	return ibv_destroy_wq(wq);
}
static int
mlx5_glue_modify_wq(struct ibv_wq *wq, struct ibv_wq_attr *wq_attr)
{
	return ibv_modify_wq(wq, wq_attr);
}

static struct ibv_flow *
mlx5_glue_create_flow(struct ibv_qp *qp, struct ibv_flow_attr *flow)
{
	return ibv_create_flow(qp, flow);
}

static int
mlx5_glue_destroy_flow(struct ibv_flow *flow_id)
{
	return ibv_destroy_flow(flow_id);
}

static struct ibv_qp *
mlx5_glue_create_qp(struct ibv_pd *pd, struct ibv_qp_init_attr *qp_init_attr)
{
	return ibv_create_qp(pd, qp_init_attr);
}

static struct ibv_qp *
mlx5_glue_create_qp_ex(struct ibv_context *context,
		       struct ibv_qp_init_attr_ex *qp_init_attr_ex)
{
	return ibv_create_qp_ex(context, qp_init_attr_ex);
}

static int
mlx5_glue_destroy_qp(struct ibv_qp *qp)
{
	return ibv_destroy_qp(qp);
}

static int
mlx5_glue_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask)
{
	return ibv_modify_qp(qp, attr, attr_mask);
}

static struct ibv_mr *
mlx5_glue_reg_mr(struct ibv_pd *pd, void *addr, size_t length, int access)
{
	return ibv_reg_mr(pd, addr, length, access);
}

static int
mlx5_glue_dereg_mr(struct ibv_mr *mr)
{
	return ibv_dereg_mr(mr);
}

static struct ibv_counter_set *
mlx5_glue_create_counter_set(struct ibv_context *context,
			     struct ibv_counter_set_init_attr *init_attr)
{
#ifndef HAVE_IBV_DEVICE_COUNTERS_SET_SUPPORT
	(void)context;
	(void)init_attr;
	return NULL;
#else
	return ibv_create_counter_set(context, init_attr);
#endif
}

static int
mlx5_glue_destroy_counter_set(struct ibv_counter_set *cs)
{
#ifndef HAVE_IBV_DEVICE_COUNTERS_SET_SUPPORT
	(void)cs;
	return ENOTSUP;
#else
	return ibv_destroy_counter_set(cs);
#endif
}

static int
mlx5_glue_describe_counter_set(struct ibv_context *context,
			       uint16_t counter_set_id,
			       struct ibv_counter_set_description *cs_desc)
{
#ifndef HAVE_IBV_DEVICE_COUNTERS_SET_SUPPORT
	(void)context;
	(void)counter_set_id;
	(void)cs_desc;
	return ENOTSUP;
#else
	return ibv_describe_counter_set(context, counter_set_id, cs_desc);
#endif
}

static int
mlx5_glue_query_counter_set(struct ibv_query_counter_set_attr *query_attr,
			    struct ibv_counter_set_data *cs_data)
{
#ifndef HAVE_IBV_DEVICE_COUNTERS_SET_SUPPORT
	(void)query_attr;
	(void)cs_data;
	return ENOTSUP;
#else
	return ibv_query_counter_set(query_attr, cs_data);
#endif
}

static void
mlx5_glue_ack_async_event(struct ibv_async_event *event)
{
	ibv_ack_async_event(event);
}

static int
mlx5_glue_get_async_event(struct ibv_context *context,
			  struct ibv_async_event *event)
{
	return ibv_get_async_event(context, event);
}

static const char *
mlx5_glue_port_state_str(enum ibv_port_state port_state)
{
	return ibv_port_state_str(port_state);
}

static struct ibv_cq *
mlx5_glue_cq_ex_to_cq(struct ibv_cq_ex *cq)
{
	return ibv_cq_ex_to_cq(cq);
}

static struct ibv_cq_ex *
mlx5_glue_dv_create_cq(struct ibv_context *context,
		       struct ibv_cq_init_attr_ex *cq_attr,
		       struct mlx5dv_cq_init_attr *mlx5_cq_attr)
{
	return mlx5dv_create_cq(context, cq_attr, mlx5_cq_attr);
}

static struct ibv_wq *
mlx5_glue_dv_create_wq(struct ibv_context *context,
		       struct ibv_wq_init_attr *wq_attr,
		       struct mlx5dv_wq_init_attr *mlx5_wq_attr)
{
#ifndef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
	(void)context;
	(void)wq_attr;
	(void)mlx5_wq_attr;
	return NULL;
#else
	return mlx5dv_create_wq(context, wq_attr, mlx5_wq_attr);
#endif
}

static int
mlx5_glue_dv_query_device(struct ibv_context *ctx,
			  struct mlx5dv_context *attrs_out)
{
	return mlx5dv_query_device(ctx, attrs_out);
}

static int
mlx5_glue_dv_set_context_attr(struct ibv_context *ibv_ctx,
			      enum mlx5dv_set_ctx_attr_type type, void *attr)
{
	return mlx5dv_set_context_attr(ibv_ctx, type, attr);
}

static int
mlx5_glue_dv_init_obj(struct mlx5dv_obj *obj, uint64_t obj_type)
{
	return mlx5dv_init_obj(obj, obj_type);
}

static struct ibv_qp *
mlx5_glue_dv_create_qp(struct ibv_context *context,
		       struct ibv_qp_init_attr_ex *qp_init_attr_ex,
		       struct mlx5dv_qp_init_attr *dv_qp_init_attr)
{
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	return mlx5dv_create_qp(context, qp_init_attr_ex, dv_qp_init_attr);
#else
	(void)context;
	(void)qp_init_attr_ex;
	(void)dv_qp_init_attr;
	return NULL;
#endif
}

const struct mlx5_glue *mlx5_glue = &(const struct mlx5_glue){
	.version = MLX5_GLUE_VERSION,
	.fork_init = mlx5_glue_fork_init,
	.alloc_pd = mlx5_glue_alloc_pd,
	.dealloc_pd = mlx5_glue_dealloc_pd,
	.get_device_list = mlx5_glue_get_device_list,
	.free_device_list = mlx5_glue_free_device_list,
	.open_device = mlx5_glue_open_device,
	.close_device = mlx5_glue_close_device,
	.query_device = mlx5_glue_query_device,
	.query_device_ex = mlx5_glue_query_device_ex,
	.query_port = mlx5_glue_query_port,
	.create_comp_channel = mlx5_glue_create_comp_channel,
	.destroy_comp_channel = mlx5_glue_destroy_comp_channel,
	.create_cq = mlx5_glue_create_cq,
	.destroy_cq = mlx5_glue_destroy_cq,
	.get_cq_event = mlx5_glue_get_cq_event,
	.ack_cq_events = mlx5_glue_ack_cq_events,
	.create_rwq_ind_table = mlx5_glue_create_rwq_ind_table,
	.destroy_rwq_ind_table = mlx5_glue_destroy_rwq_ind_table,
	.create_wq = mlx5_glue_create_wq,
	.destroy_wq = mlx5_glue_destroy_wq,
	.modify_wq = mlx5_glue_modify_wq,
	.create_flow = mlx5_glue_create_flow,
	.destroy_flow = mlx5_glue_destroy_flow,
	.create_qp = mlx5_glue_create_qp,
	.create_qp_ex = mlx5_glue_create_qp_ex,
	.destroy_qp = mlx5_glue_destroy_qp,
	.modify_qp = mlx5_glue_modify_qp,
	.reg_mr = mlx5_glue_reg_mr,
	.dereg_mr = mlx5_glue_dereg_mr,
	.create_counter_set = mlx5_glue_create_counter_set,
	.destroy_counter_set = mlx5_glue_destroy_counter_set,
	.describe_counter_set = mlx5_glue_describe_counter_set,
	.query_counter_set = mlx5_glue_query_counter_set,
	.ack_async_event = mlx5_glue_ack_async_event,
	.get_async_event = mlx5_glue_get_async_event,
	.port_state_str = mlx5_glue_port_state_str,
	.cq_ex_to_cq = mlx5_glue_cq_ex_to_cq,
	.dv_create_cq = mlx5_glue_dv_create_cq,
	.dv_create_wq = mlx5_glue_dv_create_wq,
	.dv_query_device = mlx5_glue_dv_query_device,
	.dv_set_context_attr = mlx5_glue_dv_set_context_attr,
	.dv_init_obj = mlx5_glue_dv_init_obj,
	.dv_create_qp = mlx5_glue_dv_create_qp,
};
