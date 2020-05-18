/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Intel Corporation
 */

#include "igc_api.h"

/*
 * NOTE: the following routines using the igc
 * naming style are provided to the shared
 * code but are OS specific
 */

void
igc_write_pci_cfg(struct igc_hw *hw, u32 reg, u16 *value)
{
	(void)hw;
	(void)reg;
	(void)value;
}

void
igc_read_pci_cfg(struct igc_hw *hw, u32 reg, u16 *value)
{
	(void)hw;
	(void)reg;
	*value = 0;
}

void
igc_pci_set_mwi(struct igc_hw *hw)
{
	(void)hw;
}

void
igc_pci_clear_mwi(struct igc_hw *hw)
{
	(void)hw;
}

/*
 * Read the PCI Express capabilities
 */
int32_t
igc_read_pcie_cap_reg(struct igc_hw *hw, u32 reg, u16 *value)
{
	(void)hw;
	(void)reg;
	(void)value;
	return IGC_NOT_IMPLEMENTED;
}

/*
 * Write the PCI Express capabilities
 */
int32_t
igc_write_pcie_cap_reg(struct igc_hw *hw, u32 reg, u16 *value)
{
	(void)hw;
	(void)reg;
	(void)value;

	return IGC_NOT_IMPLEMENTED;
}
