/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>

#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <rte_memcpy.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_bus_pci.h>

#include "base/txgbe.h"
#include "txgbe_ethdev.h"
#include "rte_pmd_txgbe.h"

#define TXGBE_MAX_VFTA     (128)
#define TXGBE_VF_MSG_SIZE_DEFAULT 1
#define TXGBE_VF_GET_QUEUE_MSG_SIZE 5

static inline uint16_t
dev_num_vf(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	return pci_dev->max_vfs;
}

static inline
int txgbe_vf_perm_addr_gen(struct rte_eth_dev *dev, uint16_t vf_num)
{
	unsigned char vf_mac_addr[RTE_ETHER_ADDR_LEN];
	struct txgbe_vf_info *vfinfo = *TXGBE_DEV_VFDATA(dev);
	uint16_t vfn;

	for (vfn = 0; vfn < vf_num; vfn++) {
		rte_eth_random_addr(vf_mac_addr);
		/* keep the random address as default */
		memcpy(vfinfo[vfn].vf_mac_addresses, vf_mac_addr,
			   RTE_ETHER_ADDR_LEN);
	}

	return 0;
}

static inline int
txgbe_mb_intr_setup(struct rte_eth_dev *dev)
{
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);

	intr->mask_misc |= TXGBE_ICRMISC_VFMBX;

	return 0;
}

int txgbe_pf_host_init(struct rte_eth_dev *eth_dev)
{
	struct txgbe_vf_info **vfinfo = TXGBE_DEV_VFDATA(eth_dev);
	struct txgbe_uta_info *uta_info = TXGBE_DEV_UTA_INFO(eth_dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);
	uint16_t vf_num;
	uint8_t nb_queue;
	int ret = 0;

	PMD_INIT_FUNC_TRACE();

	RTE_ETH_DEV_SRIOV(eth_dev).active = 0;
	vf_num = dev_num_vf(eth_dev);
	if (vf_num == 0)
		return ret;

	*vfinfo = rte_zmalloc("vf_info",
			sizeof(struct txgbe_vf_info) * vf_num, 0);
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

	memset(uta_info, 0, sizeof(struct txgbe_uta_info));
	hw->mac.mc_filter_type = 0;

	if (vf_num >= RTE_ETH_32_POOLS) {
		nb_queue = 2;
		RTE_ETH_DEV_SRIOV(eth_dev).active = RTE_ETH_64_POOLS;
	} else if (vf_num >= RTE_ETH_16_POOLS) {
		nb_queue = 4;
		RTE_ETH_DEV_SRIOV(eth_dev).active = RTE_ETH_32_POOLS;
	} else {
		nb_queue = 8;
		RTE_ETH_DEV_SRIOV(eth_dev).active = RTE_ETH_16_POOLS;
	}

	RTE_ETH_DEV_SRIOV(eth_dev).nb_q_per_pool = nb_queue;
	RTE_ETH_DEV_SRIOV(eth_dev).def_vmdq_idx = vf_num;
	RTE_ETH_DEV_SRIOV(eth_dev).def_pool_q_idx =
			(uint16_t)(vf_num * nb_queue);

	txgbe_vf_perm_addr_gen(eth_dev, vf_num);

	/* init_mailbox_params */
	hw->mbx.init_params(hw);

	/* set mb interrupt mask */
	txgbe_mb_intr_setup(eth_dev);

	return ret;
}

void txgbe_pf_host_uninit(struct rte_eth_dev *eth_dev)
{
	struct txgbe_vf_info **vfinfo;
	uint16_t vf_num;
	int ret;

	PMD_INIT_FUNC_TRACE();

	RTE_ETH_DEV_SRIOV(eth_dev).active = 0;
	RTE_ETH_DEV_SRIOV(eth_dev).nb_q_per_pool = 0;
	RTE_ETH_DEV_SRIOV(eth_dev).def_vmdq_idx = 0;
	RTE_ETH_DEV_SRIOV(eth_dev).def_pool_q_idx = 0;

	vf_num = dev_num_vf(eth_dev);
	if (vf_num == 0)
		return;

	vfinfo = TXGBE_DEV_VFDATA(eth_dev);
	if (*vfinfo == NULL)
		return;

	ret = rte_eth_switch_domain_free((*vfinfo)->switch_domain_id);
	if (ret)
		PMD_INIT_LOG(WARNING, "failed to free switch domain: %d", ret);

	rte_free(*vfinfo);
	*vfinfo = NULL;
}

