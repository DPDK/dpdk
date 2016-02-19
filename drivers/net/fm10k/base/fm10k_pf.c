/*******************************************************************************

Copyright (c) 2013 - 2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 3. Neither the name of the Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#include "fm10k_pf.h"
#include "fm10k_vf.h"

/**
 *  fm10k_reset_hw_pf - PF hardware reset
 *  @hw: pointer to hardware structure
 *
 *  This function should return the hardware to a state similar to the
 *  one it is in after being powered on.
 **/
STATIC s32 fm10k_reset_hw_pf(struct fm10k_hw *hw)
{
	s32 err;
	u32 reg;
	u16 i;

	DEBUGFUNC("fm10k_reset_hw_pf");

	/* Disable interrupts */
	FM10K_WRITE_REG(hw, FM10K_EIMR, FM10K_EIMR_DISABLE(ALL));

	/* Lock ITR2 reg 0 into itself and disable interrupt moderation */
	FM10K_WRITE_REG(hw, FM10K_ITR2(0), 0);
	FM10K_WRITE_REG(hw, FM10K_INT_CTRL, 0);

	/* We assume here Tx and Rx queue 0 are owned by the PF */

	/* Shut off VF access to their queues forcing them to queue 0 */
	for (i = 0; i < FM10K_TQMAP_TABLE_SIZE; i++) {
		FM10K_WRITE_REG(hw, FM10K_TQMAP(i), 0);
		FM10K_WRITE_REG(hw, FM10K_RQMAP(i), 0);
	}

	/* shut down all rings */
	err = fm10k_disable_queues_generic(hw, FM10K_MAX_QUEUES);
	if (err)
		return err;

	/* Verify that DMA is no longer active */
	reg = FM10K_READ_REG(hw, FM10K_DMA_CTRL);
	if (reg & (FM10K_DMA_CTRL_TX_ACTIVE | FM10K_DMA_CTRL_RX_ACTIVE))
		return FM10K_ERR_DMA_PENDING;

	/* verify the switch is ready for reset */
	reg = FM10K_READ_REG(hw, FM10K_DMA_CTRL2);
	if (!(reg & FM10K_DMA_CTRL2_SWITCH_READY))
		goto out;

	/* Inititate data path reset */
	reg |= FM10K_DMA_CTRL_DATAPATH_RESET;
	FM10K_WRITE_REG(hw, FM10K_DMA_CTRL, reg);

	/* Flush write and allow 100us for reset to complete */
	FM10K_WRITE_FLUSH(hw);
	usec_delay(FM10K_RESET_TIMEOUT);

	/* Verify we made it out of reset */
	reg = FM10K_READ_REG(hw, FM10K_IP);
	if (!(reg & FM10K_IP_NOTINRESET))
		err = FM10K_ERR_RESET_FAILED;

out:
	return err;
}

/**
 *  fm10k_is_ari_hierarchy_pf - Indicate ARI hierarchy support
 *  @hw: pointer to hardware structure
 *
 *  Looks at the ARI hierarchy bit to determine whether ARI is supported or not.
 **/
STATIC bool fm10k_is_ari_hierarchy_pf(struct fm10k_hw *hw)
{
	u16 sriov_ctrl = FM10K_READ_PCI_WORD(hw, FM10K_PCIE_SRIOV_CTRL);

	DEBUGFUNC("fm10k_is_ari_hierarchy_pf");

	return !!(sriov_ctrl & FM10K_PCIE_SRIOV_CTRL_VFARI);
}

/**
 *  fm10k_init_hw_pf - PF hardware initialization
 *  @hw: pointer to hardware structure
 *
 **/
STATIC s32 fm10k_init_hw_pf(struct fm10k_hw *hw)
{
	u32 dma_ctrl, txqctl;
	u16 i;

	DEBUGFUNC("fm10k_init_hw_pf");

	/* Establish default VSI as valid */
	FM10K_WRITE_REG(hw, FM10K_DGLORTDEC(fm10k_dglort_default), 0);
	FM10K_WRITE_REG(hw, FM10K_DGLORTMAP(fm10k_dglort_default),
			FM10K_DGLORTMAP_ANY);

	/* Invalidate all other GLORT entries */
	for (i = 1; i < FM10K_DGLORT_COUNT; i++)
		FM10K_WRITE_REG(hw, FM10K_DGLORTMAP(i), FM10K_DGLORTMAP_NONE);

	/* reset ITR2(0) to point to itself */
	FM10K_WRITE_REG(hw, FM10K_ITR2(0), 0);

	/* reset VF ITR2(0) to point to 0 avoid PF registers */
	FM10K_WRITE_REG(hw, FM10K_ITR2(FM10K_ITR_REG_COUNT_PF), 0);

	/* loop through all PF ITR2 registers pointing them to the previous */
	for (i = 1; i < FM10K_ITR_REG_COUNT_PF; i++)
		FM10K_WRITE_REG(hw, FM10K_ITR2(i), i - 1);

	/* Enable interrupt moderator if not already enabled */
	FM10K_WRITE_REG(hw, FM10K_INT_CTRL, FM10K_INT_CTRL_ENABLEMODERATOR);

	/* compute the default txqctl configuration */
	txqctl = FM10K_TXQCTL_PF | FM10K_TXQCTL_UNLIMITED_BW |
		 (hw->mac.default_vid << FM10K_TXQCTL_VID_SHIFT);

	for (i = 0; i < FM10K_MAX_QUEUES; i++) {
		/* configure rings for 256 Queue / 32 Descriptor cache mode */
		FM10K_WRITE_REG(hw, FM10K_TQDLOC(i),
				(i * FM10K_TQDLOC_BASE_32_DESC) |
				FM10K_TQDLOC_SIZE_32_DESC);
		FM10K_WRITE_REG(hw, FM10K_TXQCTL(i), txqctl);

		/* configure rings to provide TPH processing hints */
		FM10K_WRITE_REG(hw, FM10K_TPH_TXCTRL(i),
				FM10K_TPH_TXCTRL_DESC_TPHEN |
				FM10K_TPH_TXCTRL_DESC_RROEN |
				FM10K_TPH_TXCTRL_DESC_WROEN |
				FM10K_TPH_TXCTRL_DATA_RROEN);
		FM10K_WRITE_REG(hw, FM10K_TPH_RXCTRL(i),
				FM10K_TPH_RXCTRL_DESC_TPHEN |
				FM10K_TPH_RXCTRL_DESC_RROEN |
				FM10K_TPH_RXCTRL_DATA_WROEN |
				FM10K_TPH_RXCTRL_HDR_WROEN);
	}

	/* set max hold interval to align with 1.024 usec in all modes and
	 * store ITR scale
	 */
	switch (hw->bus.speed) {
	case fm10k_bus_speed_2500:
		dma_ctrl = FM10K_DMA_CTRL_MAX_HOLD_1US_GEN1;
		hw->mac.itr_scale = FM10K_TDLEN_ITR_SCALE_GEN1;
		break;
	case fm10k_bus_speed_5000:
		dma_ctrl = FM10K_DMA_CTRL_MAX_HOLD_1US_GEN2;
		hw->mac.itr_scale = FM10K_TDLEN_ITR_SCALE_GEN2;
		break;
	case fm10k_bus_speed_8000:
		dma_ctrl = FM10K_DMA_CTRL_MAX_HOLD_1US_GEN3;
		hw->mac.itr_scale = FM10K_TDLEN_ITR_SCALE_GEN3;
		break;
	default:
		dma_ctrl = 0;
		/* just in case, assume Gen3 ITR scale */
		hw->mac.itr_scale = FM10K_TDLEN_ITR_SCALE_GEN3;
		break;
	}

	/* Configure TSO flags */
	FM10K_WRITE_REG(hw, FM10K_DTXTCPFLGL, FM10K_TSO_FLAGS_LOW);
	FM10K_WRITE_REG(hw, FM10K_DTXTCPFLGH, FM10K_TSO_FLAGS_HI);

	/* Enable DMA engine
	 * Set Rx Descriptor size to 32
	 * Set Minimum MSS to 64
	 * Set Maximum number of Rx queues to 256 / 32 Descriptor
	 */
	dma_ctrl |= FM10K_DMA_CTRL_TX_ENABLE | FM10K_DMA_CTRL_RX_ENABLE |
		    FM10K_DMA_CTRL_RX_DESC_SIZE | FM10K_DMA_CTRL_MINMSS_64 |
		    FM10K_DMA_CTRL_32_DESC;

	FM10K_WRITE_REG(hw, FM10K_DMA_CTRL, dma_ctrl);

	/* record maximum queue count, we limit ourselves to 128 */
	hw->mac.max_queues = FM10K_MAX_QUEUES_PF;

	/* We support either 64 VFs or 7 VFs depending on if we have ARI */
	hw->iov.total_vfs = fm10k_is_ari_hierarchy_pf(hw) ? 64 : 7;

	return FM10K_SUCCESS;
}

#ifndef NO_IS_SLOT_APPROPRIATE_CHECK
/**
 *  fm10k_is_slot_appropriate_pf - Indicate appropriate slot for this SKU
 *  @hw: pointer to hardware structure
 *
 *  Looks at the PCIe bus info to confirm whether or not this slot can support
 *  the necessary bandwidth for this device.
 **/
STATIC bool fm10k_is_slot_appropriate_pf(struct fm10k_hw *hw)
{
	DEBUGFUNC("fm10k_is_slot_appropriate_pf");

	return (hw->bus.speed == hw->bus_caps.speed) &&
	       (hw->bus.width == hw->bus_caps.width);
}

#endif
/**
 *  fm10k_update_vlan_pf - Update status of VLAN ID in VLAN filter table
 *  @hw: pointer to hardware structure
 *  @vid: VLAN ID to add to table
 *  @vsi: Index indicating VF ID or PF ID in table
 *  @set: Indicates if this is a set or clear operation
 *
 *  This function adds or removes the corresponding VLAN ID from the VLAN
 *  filter table for the corresponding function.  In addition to the
 *  standard set/clear that supports one bit a multi-bit write is
 *  supported to set 64 bits at a time.
 **/
STATIC s32 fm10k_update_vlan_pf(struct fm10k_hw *hw, u32 vid, u8 vsi, bool set)
{
	u32 vlan_table, reg, mask, bit, len;

	/* verify the VSI index is valid */
	if (vsi > FM10K_VLAN_TABLE_VSI_MAX)
		return FM10K_ERR_PARAM;

	/* VLAN multi-bit write:
	 * The multi-bit write has several parts to it.
	 *    3			  2		      1			  0
	 *  1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * | RSVD0 |         Length        |C|RSVD0|        VLAN ID        |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *
	 * VLAN ID: Vlan Starting value
	 * RSVD0: Reserved section, must be 0
	 * C: Flag field, 0 is set, 1 is clear (Used in VF VLAN message)
	 * Length: Number of times to repeat the bit being set
	 */
	len = vid >> 16;
	vid = (vid << 17) >> 17;

	/* verify the reserved 0 fields are 0 */
	if (len >= FM10K_VLAN_TABLE_VID_MAX || vid >= FM10K_VLAN_TABLE_VID_MAX)
		return FM10K_ERR_PARAM;

	/* Loop through the table updating all required VLANs */
	for (reg = FM10K_VLAN_TABLE(vsi, vid / 32), bit = vid % 32;
	     len < FM10K_VLAN_TABLE_VID_MAX;
	     len -= 32 - bit, reg++, bit = 0) {
		/* record the initial state of the register */
		vlan_table = FM10K_READ_REG(hw, reg);

		/* truncate mask if we are at the start or end of the run */
		mask = (~(u32)0 >> ((len < 31) ? 31 - len : 0)) << bit;

		/* make necessary modifications to the register */
		mask &= set ? ~vlan_table : vlan_table;
		if (mask)
			FM10K_WRITE_REG(hw, reg, vlan_table ^ mask);
	}

	return FM10K_SUCCESS;
}

