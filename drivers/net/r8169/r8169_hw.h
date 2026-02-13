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

void rtl_clear_mac_ocp_bit(struct rtl_hw *hw, u16 addr, u16 mask);
void rtl_set_mac_ocp_bit(struct rtl_hw *hw, u16 addr, u16 mask);

u32 rtl_ocp_read(struct rtl_hw *hw, u16 addr, u8 len);
void rtl_ocp_write(struct rtl_hw *hw, u16 addr, u8 len, u32 value);

u32 rtl_csi_read(struct rtl_hw *hw, u32 addr);
void rtl_csi_write(struct rtl_hw *hw, u32 addr, u32 value);

void rtl_hw_config(struct rtl_hw *hw);
void rtl_nic_reset(struct rtl_hw *hw);

void rtl_enable_cfg9346_write(struct rtl_hw *hw);
void rtl_disable_cfg9346_write(struct rtl_hw *hw);

void rtl_oob_mutex_lock(struct rtl_hw *hw);
void rtl_oob_mutex_unlock(struct rtl_hw *hw);

void rtl_disable_rxdvgate(struct rtl_hw *hw);

int rtl_set_hw_ops(struct rtl_hw *hw);

void rtl_hw_disable_mac_mcu_bps(struct rtl_hw *hw);

void rtl_write_mac_mcu_ram_code(struct rtl_hw *hw, const u16 *entry,
				u16 entry_cnt);

void rtl_hw_initialize(struct rtl_hw *hw);

bool rtl_is_speed_mode_valid(struct rtl_hw *hw, u32 speed);

void rtl_get_mac_version(struct rtl_hw *hw, struct rte_pci_device *pci_dev);
int rtl_get_mac_address(struct rtl_hw *hw, struct rte_ether_addr *ea);

void rtl_rar_set(struct rtl_hw *hw, uint8_t *addr);

void rtl_set_link_option(struct rtl_hw *hw, u8 autoneg, u32 speed, u8 duplex,
			 enum rtl_fc_mode fc);

void rtl_get_tally_stats(struct rtl_hw *hw, struct rte_eth_stats *rte_stats);
void rtl_clear_tally_stats(struct rtl_hw *hw);

int rtl_tally_init(struct rte_eth_dev *dev);
void rtl_tally_free(struct rte_eth_dev *dev);

bool rtl_is_8125(struct rtl_hw *hw);

u32 rtl_eri_read(struct rtl_hw *hw, int addr, int len, int type);
int rtl_eri_write(struct rtl_hw *hw, int addr, int len, u32 value, int type);

u32 rtl_csi_other_fun_read(struct rtl_hw *hw, u8 multi_fun_sel_bit, u32 addr);
void rtl_csi_other_fun_write(struct rtl_hw *hw, u8 multi_fun_sel_bit, u32 addr,
			     u32 value);
void rtl8168_clear_and_set_mcu_ocp_bit(struct rtl_hw *hw, u16 addr,
				       u16 clearmask, u16 setmask);
void rtl8168_clear_mcu_ocp_bit(struct rtl_hw *hw, u16 addr, u16 mask);
void rtl8168_set_mcu_ocp_bit(struct rtl_hw *hw, u16 addr, u16 mask);

u64 rtl_get_hw_mcu_patch_code_ver(struct rtl_hw *hw);
u64 rtl_get_bin_mcu_patch_code_ver(const u16 *entry, u16 entry_cnt);

void rtl8125_store_rss_key(struct rtl_hw *hw);
void rtl8125_config_rss(struct rtl_hw *hw, u16 nb_rx_queues);

