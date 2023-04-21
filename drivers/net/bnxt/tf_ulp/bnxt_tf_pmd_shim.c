/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2023 Broadcom
 * All rights reserved.
 */

#include <glob.h>
#include <libgen.h>
#include <stdio.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "bnxt.h"
#include "bnxt_vnic.h"
#include "bnxt_hwrm.h"
#include "bnxt_tf_common.h"
#include "bnxt_tf_pmd_shim.h"

int
bnxt_tunnel_dst_port_free(struct bnxt *bp,
			  uint16_t port,
			  uint8_t type)
{
	return bnxt_hwrm_tunnel_dst_port_free(bp,
					      port,
					      type);
}

int
bnxt_tunnel_dst_port_alloc(struct bnxt *bp,
			   uint16_t port,
			   uint8_t type)
{
	return bnxt_hwrm_tunnel_dst_port_alloc(bp,
					       port,
					       type);
}

int
bnxt_tunnel_upar_id_get(struct bnxt *bp,
			uint8_t type,
			uint8_t *upar_id)
{
	return bnxt_hwrm_tunnel_upar_id_get(bp,
					    upar_id,
					    type);
}

struct bnxt *
bnxt_pmd_get_bp(uint16_t port)
{
	struct bnxt *bp;
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port)) {
		PMD_DRV_LOG(ERR, "Invalid port %d\n", port);
		return NULL;
	}

	dev = &rte_eth_devices[port];
	if (!is_bnxt_supported(dev)) {
		PMD_DRV_LOG(ERR, "Device %d not supported\n", port);
		return NULL;
	}

	bp = (struct bnxt *)dev->data->dev_private;
	if (!BNXT_TRUFLOW_EN(bp)) {
		PMD_DRV_LOG(ERR, "TRUFLOW not enabled\n");
		return NULL;
	}

	return bp;
}

int32_t bnxt_rss_config_action_apply(struct bnxt_ulp_mapper_parms *parms)
{
	struct bnxt_vnic_info *vnic = NULL;
	struct bnxt *bp = NULL;
	uint64_t rss_types;
	uint16_t hwrm_type;
	uint32_t rss_level, key_len;
	uint8_t *rss_key;
	struct ulp_rte_act_prop *ap = parms->act_prop;
	int32_t rc = -EINVAL;

	bp = bnxt_pmd_get_bp(parms->port_id);
	if (bp == NULL) {
		BNXT_TF_DBG(ERR, "Invalid bp for port_id %u\n", parms->port_id);
		return rc;
	}
	vnic = bnxt_get_default_vnic(bp);
	if (vnic == NULL) {
		BNXT_TF_DBG(ERR, "default vnic not available for %u\n",
			    parms->port_id);
		return rc;
	}

	/* get the details */
	memcpy(&rss_types, &ap->act_details[BNXT_ULP_ACT_PROP_IDX_RSS_TYPES],
	       BNXT_ULP_ACT_PROP_SZ_RSS_TYPES);
	memcpy(&rss_level, &ap->act_details[BNXT_ULP_ACT_PROP_IDX_RSS_LEVEL],
	       BNXT_ULP_ACT_PROP_SZ_RSS_LEVEL);
	memcpy(&key_len, &ap->act_details[BNXT_ULP_ACT_PROP_IDX_RSS_KEY_LEN],
	       BNXT_ULP_ACT_PROP_SZ_RSS_KEY_LEN);
	rss_key = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_RSS_KEY];

	hwrm_type = bnxt_rte_to_hwrm_hash_types(rss_types);
	if (!hwrm_type) {
		BNXT_TF_DBG(ERR, "Error unsupported rss config type\n");
		return rc;
	}
	/* Configure RSS only if the queue count is > 1 */
	if (vnic->rx_queue_cnt > 1) {
		vnic->hash_type = hwrm_type;
		vnic->hash_mode =
			bnxt_rte_to_hwrm_hash_level(bp, rss_types, rss_level);
		memcpy(vnic->rss_hash_key, rss_key,
		       BNXT_ULP_ACT_PROP_SZ_RSS_KEY);
		rc = bnxt_hwrm_vnic_rss_cfg(bp, vnic);
		if (rc) {
			BNXT_TF_DBG(ERR, "Error configuring vnic RSS config\n");
			return rc;
		}
		BNXT_TF_DBG(INFO, "Rss config successfully applied\n");
	}
	return 0;
}