static void
txgbe_add_tx_flow_control_drop_filter(struct rte_eth_dev *eth_dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);
	struct txgbe_filter_info *filter_info = TXGBE_DEV_FILTER(eth_dev);
	uint16_t vf_num;
	int i;
	struct txgbe_ethertype_filter ethertype_filter;

	if (!hw->mac.set_ethertype_anti_spoofing) {
		PMD_DRV_LOG(INFO, "ether type anti-spoofing is not supported.\n");
		return;
	}

	i = txgbe_ethertype_filter_lookup(filter_info,
					  TXGBE_ETHERTYPE_FLOW_CTRL);
	if (i >= 0) {
		PMD_DRV_LOG(ERR, "A ether type filter entity for flow control already exists!\n");
		return;
	}

	ethertype_filter.ethertype = TXGBE_ETHERTYPE_FLOW_CTRL;
	ethertype_filter.etqf = TXGBE_ETFLT_ENA |
				TXGBE_ETFLT_TXAS |
				TXGBE_ETHERTYPE_FLOW_CTRL;
	ethertype_filter.etqs = 0;
	ethertype_filter.conf = TRUE;
	i = txgbe_ethertype_filter_insert(filter_info,
					  &ethertype_filter);
	if (i < 0) {
		PMD_DRV_LOG(ERR, "Cannot find an unused ether type filter entity for flow control.\n");
		return;
	}

	wr32(hw, TXGBE_ETFLT(i),
			(TXGBE_ETFLT_ENA |
			TXGBE_ETFLT_TXAS |
			TXGBE_ETHERTYPE_FLOW_CTRL));

	vf_num = dev_num_vf(eth_dev);
	for (i = 0; i < vf_num; i++)
		hw->mac.set_ethertype_anti_spoofing(hw, true, i);
}

int txgbe_pf_host_configure(struct rte_eth_dev *eth_dev)
{
	uint32_t vtctl, fcrth;
	uint32_t vfre_slot, vfre_offset;
	uint16_t vf_num;
	const uint8_t VFRE_SHIFT = 5;  /* VFRE 32 bits per slot */
	const uint8_t VFRE_MASK = (uint8_t)((1U << VFRE_SHIFT) - 1);
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);
	uint32_t gpie;
	uint32_t gcr_ext;
	uint32_t vlanctrl;
	int i;

	vf_num = dev_num_vf(eth_dev);
	if (vf_num == 0)
		return -1;

	/* enable VMDq and set the default pool for PF */
	vtctl = rd32(hw, TXGBE_POOLCTL);
	vtctl &= ~TXGBE_POOLCTL_DEFPL_MASK;
	vtctl |= TXGBE_POOLCTL_DEFPL(RTE_ETH_DEV_SRIOV(eth_dev).def_vmdq_idx);
	vtctl |= TXGBE_POOLCTL_RPLEN;
	wr32(hw, TXGBE_POOLCTL, vtctl);

	vfre_offset = vf_num & VFRE_MASK;
	vfre_slot = (vf_num >> VFRE_SHIFT) > 0 ? 1 : 0;

	/* Enable pools reserved to PF only */
	wr32(hw, TXGBE_POOLRXENA(vfre_slot), (~0U) << vfre_offset);
	wr32(hw, TXGBE_POOLRXENA(vfre_slot ^ 1), vfre_slot - 1);
	wr32(hw, TXGBE_POOLTXENA(vfre_slot), (~0U) << vfre_offset);
	wr32(hw, TXGBE_POOLTXENA(vfre_slot ^ 1), vfre_slot - 1);

	wr32(hw, TXGBE_PSRCTL, TXGBE_PSRCTL_LBENA);

	/* clear VMDq map to permanent rar 0 */
	hw->mac.clear_vmdq(hw, 0, BIT_MASK32);

	/* clear VMDq map to scan rar 127 */
	wr32(hw, TXGBE_ETHADDRIDX, hw->mac.num_rar_entries);
	wr32(hw, TXGBE_ETHADDRASSL, 0);
	wr32(hw, TXGBE_ETHADDRASSH, 0);

	/* set VMDq map to default PF pool */
	hw->mac.set_vmdq(hw, 0, RTE_ETH_DEV_SRIOV(eth_dev).def_vmdq_idx);

	/*
	 * SW msut set PORTCTL.VT_Mode the same as GPIE.VT_Mode
	 */
	gpie = rd32(hw, TXGBE_GPIE);
	gpie |= TXGBE_GPIE_MSIX;
	gcr_ext = rd32(hw, TXGBE_PORTCTL);
	gcr_ext &= ~TXGBE_PORTCTL_NUMVT_MASK;

	switch (RTE_ETH_DEV_SRIOV(eth_dev).active) {
	case RTE_ETH_64_POOLS:
		gcr_ext |= TXGBE_PORTCTL_NUMVT_64;
		break;
	case RTE_ETH_32_POOLS:
		gcr_ext |= TXGBE_PORTCTL_NUMVT_32;
		break;
	case RTE_ETH_16_POOLS:
		gcr_ext |= TXGBE_PORTCTL_NUMVT_16;
		break;
	}

	wr32(hw, TXGBE_PORTCTL, gcr_ext);
	wr32(hw, TXGBE_GPIE, gpie);

	/*
	 * enable vlan filtering and allow all vlan tags through
	 */
	vlanctrl = rd32(hw, TXGBE_VLANCTL);
	vlanctrl |= TXGBE_VLANCTL_VFE; /* enable vlan filters */
	wr32(hw, TXGBE_VLANCTL, vlanctrl);

	/* enable all vlan filters */
	for (i = 0; i < TXGBE_MAX_VFTA; i++)
		wr32(hw, TXGBE_VLANTBL(i), 0xFFFFFFFF);

	/* Enable MAC Anti-Spoofing */
	hw->mac.set_mac_anti_spoofing(hw, FALSE, vf_num);

	/* set flow control threshold to max to avoid tx switch hang */
	for (i = 0; i < TXGBE_DCB_TC_MAX; i++) {
		wr32(hw, TXGBE_FCWTRLO(i), 0);
		fcrth = rd32(hw, TXGBE_PBRXSIZE(i)) - 32;
		wr32(hw, TXGBE_FCWTRHI(i), fcrth);
	}

	txgbe_add_tx_flow_control_drop_filter(eth_dev);

	return 0;
}

