/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#ifndef _IXGBE_BYPASS_H_
#define _IXGBE_BYPASS_H_

#ifdef RTE_NIC_BYPASS

struct ixgbe_bypass_mac_ops {
	s32 (*bypass_rw) (struct ixgbe_hw *hw, u32 cmd, u32 *status);
	bool (*bypass_valid_rd) (u32 in_reg, u32 out_reg);
	s32 (*bypass_set) (struct ixgbe_hw *hw, u32 cmd, u32 event, u32 action);
	s32 (*bypass_rd_eep) (struct ixgbe_hw *hw, u32 addr, u8 *value);
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

#endif /* RTE_NIC_BYPASS */

#endif /*  _IXGBE_BYPASS_H_ */
