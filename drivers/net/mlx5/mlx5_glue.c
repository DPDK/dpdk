/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 6WIND S.A.
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#include <errno.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

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

#include <rte_config.h>

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

static int
mlx5_glue_destroy_flow_action(void *action)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
#ifdef HAVE_MLX5DV_DR
	return mlx5dv_dr_action_destroy(action);
#else
	struct mlx5dv_flow_action_attr *attr = action;
	int res = 0;
	switch (attr->type) {
	case MLX5DV_FLOW_ACTION_TAG:
		break;
	default:
		res = ibv_destroy_flow_action(attr->action);
		break;
	}
	free(action);
	return res;
#endif
#else
	(void)action;
	return ENOTSUP;
#endif
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
#ifndef HAVE_IBV_DEVICE_COUNTERS_SET_V42
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
#ifndef HAVE_IBV_DEVICE_COUNTERS_SET_V42
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
#ifndef HAVE_IBV_DEVICE_COUNTERS_SET_V42
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
#ifndef HAVE_IBV_DEVICE_COUNTERS_SET_V42
	(void)query_attr;
	(void)cs_data;
	return ENOTSUP;
#else
	return ibv_query_counter_set(query_attr, cs_data);
#endif
}

static struct ibv_counters *
mlx5_glue_create_counters(struct ibv_context *context,
			  struct ibv_counters_init_attr *init_attr)
{
#ifndef HAVE_IBV_DEVICE_COUNTERS_SET_V45
	(void)context;
	(void)init_attr;
	errno = ENOTSUP;
	return NULL;
#else
	return ibv_create_counters(context, init_attr);
#endif
}

static int
mlx5_glue_destroy_counters(struct ibv_counters *counters)
{
#ifndef HAVE_IBV_DEVICE_COUNTERS_SET_V45
	(void)counters;
	return ENOTSUP;
#else
	return ibv_destroy_counters(counters);
#endif
}

static int
mlx5_glue_attach_counters(struct ibv_counters *counters,
			  struct ibv_counter_attach_attr *attr,
			  struct ibv_flow *flow)
{
#ifndef HAVE_IBV_DEVICE_COUNTERS_SET_V45
	(void)counters;
	(void)attr;
	(void)flow;
	return ENOTSUP;
#else
	return ibv_attach_counters_point_flow(counters, attr, flow);
#endif
}

static int
mlx5_glue_query_counters(struct ibv_counters *counters,
			 uint64_t *counters_value,
			 uint32_t ncounters,
			 uint32_t flags)
{
#ifndef HAVE_IBV_DEVICE_COUNTERS_SET_V45
	(void)counters;
	(void)counters_value;
	(void)ncounters;
	(void)flags;
	return ENOTSUP;
#else
	return ibv_read_counters(counters, counters_value, ncounters, flags);
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

static void *
mlx5_glue_dr_create_flow_action_dest_flow_tbl(void *tbl)
{
#ifdef HAVE_MLX5DV_DR
	return mlx5dv_dr_action_create_dest_table(tbl);
#else
	(void)tbl;
	errno = ENOTSUP;
	return NULL;
#endif
}

static void *
mlx5_glue_dr_create_flow_action_dest_vport(void *domain, uint32_t vport)
{
#ifdef HAVE_MLX5DV_DR_ESWITCH
	return mlx5dv_dr_action_create_dest_vport(domain, vport);
#else
	(void)domain;
	(void)vport;
	errno = ENOTSUP;
	return NULL;
#endif
}

static void *
mlx5_glue_dr_create_flow_action_drop(void)
{
#ifdef HAVE_MLX5DV_DR_ESWITCH
	return mlx5dv_dr_action_create_drop();
#else
	errno = ENOTSUP;
	return NULL;
#endif
}

static void *
mlx5_glue_dr_create_flow_tbl(void *domain, uint32_t level)
{
#ifdef HAVE_MLX5DV_DR
	return mlx5dv_dr_table_create(domain, level);
#else
	(void)domain;
	(void)level;
	errno = ENOTSUP;
	return NULL;
#endif
}

static int
mlx5_glue_dr_destroy_flow_tbl(void *tbl)
{
#ifdef HAVE_MLX5DV_DR
	return mlx5dv_dr_table_destroy(tbl);
#else
	(void)tbl;
	errno = ENOTSUP;
	return errno;
#endif
}

static void *
mlx5_glue_dr_create_domain(struct ibv_context *ctx,
			   enum  mlx5dv_dr_domain_type domain)
{
#ifdef HAVE_MLX5DV_DR
	return mlx5dv_dr_domain_create(ctx, domain);
#else
	(void)ctx;
	(void)domain;
	errno = ENOTSUP;
	return NULL;
#endif
}

static int
mlx5_glue_dr_destroy_domain(void *domain)
{
#ifdef HAVE_MLX5DV_DR
	return mlx5dv_dr_domain_destroy(domain);
#else
	(void)domain;
	errno = ENOTSUP;
	return errno;
#endif
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
	errno = ENOTSUP;
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
	errno = ENOTSUP;
	return NULL;
#endif
}

static void *
mlx5_glue_dv_create_flow_matcher(struct ibv_context *context,
				 struct mlx5dv_flow_matcher_attr *matcher_attr,
				 void *tbl)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
#ifdef HAVE_MLX5DV_DR
	(void)context;
	return mlx5dv_dr_matcher_create(tbl, matcher_attr->priority,
					matcher_attr->match_criteria_enable,
					matcher_attr->match_mask);
#else
	(void)tbl;
	return mlx5dv_create_flow_matcher(context, matcher_attr);
#endif
#else
	(void)context;
	(void)matcher_attr;
	(void)tbl;
	errno = ENOTSUP;
	return NULL;
#endif
}

