/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2013 - 2015 Intel Corporation
 */

#include "fm10k_api.h"
#include "fm10k_common.h"

/**
 *  fm10k_set_mac_type - Sets MAC type
 *  @hw: pointer to the HW structure
 *
 *  This function sets the mac type of the adapter based on the
 *  vendor ID and device ID stored in the hw structure.
 **/
s32 fm10k_set_mac_type(struct fm10k_hw *hw)
{
	s32 ret_val = FM10K_SUCCESS;

	DEBUGFUNC("fm10k_set_mac_type");

	if (hw->vendor_id != FM10K_INTEL_VENDOR_ID) {
		ERROR_REPORT2(FM10K_ERROR_UNSUPPORTED,
			     "Unsupported vendor id: %x\n", hw->vendor_id);
		return FM10K_ERR_DEVICE_NOT_SUPPORTED;
	}

	switch (hw->device_id) {
	case FM10K_DEV_ID_PF:
#ifdef BOULDER_RAPIDS_HW
	case FM10K_DEV_ID_SDI_FM10420_QDA2:
#endif /* BOULDER_RAPIDS_HW */
#ifdef ATWOOD_CHANNEL_HW
	case FM10K_DEV_ID_SDI_FM10420_DA2:
#endif /* ATWOOD_CHANNEL_HW */
		hw->mac.type = fm10k_mac_pf;
		break;
	case FM10K_DEV_ID_VF:
		hw->mac.type = fm10k_mac_vf;
		break;
	default:
		ret_val = FM10K_ERR_DEVICE_NOT_SUPPORTED;
		ERROR_REPORT2(FM10K_ERROR_UNSUPPORTED,
			     "Unsupported device id: %x\n",
			     hw->device_id);
		break;
	}

	DEBUGOUT2("fm10k_set_mac_type found mac: %d, returns: %d\n",
		  hw->mac.type, ret_val);

	return ret_val;
}

/**
 *  fm10k_init_shared_code - Initialize the shared code
 *  @hw: pointer to hardware structure
 *
 *  This will assign function pointers and assign the MAC type and PHY code.
 *  Does not touch the hardware. This function must be called prior to any
 *  other function in the shared code. The fm10k_hw structure should be
 *  memset to 0 prior to calling this function.  The following fields in
 *  hw structure should be filled in prior to calling this function:
 *  hw_addr, back, device_id, vendor_id, subsystem_device_id,
 *  subsystem_vendor_id, and revision_id
 **/
s32 fm10k_init_shared_code(struct fm10k_hw *hw)
{
	s32 status;

	DEBUGFUNC("fm10k_init_shared_code");

	/* Set the mac type */
	fm10k_set_mac_type(hw);

	switch (hw->mac.type) {
	case fm10k_mac_pf:
		status = fm10k_init_ops_pf(hw);
		break;
	case fm10k_mac_vf:
		status = fm10k_init_ops_vf(hw);
		break;
	default:
		status = FM10K_ERR_DEVICE_NOT_SUPPORTED;
		break;
	}

	return status;
}

#define fm10k_call_func(hw, func, params, error) \
		 ((func) ? (func params) : (error))

/**
 *  fm10k_reset_hw - Reset the hardware to known good state
 *  @hw: pointer to hardware structure
 *
 *  This function should return the hardware to a state similar to the
 *  one it is in after being powered on.
 **/
s32 fm10k_reset_hw(struct fm10k_hw *hw)
{
	return fm10k_call_func(hw, hw->mac.ops.reset_hw, (hw),
			       FM10K_NOT_IMPLEMENTED);
}

/**
 *  fm10k_init_hw - Initialize the hardware
 *  @hw: pointer to hardware structure
 *
 *  Initialize the hardware by resetting and then starting the hardware
 **/
s32 fm10k_init_hw(struct fm10k_hw *hw)
{
	return fm10k_call_func(hw, hw->mac.ops.init_hw, (hw),
			       FM10K_NOT_IMPLEMENTED);
}

/**
 *  fm10k_stop_hw - Prepares hardware to shutdown Rx/Tx
 *  @hw: pointer to hardware structure
 *
 *  Disables Rx/Tx queues and disables the DMA engine.
 **/