/**
 *  fm10k_read_mac_addr_pf - Read device MAC address
 *  @hw: pointer to the HW structure
 *
 *  Reads the device MAC address from the SM_AREA and stores the value.
 **/
STATIC s32 fm10k_read_mac_addr_pf(struct fm10k_hw *hw)
{
	u8 perm_addr[ETH_ALEN];
	u32 serial_num;

	DEBUGFUNC("fm10k_read_mac_addr_pf");

	serial_num = FM10K_READ_REG(hw, FM10K_SM_AREA(1));

	/* last byte should be all 1's */
	if ((~serial_num) << 24)
		return  FM10K_ERR_INVALID_MAC_ADDR;

	perm_addr[0] = (u8)(serial_num >> 24);
	perm_addr[1] = (u8)(serial_num >> 16);
	perm_addr[2] = (u8)(serial_num >> 8);

	serial_num = FM10K_READ_REG(hw, FM10K_SM_AREA(0));

	/* first byte should be all 1's */
	if ((~serial_num) >> 24)
		return  FM10K_ERR_INVALID_MAC_ADDR;

	perm_addr[3] = (u8)(serial_num >> 16);
	perm_addr[4] = (u8)(serial_num >> 8);
	perm_addr[5] = (u8)(serial_num);

	memcpy(hw->mac.perm_addr, perm_addr, ETH_ALEN);
	memcpy(hw->mac.addr, perm_addr, ETH_ALEN);

	return FM10K_SUCCESS;
}

/**
 *  fm10k_glort_valid_pf - Validate that the provided glort is valid
 *  @hw: pointer to the HW structure
 *  @glort: base glort to be validated
 *
 *  This function will return an error if the provided glort is invalid
 **/
bool fm10k_glort_valid_pf(struct fm10k_hw *hw, u16 glort)
{
	glort &= hw->mac.dglort_map >> FM10K_DGLORTMAP_MASK_SHIFT;

	return glort == (hw->mac.dglort_map & FM10K_DGLORTMAP_NONE);
}

/**
 *  fm10k_update_xc_addr_pf - Update device addresses
 *  @hw: pointer to the HW structure
 *  @glort: base resource tag for this request
 *  @mac: MAC address to add/remove from table
 *  @vid: VLAN ID to add/remove from table
 *  @add: Indicates if this is an add or remove operation
 *  @flags: flags field to indicate add and secure
 *
 *  This function generates a message to the Switch API requesting
 *  that the given logical port add/remove the given L2 MAC/VLAN address.
 **/
STATIC s32 fm10k_update_xc_addr_pf(struct fm10k_hw *hw, u16 glort,
				   const u8 *mac, u16 vid, bool add, u8 flags)
{
	struct fm10k_mbx_info *mbx = &hw->mbx;
	struct fm10k_mac_update mac_update;
	u32 msg[5];

	DEBUGFUNC("fm10k_update_xc_addr_pf");

	/* clear set bit from VLAN ID */
	vid &= ~FM10K_VLAN_CLEAR;

	/* if glort or VLAN are not valid return error */
	if (!fm10k_glort_valid_pf(hw, glort) || vid >= FM10K_VLAN_TABLE_VID_MAX)
		return FM10K_ERR_PARAM;

	/* record fields */
	mac_update.mac_lower = FM10K_CPU_TO_LE32(((u32)mac[2] << 24) |
						 ((u32)mac[3] << 16) |
						 ((u32)mac[4] << 8) |
						 ((u32)mac[5]));
	mac_update.mac_upper = FM10K_CPU_TO_LE16(((u16)mac[0] << 8) |
					   ((u16)mac[1]));
	mac_update.vlan = FM10K_CPU_TO_LE16(vid);
	mac_update.glort = FM10K_CPU_TO_LE16(glort);
	mac_update.action = add ? 0 : 1;
	mac_update.flags = flags;

	/* populate mac_update fields */
	fm10k_tlv_msg_init(msg, FM10K_PF_MSG_ID_UPDATE_MAC_FWD_RULE);
	fm10k_tlv_attr_put_le_struct(msg, FM10K_PF_ATTR_ID_MAC_UPDATE,
				     &mac_update, sizeof(mac_update));

	/* load onto outgoing mailbox */
	return mbx->ops.enqueue_tx(hw, mbx, msg);
}

/**
 *  fm10k_update_uc_addr_pf - Update device unicast addresses
 *  @hw: pointer to the HW structure
 *  @glort: base resource tag for this request
 *  @mac: MAC address to add/remove from table
 *  @vid: VLAN ID to add/remove from table
 *  @add: Indicates if this is an add or remove operation
 *  @flags: flags field to indicate add and secure
 *
 *  This function is used to add or remove unicast addresses for
 *  the PF.
 **/
STATIC s32 fm10k_update_uc_addr_pf(struct fm10k_hw *hw, u16 glort,
				   const u8 *mac, u16 vid, bool add, u8 flags)
{
	DEBUGFUNC("fm10k_update_uc_addr_pf");

	/* verify MAC address is valid */
	if (!FM10K_IS_VALID_ETHER_ADDR(mac))
		return FM10K_ERR_PARAM;

	return fm10k_update_xc_addr_pf(hw, glort, mac, vid, add, flags);
}

/**
 *  fm10k_update_mc_addr_pf - Update device multicast addresses
 *  @hw: pointer to the HW structure
 *  @glort: base resource tag for this request
 *  @mac: MAC address to add/remove from table
 *  @vid: VLAN ID to add/remove from table
 *  @add: Indicates if this is an add or remove operation
 *
 *  This function is used to add or remove multicast MAC addresses for
 *  the PF.
 **/
STATIC s32 fm10k_update_mc_addr_pf(struct fm10k_hw *hw, u16 glort,
				   const u8 *mac, u16 vid, bool add)
{
	DEBUGFUNC("fm10k_update_mc_addr_pf");

	/* verify multicast address is valid */
	if (!FM10K_IS_MULTICAST_ETHER_ADDR(mac))
		return FM10K_ERR_PARAM;

	return fm10k_update_xc_addr_pf(hw, glort, mac, vid, add, 0);
}

/**
 *  fm10k_update_xcast_mode_pf - Request update of multicast mode
 *  @hw: pointer to hardware structure
 *  @glort: base resource tag for this request
 *  @mode: integer value indicating mode being requested
 *
 *  This function will attempt to request a higher mode for the port
 *  so that it can enable either multicast, multicast promiscuous, or
 *  promiscuous mode of operation.
 **/
STATIC s32 fm10k_update_xcast_mode_pf(struct fm10k_hw *hw, u16 glort, u8 mode)
{
	struct fm10k_mbx_info *mbx = &hw->mbx;
	u32 msg[3], xcast_mode;

	DEBUGFUNC("fm10k_update_xcast_mode_pf");

	if (mode > FM10K_XCAST_MODE_NONE)
		return FM10K_ERR_PARAM;

	/* if glort is not valid return error */
	if (!fm10k_glort_valid_pf(hw, glort))
		return FM10K_ERR_PARAM;

	/* write xcast mode as a single u32 value,
	 * lower 16 bits: glort
	 * upper 16 bits: mode
	 */
	xcast_mode = ((u32)mode << 16) | glort;

	/* generate message requesting to change xcast mode */
	fm10k_tlv_msg_init(msg, FM10K_PF_MSG_ID_XCAST_MODES);
	fm10k_tlv_attr_put_u32(msg, FM10K_PF_ATTR_ID_XCAST_MODE, xcast_mode);

	/* load onto outgoing mailbox */
	return mbx->ops.enqueue_tx(hw, mbx, msg);
}

/**
 *  fm10k_update_int_moderator_pf - Update interrupt moderator linked list
 *  @hw: pointer to hardware structure
 *
 *  This function walks through the MSI-X vector table to determine the
 *  number of active interrupts and based on that information updates the
 *  interrupt moderator linked list.
 **/
STATIC void fm10k_update_int_moderator_pf(struct fm10k_hw *hw)
{
	u32 i;

	/* Disable interrupt moderator */
	FM10K_WRITE_REG(hw, FM10K_INT_CTRL, 0);

	/* loop through PF from last to first looking enabled vectors */
	for (i = FM10K_ITR_REG_COUNT_PF - 1; i; i--) {
		if (!FM10K_READ_REG(hw, FM10K_MSIX_VECTOR_MASK(i)))
			break;
	}

	/* always reset VFITR2[0] to point to last enabled PF vector */
	FM10K_WRITE_REG(hw, FM10K_ITR2(FM10K_ITR_REG_COUNT_PF), i);

	/* reset ITR2[0] to point to last enabled PF vector */
	if (!hw->iov.num_vfs)
		FM10K_WRITE_REG(hw, FM10K_ITR2(0), i);

	/* Enable interrupt moderator */
	FM10K_WRITE_REG(hw, FM10K_INT_CTRL, FM10K_INT_CTRL_ENABLEMODERATOR);
}

/**
 *  fm10k_update_lport_state_pf - Notify the switch of a change in port state
 *  @hw: pointer to the HW structure
 *  @glort: base resource tag for this request
 *  @count: number of logical ports being updated
 *  @enable: boolean value indicating enable or disable
 *
 *  This function is used to add/remove a logical port from the switch.
 **/
STATIC s32 fm10k_update_lport_state_pf(struct fm10k_hw *hw, u16 glort,
				       u16 count, bool enable)
{
	struct fm10k_mbx_info *mbx = &hw->mbx;
	u32 msg[3], lport_msg;

	DEBUGFUNC("fm10k_lport_state_pf");

	/* do nothing if we are being asked to create or destroy 0 ports */
	if (!count)
		return FM10K_SUCCESS;

	/* if glort is not valid return error */
	if (!fm10k_glort_valid_pf(hw, glort))
		return FM10K_ERR_PARAM;

	/* construct the lport message from the 2 pieces of data we have */
	lport_msg = ((u32)count << 16) | glort;

	/* generate lport create/delete message */
	fm10k_tlv_msg_init(msg, enable ? FM10K_PF_MSG_ID_LPORT_CREATE :
					 FM10K_PF_MSG_ID_LPORT_DELETE);
	fm10k_tlv_attr_put_u32(msg, FM10K_PF_ATTR_ID_PORT, lport_msg);

	/* load onto outgoing mailbox */
	return mbx->ops.enqueue_tx(hw, mbx, msg);
}

/**
 *  fm10k_configure_dglort_map_pf - Configures GLORT entry and queues
 *  @hw: pointer to hardware structure
 *  @dglort: pointer to dglort configuration structure
 *
 *  Reads the configuration structure contained in dglort_cfg and uses
 *  that information to then populate a DGLORTMAP/DEC entry and the queues
 *  to which it has been assigned.
 **/
