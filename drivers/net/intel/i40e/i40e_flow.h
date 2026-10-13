/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Intel Corporation
 */

#ifndef _I40E_FLOW_H_
#define _I40E_FLOW_H_

#include "../common/flow_engine.h"

int i40e_get_outer_vlan(struct i40e_pf *pf, uint16_t *tpid);

extern const struct ci_flow_engine_list i40e_flow_engine_list;

extern const struct ci_flow_engine i40e_flow_engine_ethertype;

#endif /* _I40E_FLOW_H_ */
