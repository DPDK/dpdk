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

int rtl_set_hw_ops(struct rtl_hw *hw);

void rtl_hw_disable_mac_mcu_bps(struct rtl_hw *hw);

void rtl_write_mac_mcu_ram_code(struct rtl_hw *hw, const u16 *entry,
				u16 entry_cnt);

extern const struct rtl_hw_ops rtl8125a_ops;
extern const struct rtl_hw_ops rtl8125b_ops;
extern const struct rtl_hw_ops rtl8125bp_ops;
extern const struct rtl_hw_ops rtl8125d_ops;
extern const struct rtl_hw_ops rtl8126a_ops;

#define NO_BASE_ADDRESS 0x00000000

/* Channel wait count */
#define RTL_CHANNEL_WAIT_COUNT      20000
#define RTL_CHANNEL_WAIT_TIME       1   /*  1 us */
#define RTL_CHANNEL_EXIT_DELAY_TIME 20  /* 20 us */

#define ARRAY_SIZE(arr) RTE_DIM(arr)

#define HW_SUPPORT_MAC_MCU(_M)            ((_M)->HwSuppMacMcuVer > 0)
#define HW_HAS_WRITE_PHY_MCU_RAM_CODE(_M) ((_M)->HwHasWrRamCodeToMicroP ? 1 : 0)

#endif /* R8169_HW_H */