STATIC s32 fm10k_configure_dglort_map_pf(struct fm10k_hw *hw,
					 struct fm10k_dglort_cfg *dglort)
{
	u16 glort, queue_count, vsi_count, pc_count;
	u16 vsi, queue, pc, q_idx;
	u32 txqctl, dglortdec, dglortmap;

	/* verify the dglort pointer */
	if (!dglort)
		return FM10K_ERR_PARAM;

	/* verify the dglort values */
	if ((dglort->idx > 7) || (dglort->rss_l > 7) || (dglort->pc_l > 3) ||
	    (dglort->vsi_l > 6) || (dglort->vsi_b > 64) ||
	    (dglort->queue_l > 8) || (dglort->queue_b >= 256))
		return FM10K_ERR_PARAM;

	/* determine count of VSIs and queues */
	queue_count = BIT(dglort->rss_l + dglort->pc_l);
	vsi_count = BIT(dglort->vsi_l + dglort->queue_l);
	glort = dglort->glort;
	q_idx = dglort->queue_b;

	/* configure SGLORT for queues */
	for (vsi = 0; vsi < vsi_count; vsi++, glort++) {
		for (queue = 0; queue < queue_count; queue++, q_idx++) {
			if (q_idx >= FM10K_MAX_QUEUES)
				break;

			FM10K_WRITE_REG(hw, FM10K_TX_SGLORT(q_idx), glort);
			FM10K_WRITE_REG(hw, FM10K_RX_SGLORT(q_idx), glort);
		}
	}

	/* determine count of PCs and queues */
	queue_count = BIT(dglort->queue_l + dglort->rss_l + dglort->vsi_l);
	pc_count = BIT(dglort->pc_l);

	/* configure PC for Tx queues */
	for (pc = 0; pc < pc_count; pc++) {
		q_idx = pc + dglort->queue_b;
		for (queue = 0; queue < queue_count; queue++) {
			if (q_idx >= FM10K_MAX_QUEUES)
				break;

			txqctl = FM10K_READ_REG(hw, FM10K_TXQCTL(q_idx));
			txqctl &= ~FM10K_TXQCTL_PC_MASK;
			txqctl |= pc << FM10K_TXQCTL_PC_SHIFT;
			FM10K_WRITE_REG(hw, FM10K_TXQCTL(q_idx), txqctl);

			q_idx += pc_count;
		}
	}

	/* configure DGLORTDEC */
	dglortdec = ((u32)(dglort->rss_l) << FM10K_DGLORTDEC_RSSLENGTH_SHIFT) |
		    ((u32)(dglort->queue_b) << FM10K_DGLORTDEC_QBASE_SHIFT) |
		    ((u32)(dglort->pc_l) << FM10K_DGLORTDEC_PCLENGTH_SHIFT) |
		    ((u32)(dglort->vsi_b) << FM10K_DGLORTDEC_VSIBASE_SHIFT) |
		    ((u32)(dglort->vsi_l) << FM10K_DGLORTDEC_VSILENGTH_SHIFT) |
		    ((u32)(dglort->queue_l));
	if (dglort->inner_rss)
		dglortdec |=  FM10K_DGLORTDEC_INNERRSS_ENABLE;

	/* configure DGLORTMAP */
	dglortmap = (dglort->idx == fm10k_dglort_default) ?
			FM10K_DGLORTMAP_ANY : FM10K_DGLORTMAP_ZERO;
	dglortmap <<= dglort->vsi_l + dglort->queue_l + dglort->shared_l;
	dglortmap |= dglort->glort;

	/* write values to hardware */
	FM10K_WRITE_REG(hw, FM10K_DGLORTDEC(dglort->idx), dglortdec);
	FM10K_WRITE_REG(hw, FM10K_DGLORTMAP(dglort->idx), dglortmap);

	return FM10K_SUCCESS;
}

u16 fm10k_queues_per_pool(struct fm10k_hw *hw)
{
	u16 num_pools = hw->iov.num_pools;

	return (num_pools > 32) ? 2 : (num_pools > 16) ? 4 : (num_pools > 8) ?
	       8 : FM10K_MAX_QUEUES_POOL;
}

u16 fm10k_vf_queue_index(struct fm10k_hw *hw, u16 vf_idx)
{
	u16 num_vfs = hw->iov.num_vfs;
	u16 vf_q_idx = FM10K_MAX_QUEUES;

	vf_q_idx -= fm10k_queues_per_pool(hw) * (num_vfs - vf_idx);

	return vf_q_idx;
}

STATIC u16 fm10k_vectors_per_pool(struct fm10k_hw *hw)
{
	u16 num_pools = hw->iov.num_pools;

	return (num_pools > 32) ? 8 : (num_pools > 16) ? 16 :
	       FM10K_MAX_VECTORS_POOL;
}

STATIC u16 fm10k_vf_vector_index(struct fm10k_hw *hw, u16 vf_idx)
{
	u16 vf_v_idx = FM10K_MAX_VECTORS_PF;

	vf_v_idx += fm10k_vectors_per_pool(hw) * vf_idx;

	return vf_v_idx;
}

/**
 *  fm10k_iov_assign_resources_pf - Assign pool resources for virtualization
 *  @hw: pointer to the HW structure
 *  @num_vfs: number of VFs to be allocated
 *  @num_pools: number of virtualization pools to be allocated
 *
 *  Allocates queues and traffic classes to virtualization entities to prepare
 *  the PF for SR-IOV and VMDq
 **/
STATIC s32 fm10k_iov_assign_resources_pf(struct fm10k_hw *hw, u16 num_vfs,
					 u16 num_pools)
{
	u16 qmap_stride, qpp, vpp, vf_q_idx, vf_q_idx0, qmap_idx;
	u32 vid = hw->mac.default_vid << FM10K_TXQCTL_VID_SHIFT;
	int i, j;

	/* hardware only supports up to 64 pools */
	if (num_pools > 64)
		return FM10K_ERR_PARAM;

	/* the number of VFs cannot exceed the number of pools */
	if ((num_vfs > num_pools) || (num_vfs > hw->iov.total_vfs))
		return FM10K_ERR_PARAM;

	/* record number of virtualization entities */
	hw->iov.num_vfs = num_vfs;
	hw->iov.num_pools = num_pools;

	/* determine qmap offsets and counts */
	qmap_stride = (num_vfs > 8) ? 32 : 256;
	qpp = fm10k_queues_per_pool(hw);
	vpp = fm10k_vectors_per_pool(hw);

	/* calculate starting index for queues */
	vf_q_idx = fm10k_vf_queue_index(hw, 0);
	qmap_idx = 0;

	/* establish TCs with -1 credits and no quanta to prevent transmit */
	for (i = 0; i < num_vfs; i++) {
		FM10K_WRITE_REG(hw, FM10K_TC_MAXCREDIT(i), 0);
		FM10K_WRITE_REG(hw, FM10K_TC_RATE(i), 0);
		FM10K_WRITE_REG(hw, FM10K_TC_CREDIT(i),
				FM10K_TC_CREDIT_CREDIT_MASK);
	}

	/* zero out all mbmem registers */
	for (i = FM10K_VFMBMEM_LEN * num_vfs; i--;)
		FM10K_WRITE_REG(hw, FM10K_MBMEM(i), 0);

	/* clear event notification of VF FLR */
	FM10K_WRITE_REG(hw, FM10K_PFVFLREC(0), ~0);
	FM10K_WRITE_REG(hw, FM10K_PFVFLREC(1), ~0);

	/* loop through unallocated rings assigning them back to PF */
	for (i = FM10K_MAX_QUEUES_PF; i < vf_q_idx; i++) {
		FM10K_WRITE_REG(hw, FM10K_TXDCTL(i), 0);
		FM10K_WRITE_REG(hw, FM10K_TXQCTL(i), FM10K_TXQCTL_PF |
				FM10K_TXQCTL_UNLIMITED_BW | vid);
		FM10K_WRITE_REG(hw, FM10K_RXQCTL(i), FM10K_RXQCTL_PF);
	}

	/* PF should have already updated VFITR2[0] */

	/* update all ITR registers to flow to VFITR2[0] */
	for (i = FM10K_ITR_REG_COUNT_PF + 1; i < FM10K_ITR_REG_COUNT; i++) {
		if (!(i & (vpp - 1)))
			FM10K_WRITE_REG(hw, FM10K_ITR2(i), i - vpp);
		else
			FM10K_WRITE_REG(hw, FM10K_ITR2(i), i - 1);
	}

	/* update PF ITR2[0] to reference the last vector */
	FM10K_WRITE_REG(hw, FM10K_ITR2(0),
			fm10k_vf_vector_index(hw, num_vfs - 1));

	/* loop through rings populating rings and TCs */
	for (i = 0; i < num_vfs; i++) {
		/* record index for VF queue 0 for use in end of loop */
		vf_q_idx0 = vf_q_idx;

		for (j = 0; j < qpp; j++, qmap_idx++, vf_q_idx++) {
			/* assign VF and locked TC to queues */
			FM10K_WRITE_REG(hw, FM10K_TXDCTL(vf_q_idx), 0);
			FM10K_WRITE_REG(hw, FM10K_TXQCTL(vf_q_idx),
					(i << FM10K_TXQCTL_TC_SHIFT) | i |
					FM10K_TXQCTL_VF | vid);
			FM10K_WRITE_REG(hw, FM10K_RXDCTL(vf_q_idx),
					FM10K_RXDCTL_WRITE_BACK_MIN_DELAY |
					FM10K_RXDCTL_DROP_ON_EMPTY);
			FM10K_WRITE_REG(hw, FM10K_RXQCTL(vf_q_idx),
					(i << FM10K_RXQCTL_VF_SHIFT) |
					FM10K_RXQCTL_VF);

			/* map queue pair to VF */
			FM10K_WRITE_REG(hw, FM10K_TQMAP(qmap_idx), vf_q_idx);
			FM10K_WRITE_REG(hw, FM10K_RQMAP(qmap_idx), vf_q_idx);
		}

		/* repeat the first ring for all of the remaining VF rings */
		for (; j < qmap_stride; j++, qmap_idx++) {
			FM10K_WRITE_REG(hw, FM10K_TQMAP(qmap_idx), vf_q_idx0);
			FM10K_WRITE_REG(hw, FM10K_RQMAP(qmap_idx), vf_q_idx0);
		}
	}

	/* loop through remaining indexes assigning all to queue 0 */
	while (qmap_idx < FM10K_TQMAP_TABLE_SIZE) {
		FM10K_WRITE_REG(hw, FM10K_TQMAP(qmap_idx), 0);
		FM10K_WRITE_REG(hw, FM10K_RQMAP(qmap_idx), 0);
		qmap_idx++;
	}

	return FM10K_SUCCESS;
}

/**
 *  fm10k_iov_configure_tc_pf - Configure the shaping group for VF
 *  @hw: pointer to the HW structure
 *  @vf_idx: index of VF receiving GLORT
 *  @rate: Rate indicated in Mb/s
 *
 *  Configured the TC for a given VF to allow only up to a given number
 *  of Mb/s of outgoing Tx throughput.
 **/
