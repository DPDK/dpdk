/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
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
#include <rte_ethdev.h>
#include <rte_memcpy.h>
#include <rte_malloc.h>
#include <rte_random.h>

#include "base/ixgbe_common.h"
#include "ixgbe_ethdev.h"
#include "rte_pmd_ixgbe.h"

#define IXGBE_MAX_VFTA     (128)
#define IXGBE_VF_MSG_SIZE_DEFAULT 1
#define IXGBE_VF_GET_QUEUE_MSG_SIZE 5
#define IXGBE_ETHERTYPE_FLOW_CTRL 0x8808

static inline uint16_t
dev_num_vf(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	return pci_dev->max_vfs;
}

static inline
int ixgbe_vf_perm_addr_gen(struct rte_eth_dev *dev, uint16_t vf_num)
{
	unsigned char vf_mac_addr[ETHER_ADDR_LEN];
	struct ixgbe_vf_info *vfinfo =
		*IXGBE_DEV_PRIVATE_TO_P_VFDATA(dev->data->dev_private);
	uint16_t vfn;

	for (vfn = 0; vfn < vf_num; vfn++) {
		eth_random_addr(vf_mac_addr);
		/* keep the random address as default */
		memcpy(vfinfo[vfn].vf_mac_addresses, vf_mac_addr,
			   ETHER_ADDR_LEN);
	}

	return 0;
}

static inline int
ixgbe_mb_intr_setup(struct rte_eth_dev *dev)
{
	struct ixgbe_interrupt *intr =
		IXGBE_DEV_PRIVATE_TO_INTR(dev->data->dev_private);

	intr->mask |= IXGBE_EICR_MAILBOX;

	return 0;
}

void ixgbe_pf_host_init(struct rte_eth_dev *eth_dev)
{
	struct ixgbe_vf_info **vfinfo =
		IXGBE_DEV_PRIVATE_TO_P_VFDATA(eth_dev->data->dev_private);
	struct ixgbe_mirror_info *mirror_info =
	IXGBE_DEV_PRIVATE_TO_PFDATA(eth_dev->data->dev_private);
	struct ixgbe_uta_info *uta_info =
	IXGBE_DEV_PRIVATE_TO_UTA(eth_dev->data->dev_private);
	struct ixgbe_hw *hw =
		IXGBE_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	uint16_t vf_num;
	uint8_t nb_queue;

	PMD_INIT_FUNC_TRACE();

	RTE_ETH_DEV_SRIOV(eth_dev).active = 0;
	vf_num = dev_num_vf(eth_dev);
	if (vf_num == 0)
		return;

	*vfinfo = rte_zmalloc("vf_info", sizeof(struct ixgbe_vf_info) * vf_num, 0);
	if (*vfinfo == NULL)
		rte_panic("Cannot allocate memory for private VF data\n");

	memset(mirror_info, 0, sizeof(struct ixgbe_mirror_info));
	memset(uta_info, 0, sizeof(struct ixgbe_uta_info));
	hw->mac.mc_filter_type = 0;

	if (vf_num >= ETH_32_POOLS) {
		nb_queue = 2;
		RTE_ETH_DEV_SRIOV(eth_dev).active = ETH_64_POOLS;
	} else if (vf_num >= ETH_16_POOLS) {
		nb_queue = 4;
		RTE_ETH_DEV_SRIOV(eth_dev).active = ETH_32_POOLS;
	} else {
		nb_queue = 8;
		RTE_ETH_DEV_SRIOV(eth_dev).active = ETH_16_POOLS;
	}

	RTE_ETH_DEV_SRIOV(eth_dev).nb_q_per_pool = nb_queue;
	RTE_ETH_DEV_SRIOV(eth_dev).def_vmdq_idx = vf_num;
	RTE_ETH_DEV_SRIOV(eth_dev).def_pool_q_idx = (uint16_t)(vf_num * nb_queue);

	ixgbe_vf_perm_addr_gen(eth_dev, vf_num);

	/* init_mailbox_params */
	hw->mbx.ops.init_params(hw);

	/* set mb interrupt mask */
	ixgbe_mb_intr_setup(eth_dev);
}