#define PARENT_PHY_INTF_PATH "/sys/bus/pci/devices/%s/physfn/net/*"
#define ULP_PRT_MAC_PATH "/sys/bus/pci/devices/%s/physfn/net/%s/address"

#define ULP_FILE_PATH_SIZE 256

static int32_t glob_error_fn(const char *epath, int32_t eerrno)
{
	BNXT_TF_DBG(ERR, "path %s error %d\n", epath, eerrno);
	return 0;
}

static int32_t ulp_pmd_get_mac_by_pci(const char *pci_name, uint8_t *mac)
{
	char path[ULP_FILE_PATH_SIZE], dev_str[ULP_FILE_PATH_SIZE];
	char *intf_name;
	glob_t gres;
	FILE *fp;
	int32_t rc = -EINVAL;

	memset(path, 0, sizeof(path));
	sprintf(path, PARENT_PHY_INTF_PATH, pci_name);

	/* There can be only one, no more, no less */
	if (glob(path, 0, glob_error_fn, &gres) == 0) {
		if (gres.gl_pathc != 1)
			return rc;

		/* Replace the PCI address with interface name and get index */
		intf_name = basename(gres.gl_pathv[0]);
		sprintf(path, ULP_PRT_MAC_PATH, pci_name, intf_name);

		fp = fopen(path, "r");
		if (!fp) {
			BNXT_TF_DBG(ERR, "Error in getting bond mac address\n");
			return rc;
		}

		memset(dev_str, 0, sizeof(dev_str));
		if (fgets(dev_str, sizeof(dev_str), fp) == NULL) {
			BNXT_TF_DBG(ERR, "Error in reading %s\n", path);
			fclose(fp);
			return rc;
		}

		if (sscanf(dev_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx\n",
			   &mac[0], &mac[1], &mac[2],
			   &mac[3], &mac[4], &mac[5]) == 6)
			rc = 0;
		fclose(fp);
	}
	return rc;
}

int32_t bnxt_pmd_get_parent_mac_addr(struct bnxt_ulp_mapper_parms *parms,
				     uint8_t *mac)
{
	struct bnxt *bp = NULL;
	int32_t rc = -EINVAL;

	bp = bnxt_pmd_get_bp(parms->port_id);
	if (bp == NULL) {
		BNXT_TF_DBG(ERR, "Invalid bp for port_id %u\n", parms->port_id);
		return rc;
	}
	return ulp_pmd_get_mac_by_pci(bp->pdev->name, &mac[2]);
}

uint16_t
bnxt_pmd_get_svif(uint16_t port_id, bool func_svif,
	      enum bnxt_ulp_intf_type type)
{
	struct rte_eth_dev *eth_dev;
	struct bnxt *bp;

	eth_dev = &rte_eth_devices[port_id];
	if (BNXT_ETH_DEV_IS_REPRESENTOR(eth_dev)) {
		struct bnxt_representor *vfr = eth_dev->data->dev_private;
		if (!vfr)
			return 0;

		if (type == BNXT_ULP_INTF_TYPE_VF_REP)
			return vfr->svif;

		eth_dev = vfr->parent_dev;
	}

	bp = eth_dev->data->dev_private;

	return func_svif ? bp->func_svif : bp->port_svif;
}

