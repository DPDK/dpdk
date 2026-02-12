/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _IXGBE_BYPASS_H_
#define _IXGBE_BYPASS_H_

#ifdef RTE_LIBRTE_IXGBE_BYPASS

struct ixgbe_bypass_mac_ops {
	s32 (*bypass_rw)(struct ixgbe_hw *hw, u32 cmd, u32 *status);
	bool (*bypass_valid_rd)(u32 in_reg, u32 out_reg);
	s32 (*bypass_set)(struct ixgbe_hw *hw, u32 cmd, u32 event, u32 action);
	s32 (*bypass_rd_eep)(struct ixgbe_hw *hw, u32 addr, u8 *value);
};

struct ixgbe_bypass_info {
	uint64_t reset_tm;
	struct ixgbe_bypass_mac_ops ops;
};

struct rte_eth_dev;

void ixgbe_bypass_init(struct rte_eth_dev *dev);
s32 ixgbe_bypass_state_show(struct rte_eth_dev *dev, u32 *state);
s32 ixgbe_bypass_state_store(struct rte_eth_dev *dev, u32 *new_state);
s32 ixgbe_bypass_event_show(struct rte_eth_dev *dev, u32 event, u32 *state);
s32 ixgbe_bypass_event_store(struct rte_eth_dev *dev, u32 event, u32 state);
s32 ixgbe_bypass_wd_timeout_store(struct rte_eth_dev *dev, u32 timeout);
s32 ixgbe_bypass_ver_show(struct rte_eth_dev *dev, u32 *ver);
s32 ixgbe_bypass_wd_timeout_show(struct rte_eth_dev *dev, u32 *wd_timeout);
s32 ixgbe_bypass_wd_reset(struct rte_eth_dev *dev);

s32 ixgbe_bypass_init_shared_code(struct ixgbe_hw *hw);
s32 ixgbe_bypass_init_hw(struct ixgbe_hw *hw);

#endif /* RTE_LIBRTE_IXGBE_BYPASS */

#endif /*  _IXGBE_BYPASS_H_ */