extern const struct rtl_hw_ops rtl8168g_ops;
extern const struct rtl_hw_ops rtl8168h_ops;
extern const struct rtl_hw_ops rtl8168ep_ops;
extern const struct rtl_hw_ops rtl8168fp_ops;
extern const struct rtl_hw_ops rtl8168m_ops;
extern const struct rtl_hw_ops rtl8125a_ops;
extern const struct rtl_hw_ops rtl8125b_ops;
extern const struct rtl_hw_ops rtl8125bp_ops;
extern const struct rtl_hw_ops rtl8125d_ops;
extern const struct rtl_hw_ops rtl8126a_ops;
extern const struct rtl_hw_ops rtl8168kb_ops;
extern const struct rtl_hw_ops rtl8127_ops;
extern const struct rtl_hw_ops rtl8125cp_ops;

#define NO_BASE_ADDRESS       0x00000000
#define RTL8168FP_OOBMAC_BASE 0xBAF70000

/* Channel wait count */
#define RTL_CHANNEL_WAIT_COUNT      20000
#define RTL_CHANNEL_WAIT_TIME       1   /*  1 us */
#define RTL_CHANNEL_EXIT_DELAY_TIME 20  /* 20 us */

#define ARRAY_SIZE(arr) RTE_DIM(arr)

#define HW_SUPPORT_MAC_MCU(_M)            ((_M)->HwSuppMacMcuVer > 0)
#define HW_HAS_WRITE_PHY_MCU_RAM_CODE(_M) ((_M)->HwHasWrRamCodeToMicroP ? 1 : 0)

/* Tx NO CLOSE */
#define MAX_TX_NO_CLOSE_DESC_PTR_V2       0x10000
#define MAX_TX_NO_CLOSE_DESC_PTR_MASK_V2  0xFFFF
#define MAX_TX_NO_CLOSE_DESC_PTR_V3       0x100000000
#define MAX_TX_NO_CLOSE_DESC_PTR_MASK_V3  0xFFFFFFFF
#define MAX_TX_NO_CLOSE_DESC_PTR_V4       0x80000000
#define MAX_TX_NO_CLOSE_DESC_PTR_MASK_V4  0x7FFFFFFF
#define TX_NO_CLOSE_SW_PTR_MASK_V2        0x1FFFF

/* Ram code version */
#define NIC_RAMCODE_VERSION_CFG_METHOD_21  (0x0042)
#define NIC_RAMCODE_VERSION_CFG_METHOD_24  (0x0001)
#define NIC_RAMCODE_VERSION_CFG_METHOD_23  (0x0015)
#define NIC_RAMCODE_VERSION_CFG_METHOD_26  (0x0012)
#define NIC_RAMCODE_VERSION_CFG_METHOD_28  (0x0019)
#define NIC_RAMCODE_VERSION_CFG_METHOD_29  (0x0083)
#define NIC_RAMCODE_VERSION_CFG_METHOD_31  (0x0003)
#define NIC_RAMCODE_VERSION_CFG_METHOD_35  (0x0027)
#define NIC_RAMCODE_VERSION_CFG_METHOD_36  (0x0000)
#define NIC_RAMCODE_VERSION_CFG_METHOD_48  (0x0b11)
#define NIC_RAMCODE_VERSION_CFG_METHOD_49  (0x0b33)
#define NIC_RAMCODE_VERSION_CFG_METHOD_50  (0x0b17)
#define NIC_RAMCODE_VERSION_CFG_METHOD_51  (0x0b99)
#define NIC_RAMCODE_VERSION_CFG_METHOD_54  (0x0013)
#define NIC_RAMCODE_VERSION_CFG_METHOD_55  (0x0001)
#define NIC_RAMCODE_VERSION_CFG_METHOD_56  (0x0027)
#define NIC_RAMCODE_VERSION_CFG_METHOD_57  (0x0027)
#define NIC_RAMCODE_VERSION_CFG_METHOD_58  (0x0008)
#define NIC_RAMCODE_VERSION_CFG_METHOD_70  (0x0033)
#define NIC_RAMCODE_VERSION_CFG_METHOD_71  (0x0060)
#define NIC_RAMCODE_VERSION_CFG_METHOD_91  (0x0051)

#define RTL_MAC_MCU_PAGE_SIZE 256
#define RTL_DEFAULT_MTU       1500

#endif /* R8169_HW_H */
