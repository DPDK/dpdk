/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 6WIND S.A.
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#ifndef MLX5_GLUE_H_
#define MLX5_GLUE_H_

#include <stddef.h>
#include <stdint.h>
/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/mlx5dv.h>
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_byteorder.h>

#include "mlx5_autoconf.h"

#ifndef MLX5_GLUE_VERSION
#define MLX5_GLUE_VERSION ""
#endif

#ifndef HAVE_IBV_DEVICE_COUNTERS_SET_V42
struct ibv_counter_set;
struct ibv_counter_set_data;
struct ibv_counter_set_description;
struct ibv_counter_set_init_attr;
struct ibv_query_counter_set_attr;
#endif

#ifndef HAVE_IBV_DEVICE_COUNTERS_SET_V45
struct ibv_counters;
struct ibv_counters_init_attr;
struct ibv_counter_attach_attr;
#endif

#ifndef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
struct mlx5dv_qp_init_attr;
#endif

#ifndef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
struct mlx5dv_wq_init_attr;
#endif

#ifndef HAVE_IBV_FLOW_DV_SUPPORT
struct mlx5dv_flow_matcher;
struct mlx5dv_flow_matcher_attr;
struct mlx5dv_flow_action_attr;
struct mlx5dv_flow_match_parameters;
struct mlx5dv_dr_flow_meter_attr;
struct ibv_flow_action;
enum mlx5dv_flow_action_packet_reformat_type { packet_reformat_type = 0, };
enum mlx5dv_flow_table_type { flow_table_type = 0, };
#endif

#ifndef HAVE_IBV_FLOW_DEVX_COUNTERS
#define MLX5DV_FLOW_ACTION_COUNTERS_DEVX 0
#endif

#ifndef HAVE_IBV_DEVX_OBJ
struct mlx5dv_devx_obj;
struct mlx5dv_devx_umem { uint32_t umem_id; };
struct mlx5dv_devx_uar { void *reg_addr; void *base_addr; uint32_t page_id; };
#endif

#ifndef HAVE_IBV_DEVX_ASYNC
struct mlx5dv_devx_cmd_comp;
struct mlx5dv_devx_async_cmd_hdr;
#endif

#ifndef HAVE_MLX5DV_DR
enum  mlx5dv_dr_domain_type { unused, };
struct mlx5dv_dr_domain;
struct mlx5dv_dr_action;
#define MLX5DV_DR_ACTION_FLAGS_ROOT_LEVEL 1
#endif

#ifndef HAVE_MLX5DV_DR_DEVX_PORT
struct mlx5dv_devx_port;
#endif

#ifndef HAVE_MLX5DV_DR_DEVX_PORT_V35
struct mlx5dv_port;
#endif

#define MLX5_PORT_QUERY_VPORT (1u << 0)
#define MLX5_PORT_QUERY_REG_C0 (1u << 1)

struct mlx5_port_info {
	uint16_t query_flags;
	uint16_t vport_id; /* Associated VF vport index (if any). */
	uint32_t vport_meta_tag; /* Used for vport index match ove VF LAG. */
	uint32_t vport_meta_mask; /* Used for vport index field match mask. */
};

#ifndef HAVE_MLX5_DR_CREATE_ACTION_FLOW_METER
struct mlx5dv_dr_flow_meter_attr;
#endif

#ifndef HAVE_MLX5_DR_CREATE_ACTION_FLOW_SAMPLE
struct mlx5dv_dr_flow_sampler_attr {
	uint32_t sample_ratio;
	void *default_next_table;
	size_t num_sample_actions;
	struct mlx5dv_dr_action **sample_actions;
	uint64_t action;
};
#endif

#ifndef HAVE_MLX5_DR_CREATE_ACTION_DEST_ARRAY
enum mlx5dv_dr_action_dest_type {
	MLX5DV_DR_ACTION_DEST,
	MLX5DV_DR_ACTION_DEST_REFORMAT,
};
struct mlx5dv_dr_action_dest_reformat {
	struct mlx5dv_dr_action *reformat;
	struct mlx5dv_dr_action *dest;
};
struct mlx5dv_dr_action_dest_attr {
	enum mlx5dv_dr_action_dest_type type;
	union {
		struct mlx5dv_dr_action *dest;
		struct mlx5dv_dr_action_dest_reformat *dest_reformat;
	};
};
#endif

