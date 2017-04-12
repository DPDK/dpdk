/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2017 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_malloc.h>
#include <rte_tailq.h>

#include "base/i40e_prototype.h"
#include "i40e_ethdev.h"
#include "i40e_pf.h"
#include "i40e_rxtx.h"
#include "rte_pmd_i40e.h"

/* The max bandwidth of i40e is 40Gbps. */
#define I40E_QOS_BW_MAX 40000
/* The bandwidth should be the multiple of 50Mbps. */
#define I40E_QOS_BW_GRANULARITY 50
/* The min bandwidth weight is 1. */
#define I40E_QOS_BW_WEIGHT_MIN 1
/* The max bandwidth weight is 127. */
#define I40E_QOS_BW_WEIGHT_MAX 127

int
rte_pmd_i40e_ping_vfs(uint8_t port, uint16_t vf)
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
rte_pmd_i40e_set_vf_mac_anti_spoof(uint8_t port, uint16_t vf_id, uint8_t on)
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
	(void)rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
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
rte_pmd_i40e_set_vf_vlan_anti_spoof(uint8_t port, uint16_t vf_id, uint8_t on)
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
	(void)rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
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
	enum rte_mac_filter_type filter_type;
	int ret = I40E_SUCCESS;
	void *temp;

	/* remove all the MACs */
	TAILQ_FOREACH_SAFE(f, &vsi->mac_list, next, temp) {
		vlan_num = vsi->vlan_num;
		filter_type = f->mac_info.filter_type;
		if (filter_type == RTE_MACVLAN_PERFECT_MATCH ||
		    filter_type == RTE_MACVLAN_HASH_MATCH) {
			if (vlan_num == 0) {
				PMD_DRV_LOG(ERR, "VLAN number shouldn't be 0");
				return I40E_ERR_PARAM;
			}
		} else if (filter_type == RTE_MAC_PERFECT_MATCH ||
			   filter_type == RTE_MAC_HASH_MATCH)
			vlan_num = 1;

		mv_f = rte_zmalloc("macvlan_data", vlan_num * sizeof(*mv_f), 0);
		if (!mv_f) {
			PMD_DRV_LOG(ERR, "failed to allocate memory");
			return I40E_ERR_NO_MEMORY;
		}

		for (i = 0; i < vlan_num; i++) {
			mv_f[i].filter_type = filter_type;
			(void)rte_memcpy(&mv_f[i].macaddr,
					 &f->mac_info.mac_addr,
					 ETH_ADDR_LEN);
		}
		if (filter_type == RTE_MACVLAN_PERFECT_MATCH ||
		    filter_type == RTE_MACVLAN_HASH_MATCH) {
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
	TAILQ_FOREACH_SAFE(f, &vsi->mac_list, next, temp) {
		if ((f->mac_info.filter_type == RTE_MACVLAN_PERFECT_MATCH) ||
		    (f->mac_info.filter_type == RTE_MACVLAN_HASH_MATCH)) {
			/**
			 * If vlan_num is 0, that's the first time to add mac,
			 * set mask for vlan_id 0.
			 */
			if (vsi->vlan_num == 0) {
				i40e_set_vlan_filter(vsi, 0, 1);
				vsi->vlan_num = 1;
			}
			vlan_num = vsi->vlan_num;
		} else if ((f->mac_info.filter_type == RTE_MAC_PERFECT_MATCH) ||
			   (f->mac_info.filter_type == RTE_MAC_HASH_MATCH))
			vlan_num = 1;

		mv_f = rte_zmalloc("macvlan_data", vlan_num * sizeof(*mv_f), 0);
		if (!mv_f) {
			PMD_DRV_LOG(ERR, "failed to allocate memory");
			return I40E_ERR_NO_MEMORY;
		}

		for (i = 0; i < vlan_num; i++) {
			mv_f[i].filter_type = f->mac_info.filter_type;
			(void)rte_memcpy(&mv_f[i].macaddr,
					 &f->mac_info.mac_addr,
					 ETH_ADDR_LEN);
		}

		if (f->mac_info.filter_type == RTE_MACVLAN_PERFECT_MATCH ||
		    f->mac_info.filter_type == RTE_MACVLAN_HASH_MATCH) {
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
	if (hw->aq.fw_maj_ver < 5) {
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
	(void)rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
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
rte_pmd_i40e_set_tx_loopback(uint8_t port, uint8_t on)
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
rte_pmd_i40e_set_vf_unicast_promisc(uint8_t port, uint16_t vf_id, uint8_t on)
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
rte_pmd_i40e_set_vf_multicast_promisc(uint8_t port, uint16_t vf_id, uint8_t on)
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
rte_pmd_i40e_set_vf_mac_addr(uint8_t port, uint16_t vf_id,
			     struct ether_addr *mac_addr)
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

	ether_addr_copy(mac_addr, &vf->mac_addr);

	/* Remove all existing mac */
	TAILQ_FOREACH_SAFE(f, &vsi->mac_list, next, temp)
		i40e_vsi_delete_mac(vsi, &f->mac_info.mac_addr);

	return 0;
}

/* Set vlan strip on/off for specific VF from host */
int
rte_pmd_i40e_set_vf_vlan_stripq(uint8_t port, uint16_t vf_id, uint8_t on)
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

int rte_pmd_i40e_set_vf_vlan_insert(uint8_t port, uint16_t vf_id,
				    uint16_t vlan_id)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;
	struct i40e_hw *hw;
	struct i40e_vsi *vsi;
	struct i40e_vsi_context ctxt;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	if (vlan_id > ETHER_MAX_VLAN_ID) {
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
	(void)rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
	ctxt.seid = vsi->seid;

	hw = I40E_VSI_TO_HW(vsi);
	ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
	if (ret != I40E_SUCCESS) {
		ret = -ENOTSUP;
		PMD_DRV_LOG(ERR, "Failed to update VSI params");
	}

	return ret;
}

int rte_pmd_i40e_set_vf_broadcast(uint8_t port, uint16_t vf_id,
				  uint8_t on)
{
	struct rte_eth_dev *dev;
	struct i40e_pf *pf;
	struct i40e_vsi *vsi;
	struct i40e_hw *hw;
	struct i40e_mac_filter_info filter;
	struct ether_addr broadcast = {
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
		(void)rte_memcpy(&filter.mac_addr, &broadcast, ETHER_ADDR_LEN);
		filter.filter_type = RTE_MACVLAN_PERFECT_MATCH;
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

int rte_pmd_i40e_set_vf_vlan_tag(uint8_t port, uint16_t vf_id, uint8_t on)
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
	(void)rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
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

int rte_pmd_i40e_set_vf_vlan_filter(uint8_t port, uint16_t vlan_id,
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

	if (vlan_id > ETHER_MAX_VLAN_ID || !vlan_id) {
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
rte_pmd_i40e_get_vf_stats(uint8_t port,
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
rte_pmd_i40e_reset_vf_stats(uint8_t port,
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
rte_pmd_i40e_set_vf_max_bw(uint8_t port, uint16_t vf_id, uint32_t bw)
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
rte_pmd_i40e_set_vf_tc_bw_alloc(uint8_t port, uint16_t vf_id,
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
rte_pmd_i40e_set_vf_tc_max_bw(uint8_t port, uint16_t vf_id,
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
rte_pmd_i40e_set_tc_strict_prio(uint8_t port, uint8_t tc_map)
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
		ret = i40e_aq_stop_lldp(hw, true, NULL);
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
			/* For rubust, if bandwidth is 0, use 1 instead. */
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
		ret = i40e_aq_start_lldp(hw, NULL);
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

#define I40E_PROFILE_INFO_SIZE 48
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

#define I40E_PROFILE_INFO_SIZE 48
#define I40E_MAX_PROFILE_NUM 16

/* Check if the profile info exists */
static int
i40e_check_profile_info(uint8_t port, uint8_t *profile_info_sec)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port];
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint8_t *buff;
	struct rte_pmd_i40e_profile_list *p_list;
	struct rte_pmd_i40e_profile_info *pinfo, *p;
	uint32_t i;
	int ret;

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
	pinfo = (struct rte_pmd_i40e_profile_info *)(profile_info_sec +
			     sizeof(struct i40e_profile_section_header));
	for (i = 0; i < p_list->p_count; i++) {
		p = &p_list->p_info[i];
		if ((pinfo->track_id == p->track_id) &&
		    !memcmp(&pinfo->version, &p->version,
			    sizeof(struct i40e_ddp_version)) &&
		    !memcmp(&pinfo->name, &p->name,
			    I40E_DDP_NAME_SIZE)) {
			PMD_DRV_LOG(INFO, "Profile exists.");
			rte_free(buff);
			return 1;
		}
	}

	rte_free(buff);
	return 0;
}

int
rte_pmd_i40e_process_ddp_package(uint8_t port, uint8_t *buff,
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

	if (op == RTE_PMD_I40E_PKG_OP_WR_ADD) {
		/* Check if the profile exists */
		i40e_generate_profile_info_sec(
		     ((struct i40e_profile_segment *)profile_seg_hdr)->name,
		     &((struct i40e_profile_segment *)profile_seg_hdr)->version,
		     track_id, profile_info_sec, 1);
		is_exist = i40e_check_profile_info(port, profile_info_sec);
		if (is_exist > 0) {
			PMD_DRV_LOG(ERR, "Profile already exists.");
			rte_free(profile_info_sec);
			return 1;
		} else if (is_exist < 0) {
			PMD_DRV_LOG(ERR, "Failed to check profile.");
			rte_free(profile_info_sec);
			return -EINVAL;
		}

		/* Write profile to HW */
		status = i40e_write_profile(
				hw,
				(struct i40e_profile_segment *)profile_seg_hdr,
				track_id);
		if (status) {
			PMD_DRV_LOG(ERR, "Failed to write profile.");
			rte_free(profile_info_sec);
			return status;
		}

		/* Add profile info to info list */
		status = i40e_add_rm_profile_info(hw, profile_info_sec);
		if (status)
			PMD_DRV_LOG(ERR, "Failed to add profile info.");
	} else {
		PMD_DRV_LOG(ERR, "Operation not supported.");
	}

	rte_free(profile_info_sec);
	return status;
}

int
rte_pmd_i40e_get_ddp_list(uint8_t port, uint8_t *buff, uint32_t size)
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
	    l2 != RTE_PTYPE_L2_ETHER_QINQ)
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
	    tnl != RTE_PTYPE_TUNNEL_GRENAT)
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
			uint8_t port,
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

int rte_pmd_i40e_ptype_mapping_reset(uint8_t port)
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
			uint8_t port,
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

int rte_pmd_i40e_ptype_mapping_replace(uint8_t port,
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