static void
txgbe_set_rx_mode(struct rte_eth_dev *eth_dev)
{
	struct rte_eth_dev_data *dev_data = eth_dev->data;
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);
	u32 fctrl, vmolr;
	uint16_t vfn = dev_num_vf(eth_dev);

	/* disable store-bad-packets */
	wr32m(hw, TXGBE_SECRXCTL, TXGBE_SECRXCTL_SAVEBAD, 0);

	/* Check for Promiscuous and All Multicast modes */
	fctrl = rd32m(hw, TXGBE_PSRCTL,
			~(TXGBE_PSRCTL_UCP | TXGBE_PSRCTL_MCP));
	fctrl |= TXGBE_PSRCTL_BCA |
		 TXGBE_PSRCTL_MCHFENA;

	vmolr = rd32m(hw, TXGBE_POOLETHCTL(vfn),
			~(TXGBE_POOLETHCTL_UCP |
			  TXGBE_POOLETHCTL_MCP |
			  TXGBE_POOLETHCTL_UCHA |
			  TXGBE_POOLETHCTL_MCHA));
	vmolr |= TXGBE_POOLETHCTL_BCA |
		 TXGBE_POOLETHCTL_UTA |
		 TXGBE_POOLETHCTL_VLA;

	if (dev_data->promiscuous) {
		fctrl |= TXGBE_PSRCTL_UCP |
			 TXGBE_PSRCTL_MCP;
		/* pf don't want packets routing to vf, so clear UPE */
		vmolr |= TXGBE_POOLETHCTL_MCP;
	} else if (dev_data->all_multicast) {
		fctrl |= TXGBE_PSRCTL_MCP;
		vmolr |= TXGBE_POOLETHCTL_MCP;
	} else {
		vmolr |= TXGBE_POOLETHCTL_UCHA;
		vmolr |= TXGBE_POOLETHCTL_MCHA;
	}

	wr32(hw, TXGBE_POOLETHCTL(vfn), vmolr);

	wr32(hw, TXGBE_PSRCTL, fctrl);

	txgbe_vlan_hw_strip_config(eth_dev);
}

static inline void
txgbe_vf_reset_event(struct rte_eth_dev *eth_dev, uint16_t vf)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);
	struct txgbe_vf_info *vfinfo = *(TXGBE_DEV_VFDATA(eth_dev));
	int rar_entry = hw->mac.num_rar_entries - (vf + 1);
	uint32_t vmolr = rd32(hw, TXGBE_POOLETHCTL(vf));

	vmolr |= (TXGBE_POOLETHCTL_UCHA |
			TXGBE_POOLETHCTL_BCA | TXGBE_POOLETHCTL_UTA);
	wr32(hw, TXGBE_POOLETHCTL(vf), vmolr);

	wr32(hw, TXGBE_POOLTAG(vf), 0);

	/* reset multicast table array for vf */
	vfinfo[vf].num_vf_mc_hashes = 0;

	/* reset rx mode */
	txgbe_set_rx_mode(eth_dev);

	hw->mac.clear_rar(hw, rar_entry);
}