void ixgbe_pf_host_uninit(struct rte_eth_dev *eth_dev)
{
	struct ixgbe_vf_info **vfinfo;
	uint16_t vf_num;

	PMD_INIT_FUNC_TRACE();

	vfinfo = IXGBE_DEV_PRIVATE_TO_P_VFDATA(eth_dev->data->dev_private);

	RTE_ETH_DEV_SRIOV(eth_dev).active = 0;
	RTE_ETH_DEV_SRIOV(eth_dev).nb_q_per_pool = 0;
	RTE_ETH_DEV_SRIOV(eth_dev).def_vmdq_idx = 0;
	RTE_ETH_DEV_SRIOV(eth_dev).def_pool_q_idx = 0;

	vf_num = dev_num_vf(eth_dev);
	if (vf_num == 0)
		return;

	rte_free(*vfinfo);
	*vfinfo = NULL;
}

static void
ixgbe_add_tx_flow_control_drop_filter(struct rte_eth_dev *eth_dev)
{
	struct ixgbe_hw *hw =
		IXGBE_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct ixgbe_filter_info *filter_info =
		IXGBE_DEV_PRIVATE_TO_FILTER_INFO(eth_dev->data->dev_private);
	uint16_t vf_num;
	int i;
	struct ixgbe_ethertype_filter ethertype_filter;

	if (!hw->mac.ops.set_ethertype_anti_spoofing) {
		RTE_LOG(INFO, PMD, "ether type anti-spoofing is not"
			" supported.\n");
		return;
	}

	i = ixgbe_ethertype_filter_lookup(filter_info,
					  IXGBE_ETHERTYPE_FLOW_CTRL);
	if (i >= 0) {
		RTE_LOG(ERR, PMD, "A ether type filter"
			" entity for flow control already exists!\n");
		return;
	}

	ethertype_filter.ethertype = IXGBE_ETHERTYPE_FLOW_CTRL;
	ethertype_filter.etqf = IXGBE_ETQF_FILTER_EN |
				IXGBE_ETQF_TX_ANTISPOOF |
				IXGBE_ETHERTYPE_FLOW_CTRL;
	ethertype_filter.etqs = 0;
	ethertype_filter.conf = TRUE;
	i = ixgbe_ethertype_filter_insert(filter_info,
					  &ethertype_filter);
	if (i < 0) {
		RTE_LOG(ERR, PMD, "Cannot find an unused ether type filter"
			" entity for flow control.\n");
		return;
	}

	IXGBE_WRITE_REG(hw, IXGBE_ETQF(i),
			(IXGBE_ETQF_FILTER_EN |
			IXGBE_ETQF_TX_ANTISPOOF |
			IXGBE_ETHERTYPE_FLOW_CTRL));

	vf_num = dev_num_vf(eth_dev);
	for (i = 0; i < vf_num; i++)
		hw->mac.ops.set_ethertype_anti_spoofing(hw, true, i);
}