static void *
mlx5_glue_dv_create_flow(void *matcher,
			 void *match_value,
			 size_t num_actions,
			 void *actions[])
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
#ifdef HAVE_MLX5DV_DR
	return mlx5dv_dr_rule_create(matcher, match_value, num_actions,
				     (struct mlx5dv_dr_action **)actions);
#else
	struct mlx5dv_flow_action_attr actions_attr[8];

	if (num_actions > 8)
		return NULL;
	for (size_t i = 0; i < num_actions; i++)
		actions_attr[i] =
			*((struct mlx5dv_flow_action_attr *)(actions[i]));
	return mlx5dv_create_flow(matcher, match_value,
				  num_actions, actions_attr);
#endif
#else
	(void)matcher;
	(void)match_value;
	(void)num_actions;
	(void)actions;
	return NULL;
#endif
}

static void *
mlx5_glue_dv_create_flow_action_counter(void *counter_obj, uint32_t offset)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
#ifdef HAVE_MLX5DV_DR
	return mlx5dv_dr_action_create_flow_counter(counter_obj, offset);
#else
	struct mlx5dv_flow_action_attr *action;

	(void)offset;
	action = malloc(sizeof(*action));
	if (!action)
		return NULL;
	action->type = MLX5DV_FLOW_ACTION_COUNTERS_DEVX;
	action->obj = counter_obj;
	return action;
#endif
#else
	(void)counter_obj;
	(void)offset;
	errno = ENOTSUP;
	return NULL;
#endif
}

static void *
mlx5_glue_dv_create_flow_action_dest_ibv_qp(void *qp)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
#ifdef HAVE_MLX5DV_DR
	return mlx5dv_dr_action_create_dest_ibv_qp(qp);
#else
	struct mlx5dv_flow_action_attr *action;

	action = malloc(sizeof(*action));
	if (!action)
		return NULL;
	action->type = MLX5DV_FLOW_ACTION_DEST_IBV_QP;
	action->obj = qp;
	return action;
#endif
#else
	(void)qp;
	errno = ENOTSUP;
	return NULL;
#endif
}

static void *
mlx5_glue_dv_create_flow_action_modify_header
					(struct ibv_context *ctx,
					 enum mlx5dv_flow_table_type ft_type,
					 void *domain, uint64_t flags,
					 size_t actions_sz,
					 uint64_t actions[])
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
#ifdef HAVE_MLX5DV_DR
	(void)ctx;
	(void)ft_type;
	return mlx5dv_dr_action_create_modify_header(domain, flags, actions_sz,
						     (__be64 *)actions);
