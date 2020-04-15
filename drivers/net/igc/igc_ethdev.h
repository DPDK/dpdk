/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Intel Corporation
 */

#ifndef _IGC_ETHDEV_H_
#define _IGC_ETHDEV_H_

#include <rte_ethdev.h>

#include "base/igc_osdep.h"
#include "base/igc_hw.h"
#include "base/igc_i225.h"
#include "base/igc_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IGC_QUEUE_PAIRS_NUM		4

/* structure for interrupt relative data */
struct igc_interrupt {
	uint32_t flags;
	uint32_t mask;
};

/*
 * Structure to store private data for each driver instance (for each port).
 */
struct igc_adapter {
	struct igc_hw		hw;
	struct igc_interrupt	intr;
	bool		stopped;
};

#define IGC_DEV_PRIVATE(_dev)	((_dev)->data->dev_private)

#define IGC_DEV_PRIVATE_HW(_dev) \
	(&((struct igc_adapter *)(_dev)->data->dev_private)->hw)

#define IGC_DEV_PRIVATE_INTR(_dev) \
	(&((struct igc_adapter *)(_dev)->data->dev_private)->intr)

static inline void
igc_read_reg_check_set_bits(struct igc_hw *hw, uint32_t reg, uint32_t bits)
{
	uint32_t reg_val = IGC_READ_REG(hw, reg);

	bits |= reg_val;
	if (bits == reg_val)
		return;	/* no need to write back */

	IGC_WRITE_REG(hw, reg, bits);
}

static inline void
igc_read_reg_check_clear_bits(struct igc_hw *hw, uint32_t reg, uint32_t bits)
{
	uint32_t reg_val = IGC_READ_REG(hw, reg);

	bits = reg_val & ~bits;
	if (bits == reg_val)
		return;	/* no need to write back */

	IGC_WRITE_REG(hw, reg, bits);
}

#ifdef __cplusplus
}
#endif

#endif /* _IGC_ETHDEV_H_ */