static inline void
txgbe_vf_reset_msg(struct rte_eth_dev *eth_dev, uint16_t vf)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);
	uint32_t reg;
	uint32_t reg_offset, vf_shift;
	const uint8_t VFRE_SHIFT = 5;  /* VFRE 32 bits per slot */
	const uint8_t VFRE_MASK = (uint8_t)((1U << VFRE_SHIFT) - 1);
	uint8_t  nb_q_per_pool;
	int i;

	vf_shift = vf & VFRE_MASK;
	reg_offset = (vf >> VFRE_SHIFT) > 0 ? 1 : 0;

	/* enable transmit for vf */
	reg = rd32(hw, TXGBE_POOLTXENA(reg_offset));
	reg |= (reg | (1 << vf_shift));
	wr32(hw, TXGBE_POOLTXENA(reg_offset), reg);

	/* enable all queue drop for IOV */
	nb_q_per_pool = RTE_ETH_DEV_SRIOV(eth_dev).nb_q_per_pool;
	for (i = vf * nb_q_per_pool; i < (vf + 1) * nb_q_per_pool; i++) {
		txgbe_flush(hw);
		reg = 1 << (i % 32);
		wr32m(hw, TXGBE_QPRXDROP(i / 32), reg, reg);
	}

	/* enable receive for vf */
	reg = rd32(hw, TXGBE_POOLRXENA(reg_offset));
	reg |= (reg | (1 << vf_shift));
	wr32(hw, TXGBE_POOLRXENA(reg_offset), reg);

	txgbe_vf_reset_event(eth_dev, vf);
}

static int
txgbe_disable_vf_mc_promisc(struct rte_eth_dev *eth_dev, uint32_t vf)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);
	uint32_t vmolr;

	vmolr = rd32(hw, TXGBE_POOLETHCTL(vf));

	PMD_DRV_LOG(INFO, "VF %u: disabling multicast promiscuous\n", vf);

	vmolr &= ~TXGBE_POOLETHCTL_MCP;

	wr32(hw, TXGBE_POOLETHCTL(vf), vmolr);

	return 0;
}

static int
txgbe_vf_reset(struct rte_eth_dev *eth_dev, uint16_t vf, uint32_t *msgbuf)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);
	struct txgbe_vf_info *vfinfo = *(TXGBE_DEV_VFDATA(eth_dev));
	unsigned char *vf_mac = vfinfo[vf].vf_mac_addresses;
	int rar_entry = hw->mac.num_rar_entries - (vf + 1);
	uint8_t *new_mac = (uint8_t *)(&msgbuf[1]);

	txgbe_vf_reset_msg(eth_dev, vf);

	hw->mac.set_rar(hw, rar_entry, vf_mac, vf, true);

	/* Disable multicast promiscuous at reset */
	txgbe_disable_vf_mc_promisc(eth_dev, vf);

	/* reply to reset with ack and vf mac address */
	msgbuf[0] = TXGBE_VF_RESET | TXGBE_VT_MSGTYPE_ACK;
	rte_memcpy(new_mac, vf_mac, RTE_ETHER_ADDR_LEN);
	/*
	 * Piggyback the multicast filter type so VF can compute the
	 * correct vectors
	 */
	msgbuf[3] = hw->mac.mc_filter_type;
	txgbe_write_mbx(hw, msgbuf, TXGBE_VF_PERMADDR_MSG_LEN, vf);

	return 0;
}

static int
txgbe_vf_set_mac_addr(struct rte_eth_dev *eth_dev,
		uint32_t vf, uint32_t *msgbuf)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);
	struct txgbe_vf_info *vfinfo = *(TXGBE_DEV_VFDATA(eth_dev));
	int rar_entry = hw->mac.num_rar_entries - (vf + 1);
	uint8_t *new_mac = (uint8_t *)(&msgbuf[1]);
	struct rte_ether_addr *ea = (struct rte_ether_addr *)new_mac;

	if (rte_is_valid_assigned_ether_addr(ea)) {
		rte_memcpy(vfinfo[vf].vf_mac_addresses, new_mac, 6);
		return hw->mac.set_rar(hw, rar_entry, new_mac, vf, true);
	}
	return -1;
}

