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

#include "base/e1000_defines.h"
#include "base/e1000_regs.h"
#include "base/e1000_hw.h"
#include "e1000_ethdev.h"

static inline uint16_t
dev_num_vf(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	return pci_dev->max_vfs;
}

static inline
int igb_vf_perm_addr_gen(struct rte_eth_dev *dev, uint16_t vf_num)
{
	unsigned char vf_mac_addr[ETHER_ADDR_LEN];
	struct e1000_vf_info *vfinfo =
		*E1000_DEV_PRIVATE_TO_P_VFDATA(dev->data->dev_private);
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
igb_mb_intr_setup(struct rte_eth_dev *dev)
{
	struct e1000_interrupt *intr =
		E1000_DEV_PRIVATE_TO_INTR(dev->data->dev_private);

	intr->mask |= E1000_ICR_VMMB;

	return 0;
}

void igb_pf_host_init(struct rte_eth_dev *eth_dev)
{
	struct e1000_vf_info **vfinfo =
		E1000_DEV_PRIVATE_TO_P_VFDATA(eth_dev->data->dev_private);
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	uint16_t vf_num;
	uint8_t nb_queue;

	RTE_ETH_DEV_SRIOV(eth_dev).active = 0;
	if (0 == (vf_num = dev_num_vf(eth_dev)))
		return;

	if (hw->mac.type == e1000_i350)
		nb_queue = 1;
	else if(hw->mac.type == e1000_82576)
		/* per datasheet, it should be 2, but 1 seems correct */
		nb_queue = 1;
	else
		return;

	*vfinfo = rte_zmalloc("vf_info", sizeof(struct e1000_vf_info) * vf_num, 0);
	if (*vfinfo == NULL)
		rte_panic("Cannot allocate memory for private VF data\n");

	RTE_ETH_DEV_SRIOV(eth_dev).active = ETH_8_POOLS;
	RTE_ETH_DEV_SRIOV(eth_dev).nb_q_per_pool = nb_queue;
	RTE_ETH_DEV_SRIOV(eth_dev).def_vmdq_idx = vf_num;
	RTE_ETH_DEV_SRIOV(eth_dev).def_pool_q_idx = (uint16_t)(vf_num * nb_queue);

	igb_vf_perm_addr_gen(eth_dev, vf_num);

	/* set mb interrupt mask */
	igb_mb_intr_setup(eth_dev);

	return;
}

void igb_pf_host_uninit(struct rte_eth_dev *dev)
{
	struct e1000_vf_info **vfinfo;
	uint16_t vf_num;

	PMD_INIT_FUNC_TRACE();

	vfinfo = E1000_DEV_PRIVATE_TO_P_VFDATA(dev->data->dev_private);

	RTE_ETH_DEV_SRIOV(dev).active = 0;
	RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool = 0;
	RTE_ETH_DEV_SRIOV(dev).def_vmdq_idx = 0;
	RTE_ETH_DEV_SRIOV(dev).def_pool_q_idx = 0;

	vf_num = dev_num_vf(dev);
	if (vf_num == 0)
		return;

	rte_free(*vfinfo);
	*vfinfo = NULL;
}

#define E1000_RAH_POOLSEL_SHIFT    (18)
int igb_pf_host_configure(struct rte_eth_dev *eth_dev)
{
	uint32_t vtctl;
	uint16_t vf_num;
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	uint32_t vlanctrl;
	int i;
	uint32_t rah;

	if (0 == (vf_num = dev_num_vf(eth_dev)))
		return -1;

	/* enable VMDq and set the default pool for PF */
	vtctl = E1000_READ_REG(hw, E1000_VT_CTL);
	vtctl &= ~E1000_VT_CTL_DEFAULT_POOL_MASK;
	vtctl |= RTE_ETH_DEV_SRIOV(eth_dev).def_vmdq_idx
		<< E1000_VT_CTL_DEFAULT_POOL_SHIFT;
	vtctl |= E1000_VT_CTL_VM_REPL_EN;
	E1000_WRITE_REG(hw, E1000_VT_CTL, vtctl);

	/* Enable pools reserved to PF only */
	E1000_WRITE_REG(hw, E1000_VFRE, (~0U) << vf_num);
	E1000_WRITE_REG(hw, E1000_VFTE, (~0U) << vf_num);

	/* PFDMA Tx General Switch Control Enables VMDQ loopback */
	if (hw->mac.type == e1000_i350)
		E1000_WRITE_REG(hw, E1000_TXSWC, E1000_DTXSWC_VMDQ_LOOPBACK_EN);
	else
		E1000_WRITE_REG(hw, E1000_DTXSWC, E1000_DTXSWC_VMDQ_LOOPBACK_EN);

	/* clear VMDq map to perment rar 0 */
	rah = E1000_READ_REG(hw, E1000_RAH(0));
	rah &= ~ (0xFF << E1000_RAH_POOLSEL_SHIFT);
	E1000_WRITE_REG(hw, E1000_RAH(0), rah);

	/* clear VMDq map to scan rar 32 */
	rah = E1000_READ_REG(hw, E1000_RAH(hw->mac.rar_entry_count));
	rah &= ~ (0xFF << E1000_RAH_POOLSEL_SHIFT);
	E1000_WRITE_REG(hw, E1000_RAH(hw->mac.rar_entry_count), rah);

	/* set VMDq map to default PF pool */
	rah = E1000_READ_REG(hw, E1000_RAH(0));
	rah |= (0x1 << (RTE_ETH_DEV_SRIOV(eth_dev).def_vmdq_idx +
			E1000_RAH_POOLSEL_SHIFT));
	E1000_WRITE_REG(hw, E1000_RAH(0), rah);

	/*
	 * enable vlan filtering and allow all vlan tags through
	 */
	vlanctrl = E1000_READ_REG(hw, E1000_RCTL);
	vlanctrl |= E1000_RCTL_VFE ; /* enable vlan filters */
	E1000_WRITE_REG(hw, E1000_RCTL, vlanctrl);

	/* VFTA - enable all vlan filters */
	for (i = 0; i < IGB_VFTA_SIZE; i++) {
		E1000_WRITE_REG_ARRAY(hw, E1000_VFTA, i, 0xFFFFFFFF);
	}

	/* Enable/Disable MAC Anti-Spoofing */
	e1000_vmdq_set_anti_spoofing_pf(hw, FALSE, vf_num);

	return 0;
}

static void
set_rx_mode(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *dev_data = dev->data;
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t fctrl, vmolr = E1000_VMOLR_BAM | E1000_VMOLR_AUPE;
	uint16_t vfn = dev_num_vf(dev);

	/* Check for Promiscuous and All Multicast modes */
	fctrl = E1000_READ_REG(hw, E1000_RCTL);

	/* set all bits that we expect to always be set */
	fctrl &= ~E1000_RCTL_SBP; /* disable store-bad-packets */
	fctrl |= E1000_RCTL_BAM;

	/* clear the bits we are changing the status of */
	fctrl &= ~(E1000_RCTL_UPE | E1000_RCTL_MPE);

	if (dev_data->promiscuous) {
		fctrl |= (E1000_RCTL_UPE | E1000_RCTL_MPE);
		vmolr |= (E1000_VMOLR_ROPE | E1000_VMOLR_MPME);
	} else {
		if (dev_data->all_multicast) {
			fctrl |= E1000_RCTL_MPE;
			vmolr |= E1000_VMOLR_MPME;
		} else {
			vmolr |= E1000_VMOLR_ROMPE;
		}
	}

	if ((hw->mac.type == e1000_82576) ||
		(hw->mac.type == e1000_i350)) {
		vmolr |= E1000_READ_REG(hw, E1000_VMOLR(vfn)) &
			 ~(E1000_VMOLR_MPME | E1000_VMOLR_ROMPE |
			   E1000_VMOLR_ROPE);
		E1000_WRITE_REG(hw, E1000_VMOLR(vfn), vmolr);
	}

	E1000_WRITE_REG(hw, E1000_RCTL, fctrl);
}

static inline void
igb_vf_reset_event(struct rte_eth_dev *dev, uint16_t vf)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct e1000_vf_info *vfinfo =
		*(E1000_DEV_PRIVATE_TO_P_VFDATA(dev->data->dev_private));
	uint32_t vmolr = E1000_READ_REG(hw, E1000_VMOLR(vf));

	vmolr |= (E1000_VMOLR_ROPE | E1000_VMOLR_ROMPE |
			E1000_VMOLR_BAM | E1000_VMOLR_AUPE);
	E1000_WRITE_REG(hw, E1000_VMOLR(vf), vmolr);

	E1000_WRITE_REG(hw, E1000_VMVIR(vf), 0);

	/* reset multicast table array for vf */
	vfinfo[vf].num_vf_mc_hashes = 0;

	/* reset rx mode */
	set_rx_mode(dev);
}

