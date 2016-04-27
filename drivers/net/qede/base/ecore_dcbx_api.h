/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef __ECORE_DCBX_API_H__
#define __ECORE_DCBX_API_H__

#include "ecore.h"

#define DCBX_CONFIG_MAX_APP_PROTOCOL	4

enum ecore_mib_read_type {
	ECORE_DCBX_OPERATIONAL_MIB,
	ECORE_DCBX_REMOTE_MIB,
	ECORE_DCBX_LOCAL_MIB,
	ECORE_DCBX_REMOTE_LLDP_MIB,
	ECORE_DCBX_LOCAL_LLDP_MIB
};

struct ecore_dcbx_app_data {
	bool enable;		/* DCB enabled */
	bool update;		/* Update indication */
	u8 priority;		/* Priority */
	u8 tc;			/* Traffic Class */
};

#ifndef __EXTRACT__LINUX__
enum dcbx_protocol_type {
	DCBX_PROTOCOL_ETH,
	DCBX_MAX_PROTOCOL_TYPE
};

#ifdef LINUX_REMOVE
/* We can't assume THE HSI values are available to clients, so we need
 * to redefine those here.
 */
#ifndef LLDP_CHASSIS_ID_STAT_LEN
#define LLDP_CHASSIS_ID_STAT_LEN 4
#endif
#ifndef LLDP_PORT_ID_STAT_LEN
#define LLDP_PORT_ID_STAT_LEN 4
#endif
#ifndef DCBX_MAX_APP_PROTOCOL
#define DCBX_MAX_APP_PROTOCOL 32
#endif

#endif

struct ecore_dcbx_lldp_remote {
	u32 peer_chassis_id[LLDP_CHASSIS_ID_STAT_LEN];
	u32 peer_port_id[LLDP_PORT_ID_STAT_LEN];
	bool enable_rx;
	bool enable_tx;
	u32 tx_interval;
	u32 max_credit;
};

struct ecore_dcbx_lldp_local {
	u32 local_chassis_id[LLDP_CHASSIS_ID_STAT_LEN];
	u32 local_port_id[LLDP_PORT_ID_STAT_LEN];
};

struct ecore_dcbx_app_prio {
	u8 eth;
};

struct ecore_dcbx_params {
	u32 app_bitmap[DCBX_MAX_APP_PROTOCOL];
	u16 num_app_entries;
	bool app_willing;
	bool app_valid;
	bool ets_willing;
	bool ets_enabled;
	bool valid;		/* Indicate validity of params */
	u32 ets_pri_tc_tbl[1];
	u32 ets_tc_bw_tbl[2];
	u32 ets_tc_tsa_tbl[2];
	bool pfc_willing;
	bool pfc_enabled;
	u32 pfc_bitmap;
	u8 max_pfc_tc;
	u8 max_ets_tc;
};

struct ecore_dcbx_admin_params {
	struct ecore_dcbx_params params;
	bool valid;		/* Indicate validity of params */
};

struct ecore_dcbx_remote_params {
	struct ecore_dcbx_params params;
	bool valid;		/* Indicate validity of params */
};

struct ecore_dcbx_operational_params {
	struct ecore_dcbx_app_prio app_prio;
	struct ecore_dcbx_params params;
	bool valid;		/* Indicate validity of params */
	bool enabled;
	bool ieee;
	bool cee;
	u32 err;
};

struct ecore_dcbx_get {
	struct ecore_dcbx_operational_params operational;
	struct ecore_dcbx_lldp_remote lldp_remote;
	struct ecore_dcbx_lldp_local lldp_local;
	struct ecore_dcbx_remote_params remote;
	struct ecore_dcbx_admin_params local;
};
#endif

struct ecore_dcbx_set {
	struct ecore_dcbx_admin_params config;
	bool enabled;
	u32 ver_num;
};

struct ecore_dcbx_results {
	bool dcbx_enabled;
	u8 pf_id;
	struct ecore_dcbx_app_data arr[DCBX_MAX_PROTOCOL_TYPE];
};

struct ecore_dcbx_app_metadata {
	enum dcbx_protocol_type id;
	const char *name;	/* @DPDK */
	enum ecore_pci_personality personality;
};

struct ecore_dcbx_mib_meta_data {
	struct lldp_config_params_s *lldp_local;
	struct lldp_status_params_s *lldp_remote;
	struct dcbx_local_params *local_admin;
	struct dcbx_mib *mib;
	osal_size_t size;
	u32 addr;
};

void
ecore_dcbx_set_params(struct ecore_dcbx_results *p_data,
		      struct ecore_hw_info *p_info,
		      bool enable, bool update, u8 prio, u8 tc,
		      enum dcbx_protocol_type type,
		      enum ecore_pci_personality personality);

enum _ecore_status_t ecore_dcbx_query_params(struct ecore_hwfn *,
					     struct ecore_dcbx_get *,
					     enum ecore_mib_read_type);

static const struct ecore_dcbx_app_metadata ecore_dcbx_app_update[] = {
	{DCBX_PROTOCOL_ETH, "ETH", ECORE_PCI_ETH}
};

#endif /* __ECORE_DCBX_API_H__ */