int ixgbe_pf_host_configure(struct rte_eth_dev *eth_dev)
{
	uint32_t vtctl, fcrth;
	uint32_t vfre_slot, vfre_offset;
	uint16_t vf_num;
	const uint8_t VFRE_SHIFT = 5;  /* VFRE 32 bits per slot */
	const uint8_t VFRE_MASK = (uint8_t)((1U << VFRE_SHIFT) - 1);
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	uint32_t gpie, gcr_ext;
	uint32_t vlanctrl;
	int i;

	vf_num = dev_num_vf(eth_dev);
	if (vf_num == 0)
		return -1;

	/* enable VMDq and set the default pool for PF */
	vtctl = IXGBE_READ_REG(hw, IXGBE_VT_CTL);
	vtctl |= IXGBE_VMD_CTL_VMDQ_EN;
	vtctl &= ~IXGBE_VT_CTL_POOL_MASK;
	vtctl |= RTE_ETH_DEV_SRIOV(eth_dev).def_vmdq_idx
		<< IXGBE_VT_CTL_POOL_SHIFT;
	vtctl |= IXGBE_VT_CTL_REPLEN;
	IXGBE_WRITE_REG(hw, IXGBE_VT_CTL, vtctl);

	vfre_offset = vf_num & VFRE_MASK;
	vfre_slot = (vf_num >> VFRE_SHIFT) > 0 ? 1 : 0;

	/* Enable pools reserved to PF only */
	IXGBE_WRITE_REG(hw, IXGBE_VFRE(vfre_slot), (~0U) << vfre_offset);
	IXGBE_WRITE_REG(hw, IXGBE_VFRE(vfre_slot ^ 1), vfre_slot - 1);
	IXGBE_WRITE_REG(hw, IXGBE_VFTE(vfre_slot), (~0U) << vfre_offset);
	IXGBE_WRITE_REG(hw, IXGBE_VFTE(vfre_slot ^ 1), vfre_slot - 1);

	/* PFDMA Tx General Switch Control Enables VMDQ loopback */
	IXGBE_WRITE_REG(hw, IXGBE_PFDTXGSWC, IXGBE_PFDTXGSWC_VT_LBEN);

	/* clear VMDq map to perment rar 0 */
	hw->mac.ops.clear_vmdq(hw, 0, IXGBE_CLEAR_VMDQ_ALL);

	/* clear VMDq map to scan rar 127 */
	IXGBE_WRITE_REG(hw, IXGBE_MPSAR_LO(hw->mac.num_rar_entries), 0);
	IXGBE_WRITE_REG(hw, IXGBE_MPSAR_HI(hw->mac.num_rar_entries), 0);

	/* set VMDq map to default PF pool */
	hw->mac.ops.set_vmdq(hw, 0, RTE_ETH_DEV_SRIOV(eth_dev).def_vmdq_idx);

	/*
	 * SW msut set GCR_EXT.VT_Mode the same as GPIE.VT_Mode
	 */
	gcr_ext = IXGBE_READ_REG(hw, IXGBE_GCR_EXT);
	gcr_ext &= ~IXGBE_GCR_EXT_VT_MODE_MASK;

	gpie = IXGBE_READ_REG(hw, IXGBE_GPIE);
	gpie &= ~IXGBE_GPIE_VTMODE_MASK;
	gpie |= IXGBE_GPIE_MSIX_MODE;

	switch (RTE_ETH_DEV_SRIOV(eth_dev).active) {
	case ETH_64_POOLS:
		gcr_ext |= IXGBE_GCR_EXT_VT_MODE_64;
		gpie |= IXGBE_GPIE_VTMODE_64;
		break;
	case ETH_32_POOLS:
		gcr_ext |= IXGBE_GCR_EXT_VT_MODE_32;
		gpie |= IXGBE_GPIE_VTMODE_32;
		break;
	case ETH_16_POOLS:
		gcr_ext |= IXGBE_GCR_EXT_VT_MODE_16;
		gpie |= IXGBE_GPIE_VTMODE_16;
		break;
	}

	IXGBE_WRITE_REG(hw, IXGBE_GCR_EXT, gcr_ext);
	IXGBE_WRITE_REG(hw, IXGBE_GPIE, gpie);

	/*
	 * enable vlan filtering and allow all vlan tags through
	 */
	vlanctrl = IXGBE_READ_REG(hw, IXGBE_VLNCTRL);
	vlanctrl |= IXGBE_VLNCTRL_VFE; /* enable vlan filters */
	IXGBE_WRITE_REG(hw, IXGBE_VLNCTRL, vlanctrl);

	/* VFTA - enable all vlan filters */
	for (i = 0; i < IXGBE_MAX_VFTA; i++)
		IXGBE_WRITE_REG(hw, IXGBE_VFTA(i), 0xFFFFFFFF);

	/* Enable MAC Anti-Spoofing */
	hw->mac.ops.set_mac_anti_spoofing(hw, FALSE, vf_num);

	/* set flow control threshold to max to avoid tx switch hang */
	for (i = 0; i < IXGBE_DCB_MAX_TRAFFIC_CLASS; i++) {
		IXGBE_WRITE_REG(hw, IXGBE_FCRTL_82599(i), 0);
		fcrth = IXGBE_READ_REG(hw, IXGBE_RXPBSIZE(i)) - 32;
		IXGBE_WRITE_REG(hw, IXGBE_FCRTH_82599(i), fcrth);
	}

	ixgbe_add_tx_flow_control_drop_filter(eth_dev);

	return 0;
}