STATIC s32 fm10k_iov_configure_tc_pf(struct fm10k_hw *hw, u16 vf_idx, int rate)
{
	/* configure defaults */
	u32 interval = FM10K_TC_RATE_INTERVAL_4US_GEN3;
	u32 tc_rate = FM10K_TC_RATE_QUANTA_MASK;

	/* verify vf is in range */
	if (vf_idx >= hw->iov.num_vfs)
		return FM10K_ERR_PARAM;

	/* set interval to align with 4.096 usec in all modes */
	switch (hw->bus.speed) {
	case fm10k_bus_speed_2500:
		interval = FM10K_TC_RATE_INTERVAL_4US_GEN1;
		break;
	case fm10k_bus_speed_5000:
		interval = FM10K_TC_RATE_INTERVAL_4US_GEN2;
		break;
	default:
		break;
	}

	if (rate) {
		if (rate > FM10K_VF_TC_MAX || rate < FM10K_VF_TC_MIN)
			return FM10K_ERR_PARAM;

		/* The quanta is measured in Bytes per 4.096 or 8.192 usec
		 * The rate is provided in Mbits per second
		 * To tralslate from rate to quanta we need to multiply the
		 * rate by 8.192 usec and divide by 8 bits/byte.  To avoid
		 * dealing with floating point we can round the values up
		 * to the nearest whole number ratio which gives us 128 / 125.
		 */
		tc_rate = (rate * 128) / 125;

		/* try to keep the rate limiting accurate by increasing
		 * the number of credits and interval for rates less than 4Gb/s
		 */
		if (rate < 4000)
			interval <<= 1;
		else
			tc_rate >>= 1;
	}

	/* update rate limiter with new values */
	FM10K_WRITE_REG(hw, FM10K_TC_RATE(vf_idx), tc_rate | interval);
	FM10K_WRITE_REG(hw, FM10K_TC_MAXCREDIT(vf_idx), FM10K_TC_MAXCREDIT_64K);
	FM10K_WRITE_REG(hw, FM10K_TC_CREDIT(vf_idx), FM10K_TC_MAXCREDIT_64K);

	return FM10K_SUCCESS;
}

/**
 *  fm10k_iov_assign_int_moderator_pf - Add VF interrupts to moderator list
 *  @hw: pointer to the HW structure
 *  @vf_idx: index of VF receiving GLORT
 *
 *  Update the interrupt moderator linked list to include any MSI-X
 *  interrupts which the VF has enabled in the MSI-X vector table.
 **/
STATIC s32 fm10k_iov_assign_int_moderator_pf(struct fm10k_hw *hw, u16 vf_idx)
{
	u16 vf_v_idx, vf_v_limit, i;

	/* verify vf is in range */
	if (vf_idx >= hw->iov.num_vfs)
		return FM10K_ERR_PARAM;

	/* determine vector offset and count */
	vf_v_idx = fm10k_vf_vector_index(hw, vf_idx);
	vf_v_limit = vf_v_idx + fm10k_vectors_per_pool(hw);

	/* search for first vector that is not masked */
	for (i = vf_v_limit - 1; i > vf_v_idx; i--) {
		if (!FM10K_READ_REG(hw, FM10K_MSIX_VECTOR_MASK(i)))
			break;
	}

	/* reset linked list so it now includes our active vectors */
	if (vf_idx == (hw->iov.num_vfs - 1))
		FM10K_WRITE_REG(hw, FM10K_ITR2(0), i);
	else
		FM10K_WRITE_REG(hw, FM10K_ITR2(vf_v_limit), i);

	return FM10K_SUCCESS;
}

/**
 *  fm10k_iov_assign_default_mac_vlan_pf - Assign a MAC and VLAN to VF
 *  @hw: pointer to the HW structure
 *  @vf_info: pointer to VF information structure
 *
 *  Assign a MAC address and default VLAN to a VF and notify it of the update
 **/
STATIC s32 fm10k_iov_assign_default_mac_vlan_pf(struct fm10k_hw *hw,
						struct fm10k_vf_info *vf_info)
{
	u16 qmap_stride, queues_per_pool, vf_q_idx, timeout, qmap_idx, i;
	u32 msg[4], txdctl, txqctl, tdbal = 0, tdbah = 0;
	s32 err = FM10K_SUCCESS;
	u16 vf_idx, vf_vid;

	/* verify vf is in range */
	if (!vf_info || vf_info->vf_idx >= hw->iov.num_vfs)
		return FM10K_ERR_PARAM;

	/* determine qmap offsets and counts */
	qmap_stride = (hw->iov.num_vfs > 8) ? 32 : 256;
	queues_per_pool = fm10k_queues_per_pool(hw);

	/* calculate starting index for queues */
	vf_idx = vf_info->vf_idx;
	vf_q_idx = fm10k_vf_queue_index(hw, vf_idx);
	qmap_idx = qmap_stride * vf_idx;

	/* MAP Tx queue back to 0 temporarily, and disable it */
	FM10K_WRITE_REG(hw, FM10K_TQMAP(qmap_idx), 0);
	FM10K_WRITE_REG(hw, FM10K_TXDCTL(vf_q_idx), 0);

	/* determine correct default VLAN ID */
	if (vf_info->pf_vid)
		vf_vid = vf_info->pf_vid | FM10K_VLAN_CLEAR;
	else
		vf_vid = vf_info->sw_vid;

	/* generate MAC_ADDR request */
	fm10k_tlv_msg_init(msg, FM10K_VF_MSG_ID_MAC_VLAN);
	fm10k_tlv_attr_put_mac_vlan(msg, FM10K_MAC_VLAN_MSG_DEFAULT_MAC,
				    vf_info->mac, vf_vid);

	/* load onto outgoing mailbox, ignore any errors on enqueue */
	if (vf_info->mbx.ops.enqueue_tx)
		vf_info->mbx.ops.enqueue_tx(hw, &vf_info->mbx, msg);

	/* verify ring has disabled before modifying base address registers */
	txdctl = FM10K_READ_REG(hw, FM10K_TXDCTL(vf_q_idx));
	for (timeout = 0; txdctl & FM10K_TXDCTL_ENABLE; timeout++) {
		/* limit ourselves to a 1ms timeout */
		if (timeout == 10) {
			err = FM10K_ERR_DMA_PENDING;
			goto err_out;
		}

		usec_delay(100);
		txdctl = FM10K_READ_REG(hw, FM10K_TXDCTL(vf_q_idx));
	}

	/* Update base address registers to contain MAC address */
	if (FM10K_IS_VALID_ETHER_ADDR(vf_info->mac)) {
		tdbal = (((u32)vf_info->mac[3]) << 24) |
			(((u32)vf_info->mac[4]) << 16) |
			(((u32)vf_info->mac[5]) << 8);

		tdbah = (((u32)0xFF)	        << 24) |
			(((u32)vf_info->mac[0]) << 16) |
			(((u32)vf_info->mac[1]) << 8) |
			((u32)vf_info->mac[2]);
	}

	/* Record the base address into queue 0 */
	FM10K_WRITE_REG(hw, FM10K_TDBAL(vf_q_idx), tdbal);
	FM10K_WRITE_REG(hw, FM10K_TDBAH(vf_q_idx), tdbah);

	/* Provide the VF the ITR scale, using software-defined fields in TDLEN
	 * to pass the information during VF initialization. See definition of
	 * FM10K_TDLEN_ITR_SCALE_SHIFT for more details.
	 */
	FM10K_WRITE_REG(hw, FM10K_TDLEN(vf_q_idx), hw->mac.itr_scale <<
						   FM10K_TDLEN_ITR_SCALE_SHIFT);

err_out:
	/* configure Queue control register */
	txqctl = ((u32)vf_vid << FM10K_TXQCTL_VID_SHIFT) &
		 FM10K_TXQCTL_VID_MASK;
	txqctl |= (vf_idx << FM10K_TXQCTL_TC_SHIFT) |
		  FM10K_TXQCTL_VF | vf_idx;

	/* assign VLAN ID */
	for (i = 0; i < queues_per_pool; i++)
		FM10K_WRITE_REG(hw, FM10K_TXQCTL(vf_q_idx + i), txqctl);

	/* restore the queue back to VF ownership */
	FM10K_WRITE_REG(hw, FM10K_TQMAP(qmap_idx), vf_q_idx);
	return err;
}

/**
 *  fm10k_iov_reset_resources_pf - Reassign queues and interrupts to a VF
 *  @hw: pointer to the HW structure
 *  @vf_info: pointer to VF information structure
 *
 *  Reassign the interrupts and queues to a VF following an FLR
 **/
STATIC s32 fm10k_iov_reset_resources_pf(struct fm10k_hw *hw,
					struct fm10k_vf_info *vf_info)
{
	u16 qmap_stride, queues_per_pool, vf_q_idx, qmap_idx;
	u32 tdbal = 0, tdbah = 0, txqctl, rxqctl;
	u16 vf_v_idx, vf_v_limit, vf_vid;
	u8 vf_idx = vf_info->vf_idx;
	int i;

	/* verify vf is in range */
	if (vf_idx >= hw->iov.num_vfs)
		return FM10K_ERR_PARAM;

	/* clear event notification of VF FLR */
	FM10K_WRITE_REG(hw, FM10K_PFVFLREC(vf_idx / 32), BIT(vf_idx % 32));

	/* force timeout and then disconnect the mailbox */
	vf_info->mbx.timeout = 0;
	if (vf_info->mbx.ops.disconnect)
		vf_info->mbx.ops.disconnect(hw, &vf_info->mbx);

	/* determine vector offset and count */
	vf_v_idx = fm10k_vf_vector_index(hw, vf_idx);
	vf_v_limit = vf_v_idx + fm10k_vectors_per_pool(hw);

	/* determine qmap offsets and counts */
	qmap_stride = (hw->iov.num_vfs > 8) ? 32 : 256;
	queues_per_pool = fm10k_queues_per_pool(hw);
	qmap_idx = qmap_stride * vf_idx;

	/* make all the queues inaccessible to the VF */
	for (i = qmap_idx; i < (qmap_idx + qmap_stride); i++) {
		FM10K_WRITE_REG(hw, FM10K_TQMAP(i), 0);
		FM10K_WRITE_REG(hw, FM10K_RQMAP(i), 0);
	}

	/* calculate starting index for queues */
	vf_q_idx = fm10k_vf_queue_index(hw, vf_idx);

	/* determine correct default VLAN ID */
	if (vf_info->pf_vid)
		vf_vid = vf_info->pf_vid;
	else
		vf_vid = vf_info->sw_vid;

	/* configure Queue control register */
	txqctl = ((u32)vf_vid << FM10K_TXQCTL_VID_SHIFT) |
		 (vf_idx << FM10K_TXQCTL_TC_SHIFT) |
		 FM10K_TXQCTL_VF | vf_idx;
	rxqctl = (vf_idx << FM10K_RXQCTL_VF_SHIFT) | FM10K_RXQCTL_VF;

	/* stop further DMA and reset queue ownership back to VF */
	for (i = vf_q_idx; i < (queues_per_pool + vf_q_idx); i++) {
		FM10K_WRITE_REG(hw, FM10K_TXDCTL(i), 0);
		FM10K_WRITE_REG(hw, FM10K_TXQCTL(i), txqctl);
		FM10K_WRITE_REG(hw, FM10K_RXDCTL(i),
				FM10K_RXDCTL_WRITE_BACK_MIN_DELAY |
				FM10K_RXDCTL_DROP_ON_EMPTY);
		FM10K_WRITE_REG(hw, FM10K_RXQCTL(i), rxqctl);
	}

