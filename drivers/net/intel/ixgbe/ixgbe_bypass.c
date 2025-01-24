/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <time.h>
#include <ethdev_driver.h>
#include "ixgbe_ethdev.h"
#include "ixgbe_bypass_api.h"
#include "rte_pmd_ixgbe.h"

#define	BYPASS_STATUS_OFF_MASK	3

/* Macros to check for invalid function pointers. */
#define	FUNC_PTR_OR_ERR_RET(func, retval) do {              \
	if ((func) == NULL) {                               \
		PMD_DRV_LOG(ERR, "%s:%d function not supported", \
			    __func__, __LINE__);            \
		return retval;                            \
	}                                                   \
} while (0)

#define	FUNC_PTR_OR_RET(func) do {                          \
	if ((func) == NULL) {                               \
		PMD_DRV_LOG(ERR, "%s:%d function not supported", \
			    __func__, __LINE__);            \
		return;                                     \
	}                                                   \
} while (0)


/**
 *  ixgbe_bypass_set_time - Set bypass FW time epoc.
 *
 *  @hw: pointer to hardware structure
 *
 *  This function with sync the FW date stamp with that of the
 *  system clock.
 **/
static void
ixgbe_bypass_set_time(struct ixgbe_adapter *adapter)
{
	u32 mask, value;
	u32 sec;
	struct ixgbe_hw *hw = &adapter->hw;

	sec = 0;

	/*
	 * Send the FW our current time and turn on time_valid and
	 * timer_reset bits.
	 */
	mask = BYPASS_CTL1_TIME_M |
	       BYPASS_CTL1_VALID_M |
	       BYPASS_CTL1_OFFTRST_M;
	value = (sec & BYPASS_CTL1_TIME_M) |
		BYPASS_CTL1_VALID |
		BYPASS_CTL1_OFFTRST;

	FUNC_PTR_OR_RET(adapter->bps.ops.bypass_set);

	/* Store FW reset time (in seconds from epoch). */
	adapter->bps.reset_tm = time(NULL);

	/* reset FW timer. */
	adapter->bps.ops.bypass_set(hw, BYPASS_PAGE_CTL1, mask, value);
}

/**
 * ixgbe_bypass_init - Make some environment changes for bypass
 *
 * @adapter: pointer to ixgbe_adapter structure for access to state bits
 *
 * This function collects all the modifications needed by the bypass
 * driver.
 **/
void
ixgbe_bypass_init(struct rte_eth_dev *dev)
{
	struct ixgbe_adapter *adapter;
	struct ixgbe_hw *hw;

	adapter = IXGBE_DEV_TO_ADPATER(dev);
	hw = &adapter->hw;

	/* Only allow BYPASS ops on the first port */
	if (hw->device_id != IXGBE_DEV_ID_82599_BYPASS ||
			hw->bus.func != 0) {
		PMD_DRV_LOG(ERR, "bypass function is not supported on that device");
		return;
	}

	/* set bypass ops. */
	adapter->bps.ops.bypass_rw = &ixgbe_bypass_rw_generic;
	adapter->bps.ops.bypass_valid_rd = &ixgbe_bypass_valid_rd_generic;
	adapter->bps.ops.bypass_set = &ixgbe_bypass_set_generic;
	adapter->bps.ops.bypass_rd_eep = &ixgbe_bypass_rd_eep_generic;

	/* set the time for logging. */
	ixgbe_bypass_set_time(adapter);

	/* Don't have the SDP to the laser */
	hw->mac.ops.disable_tx_laser = NULL;
	hw->mac.ops.enable_tx_laser = NULL;
	hw->mac.ops.flap_tx_laser = NULL;
}

s32
ixgbe_bypass_state_show(struct rte_eth_dev *dev, u32 *state)
{
	struct ixgbe_hw *hw;
	s32 ret_val;
	u32 cmd;
	u32 by_ctl = 0;
	struct ixgbe_adapter *adapter = IXGBE_DEV_TO_ADPATER(dev);

	hw = &adapter->hw;
	FUNC_PTR_OR_ERR_RET(adapter->bps.ops.bypass_rw, -ENOTSUP);

	cmd = BYPASS_PAGE_CTL0;
	ret_val = adapter->bps.ops.bypass_rw(hw, cmd, &by_ctl);

	/* Assume bypass_rw didn't error out, if it did state will
	 * be ignored anyway.
	 */
	*state = (by_ctl >> BYPASS_STATUS_OFF_SHIFT) &  BYPASS_STATUS_OFF_MASK;

	return ret_val;
}


