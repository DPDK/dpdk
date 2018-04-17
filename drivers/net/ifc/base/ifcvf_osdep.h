/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _IFCVF_OSDEP_H_
#define _IFCVF_OSDEP_H_

#include <stdint.h>
#include <linux/pci_regs.h>

#include <rte_cycles.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_log.h>
#include <rte_io.h>

#define DEBUGOUT(S, args...)    RTE_LOG(DEBUG, PMD, S, ##args)
#define STATIC                  static

#define msec_delay	rte_delay_ms

#define IFCVF_READ_REG8(reg)		rte_read8(reg)
#define IFCVF_WRITE_REG8(val, reg)	rte_write8((val), (reg))
#define IFCVF_READ_REG16(reg)		rte_read16(reg)
#define IFCVF_WRITE_REG16(val, reg)	rte_write16((val), (reg))
#define IFCVF_READ_REG32(reg)		rte_read32(reg)
#define IFCVF_WRITE_REG32(val, reg)	rte_write32((val), (reg))

typedef struct rte_pci_device PCI_DEV;

#define PCI_READ_CONFIG_BYTE(dev, val, where) \
	rte_pci_read_config(dev, val, 1, where)

#define PCI_READ_CONFIG_DWORD(dev, val, where) \
	rte_pci_read_config(dev, val, 4, where)

typedef uint8_t    u8;
typedef int8_t     s8;
typedef uint16_t   u16;
typedef int16_t    s16;
typedef uint32_t   u32;
typedef int32_t    s32;
typedef int64_t    s64;
typedef uint64_t   u64;

static inline int
PCI_READ_CONFIG_RANGE(PCI_DEV *dev, uint32_t *val, int size, int where)
{
	return rte_pci_read_config(dev, val, size, where);
}

#endif /* _IFCVF_OSDEP_H_ */