	/* reset TC with -1 credits and no quanta to prevent transmit */
	FM10K_WRITE_REG(hw, FM10K_TC_MAXCREDIT(vf_idx), 0);
	FM10K_WRITE_REG(hw, FM10K_TC_RATE(vf_idx), 0);
	FM10K_WRITE_REG(hw, FM10K_TC_CREDIT(vf_idx),
			FM10K_TC_CREDIT_CREDIT_MASK);

	/* update our first entry in the table based on previous VF */
	if (!vf_idx)
		hw->mac.ops.update_int_moderator(hw);
	else
		hw->iov.ops.assign_int_moderator(hw, vf_idx - 1);

	/* reset linked list so it now includes our active vectors */
	if (vf_idx == (hw->iov.num_vfs - 1))
		FM10K_WRITE_REG(hw, FM10K_ITR2(0), vf_v_idx);
	else
		FM10K_WRITE_REG(hw, FM10K_ITR2(vf_v_limit), vf_v_idx);

	/* link remaining vectors so that next points to previous */
	for (vf_v_idx++; vf_v_idx < vf_v_limit; vf_v_idx++)
		FM10K_WRITE_REG(hw, FM10K_ITR2(vf_v_idx), vf_v_idx - 1);

	/* zero out MBMEM, VLAN_TABLE, RETA, RSSRK, and MRQC registers */
	for (i = FM10K_VFMBMEM_LEN; i--;)
		FM10K_WRITE_REG(hw, FM10K_MBMEM_VF(vf_idx, i), 0);
	for (i = FM10K_VLAN_TABLE_SIZE; i--;)
		FM10K_WRITE_REG(hw, FM10K_VLAN_TABLE(vf_info->vsi, i), 0);
	for (i = FM10K_RETA_SIZE; i--;)
		FM10K_WRITE_REG(hw, FM10K_RETA(vf_info->vsi, i), 0);
	for (i = FM10K_RSSRK_SIZE; i--;)
		FM10K_WRITE_REG(hw, FM10K_RSSRK(vf_info->vsi, i), 0);
	FM10K_WRITE_REG(hw, FM10K_MRQC(vf_info->vsi), 0);

	/* Update base address registers to contain MAC address */
	if (FM10K_IS_VALID_ETHER_ADDR(vf_info->mac)) {
		tdbal = (((u32)vf_info->mac[3]) << 24) |
			(((u32)vf_info->mac[4]) << 16) |
			(((u32)vf_info->mac[5]) << 8);
		tdbah = (((u32)0xFF)	   << 24) |
			(((u32)vf_info->mac[0]) << 16) |
			(((u32)vf_info->mac[1]) << 8) |
			((u32)vf_info->mac[2]);
	}

	/* map queue pairs back to VF from last to first */
	for (i = queues_per_pool; i--;) {
		FM10K_WRITE_REG(hw, FM10K_TDBAL(vf_q_idx + i), tdbal);
		FM10K_WRITE_REG(hw, FM10K_TDBAH(vf_q_idx + i), tdbah);
		/* See definition of FM10K_TDLEN_ITR_SCALE_SHIFT for an
		 * explanation of how TDLEN is used.
		 */
		FM10K_WRITE_REG(hw, FM10K_TDLEN(vf_q_idx + i),
				hw->mac.itr_scale <<
				FM10K_TDLEN_ITR_SCALE_SHIFT);
		FM10K_WRITE_REG(hw, FM10K_TQMAP(qmap_idx + i), vf_q_idx + i);
		FM10K_WRITE_REG(hw, FM10K_RQMAP(qmap_idx + i), vf_q_idx + i);
	}

	/* repeat the first ring for all the remaining VF rings */
	for (i = queues_per_pool; i < qmap_stride; i++) {
		FM10K_WRITE_REG(hw, FM10K_TQMAP(qmap_idx + i), vf_q_idx);
		FM10K_WRITE_REG(hw, FM10K_RQMAP(qmap_idx + i), vf_q_idx);
	}

	return FM10K_SUCCESS;
}

/**
 *  fm10k_iov_set_lport_pf - Assign and enable a logical port for a given VF
 *  @hw: pointer to hardware structure
 *  @vf_info: pointer to VF information structure
 *  @lport_idx: Logical port offset from the hardware glort
 *  @flags: Set of capability flags to extend port beyond basic functionality
 *
 *  This function allows enabling a VF port by assigning it a GLORT and
 *  setting the flags so that it can enable an Rx mode.
 **/
STATIC s32 fm10k_iov_set_lport_pf(struct fm10k_hw *hw,
				  struct fm10k_vf_info *vf_info,
				  u16 lport_idx, u8 flags)
{
	u16 glort = (hw->mac.dglort_map + lport_idx) & FM10K_DGLORTMAP_NONE;

	DEBUGFUNC("fm10k_iov_set_lport_state_pf");

	/* if glort is not valid return error */
	if (!fm10k_glort_valid_pf(hw, glort))
		return FM10K_ERR_PARAM;

	vf_info->vf_flags = flags | FM10K_VF_FLAG_NONE_CAPABLE;
	vf_info->glort = glort;

	return FM10K_SUCCESS;
}

/**
 *  fm10k_iov_reset_lport_pf - Disable a logical port for a given VF
 *  @hw: pointer to hardware structure
 *  @vf_info: pointer to VF information structure
 *
 *  This function disables a VF port by stripping it of a GLORT and
 *  setting the flags so that it cannot enable any Rx mode.
 **/
STATIC void fm10k_iov_reset_lport_pf(struct fm10k_hw *hw,
				     struct fm10k_vf_info *vf_info)
{
	u32 msg[1];

	DEBUGFUNC("fm10k_iov_reset_lport_state_pf");

	/* need to disable the port if it is already enabled */
	if (FM10K_VF_FLAG_ENABLED(vf_info)) {
		/* notify switch that this port has been disabled */
		fm10k_update_lport_state_pf(hw, vf_info->glort, 1, false);

		/* generate port state response to notify VF it is not ready */
		fm10k_tlv_msg_init(msg, FM10K_VF_MSG_ID_LPORT_STATE);
		vf_info->mbx.ops.enqueue_tx(hw, &vf_info->mbx, msg);
	}

	/* clear flags and glort if it exists */
	vf_info->vf_flags = 0;
	vf_info->glort = 0;
}

/**
 *  fm10k_iov_update_stats_pf - Updates hardware related statistics for VFs
 *  @hw: pointer to hardware structure
 *  @q: stats for all queues of a VF
 *  @vf_idx: index of VF
 *
 *  This function collects queue stats for VFs.
 **/
STATIC void fm10k_iov_update_stats_pf(struct fm10k_hw *hw,
				      struct fm10k_hw_stats_q *q,
				      u16 vf_idx)
{
	u32 idx, qpp;

	/* get stats for all of the queues */
	qpp = fm10k_queues_per_pool(hw);
	idx = fm10k_vf_queue_index(hw, vf_idx);
	fm10k_update_hw_stats_q(hw, q, idx, qpp);
}

/**
 *  fm10k_iov_msg_msix_pf - Message handler for MSI-X request from VF
 *  @hw: Pointer to hardware structure
 *  @results: Pointer array to message, results[0] is pointer to message
 *  @mbx: Pointer to mailbox information structure
 *
 *  This function is a default handler for MSI-X requests from the VF.  The
 *  assumption is that in this case it is acceptable to just directly
 *  hand off the message from the VF to the underlying shared code.
 **/
s32 fm10k_iov_msg_msix_pf(struct fm10k_hw *hw, u32 **results,
			  struct fm10k_mbx_info *mbx)
{
	struct fm10k_vf_info *vf_info = (struct fm10k_vf_info *)mbx;
	u8 vf_idx = vf_info->vf_idx;

	UNREFERENCED_1PARAMETER(results);
	DEBUGFUNC("fm10k_iov_msg_msix_pf");

	return hw->iov.ops.assign_int_moderator(hw, vf_idx);
}

/**
 * fm10k_iov_select_vid - Select correct default VLAN ID
 * @hw: Pointer to hardware structure
 * @vid: VLAN ID to correct
 *
 * Will report an error if the VLAN ID is out of range. For VID = 0, it will
 * return either the pf_vid or sw_vid depending on which one is set.
 */
STATIC s32 fm10k_iov_select_vid(struct fm10k_vf_info *vf_info, u16 vid)
{
	if (!vid)
		return vf_info->pf_vid ? vf_info->pf_vid : vf_info->sw_vid;
	else if (vf_info->pf_vid && vid != vf_info->pf_vid)
		return FM10K_ERR_PARAM;
	else
		return vid;
}

/**
 *  fm10k_iov_msg_mac_vlan_pf - Message handler for MAC/VLAN request from VF
 *  @hw: Pointer to hardware structure
 *  @results: Pointer array to message, results[0] is pointer to message
 *  @mbx: Pointer to mailbox information structure
 *
 *  This function is a default handler for MAC/VLAN requests from the VF.
 *  The assumption is that in this case it is acceptable to just directly
 *  hand off the message from the VF to the underlying shared code.
 **/
s32 fm10k_iov_msg_mac_vlan_pf(struct fm10k_hw *hw, u32 **results,
			      struct fm10k_mbx_info *mbx)
{
	struct fm10k_vf_info *vf_info = (struct fm10k_vf_info *)mbx;
	u8 mac[ETH_ALEN];
	u32 *result;
	int err = FM10K_SUCCESS;
	bool set;
	u16 vlan;
	u32 vid;

	DEBUGFUNC("fm10k_iov_msg_mac_vlan_pf");

	/* we shouldn't be updating rules on a disabled interface */
	if (!FM10K_VF_FLAG_ENABLED(vf_info))
		err = FM10K_ERR_PARAM;

	if (!err && !!results[FM10K_MAC_VLAN_MSG_VLAN]) {
		result = results[FM10K_MAC_VLAN_MSG_VLAN];

		/* record VLAN id requested */
		err = fm10k_tlv_attr_get_u32(result, &vid);
		if (err)
			return err;

		/* verify upper 16 bits are zero */
		if (vid >> 16)
			return FM10K_ERR_PARAM;

		set = !(vid & FM10K_VLAN_CLEAR);
		vid &= ~FM10K_VLAN_CLEAR;

		err = fm10k_iov_select_vid(vf_info, (u16)vid);
		if (err < 0)
			return err;

		vid = err;

		/* update VSI info for VF in regards to VLAN table */
		err = hw->mac.ops.update_vlan(hw, vid, vf_info->vsi, set);
	}

	if (!err && !!results[FM10K_MAC_VLAN_MSG_MAC]) {
		result = results[FM10K_MAC_VLAN_MSG_MAC];

		/* record unicast MAC address requested */
		err = fm10k_tlv_attr_get_mac_vlan(result, mac, &vlan);
		if (err)
			return err;

		/* block attempts to set MAC for a locked device */
		if (FM10K_IS_VALID_ETHER_ADDR(vf_info->mac) &&
		    memcmp(mac, vf_info->mac, ETH_ALEN))
			return FM10K_ERR_PARAM;

		set = !(vlan & FM10K_VLAN_CLEAR);
		vlan &= ~FM10K_VLAN_CLEAR;

		err = fm10k_iov_select_vid(vf_info, vlan);
		if (err < 0)
			return err;

		vlan = (u16)err;

		/* notify switch of request for new unicast address */
		err = hw->mac.ops.update_uc_addr(hw, vf_info->glort,
						 mac, vlan, set, 0);
	}