s32
ixgbe_bypass_state_store(struct rte_eth_dev *dev, u32 *new_state)
{
	struct ixgbe_adapter *adapter = IXGBE_DEV_TO_ADPATER(dev);
	struct ixgbe_hw *hw;
	s32 ret_val;

	hw = &adapter->hw;
	FUNC_PTR_OR_ERR_RET(adapter->bps.ops.bypass_set, -ENOTSUP);

	/* Set the new state */
	ret_val = adapter->bps.ops.bypass_set(hw, BYPASS_PAGE_CTL0,
					 BYPASS_MODE_OFF_M, *new_state);
	if (ret_val)
		goto exit;

	/* Set AUTO back on so FW can receive events */
	ret_val = adapter->bps.ops.bypass_set(hw, BYPASS_PAGE_CTL0,
					 BYPASS_MODE_OFF_M, BYPASS_AUTO);

exit:
	return ret_val;

}

s32
ixgbe_bypass_event_show(struct rte_eth_dev *dev, u32 event,
			    u32 *state)
{
	struct ixgbe_hw *hw;
	s32 ret_val;
	u32 shift;
	u32 cmd;
	u32 by_ctl = 0;
	struct ixgbe_adapter *adapter = IXGBE_DEV_TO_ADPATER(dev);

	hw = &adapter->hw;
	FUNC_PTR_OR_ERR_RET(adapter->bps.ops.bypass_rw, -ENOTSUP);

	cmd = BYPASS_PAGE_CTL0;
	ret_val = adapter->bps.ops.bypass_rw(hw, cmd, &by_ctl);

	/* Assume bypass_rw didn't error out, if it did event will
	 * be ignored anyway.
	 */
	switch (event) {
	case BYPASS_EVENT_WDT_TO:
		shift = BYPASS_WDTIMEOUT_SHIFT;
		break;
	case BYPASS_EVENT_MAIN_ON:
		shift = BYPASS_MAIN_ON_SHIFT;
		break;
	case BYPASS_EVENT_MAIN_OFF:
		shift = BYPASS_MAIN_OFF_SHIFT;
		break;
	case BYPASS_EVENT_AUX_ON:
		shift = BYPASS_AUX_ON_SHIFT;
		break;
	case BYPASS_EVENT_AUX_OFF:
		shift = BYPASS_AUX_OFF_SHIFT;
		break;
	default:
		return EINVAL;
	}

	*state = (by_ctl >> shift) & 0x3;

	return ret_val;
}

s32
ixgbe_bypass_event_store(struct rte_eth_dev *dev, u32 event,
			     u32 state)
{
	struct ixgbe_hw *hw;
	u32 status;
	u32 off;
	s32 ret_val;
	struct ixgbe_adapter *adapter = IXGBE_DEV_TO_ADPATER(dev);

	hw = &adapter->hw;
	FUNC_PTR_OR_ERR_RET(adapter->bps.ops.bypass_set, -ENOTSUP);

	switch (event) {
	case BYPASS_EVENT_WDT_TO:
		off = BYPASS_WDTIMEOUT_M;
		status = state << BYPASS_WDTIMEOUT_SHIFT;
		break;
	case BYPASS_EVENT_MAIN_ON:
		off = BYPASS_MAIN_ON_M;
		status = state << BYPASS_MAIN_ON_SHIFT;
		break;
	case BYPASS_EVENT_MAIN_OFF:
		off = BYPASS_MAIN_OFF_M;
		status = state << BYPASS_MAIN_OFF_SHIFT;
		break;
	case BYPASS_EVENT_AUX_ON:
		off = BYPASS_AUX_ON_M;
		status = state << BYPASS_AUX_ON_SHIFT;
		break;
	case BYPASS_EVENT_AUX_OFF:
		off = BYPASS_AUX_OFF_M;
		status = state << BYPASS_AUX_OFF_SHIFT;
		break;
	default:
		return EINVAL;
	}

	ret_val = adapter->bps.ops.bypass_set(hw, BYPASS_PAGE_CTL0,
		off, status);

	return ret_val;
}

s32
ixgbe_bypass_wd_timeout_store(struct rte_eth_dev *dev, u32 timeout)
{
	struct ixgbe_hw *hw;
	u32 status;
	u32 mask;
	s32 ret_val;
	struct ixgbe_adapter *adapter = IXGBE_DEV_TO_ADPATER(dev);

	hw = &adapter->hw;
	FUNC_PTR_OR_ERR_RET(adapter->bps.ops.bypass_set, -ENOTSUP);

	/* disable the timer with timeout of zero */
	if (timeout == RTE_PMD_IXGBE_BYPASS_TMT_OFF) {
		status = 0x0;   /* WDG enable off */
		mask = BYPASS_WDT_ENABLE_M;
	} else {
		/* set time out value */
		mask = BYPASS_WDT_VALUE_M;

		/* enable the timer */
		status = timeout << BYPASS_WDT_TIME_SHIFT;
		status |= 0x1 << BYPASS_WDT_ENABLE_SHIFT;
		mask |= BYPASS_WDT_ENABLE_M;
	}

	ret_val = adapter->bps.ops.bypass_set(hw, BYPASS_PAGE_CTL0,
		mask, status);

	return ret_val;
}

