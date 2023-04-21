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
#define BNXT_ULP_CUST_VXLAN_SUPPORT	0x100
#define BNXT_ULP_MULTI_SHARED_SUPPORT	0x200
#define BNXT_ULP_APP_HA_DYNAMIC		0x400

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

#define ULP_APP_CUST_VXLAN_SUPPORT(ctx)	   ((ctx)->cfg_data->vxlan_port != 0)
#define ULP_APP_CUST_VXLAN_IP_SUPPORT(ctx) ((ctx)->cfg_data->vxlan_ip_port != 0)

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

struct bnxt_ulp_df_rule_info {
	uint32_t			def_port_flow_id;
	uint8_t				valid;
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
	uint32_t			vxlan_port;
	uint32_t			vxlan_ip_port;
	uint32_t			ecpri_udp_port;
	uint8_t				hu_reg_state;
	uint8_t				hu_reg_cnt;
	uint32_t			hu_session_type;
	uint8_t				ha_pool_id;
	enum bnxt_ulp_session_type	def_session_type;
};

#define BNXT_ULP_SESSION_MAX 3
struct bnxt_ulp_context {
	struct bnxt_ulp_data	*cfg_data;
	struct tf		*g_tfp[BNXT_ULP_SESSION_MAX];
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

/*
 * Allow the deletion of context only for the bnxt device that
 * created the session
 */
bool
ulp_ctx_deinit_allowed(struct bnxt_ulp_context *ulp_ctx);

/* Function to set the device id of the hardware. */
int32_t
bnxt_ulp_cntxt_dev_id_set(struct bnxt_ulp_context *ulp_ctx, uint32_t dev_id);

/* Function to get the device id of the hardware. */
int32_t
bnxt_ulp_cntxt_dev_id_get(struct bnxt_ulp_context *ulp_ctx, uint32_t *dev_id);

/* Function to get whether or not ext mem is used for EM */
int32_t
bnxt_ulp_cntxt_mem_type_get(struct bnxt_ulp_context *ulp_ctx,
			    enum bnxt_ulp_flow_mem_type *mem_type);

/* Function to set whether or not ext mem is used for EM */
int32_t
bnxt_ulp_cntxt_mem_type_set(struct bnxt_ulp_context *ulp_ctx,
			    enum bnxt_ulp_flow_mem_type mem_type);

/* Function to set the table scope id of the EEM table. */
int32_t
bnxt_ulp_cntxt_tbl_scope_id_set(struct bnxt_ulp_context *ulp_ctx,
				uint32_t tbl_scope_id);

/* Function to get the table scope id of the EEM table. */
int32_t
bnxt_ulp_cntxt_tbl_scope_id_get(struct bnxt_ulp_context *ulp_ctx,
				uint32_t *tbl_scope_id);

/* Function to set the tfp session details in the ulp context. */
int32_t
bnxt_ulp_cntxt_tfp_set(struct bnxt_ulp_context *ulp,
		       enum bnxt_ulp_session_type s_type,
		       struct tf *tfp);

/* Function to get the tfp session details from ulp context. */
struct tf *
bnxt_ulp_cntxt_tfp_get(struct bnxt_ulp_context *ulp,
		       enum bnxt_ulp_session_type s_type);

/* Get the device table entry based on the device id. */
struct bnxt_ulp_device_params *
bnxt_ulp_device_params_get(uint32_t dev_id);

int32_t
bnxt_ulp_ctxt_ptr2_mark_db_set(struct bnxt_ulp_context *ulp_ctx,
			       struct bnxt_ulp_mark_tbl *mark_tbl);

struct bnxt_ulp_mark_tbl *
bnxt_ulp_ctxt_ptr2_mark_db_get(struct bnxt_ulp_context *ulp_ctx);

/* Function to set the flow database to the ulp context. */
int32_t
bnxt_ulp_cntxt_ptr2_flow_db_set(struct bnxt_ulp_context	*ulp_ctx,
				struct bnxt_ulp_flow_db	*flow_db);

/* Function to get the flow database from the ulp context. */
struct bnxt_ulp_flow_db	*
bnxt_ulp_cntxt_ptr2_flow_db_get(struct bnxt_ulp_context	*ulp_ctx);

/* Function to get the tunnel cache table info from the ulp context. */
struct bnxt_tun_cache_entry *
bnxt_ulp_cntxt_ptr2_tun_tbl_get(struct bnxt_ulp_context	*ulp_ctx);

/* Function to get the ulp context from eth device. */
struct bnxt_ulp_context	*
bnxt_ulp_eth_dev_ptr2_cntxt_get(struct rte_eth_dev *dev);

/* Function to add the ulp mapper data to the ulp context */
int32_t
bnxt_ulp_cntxt_ptr2_mapper_data_set(struct bnxt_ulp_context *ulp_ctx,
				    void *mapper_data);

/* Function to get the ulp mapper data from the ulp context */
void *
bnxt_ulp_cntxt_ptr2_mapper_data_get(struct bnxt_ulp_context *ulp_ctx);

/* Function to set the port database to the ulp context. */
int32_t
bnxt_ulp_cntxt_ptr2_port_db_set(struct bnxt_ulp_context	*ulp_ctx,
				struct bnxt_ulp_port_db	*port_db);

/* Function to get the port database from the ulp context. */
struct bnxt_ulp_port_db *
bnxt_ulp_cntxt_ptr2_port_db_get(struct bnxt_ulp_context	*ulp_ctx);

/* Function to create default flows. */
int32_t
ulp_default_flow_create(struct rte_eth_dev *eth_dev,
			struct ulp_tlv_param *param_list,
			uint32_t ulp_class_tid,
			uint16_t port_id,
			uint32_t *flow_id);

/* Function to destroy default flows. */
int32_t
ulp_default_flow_destroy(struct rte_eth_dev *eth_dev,
			 uint32_t flow_id);

int
bnxt_ulp_flow_destroy(struct rte_eth_dev *dev, struct rte_flow *flow,
		      struct rte_flow_error *error);

int32_t
bnxt_ulp_cntxt_ptr2_fc_info_set(struct bnxt_ulp_context *ulp_ctx,
				struct bnxt_ulp_fc_info *ulp_fc_info);

struct bnxt_ulp_fc_info *
bnxt_ulp_cntxt_ptr2_fc_info_get(struct bnxt_ulp_context *ulp_ctx);

int32_t
bnxt_ulp_cntxt_ptr2_ulp_flags_get(struct bnxt_ulp_context *ulp_ctx,
				  uint32_t *flags);

int32_t
bnxt_ulp_get_df_rule_info(uint16_t port_id, struct bnxt_ulp_context *ulp_ctx,
			  struct bnxt_ulp_df_rule_info *info);

struct bnxt_ulp_vfr_rule_info*
bnxt_ulp_cntxt_ptr2_ulp_vfr_info_get(struct bnxt_ulp_context *ulp_ctx,
				     uint32_t port_id);

int32_t
bnxt_ulp_cntxt_acquire_fdb_lock(struct bnxt_ulp_context	*ulp_ctx);

void
bnxt_ulp_cntxt_release_fdb_lock(struct bnxt_ulp_context	*ulp_ctx);

int32_t
bnxt_get_action_handle_type(const struct rte_flow_action_handle *handle,
			    uint32_t *action_handle_type);

struct bnxt_ulp_shared_act_info *
bnxt_ulp_shared_act_info_get(uint32_t *num_entries);

int32_t
bnxt_get_action_handle_direction(const struct rte_flow_action_handle *handle,
				 uint32_t *dir);

uint32_t
bnxt_get_action_handle_index(const struct rte_flow_action_handle *handle);

struct bnxt_ulp_glb_resource_info *
bnxt_ulp_app_glb_resource_info_list_get(uint32_t *num_entries);

int32_t
bnxt_ulp_cntxt_app_id_set(struct bnxt_ulp_context *ulp_ctx, uint8_t app_id);

int32_t
bnxt_ulp_cntxt_app_id_get(struct bnxt_ulp_context *ulp_ctx, uint8_t *app_id);

bool
bnxt_ulp_cntxt_shared_session_enabled(struct bnxt_ulp_context *ulp_ctx);

bool
bnxt_ulp_cntxt_multi_shared_session_enabled(struct bnxt_ulp_context *ulp_ctx);

struct bnxt_ulp_app_capabilities_info *
bnxt_ulp_app_cap_list_get(uint32_t *num_entries);

int32_t
bnxt_ulp_cntxt_app_caps_init(struct bnxt *bp,
			     uint8_t app_id, uint32_t dev_id);

struct bnxt_ulp_resource_resv_info *
bnxt_ulp_resource_resv_list_get(uint32_t *num_entries);

int32_t
bnxt_ulp_cntxt_ptr2_ha_info_set(struct bnxt_ulp_context *ulp_ctx,
				struct bnxt_ulp_ha_mgr_info *ulp_ha_info);

struct bnxt_ulp_ha_mgr_info *
bnxt_ulp_cntxt_ptr2_ha_info_get(struct bnxt_ulp_context *ulp_ctx);

bool
bnxt_ulp_cntxt_ha_enabled(struct bnxt_ulp_context *ulp_ctx);

struct bnxt_ulp_context *
bnxt_ulp_cntxt_entry_acquire(void *arg);

void
bnxt_ulp_cntxt_entry_release(void);

uint8_t
bnxt_ulp_cntxt_num_shared_clients_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_cntxt_num_shared_clients_set(struct bnxt_ulp_context *ulp_ctx,
				      bool incr);

struct bnxt_flow_app_tun_ent *
bnxt_ulp_cntxt_ptr2_app_tun_list_get(struct bnxt_ulp_context *ulp);

/* Function to get the truflow app id. This defined in the build file */
uint32_t
bnxt_ulp_default_app_id_get(void);

int
bnxt_ulp_vxlan_port_set(struct bnxt_ulp_context *ulp_ctx,
			uint32_t vxlan_port);
unsigned int
bnxt_ulp_vxlan_port_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_vxlan_ip_port_set(struct bnxt_ulp_context *ulp_ctx,
			   uint32_t vxlan_ip_port);
unsigned int
bnxt_ulp_vxlan_ip_port_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_ecpri_udp_port_set(struct bnxt_ulp_context *ulp_ctx,
			    uint32_t ecpri_udp_port);
unsigned int
bnxt_ulp_ecpri_udp_port_get(struct bnxt_ulp_context *ulp_ctx);

int32_t
bnxt_flow_meter_init(struct bnxt *bp);

uint32_t
bnxt_ulp_cntxt_convert_dev_id(uint32_t ulp_dev_id);

int32_t
bnxt_ulp_ha_reg_set(struct bnxt_ulp_context *ulp_ctx,
		    uint8_t state, uint8_t cnt);

uint32_t
bnxt_ulp_ha_reg_state_get(struct bnxt_ulp_context *ulp_ctx);

uint32_t
bnxt_ulp_ha_reg_cnt_get(struct bnxt_ulp_context *ulp_ctx);

struct tf*
bnxt_ulp_bp_tfp_get(struct bnxt *bp, enum bnxt_ulp_session_type type);
#endif /* _BNXT_ULP_H_ */