#else
	struct mlx5dv_flow_action_attr *action;

	(void)domain;
	(void)flags;
	action = malloc(sizeof(*action));
	if (!action)
		return NULL;
	action->type = MLX5DV_FLOW_ACTION_IBV_FLOW_ACTION;
	action->action = mlx5dv_create_flow_action_modify_header
		(ctx, actions_sz, actions, ft_type);
	return action;
#endif
#else
	(void)ctx;
	(void)ft_type;
	(void)domain;
	(void)flags;
	(void)actions_sz;
	(void)actions;
	errno = ENOTSUP;
	return NULL;
#endif
}

static void *
mlx5_glue_dv_create_flow_action_packet_reformat
		(struct ibv_context *ctx,
		 enum mlx5dv_flow_action_packet_reformat_type reformat_type,
		 enum mlx5dv_flow_table_type ft_type,
		 struct mlx5dv_dr_domain *domain,
		 uint32_t flags, size_t data_sz, void *data)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
#ifdef HAVE_MLX5DV_DR
	(void)ctx;
	(void)ft_type;
	return mlx5dv_dr_action_create_packet_reformat(domain, flags,
						       reformat_type, data_sz,
						       data);
#else
	(void)domain;
	(void)flags;
	struct mlx5dv_flow_action_attr *action;

	action = malloc(sizeof(*action));
	if (!action)
		return NULL;
	action->type = MLX5DV_FLOW_ACTION_IBV_FLOW_ACTION;
	action->action = mlx5dv_create_flow_action_packet_reformat
		(ctx, data_sz, data, reformat_type, ft_type);
	return action;
#endif
#else
	(void)ctx;
	(void)reformat_type;
	(void)ft_type;
	(void)domain;
	(void)flags;
	(void)data_sz;
	(void)data;
	errno = ENOTSUP;
	return NULL;
#endif
}

static void *
mlx5_glue_dv_create_flow_action_tag(uint32_t tag)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
#ifdef HAVE_MLX5DV_DR
	return mlx5dv_dr_action_create_tag(tag);
#else
	struct mlx5dv_flow_action_attr *action;
	action = malloc(sizeof(*action));
	if (!action)
		return NULL;
	action->type = MLX5DV_FLOW_ACTION_TAG;
	action->tag_value = tag;
	return action;
#endif
#endif
	(void)tag;
	errno = ENOTSUP;
	return NULL;
}

static int
mlx5_glue_dv_destroy_flow(void *flow_id)
{
#ifdef HAVE_MLX5DV_DR
	return mlx5dv_dr_rule_destroy(flow_id);
#else
	return ibv_destroy_flow(flow_id);
#endif
}

static int
mlx5_glue_dv_destroy_flow_matcher(void *matcher)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
#ifdef HAVE_MLX5DV_DR
	return mlx5dv_dr_matcher_destroy(matcher);
#else
	return mlx5dv_destroy_flow_matcher(matcher);
#endif
#else
	(void)matcher;
	errno = ENOTSUP;
	return errno;
#endif
}

static struct ibv_context *
mlx5_glue_dv_open_device(struct ibv_device *device)
{
#ifdef HAVE_IBV_DEVX_OBJ
	return mlx5dv_open_device(device,
				  &(struct mlx5dv_context_attr){
					.flags = MLX5DV_CONTEXT_FLAGS_DEVX,
				  });
#else
	(void)device;
	errno = ENOTSUP;
	return NULL;
#endif
}

static struct mlx5dv_devx_obj *
mlx5_glue_devx_obj_create(struct ibv_context *ctx,
			  const void *in, size_t inlen,
			  void *out, size_t outlen)
{
#ifdef HAVE_IBV_DEVX_OBJ
	return mlx5dv_devx_obj_create(ctx, in, inlen, out, outlen);
#else
	(void)ctx;
	(void)in;
	(void)inlen;
	(void)out;
	(void)outlen;
	errno = ENOTSUP;
	return NULL;
#endif
}

static int
mlx5_glue_devx_obj_destroy(struct mlx5dv_devx_obj *obj)
{
#ifdef HAVE_IBV_DEVX_OBJ
	return mlx5dv_devx_obj_destroy(obj);
#else
	(void)obj;
	return -ENOTSUP;
#endif
}

