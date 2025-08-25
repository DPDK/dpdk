/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_ULP_UTILS_H_
#define _BNXT_ULP_UTILS_H_

#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_tailq.h>
#include <rte_spinlock.h>

#include "bnxt.h"
#include "bnxt_ulp.h"
#include "bnxt_tf_common.h"
#include "bnxt_hwrm.h"
#include "hsi_struct_def_dpdk.h"
#include "tf_core.h"
#include "tf_ext_flow_handle.h"

#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "ulp_mark_mgr.h"
#include "ulp_fc_mgr.h"
#include "ulp_sc_mgr.h"
#include "ulp_flow_db.h"
#include "ulp_mapper.h"
#include "ulp_matcher.h"
#include "ulp_port_db.h"
#include "ulp_tun.h"
#include "ulp_ha_mgr.h"
#include "bnxt_tf_pmd_shim.h"
#include "ulp_template_db_tbl.h"

static inline int32_t
bnxt_ulp_devid_get(struct bnxt *bp,
		   enum bnxt_ulp_device_id  *ulp_dev_id)
{
	if (BNXT_CHIP_P7(bp)) {
		*ulp_dev_id = BNXT_ULP_DEVICE_ID_THOR2;
		return 0;
	}

	if (BNXT_CHIP_P5(bp)) {
		*ulp_dev_id = BNXT_ULP_DEVICE_ID_THOR;
		return 0;
	}

	if (BNXT_STINGRAY(bp))
		*ulp_dev_id = BNXT_ULP_DEVICE_ID_STINGRAY;
	else
		/* Assuming Whitney */
		*ulp_dev_id = BNXT_ULP_DEVICE_ID_WH_PLUS;

	return 0;
}

static inline struct bnxt_ulp_app_capabilities_info *
bnxt_ulp_app_cap_list_get(uint32_t *num_entries)
{
	if (unlikely(!num_entries))
		return NULL;
	*num_entries = BNXT_ULP_APP_CAP_TBL_MAX_SZ;
	return ulp_app_cap_info_list;
}

static inline struct bnxt_ulp_shared_act_info *
bnxt_ulp_shared_act_info_get(uint32_t *num_entries)
{
	if (unlikely(!num_entries))
		return NULL;

	*num_entries = BNXT_ULP_GEN_TBL_MAX_SZ;

	return ulp_shared_act_info;
}

static inline struct bnxt_ulp_resource_resv_info *
bnxt_ulp_app_resource_resv_list_get(uint32_t *num_entries)
{
	if (unlikely(num_entries == NULL))
		return NULL;
	*num_entries = BNXT_ULP_APP_RESOURCE_RESV_LIST_MAX_SZ;
	return ulp_app_resource_resv_list;
}

static inline struct bnxt_ulp_resource_resv_info *
bnxt_ulp_resource_resv_list_get(uint32_t *num_entries)
{
	if (unlikely(!num_entries))
		return NULL;
	*num_entries = BNXT_ULP_RESOURCE_RESV_LIST_MAX_SZ;
	return ulp_resource_resv_list;
}

static inline struct bnxt_ulp_glb_resource_info *
bnxt_ulp_app_glb_resource_info_list_get(uint32_t *num_entries)
{
	if (unlikely(!num_entries))
		return NULL;
	*num_entries = BNXT_ULP_APP_GLB_RESOURCE_TBL_MAX_SZ;
	return ulp_app_glb_resource_tbl;
}

/* Function to set the number for vxlan_ip (custom vxlan) port into the context */
static inline int
bnxt_ulp_cntxt_ecpri_udp_port_set(struct bnxt_ulp_context *ulp_ctx,
			   uint32_t ecpri_udp_port)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return -EINVAL;

	ulp_ctx->cfg_data->ecpri_udp_port = ecpri_udp_port;

	return 0;
}

/* Function to retrieve the vxlan_ip (custom vxlan) port from the context. */
static inline unsigned int
bnxt_ulp_cntxt_ecpri_udp_port_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return 0;

	return (unsigned int)ulp_ctx->cfg_data->ecpri_udp_port;
}