	if (!err && !!results[FM10K_MAC_VLAN_MSG_MULTICAST]) {
		result = results[FM10K_MAC_VLAN_MSG_MULTICAST];

		/* record multicast MAC address requested */
		err = fm10k_tlv_attr_get_mac_vlan(result, mac, &vlan);
		if (err)
			return err;

		/* verify that the VF is allowed to request multicast */
		if (!(vf_info->vf_flags & FM10K_VF_FLAG_MULTI_ENABLED))
			return FM10K_ERR_PARAM;

		set = !(vlan & FM10K_VLAN_CLEAR);
		vlan &= ~FM10K_VLAN_CLEAR;

		err = fm10k_iov_select_vid(vf_info, vlan);
		if (err < 0)
			return err;

		vlan = (u16)err;

		/* notify switch of request for new multicast address */
		err = hw->mac.ops.update_mc_addr(hw, vf_info->glort,
						 mac, vlan, set);
	}

	return err;
}

/**
 *  fm10k_iov_supported_xcast_mode_pf - Determine best match for xcast mode
 *  @vf_info: VF info structure containing capability flags
 *  @mode: Requested xcast mode
 *
 *  This function outputs the mode that most closely matches the requested
 *  mode.  If not modes match it will request we disable the port
 **/
STATIC u8 fm10k_iov_supported_xcast_mode_pf(struct fm10k_vf_info *vf_info,
					    u8 mode)
{
	u8 vf_flags = vf_info->vf_flags;

	/* match up mode to capabilities as best as possible */
	switch (mode) {
	case FM10K_XCAST_MODE_PROMISC:
		if (vf_flags & FM10K_VF_FLAG_PROMISC_CAPABLE)
			return FM10K_XCAST_MODE_PROMISC;
		/* fallthough */
	case FM10K_XCAST_MODE_ALLMULTI:
		if (vf_flags & FM10K_VF_FLAG_ALLMULTI_CAPABLE)
			return FM10K_XCAST_MODE_ALLMULTI;
		/* fallthough */
	case FM10K_XCAST_MODE_MULTI:
		if (vf_flags & FM10K_VF_FLAG_MULTI_CAPABLE)
			return FM10K_XCAST_MODE_MULTI;
		/* fallthough */
	case FM10K_XCAST_MODE_NONE:
		if (vf_flags & FM10K_VF_FLAG_NONE_CAPABLE)
			return FM10K_XCAST_MODE_NONE;
		/* fallthough */
	default:
		break;
	}

	/* disable interface as it should not be able to request any */
	return FM10K_XCAST_MODE_DISABLE;
}

/**
 *  fm10k_iov_msg_lport_state_pf - Message handler for port state requests
 *  @hw: Pointer to hardware structure
 *  @results: Pointer array to message, results[0] is pointer to message
 *  @mbx: Pointer to mailbox information structure
 *
 *  This function is a default handler for port state requests.  The port
 *  state requests for now are basic and consist of enabling or disabling
 *  the port.
 **/
s32 fm10k_iov_msg_lport_state_pf(struct fm10k_hw *hw, u32 **results,
				 struct fm10k_mbx_info *mbx)
{
	struct fm10k_vf_info *vf_info = (struct fm10k_vf_info *)mbx;
	u32 *result;
	s32 err = FM10K_SUCCESS;
	u32 msg[2];
	u8 mode = 0;

	DEBUGFUNC("fm10k_iov_msg_lport_state_pf");

	/* verify VF is allowed to enable even minimal mode */
	if (!(vf_info->vf_flags & FM10K_VF_FLAG_NONE_CAPABLE))
		return FM10K_ERR_PARAM;

	if (!!results[FM10K_LPORT_STATE_MSG_XCAST_MODE]) {
		result = results[FM10K_LPORT_STATE_MSG_XCAST_MODE];

		/* XCAST mode update requested */
		err = fm10k_tlv_attr_get_u8(result, &mode);
		if (err)
			return FM10K_ERR_PARAM;

		/* prep for possible demotion depending on capabilities */
		mode = fm10k_iov_supported_xcast_mode_pf(vf_info, mode);

		/* if mode is not currently enabled, enable it */
		if (!(FM10K_VF_FLAG_ENABLED(vf_info) & BIT(mode)))
			fm10k_update_xcast_mode_pf(hw, vf_info->glort, mode);

		/* swap mode back to a bit flag */
		mode = FM10K_VF_FLAG_SET_MODE(mode);
	} else if (!results[FM10K_LPORT_STATE_MSG_DISABLE]) {
		/* need to disable the port if it is already enabled */
		if (FM10K_VF_FLAG_ENABLED(vf_info))
			err = fm10k_update_lport_state_pf(hw, vf_info->glort,
							  1, false);

		/* we need to clear VF_FLAG_ENABLED flags in order to ensure
		 * that we actually re-enable the LPORT state below. Note that
		 * this has no impact if the VF is already disabled, as the
		 * flags are already cleared.
		 */
		if (!err)
			vf_info->vf_flags = FM10K_VF_FLAG_CAPABLE(vf_info);

		/* when enabling the port we should reset the rate limiters */
		hw->iov.ops.configure_tc(hw, vf_info->vf_idx, vf_info->rate);

		/* set mode for minimal functionality */
		mode = FM10K_VF_FLAG_SET_MODE_NONE;

		/* generate port state response to notify VF it is ready */
		fm10k_tlv_msg_init(msg, FM10K_VF_MSG_ID_LPORT_STATE);
		fm10k_tlv_attr_put_bool(msg, FM10K_LPORT_STATE_MSG_READY);
		mbx->ops.enqueue_tx(hw, mbx, msg);
	}

	/* if enable state toggled note the update */
	if (!err && (!FM10K_VF_FLAG_ENABLED(vf_info) != !mode))
		err = fm10k_update_lport_state_pf(hw, vf_info->glort, 1,
						  !!mode);

	/* if state change succeeded, then update our stored state */
	mode |= FM10K_VF_FLAG_CAPABLE(vf_info);
	if (!err)
		vf_info->vf_flags = mode;

	return err;
}

#ifndef NO_DEFAULT_SRIOV_MSG_HANDLERS
const struct fm10k_msg_data fm10k_iov_msg_data_pf[] = {
	FM10K_TLV_MSG_TEST_HANDLER(fm10k_tlv_msg_test),
	FM10K_VF_MSG_MSIX_HANDLER(fm10k_iov_msg_msix_pf),
	FM10K_VF_MSG_MAC_VLAN_HANDLER(fm10k_iov_msg_mac_vlan_pf),
	FM10K_VF_MSG_LPORT_STATE_HANDLER(fm10k_iov_msg_lport_state_pf),
	FM10K_TLV_MSG_ERROR_HANDLER(fm10k_tlv_msg_error),
};

#endif
/**
 *  fm10k_update_stats_hw_pf - Updates hardware related statistics of PF
 *  @hw: pointer to hardware structure
 *  @stats: pointer to the stats structure to update
 *
 *  This function collects and aggregates global and per queue hardware
 *  statistics.
 **/
STATIC void fm10k_update_hw_stats_pf(struct fm10k_hw *hw,
				     struct fm10k_hw_stats *stats)
{
	u32 timeout, ur, ca, um, xec, vlan_drop, loopback_drop, nodesc_drop;
	u32 id, id_prev;

	DEBUGFUNC("fm10k_update_hw_stats_pf");

	/* Use Tx queue 0 as a canary to detect a reset */
	id = FM10K_READ_REG(hw, FM10K_TXQCTL(0));

	/* Read Global Statistics */
	do {
		timeout = fm10k_read_hw_stats_32b(hw, FM10K_STATS_TIMEOUT,
						  &stats->timeout);
		ur = fm10k_read_hw_stats_32b(hw, FM10K_STATS_UR, &stats->ur);
		ca = fm10k_read_hw_stats_32b(hw, FM10K_STATS_CA, &stats->ca);
		um = fm10k_read_hw_stats_32b(hw, FM10K_STATS_UM, &stats->um);
		xec = fm10k_read_hw_stats_32b(hw, FM10K_STATS_XEC, &stats->xec);
		vlan_drop = fm10k_read_hw_stats_32b(hw, FM10K_STATS_VLAN_DROP,
						    &stats->vlan_drop);
		loopback_drop =
			fm10k_read_hw_stats_32b(hw,
						FM10K_STATS_LOOPBACK_DROP,
						&stats->loopback_drop);
		nodesc_drop = fm10k_read_hw_stats_32b(hw,
						      FM10K_STATS_NODESC_DROP,
						      &stats->nodesc_drop);

		/* if value has not changed then we have consistent data */
		id_prev = id;
		id = FM10K_READ_REG(hw, FM10K_TXQCTL(0));
	} while ((id ^ id_prev) & FM10K_TXQCTL_ID_MASK);

	/* drop non-ID bits and set VALID ID bit */
	id &= FM10K_TXQCTL_ID_MASK;
	id |= FM10K_STAT_VALID;

	/* Update Global Statistics */
	if (stats->stats_idx == id) {
		stats->timeout.count += timeout;
		stats->ur.count += ur;
		stats->ca.count += ca;
		stats->um.count += um;
		stats->xec.count += xec;
		stats->vlan_drop.count += vlan_drop;
		stats->loopback_drop.count += loopback_drop;
		stats->nodesc_drop.count += nodesc_drop;
	}

	/* Update bases and record current PF id */
	fm10k_update_hw_base_32b(&stats->timeout, timeout);
	fm10k_update_hw_base_32b(&stats->ur, ur);
	fm10k_update_hw_base_32b(&stats->ca, ca);
	fm10k_update_hw_base_32b(&stats->um, um);
	fm10k_update_hw_base_32b(&stats->xec, xec);
	fm10k_update_hw_base_32b(&stats->vlan_drop, vlan_drop);
	fm10k_update_hw_base_32b(&stats->loopback_drop, loopback_drop);
	fm10k_update_hw_base_32b(&stats->nodesc_drop, nodesc_drop);
	stats->stats_idx = id;

	/* Update Queue Statistics */
	fm10k_update_hw_stats_q(hw, stats->q, 0, hw->mac.max_queues);
}

/**
 *  fm10k_rebind_hw_stats_pf - Resets base for hardware statistics of PF
 *  @hw: pointer to hardware structure
 *  @stats: pointer to the stats structure to update
 *
 *  This function resets the base for global and per queue hardware
 *  statistics.
 **/
STATIC void fm10k_rebind_hw_stats_pf(struct fm10k_hw *hw,
				     struct fm10k_hw_stats *stats)
{
	DEBUGFUNC("fm10k_rebind_hw_stats_pf");

	/* Unbind Global Statistics */
	fm10k_unbind_hw_stats_32b(&stats->timeout);
	fm10k_unbind_hw_stats_32b(&stats->ur);
	fm10k_unbind_hw_stats_32b(&stats->ca);
	fm10k_unbind_hw_stats_32b(&stats->um);
	fm10k_unbind_hw_stats_32b(&stats->xec);
	fm10k_unbind_hw_stats_32b(&stats->vlan_drop);
	fm10k_unbind_hw_stats_32b(&stats->loopback_drop);
	fm10k_unbind_hw_stats_32b(&stats->nodesc_drop);

	/* Unbind Queue Statistics */
	fm10k_unbind_hw_stats_q(stats->q, 0, hw->mac.max_queues);