static int
txgbe_vf_set_multicast(struct rte_eth_dev *eth_dev,
		uint32_t vf, uint32_t *msgbuf)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);
	struct txgbe_vf_info *vfinfo = *(TXGBE_DEV_VFDATA(eth_dev));
	int nb_entries = (msgbuf[0] & TXGBE_VT_MSGINFO_MASK) >>
		TXGBE_VT_MSGINFO_SHIFT;
	uint16_t *hash_list = (uint16_t *)&msgbuf[1];
	uint32_t mta_idx;
	uint32_t mta_shift;
	const uint32_t TXGBE_MTA_INDEX_MASK = 0x7F;
	const uint32_t TXGBE_MTA_BIT_SHIFT = 5;
	const uint32_t TXGBE_MTA_BIT_MASK = (0x1 << TXGBE_MTA_BIT_SHIFT) - 1;
	uint32_t reg_val;
	int i;
	u32 vmolr = rd32(hw, TXGBE_POOLETHCTL(vf));

	/* Disable multicast promiscuous first */
	txgbe_disable_vf_mc_promisc(eth_dev, vf);

	/* only so many hash values supported */
	nb_entries = RTE_MIN(nb_entries, TXGBE_MAX_VF_MC_ENTRIES);

	/* store the mc entries  */
	vfinfo->num_vf_mc_hashes = (uint16_t)nb_entries;
	for (i = 0; i < nb_entries; i++)
		vfinfo->vf_mc_hashes[i] = hash_list[i];

	if (nb_entries == 0) {
		vmolr &= ~TXGBE_POOLETHCTL_MCHA;
		wr32(hw, TXGBE_POOLETHCTL(vf), vmolr);
		return 0;
	}

	for (i = 0; i < vfinfo->num_vf_mc_hashes; i++) {
		mta_idx = (vfinfo->vf_mc_hashes[i] >> TXGBE_MTA_BIT_SHIFT)
				& TXGBE_MTA_INDEX_MASK;
		mta_shift = vfinfo->vf_mc_hashes[i] & TXGBE_MTA_BIT_MASK;
		reg_val = rd32(hw, TXGBE_MCADDRTBL(mta_idx));
		reg_val |= (1 << mta_shift);
		wr32(hw, TXGBE_MCADDRTBL(mta_idx), reg_val);
	}

	vmolr |= TXGBE_POOLETHCTL_MCHA;
	wr32(hw, TXGBE_POOLETHCTL(vf), vmolr);

	return 0;
}

static int
txgbe_vf_set_vlan(struct rte_eth_dev *eth_dev, uint32_t vf, uint32_t *msgbuf)
{
	int add, vid;
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);
	struct txgbe_vf_info *vfinfo = *(TXGBE_DEV_VFDATA(eth_dev));

	add = (msgbuf[0] & TXGBE_VT_MSGINFO_MASK)
		>> TXGBE_VT_MSGINFO_SHIFT;
	vid = TXGBE_PSRVLAN_VID(msgbuf[1]);

	if (add)
		vfinfo[vf].vlan_count++;
	else if (vfinfo[vf].vlan_count)
		vfinfo[vf].vlan_count--;
	return hw->mac.set_vfta(hw, vid, vf, (bool)add, false);
}

static int
txgbe_set_vf_lpe(struct rte_eth_dev *eth_dev,
		__rte_unused uint32_t vf, uint32_t *msgbuf)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);
	uint32_t max_frame = msgbuf[1];
	uint32_t max_frs;

	if (max_frame < RTE_ETHER_MIN_LEN ||
			max_frame > RTE_ETHER_MAX_JUMBO_FRAME_LEN)
		return -1;

	max_frs = rd32m(hw, TXGBE_FRMSZ, TXGBE_FRMSZ_MAX_MASK);
	if (max_frs < max_frame) {
		wr32m(hw, TXGBE_FRMSZ, TXGBE_FRMSZ_MAX_MASK,
			TXGBE_FRMSZ_MAX(max_frame));
	}

	return 0;
}

static int
txgbe_negotiate_vf_api(struct rte_eth_dev *eth_dev,
		uint32_t vf, uint32_t *msgbuf)
{
	uint32_t api_version = msgbuf[1];
	struct txgbe_vf_info *vfinfo = *TXGBE_DEV_VFDATA(eth_dev);

	switch (api_version) {
	case txgbe_mbox_api_10:
	case txgbe_mbox_api_11:
	case txgbe_mbox_api_12:
	case txgbe_mbox_api_13:
		vfinfo[vf].api_version = (uint8_t)api_version;
		return 0;
	default:
		break;
	}

	PMD_DRV_LOG(ERR, "Negotiate invalid api version %u from VF %d\n",
		api_version, vf);

	return -1;
}

