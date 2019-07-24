/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001 - 2015 Intel Corporation
 */
/*$FreeBSD$*/

#include "e1000_api.h"

/*
 * NOTE: the following routines using the e1000 
 * 	naming style are provided to the shared
 *	code but are OS specific
 */

void
e1000_write_pci_cfg(struct e1000_hw *hw, u32 reg, u16 *value)
{
	return;
}

void
e1000_read_pci_cfg(struct e1000_hw *hw, u32 reg, u16 *value)
{
	*value = 0;
	return;
}

void
e1000_pci_set_mwi(struct e1000_hw *hw)
{
}

void
e1000_pci_clear_mwi(struct e1000_hw *hw)
{
}


/*
 * Read the PCI Express capabilities
 */
int32_t
e1000_read_pcie_cap_reg(struct e1000_hw *hw, u32 reg, u16 *value)
{
	return E1000_NOT_IMPLEMENTED;
}

/*
 * Write the PCI Express capabilities
 */
int32_t
e1000_write_pcie_cap_reg(struct e1000_hw *hw, u32 reg, u16 *value)
{
	return E1000_NOT_IMPLEMENTED;
}
