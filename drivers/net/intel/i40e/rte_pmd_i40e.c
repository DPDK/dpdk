/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <rte_string_fns.h>
#include <rte_malloc.h>
#include <rte_tailq.h>

#include "base/i40e_prototype.h"
#include "base/i40e_dcb.h"
#include "i40e_ethdev.h"
#include "i40e_pf.h"
#include "i40e_rxtx.h"
#include "rte_pmd_i40e.h"

int
rte_pmd_i40e_ping_vfs(uint16_t port, uint16_t vf)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	if (vf >= pf->vf_num || !pf->vfs) {
		PMD_DRV_LOG(ERR, "Invalid argument.");
		return -EINVAL;
	}

	i40e_notify_vf_link_status(dev, &pf->vfs[vf]);

	return 0;
}

int
rte_pmd_i40e_set_vf_mac_anti_spoof(uint16_t port, uint16_t vf_id, uint8_t on)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;
	struct i40e_vsi *vsi;
	struct i40e_hw *hw;
	struct i40e_vsi_context ctxt;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	if (vf_id >= pf->vf_num || !pf->vfs) {
		PMD_DRV_LOG(ERR, "Invalid argument.");
		return -EINVAL;
	}

	vsi = pf->vfs[vf_id].vsi;
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Invalid VSI.");
		return -EINVAL;
	}

	/* Check if it has been already on or off */
	if (vsi->info.valid_sections &
		rte_cpu_to_le_16(I40E_AQ_VSI_PROP_SECURITY_VALID)) {
		if (on) {
			if ((vsi->info.sec_flags &
			     I40E_AQ_VSI_SEC_FLAG_ENABLE_MAC_CHK) ==
			    I40E_AQ_VSI_SEC_FLAG_ENABLE_MAC_CHK)
				return 0; /* already on */
		} else {
			if ((vsi->info.sec_flags &
			     I40E_AQ_VSI_SEC_FLAG_ENABLE_MAC_CHK) == 0)
				return 0; /* already off */
		}
	}

	vsi->info.valid_sections = cpu_to_le16(I40E_AQ_VSI_PROP_SECURITY_VALID);
	if (on)
		vsi->info.sec_flags |= I40E_AQ_VSI_SEC_FLAG_ENABLE_MAC_CHK;
	else
		vsi->info.sec_flags &= ~I40E_AQ_VSI_SEC_FLAG_ENABLE_MAC_CHK;

	memset(&ctxt, 0, sizeof(ctxt));
	rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
	ctxt.seid = vsi->seid;

	hw = I40E_VSI_TO_HW(vsi);
	ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
	if (ret != I40E_SUCCESS) {
		ret = -ENOTSUP;
		PMD_DRV_LOG(ERR, "Failed to update VSI params");
	}

	return ret;
}

static int
i40e_add_rm_all_vlan_filter(struct i40e_vsi *vsi, uint8_t add)
{
	uint32_t j, k;
	uint16_t vlan_id;
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	struct i40e_aqc_add_remove_vlan_element_data vlan_data = {0};
	int ret;

	for (j = 0; j < I40E_VFTA_SIZE; j++) {
		if (!vsi->vfta[j])
			continue;

		for (k = 0; k < I40E_UINT32_BIT_SIZE; k++) {
			if (!(vsi->vfta[j] & (1 << k)))
				continue;

			vlan_id = j * I40E_UINT32_BIT_SIZE + k;
			if (!vlan_id)
				continue;

			vlan_data.vlan_tag = rte_cpu_to_le_16(vlan_id);
			if (add)
				ret = i40e_aq_add_vlan(hw, vsi->seid,
						       &vlan_data, 1, NULL);
			else
				ret = i40e_aq_remove_vlan(hw, vsi->seid,
							  &vlan_data, 1, NULL);
			if (ret != I40E_SUCCESS) {
				PMD_DRV_LOG(ERR,
					    "Failed to add/rm vlan filter");
				return ret;
			}
		}
	}

	return I40E_SUCCESS;
}

int
rte_pmd_i40e_set_vf_vlan_anti_spoof(uint16_t port, uint16_t vf_id, uint8_t on)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;
	struct i40e_vsi *vsi;
	struct i40e_hw *hw;
	struct i40e_vsi_context ctxt;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	if (vf_id >= pf->vf_num || !pf->vfs) {
		PMD_DRV_LOG(ERR, "Invalid argument.");
		return -EINVAL;
	}

	vsi = pf->vfs[vf_id].vsi;
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Invalid VSI.");
		return -EINVAL;
	}

	/* Check if it has been already on or off */
	if (vsi->vlan_anti_spoof_on == on)
		return 0; /* already on or off */

	vsi->vlan_anti_spoof_on = on;
	if (!vsi->vlan_filter_on) {
		ret = i40e_add_rm_all_vlan_filter(vsi, on);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to add/remove VLAN filters.");
			return -ENOTSUP;
		}
	}

	vsi->info.valid_sections = cpu_to_le16(I40E_AQ_VSI_PROP_SECURITY_VALID);
	if (on)
		vsi->info.sec_flags |= I40E_AQ_VSI_SEC_FLAG_ENABLE_VLAN_CHK;
	else
		vsi->info.sec_flags &= ~I40E_AQ_VSI_SEC_FLAG_ENABLE_VLAN_CHK;

	memset(&ctxt, 0, sizeof(ctxt));
	rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
	ctxt.seid = vsi->seid;

	hw = I40E_VSI_TO_HW(vsi);
	ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
	if (ret != I40E_SUCCESS) {
		ret = -ENOTSUP;
		PMD_DRV_LOG(ERR, "Failed to update VSI params");
	}

	return ret;
}

static int
i40e_vsi_rm_mac_filter(struct i40e_vsi *vsi)
{
	struct i40e_mac_filter *f;
	struct i40e_macvlan_filter *mv_f;
	int i, vlan_num;
	enum i40e_mac_filter_type filter_type;
	int ret = I40E_SUCCESS;
	void *temp;

	/* remove all the MACs */
	RTE_TAILQ_FOREACH_SAFE(f, &vsi->mac_list, next, temp) {
		vlan_num = vsi->vlan_num;
		filter_type = f->mac_info.filter_type;
		if (filter_type == I40E_MACVLAN_PERFECT_MATCH ||
		    filter_type == I40E_MACVLAN_HASH_MATCH) {
			if (vlan_num == 0) {
				PMD_DRV_LOG(ERR, "VLAN number shouldn't be 0");
				return I40E_ERR_PARAM;
			}
		} else if (filter_type == I40E_MAC_PERFECT_MATCH ||
			   filter_type == I40E_MAC_HASH_MATCH)
			vlan_num = 1;

		mv_f = rte_zmalloc("macvlan_data", vlan_num * sizeof(*mv_f), 0);
		if (!mv_f) {
			PMD_DRV_LOG(ERR, "failed to allocate memory");
			return I40E_ERR_NO_MEMORY;
		}

		for (i = 0; i < vlan_num; i++) {
			mv_f[i].filter_type = filter_type;
			rte_memcpy(&mv_f[i].macaddr,
					 &f->mac_info.mac_addr,
					 ETH_ADDR_LEN);
		}
		if (filter_type == I40E_MACVLAN_PERFECT_MATCH ||
		    filter_type == I40E_MACVLAN_HASH_MATCH) {
			ret = i40e_find_all_vlan_for_mac(vsi, mv_f, vlan_num,
							 &f->mac_info.mac_addr);
			if (ret != I40E_SUCCESS) {
				rte_free(mv_f);
				return ret;
			}
		}

		ret = i40e_remove_macvlan_filters(vsi, mv_f, vlan_num);
		if (ret != I40E_SUCCESS) {
			rte_free(mv_f);
			return ret;
		}

		rte_free(mv_f);
		ret = I40E_SUCCESS;
	}

	return ret;
}

static int
i40e_vsi_restore_mac_filter(struct i40e_vsi *vsi)
{
	struct i40e_mac_filter *f;
	struct i40e_macvlan_filter *mv_f;
	int i, vlan_num = 0;
	int ret = I40E_SUCCESS;
	void *temp;

	/* restore all the MACs */
	RTE_TAILQ_FOREACH_SAFE(f, &vsi->mac_list, next, temp) {
		if (f->mac_info.filter_type == I40E_MACVLAN_PERFECT_MATCH ||
		    f->mac_info.filter_type == I40E_MACVLAN_HASH_MATCH) {
			/**
			 * If vlan_num is 0, that's the first time to add mac,
			 * set mask for vlan_id 0.
			 */
			if (vsi->vlan_num == 0) {
				i40e_set_vlan_filter(vsi, 0, 1);
				vsi->vlan_num = 1;
			}
			vlan_num = vsi->vlan_num;
		} else if (f->mac_info.filter_type == I40E_MAC_PERFECT_MATCH ||
			   f->mac_info.filter_type == I40E_MAC_HASH_MATCH)
			vlan_num = 1;

		mv_f = rte_zmalloc("macvlan_data", vlan_num * sizeof(*mv_f), 0);
		if (!mv_f) {
			PMD_DRV_LOG(ERR, "failed to allocate memory");
			return I40E_ERR_NO_MEMORY;
		}

		for (i = 0; i < vlan_num; i++) {
			mv_f[i].filter_type = f->mac_info.filter_type;
			rte_memcpy(&mv_f[i].macaddr,
					 &f->mac_info.mac_addr,
					 ETH_ADDR_LEN);
		}

		if (f->mac_info.filter_type == I40E_MACVLAN_PERFECT_MATCH ||
		    f->mac_info.filter_type == I40E_MACVLAN_HASH_MATCH) {
			ret = i40e_find_all_vlan_for_mac(vsi, mv_f, vlan_num,
							 &f->mac_info.mac_addr);
			if (ret != I40E_SUCCESS) {
				rte_free(mv_f);
				return ret;
			}
		}

		ret = i40e_add_macvlan_filters(vsi, mv_f, vlan_num);
		if (ret != I40E_SUCCESS) {
			rte_free(mv_f);
			return ret;
		}

		rte_free(mv_f);
		ret = I40E_SUCCESS;
	}

	return ret;
}

static int
i40e_vsi_set_tx_loopback(struct i40e_vsi *vsi, uint8_t on)
{
	struct i40e_vsi_context ctxt;
	struct i40e_hw *hw;
	int ret;

	if (!vsi)
		return -EINVAL;

	hw = I40E_VSI_TO_HW(vsi);

	/* Use the FW API if FW >= v5.0 */
	if (hw->aq.fw_maj_ver < 5 && hw->mac.type != I40E_MAC_X722) {
		PMD_INIT_LOG(ERR, "FW < v5.0, cannot enable loopback");
		return -ENOTSUP;
	}

	/* Check if it has been already on or off */
	if (vsi->info.valid_sections &
		rte_cpu_to_le_16(I40E_AQ_VSI_PROP_SWITCH_VALID)) {
		if (on) {
			if ((vsi->info.switch_id &
			     I40E_AQ_VSI_SW_ID_FLAG_ALLOW_LB) ==
			    I40E_AQ_VSI_SW_ID_FLAG_ALLOW_LB)
				return 0; /* already on */
		} else {
			if ((vsi->info.switch_id &
			     I40E_AQ_VSI_SW_ID_FLAG_ALLOW_LB) == 0)
				return 0; /* already off */
		}
	}

	/* remove all the MAC and VLAN first */
	ret = i40e_vsi_rm_mac_filter(vsi);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to remove MAC filters.");
		return ret;
	}
	if (vsi->vlan_anti_spoof_on || vsi->vlan_filter_on) {
		ret = i40e_add_rm_all_vlan_filter(vsi, 0);
		if (ret) {
			PMD_INIT_LOG(ERR, "Failed to remove VLAN filters.");
			return ret;
		}
	}

	vsi->info.valid_sections = cpu_to_le16(I40E_AQ_VSI_PROP_SWITCH_VALID);
	if (on)
		vsi->info.switch_id |= I40E_AQ_VSI_SW_ID_FLAG_ALLOW_LB;
	else
		vsi->info.switch_id &= ~I40E_AQ_VSI_SW_ID_FLAG_ALLOW_LB;

	memset(&ctxt, 0, sizeof(ctxt));
	rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
	ctxt.seid = vsi->seid;

	ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to update VSI params");
		return ret;
	}

	/* add all the MAC and VLAN back */
	ret = i40e_vsi_restore_mac_filter(vsi);
	if (ret)
		return ret;
	if (vsi->vlan_anti_spoof_on || vsi->vlan_filter_on) {
		ret = i40e_add_rm_all_vlan_filter(vsi, 1);
		if (ret)
			return ret;
	}

	return ret;
}