	/* Reinitialize bases for all stats */
	fm10k_update_hw_stats_pf(hw, stats);
}

/**
 *  fm10k_set_dma_mask_pf - Configures PhyAddrSpace to limit DMA to system
 *  @hw: pointer to hardware structure
 *  @dma_mask: 64 bit DMA mask required for platform
 *
 *  This function sets the PHYADDR.PhyAddrSpace bits for the endpoint in order
 *  to limit the access to memory beyond what is physically in the system.
 **/
STATIC void fm10k_set_dma_mask_pf(struct fm10k_hw *hw, u64 dma_mask)
{
	/* we need to write the upper 32 bits of DMA mask to PhyAddrSpace */
	u32 phyaddr = (u32)(dma_mask >> 32);

	DEBUGFUNC("fm10k_set_dma_mask_pf");

	FM10K_WRITE_REG(hw, FM10K_PHYADDR, phyaddr);
}

/**
 *  fm10k_get_fault_pf - Record a fault in one of the interface units
 *  @hw: pointer to hardware structure
 *  @type: pointer to fault type register offset
 *  @fault: pointer to memory location to record the fault
 *
 *  Record the fault register contents to the fault data structure and
 *  clear the entry from the register.
 *
 *  Returns ERR_PARAM if invalid register is specified or no error is present.
 **/
STATIC s32 fm10k_get_fault_pf(struct fm10k_hw *hw, int type,
			      struct fm10k_fault *fault)
{
	u32 func;

	DEBUGFUNC("fm10k_get_fault_pf");

	/* verify the fault register is in range and is aligned */
	switch (type) {
	case FM10K_PCA_FAULT:
	case FM10K_THI_FAULT:
	case FM10K_FUM_FAULT:
		break;
	default:
		return FM10K_ERR_PARAM;
	}

	/* only service faults that are valid */
	func = FM10K_READ_REG(hw, type + FM10K_FAULT_FUNC);
	if (!(func & FM10K_FAULT_FUNC_VALID))
		return FM10K_ERR_PARAM;

	/* read remaining fields */
	fault->address = FM10K_READ_REG(hw, type + FM10K_FAULT_ADDR_HI);
	fault->address <<= 32;
	fault->address = FM10K_READ_REG(hw, type + FM10K_FAULT_ADDR_LO);
	fault->specinfo = FM10K_READ_REG(hw, type + FM10K_FAULT_SPECINFO);

	/* clear valid bit to allow for next error */
	FM10K_WRITE_REG(hw, type + FM10K_FAULT_FUNC, FM10K_FAULT_FUNC_VALID);

	/* Record which function triggered the error */
	if (func & FM10K_FAULT_FUNC_PF)
		fault->func = 0;
	else
		fault->func = 1 + ((func & FM10K_FAULT_FUNC_VF_MASK) >>
				   FM10K_FAULT_FUNC_VF_SHIFT);

	/* record fault type */
	fault->type = func & FM10K_FAULT_FUNC_TYPE_MASK;

	return FM10K_SUCCESS;
}

/**
 *  fm10k_request_lport_map_pf - Request LPORT map from the switch API
 *  @hw: pointer to hardware structure
 *
 **/
STATIC s32 fm10k_request_lport_map_pf(struct fm10k_hw *hw)
{
	struct fm10k_mbx_info *mbx = &hw->mbx;
	u32 msg[1];

	DEBUGFUNC("fm10k_request_lport_pf");

	/* issue request asking for LPORT map */
	fm10k_tlv_msg_init(msg, FM10K_PF_MSG_ID_LPORT_MAP);

	/* load onto outgoing mailbox */
	return mbx->ops.enqueue_tx(hw, mbx, msg);
}

/**
 *  fm10k_get_host_state_pf - Returns the state of the switch and mailbox
 *  @hw: pointer to hardware structure
 *  @switch_ready: pointer to boolean value that will record switch state
 *
 *  This funciton will check the DMA_CTRL2 register and mailbox in order
 *  to determine if the switch is ready for the PF to begin requesting
 *  addresses and mapping traffic to the local interface.
 **/
STATIC s32 fm10k_get_host_state_pf(struct fm10k_hw *hw, bool *switch_ready)
{
	s32 ret_val = FM10K_SUCCESS;
	u32 dma_ctrl2;

	DEBUGFUNC("fm10k_get_host_state_pf");

	/* verify the switch is ready for interaction */
	dma_ctrl2 = FM10K_READ_REG(hw, FM10K_DMA_CTRL2);
	if (!(dma_ctrl2 & FM10K_DMA_CTRL2_SWITCH_READY))
		goto out;

	/* retrieve generic host state info */
	ret_val = fm10k_get_host_state_generic(hw, switch_ready);
	if (ret_val)
		goto out;

	/* interface cannot receive traffic without logical ports */
	if (hw->mac.dglort_map == FM10K_DGLORTMAP_NONE)
		ret_val = fm10k_request_lport_map_pf(hw);

out:
	return ret_val;
}

/* This structure defines the attibutes to be parsed below */
const struct fm10k_tlv_attr fm10k_lport_map_msg_attr[] = {
	FM10K_TLV_ATTR_U32(FM10K_PF_ATTR_ID_LPORT_MAP),
	FM10K_TLV_ATTR_LAST
};

/**
 *  fm10k_msg_lport_map_pf - Message handler for lport_map message from SM
 *  @hw: Pointer to hardware structure
 *  @results: pointer array containing parsed data
 *  @mbx: Pointer to mailbox information structure
 *
 *  This handler configures the lport mapping based on the reply from the
 *  switch API.
 **/
s32 fm10k_msg_lport_map_pf(struct fm10k_hw *hw, u32 **results,
			   struct fm10k_mbx_info *mbx)
{
	u16 glort, mask;
	u32 dglort_map;
	s32 err;

	UNREFERENCED_1PARAMETER(mbx);
	DEBUGFUNC("fm10k_msg_lport_map_pf");

	err = fm10k_tlv_attr_get_u32(results[FM10K_PF_ATTR_ID_LPORT_MAP],
				     &dglort_map);
	if (err)
		return err;

	/* extract values out of the header */
	glort = FM10K_MSG_HDR_FIELD_GET(dglort_map, LPORT_MAP_GLORT);
	mask = FM10K_MSG_HDR_FIELD_GET(dglort_map, LPORT_MAP_MASK);

	/* verify mask is set and none of the masked bits in glort are set */
	if (!mask || (glort & ~mask))
		return FM10K_ERR_PARAM;

	/* verify the mask is contiguous, and that it is 1's followed by 0's */
	if (((~(mask - 1) & mask) + mask) & FM10K_DGLORTMAP_NONE)
		return FM10K_ERR_PARAM;

	/* record the glort, mask, and port count */
	hw->mac.dglort_map = dglort_map;

	return FM10K_SUCCESS;
}

const struct fm10k_tlv_attr fm10k_update_pvid_msg_attr[] = {
	FM10K_TLV_ATTR_U32(FM10K_PF_ATTR_ID_UPDATE_PVID),
	FM10K_TLV_ATTR_LAST
};

/**
 *  fm10k_msg_update_pvid_pf - Message handler for port VLAN message from SM
 *  @hw: Pointer to hardware structure
 *  @results: pointer array containing parsed data
 *  @mbx: Pointer to mailbox information structure
 *
 *  This handler configures the default VLAN for the PF
 **/
static s32 fm10k_msg_update_pvid_pf(struct fm10k_hw *hw, u32 **results,
				    struct fm10k_mbx_info *mbx)
{
	u16 glort, pvid;
	u32 pvid_update;
	s32 err;

	UNREFERENCED_1PARAMETER(mbx);
	DEBUGFUNC("fm10k_msg_update_pvid_pf");

	err = fm10k_tlv_attr_get_u32(results[FM10K_PF_ATTR_ID_UPDATE_PVID],
				     &pvid_update);
	if (err)
		return err;

	/* extract values from the pvid update */
	glort = FM10K_MSG_HDR_FIELD_GET(pvid_update, UPDATE_PVID_GLORT);
	pvid = FM10K_MSG_HDR_FIELD_GET(pvid_update, UPDATE_PVID_PVID);

	/* if glort is not valid return error */
	if (!fm10k_glort_valid_pf(hw, glort))
		return FM10K_ERR_PARAM;

	/* verify VLAN ID is valid */
	if (pvid >= FM10K_VLAN_TABLE_VID_MAX)
		return FM10K_ERR_PARAM;

	/* record the port VLAN ID value */
	hw->mac.default_vid = pvid;

	return FM10K_SUCCESS;
}

/**
 *  fm10k_record_global_table_data - Move global table data to swapi table info
 *  @from: pointer to source table data structure
 *  @to: pointer to destination table info structure
 *
 *  This function is will copy table_data to the table_info contained in
 *  the hw struct.
 **/
static void fm10k_record_global_table_data(struct fm10k_global_table_data *from,
					   struct fm10k_swapi_table_info *to)
{
	/* convert from le32 struct to CPU byte ordered values */
	to->used = FM10K_LE32_TO_CPU(from->used);
	to->avail = FM10K_LE32_TO_CPU(from->avail);
}

const struct fm10k_tlv_attr fm10k_err_msg_attr[] = {
	FM10K_TLV_ATTR_LE_STRUCT(FM10K_PF_ATTR_ID_ERR,
				 sizeof(struct fm10k_swapi_error)),
	FM10K_TLV_ATTR_LAST
};

/**
 *  fm10k_msg_err_pf - Message handler for error reply
 *  @hw: Pointer to hardware structure
 *  @results: pointer array containing parsed data
 *  @mbx: Pointer to mailbox information structure
 *
 *  This handler will capture the data for any error replies to previous
 *  messages that the PF has sent.
 **/
s32 fm10k_msg_err_pf(struct fm10k_hw *hw, u32 **results,
		     struct fm10k_mbx_info *mbx)
{
	struct fm10k_swapi_error err_msg;
	s32 err;

	UNREFERENCED_1PARAMETER(mbx);
	DEBUGFUNC("fm10k_msg_err_pf");

	/* extract structure from message */
	err = fm10k_tlv_attr_get_le_struct(results[FM10K_PF_ATTR_ID_ERR],
					   &err_msg, sizeof(err_msg));
	if (err)
		return err;

	/* record table status */
	fm10k_record_global_table_data(&err_msg.mac, &hw->swapi.mac);
	fm10k_record_global_table_data(&err_msg.nexthop, &hw->swapi.nexthop);
	fm10k_record_global_table_data(&err_msg.ffu, &hw->swapi.ffu);

	/* record SW API status value */
	hw->swapi.status = FM10K_LE32_TO_CPU(err_msg.status);

	return FM10K_SUCCESS;
}

/* currently there is no shared 1588 timestamp handler */

const struct fm10k_tlv_attr fm10k_1588_timestamp_msg_attr[] = {
	FM10K_TLV_ATTR_LE_STRUCT(FM10K_PF_ATTR_ID_1588_TIMESTAMP,
				 sizeof(struct fm10k_swapi_1588_timestamp)),
	FM10K_TLV_ATTR_LAST
};

const struct fm10k_tlv_attr fm10k_1588_clock_owner_attr[] = {
	FM10K_TLV_ATTR_LE_STRUCT(FM10K_PF_ATTR_ID_1588_CLOCK_OWNER,
				 sizeof(struct fm10k_swapi_1588_clock_owner)),
	FM10K_TLV_ATTR_LAST
};

