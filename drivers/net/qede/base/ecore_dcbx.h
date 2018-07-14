/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016 - 2018 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 */

#ifndef __ECORE_DCBX_H__
#define __ECORE_DCBX_H__

#include "ecore.h"
#include "ecore_mcp.h"
#include "mcp_public.h"
#include "reg_addr.h"
#include "ecore_hw.h"
#include "ecore_hsi_common.h"
#include "ecore_dcbx_api.h"

#define ECORE_DCBX_DSCP_DISABLED 0XFF

struct ecore_dcbx_info {
	struct lldp_status_params_s lldp_remote[LLDP_MAX_LLDP_AGENTS];
	struct lldp_config_params_s lldp_local[LLDP_MAX_LLDP_AGENTS];
	struct dcbx_local_params local_admin;
	struct ecore_dcbx_results results;
	struct dcb_dscp_map dscp_map;
	bool dscp_nig_update;
	struct dcbx_mib operational;
	struct dcbx_mib remote;
	struct ecore_dcbx_set set;
	struct ecore_dcbx_get get;
	u8 dcbx_cap;
	u16 iwarp_port;
};

struct ecore_dcbx_mib_meta_data {
	struct lldp_config_params_s *lldp_local;
	struct lldp_status_params_s *lldp_remote;
	struct lldp_received_tlvs_s *lldp_tlvs;
	struct dcbx_local_params *local_admin;
	struct dcb_dscp_map *dscp_map;
	struct dcbx_mib *mib;
	osal_size_t size;
	u32 addr;
};

/* ECORE local interface routines */
enum _ecore_status_t
ecore_dcbx_mib_update_event(struct ecore_hwfn *, struct ecore_ptt *,
			    enum ecore_mib_read_type);

enum _ecore_status_t ecore_dcbx_info_alloc(struct ecore_hwfn *p_hwfn);
void ecore_dcbx_info_free(struct ecore_hwfn *p_hwfn);
void ecore_dcbx_set_pf_update_params(struct ecore_dcbx_results *p_src,
				     struct pf_update_ramrod_data *p_dest);

/* Returns TOS value for a given priority */
u8 ecore_dcbx_get_dscp_value(struct ecore_hwfn *p_hwfn, u8 pri);

enum _ecore_status_t
ecore_lldp_mib_update_event(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt);

#endif /* __ECORE_DCBX_H__ */
