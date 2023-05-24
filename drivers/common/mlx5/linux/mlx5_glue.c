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

#include "mlx5_glue.h"

static int
mlx5_glue_fork_init(void)
{
#ifdef HAVE_IBV_FORK_UNNEEDED
	if (ibv_is_fork_initialized() == IBV_FORK_UNNEEDED)
		return 0; /* ibv_fork_init() not needed */
#endif
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

static struct ibv_pd *
mlx5_glue_import_pd(struct ibv_context *context, uint32_t pd_handle)
{
#ifdef HAVE_MLX5_IBV_IMPORT_CTX_PD_AND_MR
	return ibv_import_pd(context, pd_handle);
#else
	(void)context;
	(void)pd_handle;
	errno = ENOTSUP;
	return NULL;
#endif
}

static int
mlx5_glue_unimport_pd(struct ibv_pd *pd)
{
#ifdef HAVE_MLX5_IBV_IMPORT_CTX_PD_AND_MR
	ibv_unimport_pd(pd);
	return 0;
#else
	(void)pd;
	errno = ENOTSUP;
	return -errno;
#endif
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

static struct ibv_context *
mlx5_glue_import_device(int cmd_fd)
{
#ifdef HAVE_MLX5_IBV_IMPORT_CTX_PD_AND_MR
	return ibv_import_device(cmd_fd);
#else
	(void)cmd_fd;
	errno = ENOTSUP;
	return NULL;
#endif
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

static const char *
mlx5_glue_get_device_name(struct ibv_device *device)
{
	return ibv_get_device_name(device);
}

static int
mlx5_glue_query_rt_values_ex(struct ibv_context *context,
			  struct ibv_values_ex *values)
{
	return ibv_query_rt_values_ex(context, values);
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
	return -ENOTSUP;
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

static struct ibv_mr *
mlx5_glue_reg_mr_iova(struct ibv_pd *pd, void *addr, size_t length,
		      uint64_t iova, int access)
{
#ifdef HAVE_MLX5_IBV_REG_MR_IOVA
		return ibv_reg_mr_iova(pd, addr, length, iova, access);
#else
	(void)pd;
	(void)addr;
	(void)length;
	(void)iova;
	(void)access;
	errno = ENOTSUP;
	return NULL;
#endif
}

static struct ibv_mr *
mlx5_glue_alloc_null_mr(struct ibv_pd *pd)
{
#ifdef HAVE_IBV_DEVX_OBJ
	return ibv_alloc_null_mr(pd);
#else
	(void)pd;
	errno = ENOTSUP;
	return NULL;
#endif
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
	return -ENOTSUP;
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
	return -ENOTSUP;
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
	return -ENOTSUP;
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
	return -ENOTSUP;
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
	return -ENOTSUP;
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
	return -ENOTSUP;
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
mlx5_glue_dr_create_flow_action_dest_port(void *domain, uint32_t port)
{
#ifdef HAVE_MLX5DV_DR_CREATE_DEST_IB_PORT
	return mlx5dv_dr_action_create_dest_ib_port(domain, port);
#else
#ifdef HAVE_MLX5DV_DR_ESWITCH
	return mlx5dv_dr_action_create_dest_vport(domain, port);
#else
	(void)domain;
	(void)port;
	errno = ENOTSUP;
	return NULL;
#endif
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
mlx5_glue_dr_create_flow_action_push_vlan(struct mlx5dv_dr_domain *domain,
					  rte_be32_t vlan_tag)
{
#ifdef HAVE_MLX5DV_DR_VLAN
	return mlx5dv_dr_action_create_push_vlan(domain, vlan_tag);
#else
	(void)domain;
	(void)vlan_tag;
	errno = ENOTSUP;
	return NULL;
#endif
}

static void *
mlx5_glue_dr_create_flow_action_pop_vlan(void)
{
#ifdef HAVE_MLX5DV_DR_VLAN
	return mlx5dv_dr_action_create_pop_vlan();
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

static int
mlx5_glue_dr_sync_domain(void *domain, uint32_t flags)
{
#ifdef HAVE_MLX5DV_DR
	return mlx5dv_dr_domain_sync(domain, flags);
#else
	(void)domain;
	(void)flags;
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
__mlx5_glue_dv_create_flow_matcher(struct ibv_context *context,
		struct mlx5dv_flow_matcher_attr *matcher_attr)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	return mlx5dv_create_flow_matcher(context, matcher_attr);
#else
	(void)context;
	(void)matcher_attr;
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
	return __mlx5_glue_dv_create_flow_matcher(context, matcher_attr);
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
__mlx5_glue_dv_create_flow(void *matcher,
			   void *match_value,
			   size_t num_actions,
			   void *actions)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	return mlx5dv_create_flow(matcher,
				  match_value,
				  num_actions,
				  (struct mlx5dv_flow_action_attr *)actions);
#else
	(void)matcher;
	(void)match_value;
	(void)num_actions;
	(void)actions;
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
	size_t i;
	struct mlx5dv_flow_action_attr actions_attr[8];

	if (num_actions > 8)
		return NULL;
	for (i = 0; i < num_actions; i++)
		actions_attr[i] =
			*((struct mlx5dv_flow_action_attr *)(actions[i]));
	return __mlx5_glue_dv_create_flow(matcher, match_value,
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
mlx5_glue_dv_create_flow_action_dest_devx_tir(void *tir)
{
#ifdef HAVE_MLX5DV_DR_ACTION_DEST_DEVX_TIR
	return mlx5dv_dr_action_create_dest_devx_tir(tir);
#else
	(void)tir;
	errno = ENOTSUP;
	return NULL;
#endif
}

static void *
__mlx5_glue_dv_create_flow_action_modify_header
					(struct ibv_context *ctx,
					 size_t actions_sz,
					 uint64_t actions[],
					 enum mlx5dv_flow_table_type ft_type)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	return mlx5dv_create_flow_action_modify_header
		(ctx, actions_sz, actions, ft_type);
#else
	(void)ctx;
	(void)ft_type;
	(void)actions_sz;
	(void)actions;
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
	action->action = __mlx5_glue_dv_create_flow_action_modify_header
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
__mlx5_glue_dv_create_flow_action_packet_reformat
		(struct ibv_context *ctx,
		 size_t data_sz, void *data,
		 enum mlx5dv_flow_action_packet_reformat_type reformat_type,
		 enum mlx5dv_flow_table_type ft_type)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	return mlx5dv_create_flow_action_packet_reformat
		(ctx, data_sz, data, reformat_type, ft_type);
#else
	(void)ctx;
	(void)reformat_type;
	(void)ft_type;
	(void)data_sz;
	(void)data;
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
	action->action = __mlx5_glue_dv_create_flow_action_packet_reformat
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
#else /* HAVE_MLX5DV_DR */
	struct mlx5dv_flow_action_attr *action;

	action = malloc(sizeof(*action));
	if (!action)
		return NULL;
	action->type = MLX5DV_FLOW_ACTION_TAG;
	action->tag_value = tag;
	return action;
#endif /* HAVE_MLX5DV_DR */
#else /* HAVE_IBV_FLOW_DV_SUPPORT */
	(void)tag;
	errno = ENOTSUP;
	return NULL;
#endif /* HAVE_IBV_FLOW_DV_SUPPORT */
}

static void *
mlx5_glue_dv_create_flow_action_meter(struct mlx5dv_dr_flow_meter_attr *attr)
{
#if defined(HAVE_MLX5DV_DR) && defined(HAVE_MLX5_DR_CREATE_ACTION_FLOW_METER)
	return mlx5dv_dr_action_create_flow_meter(attr);
#else
	(void)attr;
	errno = ENOTSUP;
	return NULL;
#endif
}

static int
mlx5_glue_dv_modify_flow_action_meter(void *action,
				      struct mlx5dv_dr_flow_meter_attr *attr,
				      uint64_t modify_bits)
{
#if defined(HAVE_MLX5DV_DR) && defined(HAVE_MLX5_DR_CREATE_ACTION_FLOW_METER)
	return mlx5dv_dr_action_modify_flow_meter(action, attr, modify_bits);
#else
	(void)action;
	(void)attr;
	(void)modify_bits;
	errno = ENOTSUP;
	return errno;
#endif
}

static void *
mlx5_glue_dv_create_flow_action_aso(struct mlx5dv_dr_domain *domain,
				    void *aso_obj,
				    uint32_t offset,
				    uint32_t flags,
				    uint8_t return_reg_c)
{
#if defined(HAVE_MLX5DV_DR) && defined(HAVE_MLX5_DR_CREATE_ACTION_ASO)
	return mlx5dv_dr_action_create_aso(domain, aso_obj, offset,
					   flags, return_reg_c);
#else
	(void)domain;
	(void)aso_obj;
	(void)offset;
	(void)flags;
	(void)return_reg_c;
	errno = ENOTSUP;
	return NULL;
#endif
}

static void *
mlx5_glue_dr_create_flow_action_default_miss(void)
{
#if defined(HAVE_MLX5DV_DR) && defined(HAVE_MLX5_DR_CREATE_ACTION_DEFAULT_MISS)
	return mlx5dv_dr_action_create_default_miss();
#else
	errno = ENOTSUP;
	return NULL;
#endif
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
__mlx5_glue_dv_destroy_flow_matcher(void *matcher)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	return mlx5dv_destroy_flow_matcher(matcher);
#else
	(void)matcher;
	errno = ENOTSUP;
	return errno;
#endif
}

static int
mlx5_glue_dv_destroy_flow_matcher(void *matcher)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
#ifdef HAVE_MLX5DV_DR
	return mlx5dv_dr_matcher_destroy(matcher);
#else
	return __mlx5_glue_dv_destroy_flow_matcher(matcher);
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

static struct mlx5dv_devx_cmd_comp *
mlx5_glue_devx_create_cmd_comp(struct ibv_context *ctx)
{
#ifdef HAVE_IBV_DEVX_ASYNC
	return mlx5dv_devx_create_cmd_comp(ctx);
#else
	(void)ctx;
	errno = -ENOTSUP;
	return NULL;
#endif
}

static void
mlx5_glue_devx_destroy_cmd_comp(struct mlx5dv_devx_cmd_comp *cmd_comp)
{
#ifdef HAVE_IBV_DEVX_ASYNC
	mlx5dv_devx_destroy_cmd_comp(cmd_comp);
#else
	(void)cmd_comp;
	errno = -ENOTSUP;
#endif
}

static int
mlx5_glue_devx_obj_query_async(struct mlx5dv_devx_obj *obj, const void *in,
			       size_t inlen, size_t outlen, uint64_t wr_id,
			       struct mlx5dv_devx_cmd_comp *cmd_comp)
{
#ifdef HAVE_IBV_DEVX_ASYNC
	return mlx5dv_devx_obj_query_async(obj, in, inlen, outlen, wr_id,
					   cmd_comp);
#else
	(void)obj;
	(void)in;
	(void)inlen;
	(void)outlen;
	(void)wr_id;
	(void)cmd_comp;
	return -ENOTSUP;
#endif
}

static int
mlx5_glue_devx_get_async_cmd_comp(struct mlx5dv_devx_cmd_comp *cmd_comp,
				  struct mlx5dv_devx_async_cmd_hdr *cmd_resp,
				  size_t cmd_resp_len)
{
#ifdef HAVE_IBV_DEVX_ASYNC
	return mlx5dv_devx_get_async_cmd_comp(cmd_comp, cmd_resp,
					      cmd_resp_len);
#else
	(void)cmd_comp;
	(void)cmd_resp;
	(void)cmd_resp_len;
	return -ENOTSUP;
#endif
}

static struct mlx5dv_devx_umem *
mlx5_glue_devx_umem_reg(struct ibv_context *context, void *addr, size_t size,
			uint32_t access)
{
#ifdef HAVE_IBV_DEVX_OBJ
	return mlx5dv_devx_umem_reg(context, addr, size, access);
#else
	(void)context;
	(void)addr;
	(void)size;
	(void)access;
	errno = -ENOTSUP;
	return NULL;
#endif
}

static int
mlx5_glue_devx_umem_dereg(struct mlx5dv_devx_umem *dv_devx_umem)
{
#ifdef HAVE_IBV_DEVX_OBJ
	return mlx5dv_devx_umem_dereg(dv_devx_umem);
#else
	(void)dv_devx_umem;
	return -ENOTSUP;
#endif
}

static int
mlx5_glue_devx_qp_query(struct ibv_qp *qp,
			const void *in, size_t inlen,
			void *out, size_t outlen)
{
#ifdef HAVE_IBV_DEVX_QP
	return mlx5dv_devx_qp_query(qp, in, inlen, out, outlen);
#else
	(void)qp;
	(void)in;
	(void)inlen;
	(void)out;
	(void)outlen;
	errno = ENOTSUP;
	return errno;
#endif
}

static int
mlx5_glue_devx_wq_query(struct ibv_wq *wq, const void *in, size_t inlen,
			void *out, size_t outlen)
{
#ifdef HAVE_IBV_DEVX_QP
	return mlx5dv_devx_wq_query(wq, in, inlen, out, outlen);
#else
	(void)wq;
	(void)in;
	(void)inlen;
	(void)out;
	(void)outlen;
	errno = ENOTSUP;
	return errno;
#endif
}

static int
mlx5_glue_devx_port_query(struct ibv_context *ctx,
			  uint32_t port_num,
			  struct mlx5_port_info *info)
{
	int err = 0;

	info->query_flags = 0;
#ifdef HAVE_MLX5DV_DR_DEVX_PORT_V35
	/* The DevX port query API is implemented (rdma-core v35 and above). */
	struct mlx5_ib_uapi_query_port devx_port;

	memset(&devx_port, 0, sizeof(devx_port));
	err = mlx5dv_query_port(ctx, port_num, &devx_port);
	if (err)
		return err;
	if (devx_port.flags & MLX5DV_QUERY_PORT_VPORT_REG_C0) {
		info->vport_meta_tag = devx_port.reg_c0.value;
		info->vport_meta_mask = devx_port.reg_c0.mask;
		info->query_flags |= MLX5_PORT_QUERY_REG_C0;
	}
	if (devx_port.flags & MLX5DV_QUERY_PORT_VPORT) {
		info->vport_id = devx_port.vport;
		info->query_flags |= MLX5_PORT_QUERY_VPORT;
	}
	if (devx_port.flags & MLX5DV_QUERY_PORT_ESW_OWNER_VHCA_ID) {
		info->esw_owner_vhca_id = devx_port.esw_owner_vhca_id;
		info->query_flags |= MLX5_PORT_QUERY_ESW_OWNER_VHCA_ID;
	}
#else
#ifdef HAVE_MLX5DV_DR_DEVX_PORT
	/* The legacy DevX port query API is implemented (prior v35). */
	struct mlx5dv_devx_port devx_port = {
		.comp_mask = MLX5DV_DEVX_PORT_VPORT |
			     MLX5DV_DEVX_PORT_MATCH_REG_C_0 |
			     MLX5DV_DEVX_PORT_VPORT_VHCA_ID |
			     MLX5DV_DEVX_PORT_ESW_OWNER_VHCA_ID
	};

	err = mlx5dv_query_devx_port(ctx, port_num, &devx_port);
	if (err)
		return err;
	if (devx_port.comp_mask & MLX5DV_DEVX_PORT_MATCH_REG_C_0) {
		info->vport_meta_tag = devx_port.reg_c_0.value;
		info->vport_meta_mask = devx_port.reg_c_0.mask;
		info->query_flags |= MLX5_PORT_QUERY_REG_C0;
	}
	if (devx_port.comp_mask & MLX5DV_DEVX_PORT_VPORT) {
		info->vport_id = devx_port.vport_num;
		info->query_flags |= MLX5_PORT_QUERY_VPORT;
	}
#else
	RTE_SET_USED(ctx);
	RTE_SET_USED(port_num);
#endif /* HAVE_MLX5DV_DR_DEVX_PORT */
#endif /* HAVE_MLX5DV_DR_DEVX_PORT_V35 */
	return err;
}

static int
mlx5_glue_dr_dump_single_rule(FILE *file, void *rule)
{
#ifdef HAVE_MLX5_DR_FLOW_DUMP_RULE
	return mlx5dv_dump_dr_rule(file, rule);
#else
	RTE_SET_USED(file);
	RTE_SET_USED(rule);
	return -ENOTSUP;
#endif
}

static int
mlx5_glue_dr_dump_domain(FILE *file, void *domain)
{
#ifdef HAVE_MLX5_DR_FLOW_DUMP
	return mlx5dv_dump_dr_domain(file, domain);
#else
	RTE_SET_USED(file);
	RTE_SET_USED(domain);
	return -ENOTSUP;
#endif
}

static void *
mlx5_glue_dr_create_flow_action_sampler
			(struct mlx5dv_dr_flow_sampler_attr *attr)
{
#ifdef HAVE_MLX5_DR_CREATE_ACTION_FLOW_SAMPLE
	return mlx5dv_dr_action_create_flow_sampler(attr);
#else
	(void)attr;
	errno = ENOTSUP;
	return NULL;
#endif
}

static void *
mlx5_glue_dr_action_create_dest_array
			(void *domain,
			 size_t num_dest,
			 struct mlx5dv_dr_action_dest_attr *dests[])
{
#ifdef HAVE_MLX5_DR_CREATE_ACTION_DEST_ARRAY
	return mlx5dv_dr_action_create_dest_array
				(domain,
				num_dest,
				dests);
#else
	(void)domain;
	(void)num_dest;
	(void)dests;
	errno = ENOTSUP;
	return NULL;
#endif
}

static int
mlx5_glue_devx_query_eqn(struct ibv_context *ctx, uint32_t cpus,
			 uint32_t *eqn)
{
#ifdef HAVE_IBV_DEVX_OBJ
	return mlx5dv_devx_query_eqn(ctx, cpus, eqn);
#else
	(void)ctx;
	(void)cpus;
	(void)eqn;
	return -ENOTSUP;
#endif
}

static struct mlx5dv_devx_event_channel *
mlx5_glue_devx_create_event_channel(struct ibv_context *ctx, int flags)
{
#ifdef HAVE_IBV_DEVX_EVENT
	return mlx5dv_devx_create_event_channel(ctx, flags);
#else
	(void)ctx;
	(void)flags;
	errno = ENOTSUP;
	return NULL;
#endif
}

static void
mlx5_glue_devx_destroy_event_channel(struct mlx5dv_devx_event_channel *eventc)
{
#ifdef HAVE_IBV_DEVX_EVENT
	mlx5dv_devx_destroy_event_channel(eventc);
#else
	(void)eventc;
#endif
}

static int
mlx5_glue_devx_subscribe_devx_event(struct mlx5dv_devx_event_channel *eventc,
				    struct mlx5dv_devx_obj *obj,
				    uint16_t events_sz, uint16_t events_num[],
				    uint64_t cookie)
{
#ifdef HAVE_IBV_DEVX_EVENT
	return mlx5dv_devx_subscribe_devx_event(eventc, obj, events_sz,
						events_num, cookie);
#else
	(void)eventc;
	(void)obj;
	(void)events_sz;
	(void)events_num;
	(void)cookie;
	return -ENOTSUP;
#endif
}

static int
mlx5_glue_devx_subscribe_devx_event_fd(struct mlx5dv_devx_event_channel *eventc,
				       int fd, struct mlx5dv_devx_obj *obj,
				       uint16_t event_num)
{
#ifdef HAVE_IBV_DEVX_EVENT
	return mlx5dv_devx_subscribe_devx_event_fd(eventc, fd, obj, event_num);
#else
	(void)eventc;
	(void)fd;
	(void)obj;
	(void)event_num;
	return -ENOTSUP;
#endif
}

static ssize_t
mlx5_glue_devx_get_event(struct mlx5dv_devx_event_channel *eventc,
			 struct mlx5dv_devx_async_event_hdr *event_data,
			 size_t event_resp_len)
{
#ifdef HAVE_IBV_DEVX_EVENT
	return mlx5dv_devx_get_event(eventc, event_data, event_resp_len);
#else
	(void)eventc;
	(void)event_data;
	(void)event_resp_len;
	errno = ENOTSUP;
	return -1;
#endif
}

static struct mlx5dv_devx_uar *
mlx5_glue_devx_alloc_uar(struct ibv_context *context, uint32_t flags)
{
#ifdef HAVE_IBV_DEVX_OBJ
	return mlx5dv_devx_alloc_uar(context, flags);
#else
	(void)context;
	(void)flags;
	errno = ENOTSUP;
	return NULL;
#endif
}

static void
mlx5_glue_devx_free_uar(struct mlx5dv_devx_uar *devx_uar)
{
#ifdef HAVE_IBV_DEVX_OBJ
	mlx5dv_devx_free_uar(devx_uar);
#else
	(void)devx_uar;
#endif
}

static struct mlx5dv_var *
mlx5_glue_dv_alloc_var(struct ibv_context *context, uint32_t flags)
{
#ifdef HAVE_IBV_VAR
	return mlx5dv_alloc_var(context, flags);
#else
	(void)context;
	(void)flags;
	errno = ENOTSUP;
	return NULL;
#endif
}

static void
mlx5_glue_dv_free_var(struct mlx5dv_var *var)
{
#ifdef HAVE_IBV_VAR
	mlx5dv_free_var(var);
#else
	(void)var;
	errno = ENOTSUP;
#endif
}

static void
mlx5_glue_dr_reclaim_domain_memory(void *domain, uint32_t enable)
{
#ifdef HAVE_MLX5DV_DR_MEM_RECLAIM
	mlx5dv_dr_domain_set_reclaim_device_memory(domain, enable);
#else
	(void)(enable);
	(void)(domain);
#endif
}

static struct mlx5dv_pp *
mlx5_glue_dv_alloc_pp(struct ibv_context *context,
		      size_t pp_context_sz,
		      const void *pp_context,
		      uint32_t flags)
{
#ifdef HAVE_MLX5DV_PP_ALLOC
	return mlx5dv_pp_alloc(context, pp_context_sz, pp_context, flags);
#else
	RTE_SET_USED(context);
	RTE_SET_USED(pp_context_sz);
	RTE_SET_USED(pp_context);
	RTE_SET_USED(flags);
	errno = ENOTSUP;
	return NULL;
#endif
}

static void
mlx5_glue_dr_allow_duplicate_rules(void *domain, uint32_t allow)
{
#ifdef HAVE_MLX5_DR_ALLOW_DUPLICATE
	mlx5dv_dr_domain_allow_duplicate_rules(domain, allow);
#else
	(void)(allow);
	(void)(domain);
#endif
}

static void
mlx5_glue_dv_free_pp(struct mlx5dv_pp *pp)
{
#ifdef HAVE_MLX5DV_PP_ALLOC
	mlx5dv_pp_free(pp);
#else
	RTE_SET_USED(pp);
#endif
}

static void *
mlx5_glue_dr_create_flow_action_send_to_kernel(void *tbl, uint16_t priority)
{
#ifdef HAVE_MLX5DV_DR_ACTION_CREATE_DEST_ROOT_TABLE
	struct mlx5dv_dr_table *table = (struct mlx5dv_dr_table *)tbl;

	return mlx5dv_dr_action_create_dest_root_table(table, priority);
#else
	RTE_SET_USED(tbl);
	RTE_SET_USED(priority);
	errno = ENOTSUP;
	return NULL;
#endif
}

static struct mlx5dv_steering_anchor *
mlx5_glue_dv_create_steering_anchor(struct ibv_context *context,
				    struct mlx5dv_steering_anchor_attr *attr)
{
#ifdef HAVE_MLX5DV_CREATE_STEERING_ANCHOR
	return mlx5dv_create_steering_anchor(context, attr);
#else
	(void)context;
	(void)attr;
	errno = ENOTSUP;
	return NULL;
#endif
}

static int
mlx5_glue_dv_destroy_steering_anchor(struct mlx5dv_steering_anchor *sa)
{
#ifdef HAVE_MLX5DV_CREATE_STEERING_ANCHOR
	return mlx5dv_destroy_steering_anchor(sa);
#else
	(void)sa;
	errno = ENOTSUP;
	return -ENOTSUP;
#endif
}

__rte_cache_aligned
const struct mlx5_glue *mlx5_glue = &(const struct mlx5_glue) {
	.version = MLX5_GLUE_VERSION,
	.fork_init = mlx5_glue_fork_init,
	.alloc_pd = mlx5_glue_alloc_pd,
	.dealloc_pd = mlx5_glue_dealloc_pd,
	.import_pd = mlx5_glue_import_pd,
	.unimport_pd = mlx5_glue_unimport_pd,
	.get_device_list = mlx5_glue_get_device_list,
	.free_device_list = mlx5_glue_free_device_list,
	.open_device = mlx5_glue_open_device,
	.import_device = mlx5_glue_import_device,
	.close_device = mlx5_glue_close_device,
	.query_device = mlx5_glue_query_device,
	.query_device_ex = mlx5_glue_query_device_ex,
	.get_device_name = mlx5_glue_get_device_name,
	.query_rt_values_ex = mlx5_glue_query_rt_values_ex,
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
	.reg_mr_iova = mlx5_glue_reg_mr_iova,
	.alloc_null_mr = mlx5_glue_alloc_null_mr,
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
	.dr_create_flow_action_dest_port =
		mlx5_glue_dr_create_flow_action_dest_port,
	.dr_create_flow_action_drop =
		mlx5_glue_dr_create_flow_action_drop,
	.dr_create_flow_action_push_vlan =
		mlx5_glue_dr_create_flow_action_push_vlan,
	.dr_create_flow_action_pop_vlan =
		mlx5_glue_dr_create_flow_action_pop_vlan,
	.dr_create_flow_tbl = mlx5_glue_dr_create_flow_tbl,
	.dr_destroy_flow_tbl = mlx5_glue_dr_destroy_flow_tbl,
	.dr_create_domain = mlx5_glue_dr_create_domain,
	.dr_destroy_domain = mlx5_glue_dr_destroy_domain,
	.dr_sync_domain = mlx5_glue_dr_sync_domain,
	.dv_create_cq = mlx5_glue_dv_create_cq,
	.dv_create_wq = mlx5_glue_dv_create_wq,
	.dv_query_device = mlx5_glue_dv_query_device,
	.dv_set_context_attr = mlx5_glue_dv_set_context_attr,
	.dv_init_obj = mlx5_glue_dv_init_obj,
	.dv_create_qp = mlx5_glue_dv_create_qp,
	.dv_create_flow_matcher = mlx5_glue_dv_create_flow_matcher,
	.dv_create_flow_matcher_root = __mlx5_glue_dv_create_flow_matcher,
	.dv_create_flow = mlx5_glue_dv_create_flow,
	.dv_create_flow_root = __mlx5_glue_dv_create_flow,
	.dv_create_flow_action_counter =
		mlx5_glue_dv_create_flow_action_counter,
	.dv_create_flow_action_dest_ibv_qp =
		mlx5_glue_dv_create_flow_action_dest_ibv_qp,
	.dv_create_flow_action_dest_devx_tir =
		mlx5_glue_dv_create_flow_action_dest_devx_tir,
	.dv_create_flow_action_modify_header =
		mlx5_glue_dv_create_flow_action_modify_header,
	.dv_create_flow_action_modify_header_root =
		__mlx5_glue_dv_create_flow_action_modify_header,
	.dv_create_flow_action_packet_reformat =
		mlx5_glue_dv_create_flow_action_packet_reformat,
	.dv_create_flow_action_packet_reformat_root =
		__mlx5_glue_dv_create_flow_action_packet_reformat,
	.dv_create_flow_action_tag =  mlx5_glue_dv_create_flow_action_tag,
	.dv_create_flow_action_meter = mlx5_glue_dv_create_flow_action_meter,
	.dv_modify_flow_action_meter = mlx5_glue_dv_modify_flow_action_meter,
	.dv_create_flow_action_aso = mlx5_glue_dv_create_flow_action_aso,
	.dr_create_flow_action_default_miss =
		mlx5_glue_dr_create_flow_action_default_miss,
	.dv_destroy_flow = mlx5_glue_dv_destroy_flow,
	.dv_destroy_flow_matcher = mlx5_glue_dv_destroy_flow_matcher,
	.dv_destroy_flow_matcher_root = __mlx5_glue_dv_destroy_flow_matcher,
	.dv_open_device = mlx5_glue_dv_open_device,
	.devx_obj_create = mlx5_glue_devx_obj_create,
	.devx_obj_destroy = mlx5_glue_devx_obj_destroy,
	.devx_obj_query = mlx5_glue_devx_obj_query,
	.devx_obj_modify = mlx5_glue_devx_obj_modify,
	.devx_general_cmd = mlx5_glue_devx_general_cmd,
	.devx_create_cmd_comp = mlx5_glue_devx_create_cmd_comp,
	.devx_destroy_cmd_comp = mlx5_glue_devx_destroy_cmd_comp,
	.devx_obj_query_async = mlx5_glue_devx_obj_query_async,
	.devx_get_async_cmd_comp = mlx5_glue_devx_get_async_cmd_comp,
	.devx_umem_reg = mlx5_glue_devx_umem_reg,
	.devx_umem_dereg = mlx5_glue_devx_umem_dereg,
	.devx_qp_query = mlx5_glue_devx_qp_query,
	.devx_wq_query = mlx5_glue_devx_wq_query,
	.devx_port_query = mlx5_glue_devx_port_query,
	.dr_dump_domain = mlx5_glue_dr_dump_domain,
	.dr_dump_rule = mlx5_glue_dr_dump_single_rule,
	.dr_reclaim_domain_memory = mlx5_glue_dr_reclaim_domain_memory,
	.dr_create_flow_action_sampler =
		mlx5_glue_dr_create_flow_action_sampler,
	.dr_create_flow_action_dest_array =
		mlx5_glue_dr_action_create_dest_array,
	.dr_allow_duplicate_rules = mlx5_glue_dr_allow_duplicate_rules,
	.devx_query_eqn = mlx5_glue_devx_query_eqn,
	.devx_create_event_channel = mlx5_glue_devx_create_event_channel,
	.devx_destroy_event_channel = mlx5_glue_devx_destroy_event_channel,
	.devx_subscribe_devx_event = mlx5_glue_devx_subscribe_devx_event,
	.devx_subscribe_devx_event_fd = mlx5_glue_devx_subscribe_devx_event_fd,
	.devx_get_event = mlx5_glue_devx_get_event,
	.devx_alloc_uar = mlx5_glue_devx_alloc_uar,
	.devx_free_uar = mlx5_glue_devx_free_uar,
	.dv_alloc_var = mlx5_glue_dv_alloc_var,
	.dv_free_var = mlx5_glue_dv_free_var,
	.dv_alloc_pp = mlx5_glue_dv_alloc_pp,
	.dv_free_pp = mlx5_glue_dv_free_pp,
	.dr_create_flow_action_send_to_kernel =
		mlx5_glue_dr_create_flow_action_send_to_kernel,
	.create_steering_anchor = mlx5_glue_dv_create_steering_anchor,
	.destroy_steering_anchor = mlx5_glue_dv_destroy_steering_anchor,
};