int
rte_pmd_i40e_set_tx_loopback(uint16_t port, uint8_t on)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;
	struct i40e_pf_vf *vf;
	struct i40e_vsi *vsi;
	uint16_t vf_id;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	/* setup PF TX loopback */
	vsi = pf->main_vsi;
	ret = i40e_vsi_set_tx_loopback(vsi, on);
	if (ret)
		return -ENOTSUP;

	/* setup TX loopback for all the VFs */
	if (!pf->vfs) {
		/* if no VF, do nothing. */
		return 0;
	}

	for (vf_id = 0; vf_id < pf->vf_num; vf_id++) {
		vf = &pf->vfs[vf_id];
		vsi = vf->vsi;

		ret = i40e_vsi_set_tx_loopback(vsi, on);
		if (ret)
			return -ENOTSUP;
	}

	return ret;
}

int
rte_pmd_i40e_set_vf_unicast_promisc(uint16_t port, uint16_t vf_id, uint8_t on)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;
	struct i40e_vsi *vsi;
	struct i40e_hw *hw;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	if (vf_id >= pf->vf_num || !pf->vfs) {
		PMD_DRV_LOG(ERR, "Invalid argument.");
		return -EINVAL;
	}

	vsi = pf->vfs[vf_id].vsi;
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Invalid VSI.");
		return -EINVAL;
	}

	hw = I40E_VSI_TO_HW(vsi);

	ret = i40e_aq_set_vsi_unicast_promiscuous(hw, vsi->seid,
						  on, NULL, true);
	if (ret != I40E_SUCCESS) {
		ret = -ENOTSUP;
		PMD_DRV_LOG(ERR, "Failed to set unicast promiscuous mode");
	}

	return ret;
}

int
rte_pmd_i40e_set_vf_multicast_promisc(uint16_t port, uint16_t vf_id, uint8_t on)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;
	struct i40e_vsi *vsi;
	struct i40e_hw *hw;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	if (vf_id >= pf->vf_num || !pf->vfs) {
		PMD_DRV_LOG(ERR, "Invalid argument.");
		return -EINVAL;
	}

	vsi = pf->vfs[vf_id].vsi;
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Invalid VSI.");
		return -EINVAL;
	}

	hw = I40E_VSI_TO_HW(vsi);

	ret = i40e_aq_set_vsi_multicast_promiscuous(hw, vsi->seid,
						    on, NULL);
	if (ret != I40E_SUCCESS) {
		ret = -ENOTSUP;
		PMD_DRV_LOG(ERR, "Failed to set multicast promiscuous mode");
	}

	return ret;
}

int
rte_pmd_i40e_set_vf_mac_addr(uint16_t port, uint16_t vf_id,
			     struct rte_ether_addr *mac_addr)
{
	struct i40e_mac_filter *f;
	struct rte_eth_dev *dev;
	struct i40e_pf_vf *vf;
	struct i40e_vsi *vsi;
	struct i40e_pf *pf;
	void *temp;

	if (i40e_validate_mac_addr((u8 *)mac_addr) != I40E_SUCCESS)
		return -EINVAL;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	if (vf_id >= pf->vf_num || !pf->vfs)
		return -EINVAL;

	vf = &pf->vfs[vf_id];
	vsi = vf->vsi;
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Invalid VSI.");
		return -EINVAL;
	}

	rte_ether_addr_copy(mac_addr, &vf->mac_addr);

	/* Remove all existing mac */
	RTE_TAILQ_FOREACH_SAFE(f, &vsi->mac_list, next, temp)
		if (i40e_vsi_delete_mac(vsi, &f->mac_info.mac_addr)
				!= I40E_SUCCESS)
			PMD_DRV_LOG(WARNING, "Delete MAC failed");

	return 0;
}

static const struct rte_ether_addr null_mac_addr;

int
rte_pmd_i40e_remove_vf_mac_addr(uint16_t port, uint16_t vf_id,
	struct rte_ether_addr *mac_addr)
{
	struct rte_eth_dev *dev;
	struct i40e_pf_vf *vf;
	struct i40e_vsi *vsi;
	struct i40e_pf *pf;
	int ret;

	if (i40e_validate_mac_addr((u8 *)mac_addr) != I40E_SUCCESS)
		return -EINVAL;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	if (vf_id >= pf->vf_num || !pf->vfs)
		return -EINVAL;

	vf = &pf->vfs[vf_id];
	vsi = vf->vsi;
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Invalid VSI.");
		return -EINVAL;
	}

	if (rte_is_same_ether_addr(mac_addr, &vf->mac_addr))
		/* Reset the mac with NULL address */
		rte_ether_addr_copy(&null_mac_addr, &vf->mac_addr);

	/* Remove the mac */
	ret = i40e_vsi_delete_mac(vsi, mac_addr);
	if (ret != I40E_SUCCESS)
		return ret;
	return 0;
}

/* Set vlan strip on/off for specific VF from host */
int
rte_pmd_i40e_set_vf_vlan_stripq(uint16_t port, uint16_t vf_id, uint8_t on)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;
	struct i40e_vsi *vsi;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	if (vf_id >= pf->vf_num || !pf->vfs) {
		PMD_DRV_LOG(ERR, "Invalid argument.");
		return -EINVAL;
	}

	vsi = pf->vfs[vf_id].vsi;

	if (!vsi)
		return -EINVAL;

	ret = i40e_vsi_config_vlan_stripping(vsi, !!on);
	if (ret != I40E_SUCCESS) {
		ret = -ENOTSUP;
		PMD_DRV_LOG(ERR, "Failed to set VLAN stripping!");
	}

	return ret;
}

int rte_pmd_i40e_set_vf_vlan_insert(uint16_t port, uint16_t vf_id,
				    uint16_t vlan_id)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;
	struct i40e_hw *hw;
	struct i40e_vsi *vsi;
	struct i40e_vsi_context ctxt;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	if (vlan_id > RTE_ETHER_MAX_VLAN_ID) {
		PMD_DRV_LOG(ERR, "Invalid VLAN ID.");
		return -EINVAL;
	}

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	hw = I40E_PF_TO_HW(pf);

	/**
	 * return -ENODEV if SRIOV not enabled, VF number not configured
	 * or no queue assigned.
	 */
	if (!hw->func_caps.sr_iov_1_1 || pf->vf_num == 0 ||
	    pf->vf_nb_qps == 0)
		return -ENODEV;

	if (vf_id >= pf->vf_num || !pf->vfs) {
		PMD_DRV_LOG(ERR, "Invalid VF ID.");
		return -EINVAL;
	}

	vsi = pf->vfs[vf_id].vsi;
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Invalid VSI.");
		return -EINVAL;
	}

	vsi->info.valid_sections = cpu_to_le16(I40E_AQ_VSI_PROP_VLAN_VALID);
	vsi->info.pvid = vlan_id;
	if (vlan_id > 0)
		vsi->info.port_vlan_flags |= I40E_AQ_VSI_PVLAN_INSERT_PVID;
	else
		vsi->info.port_vlan_flags &= ~I40E_AQ_VSI_PVLAN_INSERT_PVID;

	memset(&ctxt, 0, sizeof(ctxt));
	rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
	ctxt.seid = vsi->seid;

	hw = I40E_VSI_TO_HW(vsi);
	ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
	if (ret != I40E_SUCCESS) {
		ret = -ENOTSUP;
		PMD_DRV_LOG(ERR, "Failed to update VSI params");
	}

	return ret;
}

int rte_pmd_i40e_set_vf_broadcast(uint16_t port, uint16_t vf_id,
				  uint8_t on)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;
	struct i40e_vsi *vsi;
	struct i40e_hw *hw;
	struct i40e_mac_filter_info filter;
	struct rte_ether_addr broadcast = {
		.addr_bytes = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff} };
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	if (on > 1) {
		PMD_DRV_LOG(ERR, "on should be 0 or 1.");
		return -EINVAL;
	}

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	hw = I40E_PF_TO_HW(pf);

	if (vf_id >= pf->vf_num || !pf->vfs) {
		PMD_DRV_LOG(ERR, "Invalid VF ID.");
		return -EINVAL;
	}

	/**
	 * return -ENODEV if SRIOV not enabled, VF number not configured
	 * or no queue assigned.
	 */
	if (!hw->func_caps.sr_iov_1_1 || pf->vf_num == 0 ||
	    pf->vf_nb_qps == 0) {
		PMD_DRV_LOG(ERR, "SRIOV is not enabled or no queue.");
		return -ENODEV;
	}

	vsi = pf->vfs[vf_id].vsi;
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Invalid VSI.");
		return -EINVAL;
	}

	if (on) {
		rte_memcpy(&filter.mac_addr, &broadcast, RTE_ETHER_ADDR_LEN);
		filter.filter_type = I40E_MACVLAN_PERFECT_MATCH;
		ret = i40e_vsi_add_mac(vsi, &filter);
	} else {
		ret = i40e_vsi_delete_mac(vsi, &broadcast);
	}

	if (ret != I40E_SUCCESS && ret != I40E_ERR_PARAM) {
		ret = -ENOTSUP;
		PMD_DRV_LOG(ERR, "Failed to set VSI broadcast");
	} else {
		ret = 0;
	}

	return ret;
}

int rte_pmd_i40e_set_vf_vlan_tag(uint16_t port, uint16_t vf_id, uint8_t on)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;
	struct i40e_hw *hw;
	struct i40e_vsi *vsi;
	struct i40e_vsi_context ctxt;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	if (on > 1) {
		PMD_DRV_LOG(ERR, "on should be 0 or 1.");
		return -EINVAL;
	}

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	hw = I40E_PF_TO_HW(pf);

	/**
	 * return -ENODEV if SRIOV not enabled, VF number not configured
	 * or no queue assigned.
	 */
	if (!hw->func_caps.sr_iov_1_1 || pf->vf_num == 0 ||
	    pf->vf_nb_qps == 0) {
		PMD_DRV_LOG(ERR, "SRIOV is not enabled or no queue.");
		return -ENODEV;
	}

	if (vf_id >= pf->vf_num || !pf->vfs) {
		PMD_DRV_LOG(ERR, "Invalid VF ID.");
		return -EINVAL;
	}

	vsi = pf->vfs[vf_id].vsi;
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Invalid VSI.");
		return -EINVAL;
	}

	vsi->info.valid_sections = cpu_to_le16(I40E_AQ_VSI_PROP_VLAN_VALID);
	if (on) {
		vsi->info.port_vlan_flags |= I40E_AQ_VSI_PVLAN_MODE_TAGGED;
		vsi->info.port_vlan_flags &= ~I40E_AQ_VSI_PVLAN_MODE_UNTAGGED;
	} else {
		vsi->info.port_vlan_flags |= I40E_AQ_VSI_PVLAN_MODE_UNTAGGED;
		vsi->info.port_vlan_flags &= ~I40E_AQ_VSI_PVLAN_MODE_TAGGED;
	}

	memset(&ctxt, 0, sizeof(ctxt));
	rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
	ctxt.seid = vsi->seid;

	hw = I40E_VSI_TO_HW(vsi);
	ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
	if (ret != I40E_SUCCESS) {
		ret = -ENOTSUP;
		PMD_DRV_LOG(ERR, "Failed to update VSI params");
	}

	return ret;
}

static int
i40e_vlan_filter_count(struct i40e_vsi *vsi)
{
	uint32_t j, k;
	uint16_t vlan_id;
	int count = 0;

	for (j = 0; j < I40E_VFTA_SIZE; j++) {
		if (!vsi->vfta[j])
			continue;

		for (k = 0; k < I40E_UINT32_BIT_SIZE; k++) {
			if (!(vsi->vfta[j] & (1 << k)))
				continue;

			vlan_id = j * I40E_UINT32_BIT_SIZE + k;
			if (!vlan_id)
				continue;

			count++;
		}
	}

	return count;
}

int rte_pmd_i40e_set_vf_vlan_filter(uint16_t port, uint16_t vlan_id,
				    uint64_t vf_mask, uint8_t on)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;
	struct i40e_hw *hw;
	struct i40e_vsi *vsi;
	uint16_t vf_idx;
	int ret = I40E_SUCCESS;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	if (vlan_id > RTE_ETHER_MAX_VLAN_ID || !vlan_id) {
		PMD_DRV_LOG(ERR, "Invalid VLAN ID.");
		return -EINVAL;
	}

	if (vf_mask == 0) {
		PMD_DRV_LOG(ERR, "No VF.");
		return -EINVAL;
	}

	if (on > 1) {
		PMD_DRV_LOG(ERR, "on is should be 0 or 1.");
		return -EINVAL;
	}

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	hw = I40E_PF_TO_HW(pf);

	/**
	 * return -ENODEV if SRIOV not enabled, VF number not configured
	 * or no queue assigned.
	 */
	if (!hw->func_caps.sr_iov_1_1 || pf->vf_num == 0 ||
	    pf->vf_nb_qps == 0) {
		PMD_DRV_LOG(ERR, "SRIOV is not enabled or no queue.");
		return -ENODEV;
	}

	for (vf_idx = 0; vf_idx < pf->vf_num && ret == I40E_SUCCESS; vf_idx++) {
		if (vf_mask & ((uint64_t)(1ULL << vf_idx))) {
			vsi = pf->vfs[vf_idx].vsi;
			if (on) {
				if (!vsi->vlan_filter_on) {
					vsi->vlan_filter_on = true;
					i40e_aq_set_vsi_vlan_promisc(hw,
								     vsi->seid,
								     false,
								     NULL);
					if (!vsi->vlan_anti_spoof_on)
						i40e_add_rm_all_vlan_filter(
							vsi, true);
				}
				ret = i40e_vsi_add_vlan(vsi, vlan_id);
			} else {
				ret = i40e_vsi_delete_vlan(vsi, vlan_id);

				if (!i40e_vlan_filter_count(vsi)) {
					vsi->vlan_filter_on = false;
					i40e_aq_set_vsi_vlan_promisc(hw,
								     vsi->seid,
								     true,
								     NULL);
				}
			}
		}
	}

	if (ret != I40E_SUCCESS) {
		ret = -ENOTSUP;
		PMD_DRV_LOG(ERR, "Failed to set VF VLAN filter, on = %d", on);
	}

	return ret;
}

