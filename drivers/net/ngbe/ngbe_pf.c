/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <rte_ether.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_bus_pci.h>

#include "base/ngbe.h"
#include "ngbe_ethdev.h"

#define NGBE_MAX_VFTA     (128)

static inline uint16_t
dev_num_vf(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	/* EM only support 7 VFs. */
	return pci_dev->max_vfs;
}

static inline
int ngbe_vf_perm_addr_gen(struct rte_eth_dev *dev, uint16_t vf_num)
{
	unsigned char vf_mac_addr[RTE_ETHER_ADDR_LEN];
	struct ngbe_vf_info *vfinfo = *NGBE_DEV_VFDATA(dev);
	uint16_t vfn;

	for (vfn = 0; vfn < vf_num; vfn++) {
		rte_eth_random_addr(vf_mac_addr);
		/* keep the random address as default */
		memcpy(vfinfo[vfn].vf_mac_addresses, vf_mac_addr,
			   RTE_ETHER_ADDR_LEN);
	}

	return 0;
}

int ngbe_pf_host_init(struct rte_eth_dev *eth_dev)
{
	struct ngbe_vf_info **vfinfo = NGBE_DEV_VFDATA(eth_dev);
	struct ngbe_uta_info *uta_info = NGBE_DEV_UTA_INFO(eth_dev);
	struct ngbe_hw *hw = ngbe_dev_hw(eth_dev);
	uint16_t vf_num;
	uint8_t nb_queue = 1;
	int ret = 0;

	PMD_INIT_FUNC_TRACE();

	RTE_ETH_DEV_SRIOV(eth_dev).active = 0;
	vf_num = dev_num_vf(eth_dev);
	if (vf_num == 0)
		return ret;

	*vfinfo = rte_zmalloc("vf_info",
			sizeof(struct ngbe_vf_info) * vf_num, 0);
	if (*vfinfo == NULL) {
		PMD_INIT_LOG(ERR,
			"Cannot allocate memory for private VF data\n");
		return -ENOMEM;
	}

	ret = rte_eth_switch_domain_alloc(&(*vfinfo)->switch_domain_id);
	if (ret) {
		PMD_INIT_LOG(ERR,
			"failed to allocate switch domain for device %d", ret);
		rte_free(*vfinfo);
		*vfinfo = NULL;
		return ret;
	}

	memset(uta_info, 0, sizeof(struct ngbe_uta_info));
	hw->mac.mc_filter_type = 0;

	RTE_ETH_DEV_SRIOV(eth_dev).active = RTE_ETH_8_POOLS;
	RTE_ETH_DEV_SRIOV(eth_dev).nb_q_per_pool = nb_queue;
	RTE_ETH_DEV_SRIOV(eth_dev).def_pool_q_idx =
			(uint16_t)(vf_num * nb_queue);

	ngbe_vf_perm_addr_gen(eth_dev, vf_num);

	/* init_mailbox_params */
	hw->mbx.init_params(hw);

	return ret;
}

void ngbe_pf_host_uninit(struct rte_eth_dev *eth_dev)
{
	struct ngbe_vf_info **vfinfo;
	uint16_t vf_num;
	int ret;

	PMD_INIT_FUNC_TRACE();

	RTE_ETH_DEV_SRIOV(eth_dev).active = 0;
	RTE_ETH_DEV_SRIOV(eth_dev).nb_q_per_pool = 0;
	RTE_ETH_DEV_SRIOV(eth_dev).def_pool_q_idx = 0;

	vf_num = dev_num_vf(eth_dev);
	if (vf_num == 0)
		return;

	vfinfo = NGBE_DEV_VFDATA(eth_dev);
	if (*vfinfo == NULL)
		return;

	ret = rte_eth_switch_domain_free((*vfinfo)->switch_domain_id);
	if (ret)
		PMD_INIT_LOG(WARNING, "failed to free switch domain: %d", ret);

	rte_free(*vfinfo);
	*vfinfo = NULL;
}

int ngbe_pf_host_configure(struct rte_eth_dev *eth_dev)
{
	uint32_t vtctl, fcrth;
	uint32_t vfre_offset;
	uint16_t vf_num;
	const uint8_t VFRE_SHIFT = 5;  /* VFRE 32 bits per slot */
	const uint8_t VFRE_MASK = (uint8_t)((1U << VFRE_SHIFT) - 1);
	struct ngbe_hw *hw = ngbe_dev_hw(eth_dev);
	uint32_t gpie;
	uint32_t gcr_ext;
	uint32_t vlanctrl;
	int i;

	vf_num = dev_num_vf(eth_dev);
	if (vf_num == 0)
		return -1;

	/* set the default pool for PF */
	vtctl = rd32(hw, NGBE_POOLCTL);
	vtctl &= ~NGBE_POOLCTL_DEFPL_MASK;
	vtctl |= NGBE_POOLCTL_DEFPL(vf_num);
	vtctl |= NGBE_POOLCTL_RPLEN;
	wr32(hw, NGBE_POOLCTL, vtctl);

	vfre_offset = vf_num & VFRE_MASK;

	/* Enable pools reserved to PF only */
	wr32(hw, NGBE_POOLRXENA(0), (~0U) << vfre_offset);
	wr32(hw, NGBE_POOLTXENA(0), (~0U) << vfre_offset);

	wr32(hw, NGBE_PSRCTL, NGBE_PSRCTL_LBENA);

	/* clear VMDq map to perment rar 0 */
	hw->mac.clear_vmdq(hw, 0, BIT_MASK32);

	/* clear VMDq map to scan rar 31 */
	wr32(hw, NGBE_ETHADDRIDX, hw->mac.num_rar_entries);
	wr32(hw, NGBE_ETHADDRASS, 0);

	/* set VMDq map to default PF pool */
	hw->mac.set_vmdq(hw, 0, vf_num);

	/*
	 * SW msut set PORTCTL.VT_Mode the same as GPIE.VT_Mode
	 */
	gpie = rd32(hw, NGBE_GPIE);
	gpie |= NGBE_GPIE_MSIX;
	gcr_ext = rd32(hw, NGBE_PORTCTL);
	gcr_ext &= ~NGBE_PORTCTL_NUMVT_MASK;

	if (RTE_ETH_DEV_SRIOV(eth_dev).active == RTE_ETH_8_POOLS)
		gcr_ext |= NGBE_PORTCTL_NUMVT_8;

	wr32(hw, NGBE_PORTCTL, gcr_ext);
	wr32(hw, NGBE_GPIE, gpie);

	/*
	 * enable vlan filtering and allow all vlan tags through
	 */
	vlanctrl = rd32(hw, NGBE_VLANCTL);
	vlanctrl |= NGBE_VLANCTL_VFE; /* enable vlan filters */
	wr32(hw, NGBE_VLANCTL, vlanctrl);

	/* enable all vlan filters */
	for (i = 0; i < NGBE_MAX_VFTA; i++)
		wr32(hw, NGBE_VLANTBL(i), 0xFFFFFFFF);

	/* Enable MAC Anti-Spoofing */
	hw->mac.set_mac_anti_spoofing(hw, FALSE, vf_num);

	/* set flow control threshold to max to avoid tx switch hang */
	wr32(hw, NGBE_FCWTRLO, 0);
	fcrth = rd32(hw, NGBE_PBRXSIZE) - 32;
	wr32(hw, NGBE_FCWTRHI, fcrth);

	return 0;
}