/* Function to set the number for vxlan_ip (custom vxlan) port into the context */
static inline int
bnxt_ulp_cntxt_vxlan_ip_port_set(struct bnxt_ulp_context *ulp_ctx,
			   uint32_t vxlan_ip_port)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return -EINVAL;

	ulp_ctx->cfg_data->vxlan_ip_port = vxlan_ip_port;
	if (vxlan_ip_port)
		ulp_ctx->cfg_data->ulp_flags |= BNXT_ULP_STATIC_VXLAN_SUPPORT;
	return 0;
}

/* Function to retrieve the vxlan_ip (custom vxlan) port from the context. */
static inline unsigned int
bnxt_ulp_cntxt_vxlan_ip_port_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return 0;

	return (unsigned int)ulp_ctx->cfg_data->vxlan_ip_port;
}

/* Function to set the number for vxlan_gpe next_proto into the context */
static inline uint32_t
bnxt_ulp_vxlan_gpe_next_proto_set(struct bnxt_ulp_context *ulp_ctx,
				  uint8_t tunnel_next_proto)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return -EINVAL;

	ulp_ctx->cfg_data->tunnel_next_proto = tunnel_next_proto;

	return 0;
}

/* Function to retrieve the vxlan_gpe next_proto from the context. */
static inline uint8_t
bnxt_ulp_vxlan_gpe_next_proto_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return 0;

	return ulp_ctx->cfg_data->tunnel_next_proto;
}

/* Function to set the number for vxlan port into the context */
static inline int
bnxt_ulp_cntxt_vxlan_port_set(struct bnxt_ulp_context *ulp_ctx,
			uint32_t vxlan_port)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return -EINVAL;

	ulp_ctx->cfg_data->vxlan_port = vxlan_port;
	if (vxlan_port)
		ulp_ctx->cfg_data->ulp_flags |= BNXT_ULP_STATIC_VXLAN_SUPPORT;

	return 0;
}

/* Function to retrieve the vxlan port from the context. */
static inline unsigned int
bnxt_ulp_cntxt_vxlan_port_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return 0;

	return (unsigned int)ulp_ctx->cfg_data->vxlan_port;
}

static inline int
bnxt_ulp_default_app_priority_set(struct bnxt_ulp_context *ulp_ctx,
				  uint32_t prio)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return -EINVAL;

	ulp_ctx->cfg_data->default_priority = prio;
	return 0;
}

static inline unsigned int
bnxt_ulp_default_app_priority_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return 0;

	return (unsigned int)ulp_ctx->cfg_data->default_priority;
}

static inline int
bnxt_ulp_max_def_priority_set(struct bnxt_ulp_context *ulp_ctx,
			      uint32_t prio)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return -EINVAL;

	ulp_ctx->cfg_data->max_def_priority = prio;
	return 0;
}

static inline unsigned int
bnxt_ulp_max_def_priority_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return 0;

	return (unsigned int)ulp_ctx->cfg_data->max_def_priority;
}

static inline int
bnxt_ulp_min_flow_priority_set(struct bnxt_ulp_context *ulp_ctx, uint32_t prio)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return -EINVAL;

	ulp_ctx->cfg_data->min_flow_priority = prio;
	return 0;
}

static inline unsigned int
bnxt_ulp_min_flow_priority_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return 0;

	return ulp_ctx->cfg_data->min_flow_priority;
}

static inline int
bnxt_ulp_max_flow_priority_set(struct bnxt_ulp_context *ulp_ctx, uint32_t prio)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return -EINVAL;

	ulp_ctx->cfg_data->max_flow_priority = prio;
	return 0;
}

static inline unsigned int
bnxt_ulp_max_flow_priority_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return 0;

	return ulp_ctx->cfg_data->max_flow_priority;
}

/* Below are the access functions to access internal data of ulp context. */
/* Function to set the Mark DB into the context */
static inline int32_t
bnxt_ulp_cntxt_ptr2_mark_db_set(struct bnxt_ulp_context *ulp_ctx,
				struct bnxt_ulp_mark_tbl *mark_tbl)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data)) {
		BNXT_DRV_DBG(ERR, "Invalid ulp context data\n");
		return -EINVAL;
	}

	ulp_ctx->cfg_data->mark_tbl = mark_tbl;

	return 0;
}

/* Function to retrieve the Mark DB from the context. */
static inline struct bnxt_ulp_mark_tbl *
bnxt_ulp_cntxt_ptr2_mark_db_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return NULL;

	return ulp_ctx->cfg_data->mark_tbl;
}

