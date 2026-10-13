/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Intel Corporation
 */

#ifndef _I40E_FLOW_H_
#define _I40E_FLOW_H_

#include "../common/flow_engine.h"

int i40e_get_outer_vlan(struct i40e_pf *pf, uint16_t *tpid);
uint8_t
i40e_flow_fdir_get_pctype_value(struct i40e_pf *pf,
		enum rte_flow_item_type item_type,
		struct i40e_fdir_filter *filter);

extern const struct ci_flow_engine_list i40e_flow_engine_list;

extern const struct ci_flow_engine i40e_flow_engine_ethertype;
extern const struct ci_flow_engine i40e_flow_engine_fdir;

#endif /* _I40E_FLOW_H_ */