static inline void
igb_vf_reset_msg(struct rte_eth_dev *dev, uint16_t vf)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t reg;

	/* enable transmit and receive for vf */
	reg = E1000_READ_REG(hw, E1000_VFTE);
	reg |= (reg | (1 << vf));
	E1000_WRITE_REG(hw, E1000_VFTE, reg);

	reg = E1000_READ_REG(hw, E1000_VFRE);
	reg |= (reg | (1 << vf));
	E1000_WRITE_REG(hw, E1000_VFRE, reg);

	igb_vf_reset_event(dev, vf);
}

static int
igb_vf_reset(struct rte_eth_dev *dev, uint16_t vf, uint32_t *msgbuf)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct e1000_vf_info *vfinfo =
		*(E1000_DEV_PRIVATE_TO_P_VFDATA(dev->data->dev_private));
	unsigned char *vf_mac = vfinfo[vf].vf_mac_addresses;
	int rar_entry = hw->mac.rar_entry_count - (vf + 1);
	uint8_t *new_mac = (uint8_t *)(&msgbuf[1]);
	uint32_t rah;

	igb_vf_reset_msg(dev, vf);

	hw->mac.ops.rar_set(hw, vf_mac, rar_entry);
	rah = E1000_READ_REG(hw, E1000_RAH(rar_entry));
	rah |= (0x1 << (vf + E1000_RAH_POOLSEL_SHIFT));
	E1000_WRITE_REG(hw, E1000_RAH(rar_entry), rah);

	/* reply to reset with ack and vf mac address */
	msgbuf[0] = E1000_VF_RESET | E1000_VT_MSGTYPE_ACK;
	rte_memcpy(new_mac, vf_mac, ETHER_ADDR_LEN);
	e1000_write_mbx(hw, msgbuf, 3, vf);

	return 0;
}

