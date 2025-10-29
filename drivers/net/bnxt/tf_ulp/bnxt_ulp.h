/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_ULP_H_
#define _BNXT_ULP_H_

#include <inttypes.h>
#include <stdbool.h>
#include <sys/queue.h>

#include "rte_version.h"
#include "rte_ethdev.h"
#include "rte_mtr.h"

#include "bnxt.h"
#include "ulp_template_db_enum.h"
#include "ulp_tun.h"
#include "bnxt_tf_common.h"

/* NAT defines to reuse existing inner L2 SMAC and DMAC */
#define BNXT_ULP_NAT_INNER_L2_HEADER_SMAC	0x2000
#define BNXT_ULP_NAT_OUTER_MOST_L2_HDR_SMAC	0x6000
#define BNXT_ULP_NAT_OUTER_MOST_L2_VLAN_TAGS	0xc00
#define BNXT_ULP_NAT_INNER_L2_HEADER_DMAC	0x100
#define BNXT_ULP_NAT_OUTER_MOST_L2_HDR_DMAC	0x300
#define BNXT_ULP_NAT_OUTER_MOST_FLAGS (BNXT_ULP_NAT_OUTER_MOST_L2_HDR_SMAC |\
					BNXT_ULP_NAT_OUTER_MOST_L2_VLAN_TAGS |\
					BNXT_ULP_NAT_OUTER_MOST_L2_HDR_DMAC)

/* defines for the ulp_flags */
#define BNXT_ULP_VF_REP_ENABLED		0x1
#define BNXT_ULP_SHARED_SESSION_ENABLED	0x2
#define BNXT_ULP_APP_DEV_UNSUPPORTED	0x4
#define BNXT_ULP_HIGH_AVAIL_ENABLED	0x8
#define BNXT_ULP_APP_UNICAST_ONLY	0x10
#define BNXT_ULP_APP_SOCKET_DIRECT	0x20
#define BNXT_ULP_APP_TOS_PROTO_SUPPORT	0x40
#define BNXT_ULP_APP_BC_MC_SUPPORT	0x80
#define BNXT_ULP_STATIC_VXLAN_SUPPORT	0x100
#define BNXT_ULP_MULTI_SHARED_SUPPORT	0x200
#define BNXT_ULP_APP_HA_DYNAMIC		0x400
#define BNXT_ULP_APP_SRV6               0x800
#define BNXT_ULP_APP_L2_ETYPE		0x1000
#define BNXT_ULP_SHARED_TBL_SCOPE_ENABLED 0x2000
#define BNXT_ULP_DYNAMIC_VXLAN_PORT	0x4000
#define BNXT_ULP_DYNAMIC_GENEVE_PORT	0x8000

#define ULP_VF_REP_IS_ENABLED(flag)	((flag) & BNXT_ULP_VF_REP_ENABLED)
#define ULP_SHARED_SESSION_IS_ENABLED(flag) ((flag) &\
					     BNXT_ULP_SHARED_SESSION_ENABLED)
#define ULP_APP_DEV_UNSUPPORTED_ENABLED(flag)	((flag) &\
						 BNXT_ULP_APP_DEV_UNSUPPORTED)
#define ULP_HIGH_AVAIL_IS_ENABLED(flag)	((flag) & BNXT_ULP_HIGH_AVAIL_ENABLED)
#define ULP_SOCKET_DIRECT_IS_ENABLED(flag) ((flag) & BNXT_ULP_APP_SOCKET_DIRECT)
#define ULP_APP_TOS_PROTO_SUPPORT(ctx)	((ctx)->cfg_data->ulp_flags &\
					BNXT_ULP_APP_TOS_PROTO_SUPPORT)
#define ULP_APP_BC_MC_SUPPORT(ctx)	((ctx)->cfg_data->ulp_flags &\
					BNXT_ULP_APP_BC_MC_SUPPORT)
#define ULP_MULTI_SHARED_IS_SUPPORTED(ctx)	((ctx)->cfg_data->ulp_flags &\
					BNXT_ULP_MULTI_SHARED_SUPPORT)
#define ULP_APP_HA_IS_DYNAMIC(ctx)	((ctx)->cfg_data->ulp_flags &\
					BNXT_ULP_APP_HA_DYNAMIC)

#define ULP_APP_L2_ETYPE_SUPPORT(ctx)	((ctx)->cfg_data->ulp_flags &\
					BNXT_ULP_APP_L2_ETYPE)

#define ULP_APP_STATIC_VXLAN_PORT_EN(ctx)	((ctx)->cfg_data->ulp_flags &\
					BNXT_ULP_STATIC_VXLAN_SUPPORT)
#define ULP_APP_DYNAMIC_VXLAN_PORT_EN(ctx)	((ctx)->cfg_data->ulp_flags &\
					BNXT_ULP_DYNAMIC_VXLAN_PORT)
#define ULP_APP_DYNAMIC_GENEVE_PORT_EN(ctx)	((ctx)->cfg_data->ulp_flags &\
					BNXT_ULP_DYNAMIC_GENEVE_PORT)