s32 fm10k_stop_hw(struct fm10k_hw *hw)
{
	return fm10k_call_func(hw, hw->mac.ops.stop_hw, (hw),
			       FM10K_NOT_IMPLEMENTED);
}

/**
 *  fm10k_start_hw - Prepares hardware for Rx/Tx
 *  @hw: pointer to hardware structure
 *
 *  This function sets the flags indicating that the hardware is ready to
 *  begin operation.
 **/
s32 fm10k_start_hw(struct fm10k_hw *hw)
{
	return fm10k_call_func(hw, hw->mac.ops.start_hw, (hw),
			       FM10K_NOT_IMPLEMENTED);
}

/**
 *  fm10k_get_bus_info - Set PCI bus info
 *  @hw: pointer to hardware structure
 *
 *  Sets the PCI bus info (speed, width, type) within the fm10k_hw structure
 **/
s32 fm10k_get_bus_info(struct fm10k_hw *hw)
{
	return fm10k_call_func(hw, hw->mac.ops.get_bus_info, (hw),
			       FM10K_NOT_IMPLEMENTED);
}

#ifndef NO_IS_SLOT_APPROPRIATE_CHECK
/**
 *  fm10k_is_slot_appropriate - Indicate appropriate slot for this SKU
 *  @hw: pointer to hardware structure
 *
 *  Looks at the PCIe bus info to confirm whether or not this slot can support
 *  the necessary bandwidth for this device.
 **/
bool fm10k_is_slot_appropriate(struct fm10k_hw *hw)
{
	if (hw->mac.ops.is_slot_appropriate)
		return hw->mac.ops.is_slot_appropriate(hw);
	return true;
}

#endif
/**
 *  fm10k_update_vlan - Clear VLAN ID to VLAN filter table
 *  @hw: pointer to hardware structure
 *  @vid: VLAN ID to add to table
 *  @idx: Index indicating VF ID or PF ID in table
 *  @set: Indicates if this is a set or clear operation
 *
 *  This function adds or removes the corresponding VLAN ID from the VLAN
 *  filter table for the corresponding function.
 **/
s32 fm10k_update_vlan(struct fm10k_hw *hw, u32 vid, u8 idx, bool set)
{
	return fm10k_call_func(hw, hw->mac.ops.update_vlan, (hw, vid, idx, set),
			       FM10K_NOT_IMPLEMENTED);
}

/**
 *  fm10k_read_mac_addr - Reads MAC address
 *  @hw: pointer to hardware structure
 *
 *  Reads the MAC address out of the interface and stores it in the HW
 *  structures.
 **/
s32 fm10k_read_mac_addr(struct fm10k_hw *hw)
{
	return fm10k_call_func(hw, hw->mac.ops.read_mac_addr, (hw),
			       FM10K_NOT_IMPLEMENTED);
}

/**
 *  fm10k_update_hw_stats - Update hw statistics
 *  @hw: pointer to hardware structure
 *
 *  This function updates statistics that are related to hardware.
 * */
void fm10k_update_hw_stats(struct fm10k_hw *hw, struct fm10k_hw_stats *stats)
{
	switch (hw->mac.type) {
	case fm10k_mac_pf:
		return fm10k_update_hw_stats_pf(hw, stats);
	case fm10k_mac_vf:
		return fm10k_update_hw_stats_vf(hw, stats);
	default:
		break;
	}
}

/**
 *  fm10k_rebind_hw_stats - Reset base for hw statistics
 *  @hw: pointer to hardware structure
 *
 *  This function resets the base for statistics that are related to hardware.
 * */
void fm10k_rebind_hw_stats(struct fm10k_hw *hw, struct fm10k_hw_stats *stats)
{
	switch (hw->mac.type) {
	case fm10k_mac_pf:
		return fm10k_rebind_hw_stats_pf(hw, stats);
	case fm10k_mac_vf:
		return fm10k_rebind_hw_stats_vf(hw, stats);
	default:
		break;
	}
}

/**
 *  fm10k_configure_dglort_map - Configures GLORT entry and queues
 *  @hw: pointer to hardware structure
 *  @dglort: pointer to dglort configuration structure
 *
 *  Reads the configuration structure contained in dglort_cfg and uses
 *  that information to then populate a DGLORTMAP/DEC entry and the queues
 *  to which it has been assigned.
 **/