void
bnxt_pmd_get_iface_mac(uint16_t port, enum bnxt_ulp_intf_type type,
		       uint8_t *mac, uint8_t *parent_mac)
{
	struct rte_eth_dev *eth_dev;
	struct bnxt *bp;

	if (type != BNXT_ULP_INTF_TYPE_TRUSTED_VF &&
	    type != BNXT_ULP_INTF_TYPE_PF)
		return;

	eth_dev = &rte_eth_devices[port];
	bp = eth_dev->data->dev_private;
	memcpy(mac, bp->mac_addr, RTE_ETHER_ADDR_LEN);

	if (type == BNXT_ULP_INTF_TYPE_TRUSTED_VF)
		memcpy(parent_mac, bp->parent->mac_addr, RTE_ETHER_ADDR_LEN);
}

uint16_t
bnxt_pmd_get_parent_vnic_id(uint16_t port, enum bnxt_ulp_intf_type type)
{
	struct rte_eth_dev *eth_dev;
	struct bnxt *bp;

	if (type != BNXT_ULP_INTF_TYPE_TRUSTED_VF)
		return 0;

	eth_dev = &rte_eth_devices[port];
	bp = eth_dev->data->dev_private;

	return bp->parent->vnic;
}

uint16_t
bnxt_pmd_get_vnic_id(uint16_t port, enum bnxt_ulp_intf_type type)
{
	struct rte_eth_dev *eth_dev;
	struct bnxt_vnic_info *vnic;
	struct bnxt *bp;

	eth_dev = &rte_eth_devices[port];
	if (BNXT_ETH_DEV_IS_REPRESENTOR(eth_dev)) {
		struct bnxt_representor *vfr = eth_dev->data->dev_private;
		if (!vfr)
			return 0;

		if (type == BNXT_ULP_INTF_TYPE_VF_REP)
			return vfr->dflt_vnic_id;

		eth_dev = vfr->parent_dev;
	}

	bp = eth_dev->data->dev_private;

	vnic = bnxt_get_default_vnic(bp);

	return vnic->fw_vnic_id;
}

uint16_t
bnxt_pmd_get_fw_func_id(uint16_t port, enum bnxt_ulp_intf_type type)
{
	struct rte_eth_dev *eth_dev;
	struct bnxt *bp;

	eth_dev = &rte_eth_devices[port];
	if (BNXT_ETH_DEV_IS_REPRESENTOR(eth_dev)) {
		struct bnxt_representor *vfr = eth_dev->data->dev_private;
		if (!vfr)
			return 0;

		if (type == BNXT_ULP_INTF_TYPE_VF_REP)
			return vfr->fw_fid;

		eth_dev = vfr->parent_dev;
	}

	bp = eth_dev->data->dev_private;

	return bp->fw_fid;
}

enum bnxt_ulp_intf_type
bnxt_pmd_get_interface_type(uint16_t port)
{
	struct rte_eth_dev *eth_dev;
	struct bnxt *bp;

	eth_dev = &rte_eth_devices[port];
	if (BNXT_ETH_DEV_IS_REPRESENTOR(eth_dev))
		return BNXT_ULP_INTF_TYPE_VF_REP;

	bp = eth_dev->data->dev_private;
	if (BNXT_PF(bp))
		return BNXT_ULP_INTF_TYPE_PF;
	else if (BNXT_VF_IS_TRUSTED(bp))
		return BNXT_ULP_INTF_TYPE_TRUSTED_VF;
	else if (BNXT_VF(bp))
		return BNXT_ULP_INTF_TYPE_VF;

	return BNXT_ULP_INTF_TYPE_INVALID;
}

uint16_t
bnxt_pmd_get_phy_port_id(uint16_t port_id)
{
	struct bnxt_representor *vfr;
	struct rte_eth_dev *eth_dev;
	struct bnxt *bp;

	eth_dev = &rte_eth_devices[port_id];
	if (BNXT_ETH_DEV_IS_REPRESENTOR(eth_dev)) {
		vfr = eth_dev->data->dev_private;
		if (!vfr)
			return 0;

		eth_dev = vfr->parent_dev;
	}

	bp = eth_dev->data->dev_private;

	return BNXT_PF(bp) ? bp->pf->port_id : bp->parent->port_id;
}