static int
mlx5_glue_devx_obj_query(struct mlx5dv_devx_obj *obj,
			 const void *in, size_t inlen,
			 void *out, size_t outlen)
{
#ifdef HAVE_IBV_DEVX_OBJ
	return mlx5dv_devx_obj_query(obj, in, inlen, out, outlen);
#else
	(void)obj;
	(void)in;
	(void)inlen;
	(void)out;
	(void)outlen;
	return -ENOTSUP;
#endif
}

static int
mlx5_glue_devx_obj_modify(struct mlx5dv_devx_obj *obj,
			  const void *in, size_t inlen,
			  void *out, size_t outlen)
{
#ifdef HAVE_IBV_DEVX_OBJ
	return mlx5dv_devx_obj_modify(obj, in, inlen, out, outlen);
#else
	(void)obj;
	(void)in;
	(void)inlen;
	(void)out;
	(void)outlen;
	return -ENOTSUP;
#endif
}

static int
mlx5_glue_devx_general_cmd(struct ibv_context *ctx,
			   const void *in, size_t inlen,
			   void *out, size_t outlen)
{
#ifdef HAVE_IBV_DEVX_OBJ
	return mlx5dv_devx_general_cmd(ctx, in, inlen, out, outlen);
#else
	(void)ctx;
	(void)in;
	(void)inlen;
	(void)out;
	(void)outlen;
	return -ENOTSUP;
#endif
}

alignas(RTE_CACHE_LINE_SIZE)
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
	.destroy_flow_action = mlx5_glue_destroy_flow_action,
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
	.create_counters = mlx5_glue_create_counters,
	.destroy_counters = mlx5_glue_destroy_counters,
	.attach_counters = mlx5_glue_attach_counters,
	.query_counters = mlx5_glue_query_counters,
	.ack_async_event = mlx5_glue_ack_async_event,
	.get_async_event = mlx5_glue_get_async_event,
	.port_state_str = mlx5_glue_port_state_str,
	.cq_ex_to_cq = mlx5_glue_cq_ex_to_cq,
	.dr_create_flow_action_dest_flow_tbl =
		mlx5_glue_dr_create_flow_action_dest_flow_tbl,
	.dr_create_flow_action_dest_vport =
		mlx5_glue_dr_create_flow_action_dest_vport,
	.dr_create_flow_action_drop =
		mlx5_glue_dr_create_flow_action_drop,
	.dr_create_flow_tbl = mlx5_glue_dr_create_flow_tbl,
	.dr_destroy_flow_tbl = mlx5_glue_dr_destroy_flow_tbl,
	.dr_create_domain = mlx5_glue_dr_create_domain,
	.dr_destroy_domain = mlx5_glue_dr_destroy_domain,
	.dv_create_cq = mlx5_glue_dv_create_cq,
	.dv_create_wq = mlx5_glue_dv_create_wq,
	.dv_query_device = mlx5_glue_dv_query_device,
	.dv_set_context_attr = mlx5_glue_dv_set_context_attr,
	.dv_init_obj = mlx5_glue_dv_init_obj,
	.dv_create_qp = mlx5_glue_dv_create_qp,
	.dv_create_flow_matcher = mlx5_glue_dv_create_flow_matcher,
	.dv_create_flow = mlx5_glue_dv_create_flow,
	.dv_create_flow_action_counter =
		mlx5_glue_dv_create_flow_action_counter,
	.dv_create_flow_action_dest_ibv_qp =
		mlx5_glue_dv_create_flow_action_dest_ibv_qp,
	.dv_create_flow_action_modify_header =
		mlx5_glue_dv_create_flow_action_modify_header,
	.dv_create_flow_action_packet_reformat =
		mlx5_glue_dv_create_flow_action_packet_reformat,
	.dv_create_flow_action_tag =  mlx5_glue_dv_create_flow_action_tag,
	.dv_destroy_flow = mlx5_glue_dv_destroy_flow,
	.dv_destroy_flow_matcher = mlx5_glue_dv_destroy_flow_matcher,
	.dv_open_device = mlx5_glue_dv_open_device,
	.devx_obj_create = mlx5_glue_devx_obj_create,
	.devx_obj_destroy = mlx5_glue_devx_obj_destroy,
	.devx_obj_query = mlx5_glue_devx_obj_query,
	.devx_obj_modify = mlx5_glue_devx_obj_modify,
	.devx_general_cmd = mlx5_glue_devx_general_cmd,
};
