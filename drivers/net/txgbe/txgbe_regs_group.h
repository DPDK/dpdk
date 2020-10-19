/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#ifndef _TXGBE_REGS_GROUP_H_
#define _TXGBE_REGS_GROUP_H_

#include "txgbe_ethdev.h"

struct txgbe_hw;
struct reg_info {
	uint32_t base_addr;
	uint32_t count;
	uint32_t stride;
	const char *name;
};

static inline int
txgbe_read_regs(struct txgbe_hw *hw, const struct reg_info *reg,
	uint32_t *reg_buf)
{
	unsigned int i;

	for (i = 0; i < reg->count; i++)
		reg_buf[i] = rd32(hw,
					reg->base_addr + i * reg->stride);
	return reg->count;
};

static inline int
txgbe_regs_group_count(const struct reg_info *regs)
{
	int count = 0;
	int i = 0;

	while (regs[i].count)
		count += regs[i++].count;
	return count;
};

static inline int
txgbe_read_regs_group(struct rte_eth_dev *dev, uint32_t *reg_buf,
					  const struct reg_info *regs)
{
	int count = 0;
	int i = 0;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	while (regs[i].count)
		count += txgbe_read_regs(hw, &regs[i++], &reg_buf[count]);
	return count;
};

#endif /* _TXGBE_REGS_GROUP_H_ */