uint16_t
bnxt_pmd_get_parif(uint16_t port_id, enum bnxt_ulp_intf_type type)
{
	struct rte_eth_dev *eth_dev;
	struct bnxt *bp;

	eth_dev = &rte_eth_devices[port_id];
	if (BNXT_ETH_DEV_IS_REPRESENTOR(eth_dev)) {
		struct bnxt_representor *vfr = eth_dev->data->dev_private;
		if (!vfr)
			return 0;

		if (type == BNXT_ULP_INTF_TYPE_VF_REP)
			return vfr->fw_fid - 1;

		eth_dev = vfr->parent_dev;
	}

	bp = eth_dev->data->dev_private;

	return BNXT_PF(bp) ? bp->fw_fid - 1 : bp->parent->fid - 1;
}

uint16_t
bnxt_pmd_get_vport(uint16_t port_id)
{
	return (1 << bnxt_pmd_get_phy_port_id(port_id));
}

int32_t
bnxt_pmd_set_unicast_rxmask(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = eth_dev->data->dev_private;
	struct bnxt_vnic_info *vnic;
	uint32_t old_flags;
	int32_t rc;

	rc = is_bnxt_in_error(bp);
	if (rc)
		return rc;

	/* Filter settings will get applied when port is started */
	if (!eth_dev->data->dev_started)
		return 0;

	if (bp->vnic_info == NULL)
		return 0;

	vnic = bnxt_get_default_vnic(bp);

	old_flags = vnic->flags;
	vnic->flags |= BNXT_VNIC_INFO_UCAST;
	vnic->flags &= ~BNXT_VNIC_INFO_PROMISC;
	vnic->flags &= ~BNXT_VNIC_INFO_ALLMULTI;
	vnic->flags &= ~BNXT_VNIC_INFO_BCAST;
	rc = bnxt_hwrm_cfa_l2_set_rx_mask(bp, vnic, 0, NULL);
	if (rc != 0)
		vnic->flags = old_flags;

	return rc;
}

int32_t bnxt_pmd_queue_action_create(struct bnxt_ulp_mapper_parms *parms,
				     uint16_t *vnic_idx, uint16_t *vnic_id)
{
	struct bnxt *bp = NULL;
	uint16_t q_index;
	struct ulp_rte_act_prop *ap = parms->act_prop;

	bp = bnxt_pmd_get_bp(parms->port_id);
	if (bp == NULL) {
		BNXT_TF_DBG(ERR, "Invalid bp for port_id %u\n", parms->port_id);
		return -EINVAL;
	}

	memcpy(&q_index, &ap->act_details[BNXT_ULP_ACT_PROP_IDX_QUEUE_INDEX],
	       BNXT_ULP_ACT_PROP_SZ_QUEUE_INDEX);

	return bnxt_vnic_queue_action_alloc(bp, q_index, vnic_idx, vnic_id);
}

int32_t bnxt_pmd_queue_action_delete(struct tf *tfp, uint16_t vnic_idx)
{
	struct bnxt *bp = NULL;

	bp = tfp->bp;
	if (bp == NULL) {
		BNXT_TF_DBG(ERR, "Invalid bp\n");
		return -EINVAL;
	}
	return bnxt_vnic_queue_action_free(bp, vnic_idx);
}

