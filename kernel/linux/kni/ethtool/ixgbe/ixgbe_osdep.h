/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************

  Intel 10 Gigabit PCI Express Linux driver
  Copyright(c) 1999 - 2012 Intel Corporation.

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/


/* glue for the OS independent part of ixgbe
 * includes register access macros
 */

#ifndef _IXGBE_OSDEP_H_
#define _IXGBE_OSDEP_H_

#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/if_ether.h>
#include <linux/sched.h>
#include "kcompat.h"


#ifndef msleep
#define msleep(x)	do { if (in_interrupt()) { \
				/* Don't mdelay in interrupt context! */ \
				BUG(); \
			} else { \
				msleep(x); \
			} } while (0)

#endif

#undef ASSERT

#ifdef DBG
#define hw_dbg(hw, S, A...)	printk(KERN_DEBUG S, ## A)
#else
#define hw_dbg(hw, S, A...)	do {} while (0)
#endif

#define e_dev_info(format, arg...) \
	dev_info(pci_dev_to_dev(adapter->pdev), format, ## arg)
#define e_dev_warn(format, arg...) \
	dev_warn(pci_dev_to_dev(adapter->pdev), format, ## arg)
#define e_dev_err(format, arg...) \
	dev_err(pci_dev_to_dev(adapter->pdev), format, ## arg)
#define e_dev_notice(format, arg...) \
	dev_notice(pci_dev_to_dev(adapter->pdev), format, ## arg)
#define e_info(msglvl, format, arg...) \
	netif_info(adapter, msglvl, adapter->netdev, format, ## arg)
#define e_err(msglvl, format, arg...) \
	netif_err(adapter, msglvl, adapter->netdev, format, ## arg)
#define e_warn(msglvl, format, arg...) \
	netif_warn(adapter, msglvl, adapter->netdev, format, ## arg)
#define e_crit(msglvl, format, arg...) \
	netif_crit(adapter, msglvl, adapter->netdev, format, ## arg)


#ifdef DBG
#define IXGBE_WRITE_REG(a, reg, value) do {\
	switch (reg) { \
	case IXGBE_EIMS: \
	case IXGBE_EIMC: \
	case IXGBE_EIAM: \
	case IXGBE_EIAC: \
	case IXGBE_EICR: \
	case IXGBE_EICS: \
		printk("%s: Reg - 0x%05X, value - 0x%08X\n", __func__, \
		       reg, (u32)(value)); \
	default: \
		break; \
	} \
	writel((value), ((a)->hw_addr + (reg))); \
} while (0)
#else
#define IXGBE_WRITE_REG(a, reg, value) writel((value), ((a)->hw_addr + (reg)))
#endif

#define IXGBE_READ_REG(a, reg) readl((a)->hw_addr + (reg))

#define IXGBE_WRITE_REG_ARRAY(a, reg, offset, value) ( \
	writel((value), ((a)->hw_addr + (reg) + ((offset) << 2))))

#define IXGBE_READ_REG_ARRAY(a, reg, offset) ( \
	readl((a)->hw_addr + (reg) + ((offset) << 2)))

#ifndef writeq
#define writeq(val, addr)	do { writel((u32) (val), addr); \
				     writel((u32) (val >> 32), (addr + 4)); \
				} while (0);
#endif

#define IXGBE_WRITE_REG64(a, reg, value) writeq((value), ((a)->hw_addr + (reg)))

#define IXGBE_WRITE_FLUSH(a) IXGBE_READ_REG(a, IXGBE_STATUS)
struct ixgbe_hw;
extern u16 ixgbe_read_pci_cfg_word(struct ixgbe_hw *hw, u32 reg);
extern void ixgbe_write_pci_cfg_word(struct ixgbe_hw *hw, u32 reg, u16 value);
extern void ewarn(struct ixgbe_hw *hw, const char *str, u32 status);

#define IXGBE_READ_PCIE_WORD ixgbe_read_pci_cfg_word
#define IXGBE_WRITE_PCIE_WORD ixgbe_write_pci_cfg_word
#define IXGBE_EEPROM_GRANT_ATTEMPS 100
#define IXGBE_HTONL(_i) htonl(_i)
#define IXGBE_NTOHL(_i) ntohl(_i)
#define IXGBE_NTOHS(_i) ntohs(_i)
#define IXGBE_CPU_TO_LE32(_i) cpu_to_le32(_i)
#define IXGBE_LE32_TO_CPUS(_i) le32_to_cpus(_i)
#define EWARN(H, W, S) ewarn(H, W, S)

#endif /* _IXGBE_OSDEP_H_ */
