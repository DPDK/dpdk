/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _IFPGA_ENUMERATE_H_
#define _IFPGA_ENUMERATE_H_

#define FME_PORT_OFST_BAR_SKIP  7

int ifpga_bus_init(struct ifpga_hw *hw);
int ifpga_bus_uinit(struct ifpga_hw *hw);
int ifpga_bus_enumerate(struct ifpga_hw *hw);

#endif /* _IFPGA_ENUMERATE_H_ */