static void
set_rx_mode(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *dev_data = dev->data;
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	u32 fctrl, vmolr = IXGBE_VMOLR_BAM | IXGBE_VMOLR_AUPE;
	uint16_t vfn = dev_num_vf(dev);

	/* Check for Promiscuous and All Multicast modes */
	fctrl = IXGBE_READ_REG(hw, IXGBE_FCTRL);

	/* set all bits that we expect to always be set */
	fctrl &= ~IXGBE_FCTRL_SBP; /* disable store-bad-packets */
	fctrl |= IXGBE_FCTRL_BAM;

	/* clear the bits we are changing the status of */
	fctrl &= ~(IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);

	if (dev_data->promiscuous) {
		fctrl |= (IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
		vmolr |= (IXGBE_VMOLR_ROPE | IXGBE_VMOLR_MPE);
	} else {
		if (dev_data->all_multicast) {
			fctrl |= IXGBE_FCTRL_MPE;
			vmolr |= IXGBE_VMOLR_MPE;
		} else {
			vmolr |= IXGBE_VMOLR_ROMPE;
		}
	}

	if (hw->mac.type != ixgbe_mac_82598EB) {
		vmolr |= IXGBE_READ_REG(hw, IXGBE_VMOLR(vfn)) &
			 ~(IXGBE_VMOLR_MPE | IXGBE_VMOLR_ROMPE |
			   IXGBE_VMOLR_ROPE);
		IXGBE_WRITE_REG(hw, IXGBE_VMOLR(vfn), vmolr);
	}

	IXGBE_WRITE_REG(hw, IXGBE_FCTRL, fctrl);

	if (dev->data->dev_conf.rxmode.hw_vlan_strip)
		ixgbe_vlan_hw_strip_enable_all(dev);
	else
		ixgbe_vlan_hw_strip_disable_all(dev);
}

static inline void
ixgbe_vf_reset_event(struct rte_eth_dev *dev, uint16_t vf)
{
	struct ixgbe_hw *hw =
		IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbe_vf_info *vfinfo =
		*(IXGBE_DEV_PRIVATE_TO_P_VFDATA(dev->data->dev_private));
	int rar_entry = hw->mac.num_rar_entries - (vf + 1);
	uint32_t vmolr = IXGBE_READ_REG(hw, IXGBE_VMOLR(vf));

	vmolr |= (IXGBE_VMOLR_ROPE | IXGBE_VMOLR_ROMPE |
			IXGBE_VMOLR_BAM | IXGBE_VMOLR_AUPE);
	IXGBE_WRITE_REG(hw, IXGBE_VMOLR(vf), vmolr);

	IXGBE_WRITE_REG(hw, IXGBE_VMVIR(vf), 0);

	/* reset multicast table array for vf */
	vfinfo[vf].num_vf_mc_hashes = 0;

	/* reset rx mode */
	set_rx_mode(dev);

	hw->mac.ops.clear_rar(hw, rar_entry);
}

static inline void
ixgbe_vf_reset_msg(struct rte_eth_dev *dev, uint16_t vf)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t reg;
	uint32_t reg_offset, vf_shift;
	const uint8_t VFRE_SHIFT = 5;  /* VFRE 32 bits per slot */
	const uint8_t VFRE_MASK = (uint8_t)((1U << VFRE_SHIFT) - 1);
	uint8_t  nb_q_per_pool;
	int i;

	vf_shift = vf & VFRE_MASK;
	reg_offset = (vf >> VFRE_SHIFT) > 0 ? 1 : 0;

	/* enable transmit for vf */
	reg = IXGBE_READ_REG(hw, IXGBE_VFTE(reg_offset));
	reg |= (reg | (1 << vf_shift));
	IXGBE_WRITE_REG(hw, IXGBE_VFTE(reg_offset), reg);

	/* enable all queue drop for IOV */
	nb_q_per_pool = RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool;
	for (i = vf * nb_q_per_pool; i < (vf + 1) * nb_q_per_pool; i++) {
		IXGBE_WRITE_FLUSH(hw);
		reg = IXGBE_QDE_ENABLE | IXGBE_QDE_WRITE;
		reg |= i << IXGBE_QDE_IDX_SHIFT;
		IXGBE_WRITE_REG(hw, IXGBE_QDE, reg);
	}

	/* enable receive for vf */
	reg = IXGBE_READ_REG(hw, IXGBE_VFRE(reg_offset));
	reg |= (reg | (1 << vf_shift));
	IXGBE_WRITE_REG(hw, IXGBE_VFRE(reg_offset), reg);

	/* Enable counting of spoofed packets in the SSVPC register */
	reg = IXGBE_READ_REG(hw, IXGBE_VMECM(reg_offset));
	reg |= (1 << vf_shift);
	IXGBE_WRITE_REG(hw, IXGBE_VMECM(reg_offset), reg);

	ixgbe_vf_reset_event(dev, vf);
}

