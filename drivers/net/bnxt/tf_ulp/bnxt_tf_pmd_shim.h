/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2021 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_TF_PMD_ABSTRACT_H_
#define _BNXT_TF_PMD_ABSTRACT_H_

#include "bnxt_tf_common.h"
#include "ulp_mapper.h"

int32_t bnxt_rss_config_action_apply(struct bnxt_ulp_mapper_parms *parms);
int32_t bnxt_pmd_get_parent_mac_addr(struct bnxt_ulp_mapper_parms *parms,
				     uint8_t *mac);
void bnxt_pmd_get_iface_mac(uint16_t port, enum bnxt_ulp_intf_type type,
			    uint8_t *mac, uint8_t *parent_mac);
uint16_t bnxt_pmd_get_vnic_id(uint16_t port, enum bnxt_ulp_intf_type type);
uint16_t bnxt_pmd_get_parent_vnic_id(uint16_t port, enum bnxt_ulp_intf_type type);
struct bnxt *bnxt_pmd_get_bp(uint16_t port);
uint16_t bnxt_pmd_get_svif(uint16_t port_id, bool func_svif,
			   enum bnxt_ulp_intf_type type);
uint16_t bnxt_pmd_get_fw_func_id(uint16_t port, enum bnxt_ulp_intf_type type);
uint16_t bnxt_pmd_get_parif(uint16_t port, enum bnxt_ulp_intf_type type);
uint16_t bnxt_pmd_get_phy_port_id(uint16_t port);
uint16_t bnxt_pmd_get_vport(uint16_t port);
enum bnxt_ulp_intf_type bnxt_pmd_get_interface_type(uint16_t port);
int32_t bnxt_pmd_set_unicast_rxmask(struct rte_eth_dev *eth_dev);

#endif /* _BNXT_TF_PMD_ABSTRACT_H_ */
