/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _IFPGA_API_H_
#define _IFPGA_API_H_

#include "opae_hw_api.h"
#include "ifpga_hw.h"

extern struct opae_adapter_ops ifpga_adapter_ops;
extern struct opae_manager_ops ifpga_mgr_ops;
extern struct opae_bridge_ops ifpga_br_ops;
extern struct opae_accelerator_ops ifpga_acc_ops;
extern struct opae_manager_networking_ops ifpga_mgr_network_ops;

/* common APIs */
int ifpga_get_prop(struct ifpga_hw *hw, u32 fiu_id, u32 port_id,
		   struct feature_prop *prop);
int ifpga_set_prop(struct ifpga_hw *hw, u32 fiu_id, u32 port_id,
		   struct feature_prop *prop);
int ifpga_set_irq(struct ifpga_hw *hw, u32 fiu_id, u32 port_id,
		  u32 feature_id, void *irq_set);

/* FME APIs */
int ifpga_pr(struct ifpga_hw *hw, u32 port_id, void *buffer, u32 size,
	     u64 *status);

#endif /* _IFPGA_API_H_ */