static int
txgbe_get_vf_queues(struct rte_eth_dev *eth_dev, uint32_t vf, uint32_t *msgbuf)
{
	struct txgbe_vf_info *vfinfo = *TXGBE_DEV_VFDATA(eth_dev);
	uint32_t default_q = vf * RTE_ETH_DEV_SRIOV(eth_dev).nb_q_per_pool;
	struct rte_eth_conf *eth_conf;
	struct rte_eth_vmdq_dcb_tx_conf *vmdq_dcb_tx_conf;
	u8 num_tcs;
	struct txgbe_hw *hw;
	u32 vmvir;
	u32 vlana;
	u32 vid;
	u32 user_priority;

	/* Verify if the PF supports the mbox APIs version or not */
	switch (vfinfo[vf].api_version) {
	case txgbe_mbox_api_20:
	case txgbe_mbox_api_11:
	case txgbe_mbox_api_12:
	case txgbe_mbox_api_13:
		break;
	default:
		return -1;
	}

	/* Notify VF of Rx and Tx queue number */
	msgbuf[TXGBE_VF_RX_QUEUES] = RTE_ETH_DEV_SRIOV(eth_dev).nb_q_per_pool;
	msgbuf[TXGBE_VF_TX_QUEUES] = RTE_ETH_DEV_SRIOV(eth_dev).nb_q_per_pool;

	/* Notify VF of default queue */
	msgbuf[TXGBE_VF_DEF_QUEUE] = default_q;

	/* Notify VF of number of DCB traffic classes */
	eth_conf = &eth_dev->data->dev_conf;
	switch (eth_conf->txmode.mq_mode) {
	case RTE_ETH_MQ_TX_NONE:
	case RTE_ETH_MQ_TX_DCB:
		PMD_DRV_LOG(ERR, "PF must work with virtualization for VF %u"
			", but its tx mode = %d\n", vf,
			eth_conf->txmode.mq_mode);
		return -1;

	case RTE_ETH_MQ_TX_VMDQ_DCB:
		vmdq_dcb_tx_conf = &eth_conf->tx_adv_conf.vmdq_dcb_tx_conf;
		switch (vmdq_dcb_tx_conf->nb_queue_pools) {
		case RTE_ETH_16_POOLS:
			num_tcs = RTE_ETH_8_TCS;
			break;
		case RTE_ETH_32_POOLS:
			num_tcs = RTE_ETH_4_TCS;
			break;
		default:
			return -1;
		}
		break;

	/* RTE_ETH_MQ_TX_VMDQ_ONLY,  DCB not enabled */
	case RTE_ETH_MQ_TX_VMDQ_ONLY:
		hw = TXGBE_DEV_HW(eth_dev);
		vmvir = rd32(hw, TXGBE_POOLTAG(vf));
		vlana = vmvir & TXGBE_POOLTAG_ACT_MASK;
		vid = vmvir & TXGBE_POOLTAG_VTAG_MASK;
		user_priority =
			TXGBD_POOLTAG_VTAG_UP(vmvir);
		if (vlana == TXGBE_POOLTAG_ACT_ALWAYS &&
			(vid !=  0 || user_priority != 0))
			num_tcs = 1;
		else
			num_tcs = 0;
		break;

	default:
		PMD_DRV_LOG(ERR, "PF work with invalid mode = %d\n",
			eth_conf->txmode.mq_mode);
		return -1;
	}
	msgbuf[TXGBE_VF_TRANS_VLAN] = num_tcs;

	return 0;
}