int
rte_pmd_i40e_get_vf_stats(uint16_t port,
			  uint16_t vf_id,
			  struct rte_eth_stats *stats)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;
	struct i40e_vsi *vsi;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	if (vf_id >= pf->vf_num || !pf->vfs) {
		PMD_DRV_LOG(ERR, "Invalid VF ID.");
		return -EINVAL;
	}

	vsi = pf->vfs[vf_id].vsi;
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Invalid VSI.");
		return -EINVAL;
	}

	i40e_update_vsi_stats(vsi);

	stats->ipackets = vsi->eth_stats.rx_unicast +
			vsi->eth_stats.rx_multicast +
			vsi->eth_stats.rx_broadcast;
	stats->opackets = vsi->eth_stats.tx_unicast +
			vsi->eth_stats.tx_multicast +
			vsi->eth_stats.tx_broadcast;
	stats->ibytes   = vsi->eth_stats.rx_bytes;
	stats->obytes   = vsi->eth_stats.tx_bytes;
	stats->ierrors  = vsi->eth_stats.rx_discards;
	stats->oerrors  = vsi->eth_stats.tx_errors + vsi->eth_stats.tx_discards;

	return 0;
}

int
rte_pmd_i40e_reset_vf_stats(uint16_t port,
			    uint16_t vf_id)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;
	struct i40e_vsi *vsi;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	if (vf_id >= pf->vf_num || !pf->vfs) {
		PMD_DRV_LOG(ERR, "Invalid VF ID.");
		return -EINVAL;
	}

	vsi = pf->vfs[vf_id].vsi;
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Invalid VSI.");
		return -EINVAL;
	}

	vsi->offset_loaded = false;
	i40e_update_vsi_stats(vsi);

	return 0;
}

int
rte_pmd_i40e_set_vf_max_bw(uint16_t port, uint16_t vf_id, uint32_t bw)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;
	struct i40e_vsi *vsi;
	struct i40e_hw *hw;
	int ret = 0;
	int i;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	if (vf_id >= pf->vf_num || !pf->vfs) {
		PMD_DRV_LOG(ERR, "Invalid VF ID.");
		return -EINVAL;
	}

	vsi = pf->vfs[vf_id].vsi;
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Invalid VSI.");
		return -EINVAL;
	}

	if (bw > I40E_QOS_BW_MAX) {
		PMD_DRV_LOG(ERR, "Bandwidth should not be larger than %dMbps.",
			    I40E_QOS_BW_MAX);
		return -EINVAL;
	}

	if (bw % I40E_QOS_BW_GRANULARITY) {
		PMD_DRV_LOG(ERR, "Bandwidth should be the multiple of %dMbps.",
			    I40E_QOS_BW_GRANULARITY);
		return -EINVAL;
	}

	bw /= I40E_QOS_BW_GRANULARITY;

	hw = I40E_VSI_TO_HW(vsi);

	/* No change. */
	if (bw == vsi->bw_info.bw_limit) {
		PMD_DRV_LOG(INFO,
			    "No change for VF max bandwidth. Nothing to do.");
		return 0;
	}

	/**
	 * VF bandwidth limitation and TC bandwidth limitation cannot be
	 * enabled in parallel, quit if TC bandwidth limitation is enabled.
	 *
	 * If bw is 0, means disable bandwidth limitation. Then no need to
	 * check TC bandwidth limitation.
	 */
	if (bw) {
		for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
			if ((vsi->enabled_tc & BIT_ULL(i)) &&
			    vsi->bw_info.bw_ets_credits[i])
				break;
		}
		if (i != I40E_MAX_TRAFFIC_CLASS) {
			PMD_DRV_LOG(ERR,
				    "TC max bandwidth has been set on this VF,"
				    " please disable it first.");
			return -EINVAL;
		}
	}

	ret = i40e_aq_config_vsi_bw_limit(hw, vsi->seid, (uint16_t)bw, 0, NULL);
	if (ret) {
		PMD_DRV_LOG(ERR,
			    "Failed to set VF %d bandwidth, err(%d).",
			    vf_id, ret);
		return -EINVAL;
	}

	/* Store the configuration. */
	vsi->bw_info.bw_limit = (uint16_t)bw;
	vsi->bw_info.bw_max = 0;

	return 0;
}

int
rte_pmd_i40e_set_vf_tc_bw_alloc(uint16_t port, uint16_t vf_id,
				uint8_t tc_num, uint8_t *bw_weight)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;
	struct i40e_vsi *vsi;
	struct i40e_hw *hw;
	struct i40e_aqc_configure_vsi_tc_bw_data tc_bw;
	int ret = 0;
	int i, j;
	uint16_t sum;
	bool b_change = false;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	if (vf_id >= pf->vf_num || !pf->vfs) {
		PMD_DRV_LOG(ERR, "Invalid VF ID.");
		return -EINVAL;
	}

	vsi = pf->vfs[vf_id].vsi;
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Invalid VSI.");
		return -EINVAL;
	}

	if (tc_num > I40E_MAX_TRAFFIC_CLASS) {
		PMD_DRV_LOG(ERR, "TCs should be no more than %d.",
			    I40E_MAX_TRAFFIC_CLASS);
		return -EINVAL;
	}

	sum = 0;
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (vsi->enabled_tc & BIT_ULL(i))
			sum++;
	}
	if (sum != tc_num) {
		PMD_DRV_LOG(ERR,
			    "Weight should be set for all %d enabled TCs.",
			    sum);
		return -EINVAL;
	}

	sum = 0;
	for (i = 0; i < tc_num; i++) {
		if (!bw_weight[i]) {
			PMD_DRV_LOG(ERR,
				    "The weight should be 1 at least.");
			return -EINVAL;
		}
		sum += bw_weight[i];
	}
	if (sum != 100) {
		PMD_DRV_LOG(ERR,
			    "The summary of the TC weight should be 100.");
		return -EINVAL;
	}

	/**
	 * Create the configuration for all the TCs.
	 */
	memset(&tc_bw, 0, sizeof(tc_bw));
	tc_bw.tc_valid_bits = vsi->enabled_tc;
	j = 0;
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (vsi->enabled_tc & BIT_ULL(i)) {
			if (bw_weight[j] !=
				vsi->bw_info.bw_ets_share_credits[i])
				b_change = true;

			tc_bw.tc_bw_credits[i] = bw_weight[j];
			j++;
		}
	}

	/* No change. */
	if (!b_change) {
		PMD_DRV_LOG(INFO,
			    "No change for TC allocated bandwidth."
			    " Nothing to do.");
		return 0;
	}

	hw = I40E_VSI_TO_HW(vsi);

	ret = i40e_aq_config_vsi_tc_bw(hw, vsi->seid, &tc_bw, NULL);
	if (ret) {
		PMD_DRV_LOG(ERR,
			    "Failed to set VF %d TC bandwidth weight, err(%d).",
			    vf_id, ret);
		return -EINVAL;
	}

	/* Store the configuration. */
	j = 0;
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (vsi->enabled_tc & BIT_ULL(i)) {
			vsi->bw_info.bw_ets_share_credits[i] = bw_weight[j];
			j++;
		}
	}

	return 0;
}

int
rte_pmd_i40e_set_vf_tc_max_bw(uint16_t port, uint16_t vf_id,
			      uint8_t tc_no, uint32_t bw)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;
	struct i40e_vsi *vsi;
	struct i40e_hw *hw;
	struct i40e_aqc_configure_vsi_ets_sla_bw_data tc_bw;
	int ret = 0;
	int i;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	if (vf_id >= pf->vf_num || !pf->vfs) {
		PMD_DRV_LOG(ERR, "Invalid VF ID.");
		return -EINVAL;
	}

	vsi = pf->vfs[vf_id].vsi;
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Invalid VSI.");
		return -EINVAL;
	}

	if (bw > I40E_QOS_BW_MAX) {
		PMD_DRV_LOG(ERR, "Bandwidth should not be larger than %dMbps.",
			    I40E_QOS_BW_MAX);
		return -EINVAL;
	}

	if (bw % I40E_QOS_BW_GRANULARITY) {
		PMD_DRV_LOG(ERR, "Bandwidth should be the multiple of %dMbps.",
			    I40E_QOS_BW_GRANULARITY);
		return -EINVAL;
	}

	bw /= I40E_QOS_BW_GRANULARITY;

	if (tc_no >= I40E_MAX_TRAFFIC_CLASS) {
		PMD_DRV_LOG(ERR, "TC No. should be less than %d.",
			    I40E_MAX_TRAFFIC_CLASS);
		return -EINVAL;
	}

	hw = I40E_VSI_TO_HW(vsi);

	if (!(vsi->enabled_tc & BIT_ULL(tc_no))) {
		PMD_DRV_LOG(ERR, "VF %d TC %d isn't enabled.",
			    vf_id, tc_no);
		return -EINVAL;
	}

	/* No change. */
	if (bw == vsi->bw_info.bw_ets_credits[tc_no]) {
		PMD_DRV_LOG(INFO,
			    "No change for TC max bandwidth. Nothing to do.");
		return 0;
	}

	/**
	 * VF bandwidth limitation and TC bandwidth limitation cannot be
	 * enabled in parallel, disable VF bandwidth limitation if it's
	 * enabled.
	 * If bw is 0, means disable bandwidth limitation. Then no need to
	 * care about VF bandwidth limitation configuration.
	 */
	if (bw && vsi->bw_info.bw_limit) {
		ret = i40e_aq_config_vsi_bw_limit(hw, vsi->seid, 0, 0, NULL);
		if (ret) {
			PMD_DRV_LOG(ERR,
				    "Failed to disable VF(%d)"
				    " bandwidth limitation, err(%d).",
				    vf_id, ret);
			return -EINVAL;
		}

		PMD_DRV_LOG(INFO,
			    "VF max bandwidth is disabled according"
			    " to TC max bandwidth setting.");
	}

	/**
	 * Get all the TCs' info to create a whole picture.
	 * Because the incremental change isn't permitted.
	 */
	memset(&tc_bw, 0, sizeof(tc_bw));
	tc_bw.tc_valid_bits = vsi->enabled_tc;
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (vsi->enabled_tc & BIT_ULL(i)) {
			tc_bw.tc_bw_credits[i] =
				rte_cpu_to_le_16(
					vsi->bw_info.bw_ets_credits[i]);
		}
	}
	tc_bw.tc_bw_credits[tc_no] = rte_cpu_to_le_16((uint16_t)bw);

	ret = i40e_aq_config_vsi_ets_sla_bw_limit(hw, vsi->seid, &tc_bw, NULL);
	if (ret) {
		PMD_DRV_LOG(ERR,
			    "Failed to set VF %d TC %d max bandwidth, err(%d).",
			    vf_id, tc_no, ret);
		return -EINVAL;
	}

	/* Store the configuration. */
	vsi->bw_info.bw_ets_credits[tc_no] = (uint16_t)bw;

	return 0;
}

