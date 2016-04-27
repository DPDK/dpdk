/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
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

#define ECORE_MFW_GET_FIELD(name, field) \
	(((name) & (field ## _MASK)) >> (field ## _SHIFT))

struct ecore_dcbx_info {
	struct lldp_status_params_s lldp_remote[LLDP_MAX_LLDP_AGENTS];
	struct lldp_config_params_s lldp_local[LLDP_MAX_LLDP_AGENTS];
	struct dcbx_local_params local_admin;
	struct ecore_dcbx_results results;
	struct dcbx_mib operational;
	struct dcbx_mib remote;
	struct ecore_dcbx_set set;
	struct ecore_dcbx_get get;
	u8 dcbx_cap;
};

/* Upper layer driver interface routines */
enum _ecore_status_t ecore_dcbx_config_params(struct ecore_hwfn *,
					      struct ecore_ptt *,
					      struct ecore_dcbx_set *);

/* ECORE local interface routines */
enum _ecore_status_t
ecore_dcbx_mib_update_event(struct ecore_hwfn *, struct ecore_ptt *,
			    enum ecore_mib_read_type);

enum _ecore_status_t ecore_dcbx_read_lldp_params(struct ecore_hwfn *,
						 struct ecore_ptt *);
enum _ecore_status_t ecore_dcbx_info_alloc(struct ecore_hwfn *p_hwfn);
void ecore_dcbx_info_free(struct ecore_hwfn *, struct ecore_dcbx_info *);
void ecore_dcbx_set_pf_update_params(struct ecore_dcbx_results *p_src,
				     struct pf_update_ramrod_data *p_dest);
/* @@@TBD eagle phy workaround */
void ecore_dcbx_eagle_workaround(struct ecore_hwfn *, struct ecore_ptt *,
				 bool set_to_pfc);

#endif /* __ECORE_DCBX_H__ */