static int
txgbe_set_vf_mc_promisc(struct rte_eth_dev *eth_dev,
		uint32_t vf, uint32_t *msgbuf)
{
	struct txgbe_vf_info *vfinfo = *(TXGBE_DEV_VFDATA(eth_dev));
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);
	int xcast_mode = msgbuf[1];	/* msgbuf contains the flag to enable */
	u32 vmolr, fctrl, disable, enable;

	switch (vfinfo[vf].api_version) {
	case txgbe_mbox_api_12:
		/* promisc introduced in 1.3 version */
		if (xcast_mode == TXGBEVF_XCAST_MODE_PROMISC)
			return -EOPNOTSUPP;
		break;
		/* Fall threw */
	case txgbe_mbox_api_13:
		break;
	default:
		return -1;
	}

	if (vfinfo[vf].xcast_mode == xcast_mode)
		goto out;

	switch (xcast_mode) {
	case TXGBEVF_XCAST_MODE_NONE:
		disable = TXGBE_POOLETHCTL_BCA | TXGBE_POOLETHCTL_MCHA |
			  TXGBE_POOLETHCTL_MCP | TXGBE_POOLETHCTL_UCP |
			  TXGBE_POOLETHCTL_VLP;
		enable = 0;
		break;
	case TXGBEVF_XCAST_MODE_MULTI:
		disable = TXGBE_POOLETHCTL_MCP | TXGBE_POOLETHCTL_UCP |
			  TXGBE_POOLETHCTL_VLP;
		enable = TXGBE_POOLETHCTL_BCA | TXGBE_POOLETHCTL_MCHA;
		break;
	case TXGBEVF_XCAST_MODE_ALLMULTI:
		disable = TXGBE_POOLETHCTL_UCP | TXGBE_POOLETHCTL_VLP;
		enable = TXGBE_POOLETHCTL_BCA | TXGBE_POOLETHCTL_MCHA |
			 TXGBE_POOLETHCTL_MCP;
		break;
	case TXGBEVF_XCAST_MODE_PROMISC:
		fctrl = rd32(hw, TXGBE_PSRCTL);
		if (!(fctrl & TXGBE_PSRCTL_UCP)) {
			/* VF promisc requires PF in promisc */
			PMD_DRV_LOG(ERR,
			       "Enabling VF promisc requires PF in promisc\n");
			return -1;
		}

		disable = 0;
		enable = TXGBE_POOLETHCTL_BCA | TXGBE_POOLETHCTL_MCHA |
			 TXGBE_POOLETHCTL_MCP | TXGBE_POOLETHCTL_UCP |
			 TXGBE_POOLETHCTL_VLP;
		break;
	default:
		return -1;
	}

	vmolr = rd32(hw, TXGBE_POOLETHCTL(vf));
	vmolr &= ~disable;
	vmolr |= enable;
	wr32(hw, TXGBE_POOLETHCTL(vf), vmolr);
	vfinfo[vf].xcast_mode = xcast_mode;

out:
	msgbuf[1] = xcast_mode;

	return 0;
}

static int
txgbe_set_vf_macvlan_msg(struct rte_eth_dev *dev, uint32_t vf, uint32_t *msgbuf)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_vf_info *vf_info = *(TXGBE_DEV_VFDATA(dev));
	uint8_t *new_mac = (uint8_t *)(&msgbuf[1]);
	struct rte_ether_addr *ea = (struct rte_ether_addr *)new_mac;
	int index = (msgbuf[0] & TXGBE_VT_MSGINFO_MASK) >>
		    TXGBE_VT_MSGINFO_SHIFT;

	if (index) {
		if (!rte_is_valid_assigned_ether_addr(ea)) {
			PMD_DRV_LOG(ERR, "set invalid mac vf:%d\n", vf);
			return -1;
		}

		vf_info[vf].mac_count++;

		hw->mac.set_rar(hw, vf_info[vf].mac_count,
				new_mac, vf, true);
	} else {
		if (vf_info[vf].mac_count) {
			hw->mac.clear_rar(hw, vf_info[vf].mac_count);
			vf_info[vf].mac_count = 0;
		}
	}
	return 0;
}

