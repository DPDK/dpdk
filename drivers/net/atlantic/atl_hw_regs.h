/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/* Copyright (C) 2014-2017 aQuantia Corporation. */

/* File aq_hw_utils.h: Declaration of helper functions used across hardware
 * layer.
 */

#ifndef AQ_HW_UTILS_H
#define AQ_HW_UTILS_H

#include <errno.h>
#include <rte_common.h>
#include <rte_io.h>
#include <rte_byteorder.h>
#include <rte_random.h>
#include <rte_cycles.h>
#include "atl_common.h"
#include "atl_types.h"


#ifndef HIDWORD
#define LODWORD(_qw)    ((u32)(_qw))
#define HIDWORD(_qw)    ((u32)(((_qw) >> 32) & 0xffffffff))
#endif

#define AQ_HW_SLEEP(_US_) rte_delay_ms(_US_)

#define mdelay rte_delay_ms
#define udelay rte_delay_us
#define ARRAY_SIZE(arr) RTE_DIM(arr)
#define BIT(x)	(1UL << (x))

#define AQ_HW_WAIT_FOR(_B_, _US_, _N_) \
do { \
	unsigned int AQ_HW_WAIT_FOR_i; \
	for (AQ_HW_WAIT_FOR_i = _N_; (!(_B_)) && (AQ_HW_WAIT_FOR_i);\
	--AQ_HW_WAIT_FOR_i) {\
		udelay(_US_); \
	} \
	if (!AQ_HW_WAIT_FOR_i) {\
		err = -ETIMEDOUT; \
	} \
} while (0)

#define ATL_WRITE_FLUSH(aq_hw) { (void)aq_hw_read_reg(aq_hw, 0x10); }

void aq_hw_write_reg_bit(struct aq_hw_s *aq_hw, u32 addr, u32 msk,
			 u32 shift, u32 val);
u32 aq_hw_read_reg_bit(struct aq_hw_s *aq_hw, u32 addr, u32 msk, u32 shift);
u32 aq_hw_read_reg(struct aq_hw_s *hw, u32 reg);
void aq_hw_write_reg(struct aq_hw_s *hw, u32 reg, u32 value);
int aq_hw_err_from_flags(struct aq_hw_s *hw);

#endif /* AQ_HW_UTILS_H */