int
rte_pmd_i40e_set_tc_strict_prio(uint16_t port, uint8_t tc_map)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;
	struct i40e_vsi *vsi;
	struct i40e_veb *veb;
	struct i40e_hw *hw;
	struct i40e_aqc_configure_switching_comp_ets_data ets_data;
	int i;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	vsi = pf->main_vsi;
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Invalid VSI.");
		return -EINVAL;
	}

	veb = vsi->veb;
	if (!veb) {
		PMD_DRV_LOG(ERR, "Invalid VEB.");
		return -EINVAL;
	}

	if ((tc_map & veb->enabled_tc) != tc_map) {
		PMD_DRV_LOG(ERR,
			    "TC bitmap isn't the subset of enabled TCs 0x%x.",
			    veb->enabled_tc);
		return -EINVAL;
	}

	if (tc_map == veb->strict_prio_tc) {
		PMD_DRV_LOG(INFO, "No change for TC bitmap. Nothing to do.");
		return 0;
	}

	hw = I40E_VSI_TO_HW(vsi);

	/* Disable DCBx if it's the first time to set strict priority. */
	if (!veb->strict_prio_tc) {
		ret = i40e_aq_stop_lldp(hw, true, true, NULL);
		if (ret)
			PMD_DRV_LOG(INFO,
				    "Failed to disable DCBx as it's already"
				    " disabled.");
		else
			PMD_DRV_LOG(INFO,
				    "DCBx is disabled according to strict"
				    " priority setting.");
	}

	memset(&ets_data, 0, sizeof(ets_data));
	ets_data.tc_valid_bits = veb->enabled_tc;
	ets_data.seepage = I40E_AQ_ETS_SEEPAGE_EN_MASK;
	ets_data.tc_strict_priority_flags = tc_map;
	/* Get all TCs' bandwidth. */
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (veb->enabled_tc & BIT_ULL(i)) {
			/* For robust, if bandwidth is 0, use 1 instead. */
			if (veb->bw_info.bw_ets_share_credits[i])
				ets_data.tc_bw_share_credits[i] =
					veb->bw_info.bw_ets_share_credits[i];
			else
				ets_data.tc_bw_share_credits[i] =
					I40E_QOS_BW_WEIGHT_MIN;
		}
	}

	if (!veb->strict_prio_tc)
		ret = i40e_aq_config_switch_comp_ets(
			hw, veb->uplink_seid,
			&ets_data, i40e_aqc_opc_enable_switching_comp_ets,
			NULL);
	else if (tc_map)
		ret = i40e_aq_config_switch_comp_ets(
			hw, veb->uplink_seid,
			&ets_data, i40e_aqc_opc_modify_switching_comp_ets,
			NULL);
	else
		ret = i40e_aq_config_switch_comp_ets(
			hw, veb->uplink_seid,
			&ets_data, i40e_aqc_opc_disable_switching_comp_ets,
			NULL);

	if (ret) {
		PMD_DRV_LOG(ERR,
			    "Failed to set TCs' strict priority mode."
			    " err (%d)", ret);
		return -EINVAL;
	}

	veb->strict_prio_tc = tc_map;

	/* Enable DCBx again, if all the TCs' strict priority disabled. */
	if (!tc_map) {
		ret = i40e_aq_start_lldp(hw, true, NULL);
		if (ret) {
			PMD_DRV_LOG(ERR,
				    "Failed to enable DCBx, err(%d).", ret);
			return -EINVAL;
		}

		PMD_DRV_LOG(INFO,
			    "DCBx is enabled again according to strict"
			    " priority setting.");
	}

	return ret;
}

#define I40E_PROFILE_INFO_SIZE sizeof(struct rte_pmd_i40e_profile_info)
#define I40E_MAX_PROFILE_NUM 16

static void
i40e_generate_profile_info_sec(char *name, struct i40e_ddp_version *version,
			       uint32_t track_id, uint8_t *profile_info_sec,
			       bool add)
{
	struct i40e_profile_section_header *sec = NULL;
	struct i40e_profile_info *pinfo;

	sec = (struct i40e_profile_section_header *)profile_info_sec;
	sec->tbl_size = 1;
	sec->data_end = sizeof(struct i40e_profile_section_header) +
		sizeof(struct i40e_profile_info);
	sec->section.type = SECTION_TYPE_INFO;
	sec->section.offset = sizeof(struct i40e_profile_section_header);
	sec->section.size = sizeof(struct i40e_profile_info);
	pinfo = (struct i40e_profile_info *)(profile_info_sec +
					     sec->section.offset);
	pinfo->track_id = track_id;
	memcpy(pinfo->name, name, I40E_DDP_NAME_SIZE);
	memcpy(&pinfo->version, version, sizeof(struct i40e_ddp_version));
	if (add)
		pinfo->op = I40E_DDP_ADD_TRACKID;
	else
		pinfo->op = I40E_DDP_REMOVE_TRACKID;
}

static enum i40e_status_code
i40e_add_rm_profile_info(struct i40e_hw *hw, uint8_t *profile_info_sec)
{
	enum i40e_status_code status = I40E_SUCCESS;
	struct i40e_profile_section_header *sec;
	uint32_t track_id;
	uint32_t offset = 0;
	uint32_t info = 0;

	sec = (struct i40e_profile_section_header *)profile_info_sec;
	track_id = ((struct i40e_profile_info *)(profile_info_sec +
					 sec->section.offset))->track_id;

	status = i40e_aq_write_ddp(hw, (void *)sec, sec->data_end,
				   track_id, &offset, &info, NULL);
	if (status)
		PMD_DRV_LOG(ERR, "Failed to add/remove profile info: "
			    "offset %d, info %d",
			    offset, info);

	return status;
}

/* Check if the profile info exists */
static int
i40e_check_profile_info(uint16_t port, uint8_t *profile_info_sec)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port];
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint8_t *buff;
	struct rte_pmd_i40e_profile_list *p_list;
	struct rte_pmd_i40e_profile_info *pinfo, *p;
	uint32_t i;
	int ret;
	static const uint32_t group_mask = 0x00ff0000;

	pinfo = (struct rte_pmd_i40e_profile_info *)(profile_info_sec +
			     sizeof(struct i40e_profile_section_header));
	if (pinfo->track_id == 0) {
		PMD_DRV_LOG(INFO, "Read-only profile.");
		return 0;
	}
	buff = rte_zmalloc("pinfo_list",
			   (I40E_PROFILE_INFO_SIZE * I40E_MAX_PROFILE_NUM + 4),
			   0);
	if (!buff) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		return -1;
	}

	ret = i40e_aq_get_ddp_list(
		hw, (void *)buff,
		(I40E_PROFILE_INFO_SIZE * I40E_MAX_PROFILE_NUM + 4),
		0, NULL);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to get profile info list.");
		rte_free(buff);
		return -1;
	}
	p_list = (struct rte_pmd_i40e_profile_list *)buff;
	for (i = 0; i < p_list->p_count; i++) {
		p = &p_list->p_info[i];
		if (pinfo->track_id == p->track_id) {
			PMD_DRV_LOG(INFO, "Profile exists.");
			rte_free(buff);
			return 1;
		}
	}
	/* profile with group id 0xff is compatible with any other profile */
	if ((pinfo->track_id & group_mask) == group_mask) {
		rte_free(buff);
		return 0;
	}
	for (i = 0; i < p_list->p_count; i++) {
		p = &p_list->p_info[i];
		if ((p->track_id & group_mask) == 0) {
			PMD_DRV_LOG(INFO, "Profile of the group 0 exists.");
			rte_free(buff);
			return 2;
		}
	}
	for (i = 0; i < p_list->p_count; i++) {
		p = &p_list->p_info[i];
		if ((p->track_id & group_mask) == group_mask)
			continue;
		if ((pinfo->track_id & group_mask) !=
		    (p->track_id & group_mask)) {
			PMD_DRV_LOG(INFO, "Profile of different group exists.");
			rte_free(buff);
			return 3;
		}
	}

	rte_free(buff);
	return 0;
}

int
rte_pmd_i40e_process_ddp_package(uint16_t port, uint8_t *buff,
				 uint32_t size,
				 enum rte_pmd_i40e_package_op op)
{
	struct rte_eth_dev *dev;
	struct i40e_hw *hw;
	struct i40e_package_header *pkg_hdr;
	struct i40e_generic_seg_header *profile_seg_hdr;
	struct i40e_generic_seg_header *metadata_seg_hdr;
	uint32_t track_id;
	uint8_t *profile_info_sec;
	int is_exist;
	enum i40e_status_code status = I40E_SUCCESS;
	static const uint32_t type_mask = 0xff000000;

	if (op != RTE_PMD_I40E_PKG_OP_WR_ADD &&
		op != RTE_PMD_I40E_PKG_OP_WR_ONLY &&
		op != RTE_PMD_I40E_PKG_OP_WR_DEL) {
		PMD_DRV_LOG(ERR, "Operation not supported.");
		return -ENOTSUP;
	}

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (size < (sizeof(struct i40e_package_header) +
		    sizeof(struct i40e_metadata_segment) +
		    sizeof(uint32_t) * 2)) {
		PMD_DRV_LOG(ERR, "Buff is invalid.");
		return -EINVAL;
	}

	pkg_hdr = (struct i40e_package_header *)buff;

	if (!pkg_hdr) {
		PMD_DRV_LOG(ERR, "Failed to fill the package structure");
		return -EINVAL;
	}

	if (pkg_hdr->segment_count < 2) {
		PMD_DRV_LOG(ERR, "Segment_count should be 2 at least.");
		return -EINVAL;
	}

	/* Find metadata segment */
	metadata_seg_hdr = i40e_find_segment_in_package(SEGMENT_TYPE_METADATA,
							pkg_hdr);
	if (!metadata_seg_hdr) {
		PMD_DRV_LOG(ERR, "Failed to find metadata segment header");
		return -EINVAL;
	}
	track_id = ((struct i40e_metadata_segment *)metadata_seg_hdr)->track_id;
	if (track_id == I40E_DDP_TRACKID_INVALID) {
		PMD_DRV_LOG(ERR, "Invalid track_id");
		return -EINVAL;
	}

	/* force read-only track_id for type 0 */
	if ((track_id & type_mask) == 0)
		track_id = 0;

	/* Find profile segment */
	profile_seg_hdr = i40e_find_segment_in_package(SEGMENT_TYPE_I40E,
						       pkg_hdr);
	if (!profile_seg_hdr) {
		PMD_DRV_LOG(ERR, "Failed to find profile segment header");
		return -EINVAL;
	}

	profile_info_sec = rte_zmalloc(
		"i40e_profile_info",
		sizeof(struct i40e_profile_section_header) +
		sizeof(struct i40e_profile_info),
		0);
	if (!profile_info_sec) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory");
		return -EINVAL;
	}

	/* Check if the profile already loaded */
	i40e_generate_profile_info_sec(
		((struct i40e_profile_segment *)profile_seg_hdr)->name,
		&((struct i40e_profile_segment *)profile_seg_hdr)->version,
		track_id, profile_info_sec,
		op == RTE_PMD_I40E_PKG_OP_WR_ADD);
	is_exist = i40e_check_profile_info(port, profile_info_sec);
	if (is_exist < 0) {
		PMD_DRV_LOG(ERR, "Failed to check profile.");
		rte_free(profile_info_sec);
		return -EINVAL;
	}

	if (op == RTE_PMD_I40E_PKG_OP_WR_ADD) {
		if (is_exist) {
			if (is_exist == 1)
				PMD_DRV_LOG(ERR, "Profile already exists.");
			else if (is_exist == 2)
				PMD_DRV_LOG(ERR, "Profile of group 0 already exists.");
			else if (is_exist == 3)
				PMD_DRV_LOG(ERR, "Profile of different group already exists");
			i40e_update_customized_info(dev, buff, size, op);
			rte_free(profile_info_sec);
			return -EEXIST;
		}
	} else if (op == RTE_PMD_I40E_PKG_OP_WR_DEL) {
		if (is_exist != 1) {
			PMD_DRV_LOG(ERR, "Profile does not exist.");
			rte_free(profile_info_sec);
			return -EACCES;
		}
	}

	if (op == RTE_PMD_I40E_PKG_OP_WR_DEL) {
		status = i40e_rollback_profile(
			hw,
			(struct i40e_profile_segment *)profile_seg_hdr,
			track_id);
		if (status) {
			PMD_DRV_LOG(ERR, "Failed to write profile for delete.");
			rte_free(profile_info_sec);
			return status;
		}
	} else {
		status = i40e_write_profile(
			hw,
			(struct i40e_profile_segment *)profile_seg_hdr,
			track_id);
		if (status) {
			if (op == RTE_PMD_I40E_PKG_OP_WR_ADD)
				PMD_DRV_LOG(ERR, "Failed to write profile for add.");
			else
				PMD_DRV_LOG(ERR, "Failed to write profile.");
			rte_free(profile_info_sec);
			return status;
		}
	}

	if (track_id && (op != RTE_PMD_I40E_PKG_OP_WR_ONLY)) {
		/* Modify loaded profiles info list */
		status = i40e_add_rm_profile_info(hw, profile_info_sec);
		if (status) {
			if (op == RTE_PMD_I40E_PKG_OP_WR_ADD)
				PMD_DRV_LOG(ERR, "Failed to add profile to info list.");
			else
				PMD_DRV_LOG(ERR, "Failed to delete profile from info list.");
		}
	}

	if (op == RTE_PMD_I40E_PKG_OP_WR_ADD ||
	    op == RTE_PMD_I40E_PKG_OP_WR_DEL)
		i40e_update_customized_info(dev, buff, size, op);

	rte_free(profile_info_sec);
	return status;
}

/* Get number of tvl records in the section */
static unsigned int
i40e_get_tlv_section_size(struct i40e_profile_section_header *sec)
{
	unsigned int i, nb_rec, nb_tlv = 0;
	struct i40e_profile_tlv_section_record *tlv;

	if (!sec)
		return nb_tlv;

	/* get number of records in the section */
	nb_rec = sec->section.size /
				sizeof(struct i40e_profile_tlv_section_record);
	for (i = 0; i < nb_rec; ) {
		tlv = (struct i40e_profile_tlv_section_record *)&sec[1 + i];
		i += tlv->len;
		nb_tlv++;
	}
	return nb_tlv;
}