static int
ixgbe_enable_vf_mc_promisc(struct rte_eth_dev *dev, uint32_t vf)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t vmolr;

	vmolr = IXGBE_READ_REG(hw, IXGBE_VMOLR(vf));

	RTE_LOG(INFO, PMD, "VF %u: enabling multicast promiscuous\n", vf);

	vmolr |= IXGBE_VMOLR_MPE;

	IXGBE_WRITE_REG(hw, IXGBE_VMOLR(vf), vmolr);

	return 0;
}

static int
ixgbe_disable_vf_mc_promisc(struct rte_eth_dev *dev, uint32_t vf)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t vmolr;

	vmolr = IXGBE_READ_REG(hw, IXGBE_VMOLR(vf));

	RTE_LOG(INFO, PMD, "VF %u: disabling multicast promiscuous\n", vf);

	vmolr &= ~IXGBE_VMOLR_MPE;

	IXGBE_WRITE_REG(hw, IXGBE_VMOLR(vf), vmolr);

	return 0;
}

static int
ixgbe_vf_reset(struct rte_eth_dev *dev, uint16_t vf, uint32_t *msgbuf)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbe_vf_info *vfinfo =
		*(IXGBE_DEV_PRIVATE_TO_P_VFDATA(dev->data->dev_private));
	unsigned char *vf_mac = vfinfo[vf].vf_mac_addresses;
	int rar_entry = hw->mac.num_rar_entries - (vf + 1);
	uint8_t *new_mac = (uint8_t *)(&msgbuf[1]);

	ixgbe_vf_reset_msg(dev, vf);

	hw->mac.ops.set_rar(hw, rar_entry, vf_mac, vf, IXGBE_RAH_AV);

	/* Disable multicast promiscuous at reset */
	ixgbe_disable_vf_mc_promisc(dev, vf);

	/* reply to reset with ack and vf mac address */
	msgbuf[0] = IXGBE_VF_RESET | IXGBE_VT_MSGTYPE_ACK;
	rte_memcpy(new_mac, vf_mac, ETHER_ADDR_LEN);
	/*
	 * Piggyback the multicast filter type so VF can compute the
	 * correct vectors
	 */
	msgbuf[3] = hw->mac.mc_filter_type;
	ixgbe_write_mbx(hw, msgbuf, IXGBE_VF_PERMADDR_MSG_LEN, vf);

	return 0;
}

static int
ixgbe_vf_set_mac_addr(struct rte_eth_dev *dev, uint32_t vf, uint32_t *msgbuf)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbe_vf_info *vfinfo =
		*(IXGBE_DEV_PRIVATE_TO_P_VFDATA(dev->data->dev_private));
	int rar_entry = hw->mac.num_rar_entries - (vf + 1);
	uint8_t *new_mac = (uint8_t *)(&msgbuf[1]);

	if (is_valid_assigned_ether_addr((struct ether_addr *)new_mac)) {
		rte_memcpy(vfinfo[vf].vf_mac_addresses, new_mac, 6);
		return hw->mac.ops.set_rar(hw, rar_entry, new_mac, vf, IXGBE_RAH_AV);
	}
	return -1;
}