static int
igb_vf_set_mac_addr(struct rte_eth_dev *dev, uint32_t vf, uint32_t *msgbuf)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct e1000_vf_info *vfinfo =
		*(E1000_DEV_PRIVATE_TO_P_VFDATA(dev->data->dev_private));
	int rar_entry = hw->mac.rar_entry_count - (vf + 1);
	uint8_t *new_mac = (uint8_t *)(&msgbuf[1]);
	int rah;

	if (is_unicast_ether_addr((struct ether_addr *)new_mac)) {
		if (!is_zero_ether_addr((struct ether_addr *)new_mac))
			rte_memcpy(vfinfo[vf].vf_mac_addresses, new_mac,
				sizeof(vfinfo[vf].vf_mac_addresses));
		hw->mac.ops.rar_set(hw, new_mac, rar_entry);
		rah = E1000_READ_REG(hw, E1000_RAH(rar_entry));
		rah |= (0x1 << (E1000_RAH_POOLSEL_SHIFT + vf));
		E1000_WRITE_REG(hw, E1000_RAH(rar_entry), rah);
		return 0;
	}
	return -1;
}

static int
igb_vf_set_multicast(struct rte_eth_dev *dev, __rte_unused uint32_t vf, uint32_t *msgbuf)
{
	int i;
	uint32_t vector_bit;
	uint32_t vector_reg;
	uint32_t mta_reg;
	int entries = (msgbuf[0] & E1000_VT_MSGINFO_MASK) >>
		E1000_VT_MSGINFO_SHIFT;
	uint16_t *hash_list = (uint16_t *)&msgbuf[1];
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct e1000_vf_info *vfinfo =
		*(E1000_DEV_PRIVATE_TO_P_VFDATA(dev->data->dev_private));

	/* only so many hash values supported */
	entries = RTE_MIN(entries, E1000_MAX_VF_MC_ENTRIES);

	/*
	 * salt away the number of multi cast addresses assigned
	 * to this VF for later use to restore when the PF multi cast
	 * list changes
	 */
	vfinfo->num_vf_mc_hashes = (uint16_t)entries;

	/*
	 * VFs are limited to using the MTA hash table for their multicast
	 * addresses
	 */
	for (i = 0; i < entries; i++) {
		vfinfo->vf_mc_hashes[i] = hash_list[i];
	}

	for (i = 0; i < vfinfo->num_vf_mc_hashes; i++) {
		vector_reg = (vfinfo->vf_mc_hashes[i] >> 5) & 0x7F;
		vector_bit = vfinfo->vf_mc_hashes[i] & 0x1F;
		mta_reg = E1000_READ_REG_ARRAY(hw, E1000_MTA, vector_reg);
		mta_reg |= (1 << vector_bit);
		E1000_WRITE_REG_ARRAY(hw, E1000_MTA, vector_reg, mta_reg);
	}

	return 0;
}