int rte_pmd_i40e_get_ddp_info(uint8_t *pkg_buff, uint32_t pkg_size,
	uint8_t *info_buff, uint32_t info_size,
	enum rte_pmd_i40e_package_info type)
{
	uint32_t ret_size;
	struct i40e_package_header *pkg_hdr;
	struct i40e_generic_seg_header *i40e_seg_hdr;
	struct i40e_generic_seg_header *note_seg_hdr;
	struct i40e_generic_seg_header *metadata_seg_hdr;

	if (!info_buff) {
		PMD_DRV_LOG(ERR, "Output info buff is invalid.");
		return -EINVAL;
	}

	if (!pkg_buff || pkg_size < (sizeof(struct i40e_package_header) +
		sizeof(struct i40e_metadata_segment) +
		sizeof(uint32_t) * 2)) {
		PMD_DRV_LOG(ERR, "Package buff is invalid.");
		return -EINVAL;
	}

	pkg_hdr = (struct i40e_package_header *)pkg_buff;
	if (pkg_hdr->segment_count < 2) {
		PMD_DRV_LOG(ERR, "Segment_count should be 2 at least.");
		return -EINVAL;
	}

	/* Find metadata segment */
	metadata_seg_hdr = i40e_find_segment_in_package(SEGMENT_TYPE_METADATA,
		pkg_hdr);

	/* Find global notes segment */
	note_seg_hdr = i40e_find_segment_in_package(SEGMENT_TYPE_NOTES,
		pkg_hdr);

	/* Find i40e profile segment */
	i40e_seg_hdr = i40e_find_segment_in_package(SEGMENT_TYPE_I40E, pkg_hdr);

	/* get global header info */
	if (type == RTE_PMD_I40E_PKG_INFO_GLOBAL_HEADER) {
		struct rte_pmd_i40e_profile_info *info =
			(struct rte_pmd_i40e_profile_info *)info_buff;

		if (info_size < sizeof(struct rte_pmd_i40e_profile_info)) {
			PMD_DRV_LOG(ERR, "Output info buff size is invalid.");
			return -EINVAL;
		}

		if (!metadata_seg_hdr) {
			PMD_DRV_LOG(ERR, "Failed to find metadata segment header");
			return -EINVAL;
		}

		memset(info, 0, sizeof(struct rte_pmd_i40e_profile_info));
		info->owner = RTE_PMD_I40E_DDP_OWNER_UNKNOWN;
		info->track_id =
			((struct i40e_metadata_segment *)metadata_seg_hdr)->track_id;

		memcpy(info->name,
			((struct i40e_metadata_segment *)metadata_seg_hdr)->name,
			I40E_DDP_NAME_SIZE);
		memcpy(&info->version,
			&((struct i40e_metadata_segment *)metadata_seg_hdr)->version,
			sizeof(struct i40e_ddp_version));
		return I40E_SUCCESS;
	}

	/* get global note size */
	if (type == RTE_PMD_I40E_PKG_INFO_GLOBAL_NOTES_SIZE) {
		if (info_size < sizeof(uint32_t)) {
			PMD_DRV_LOG(ERR, "Invalid information buffer size");
			return -EINVAL;
		}
		if (note_seg_hdr == NULL)
			ret_size = 0;
		else
			ret_size = note_seg_hdr->size;
		*(uint32_t *)info_buff = ret_size;
		return I40E_SUCCESS;
	}

	/* get global note */
	if (type == RTE_PMD_I40E_PKG_INFO_GLOBAL_NOTES) {
		if (note_seg_hdr == NULL)
			return -ENOTSUP;
		if (info_size < note_seg_hdr->size) {
			PMD_DRV_LOG(ERR, "Information buffer size is too small");
			return -EINVAL;
		}
		memcpy(info_buff, &note_seg_hdr[1], note_seg_hdr->size);
		return I40E_SUCCESS;
	}

	/* get i40e segment header info */
	if (type == RTE_PMD_I40E_PKG_INFO_HEADER) {
		struct rte_pmd_i40e_profile_info *info =
			(struct rte_pmd_i40e_profile_info *)info_buff;

		if (info_size < sizeof(struct rte_pmd_i40e_profile_info)) {
			PMD_DRV_LOG(ERR, "Output info buff size is invalid.");
			return -EINVAL;
		}

		if (!metadata_seg_hdr) {
			PMD_DRV_LOG(ERR, "Failed to find metadata segment header");
			return -EINVAL;
		}

		if (!i40e_seg_hdr) {
			PMD_DRV_LOG(ERR, "Failed to find i40e segment header");
			return -EINVAL;
		}

		memset(info, 0, sizeof(struct rte_pmd_i40e_profile_info));
		info->owner = RTE_PMD_I40E_DDP_OWNER_UNKNOWN;
		info->track_id =
			((struct i40e_metadata_segment *)metadata_seg_hdr)->track_id;

		memcpy(info->name,
			((struct i40e_profile_segment *)i40e_seg_hdr)->name,
			I40E_DDP_NAME_SIZE);
		memcpy(&info->version,
			&((struct i40e_profile_segment *)i40e_seg_hdr)->version,
			sizeof(struct i40e_ddp_version));
		return I40E_SUCCESS;
	}

	/* get number of devices */
	if (type == RTE_PMD_I40E_PKG_INFO_DEVID_NUM) {
		if (info_size < sizeof(uint32_t)) {
			PMD_DRV_LOG(ERR, "Invalid information buffer size");
			return -EINVAL;
		}
		*(uint32_t *)info_buff =
			((struct i40e_profile_segment *)i40e_seg_hdr)->device_table_count;
		return I40E_SUCCESS;
	}

	/* get list of devices */
	if (type == RTE_PMD_I40E_PKG_INFO_DEVID_LIST) {
		uint32_t dev_num;
		dev_num =
			((struct i40e_profile_segment *)i40e_seg_hdr)->device_table_count;
		if (info_size < sizeof(struct rte_pmd_i40e_ddp_device_id) * dev_num) {
			PMD_DRV_LOG(ERR, "Invalid information buffer size");
			return -EINVAL;
		}
		memcpy(info_buff,
			((struct i40e_profile_segment *)i40e_seg_hdr)->device_table,
			sizeof(struct rte_pmd_i40e_ddp_device_id) * dev_num);
		return I40E_SUCCESS;
	}

	/* get number of protocols */
	if (type == RTE_PMD_I40E_PKG_INFO_PROTOCOL_NUM) {
		struct i40e_profile_section_header *proto;

		if (info_size < sizeof(uint32_t)) {
			PMD_DRV_LOG(ERR, "Invalid information buffer size");
			return -EINVAL;
		}
		proto = i40e_find_section_in_profile(SECTION_TYPE_PROTO,
				(struct i40e_profile_segment *)i40e_seg_hdr);
		*(uint32_t *)info_buff = i40e_get_tlv_section_size(proto);
		return I40E_SUCCESS;
	}

	/* get list of protocols */
	if (type == RTE_PMD_I40E_PKG_INFO_PROTOCOL_LIST) {
		uint32_t i, j, nb_tlv, nb_rec, nb_proto_info;
		struct rte_pmd_i40e_proto_info *pinfo;
		struct i40e_profile_section_header *proto;
		struct i40e_profile_tlv_section_record *tlv;

		pinfo = (struct rte_pmd_i40e_proto_info *)info_buff;
		nb_proto_info = info_size /
					sizeof(struct rte_pmd_i40e_proto_info);
		for (i = 0; i < nb_proto_info; i++) {
			pinfo[i].proto_id = RTE_PMD_I40E_PROTO_UNUSED;
			memset(pinfo[i].name, 0, RTE_PMD_I40E_DDP_NAME_SIZE);
		}
		proto = i40e_find_section_in_profile(SECTION_TYPE_PROTO,
				(struct i40e_profile_segment *)i40e_seg_hdr);
		nb_tlv = i40e_get_tlv_section_size(proto);
		if (nb_tlv == 0)
			return I40E_SUCCESS;
		if (nb_proto_info < nb_tlv) {
			PMD_DRV_LOG(ERR, "Invalid information buffer size");
			return -EINVAL;
		}
		/* get number of records in the section */
		nb_rec = proto->section.size /
				sizeof(struct i40e_profile_tlv_section_record);
		tlv = (struct i40e_profile_tlv_section_record *)&proto[1];
		for (i = j = 0; i < nb_rec; j++) {
			pinfo[j].proto_id = tlv->data[0];
			strlcpy(pinfo[j].name, (const char *)&tlv->data[1],
				I40E_DDP_NAME_SIZE);
			i += tlv->len;
			tlv = &tlv[tlv->len];
		}
		return I40E_SUCCESS;
	}

	/* get number of packet classification types */
	if (type == RTE_PMD_I40E_PKG_INFO_PCTYPE_NUM) {
		struct i40e_profile_section_header *pctype;

		if (info_size < sizeof(uint32_t)) {
			PMD_DRV_LOG(ERR, "Invalid information buffer size");
			return -EINVAL;
		}
		pctype = i40e_find_section_in_profile(SECTION_TYPE_PCTYPE,
				(struct i40e_profile_segment *)i40e_seg_hdr);
		*(uint32_t *)info_buff = i40e_get_tlv_section_size(pctype);
		return I40E_SUCCESS;
	}

	/* get list of packet classification types */
	if (type == RTE_PMD_I40E_PKG_INFO_PCTYPE_LIST) {
		uint32_t i, j, nb_tlv, nb_rec, nb_proto_info;
		struct rte_pmd_i40e_ptype_info *pinfo;
		struct i40e_profile_section_header *pctype;
		struct i40e_profile_tlv_section_record *tlv;

		pinfo = (struct rte_pmd_i40e_ptype_info *)info_buff;
		nb_proto_info = info_size /
					sizeof(struct rte_pmd_i40e_ptype_info);
		for (i = 0; i < nb_proto_info; i++)
			memset(&pinfo[i], RTE_PMD_I40E_PROTO_UNUSED,
			       sizeof(struct rte_pmd_i40e_ptype_info));
		pctype = i40e_find_section_in_profile(SECTION_TYPE_PCTYPE,
				(struct i40e_profile_segment *)i40e_seg_hdr);
		nb_tlv = i40e_get_tlv_section_size(pctype);
		if (nb_tlv == 0)
			return I40E_SUCCESS;
		if (nb_proto_info < nb_tlv) {
			PMD_DRV_LOG(ERR, "Invalid information buffer size");
			return -EINVAL;
		}

		/* get number of records in the section */
		nb_rec = pctype->section.size /
				sizeof(struct i40e_profile_tlv_section_record);
		tlv = (struct i40e_profile_tlv_section_record *)&pctype[1];
		for (i = j = 0; i < nb_rec; j++) {
			memcpy(&pinfo[j], tlv->data,
			       sizeof(struct rte_pmd_i40e_ptype_info));
			i += tlv->len;
			tlv = &tlv[tlv->len];
		}
		return I40E_SUCCESS;
	}

	/* get number of packet types */
	if (type == RTE_PMD_I40E_PKG_INFO_PTYPE_NUM) {
		struct i40e_profile_section_header *ptype;

		if (info_size < sizeof(uint32_t)) {
			PMD_DRV_LOG(ERR, "Invalid information buffer size");
			return -EINVAL;
		}
		ptype = i40e_find_section_in_profile(SECTION_TYPE_PTYPE,
				(struct i40e_profile_segment *)i40e_seg_hdr);
		*(uint32_t *)info_buff = i40e_get_tlv_section_size(ptype);
		return I40E_SUCCESS;
	}

	/* get list of packet types */
	if (type == RTE_PMD_I40E_PKG_INFO_PTYPE_LIST) {
		uint32_t i, j, nb_tlv, nb_rec, nb_proto_info;
		struct rte_pmd_i40e_ptype_info *pinfo;
		struct i40e_profile_section_header *ptype;
		struct i40e_profile_tlv_section_record *tlv;

		pinfo = (struct rte_pmd_i40e_ptype_info *)info_buff;
		nb_proto_info = info_size /
					sizeof(struct rte_pmd_i40e_ptype_info);
		for (i = 0; i < nb_proto_info; i++)
			memset(&pinfo[i], RTE_PMD_I40E_PROTO_UNUSED,
			       sizeof(struct rte_pmd_i40e_ptype_info));
		ptype = i40e_find_section_in_profile(SECTION_TYPE_PTYPE,
				(struct i40e_profile_segment *)i40e_seg_hdr);
		nb_tlv = i40e_get_tlv_section_size(ptype);
		if (nb_tlv == 0)
			return I40E_SUCCESS;
		if (nb_proto_info < nb_tlv) {
			PMD_DRV_LOG(ERR, "Invalid information buffer size");
			return -EINVAL;
		}
		/* get number of records in the section */
		nb_rec = ptype->section.size /
				sizeof(struct i40e_profile_tlv_section_record);
		for (i = j = 0; i < nb_rec; j++) {
			tlv = (struct i40e_profile_tlv_section_record *)
								&ptype[1 + i];
			memcpy(&pinfo[j], tlv->data,
			       sizeof(struct rte_pmd_i40e_ptype_info));
			i += tlv->len;
		}
		return I40E_SUCCESS;
	}