static int
ixgbe_vf_set_multicast(struct rte_eth_dev *dev, uint32_t vf, uint32_t *msgbuf)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbe_vf_info *vfinfo =
		*(IXGBE_DEV_PRIVATE_TO_P_VFDATA(dev->data->dev_private));
	int nb_entries = (msgbuf[0] & IXGBE_VT_MSGINFO_MASK) >>
		IXGBE_VT_MSGINFO_SHIFT;
	uint16_t *hash_list = (uint16_t *)&msgbuf[1];
	uint32_t mta_idx;
	uint32_t mta_shift;
	const uint32_t IXGBE_MTA_INDEX_MASK = 0x7F;
	const uint32_t IXGBE_MTA_BIT_SHIFT = 5;
	const uint32_t IXGBE_MTA_BIT_MASK = (0x1 << IXGBE_MTA_BIT_SHIFT) - 1;
	uint32_t reg_val;
	int i;

	/* Disable multicast promiscuous first */
	ixgbe_disable_vf_mc_promisc(dev, vf);

	/* only so many hash values supported */
	nb_entries = RTE_MIN(nb_entries, IXGBE_MAX_VF_MC_ENTRIES);

	/* store the mc entries  */
	vfinfo->num_vf_mc_hashes = (uint16_t)nb_entries;
	for (i = 0; i < nb_entries; i++) {
		vfinfo->vf_mc_hashes[i] = hash_list[i];
	}

	for (i = 0; i < vfinfo->num_vf_mc_hashes; i++) {
		mta_idx = (vfinfo->vf_mc_hashes[i] >> IXGBE_MTA_BIT_SHIFT)
				& IXGBE_MTA_INDEX_MASK;
		mta_shift = vfinfo->vf_mc_hashes[i] & IXGBE_MTA_BIT_MASK;
		reg_val = IXGBE_READ_REG(hw, IXGBE_MTA(mta_idx));
		reg_val |= (1 << mta_shift);
		IXGBE_WRITE_REG(hw, IXGBE_MTA(mta_idx), reg_val);
	}

	return 0;
}

static int
ixgbe_vf_set_vlan(struct rte_eth_dev *dev, uint32_t vf, uint32_t *msgbuf)
{
	int add, vid;
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbe_vf_info *vfinfo =
		*(IXGBE_DEV_PRIVATE_TO_P_VFDATA(dev->data->dev_private));

	add = (msgbuf[0] & IXGBE_VT_MSGINFO_MASK)
		>> IXGBE_VT_MSGINFO_SHIFT;
	vid = (msgbuf[1] & IXGBE_VLVF_VLANID_MASK);

	if (add)
		vfinfo[vf].vlan_count++;
	else if (vfinfo[vf].vlan_count)
		vfinfo[vf].vlan_count--;
	return hw->mac.ops.set_vfta(hw, vid, vf, (bool)add, false);
}

static int
ixgbe_set_vf_lpe(struct rte_eth_dev *dev, __rte_unused uint32_t vf, uint32_t *msgbuf)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t new_mtu = msgbuf[1];
	uint32_t max_frs;
	int max_frame = new_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;

	/* X540 and X550 support jumbo frames in IOV mode */
	if (hw->mac.type != ixgbe_mac_X540 &&
		hw->mac.type != ixgbe_mac_X550 &&
		hw->mac.type != ixgbe_mac_X550EM_x &&
		hw->mac.type != ixgbe_mac_X550EM_a)
		return -1;

	if ((max_frame < ETHER_MIN_LEN) || (max_frame > ETHER_MAX_JUMBO_FRAME_LEN))
		return -1;

	max_frs = (IXGBE_READ_REG(hw, IXGBE_MAXFRS) &
		   IXGBE_MHADD_MFS_MASK) >> IXGBE_MHADD_MFS_SHIFT;
	if (max_frs < new_mtu) {
		max_frs = new_mtu << IXGBE_MHADD_MFS_SHIFT;
		IXGBE_WRITE_REG(hw, IXGBE_MAXFRS, max_frs);
	}

	return 0;
}