static inline bool
bnxt_ulp_cntxt_shared_session_enabled(struct bnxt_ulp_context *ulp_ctx)
{
	return ULP_SHARED_SESSION_IS_ENABLED(ulp_ctx->cfg_data->ulp_flags);
}

static inline bool
bnxt_ulp_cntxt_multi_shared_session_enabled(struct bnxt_ulp_context *ulp_ctx)
{
	return ULP_MULTI_SHARED_IS_SUPPORTED(ulp_ctx);
}

static inline int32_t
bnxt_ulp_cntxt_app_id_set(struct bnxt_ulp_context *ulp_ctx, uint8_t app_id)
{
	if (unlikely(!ulp_ctx))
		return -EINVAL;
	ulp_ctx->cfg_data->app_id = app_id;
	return 0;
}

static inline int32_t
bnxt_ulp_cntxt_app_id_get(struct bnxt_ulp_context *ulp_ctx, uint8_t *app_id)
{
	/* Default APP id is zero */
	if (unlikely(!ulp_ctx || !app_id))
		return -EINVAL;
	*app_id = ulp_ctx->cfg_data->app_id;
	return 0;
}

/* Function to set the device id of the hardware. */
static inline int32_t
bnxt_ulp_cntxt_dev_id_set(struct bnxt_ulp_context *ulp_ctx,
			  uint32_t dev_id)
{
	if (likely(ulp_ctx && ulp_ctx->cfg_data)) {
		ulp_ctx->cfg_data->dev_id = dev_id;
		return 0;
	}

	return -EINVAL;
}

/* Function to get the device id of the hardware. */
static inline int32_t
bnxt_ulp_cntxt_dev_id_get(struct bnxt_ulp_context *ulp_ctx,
			  uint32_t *dev_id)
{
	if (likely(ulp_ctx && ulp_ctx->cfg_data)) {
		*dev_id = ulp_ctx->cfg_data->dev_id;
		return 0;
	}
	*dev_id = BNXT_ULP_DEVICE_ID_LAST;
	BNXT_DRV_DBG(ERR, "Failed to read dev_id from ulp ctxt\n");
	return -EINVAL;
}

static inline int32_t
bnxt_ulp_cntxt_mem_type_set(struct bnxt_ulp_context *ulp_ctx,
			    enum bnxt_ulp_flow_mem_type mem_type)
{
	if (likely(ulp_ctx && ulp_ctx->cfg_data)) {
		ulp_ctx->cfg_data->mem_type = mem_type;
		return 0;
	}
	BNXT_DRV_DBG(ERR, "Failed to write mem_type in ulp ctxt\n");
	return -EINVAL;
}

static inline int32_t
bnxt_ulp_cntxt_mem_type_get(struct bnxt_ulp_context *ulp_ctx,
			    enum bnxt_ulp_flow_mem_type *mem_type)
{
	if (likely(ulp_ctx && ulp_ctx->cfg_data)) {
		*mem_type = ulp_ctx->cfg_data->mem_type;
		return 0;
	}
	*mem_type = BNXT_ULP_FLOW_MEM_TYPE_LAST;
	BNXT_DRV_DBG(ERR, "Failed to read mem_type in ulp ctxt\n");
	return -EINVAL;
}

/* Function to get the table scope id of the EEM table. */
static inline int32_t
bnxt_ulp_cntxt_tbl_scope_id_get(struct bnxt_ulp_context *ulp_ctx,
				uint32_t *tbl_scope_id)
{
	if (likely(ulp_ctx && ulp_ctx->cfg_data)) {
		*tbl_scope_id = ulp_ctx->cfg_data->tbl_scope_id;
		return 0;
	}

	return -EINVAL;
}

/* Function to set the table scope id of the EEM table. */
static inline int32_t
bnxt_ulp_cntxt_tbl_scope_id_set(struct bnxt_ulp_context *ulp_ctx,
				uint32_t tbl_scope_id)
{
	if (likely(ulp_ctx && ulp_ctx->cfg_data)) {
		ulp_ctx->cfg_data->tbl_scope_id = tbl_scope_id;
		return 0;
	}

	return -EINVAL;
}

