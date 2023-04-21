/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2023 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_TF_PMD_ABSTRACT_H_
#define _BNXT_TF_PMD_ABSTRACT_H_

#include "bnxt_tf_common.h"
#include "ulp_mapper.h"

/* Simple structure to manage the custom global tunnel */
struct bnxt_global_tunnel_info {
	uint16_t dport;
	uint16_t ref_cnt;
};

/* Internal Tunnel type, */
enum bnxt_global_register_tunnel_type {
	BNXT_GLOBAL_REGISTER_TUNNEL_UNUSED = 0,
	BNXT_GLOBAL_REGISTER_TUNNEL_VXLAN,
	BNXT_GLOBAL_REGISTER_TUNNEL_ECPRI,
	BNXT_GLOBAL_REGISTER_TUNNEL_MAX
};

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
int32_t bnxt_pmd_queue_action_create(struct bnxt_ulp_mapper_parms *parms,
				     uint16_t *vnic_idx, uint16_t *vnic_id);
int32_t bnxt_pmd_queue_action_delete(struct tf *tfp, uint16_t vnic_idx);
int32_t bnxt_pmd_rss_action_create(struct bnxt_ulp_mapper_parms *parms,
				   uint16_t *vnic_idx, uint16_t *vnic_id);
int32_t bnxt_pmd_rss_action_delete(struct tf *tfp, uint16_t vnic_idx);
int32_t bnxt_tunnel_dst_port_free(struct bnxt *bp,
				  uint16_t port,
				  uint8_t type);
int32_t bnxt_tunnel_dst_port_alloc(struct bnxt *bp,
				   uint16_t port,
				   uint8_t type);
int32_t
bnxt_pmd_global_tunnel_set(uint16_t port_id, uint8_t type,
			   uint16_t udp_port, uint32_t *handle);
int32_t
bnxt_tunnel_upar_id_get(struct bnxt *bp,
			uint8_t type,
			uint8_t *upar_id);
int32_t bnxt_pmd_get_hot_upgrade_env(void);
#endif /* _BNXT_TF_PMD_ABSTRACT_H_ */