s32
ixgbe_bypass_ver_show(struct rte_eth_dev *dev, u32 *ver)
{
	struct ixgbe_hw *hw;
	u32 cmd;
	u32 status;
	s32 ret_val;
	struct ixgbe_adapter *adapter = IXGBE_DEV_TO_ADPATER(dev);

	hw = &adapter->hw;
	FUNC_PTR_OR_ERR_RET(adapter->bps.ops.bypass_rw, -ENOTSUP);

	cmd = BYPASS_PAGE_CTL2 | BYPASS_WE;
	cmd |= (BYPASS_EEPROM_VER_ADD << BYPASS_CTL2_OFFSET_SHIFT) &
	       BYPASS_CTL2_OFFSET_M;
	ret_val = adapter->bps.ops.bypass_rw(hw, cmd, &status);
	if (ret_val)
		goto exit;

	/* wait for the write to stick */
	msleep(100);

	/* Now read the results */
	cmd &= ~BYPASS_WE;
	ret_val = adapter->bps.ops.bypass_rw(hw, cmd, &status);
	if (ret_val)
		goto exit;

	*ver = status & BYPASS_CTL2_DATA_M;      /* only one byte of date */

exit:
	return ret_val;
}

s32
ixgbe_bypass_wd_timeout_show(struct rte_eth_dev *dev, u32 *wd_timeout)
{
	struct ixgbe_hw *hw;
	u32 by_ctl = 0;
	u32 cmd;
	u32 wdg;
	s32 ret_val;
	struct ixgbe_adapter *adapter = IXGBE_DEV_TO_ADPATER(dev);

	hw = &adapter->hw;
	FUNC_PTR_OR_ERR_RET(adapter->bps.ops.bypass_rw, -ENOTSUP);

	cmd = BYPASS_PAGE_CTL0;
	ret_val = adapter->bps.ops.bypass_rw(hw, cmd, &by_ctl);

	wdg = by_ctl & BYPASS_WDT_ENABLE_M;
	if (!wdg)
		*wd_timeout = RTE_PMD_IXGBE_BYPASS_TMT_OFF;
	else
		*wd_timeout = (by_ctl >> BYPASS_WDT_TIME_SHIFT) &
			BYPASS_WDT_MASK;

	return ret_val;
}

s32
ixgbe_bypass_wd_reset(struct rte_eth_dev *dev)
{
	u32 cmd;
	u32 status;
	u32 sec;
	u32 count = 0;
	s32 ret_val;
	struct ixgbe_hw *hw;
	struct ixgbe_adapter *adapter = IXGBE_DEV_TO_ADPATER(dev);

	hw = &adapter->hw;

	FUNC_PTR_OR_ERR_RET(adapter->bps.ops.bypass_rw, -ENOTSUP);
	FUNC_PTR_OR_ERR_RET(adapter->bps.ops.bypass_valid_rd, -ENOTSUP);

	/* Use the lower level bit-bang functions since we don't need
	 * to read the register first to get it's current state as we
	 * are setting every thing in this write.
	 */
	/* Set up WD pet */
	cmd = BYPASS_PAGE_CTL1 | BYPASS_WE | BYPASS_CTL1_WDT_PET;

	/* Resync the FW time while writing to CTL1 anyway */
	adapter->bps.reset_tm = time(NULL);
	sec = 0;

	cmd |= (sec & BYPASS_CTL1_TIME_M) | BYPASS_CTL1_VALID;

	/* reset FW timer offset since we are resetting the clock */
	cmd |= BYPASS_CTL1_OFFTRST;

	ret_val = adapter->bps.ops.bypass_rw(hw, cmd, &status);

	/* Read until it matches what we wrote, or we time out */
	do {
		if (count++ > 10) {
			ret_val = IXGBE_BYPASS_FW_WRITE_FAILURE;
			break;
		}

		if (adapter->bps.ops.bypass_rw(hw, BYPASS_PAGE_CTL1, &status)) {
			ret_val = IXGBE_ERR_INVALID_ARGUMENT;
			break;
		}
	} while (!adapter->bps.ops.bypass_valid_rd(cmd, status));

	return ret_val;
}