/* Function to set the v3 table scope id, only works for tfc objects */
static inline int32_t
bnxt_ulp_cntxt_tsid_set(struct bnxt_ulp_context *ulp_ctx, uint8_t tsid)
{
	if (likely(ulp_ctx && ulp_ctx->tfo_type == BNXT_ULP_TFO_TYPE_TFC)) {
		ulp_ctx->tsid = tsid;
		ULP_BITMAP_SET(ulp_ctx->tfo_flags, BNXT_ULP_TFO_TSID_FLAG);
		return 0;
	}
	return -EINVAL;
}

/* Function to reset the v3 table scope id, only works for tfc objects */
static inline void
bnxt_ulp_cntxt_tsid_reset(struct bnxt_ulp_context *ulp_ctx)
{
	if (ulp_ctx && ulp_ctx->tfo_type == BNXT_ULP_TFO_TYPE_TFC)
		ULP_BITMAP_RESET(ulp_ctx->tfo_flags, BNXT_ULP_TFO_TSID_FLAG);
}

/* Function to set the v3 table scope id, only works for tfc objects */
static inline int32_t
bnxt_ulp_cntxt_tsid_get(struct bnxt_ulp_context *ulp_ctx, uint8_t *tsid)
{
	if (likely(ulp_ctx && tsid &&
		   ulp_ctx->tfo_type == BNXT_ULP_TFO_TYPE_TFC &&
		   ULP_BITMAP_ISSET(ulp_ctx->tfo_flags, BNXT_ULP_TFO_TSID_FLAG))) {
		*tsid = ulp_ctx->tsid;
		return 0;
	}
	return -EINVAL;
}

/* Function to set the v3 session id, only works for tfc objects */
static inline int32_t
bnxt_ulp_cntxt_sid_set(struct bnxt_ulp_context *ulp_ctx,
		       uint16_t sid)
{
	if (likely(ulp_ctx && ulp_ctx->tfo_type == BNXT_ULP_TFO_TYPE_TFC)) {
		ulp_ctx->sid = sid;
		ULP_BITMAP_SET(ulp_ctx->tfo_flags, BNXT_ULP_TFO_SID_FLAG);
		return 0;
	}
	return -EINVAL;
}

/*
 * Function to reset the v3 session id, only works for tfc objects
 * There isn't a known invalid value for sid, so this is necessary
 */
static inline void
bnxt_ulp_cntxt_sid_reset(struct bnxt_ulp_context *ulp_ctx)
{
	if (ulp_ctx && ulp_ctx->tfo_type == BNXT_ULP_TFO_TYPE_TFC)
		ULP_BITMAP_RESET(ulp_ctx->tfo_flags, BNXT_ULP_TFO_SID_FLAG);
}

/* Function to get the v3 session id, only works for tfc objects */
static inline int32_t
bnxt_ulp_cntxt_sid_get(struct bnxt_ulp_context *ulp_ctx,
		       uint16_t *sid)
{
	if (likely(ulp_ctx && sid &&
		   ulp_ctx->tfo_type == BNXT_ULP_TFO_TYPE_TFC &&
		   ULP_BITMAP_ISSET(ulp_ctx->tfo_flags, BNXT_ULP_TFO_SID_FLAG))) {
		*sid = ulp_ctx->sid;
		return 0;
	}
	return -EINVAL;
}

/* Function to get the number of shared clients attached */
static inline uint8_t
bnxt_ulp_cntxt_num_shared_clients_get(struct bnxt_ulp_context *ulp)
{
	if (likely(ulp == NULL || ulp->cfg_data == NULL)) {
		BNXT_DRV_DBG(ERR, "Invalid arguments\n");
		return 0;
	}
	return ulp->cfg_data->num_shared_clients;
}

/* Function to set the number of shared clients */
static inline int
bnxt_ulp_cntxt_num_shared_clients_set(struct bnxt_ulp_context *ulp, bool incr)
{
	if (unlikely(ulp == NULL || ulp->cfg_data == NULL)) {
		BNXT_DRV_DBG(ERR, "Invalid arguments\n");
		return 0;
	}
	if (incr)
		ulp->cfg_data->num_shared_clients++;
	else if (ulp->cfg_data->num_shared_clients)
		ulp->cfg_data->num_shared_clients--;

	BNXT_DRV_DBG(DEBUG, "%d:clients(%d)\n", incr,
		     ulp->cfg_data->num_shared_clients);

	return 0;
}