int32_t bnxt_pmd_rss_action_create(struct bnxt_ulp_mapper_parms *parms,
				   uint16_t *vnic_idx, uint16_t *vnic_id)
{
	struct bnxt *bp = NULL;
	struct bnxt_vnic_rss_info rss_info = {0};
	struct ulp_rte_act_prop *ap = parms->act_prop;

	bp = bnxt_pmd_get_bp(parms->port_id);
	if (bp == NULL) {
		BNXT_TF_DBG(ERR, "Invalid bp for port_id %u\n", parms->port_id);
		return -EINVAL;
	}

	/* get the details */
	memset(&rss_info, 0, sizeof(rss_info));
	memcpy(&rss_info.rss_types,
	       &ap->act_details[BNXT_ULP_ACT_PROP_IDX_RSS_TYPES],
	       BNXT_ULP_ACT_PROP_SZ_RSS_TYPES);
	memcpy(&rss_info.rss_level,
	       &ap->act_details[BNXT_ULP_ACT_PROP_IDX_RSS_LEVEL],
	       BNXT_ULP_ACT_PROP_SZ_RSS_LEVEL);
	memcpy(&rss_info.key_len,
	       &ap->act_details[BNXT_ULP_ACT_PROP_IDX_RSS_KEY_LEN],
	       BNXT_ULP_ACT_PROP_SZ_RSS_KEY_LEN);
	if (rss_info.key_len)
		rss_info.key = &ap->act_details[BNXT_ULP_ACT_PROP_IDX_RSS_KEY];
	memcpy(&rss_info.queue_num,
	       &ap->act_details[BNXT_ULP_ACT_PROP_IDX_RSS_QUEUE_NUM],
	       BNXT_ULP_ACT_PROP_SZ_RSS_QUEUE_NUM);

	/* Validate the size of the queue list */
	if (sizeof(rss_info.queue_list) < BNXT_ULP_ACT_PROP_SZ_RSS_QUEUE) {
		BNXT_TF_DBG(ERR, "Mismatch of RSS queue size in template\n");
		return -EINVAL;
	}
	memcpy(rss_info.queue_list,
	       &ap->act_details[BNXT_ULP_ACT_PROP_IDX_RSS_QUEUE],
	       BNXT_ULP_ACT_PROP_SZ_RSS_QUEUE);

	return bnxt_vnic_rss_action_alloc(bp, &rss_info, vnic_idx, vnic_id);
}

int32_t bnxt_pmd_rss_action_delete(struct tf *tfp, uint16_t vnic_idx)
{
	struct bnxt *bp = tfp->bp;

	if (bp == NULL) {
		BNXT_TF_DBG(ERR, "Invalid bp\n");
		return -EINVAL;
	}
	return bnxt_vnic_rss_action_free(bp, vnic_idx);
}

#define ULP_GLOBAL_TUNNEL_PORT_ID_SHIFT  16
#define ULP_GLOBAL_TUNNEL_PORT_ID_MASK   ((uint16_t)0xffff)
#define ULP_GLOBAL_TUNNEL_UPARID_SHIFT   8
#define ULP_GLOBAL_TUNNEL_UPARID_MASK    ((uint16_t)0xff)
#define ULP_GLOBAL_TUNNEL_TYPE_SHIFT     0
#define ULP_GLOBAL_TUNNEL_TYPE_MASK      ((uint16_t)0xffff)

/* Extracts the dpdk port id and tunnel type from the handle */
static void
bnxt_pmd_global_reg_hndl_to_data(uint32_t handle, uint16_t *port,
				 uint8_t *upar_id, uint8_t *type)
{
	*type    = (handle >> ULP_GLOBAL_TUNNEL_TYPE_SHIFT) &
		   ULP_GLOBAL_TUNNEL_TYPE_MASK;
	*upar_id = (handle >> ULP_GLOBAL_TUNNEL_UPARID_SHIFT) &
		   ULP_GLOBAL_TUNNEL_UPARID_MASK;
	*port    = (handle >> ULP_GLOBAL_TUNNEL_PORT_ID_SHIFT) &
		   ULP_GLOBAL_TUNNEL_PORT_ID_MASK;
}

/* Packs the dpdk port id and tunnel type in the handle */
static void
bnxt_pmd_global_reg_data_to_hndl(uint16_t port_id, uint8_t upar_id,
				 uint8_t type, uint32_t *handle)
{
	*handle	=  (port_id & ULP_GLOBAL_TUNNEL_PORT_ID_MASK) <<
		   ULP_GLOBAL_TUNNEL_PORT_ID_SHIFT;
	*handle	|= (upar_id & ULP_GLOBAL_TUNNEL_UPARID_MASK) <<
		   ULP_GLOBAL_TUNNEL_UPARID_SHIFT;
	*handle |= (type & ULP_GLOBAL_TUNNEL_TYPE_MASK) <<
		   ULP_GLOBAL_TUNNEL_TYPE_SHIFT;
}

