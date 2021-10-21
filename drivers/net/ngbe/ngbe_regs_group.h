/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _NGBE_REGS_GROUP_H_
#define _NGBE_REGS_GROUP_H_

#include "ngbe_ethdev.h"

struct ngbe_hw;
struct reg_info {
	uint32_t base_addr;
	uint32_t count;
	uint32_t stride;
	const char *name;
};

static inline int
ngbe_read_regs(struct ngbe_hw *hw, const struct reg_info *reg,
	uint32_t *reg_buf)
{
	unsigned int i;

	for (i = 0; i < reg->count; i++)
		reg_buf[i] = rd32(hw, reg->base_addr + i * reg->stride);
	return reg->count;
};

static inline int
ngbe_regs_group_count(const struct reg_info *regs)
{
	int count = 0;
	int i = 0;

	while (regs[i].count)
		count += regs[i++].count;
	return count;
};

static inline int
ngbe_read_regs_group(struct rte_eth_dev *dev, uint32_t *reg_buf,
					  const struct reg_info *regs)
{
	int count = 0;
	int i = 0;
	struct ngbe_hw *hw = ngbe_dev_hw(dev);

	while (regs[i].count)
		count += ngbe_read_regs(hw, &regs[i++], &reg_buf[count]);
	return count;
};

#endif /* _NGBE_REGS_GROUP_H_ */