	PMD_DRV_LOG(ERR, "Info type %u is invalid.", type);
	return -EINVAL;
}

int
rte_pmd_i40e_get_ddp_list(uint16_t port, uint8_t *buff, uint32_t size)
{
	struct rte_eth_dev *dev;
	struct i40e_hw *hw;
	enum i40e_status_code status = I40E_SUCCESS;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	if (size < (I40E_PROFILE_INFO_SIZE * I40E_MAX_PROFILE_NUM + 4))
		return -EINVAL;

	hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	status = i40e_aq_get_ddp_list(hw, (void *)buff,
				      size, 0, NULL);

	return status;
}

static int check_invalid_pkt_type(uint32_t pkt_type)
{
	uint32_t l2, l3, l4, tnl, il2, il3, il4;

	l2 = pkt_type & RTE_PTYPE_L2_MASK;
	l3 = pkt_type & RTE_PTYPE_L3_MASK;
	l4 = pkt_type & RTE_PTYPE_L4_MASK;
	tnl = pkt_type & RTE_PTYPE_TUNNEL_MASK;
	il2 = pkt_type & RTE_PTYPE_INNER_L2_MASK;
	il3 = pkt_type & RTE_PTYPE_INNER_L3_MASK;
	il4 = pkt_type & RTE_PTYPE_INNER_L4_MASK;

	if (l2 &&
	    l2 != RTE_PTYPE_L2_ETHER &&
	    l2 != RTE_PTYPE_L2_ETHER_TIMESYNC &&
	    l2 != RTE_PTYPE_L2_ETHER_ARP &&
	    l2 != RTE_PTYPE_L2_ETHER_LLDP &&
	    l2 != RTE_PTYPE_L2_ETHER_NSH &&
	    l2 != RTE_PTYPE_L2_ETHER_VLAN &&
	    l2 != RTE_PTYPE_L2_ETHER_QINQ &&
	    l2 != RTE_PTYPE_L2_ETHER_PPPOE)
		return -1;

	if (l3 &&
	    l3 != RTE_PTYPE_L3_IPV4 &&
	    l3 != RTE_PTYPE_L3_IPV4_EXT &&
	    l3 != RTE_PTYPE_L3_IPV6 &&
	    l3 != RTE_PTYPE_L3_IPV4_EXT_UNKNOWN &&
	    l3 != RTE_PTYPE_L3_IPV6_EXT &&
	    l3 != RTE_PTYPE_L3_IPV6_EXT_UNKNOWN)
		return -1;

	if (l4 &&
	    l4 != RTE_PTYPE_L4_TCP &&
	    l4 != RTE_PTYPE_L4_UDP &&
	    l4 != RTE_PTYPE_L4_FRAG &&
	    l4 != RTE_PTYPE_L4_SCTP &&
	    l4 != RTE_PTYPE_L4_ICMP &&
	    l4 != RTE_PTYPE_L4_NONFRAG)
		return -1;

	if (tnl &&
	    tnl != RTE_PTYPE_TUNNEL_IP &&
	    tnl != RTE_PTYPE_TUNNEL_GRENAT &&
	    tnl != RTE_PTYPE_TUNNEL_VXLAN &&
	    tnl != RTE_PTYPE_TUNNEL_NVGRE &&
	    tnl != RTE_PTYPE_TUNNEL_GENEVE &&
	    tnl != RTE_PTYPE_TUNNEL_GTPC &&
	    tnl != RTE_PTYPE_TUNNEL_GTPU &&
	    tnl != RTE_PTYPE_TUNNEL_L2TP &&
	    tnl != RTE_PTYPE_TUNNEL_ESP)
		return -1;

	if (il2 &&
	    il2 != RTE_PTYPE_INNER_L2_ETHER &&
	    il2 != RTE_PTYPE_INNER_L2_ETHER_VLAN &&
	    il2 != RTE_PTYPE_INNER_L2_ETHER_QINQ)
		return -1;

	if (il3 &&
	    il3 != RTE_PTYPE_INNER_L3_IPV4 &&
	    il3 != RTE_PTYPE_INNER_L3_IPV4_EXT &&
	    il3 != RTE_PTYPE_INNER_L3_IPV6 &&
	    il3 != RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN &&
	    il3 != RTE_PTYPE_INNER_L3_IPV6_EXT &&
	    il3 != RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN)
		return -1;

	if (il4 &&
	    il4 != RTE_PTYPE_INNER_L4_TCP &&
	    il4 != RTE_PTYPE_INNER_L4_UDP &&
	    il4 != RTE_PTYPE_INNER_L4_FRAG &&
	    il4 != RTE_PTYPE_INNER_L4_SCTP &&
	    il4 != RTE_PTYPE_INNER_L4_ICMP &&
	    il4 != RTE_PTYPE_INNER_L4_NONFRAG)
		return -1;

	return 0;
}

static int check_invalid_ptype_mapping(
		struct rte_pmd_i40e_ptype_mapping *mapping_table,
		uint16_t count)
{
	int i;

	for (i = 0; i < count; i++) {
		uint16_t ptype = mapping_table[i].hw_ptype;
		uint32_t pkt_type = mapping_table[i].sw_ptype;

		if (ptype >= I40E_MAX_PKT_TYPE)
			return -1;

		if (pkt_type == RTE_PTYPE_UNKNOWN)
			continue;

		if (pkt_type & RTE_PMD_I40E_PTYPE_USER_DEFINE_MASK)
			continue;

		if (check_invalid_pkt_type(pkt_type))
			return -1;
	}

	return 0;
}

int
rte_pmd_i40e_ptype_mapping_update(
			uint16_t port,
			struct rte_pmd_i40e_ptype_mapping *mapping_items,
			uint16_t count,
			uint8_t exclusive)
{
	struct rte_eth_dev *dev;
	struct i40e_adapter *ad;
	int i;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	if (count > I40E_MAX_PKT_TYPE)
		return -EINVAL;

	if (check_invalid_ptype_mapping(mapping_items, count))
		return -EINVAL;

	ad = I40E_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);

	if (exclusive) {
		for (i = 0; i < I40E_MAX_PKT_TYPE; i++)
			ad->ptype_tbl[i] = RTE_PTYPE_UNKNOWN;
	}

	for (i = 0; i < count; i++)
		ad->ptype_tbl[mapping_items[i].hw_ptype]
			= mapping_items[i].sw_ptype;

	return 0;
}

int rte_pmd_i40e_ptype_mapping_reset(uint16_t port)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	i40e_set_default_ptype_table(dev);

	return 0;
}

int rte_pmd_i40e_ptype_mapping_get(
			uint16_t port,
			struct rte_pmd_i40e_ptype_mapping *mapping_items,
			uint16_t size,
			uint16_t *count,
			uint8_t valid_only)
{
	struct rte_eth_dev *dev;
	struct i40e_adapter *ad;
	int n = 0;
	uint16_t i;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	ad = I40E_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);

	for (i = 0; i < I40E_MAX_PKT_TYPE; i++) {
		if (n >= size)
			break;
		if (valid_only && ad->ptype_tbl[i] == RTE_PTYPE_UNKNOWN)
			continue;
		mapping_items[n].hw_ptype = i;
		mapping_items[n].sw_ptype = ad->ptype_tbl[i];
		n++;
	}

	*count = n;
	return 0;
}

int rte_pmd_i40e_ptype_mapping_replace(uint16_t port,
				       uint32_t target,
				       uint8_t mask,
				       uint32_t pkt_type)
{
	struct rte_eth_dev *dev;
	struct i40e_adapter *ad;
	uint16_t i;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	if (!mask && check_invalid_pkt_type(target))
		return -EINVAL;

	if (check_invalid_pkt_type(pkt_type))
		return -EINVAL;

	ad = I40E_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);

	for (i = 0; i < I40E_MAX_PKT_TYPE; i++) {
		if (mask) {
			if ((target | ad->ptype_tbl[i]) == target &&
			    (target & ad->ptype_tbl[i]))
				ad->ptype_tbl[i] = pkt_type;
		} else {
			if (ad->ptype_tbl[i] == target)
				ad->ptype_tbl[i] = pkt_type;
		}
	}

	return 0;
}

int
rte_pmd_i40e_add_vf_mac_addr(uint16_t port, uint16_t vf_id,
			     struct rte_ether_addr *mac_addr)
{
	struct rte_eth_dev *dev;
	struct i40e_pf_vf *vf;
	struct i40e_vsi *vsi;
	struct i40e_pf *pf;
	struct i40e_mac_filter_info mac_filter;
	int ret;

	if (mac_addr == NULL)
		return -EINVAL;

	if (i40e_validate_mac_addr((u8 *)mac_addr) != I40E_SUCCESS)
		return -EINVAL;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	if (vf_id >= pf->vf_num || !pf->vfs)
		return -EINVAL;

	vf = &pf->vfs[vf_id];
	vsi = vf->vsi;
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Invalid VSI.");
		return -EINVAL;
	}

	mac_filter.filter_type = I40E_MACVLAN_PERFECT_MATCH;
	rte_ether_addr_copy(mac_addr, &mac_filter.mac_addr);
	ret = i40e_vsi_add_mac(vsi, &mac_filter);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to add MAC filter.");
		return -1;
	}

	return 0;
}

int rte_pmd_i40e_flow_type_mapping_reset(uint16_t port)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	i40e_set_default_pctype_table(dev);

	return 0;
}

int rte_pmd_i40e_flow_type_mapping_get(
			uint16_t port,
			struct rte_pmd_i40e_flow_type_mapping *mapping_items)
{
	struct rte_eth_dev *dev;
	struct i40e_adapter *ad;
	uint16_t i;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	ad = I40E_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);

	for (i = 0; i < I40E_FLOW_TYPE_MAX; i++) {
		mapping_items[i].flow_type = i;
		mapping_items[i].pctype = ad->pctypes_tbl[i];
	}

	return 0;
}

int
rte_pmd_i40e_flow_type_mapping_update(
			uint16_t port,
			struct rte_pmd_i40e_flow_type_mapping *mapping_items,
			uint16_t count,
			uint8_t exclusive)
{
	struct rte_eth_dev *dev;
	struct i40e_adapter *ad;
	int i;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	if (count > I40E_FLOW_TYPE_MAX)
		return -EINVAL;

	for (i = 0; i < count; i++)
		if (mapping_items[i].flow_type >= I40E_FLOW_TYPE_MAX ||
		    mapping_items[i].flow_type == RTE_ETH_FLOW_UNKNOWN ||
		    (mapping_items[i].pctype &
		    (1ULL << I40E_FILTER_PCTYPE_INVALID)))
			return -EINVAL;

	ad = I40E_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);

	if (exclusive) {
		for (i = 0; i < I40E_FLOW_TYPE_MAX; i++)
			ad->pctypes_tbl[i] = 0ULL;
		ad->flow_types_mask = 0ULL;
	}

	for (i = 0; i < count; i++) {
		ad->pctypes_tbl[mapping_items[i].flow_type] =
						mapping_items[i].pctype;
		if (mapping_items[i].pctype)
			ad->flow_types_mask |=
					(1ULL << mapping_items[i].flow_type);
		else
			ad->flow_types_mask &=
					~(1ULL << mapping_items[i].flow_type);
	}

	for (i = 0, ad->pctypes_mask = 0ULL; i < I40E_FLOW_TYPE_MAX; i++)
		ad->pctypes_mask |= ad->pctypes_tbl[i];

	return 0;
}

int
rte_pmd_i40e_query_vfid_by_mac(uint16_t port,
			const struct rte_ether_addr *vf_mac)
{
	struct rte_eth_dev *dev;
	struct rte_ether_addr *mac;
	struct i40e_pf *pf;
	int vf_id;
	struct i40e_pf_vf *vf;
	uint16_t vf_num;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);
	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	vf_num = pf->vf_num;

	for (vf_id = 0; vf_id < vf_num; vf_id++) {
		vf = &pf->vfs[vf_id];
		mac = &vf->mac_addr;

		if (rte_is_same_ether_addr(mac, vf_mac))
			return vf_id;
	}

	return -EINVAL;
}

static int
i40e_vsi_update_queue_region_mapping(struct i40e_hw *hw,
			      struct i40e_pf *pf)
{
	uint16_t i;
	struct i40e_vsi *vsi = pf->main_vsi;
	uint16_t queue_offset, bsf, tc_index;
	struct i40e_vsi_context ctxt;
	struct i40e_aqc_vsi_properties_data *vsi_info;
	struct i40e_queue_regions *region_info =
				&pf->queue_region;
	int32_t ret = -EINVAL;

	if (!region_info->queue_region_number) {
		PMD_INIT_LOG(ERR, "there is no that region id been set before");
		return ret;
	}

	memset(&ctxt, 0, sizeof(struct i40e_vsi_context));