static inline int32_t
bnxt_ulp_cntxt_bp_set(struct bnxt_ulp_context *ulp, struct bnxt *bp)
{
	if (unlikely(ulp == NULL)) {
		BNXT_DRV_DBG(ERR, "Invalid arguments\n");
		return -EINVAL;
	}
	ulp->bp = bp;
	return 0;
}

static inline struct bnxt*
bnxt_ulp_cntxt_bp_get(struct bnxt_ulp_context *ulp)
{
	if (unlikely(ulp == NULL)) {
		BNXT_DRV_DBG(ERR, "Invalid arguments\n");
		return NULL;
	}
	return ulp->bp;
}

static inline int32_t
bnxt_ulp_cntxt_fid_get(struct bnxt_ulp_context *ulp, uint16_t *fid)
{
	if (unlikely(ulp == NULL || fid == NULL))
		return -EINVAL;

	*fid = ulp->bp->fw_fid;
	return 0;
}

static inline void
bnxt_ulp_cntxt_ptr2_default_class_bits_set(struct bnxt_ulp_context *ulp_ctx,
					   uint64_t bits)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return;
	ulp_ctx->cfg_data->default_class_bits = bits;
}

static inline uint64_t
bnxt_ulp_cntxt_ptr2_default_class_bits_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return 0;
	return ulp_ctx->cfg_data->default_class_bits;
}

static inline void
bnxt_ulp_cntxt_ptr2_default_act_bits_set(struct bnxt_ulp_context *ulp_ctx,
					 uint64_t bits)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return;
	ulp_ctx->cfg_data->default_act_bits = bits;
}

static inline uint64_t
bnxt_ulp_cntxt_ptr2_default_act_bits_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return 0;
	return ulp_ctx->cfg_data->default_act_bits;
}

/*
 * Get the device table entry based on the device id.
 *
 * dev_id [in] The device id of the hardware
 *
 * Returns the pointer to the device parameters.
 */
static inline struct bnxt_ulp_device_params *
bnxt_ulp_device_params_get(uint32_t dev_id)
{
	if (dev_id < BNXT_ULP_MAX_NUM_DEVICES)
		return &ulp_device_params[dev_id];
	return NULL;
}

/* Function to set the flow database to the ulp context. */
static inline int32_t
bnxt_ulp_cntxt_ptr2_flow_db_set(struct bnxt_ulp_context	*ulp_ctx,
				struct bnxt_ulp_flow_db	*flow_db)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return -EINVAL;

	ulp_ctx->cfg_data->flow_db = flow_db;
	return 0;
}

/* Function to get the flow database from the ulp context. */
static inline struct bnxt_ulp_flow_db	*
bnxt_ulp_cntxt_ptr2_flow_db_get(struct bnxt_ulp_context	*ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return NULL;

	return ulp_ctx->cfg_data->flow_db;
}

/* Function to get the tunnel cache table info from the ulp context. */
static inline struct bnxt_tun_cache_entry *
bnxt_ulp_cntxt_ptr2_tun_tbl_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return NULL;

	return ulp_ctx->cfg_data->tun_tbl;
}

/* Function to get the ulp context from eth device. */
static inline struct bnxt_ulp_context	*
bnxt_ulp_eth_dev_ptr2_cntxt_get(struct rte_eth_dev	*dev)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;

	if (BNXT_ETH_DEV_IS_REPRESENTOR(dev)) {
		struct bnxt_representor *vfr = dev->data->dev_private;

		bp = vfr->parent_dev->data->dev_private;
	}

	if (unlikely(!bp)) {
		BNXT_DRV_DBG(ERR, "Bnxt private data is not initialized\n");
		return NULL;
	}
	return bp->ulp_ctx;
}

static inline int32_t
bnxt_ulp_cntxt_ptr2_mapper_data_set(struct bnxt_ulp_context *ulp_ctx,
				    void *mapper_data)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data)) {
		BNXT_DRV_DBG(ERR, "Invalid ulp context data\n");
		return -EINVAL;
	}

	ulp_ctx->cfg_data->mapper_data = mapper_data;
	return 0;
}

static inline void *
bnxt_ulp_cntxt_ptr2_mapper_data_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data)) {
		BNXT_DRV_DBG(ERR, "Invalid ulp context data\n");
		return NULL;
	}

	return ulp_ctx->cfg_data->mapper_data;
}