static int
igb_vf_set_vlan(struct rte_eth_dev *dev, uint32_t vf, uint32_t *msgbuf)
{
	int add, vid;
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct e1000_vf_info *vfinfo =
		*(E1000_DEV_PRIVATE_TO_P_VFDATA(dev->data->dev_private));
	uint32_t vid_idx, vid_bit, vfta;

	add = (msgbuf[0] & E1000_VT_MSGINFO_MASK)
		>> E1000_VT_MSGINFO_SHIFT;
	vid = (msgbuf[1] & E1000_VLVF_VLANID_MASK);

	if (add)
		vfinfo[vf].vlan_count++;
	else if (vfinfo[vf].vlan_count)
		vfinfo[vf].vlan_count--;

	vid_idx = (uint32_t)((vid >> E1000_VFTA_ENTRY_SHIFT) &
			     E1000_VFTA_ENTRY_MASK);
	vid_bit = (uint32_t)(1 << (vid & E1000_VFTA_ENTRY_BIT_SHIFT_MASK));
	vfta = E1000_READ_REG_ARRAY(hw, E1000_VFTA, vid_idx);
	if (add)
		vfta |= vid_bit;
	else
		vfta &= ~vid_bit;

	E1000_WRITE_REG_ARRAY(hw, E1000_VFTA, vid_idx, vfta);
	E1000_WRITE_FLUSH(hw);

	return 0;
}

static int
igb_vf_set_rlpml(struct rte_eth_dev *dev, uint32_t vf, uint32_t *msgbuf)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint16_t rlpml = msgbuf[1] & E1000_VMOLR_RLPML_MASK;
	uint32_t max_frame = rlpml + ETHER_HDR_LEN + ETHER_CRC_LEN;
	uint32_t vmolr;

	if ((max_frame < ETHER_MIN_LEN) || (max_frame > ETHER_MAX_JUMBO_FRAME_LEN))
		return -1;

	vmolr = E1000_READ_REG(hw, E1000_VMOLR(vf));

	vmolr &= ~E1000_VMOLR_RLPML_MASK;
	vmolr |= rlpml;

	/* Enable Long Packet support */
	vmolr |= E1000_VMOLR_LPE;

	E1000_WRITE_REG(hw, E1000_VMOLR(vf), vmolr);
	E1000_WRITE_FLUSH(hw);

	return 0;
}

static int
igb_rcv_msg_from_vf(struct rte_eth_dev *dev, uint16_t vf)
{
	uint16_t mbx_size = E1000_VFMAILBOX_SIZE;
	uint32_t msgbuf[E1000_VFMAILBOX_SIZE];
	int32_t retval;
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	retval = e1000_read_mbx(hw, msgbuf, mbx_size, vf);
	if (retval) {
		PMD_INIT_LOG(ERR, "Error mbx recv msg from VF %d", vf);
		return retval;
	}

	/* do nothing with the message already processed */
	if (msgbuf[0] & (E1000_VT_MSGTYPE_ACK | E1000_VT_MSGTYPE_NACK))
		return retval;

	/* flush the ack before we write any messages back */
	E1000_WRITE_FLUSH(hw);

	/* perform VF reset */
	if (msgbuf[0] == E1000_VF_RESET) {
		return igb_vf_reset(dev, vf, msgbuf);
	}

	/* check & process VF to PF mailbox message */
	switch ((msgbuf[0] & 0xFFFF)) {
	case E1000_VF_SET_MAC_ADDR:
		retval = igb_vf_set_mac_addr(dev, vf, msgbuf);
		break;
	case E1000_VF_SET_MULTICAST:
		retval = igb_vf_set_multicast(dev, vf, msgbuf);
		break;
	case E1000_VF_SET_LPE:
		retval = igb_vf_set_rlpml(dev, vf, msgbuf);
		break;
	case E1000_VF_SET_VLAN:
		retval = igb_vf_set_vlan(dev, vf, msgbuf);
		break;
	default:
		PMD_INIT_LOG(DEBUG, "Unhandled Msg %8.8x",
			     (unsigned) msgbuf[0]);
		retval = E1000_ERR_MBX;
		break;
	}

	/* response the VF according to the message process result */
	if (retval)
		msgbuf[0] |= E1000_VT_MSGTYPE_NACK;
	else
		msgbuf[0] |= E1000_VT_MSGTYPE_ACK;

	msgbuf[0] |= E1000_VT_MSGTYPE_CTS;

	e1000_write_mbx(hw, msgbuf, 1, vf);

	return retval;
}

static inline void
igb_rcv_ack_from_vf(struct rte_eth_dev *dev, uint16_t vf)
{
	uint32_t msg = E1000_VT_MSGTYPE_NACK;
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	e1000_write_mbx(hw, &msg, 1, vf);
}

void igb_pf_mbx_process(struct rte_eth_dev *eth_dev)
{
	uint16_t vf;
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);

	for (vf = 0; vf < dev_num_vf(eth_dev); vf++) {
		/* check & process vf function level reset */
		if (!e1000_check_for_rst(hw, vf))
			igb_vf_reset_event(eth_dev, vf);

		/* check & process vf mailbox messages */
		if (!e1000_check_for_msg(hw, vf))
			igb_rcv_msg_from_vf(eth_dev, vf);

		/* check & process acks from vf */
		if (!e1000_check_for_ack(hw, vf))
			igb_rcv_ack_from_vf(eth_dev, vf);
	}
}