static struct bnxt_global_tunnel_info
	      ulp_global_tunnel_db[BNXT_GLOBAL_REGISTER_TUNNEL_MAX] = {{0}};
/* Sets or resets the tunnel ports.
 * If dport == 0, then the port_id and type are retrieved from the handle.
 * otherwise, the incoming port_id, type, and dport are used.
 * The type is enum ulp_mapper_ulp_global_tunnel_type
 */
int32_t
bnxt_pmd_global_tunnel_set(uint16_t port_id, uint8_t type,
			   uint16_t udp_port, uint32_t *handle)
{
	uint16_t lport_id, ldport;
	uint8_t hwtype, ltype, lupar_id;
	struct bnxt *bp;
	int32_t rc = 0;

	/* convert to HWRM type */
	switch (type) {
	case BNXT_GLOBAL_REGISTER_TUNNEL_VXLAN:
		hwtype = HWRM_TUNNEL_DST_PORT_ALLOC_INPUT_TUNNEL_TYPE_VXLAN;
		break;
	case BNXT_GLOBAL_REGISTER_TUNNEL_ECPRI:
		hwtype = HWRM_TUNNEL_DST_PORT_ALLOC_INPUT_TUNNEL_TYPE_ECPRI;
		break;
	default:
		BNXT_TF_DBG(ERR, "Tunnel Type (%d) invalid\n", type);
		return -EINVAL;
	}

	if (!udp_port && type != BNXT_GLOBAL_REGISTER_TUNNEL_ECPRI) {
		/* Free based on the handle */
		if (!handle) {
			BNXT_TF_DBG(ERR, "Free with invalid handle\n");
			return -EINVAL;
		}
		bnxt_pmd_global_reg_hndl_to_data(*handle, &lport_id,
						  &lupar_id, &ltype);

		bp = bnxt_pmd_get_bp(lport_id);
		if (!bp) {
			BNXT_TF_DBG(ERR, "Unable to get dev by port %d\n",
				    lport_id);
			return -EINVAL;
		}

		if (!ulp_global_tunnel_db[ltype].ref_cnt)
			return 0;
		ldport = ulp_global_tunnel_db[ltype].dport;
		rc = bnxt_hwrm_tunnel_dst_port_free(bp, ldport, hwtype);
		if (rc) {
			BNXT_TF_DBG(ERR,
				    "Unable to free tunnel dst port (%d)\n",
				    ldport);
			return rc;
		}
		ulp_global_tunnel_db[ltype].ref_cnt--;
		if (ulp_global_tunnel_db[ltype].ref_cnt == 0)
			ulp_global_tunnel_db[ltype].dport = 0;
	} else {
		bp = bnxt_pmd_get_bp(port_id);
		if (!bp) {
			BNXT_TF_DBG(ERR, "Unable to get dev by port %d\n",
				    port_id);
			return -EINVAL;
		}

		rc = bnxt_hwrm_tunnel_dst_port_alloc(bp, udp_port, hwtype);
		if (!rc) {
			ulp_global_tunnel_db[type].ref_cnt++;
			ulp_global_tunnel_db[type].dport = udp_port;
			bnxt_pmd_global_reg_data_to_hndl(port_id, bp->ecpri_upar_in_use,
							 type, handle);
		}
	}
	return rc;
}

#define BNXT_ULP_HOT_UP_DYNAMIC_ENV_VAR "BNXT_ULP_T_HA_SUPPORT"
/* This function queries the linux shell variable to determine
 * whether Hot upgrade should be disabled or not.
 * If BNXT_ULP_T_HA_SUPPORT is set to zero explicitly then
 * hotupgrade is disabled.
 */
int32_t bnxt_pmd_get_hot_upgrade_env(void)
{
	char *env;
	int32_t hot_up = 1;

	env = getenv(BNXT_ULP_HOT_UP_DYNAMIC_ENV_VAR);
	if (env && strcmp(env, "0") == 0)
		hot_up = 0;
	return hot_up;
}