#ifndef HAVE_IBV_DEVX_EVENT
struct mlx5dv_devx_event_channel { int fd; };
struct mlx5dv_devx_async_event_hdr;
#define MLX5DV_DEVX_CREATE_EVENT_CHANNEL_FLAGS_OMIT_EV_DATA 1
#endif

#ifndef HAVE_IBV_VAR
struct mlx5dv_var { uint32_t page_id; uint32_t length; off_t mmap_off;
			uint64_t comp_mask; };
#endif

#ifndef HAVE_IBV_RELAXED_ORDERING
#define IBV_ACCESS_RELAXED_ORDERING 0
#endif

struct mlx5_glue {
	const char *version;
	int (*fork_init)(void);
	struct ibv_pd *(*alloc_pd)(struct ibv_context *context);
	int (*dealloc_pd)(struct ibv_pd *pd);
	struct ibv_device **(*get_device_list)(int *num_devices);
	void (*free_device_list)(struct ibv_device **list);
	struct ibv_context *(*open_device)(struct ibv_device *device);
	int (*close_device)(struct ibv_context *context);
	int (*query_device)(struct ibv_context *context,
			    struct ibv_device_attr *device_attr);
	int (*query_device_ex)(struct ibv_context *context,
			       const struct ibv_query_device_ex_input *input,
			       struct ibv_device_attr_ex *attr);
	int (*query_rt_values_ex)(struct ibv_context *context,
			       struct ibv_values_ex *values);
	int (*query_port)(struct ibv_context *context, uint8_t port_num,
			  struct ibv_port_attr *port_attr);
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
	struct ibv_rwq_ind_table *(*create_rwq_ind_table)
		(struct ibv_context *context,
		 struct ibv_rwq_ind_table_init_attr *init_attr);
	int (*destroy_rwq_ind_table)(struct ibv_rwq_ind_table *rwq_ind_table);
	struct ibv_wq *(*create_wq)(struct ibv_context *context,
				    struct ibv_wq_init_attr *wq_init_attr);
	int (*destroy_wq)(struct ibv_wq *wq);
	int (*modify_wq)(struct ibv_wq *wq, struct ibv_wq_attr *wq_attr);
	struct ibv_flow *(*create_flow)(struct ibv_qp *qp,
					struct ibv_flow_attr *flow);
	int (*destroy_flow)(struct ibv_flow *flow_id);
	int (*destroy_flow_action)(void *action);
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
	struct ibv_mr *(*reg_mr_iova)(struct ibv_pd *pd, void *addr,
				      size_t length, uint64_t iova,
				      int access);
	struct ibv_mr *(*alloc_null_mr)(struct ibv_pd *pd);
	int (*dereg_mr)(struct ibv_mr *mr);
	struct ibv_counter_set *(*create_counter_set)
		(struct ibv_context *context,
		 struct ibv_counter_set_init_attr *init_attr);
	int (*destroy_counter_set)(struct ibv_counter_set *cs);
	int (*describe_counter_set)
		(struct ibv_context *context,
		 uint16_t counter_set_id,
		 struct ibv_counter_set_description *cs_desc);
	int (*query_counter_set)(struct ibv_query_counter_set_attr *query_attr,
				 struct ibv_counter_set_data *cs_data);
	struct ibv_counters *(*create_counters)
		(struct ibv_context *context,
		 struct ibv_counters_init_attr *init_attr);
	int (*destroy_counters)(struct ibv_counters *counters);
	int (*attach_counters)(struct ibv_counters *counters,
			       struct ibv_counter_attach_attr *attr,
			       struct ibv_flow *flow);
	int (*query_counters)(struct ibv_counters *counters,
			      uint64_t *counters_value,
			      uint32_t ncounters,
			      uint32_t flags);
	void (*ack_async_event)(struct ibv_async_event *event);
	int (*get_async_event)(struct ibv_context *context,
			       struct ibv_async_event *event);
	const char *(*port_state_str)(enum ibv_port_state port_state);
	struct ibv_cq *(*cq_ex_to_cq)(struct ibv_cq_ex *cq);
	void *(*dr_create_flow_action_dest_flow_tbl)(void *tbl);
	void *(*dr_create_flow_action_dest_port)(void *domain,
						 uint32_t port);
	void *(*dr_create_flow_action_drop)(void);
	void *(*dr_create_flow_action_push_vlan)
					(struct mlx5dv_dr_domain *domain,
					 rte_be32_t vlan_tag);
	void *(*dr_create_flow_action_pop_vlan)(void);
	void *(*dr_create_flow_tbl)(void *domain, uint32_t level);
	int (*dr_destroy_flow_tbl)(void *tbl);
	void *(*dr_create_domain)(struct ibv_context *ctx,
				  enum mlx5dv_dr_domain_type domain);
	int (*dr_destroy_domain)(void *domain);
	int (*dr_sync_domain)(void *domain, uint32_t flags);
	struct ibv_cq_ex *(*dv_create_cq)
		(struct ibv_context *context,
		 struct ibv_cq_init_attr_ex *cq_attr,
		 struct mlx5dv_cq_init_attr *mlx5_cq_attr);
	struct ibv_wq *(*dv_create_wq)
		(struct ibv_context *context,
		 struct ibv_wq_init_attr *wq_attr,
		 struct mlx5dv_wq_init_attr *mlx5_wq_attr);
	int (*dv_query_device)(struct ibv_context *ctx_in,
			       struct mlx5dv_context *attrs_out);
	int (*dv_set_context_attr)(struct ibv_context *ibv_ctx,
				   enum mlx5dv_set_ctx_attr_type type,
				   void *attr);
	int (*dv_init_obj)(struct mlx5dv_obj *obj, uint64_t obj_type);
	struct ibv_qp *(*dv_create_qp)
		(struct ibv_context *context,
		 struct ibv_qp_init_attr_ex *qp_init_attr_ex,
		 struct mlx5dv_qp_init_attr *dv_qp_init_attr);
	void *(*dv_create_flow_matcher)
		(struct ibv_context *context,
		 struct mlx5dv_flow_matcher_attr *matcher_attr,
		 void *tbl);
	void *(*dv_create_flow)(void *matcher, void *match_value,
			  size_t num_actions, void *actions[]);
	void *(*dv_create_flow_action_counter)(void *obj, uint32_t  offset);
	void *(*dv_create_flow_action_dest_ibv_qp)(void *qp);
	void *(*dv_create_flow_action_dest_devx_tir)(void *tir);
	void *(*dv_create_flow_action_modify_header)
		(struct ibv_context *ctx, enum mlx5dv_flow_table_type ft_type,
		 void *domain, uint64_t flags, size_t actions_sz,
		 uint64_t actions[]);
	void *(*dv_create_flow_action_packet_reformat)
		(struct ibv_context *ctx,
		 enum mlx5dv_flow_action_packet_reformat_type reformat_type,
		 enum mlx5dv_flow_table_type ft_type,
		 struct mlx5dv_dr_domain *domain,
		 uint32_t flags, size_t data_sz, void *data);
	void *(*dv_create_flow_action_tag)(uint32_t tag);
	void *(*dv_create_flow_action_meter)
		(struct mlx5dv_dr_flow_meter_attr *attr);
	int (*dv_modify_flow_action_meter)(void *action,
		struct mlx5dv_dr_flow_meter_attr *attr, uint64_t modify_bits);
	void *(*dr_create_flow_action_default_miss)(void);
	int (*dv_destroy_flow)(void *flow);
	int (*dv_destroy_flow_matcher)(void *matcher);
	struct ibv_context *(*dv_open_device)(struct ibv_device *device);
	struct mlx5dv_var *(*dv_alloc_var)(struct ibv_context *context,
					   uint32_t flags);
	void (*dv_free_var)(struct mlx5dv_var *var);
	struct mlx5dv_devx_uar *(*devx_alloc_uar)(struct ibv_context *context,
						  uint32_t flags);
	void (*devx_free_uar)(struct mlx5dv_devx_uar *devx_uar);
	struct mlx5dv_devx_obj *(*devx_obj_create)
					(struct ibv_context *ctx,
					 const void *in, size_t inlen,
					 void *out, size_t outlen);
	int (*devx_obj_destroy)(struct mlx5dv_devx_obj *obj);
	int (*devx_obj_query)(struct mlx5dv_devx_obj *obj,
			      const void *in, size_t inlen,
			      void *out, size_t outlen);
	int (*devx_obj_modify)(struct mlx5dv_devx_obj *obj,
			       const void *in, size_t inlen,
			       void *out, size_t outlen);
	int (*devx_general_cmd)(struct ibv_context *context,
				const void *in, size_t inlen,
				void *out, size_t outlen);
	struct mlx5dv_devx_cmd_comp *(*devx_create_cmd_comp)
					(struct ibv_context *context);
	void (*devx_destroy_cmd_comp)(struct mlx5dv_devx_cmd_comp *cmd_comp);
	int (*devx_obj_query_async)(struct mlx5dv_devx_obj *obj,
				    const void *in, size_t inlen,
				    size_t outlen, uint64_t wr_id,
				    struct mlx5dv_devx_cmd_comp *cmd_comp);
	int (*devx_get_async_cmd_comp)(struct mlx5dv_devx_cmd_comp *cmd_comp,
				       struct mlx5dv_devx_async_cmd_hdr *resp,
				       size_t cmd_resp_len);
	struct mlx5dv_devx_umem *(*devx_umem_reg)(struct ibv_context *context,
						  void *addr, size_t size,
						  uint32_t access);
	int (*devx_umem_dereg)(struct mlx5dv_devx_umem *dv_devx_umem);
	int (*devx_qp_query)(struct ibv_qp *qp,
			     const void *in, size_t inlen,
			     void *out, size_t outlen);
	int (*devx_wq_query)(struct ibv_wq *wq, const void *in, size_t inlen,
			     void *out, size_t outlen);
	int (*devx_port_query)(struct ibv_context *ctx,
			       uint32_t port_num,
			       struct mlx5_port_info *info);
	int (*dr_dump_domain)(FILE *file, void *domain);
	int (*dr_dump_rule)(FILE *file, void *rule);
	int (*devx_query_eqn)(struct ibv_context *context, uint32_t cpus,
			      uint32_t *eqn);
	struct mlx5dv_devx_event_channel *(*devx_create_event_channel)
				(struct ibv_context *context, int flags);
	void (*devx_destroy_event_channel)
			(struct mlx5dv_devx_event_channel *event_channel);
	int (*devx_subscribe_devx_event)
			(struct mlx5dv_devx_event_channel *event_channel,
			 struct mlx5dv_devx_obj *obj,
			 uint16_t events_sz,
			 uint16_t events_num[],
			 uint64_t cookie);
	int (*devx_subscribe_devx_event_fd)
			(struct mlx5dv_devx_event_channel *event_channel,
			 int fd,
			 struct mlx5dv_devx_obj *obj,
			 uint16_t event_num);
	ssize_t (*devx_get_event)
			(struct mlx5dv_devx_event_channel *event_channel,
			 struct mlx5dv_devx_async_event_hdr *event_data,
			 size_t event_resp_len);
	void (*dr_reclaim_domain_memory)(void *domain, uint32_t enable);
	void (*dr_allow_duplicate_rules)(void *domain, uint32_t allow);
	struct mlx5dv_pp *(*dv_alloc_pp)(struct ibv_context *context,
					 size_t pp_context_sz,
					 const void *pp_context,
					 uint32_t flags);
	void (*dv_free_pp)(struct mlx5dv_pp *pp);
	void *(*dr_create_flow_action_sampler)
			(struct mlx5dv_dr_flow_sampler_attr *attr);
	void *(*dr_create_flow_action_dest_array)
			(void *domain,
			 size_t num_dest,
			 struct mlx5dv_dr_action_dest_attr *dests[]);
	void *(*dv_create_flow_action_aso)
			(struct mlx5dv_dr_domain *domain, void *aso_obj,
			 uint32_t offset, uint32_t flags, uint8_t return_reg_c);
};

extern const struct mlx5_glue *mlx5_glue;

#endif /* MLX5_GLUE_H_ */