static inline int32_t
bnxt_ulp_cntxt_ptr2_matcher_data_set(struct bnxt_ulp_context *ulp_ctx,
				     void *matcher_data)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data)) {
		BNXT_DRV_DBG(ERR, "Invalid ulp context data\n");
		return -EINVAL;
	}

	ulp_ctx->cfg_data->matcher_data = matcher_data;
	return 0;
}

static inline void *
bnxt_ulp_cntxt_ptr2_matcher_data_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data)) {
		BNXT_DRV_DBG(ERR, "Invalid ulp context data\n");
		return NULL;
	}

	return ulp_ctx->cfg_data->matcher_data;
}

/* Function to set the port database to the ulp context. */
static inline int32_t
bnxt_ulp_cntxt_ptr2_port_db_set(struct bnxt_ulp_context	*ulp_ctx,
				struct bnxt_ulp_port_db	*port_db)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return -EINVAL;

	ulp_ctx->cfg_data->port_db = port_db;
	return 0;
}

/* Function to get the port database from the ulp context. */
static inline struct bnxt_ulp_port_db *
bnxt_ulp_cntxt_ptr2_port_db_get(struct bnxt_ulp_context	*ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return NULL;

	return ulp_ctx->cfg_data->port_db;
}

/* Function to set the flow counter info into the context */
static inline int32_t
bnxt_ulp_cntxt_ptr2_fc_info_set(struct bnxt_ulp_context *ulp_ctx,
				struct bnxt_ulp_fc_info *ulp_fc_info)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data)) {
		BNXT_DRV_DBG(ERR, "Invalid ulp context data\n");
		return -EINVAL;
	}

	ulp_ctx->cfg_data->fc_info = ulp_fc_info;

	return 0;
}

/* Function to retrieve the flow counter info from the context. */
static inline struct bnxt_ulp_fc_info *
bnxt_ulp_cntxt_ptr2_fc_info_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return NULL;

	return ulp_ctx->cfg_data->fc_info;
}

/* Function to set the flow counter info into the context */
static inline int32_t
bnxt_ulp_cntxt_ptr2_sc_info_set(struct bnxt_ulp_context *ulp_ctx,
				struct bnxt_ulp_sc_info *ulp_sc_info)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data)) {
		BNXT_DRV_DBG(ERR, "Invalid ulp context data\n");
		return -EINVAL;
	}

	ulp_ctx->cfg_data->sc_info = ulp_sc_info;

	return 0;
}

/* Function to retrieve the flow counter info from the context. */
static inline struct bnxt_ulp_sc_info *
bnxt_ulp_cntxt_ptr2_sc_info_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return NULL;

	return ulp_ctx->cfg_data->sc_info;
}

/* Function to get the ulp flags from the ulp context. */
static inline int32_t
bnxt_ulp_cntxt_ptr2_ulp_flags_get(struct bnxt_ulp_context *ulp_ctx,
				  uint32_t *flags)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return -1;

	*flags =  ulp_ctx->cfg_data->ulp_flags;
	return 0;
}

/* Function to get the ulp vfr info from the ulp context. */
static inline struct bnxt_ulp_vfr_rule_info*
bnxt_ulp_cntxt_ptr2_ulp_vfr_info_get(struct bnxt_ulp_context *ulp_ctx,
				     uint32_t port_id)
{
	if (unlikely(!ulp_ctx ||
		     !ulp_ctx->cfg_data ||
		     port_id >= RTE_MAX_ETHPORTS))
		return NULL;

	return &ulp_ctx->cfg_data->vfr_rule_info[port_id];
}

/* Function to acquire the flow database lock from the ulp context. */
static inline int32_t
bnxt_ulp_cntxt_acquire_fdb_lock(struct bnxt_ulp_context	*ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return -1;

	if (pthread_mutex_lock(&ulp_ctx->cfg_data->flow_db_lock)) {
		BNXT_DRV_DBG(ERR, "unable to acquire fdb lock\n");
		return -1;
	}
	return 0;
}

/* Function to release the flow database lock from the ulp context. */
static inline void
bnxt_ulp_cntxt_release_fdb_lock(struct bnxt_ulp_context	*ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return;

	pthread_mutex_unlock(&ulp_ctx->cfg_data->flow_db_lock);
}