enum bnxt_ulp_flow_mem_type {
	BNXT_ULP_FLOW_MEM_TYPE_INT = 0,
	BNXT_ULP_FLOW_MEM_TYPE_EXT = 1,
	BNXT_ULP_FLOW_MEM_TYPE_BOTH = 2,
	BNXT_ULP_FLOW_MEM_TYPE_LAST = 3
};

enum bnxt_rte_flow_item_type {
	BNXT_RTE_FLOW_ITEM_TYPE_END = (uint32_t)INT_MIN,
	BNXT_RTE_FLOW_ITEM_TYPE_VXLAN_DECAP,
	BNXT_RTE_FLOW_ITEM_TYPE_LAST
};

enum bnxt_rte_flow_action_type {
	BNXT_RTE_FLOW_ACTION_TYPE_END = (uint32_t)INT_MIN,
	BNXT_RTE_FLOW_ACTION_TYPE_VXLAN_DECAP,
	BNXT_RTE_FLOW_ACTION_TYPE_LAST
};

#define BNXT_ULP_MAX_GROUP_CNT		8
struct bnxt_ulp_grp_rule_info {
	uint32_t			group_id;
	uint32_t			flow_id;
	uint8_t				dir;
	uint8_t				valid;
};

struct bnxt_ulp_df_rule_info {
	uint32_t			def_port_flow_id;
	uint32_t			promisc_flow_id;
	uint8_t				valid;
	struct bnxt_ulp_grp_rule_info	grp_df_rule[BNXT_ULP_MAX_GROUP_CNT];
};

struct bnxt_ulp_vfr_rule_info {
	uint32_t			vfr_flow_id;
	uint16_t			parent_port_id;
	uint8_t				valid;
};

struct bnxt_ulp_data {
	uint32_t			tbl_scope_id;
	struct bnxt_ulp_mark_tbl	*mark_tbl;
	uint32_t			dev_id; /* Hardware device id */
	uint32_t			ref_cnt;
	struct bnxt_ulp_flow_db		*flow_db;
	pthread_mutex_t			flow_db_lock;
	void				*mapper_data;
	void				*matcher_data;
	struct bnxt_ulp_port_db		*port_db;
	struct bnxt_ulp_fc_info		*fc_info;
	struct bnxt_ulp_ha_mgr_info	*ha_info;
	uint32_t			ulp_flags;
	struct bnxt_ulp_df_rule_info	df_rule_info[RTE_MAX_ETHPORTS];
	struct bnxt_ulp_vfr_rule_info	vfr_rule_info[RTE_MAX_ETHPORTS];
	enum bnxt_ulp_flow_mem_type	mem_type;
#define	BNXT_ULP_TUN_ENTRY_INVALID	-1
#define	BNXT_ULP_MAX_TUN_CACHE_ENTRIES	16
	struct bnxt_tun_cache_entry	tun_tbl[BNXT_ULP_MAX_TUN_CACHE_ENTRIES];
	uint8_t				app_id;
	uint8_t				num_shared_clients;
	struct bnxt_flow_app_tun_ent	app_tun[BNXT_ULP_MAX_TUN_CACHE_ENTRIES];
	uint32_t			default_priority;
	uint32_t			max_def_priority;
	uint32_t			min_flow_priority;
	uint32_t			max_flow_priority;
	uint32_t			ha_priority;
	uint32_t			vxlan_port;
	uint32_t			vxlan_gpe_port;
	uint32_t			vxlan_ip_port;
	uint32_t			ecpri_udp_port;
	uint32_t			hu_session_type;
	uint32_t			max_pools;
	uint32_t			num_rx_flows;
	uint32_t			num_tx_flows;
	uint16_t			act_rx_max_sz;
	uint16_t			act_tx_max_sz;
	uint16_t			em_rx_key_max_sz;
	uint16_t			em_tx_key_max_sz;
	uint32_t			page_sz;
	uint8_t				hu_reg_state;
	uint8_t				hu_reg_cnt;
	uint8_t				ha_pool_id;
	uint8_t				tunnel_next_proto;
	uint8_t				em_multiplier;
	enum bnxt_ulp_session_type	def_session_type;
	uint16_t			num_key_recipes_per_dir;
	uint64_t			feature_bits;
	uint64_t			default_class_bits;
	uint64_t			default_act_bits;
	struct ulp_fc_tfc_stats_cache_entry *stats_cache;
	struct bnxt_ulp_sc_info		*sc_info;
	struct bnxt_ulp_tfc_ha_mgr_info *tfc_ha_info;
	uint8_t				app_instance_id;
};

enum bnxt_ulp_tfo_type {
	BNXT_ULP_TFO_TYPE_INVALID = 0,
	BNXT_ULP_TFO_TYPE_TF,
	BNXT_ULP_TFO_TYPE_TFC
};

#define BNXT_ULP_SESSION_MAX 3
#define BNXT_ULP_TFO_SID_FLAG (1)
#define BNXT_ULP_TFO_TSID_FLAG (1 << 1)