const struct fm10k_tlv_attr fm10k_master_clk_offset_attr[] = {
	FM10K_TLV_ATTR_U64(FM10K_PF_ATTR_ID_MASTER_CLK_OFFSET),
	FM10K_TLV_ATTR_LAST
};

/**
 *  fm10k_iov_notify_offset_pf - Notify VF of change in PTP offset
 *  @hw: pointer to hardware structure
 *  @vf_info: pointer to the vf info structure
 *  @offset: 64bit unsigned offset from hardware SYSTIME
 *
 *  This function sends a message to a given VF to notify it of PTP offset
 *  changes.
 **/
STATIC void fm10k_iov_notify_offset_pf(struct fm10k_hw *hw,
				       struct fm10k_vf_info *vf_info,
				       u64 offset)
{
	u32 msg[4];

	fm10k_tlv_msg_init(msg, FM10K_VF_MSG_ID_1588);
	fm10k_tlv_attr_put_u64(msg, FM10K_1588_MSG_CLK_OFFSET, offset);

	if (vf_info->mbx.ops.enqueue_tx)
		vf_info->mbx.ops.enqueue_tx(hw, &vf_info->mbx, msg);
}

/**
 *  fm10k_msg_1588_clock_owner_pf - Message handler for clock ownership from SM
 *  @hw: pointer to hardware structure
 *  @results: pointer to array containing parsed data,
 *  @mbx: Pointer to mailbox information structure
 *
 *  This handler configures the FM10K_HW_FLAG_CLOCK_OWNER field for the PF
 */
s32 fm10k_msg_1588_clock_owner_pf(struct fm10k_hw *hw, u32 **results,
				  struct fm10k_mbx_info *mbx)
{
	struct fm10k_swapi_1588_clock_owner msg;
	u16 glort;
	s32 err;

	UNREFERENCED_1PARAMETER(mbx);
	DEBUGFUNC("fm10k_msg_1588_clock_owner");

	err = fm10k_tlv_attr_get_le_struct(
		results[FM10K_PF_ATTR_ID_1588_CLOCK_OWNER],
		&msg, sizeof(msg));
	if (err)
		return err;

	/* We own the clock iff the glort matches us and the enabled field is
	 * true. Otherwise, the clock must belong to some other port.
	 */
	glort = le16_to_cpu(msg.glort);
	if (fm10k_glort_valid_pf(hw, glort) && msg.enabled)
		hw->flags |= FM10K_HW_FLAG_CLOCK_OWNER;
	else
		hw->flags &= ~FM10K_HW_FLAG_CLOCK_OWNER;

	return FM10K_SUCCESS;
}

/**
 *  fm10k_adjust_systime_pf - Adjust systime frequency
 *  @hw: pointer to hardware structure
 *  @ppb: adjustment rate in parts per billion
 *
 *  This function will adjust the SYSTIME_CFG register contained in BAR 4
 *  if this function is supported for BAR 4 access.  The adjustment amount
 *  is based on the parts per billion value provided and adjusted to a
 *  value based on parts per 2^48 clock cycles.
 *
 *  If adjustment is not supported or the requested value is too large
 *  we will return an error.
 **/
STATIC s32 fm10k_adjust_systime_pf(struct fm10k_hw *hw, s32 ppb)
{
	u64 systime_adjust;

	DEBUGFUNC("fm10k_adjust_systime_pf");

	/* ensure that we control the clock */
	if (!(hw->flags & FM10K_HW_FLAG_CLOCK_OWNER))
		return FM10K_ERR_DEVICE_NOT_SUPPORTED;

	/* if sw_addr is not set we don't have switch register access */
	if (!hw->sw_addr)
		return ppb ? FM10K_ERR_PARAM : FM10K_SUCCESS;

	/* we must convert the value from parts per billion to parts per
	 * 2^48 cycles.  In addition I have opted to only use the 30 most
	 * significant bits of the adjustment value as the 8 least
	 * significant bits are located in another register and represent
	 * a value significantly less than a part per billion, the result
	 * of dropping the 8 least significant bits is that the adjustment
	 * value is effectively multiplied by 2^8 when we write it.
	 *
	 * As a result of all this the math for this breaks down as follows:
	 *	ppb / 10^9 == adjust * 2^8 / 2^48
	 * If we solve this for adjust, and simplify it comes out as:
	 *	ppb * 2^31 / 5^9 == adjust
	 */
	systime_adjust = (ppb < 0) ? -ppb : ppb;
	systime_adjust <<= 31;
	do_div(systime_adjust, 1953125);

	/* verify the requested adjustment value is in range */
	if (systime_adjust > FM10K_SW_SYSTIME_ADJUST_MASK)
		return FM10K_ERR_PARAM;

	if (ppb > 0)
		systime_adjust |= FM10K_SW_SYSTIME_ADJUST_DIR_POSITIVE;

	FM10K_WRITE_SW_REG(hw, FM10K_SW_SYSTIME_ADJUST, (u32)systime_adjust);

	return FM10K_SUCCESS;
}

/**
 *  fm10k_notify_offset_pf - Notify switch of change in PTP offset
 *  @hw: pointer to hardware structure
 *  @offset: 64bit unsigned offset of SYSTIME
 *
 *  This function sends a message to the switch to indicate a change in the
 *  offset of the hardware SYSTIME registers. The switch manager is
 *  responsible for transmitting this message to other hosts.
 */
STATIC s32 fm10k_notify_offset_pf(struct fm10k_hw *hw, u64 offset)
{
	struct fm10k_mbx_info *mbx = &hw->mbx;
	u32 msg[4];

	DEBUGFUNC("fm10k_notify_offset_pf");

	/* ensure that we control the clock */
	if (!(hw->flags & FM10K_HW_FLAG_CLOCK_OWNER))
		return FM10K_ERR_DEVICE_NOT_SUPPORTED;

	fm10k_tlv_msg_init(msg, FM10K_PF_MSG_ID_MASTER_CLK_OFFSET);
	fm10k_tlv_attr_put_u64(msg, FM10K_PF_ATTR_ID_MASTER_CLK_OFFSET, offset);

	/* load onto outgoing mailbox */
	return mbx->ops.enqueue_tx(hw, mbx, msg);
}

/**
 *  fm10k_read_systime_pf - Reads value of systime registers
 *  @hw: pointer to the hardware structure
 *
 *  Function reads the content of 2 registers, combined to represent a 64 bit
 *  value measured in nanosecods.  In order to guarantee the value is accurate
 *  we check the 32 most significant bits both before and after reading the
 *  32 least significant bits to verify they didn't change as we were reading
 *  the registers.
 **/
static u64 fm10k_read_systime_pf(struct fm10k_hw *hw)
{
	u32 systime_l, systime_h, systime_tmp;

	systime_h = fm10k_read_reg(hw, FM10K_SYSTIME + 1);

	do {
		systime_tmp = systime_h;
		systime_l = fm10k_read_reg(hw, FM10K_SYSTIME);
		systime_h = fm10k_read_reg(hw, FM10K_SYSTIME + 1);
	} while (systime_tmp != systime_h);

	return ((u64)systime_h << 32) | systime_l;
}

static const struct fm10k_msg_data fm10k_msg_data_pf[] = {
	FM10K_PF_MSG_ERR_HANDLER(XCAST_MODES, fm10k_msg_err_pf),
	FM10K_PF_MSG_ERR_HANDLER(UPDATE_MAC_FWD_RULE, fm10k_msg_err_pf),
	FM10K_PF_MSG_LPORT_MAP_HANDLER(fm10k_msg_lport_map_pf),
	FM10K_PF_MSG_ERR_HANDLER(LPORT_CREATE, fm10k_msg_err_pf),
	FM10K_PF_MSG_ERR_HANDLER(LPORT_DELETE, fm10k_msg_err_pf),
	FM10K_PF_MSG_UPDATE_PVID_HANDLER(fm10k_msg_update_pvid_pf),
	FM10K_PF_MSG_1588_CLOCK_OWNER_HANDLER(fm10k_msg_1588_clock_owner_pf),
	FM10K_TLV_MSG_ERROR_HANDLER(fm10k_tlv_msg_error),
};

/**
 *  fm10k_init_ops_pf - Inits func ptrs and MAC type
 *  @hw: pointer to hardware structure
 *
 *  Initialize the function pointers and assign the MAC type for PF.
 *  Does not touch the hardware.
 **/
s32 fm10k_init_ops_pf(struct fm10k_hw *hw)
{
	struct fm10k_mac_info *mac = &hw->mac;
	struct fm10k_iov_info *iov = &hw->iov;

	DEBUGFUNC("fm10k_init_ops_pf");

	fm10k_init_ops_generic(hw);

	mac->ops.reset_hw = &fm10k_reset_hw_pf;
	mac->ops.init_hw = &fm10k_init_hw_pf;
	mac->ops.start_hw = &fm10k_start_hw_generic;
	mac->ops.stop_hw = &fm10k_stop_hw_generic;
#ifndef NO_IS_SLOT_APPROPRIATE_CHECK
	mac->ops.is_slot_appropriate = &fm10k_is_slot_appropriate_pf;
#endif
	mac->ops.update_vlan = &fm10k_update_vlan_pf;
	mac->ops.read_mac_addr = &fm10k_read_mac_addr_pf;
	mac->ops.update_uc_addr = &fm10k_update_uc_addr_pf;
	mac->ops.update_mc_addr = &fm10k_update_mc_addr_pf;
	mac->ops.update_xcast_mode = &fm10k_update_xcast_mode_pf;
	mac->ops.update_int_moderator = &fm10k_update_int_moderator_pf;
	mac->ops.update_lport_state = &fm10k_update_lport_state_pf;
	mac->ops.update_hw_stats = &fm10k_update_hw_stats_pf;
	mac->ops.rebind_hw_stats = &fm10k_rebind_hw_stats_pf;
	mac->ops.configure_dglort_map = &fm10k_configure_dglort_map_pf;
	mac->ops.set_dma_mask = &fm10k_set_dma_mask_pf;
	mac->ops.get_fault = &fm10k_get_fault_pf;
	mac->ops.get_host_state = &fm10k_get_host_state_pf;
	mac->ops.adjust_systime = &fm10k_adjust_systime_pf;
	mac->ops.notify_offset = &fm10k_notify_offset_pf;
	mac->ops.read_systime = &fm10k_read_systime_pf;

	mac->max_msix_vectors = fm10k_get_pcie_msix_count_generic(hw);

	iov->ops.assign_resources = &fm10k_iov_assign_resources_pf;
	iov->ops.configure_tc = &fm10k_iov_configure_tc_pf;
	iov->ops.assign_int_moderator = &fm10k_iov_assign_int_moderator_pf;
	iov->ops.assign_default_mac_vlan = fm10k_iov_assign_default_mac_vlan_pf;
	iov->ops.reset_resources = &fm10k_iov_reset_resources_pf;
	iov->ops.set_lport = &fm10k_iov_set_lport_pf;
	iov->ops.reset_lport = &fm10k_iov_reset_lport_pf;
	iov->ops.update_stats = &fm10k_iov_update_stats_pf;
	iov->ops.notify_offset = &fm10k_iov_notify_offset_pf;

	return fm10k_sm_mbx_init(hw, &hw->mbx, fm10k_msg_data_pf);
}