s32 fm10k_configure_dglort_map(struct fm10k_hw *hw,
			       struct fm10k_dglort_cfg *dglort)
{
	return fm10k_call_func(hw, hw->mac.ops.configure_dglort_map,
			       (hw, dglort), FM10K_NOT_IMPLEMENTED);
}

/**
 *  fm10k_set_dma_mask - Configures PhyAddrSpace to limit DMA to system
 *  @hw: pointer to hardware structure
 *  @dma_mask: 64 bit DMA mask required for platform
 *
 *  This function configures the endpoint to limit the access to memory
 *  beyond what is physically in the system.
 **/
void fm10k_set_dma_mask(struct fm10k_hw *hw, u64 dma_mask)
{
	if (hw->mac.ops.set_dma_mask)
		hw->mac.ops.set_dma_mask(hw, dma_mask);
}

/**
 *  fm10k_get_fault - Record a fault in one of the interface units
 *  @hw: pointer to hardware structure
 *  @type: pointer to fault type register offset
 *  @fault: pointer to memory location to record the fault
 *
 *  Record the fault register contents to the fault data structure and
 *  clear the entry from the register.
 *
 *  Returns ERR_PARAM if invalid register is specified or no error is present.
 **/
s32 fm10k_get_fault(struct fm10k_hw *hw, int type, struct fm10k_fault *fault)
{
	return fm10k_call_func(hw, hw->mac.ops.get_fault, (hw, type, fault),
			       FM10K_NOT_IMPLEMENTED);
}

/**
 *  fm10k_update_uc_addr - Update device unicast address
 *  @hw: pointer to the HW structure
 *  @lport: logical port ID to update - unused
 *  @mac: MAC address to add/remove from table
 *  @vid: VLAN ID to add/remove from table
 *  @add: Indicates if this is an add or remove operation
 *  @flags: flags field to indicate add and secure - unused
 *
 *  This function is used to add or remove unicast MAC addresses
 **/
s32 fm10k_update_uc_addr(struct fm10k_hw *hw, u16 lport,
			  const u8 *mac, u16 vid, bool add, u8 flags)
{
	return fm10k_call_func(hw, hw->mac.ops.update_uc_addr,
			       (hw, lport, mac, vid, add, flags),
			       FM10K_NOT_IMPLEMENTED);
}

/**
 *  fm10k_update_mc_addr - Update device multicast address
 *  @hw: pointer to the HW structure
 *  @lport: logical port ID to update - unused
 *  @mac: MAC address to add/remove from table
 *  @vid: VLAN ID to add/remove from table
 *  @add: Indicates if this is an add or remove operation
 *
 *  This function is used to add or remove multicast MAC addresses
 **/
s32 fm10k_update_mc_addr(struct fm10k_hw *hw, u16 lport,
			 const u8 *mac, u16 vid, bool add)
{
	return fm10k_call_func(hw, hw->mac.ops.update_mc_addr,
			       (hw, lport, mac, vid, add),
			       FM10K_NOT_IMPLEMENTED);
}

/**
 *  fm10k_adjust_systime - Adjust systime frequency
 *  @hw: pointer to hardware structure
 *  @ppb: adjustment rate in parts per billion
 *
 *  This function is meant to update the frequency of the clock represented
 *  by the SYSTIME register.
 **/
s32 fm10k_adjust_systime(struct fm10k_hw *hw, s32 ppb)
{
	return fm10k_call_func(hw, hw->mac.ops.adjust_systime,
			       (hw, ppb), FM10K_NOT_IMPLEMENTED);
}

/**
 *  fm10k_notify_offset - Notify switch of change in PTP offset
 *  @hw: pointer to hardware structure
 *  @offset: 64bit unsigned offset from hardware SYSTIME value
 *
 *  This function is meant to notify switch of change in the PTP offset for
 *  the hardware SYSTIME registers.
 **/
s32 fm10k_notify_offset(struct fm10k_hw *hw, u64 offset)
{
	return fm10k_call_func(hw, hw->mac.ops.notify_offset,
			       (hw, offset), FM10K_NOT_IMPLEMENTED);
}