/* Function to extract the action type from the shared action handle. */
static inline int32_t
bnxt_get_action_handle_type(const struct rte_flow_action_handle *handle,
			    uint32_t *action_handle_type)
{
	if (unlikely(!action_handle_type))
		return -EINVAL;

	*action_handle_type = (uint32_t)(((uint64_t)handle >> 32) & 0xffffffff);
	if (*action_handle_type >= BNXT_ULP_GEN_TBL_MAX_SZ)
		return -EINVAL;

	return 0;
}

/* Function to extract the direction from the shared action handle. */
static inline int32_t
bnxt_get_action_handle_direction(const struct rte_flow_action_handle *handle,
				 uint32_t *dir)
{
	uint32_t shared_type;
	int32_t ret = 0;

	ret = bnxt_get_action_handle_type(handle, &shared_type);
	if (unlikely(ret))
		return ret;

	*dir = shared_type & 0x1 ? BNXT_ULP_DIR_EGRESS : BNXT_ULP_DIR_INGRESS;

	return ret;
}

/* Function to extract the action index from the shared action handle. */
static inline uint32_t
bnxt_get_action_handle_index(const struct rte_flow_action_handle *handle)
{
	return (uint32_t)((uint64_t)handle & 0xffffffff);
}

/* Function to set the ha info into the context */
static inline int32_t
bnxt_ulp_cntxt_ptr2_ha_info_set(struct bnxt_ulp_context *ulp_ctx,
				struct bnxt_ulp_ha_mgr_info *ulp_ha_info)
{
	if (unlikely(ulp_ctx == NULL || ulp_ctx->cfg_data == NULL)) {
		BNXT_DRV_DBG(ERR, "Invalid ulp context data\n");
		return -EINVAL;
	}
	ulp_ctx->cfg_data->ha_info = ulp_ha_info;
	return 0;
}

/* Function to retrieve the ha info from the context. */
static inline struct bnxt_ulp_ha_mgr_info *
bnxt_ulp_cntxt_ptr2_ha_info_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(ulp_ctx == NULL || ulp_ctx->cfg_data == NULL))
		return NULL;
	return ulp_ctx->cfg_data->ha_info;
}

static inline bool
bnxt_ulp_cntxt_ha_enabled(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(ulp_ctx == NULL || ulp_ctx->cfg_data == NULL))
		return false;
	return !!ULP_HIGH_AVAIL_IS_ENABLED(ulp_ctx->cfg_data->ulp_flags);
}

/* Function to get the app tunnel details from the ulp context. */
static inline struct bnxt_flow_app_tun_ent *
bnxt_ulp_cntxt_ptr2_app_tun_list_get(struct bnxt_ulp_context *ulp)
{
	if (unlikely(!ulp || !ulp->cfg_data))
		return NULL;

	return ulp->cfg_data->app_tun;
}

/* Function to get the truflow app id. This defined in the build file */
static inline uint32_t
bnxt_ulp_default_app_id_get(void)
{
	return BNXT_TF_APP_ID;
}

/* Function to convert ulp dev id to regular dev id. */
static inline uint32_t
bnxt_ulp_cntxt_convert_dev_id(uint32_t ulp_dev_id)
{
	enum tf_device_type type = 0;

	switch (ulp_dev_id) {
	case BNXT_ULP_DEVICE_ID_WH_PLUS:
		type = TF_DEVICE_TYPE_P4;
		break;
	case BNXT_ULP_DEVICE_ID_STINGRAY:
		type = TF_DEVICE_TYPE_SR;
		break;
	case BNXT_ULP_DEVICE_ID_THOR:
		type = TF_DEVICE_TYPE_P5;
		break;
	default:
		BNXT_DRV_DBG(ERR, "Invalid device id\n");
		break;
	}
	return type;
}

/* This function sets the IF table index for the
 * Application to poll to get the hot upgrade state and count details from
 * the firmware.
 */
static inline int32_t
bnxt_ulp_cntxt_ha_reg_set(struct bnxt_ulp_context *ulp_ctx,
		    uint8_t state, uint8_t cnt)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return -EINVAL;

	if (ULP_MULTI_SHARED_IS_SUPPORTED(ulp_ctx)) {
		ulp_ctx->cfg_data->hu_reg_state = state;
		ulp_ctx->cfg_data->hu_reg_cnt = cnt;
	} else {
		ulp_ctx->cfg_data->hu_reg_state = ULP_HA_IF_TBL_IDX;
		ulp_ctx->cfg_data->hu_reg_cnt = ULP_HA_CLIENT_CNT_IF_TBL_IDX;
	}
	return 0;
}

