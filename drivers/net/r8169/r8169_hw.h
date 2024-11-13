/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#ifndef R8169_HW_H
#define R8169_HW_H

#include <stdint.h>

#include <bus_pci_driver.h>
#include <rte_ethdev.h>
#include <rte_ethdev_core.h>

#include "r8169_compat.h"
#include "r8169_ethdev.h"
#include "r8169_phy.h"

u16 rtl_mac_ocp_read(struct rtl_hw *hw, u16 addr);
void rtl_mac_ocp_write(struct rtl_hw *hw, u16 addr, u16 value);

u32 rtl_ocp_read(struct rtl_hw *hw, u16 addr, u8 len);
void rtl_ocp_write(struct rtl_hw *hw, u16 addr, u8 len, u32 value);

u32 rtl_csi_read(struct rtl_hw *hw, u32 addr);
void rtl_csi_write(struct rtl_hw *hw, u32 addr, u32 value);

void rtl_hw_config(struct rtl_hw *hw);
void rtl_nic_reset(struct rtl_hw *hw);

void rtl_enable_cfg9346_write(struct rtl_hw *hw);
void rtl_disable_cfg9346_write(struct rtl_hw *hw);

void rtl8125_oob_mutex_lock(struct rtl_hw *hw);
void rtl8125_oob_mutex_unlock(struct rtl_hw *hw);

void rtl_disable_rxdvgate(struct rtl_hw *hw);

#define NO_BASE_ADDRESS 0x00000000

/* Channel wait count */
#define RTL_CHANNEL_WAIT_COUNT      20000
#define RTL_CHANNEL_WAIT_TIME       1   /*  1 us */
#define RTL_CHANNEL_EXIT_DELAY_TIME 20  /* 20 us */

#endif /* R8169_HW_H */