static int
ixgbe_negotiate_vf_api(struct rte_eth_dev *dev, uint32_t vf, uint32_t *msgbuf)
{
	uint32_t api_version = msgbuf[1];
	struct ixgbe_vf_info *vfinfo =
		*IXGBE_DEV_PRIVATE_TO_P_VFDATA(dev->data->dev_private);

	switch (api_version) {
	case ixgbe_mbox_api_10:
	case ixgbe_mbox_api_11:
	case ixgbe_mbox_api_12:
		vfinfo[vf].api_version = (uint8_t)api_version;
		return 0;
	default:
		break;
	}

	RTE_LOG(ERR, PMD, "Negotiate invalid api version %u from VF %d\n",
		api_version, vf);

	return -1;
}

static int
ixgbe_get_vf_queues(struct rte_eth_dev *dev, uint32_t vf, uint32_t *msgbuf)
{
	struct ixgbe_vf_info *vfinfo =
		*IXGBE_DEV_PRIVATE_TO_P_VFDATA(dev->data->dev_private);
	uint32_t default_q = vf * RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool;

	/* Verify if the PF supports the mbox APIs version or not */
	switch (vfinfo[vf].api_version) {
	case ixgbe_mbox_api_20:
	case ixgbe_mbox_api_11:
	case ixgbe_mbox_api_12:
		break;
	default:
		return -1;
	}

	/* Notify VF of Rx and Tx queue number */
	msgbuf[IXGBE_VF_RX_QUEUES] = RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool;
	msgbuf[IXGBE_VF_TX_QUEUES] = RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool;

	/* Notify VF of default queue */
	msgbuf[IXGBE_VF_DEF_QUEUE] = default_q;

	/*
	 * FIX ME if it needs fill msgbuf[IXGBE_VF_TRANS_VLAN]
	 * for VLAN strip or VMDQ_DCB or VMDQ_DCB_RSS
	 */

	return 0;
}

static int
ixgbe_set_vf_mc_promisc(struct rte_eth_dev *dev, uint32_t vf, uint32_t *msgbuf)
{
	struct ixgbe_vf_info *vfinfo =
		*(IXGBE_DEV_PRIVATE_TO_P_VFDATA(dev->data->dev_private));
	bool enable = !!msgbuf[1];	/* msgbuf contains the flag to enable */

	switch (vfinfo[vf].api_version) {
	case ixgbe_mbox_api_12:
		break;
	default:
		return -1;
	}

	if (enable)
		return ixgbe_enable_vf_mc_promisc(dev, vf);
	else
		return ixgbe_disable_vf_mc_promisc(dev, vf);
}