	/* Update Queue Pairs Mapping for currently enabled UPs */
	ctxt.seid = vsi->seid;
	ctxt.pf_num = hw->pf_id;
	ctxt.vf_num = 0;
	ctxt.uplink_seid = vsi->uplink_seid;
	ctxt.info = vsi->info;
	vsi_info = &ctxt.info;

	memset(vsi_info->tc_mapping, 0, sizeof(uint16_t) * 8);
	memset(vsi_info->queue_mapping, 0, sizeof(uint16_t) * 16);

	/* Configure queue region and queue mapping parameters,
	 * for enabled queue region, allocate queues to this region.
	 */

	for (i = 0; i < region_info->queue_region_number; i++) {
		tc_index = region_info->region[i].region_id;
		bsf = rte_bsf32(region_info->region[i].queue_num);
		queue_offset = region_info->region[i].queue_start_index;
		vsi_info->tc_mapping[tc_index] = rte_cpu_to_le_16(
			(queue_offset << I40E_AQ_VSI_TC_QUE_OFFSET_SHIFT) |
				(bsf << I40E_AQ_VSI_TC_QUE_NUMBER_SHIFT));
	}

	/* Associate queue number with VSI, Keep vsi->nb_qps unchanged */
	vsi_info->mapping_flags |=
			rte_cpu_to_le_16(I40E_AQ_VSI_QUE_MAP_CONTIG);
	vsi_info->queue_mapping[0] = rte_cpu_to_le_16(vsi->base_queue);
	vsi_info->valid_sections |=
		rte_cpu_to_le_16(I40E_AQ_VSI_PROP_QUEUE_MAP_VALID);

	/* Update the VSI after updating the VSI queue-mapping information */
	ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to configure queue region mapping = %d ",
				hw->aq.asq_last_status);
		return ret;
	}
	/* update the local VSI info with updated queue map */
	rte_memcpy(&vsi->info.tc_mapping, &ctxt.info.tc_mapping,
					sizeof(vsi->info.tc_mapping));
	rte_memcpy(&vsi->info.queue_mapping,
			&ctxt.info.queue_mapping,
			sizeof(vsi->info.queue_mapping));
	vsi->info.mapping_flags = ctxt.info.mapping_flags;
	vsi->info.valid_sections = 0;

	return 0;
}


static int
i40e_queue_region_set_region(struct i40e_pf *pf,
				struct rte_pmd_i40e_queue_region_conf *conf_ptr)
{
	uint16_t i;
	struct i40e_vsi *main_vsi = pf->main_vsi;
	struct i40e_queue_regions *info = &pf->queue_region;
	int32_t ret = -EINVAL;

	if (!((rte_is_power_of_2(conf_ptr->queue_num)) &&
				conf_ptr->queue_num <= 64)) {
		PMD_DRV_LOG(ERR, "The region sizes should be any of the following values: 1, 2, 4, 8, 16, 32, 64 as long as the "
			"total number of queues do not exceed the VSI allocation");
		return ret;
	}

	if (conf_ptr->region_id > I40E_REGION_MAX_INDEX) {
		PMD_DRV_LOG(ERR, "the queue region max index is 7");
		return ret;
	}

	if ((conf_ptr->queue_start_index + conf_ptr->queue_num)
					> main_vsi->nb_used_qps) {
		PMD_DRV_LOG(ERR, "the queue index exceeds the VSI range");
		return ret;
	}

	for (i = 0; i < info->queue_region_number; i++)
		if (conf_ptr->region_id == info->region[i].region_id)
			break;

	if (i == info->queue_region_number &&
				i <= I40E_REGION_MAX_INDEX) {
		info->region[i].region_id = conf_ptr->region_id;
		info->region[i].queue_num = conf_ptr->queue_num;
		info->region[i].queue_start_index =
			conf_ptr->queue_start_index;
		info->queue_region_number++;
	} else {
		PMD_DRV_LOG(ERR, "queue region number exceeds maxnum 8 or the queue region id has been set before");
		return ret;
	}

	return 0;
}

static int
i40e_queue_region_set_flowtype(struct i40e_pf *pf,
			struct rte_pmd_i40e_queue_region_conf *rss_region_conf)
{
	int32_t ret = -EINVAL;
	struct i40e_queue_regions *info = &pf->queue_region;
	uint16_t i, j;
	uint16_t region_index, flowtype_index;

	/* For the pctype or hardware flowtype of packet,
	 * the specific index for each type has been defined
	 * in file i40e_type.h as enum i40e_filter_pctype.
	 */

	if (rss_region_conf->region_id > I40E_PFQF_HREGION_MAX_INDEX) {
		PMD_DRV_LOG(ERR, "the queue region max index is 7");
		return ret;
	}

	if (rss_region_conf->hw_flowtype >= I40E_FILTER_PCTYPE_MAX) {
		PMD_DRV_LOG(ERR, "the hw_flowtype or PCTYPE max index is 63");
		return ret;
	}


	for (i = 0; i < info->queue_region_number; i++)
		if (rss_region_conf->region_id == info->region[i].region_id)
			break;

	if (i == info->queue_region_number) {
		PMD_DRV_LOG(ERR, "that region id has not been set before");
		ret = -EINVAL;
		return ret;
	}
	region_index = i;

	for (i = 0; i < info->queue_region_number; i++) {
		for (j = 0; j < info->region[i].flowtype_num; j++) {
			if (rss_region_conf->hw_flowtype ==
				info->region[i].hw_flowtype[j]) {
				PMD_DRV_LOG(ERR, "that hw_flowtype has been set before");
				return 0;
			}
		}
	}

	flowtype_index = info->region[region_index].flowtype_num;
	info->region[region_index].hw_flowtype[flowtype_index] =
					rss_region_conf->hw_flowtype;
	info->region[region_index].flowtype_num++;

	return 0;
}

static void
i40e_queue_region_pf_flowtype_conf(struct i40e_hw *hw,
				struct i40e_pf *pf)
{
	uint8_t hw_flowtype;
	uint32_t pfqf_hregion;
	uint16_t i, j, index;
	struct i40e_queue_regions *info = &pf->queue_region;

	/* For the pctype or hardware flowtype of packet,
	 * the specific index for each type has been defined
	 * in file i40e_type.h as enum i40e_filter_pctype.
	 */

	for (i = 0; i < info->queue_region_number; i++) {
		for (j = 0; j < info->region[i].flowtype_num; j++) {
			hw_flowtype = info->region[i].hw_flowtype[j];
			index = hw_flowtype >> 3;
			pfqf_hregion =
				i40e_read_rx_ctl(hw, I40E_PFQF_HREGION(index));

			if ((hw_flowtype & 0x7) == 0) {
				pfqf_hregion |= info->region[i].region_id <<
					I40E_PFQF_HREGION_REGION_0_SHIFT;
				pfqf_hregion |= 1 <<
					I40E_PFQF_HREGION_OVERRIDE_ENA_0_SHIFT;
			} else if ((hw_flowtype & 0x7) == 1) {
				pfqf_hregion |= info->region[i].region_id  <<
					I40E_PFQF_HREGION_REGION_1_SHIFT;
				pfqf_hregion |= 1 <<
					I40E_PFQF_HREGION_OVERRIDE_ENA_1_SHIFT;
			} else if ((hw_flowtype & 0x7) == 2) {
				pfqf_hregion |= info->region[i].region_id  <<
					I40E_PFQF_HREGION_REGION_2_SHIFT;
				pfqf_hregion |= 1 <<
					I40E_PFQF_HREGION_OVERRIDE_ENA_2_SHIFT;
			} else if ((hw_flowtype & 0x7) == 3) {
				pfqf_hregion |= info->region[i].region_id  <<
					I40E_PFQF_HREGION_REGION_3_SHIFT;
				pfqf_hregion |= 1 <<
					I40E_PFQF_HREGION_OVERRIDE_ENA_3_SHIFT;
			} else if ((hw_flowtype & 0x7) == 4) {
				pfqf_hregion |= info->region[i].region_id  <<
					I40E_PFQF_HREGION_REGION_4_SHIFT;
				pfqf_hregion |= 1 <<
					I40E_PFQF_HREGION_OVERRIDE_ENA_4_SHIFT;
			} else if ((hw_flowtype & 0x7) == 5) {
				pfqf_hregion |= info->region[i].region_id  <<
					I40E_PFQF_HREGION_REGION_5_SHIFT;
				pfqf_hregion |= 1 <<
					I40E_PFQF_HREGION_OVERRIDE_ENA_5_SHIFT;
			} else if ((hw_flowtype & 0x7) == 6) {
				pfqf_hregion |= info->region[i].region_id  <<
					I40E_PFQF_HREGION_REGION_6_SHIFT;
				pfqf_hregion |= 1 <<
					I40E_PFQF_HREGION_OVERRIDE_ENA_6_SHIFT;
			} else {
				pfqf_hregion |= info->region[i].region_id  <<
					I40E_PFQF_HREGION_REGION_7_SHIFT;
				pfqf_hregion |= 1 <<
					I40E_PFQF_HREGION_OVERRIDE_ENA_7_SHIFT;
			}

			i40e_write_rx_ctl(hw, I40E_PFQF_HREGION(index),
						pfqf_hregion);
		}
	}
}

static int
i40e_queue_region_set_user_priority(struct i40e_pf *pf,
		struct rte_pmd_i40e_queue_region_conf *rss_region_conf)
{
	struct i40e_queue_regions *info = &pf->queue_region;
	int32_t ret = -EINVAL;
	uint16_t i, j, region_index;

	if (rss_region_conf->user_priority >= I40E_MAX_USER_PRIORITY) {
		PMD_DRV_LOG(ERR, "the queue region max index is 7");
		return ret;
	}

	if (rss_region_conf->region_id > I40E_REGION_MAX_INDEX) {
		PMD_DRV_LOG(ERR, "the region_id max index is 7");
		return ret;
	}

	for (i = 0; i < info->queue_region_number; i++)
		if (rss_region_conf->region_id == info->region[i].region_id)
			break;

	if (i == info->queue_region_number) {
		PMD_DRV_LOG(ERR, "that region id has not been set before");
		ret = -EINVAL;
		return ret;
	}

	region_index = i;

	for (i = 0; i < info->queue_region_number; i++) {
		for (j = 0; j < info->region[i].user_priority_num; j++) {
			if (info->region[i].user_priority[j] ==
				rss_region_conf->user_priority) {
				PMD_DRV_LOG(ERR, "that user priority has been set before");
				return 0;
			}
		}
	}

	j = info->region[region_index].user_priority_num;
	info->region[region_index].user_priority[j] =
					rss_region_conf->user_priority;
	info->region[region_index].user_priority_num++;

	return 0;
}

static int
i40e_queue_region_dcb_configure(struct i40e_hw *hw,
				struct i40e_pf *pf)
{
	struct i40e_dcbx_config dcb_cfg_local;
	struct i40e_dcbx_config *dcb_cfg;
	struct i40e_queue_regions *info = &pf->queue_region;
	struct i40e_dcbx_config *old_cfg = &hw->local_dcbx_config;
	int32_t ret = -EINVAL;
	uint16_t i, j, prio_index, region_index;
	uint8_t tc_map, tc_bw, bw_lf, dcb_flag = 0;

	if (!info->queue_region_number) {
		PMD_DRV_LOG(ERR, "No queue region been set before");
		return ret;
	}

	for (i = 0; i < info->queue_region_number; i++) {
		if (info->region[i].user_priority_num) {
			dcb_flag = 1;
			break;
		}
	}

	if (dcb_flag == 0)
		return 0;

	dcb_cfg = &dcb_cfg_local;
	memset(dcb_cfg, 0, sizeof(struct i40e_dcbx_config));

	/* assume each tc has the same bw */
	tc_bw = I40E_MAX_PERCENT / info->queue_region_number;
	for (i = 0; i < info->queue_region_number; i++)
		dcb_cfg->etscfg.tcbwtable[i] = tc_bw;
	/* to ensure the sum of tcbw is equal to 100 */
	bw_lf = I40E_MAX_PERCENT %  info->queue_region_number;
	for (i = 0; i < bw_lf; i++)
		dcb_cfg->etscfg.tcbwtable[i]++;

	/* assume each tc has the same Transmission Selection Algorithm */
	for (i = 0; i < info->queue_region_number; i++)
		dcb_cfg->etscfg.tsatable[i] = I40E_IEEE_TSA_ETS;

	for (i = 0; i < info->queue_region_number; i++) {
		for (j = 0; j < info->region[i].user_priority_num; j++) {
			prio_index = info->region[i].user_priority[j];
			region_index = info->region[i].region_id;
			dcb_cfg->etscfg.prioritytable[prio_index] =
						region_index;
		}
	}

	/* FW needs one App to configure HW */
	dcb_cfg->numapps = I40E_DEFAULT_DCB_APP_NUM;
	dcb_cfg->app[0].selector = I40E_APP_SEL_ETHTYPE;
	dcb_cfg->app[0].priority = I40E_DEFAULT_DCB_APP_PRIO;
	dcb_cfg->app[0].protocolid = I40E_APP_PROTOID_FCOE;