/* This function gets the IF table index for the
 * Application to poll to get the application hot upgrade state from
 * the firmware.
 */
static inline uint32_t
bnxt_ulp_cntxt_ha_reg_state_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return 0;

	return (uint32_t)ulp_ctx->cfg_data->hu_reg_state;
}

/* This function gets the IF table index for the
 * Application to poll to get the application count from
 * the firmware.
 */
static inline uint32_t
bnxt_ulp_cntxt_ha_reg_cnt_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return 0;

	return (uint32_t)ulp_ctx->cfg_data->hu_reg_cnt;
}

/* This function sets the number of key recipes supported
 * Generally, this should be set to the number of flexible keys
 * supported
 */
static inline void
bnxt_ulp_num_key_recipes_set(struct bnxt_ulp_context *ulp_ctx,
			     uint16_t num_recipes)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return;
	ulp_ctx->cfg_data->num_key_recipes_per_dir = num_recipes;
}

/* This function gets the number of key recipes supported */
static inline int32_t
bnxt_ulp_num_key_recipes_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return 0;
	return ulp_ctx->cfg_data->num_key_recipes_per_dir;
}

/* This function gets the feature bits */
static inline uint64_t
bnxt_ulp_feature_bits_get(struct bnxt_ulp_context *ulp_ctx)
{
	if (unlikely(!ulp_ctx || !ulp_ctx->cfg_data))
		return 0;
	return ulp_ctx->cfg_data->feature_bits;
}

/* Add the VF Rep endpoint to the session */
static inline int32_t
bnxt_ulp_vfr_session_fid_add(struct bnxt_ulp_context *ulp_ctx,
			     uint16_t vfr_fid)
{
	int32_t rc = 0;

	if (unlikely(ulp_ctx == NULL || ulp_ctx->ops == NULL))
		return -EINVAL;
	if (ulp_ctx->ops->ulp_vfr_session_fid_add)
		rc = ulp_ctx->ops->ulp_vfr_session_fid_add(ulp_ctx, vfr_fid);

	return rc;
}

/* Remove the VF Rep endpoint from the session */
static inline int32_t
bnxt_ulp_vfr_session_fid_rem(struct bnxt_ulp_context *ulp_ctx,
			     uint16_t vfr_fid)
{
	int32_t rc = 0;

	if (unlikely(ulp_ctx == NULL || ulp_ctx->ops == NULL))
		return -EINVAL;
	if (ulp_ctx->ops->ulp_vfr_session_fid_rem)
		rc = ulp_ctx->ops->ulp_vfr_session_fid_rem(ulp_ctx, vfr_fid);
	return rc;
}

static inline int32_t
bnxt_ulp_cap_feat_process(uint64_t feat_bits, uint64_t *out_bits)
{
#ifdef RTE_BNXT_TF_FEAT_BITS
	uint64_t bit = RTE_BNXT_TF_FEAT_BITS;
#else
	uint64_t bit = 0;
#endif

	*out_bits = 0;
	if ((feat_bits | bit) != feat_bits) {
		BNXT_DRV_DBG(ERR, "Invalid TF feature bit is set %" PRIu64 "\n",
			     bit);
		return -EINVAL;
	}
	if ((bit & BNXT_ULP_FEATURE_BIT_PARENT_DMAC) &&
	    (bit & BNXT_ULP_FEATURE_BIT_PORT_DMAC)) {
		BNXT_DRV_DBG(ERR, "Invalid both Port and Parent Mac set\n");
		return -EINVAL;
	}

	if (bit & BNXT_ULP_FEATURE_BIT_PARENT_DMAC)
		BNXT_DRV_DBG(ERR, "Parent Mac Address Feature is enabled\n");
	else if (bit & BNXT_ULP_FEATURE_BIT_PORT_DMAC)
		BNXT_DRV_DBG(ERR, "Port Mac Address Feature is enabled\n");
	if (bit & BNXT_ULP_FEATURE_BIT_MULTI_TUNNEL_FLOW)
		BNXT_DRV_DBG(ERR, "Multi Tunnel Flow Feature is enabled\n");

	*out_bits =  bit;
	return 0;
}

#endif /* _BNXT_ULP_UTILS_H_ */