static int
ixgbe_rcv_msg_from_vf(struct rte_eth_dev *dev, uint16_t vf)
{
	uint16_t mbx_size = IXGBE_VFMAILBOX_SIZE;
	uint16_t msg_size = IXGBE_VF_MSG_SIZE_DEFAULT;
	uint32_t msgbuf[IXGBE_VFMAILBOX_SIZE];
	int32_t retval;
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbe_vf_info *vfinfo =
		*IXGBE_DEV_PRIVATE_TO_P_VFDATA(dev->data->dev_private);
	struct rte_pmd_ixgbe_mb_event_param cb_param;

	retval = ixgbe_read_mbx(hw, msgbuf, mbx_size, vf);
	if (retval) {
		PMD_DRV_LOG(ERR, "Error mbx recv msg from VF %d", vf);
		return retval;
	}

	/* do nothing with the message already been processed */
	if (msgbuf[0] & (IXGBE_VT_MSGTYPE_ACK | IXGBE_VT_MSGTYPE_NACK))
		return retval;

	/* flush the ack before we write any messages back */
	IXGBE_WRITE_FLUSH(hw);

	/**
	 * initialise structure to send to user application
	 * will return response from user in retval field
	 */
	cb_param.retval = RTE_PMD_IXGBE_MB_EVENT_PROCEED;
	cb_param.vfid = vf;
	cb_param.msg_type = msgbuf[0] & 0xFFFF;
	cb_param.msg = (void *)msgbuf;

	/* perform VF reset */
	if (msgbuf[0] == IXGBE_VF_RESET) {
		int ret = ixgbe_vf_reset(dev, vf, msgbuf);

		vfinfo[vf].clear_to_send = true;

		/* notify application about VF reset */
		_rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_VF_MBOX, &cb_param);
		return ret;
	}

	/**
	 * ask user application if we allowed to perform those functions
	 * if we get cb_param.retval == RTE_PMD_IXGBE_MB_EVENT_PROCEED
	 * then business as usual,
	 * if 0, do nothing and send ACK to VF
	 * if cb_param.retval > 1, do nothing and send NAK to VF
	 */
	_rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_VF_MBOX, &cb_param);

	retval = cb_param.retval;

	/* check & process VF to PF mailbox message */
	switch ((msgbuf[0] & 0xFFFF)) {
	case IXGBE_VF_SET_MAC_ADDR:
		if (retval == RTE_PMD_IXGBE_MB_EVENT_PROCEED)
			retval = ixgbe_vf_set_mac_addr(dev, vf, msgbuf);
		break;
	case IXGBE_VF_SET_MULTICAST:
		if (retval == RTE_PMD_IXGBE_MB_EVENT_PROCEED)
			retval = ixgbe_vf_set_multicast(dev, vf, msgbuf);
		break;
	case IXGBE_VF_SET_LPE:
		if (retval == RTE_PMD_IXGBE_MB_EVENT_PROCEED)
			retval = ixgbe_set_vf_lpe(dev, vf, msgbuf);
		break;
	case IXGBE_VF_SET_VLAN:
		if (retval == RTE_PMD_IXGBE_MB_EVENT_PROCEED)
			retval = ixgbe_vf_set_vlan(dev, vf, msgbuf);
		break;
	case IXGBE_VF_API_NEGOTIATE:
		retval = ixgbe_negotiate_vf_api(dev, vf, msgbuf);
		break;
	case IXGBE_VF_GET_QUEUES:
		retval = ixgbe_get_vf_queues(dev, vf, msgbuf);
		msg_size = IXGBE_VF_GET_QUEUE_MSG_SIZE;
		break;
	case IXGBE_VF_UPDATE_XCAST_MODE:
		if (retval == RTE_PMD_IXGBE_MB_EVENT_PROCEED)
			retval = ixgbe_set_vf_mc_promisc(dev, vf, msgbuf);
		break;
	default:
		PMD_DRV_LOG(DEBUG, "Unhandled Msg %8.8x", (unsigned)msgbuf[0]);
		retval = IXGBE_ERR_MBX;
		break;
	}

	/* response the VF according to the message process result */
	if (retval)
		msgbuf[0] |= IXGBE_VT_MSGTYPE_NACK;
	else
		msgbuf[0] |= IXGBE_VT_MSGTYPE_ACK;

	msgbuf[0] |= IXGBE_VT_MSGTYPE_CTS;

	ixgbe_write_mbx(hw, msgbuf, msg_size, vf);

	return retval;
}

static inline void
ixgbe_rcv_ack_from_vf(struct rte_eth_dev *dev, uint16_t vf)
{
	uint32_t msg = IXGBE_VT_MSGTYPE_NACK;
	struct ixgbe_hw *hw =
		IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbe_vf_info *vfinfo =
		*IXGBE_DEV_PRIVATE_TO_P_VFDATA(dev->data->dev_private);

	if (!vfinfo[vf].clear_to_send)
		ixgbe_write_mbx(hw, &msg, 1, vf);
}

void ixgbe_pf_mbx_process(struct rte_eth_dev *eth_dev)
{
	uint16_t vf;
	struct ixgbe_hw *hw =
		IXGBE_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);

	for (vf = 0; vf < dev_num_vf(eth_dev); vf++) {
		/* check & process vf function level reset */
		if (!ixgbe_check_for_rst(hw, vf))
			ixgbe_vf_reset_event(eth_dev, vf);

		/* check & process vf mailbox messages */
		if (!ixgbe_check_for_msg(hw, vf))
			ixgbe_rcv_msg_from_vf(eth_dev, vf);

		/* check & process acks from vf */
		if (!ixgbe_check_for_ack(hw, vf))
			ixgbe_rcv_ack_from_vf(eth_dev, vf);
	}
}