	tc_map = RTE_LEN2MASK(info->queue_region_number, uint8_t);

	dcb_cfg->pfc.willing = 0;
	dcb_cfg->pfc.pfccap = I40E_MAX_TRAFFIC_CLASS;
	dcb_cfg->pfc.pfcenable = tc_map;

	/* Copy the new config to the current config */
	*old_cfg = *dcb_cfg;
	old_cfg->etsrec = old_cfg->etscfg;
	ret = i40e_set_dcb_config(hw);

	if (ret) {
		PMD_DRV_LOG(ERR, "Set queue region DCB Config failed, err %s aq_err %s",
			 i40e_stat_str(hw, ret),
			 i40e_aq_str(hw, hw->aq.asq_last_status));
		return ret;
	}

	return 0;
}

int
i40e_flush_queue_region_all_conf(struct rte_eth_dev *dev,
	struct i40e_hw *hw, struct i40e_pf *pf, uint16_t on)
{
	int32_t ret = -EINVAL;
	struct i40e_queue_regions *info = &pf->queue_region;
	struct i40e_vsi *main_vsi = pf->main_vsi;

	if (on) {
		i40e_queue_region_pf_flowtype_conf(hw, pf);

		ret = i40e_vsi_update_queue_region_mapping(hw, pf);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(INFO, "Failed to flush queue region mapping.");
			return ret;
		}

		ret = i40e_queue_region_dcb_configure(hw, pf);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(INFO, "Failed to flush dcb.");
			return ret;
		}

		return 0;
	}

	if (info->queue_region_number) {
		info->queue_region_number = 1;
		info->region[0].queue_num = main_vsi->nb_used_qps;
		info->region[0].queue_start_index = 0;

		ret = i40e_vsi_update_queue_region_mapping(hw, pf);
		if (ret != I40E_SUCCESS)
			PMD_DRV_LOG(INFO, "Failed to flush queue region mapping.");

		ret = i40e_dcb_init_configure(dev, TRUE);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(INFO, "Failed to flush dcb.");
			pf->flags &= ~I40E_FLAG_DCB;
		}

		i40e_init_queue_region_conf(dev);
	}
	return 0;
}

static int
i40e_queue_region_pf_check_rss(struct i40e_pf *pf)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	uint64_t hena;

	hena = (uint64_t)i40e_read_rx_ctl(hw, I40E_PFQF_HENA(0));
	hena |= ((uint64_t)i40e_read_rx_ctl(hw, I40E_PFQF_HENA(1))) << 32;

	if (!hena)
		return -ENOTSUP;

	return 0;
}

static int
i40e_queue_region_get_all_info(struct i40e_pf *pf,
		struct i40e_queue_regions *regions_ptr)
{
	struct i40e_queue_regions *info = &pf->queue_region;

	rte_memcpy(regions_ptr, info,
			sizeof(struct i40e_queue_regions));

	return 0;
}

int rte_pmd_i40e_rss_queue_region_conf(uint16_t port_id,
		enum rte_pmd_i40e_queue_region_op op_type, void *arg)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int32_t ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	if (!(!i40e_queue_region_pf_check_rss(pf)))
		return -ENOTSUP;

	/* This queue region feature only support pf by now. It should
	 * be called after dev_start, and will be clear after dev_stop.
	 * "RTE_PMD_I40E_RSS_QUEUE_REGION_ALL_FLUSH_ON"
	 * is just an enable function which server for other configuration,
	 * it is for all configuration about queue region from up layer,
	 * at first will only keep in DPDK softwarestored in driver,
	 * only after "FLUSH_ON", it commit all configuration to HW.
	 * Because PMD had to set hardware configuration at a time, so
	 * it will record all up layer command at first.
	 * "RTE_PMD_I40E_RSS_QUEUE_REGION_ALL_FLUSH_OFF" is
	 * just clean all configuration about queue region just now,
	 * and restore all to DPDK i40e driver default
	 * config when start up.
	 */

	switch (op_type) {
	case RTE_PMD_I40E_RSS_QUEUE_REGION_SET:
		ret = i40e_queue_region_set_region(pf,
				(struct rte_pmd_i40e_queue_region_conf *)arg);
		break;
	case RTE_PMD_I40E_RSS_QUEUE_REGION_FLOWTYPE_SET:
		ret = i40e_queue_region_set_flowtype(pf,
				(struct rte_pmd_i40e_queue_region_conf *)arg);
		break;
	case RTE_PMD_I40E_RSS_QUEUE_REGION_USER_PRIORITY_SET:
		ret = i40e_queue_region_set_user_priority(pf,
				(struct rte_pmd_i40e_queue_region_conf *)arg);
		break;
	case RTE_PMD_I40E_RSS_QUEUE_REGION_ALL_FLUSH_ON:
		ret = i40e_flush_queue_region_all_conf(dev, hw, pf, 1);
		break;
	case RTE_PMD_I40E_RSS_QUEUE_REGION_ALL_FLUSH_OFF:
		ret = i40e_flush_queue_region_all_conf(dev, hw, pf, 0);
		break;
	case RTE_PMD_I40E_RSS_QUEUE_REGION_INFO_GET:
		ret = i40e_queue_region_get_all_info(pf,
				(struct i40e_queue_regions *)arg);
		break;
	default:
		PMD_DRV_LOG(WARNING, "op type (%d) not supported",
			    op_type);
		ret = -EINVAL;
	}

	I40E_WRITE_FLUSH(hw);

	return ret;
}

int rte_pmd_i40e_flow_add_del_packet_template(
			uint16_t port,
			const struct rte_pmd_i40e_pkt_template_conf *conf,
			uint8_t add)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port];
	struct i40e_fdir_filter_conf filter_conf;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	if (conf == NULL)
		return -EINVAL;

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	memset(&filter_conf, 0, sizeof(filter_conf));
	filter_conf.soft_id = conf->soft_id;
	filter_conf.input.flow.raw_flow.pctype = conf->input.pctype;
	filter_conf.input.flow.raw_flow.packet = conf->input.packet;
	filter_conf.input.flow.raw_flow.length = conf->input.length;
	filter_conf.input.flow_ext.pkt_template = true;

	filter_conf.action.rx_queue = conf->action.rx_queue;
	filter_conf.action.behavior =
		(enum i40e_fdir_behavior)conf->action.behavior;
	filter_conf.action.report_status =
		(enum i40e_fdir_status)conf->action.report_status;
	filter_conf.action.flex_off = conf->action.flex_off;

	return i40e_flow_add_del_fdir_filter(dev, &filter_conf, add);
}

int
rte_pmd_i40e_inset_get(uint16_t port, uint8_t pctype,
		       struct rte_pmd_i40e_inset *inset,
		       enum rte_pmd_i40e_inset_type inset_type)
{
	struct rte_eth_dev *dev;
	struct i40e_hw *hw;
	uint64_t inset_reg;
	uint32_t mask_reg[2];
	int i;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	if (pctype > 63)
		return -EINVAL;

	hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	memset(inset, 0, sizeof(struct rte_pmd_i40e_inset));

	switch (inset_type) {
	case INSET_HASH:
		/* Get input set */
		inset_reg =
			i40e_read_rx_ctl(hw, I40E_GLQF_HASH_INSET(1, pctype));
		inset_reg <<= I40E_32_BIT_WIDTH;
		inset_reg |=
			i40e_read_rx_ctl(hw, I40E_GLQF_HASH_INSET(0, pctype));
		/* Get field mask */
		mask_reg[0] =
			i40e_read_rx_ctl(hw, I40E_GLQF_HASH_MSK(0, pctype));
		mask_reg[1] =
			i40e_read_rx_ctl(hw, I40E_GLQF_HASH_MSK(1, pctype));
		break;
	case INSET_FDIR:
		inset_reg =
			i40e_read_rx_ctl(hw, I40E_PRTQF_FD_INSET(pctype, 1));
		inset_reg <<= I40E_32_BIT_WIDTH;
		inset_reg |=
			i40e_read_rx_ctl(hw, I40E_PRTQF_FD_INSET(pctype, 0));
		mask_reg[0] =
			i40e_read_rx_ctl(hw, I40E_GLQF_FD_MSK(0, pctype));
		mask_reg[1] =
			i40e_read_rx_ctl(hw, I40E_GLQF_FD_MSK(1, pctype));
		break;
	case INSET_FDIR_FLX:
		inset_reg =
			i40e_read_rx_ctl(hw, I40E_PRTQF_FD_FLXINSET(pctype));
		mask_reg[0] =
			i40e_read_rx_ctl(hw, I40E_PRTQF_FD_MSK(pctype, 0));
		mask_reg[1] =
			i40e_read_rx_ctl(hw, I40E_PRTQF_FD_MSK(pctype, 1));
		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported input set type.");
		return -EINVAL;
	}

	inset->inset = inset_reg;

	for (i = 0; i < 2; i++) {
		inset->mask[i].field_idx = ((mask_reg[i] >> 16) & 0x3F);
		inset->mask[i].mask = mask_reg[i] & 0xFFFF;
	}

	return 0;
}

int
rte_pmd_i40e_inset_set(uint16_t port, uint8_t pctype,
		       struct rte_pmd_i40e_inset *inset,
		       enum rte_pmd_i40e_inset_type inset_type)
{
	struct rte_eth_dev *dev;
	struct i40e_hw *hw;
	struct i40e_pf *pf;
	uint64_t inset_reg;
	uint32_t mask_reg[2];
	int i;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	if (pctype > 63)
		return -EINVAL;

	hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	if (pf->support_multi_driver) {
		PMD_DRV_LOG(ERR, "Input set configuration is not supported.");
		return -ENOTSUP;
	}

	inset_reg = inset->inset;
	for (i = 0; i < 2; i++)
		mask_reg[i] = (inset->mask[i].field_idx << 16) |
			inset->mask[i].mask;

	switch (inset_type) {
	case INSET_HASH:
		i40e_check_write_global_reg(hw, I40E_GLQF_HASH_INSET(0, pctype),
					    (uint32_t)(inset_reg & UINT32_MAX));
		i40e_check_write_global_reg(hw, I40E_GLQF_HASH_INSET(1, pctype),
					    (uint32_t)((inset_reg >>
					     I40E_32_BIT_WIDTH) & UINT32_MAX));
		for (i = 0; i < 2; i++)
			i40e_check_write_global_reg(hw,
						  I40E_GLQF_HASH_MSK(i, pctype),
						  mask_reg[i]);
		break;
	case INSET_FDIR:
		i40e_check_write_reg(hw, I40E_PRTQF_FD_INSET(pctype, 0),
				     (uint32_t)(inset_reg & UINT32_MAX));
		i40e_check_write_reg(hw, I40E_PRTQF_FD_INSET(pctype, 1),
				     (uint32_t)((inset_reg >>
					      I40E_32_BIT_WIDTH) & UINT32_MAX));
		for (i = 0; i < 2; i++)
			i40e_check_write_global_reg(hw,
						    I40E_GLQF_FD_MSK(i, pctype),
						    mask_reg[i]);
		break;
	case INSET_FDIR_FLX:
		i40e_check_write_reg(hw, I40E_PRTQF_FD_FLXINSET(pctype),
				     (uint32_t)(inset_reg & UINT32_MAX));
		for (i = 0; i < 2; i++)
			i40e_check_write_reg(hw, I40E_PRTQF_FD_MSK(pctype, i),
					     mask_reg[i]);
		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported input set type.");
		return -EINVAL;
	}

	I40E_WRITE_FLUSH(hw);
	return 0;
}

int
rte_pmd_i40e_get_fdir_info(uint16_t port, struct rte_eth_fdir_info *fdir_info)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	i40e_fdir_info_get(dev, fdir_info);

	return 0;
}

int
rte_pmd_i40e_get_fdir_stats(uint16_t port, struct rte_eth_fdir_stats *fdir_stat)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	i40e_fdir_stats_get(dev, fdir_stat);

	return 0;
}

int
rte_pmd_i40e_set_gre_key_len(uint16_t port, uint8_t len)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;
	struct i40e_hw *hw;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	hw = I40E_PF_TO_HW(pf);

	return i40e_dev_set_gre_key_len(hw, len);
}

int
rte_pmd_i40e_set_switch_dev(uint16_t port_id, struct rte_eth_dev *switch_dev)
{
	struct rte_eth_dev *i40e_dev;
	struct i40e_hw *hw;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	i40e_dev = &rte_eth_devices[port_id];
	if (!is_i40e_supported(i40e_dev))
		return -ENOTSUP;

	hw = I40E_DEV_PRIVATE_TO_HW(i40e_dev->data->dev_private);
	if (!hw)
		return -1;

	hw->switch_dev = switch_dev;

	return 0;
}

int
rte_pmd_i40e_set_pf_src_prune(uint16_t port, uint8_t on)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_i40e_supported(dev))
		return -ENOTSUP;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	ret = i40e_pf_set_source_prune(pf, on);
	return ret;
}