struct bnxt_ulp_context {
	struct bnxt_ulp_data	*cfg_data;
	struct bnxt *bp;
	enum bnxt_ulp_tfo_type tfo_type;
	union {
		void *g_tfp[BNXT_ULP_SESSION_MAX];
		struct {
			uint32_t tfo_flags;
			void *tfcp;
			uint16_t sid;
			uint8_t tsid;
		};
	};
	const struct bnxt_ulp_core_ops *ops;
};

struct bnxt_ulp_pci_info {
	uint32_t	domain;
	uint8_t		bus;
};

#define BNXT_ULP_DEVICE_SERIAL_NUM_SIZE 8
struct bnxt_ulp_session_state {
	STAILQ_ENTRY(bnxt_ulp_session_state)	next;
	bool				bnxt_ulp_init;
	pthread_mutex_t			bnxt_ulp_mutex;
	struct bnxt_ulp_pci_info	pci_info;
	uint8_t				dsn[BNXT_ULP_DEVICE_SERIAL_NUM_SIZE];
	struct bnxt_ulp_data		*cfg_data;
	struct tf			*g_tfp[BNXT_ULP_SESSION_MAX];
	uint32_t			session_opened[BNXT_ULP_SESSION_MAX];
	/* Need to revisit a union for the tf related data */
	uint16_t			session_id;
};

/* ULP flow id structure */
struct rte_tf_flow {
	uint32_t	flow_id;
};

struct ulp_tlv_param {
	enum bnxt_ulp_df_param_type type;
	uint32_t length;
	uint8_t value[16];
};

struct ulp_context_list_entry {
	TAILQ_ENTRY(ulp_context_list_entry)	next;
	struct bnxt_ulp_context			*ulp_ctx;
};

struct bnxt_ulp_core_ops {
	int32_t
	(*ulp_init)(struct bnxt *bp,
		      struct bnxt_ulp_session_state *session);
	void
	(*ulp_deinit)(struct bnxt *bp,
		      struct bnxt_ulp_session_state *session);
	int32_t
	(*ulp_ctx_attach)(struct bnxt *bp,
			  struct bnxt_ulp_session_state *session);
	void
	(*ulp_ctx_detach)(struct bnxt *bp,
			  struct bnxt_ulp_session_state *session);
	int32_t
	(*ulp_vfr_session_fid_add)(struct bnxt_ulp_context *ulp_ctx,
				  uint16_t rep_fid);
	int32_t
	(*ulp_vfr_session_fid_rem)(struct bnxt_ulp_context *ulp_ctx,
				  uint16_t rep_fid);

	int32_t
	(*ulp_mtr_cap_get)(struct bnxt *bp,
			   struct rte_mtr_capabilities *cap);
};

extern const struct bnxt_ulp_core_ops bnxt_ulp_tf_core_ops;
extern const struct bnxt_ulp_core_ops bnxt_ulp_tfc_core_ops;

bool
ulp_is_default_session_active(struct bnxt_ulp_context *ulp_ctx);

/*
 * Allow the deletion of context only for the bnxt device that
 * created the session
 */
bool
ulp_ctx_deinit_allowed(struct bnxt_ulp_context *ulp_ctx);

void
bnxt_ulp_destroy_vfr_default_rules(struct bnxt *bp, bool global);

int32_t
bnxt_flow_mtr_init(struct bnxt *bp __rte_unused);

/* Function to create default flows. */
int32_t
ulp_default_flow_create(struct rte_eth_dev *eth_dev,
			struct ulp_tlv_param *param_list,
			uint32_t ulp_class_tid,
			uint16_t port_id,
			uint32_t *flow_id);

int
bnxt_ulp_flow_destroy(struct rte_eth_dev *dev, struct rte_flow *flow,
		      struct rte_flow_error *error);

/* Function to destroy default flows. */
int32_t
ulp_default_flow_destroy(struct rte_eth_dev *eth_dev,
			 uint32_t flow_id);

int32_t
bnxt_ulp_cntxt_list_init(void);

int32_t
bnxt_ulp_cntxt_list_add(struct bnxt_ulp_context *ulp_ctx);

void
bnxt_ulp_cntxt_list_del(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_cntxt_list_count(void);

struct bnxt_ulp_context *
bnxt_ulp_cntxt_entry_acquire(void *arg);

void
bnxt_ulp_cntxt_entry_release(void);

int32_t
bnxt_ulp_promisc_mode_set(struct bnxt *bp, uint8_t enable);

int32_t
bnxt_ulp_set_prio_attribute(struct ulp_rte_parser_params *params,
			    const struct rte_flow_attr *attr);

void
bnxt_ulp_set_dir_attributes(struct ulp_rte_parser_params *params,
			    const struct rte_flow_attr *attr);

void
bnxt_ulp_init_parser_cf_defaults(struct ulp_rte_parser_params *params,
				 uint16_t port_id);

int32_t
bnxt_ulp_grp_miss_act_set(struct rte_eth_dev *dev,
			  const struct rte_flow_attr *attr,
			  const struct rte_flow_action actions[],
			  uint32_t *flow_id);

int32_t
bnxt_ulp_hot_upgrade_process(struct bnxt *bp);

#endif /* _BNXT_ULP_H_ */