static int
txgbe_rcv_msg_from_vf(struct rte_eth_dev *eth_dev, uint16_t vf)
{
	uint16_t mbx_size = TXGBE_P2VMBX_SIZE;
	uint16_t msg_size = TXGBE_VF_MSG_SIZE_DEFAULT;
	uint32_t msgbuf[TXGBE_P2VMBX_SIZE];
	int32_t retval;
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);
	struct txgbe_vf_info *vfinfo = *TXGBE_DEV_VFDATA(eth_dev);
	struct rte_pmd_txgbe_mb_event_param ret_param;

	retval = txgbe_read_mbx(hw, msgbuf, mbx_size, vf);
	if (retval) {
		PMD_DRV_LOG(ERR, "Error mbx recv msg from VF %d", vf);
		return retval;
	}

	/* do nothing with the message already been processed */
	if (msgbuf[0] & (TXGBE_VT_MSGTYPE_ACK | TXGBE_VT_MSGTYPE_NACK))
		return retval;

	/* flush the ack before we write any messages back */
	txgbe_flush(hw);

	/**
	 * initialise structure to send to user application
	 * will return response from user in retval field
	 */
	ret_param.retval = RTE_PMD_TXGBE_MB_EVENT_PROCEED;
	ret_param.vfid = vf;
	ret_param.msg_type = msgbuf[0] & 0xFFFF;
	ret_param.msg = (void *)msgbuf;

	/* perform VF reset */
	if (msgbuf[0] == TXGBE_VF_RESET) {
		int ret = txgbe_vf_reset(eth_dev, vf, msgbuf);

		vfinfo[vf].clear_to_send = true;

		/* notify application about VF reset */
		rte_eth_dev_callback_process(eth_dev, RTE_ETH_EVENT_VF_MBOX,
					      &ret_param);
		return ret;
	}

	/**
	 * ask user application if we allowed to perform those functions
	 * if we get ret_param.retval == RTE_PMD_TXGBE_MB_EVENT_PROCEED
	 * then business as usual,
	 * if 0, do nothing and send ACK to VF
	 * if ret_param.retval > 1, do nothing and send NAK to VF
	 */
	rte_eth_dev_callback_process(eth_dev, RTE_ETH_EVENT_VF_MBOX,
				      &ret_param);

	retval = ret_param.retval;

	/* check & process VF to PF mailbox message */
	switch ((msgbuf[0] & 0xFFFF)) {
	case TXGBE_VF_SET_MAC_ADDR:
		if (retval == RTE_PMD_TXGBE_MB_EVENT_PROCEED)
			retval = txgbe_vf_set_mac_addr(eth_dev, vf, msgbuf);
		break;
	case TXGBE_VF_SET_MULTICAST:
		if (retval == RTE_PMD_TXGBE_MB_EVENT_PROCEED)
			retval = txgbe_vf_set_multicast(eth_dev, vf, msgbuf);
		break;
	case TXGBE_VF_SET_LPE:
		if (retval == RTE_PMD_TXGBE_MB_EVENT_PROCEED)
			retval = txgbe_set_vf_lpe(eth_dev, vf, msgbuf);
		break;
	case TXGBE_VF_SET_VLAN:
		if (retval == RTE_PMD_TXGBE_MB_EVENT_PROCEED)
			retval = txgbe_vf_set_vlan(eth_dev, vf, msgbuf);
		break;
	case TXGBE_VF_API_NEGOTIATE:
		retval = txgbe_negotiate_vf_api(eth_dev, vf, msgbuf);
		break;
	case TXGBE_VF_GET_QUEUES:
		retval = txgbe_get_vf_queues(eth_dev, vf, msgbuf);
		msg_size = TXGBE_VF_GET_QUEUE_MSG_SIZE;
		break;
	case TXGBE_VF_UPDATE_XCAST_MODE:
		if (retval == RTE_PMD_TXGBE_MB_EVENT_PROCEED)
			retval = txgbe_set_vf_mc_promisc(eth_dev, vf, msgbuf);
		break;
	case TXGBE_VF_SET_MACVLAN:
		if (retval == RTE_PMD_TXGBE_MB_EVENT_PROCEED)
			retval = txgbe_set_vf_macvlan_msg(eth_dev, vf, msgbuf);
		break;
	default:
		PMD_DRV_LOG(DEBUG, "Unhandled Msg %8.8x", (uint32_t)msgbuf[0]);
		retval = TXGBE_ERR_MBX;
		break;
	}

	/* response the VF according to the message process result */
	if (retval)
		msgbuf[0] |= TXGBE_VT_MSGTYPE_NACK;
	else
		msgbuf[0] |= TXGBE_VT_MSGTYPE_ACK;

	msgbuf[0] |= TXGBE_VT_MSGTYPE_CTS;

	txgbe_write_mbx(hw, msgbuf, msg_size, vf);

	return retval;
}

static inline void
txgbe_rcv_ack_from_vf(struct rte_eth_dev *eth_dev, uint16_t vf)
{
	uint32_t msg = TXGBE_VT_MSGTYPE_NACK;
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);
	struct txgbe_vf_info *vfinfo = *TXGBE_DEV_VFDATA(eth_dev);

	if (!vfinfo[vf].clear_to_send)
		txgbe_write_mbx(hw, &msg, 1, vf);
}

void txgbe_pf_mbx_process(struct rte_eth_dev *eth_dev)
{
	uint16_t vf;
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);

	for (vf = 0; vf < dev_num_vf(eth_dev); vf++) {
		/* check & process vf function level reset */
		if (!txgbe_check_for_rst(hw, vf))
			txgbe_vf_reset_event(eth_dev, vf);

		/* check & process vf mailbox messages */
		if (!txgbe_check_for_msg(hw, vf))
			txgbe_rcv_msg_from_vf(eth_dev, vf);

		/* check & process acks from vf */
		if (!txgbe_check_for_ack(hw, vf))
			txgbe_rcv_ack_from_vf(eth_dev, vf);
	}
}
