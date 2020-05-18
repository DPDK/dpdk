/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Intel Corporation
 */


#ifndef _IGC_OSDEP_H_
#define _IGC_OSDEP_H_

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_byteorder.h>
#include <rte_io.h>

#include "../igc_logs.h"

#define DELAY(x) rte_delay_us(x)
#define usec_delay(x) DELAY(x)
#define usec_delay_irq(x) DELAY(x)
#define msec_delay(x) DELAY(1000 * (x))
#define msec_delay_irq(x) DELAY(1000 * (x))

#define DEBUGFUNC(F)            DEBUGOUT(F "\n")
#define DEBUGOUT(S, args...)    PMD_DRV_LOG_RAW(DEBUG, S, ##args)
#define DEBUGOUT1(S, args...)   DEBUGOUT(S, ##args)
#define DEBUGOUT2(S, args...)   DEBUGOUT(S, ##args)
#define DEBUGOUT3(S, args...)   DEBUGOUT(S, ##args)
#define DEBUGOUT6(S, args...)   DEBUGOUT(S, ##args)
#define DEBUGOUT7(S, args...)   DEBUGOUT(S, ##args)

#define UNREFERENCED_PARAMETER(_p)	(void)(_p)
#define UNREFERENCED_1PARAMETER(_p)	(void)(_p)
#define UNREFERENCED_2PARAMETER(_p, _q)	\
	do {				\
		(void)(_p);		\
		(void)(_q);		\
	} while (0)
#define UNREFERENCED_3PARAMETER(_p, _q, _r)	\
	do {					\
		(void)(_p);			\
		(void)(_q);			\
		(void)(_r);			\
	} while (0)
#define UNREFERENCED_4PARAMETER(_p, _q, _r, _s)	\
	do {					\
		(void)(_p);			\
		(void)(_q);			\
		(void)(_r);			\
		(void)(_s);			\
	} while (0)

#define	CMD_MEM_WRT_INVALIDATE	0x0010  /* BIT_4 */

/* Mutex used in the shared code */
#define IGC_MUTEX                     uintptr_t
#define IGC_MUTEX_INIT(mutex)         (*(mutex) = 0)
#define IGC_MUTEX_LOCK(mutex)         (*(mutex) = 1)
#define IGC_MUTEX_UNLOCK(mutex)       (*(mutex) = 0)

typedef uint64_t	u64;
typedef uint32_t	u32;
typedef uint16_t	u16;
typedef uint8_t		u8;
typedef int64_t		s64;
typedef int32_t		s32;
typedef int16_t		s16;
typedef int8_t		s8;

#define __le16		u16
#define __le32		u32
#define __le64		u64

#define IGC_WRITE_FLUSH(a) IGC_READ_REG(a, IGC_STATUS)

#define IGC_PCI_REG(reg)	rte_read32(reg)

#define IGC_PCI_REG16(reg)	rte_read16(reg)

#define IGC_PCI_REG_WRITE(reg, value)			\
	rte_write32((rte_cpu_to_le_32(value)), reg)

#define IGC_PCI_REG_WRITE_RELAXED(reg, value)		\
	rte_write32_relaxed((rte_cpu_to_le_32(value)), reg)

#define IGC_PCI_REG_WRITE16(reg, value)		\
	rte_write16((rte_cpu_to_le_16(value)), reg)

#define IGC_PCI_REG_ADDR(hw, reg) \
	((volatile uint32_t *)((char *)(hw)->hw_addr + (reg)))

#define IGC_PCI_REG_ARRAY_ADDR(hw, reg, index) \
	IGC_PCI_REG_ADDR((hw), (reg) + ((index) << 2))

#define IGC_PCI_REG_FLASH_ADDR(hw, reg) \
	((volatile uint32_t *)((char *)(hw)->flash_address + (reg)))

static inline uint32_t igc_read_addr(volatile void *addr)
{
	return rte_le_to_cpu_32(IGC_PCI_REG(addr));
}

static inline uint16_t igc_read_addr16(volatile void *addr)
{
	return rte_le_to_cpu_16(IGC_PCI_REG16(addr));
}

/* Register READ/WRITE macros */

#define IGC_READ_REG(hw, reg) \
	igc_read_addr(IGC_PCI_REG_ADDR((hw), (reg)))

#define IGC_READ_REG_LE_VALUE(hw, reg) \
	rte_read32(IGC_PCI_REG_ADDR((hw), (reg)))

#define IGC_WRITE_REG(hw, reg, value) \
	IGC_PCI_REG_WRITE(IGC_PCI_REG_ADDR((hw), (reg)), (value))

#define IGC_WRITE_REG_LE_VALUE(hw, reg, value) \
	rte_write32(value, IGC_PCI_REG_ADDR((hw), (reg)))

#define IGC_READ_REG_ARRAY(hw, reg, index) \
	IGC_PCI_REG(IGC_PCI_REG_ARRAY_ADDR((hw), (reg), (index)))

#define IGC_WRITE_REG_ARRAY(hw, reg, index, value) \
	IGC_PCI_REG_WRITE(IGC_PCI_REG_ARRAY_ADDR((hw), (reg), (index)), \
			(value))

#define IGC_READ_REG_ARRAY_DWORD IGC_READ_REG_ARRAY
#define IGC_WRITE_REG_ARRAY_DWORD IGC_WRITE_REG_ARRAY

/*
 * To be able to do IO write, we need to map IO BAR
 * (bar 2/4 depending on device).
 * Right now mapping multiple BARs is not supported by DPDK.
 * Fortunatelly we need it only for legacy hw support.
 */

#define IGC_WRITE_REG_IO(hw, reg, value) \
	IGC_WRITE_REG(hw, reg, value)

/*
 * Tested on I217/I218 chipset.
 */

#define IGC_READ_FLASH_REG(hw, reg) \
	igc_read_addr(IGC_PCI_REG_FLASH_ADDR((hw), (reg)))

#define IGC_READ_FLASH_REG16(hw, reg)  \
	igc_read_addr16(IGC_PCI_REG_FLASH_ADDR((hw), (reg)))

#define IGC_WRITE_FLASH_REG(hw, reg, value)  \
	IGC_PCI_REG_WRITE(IGC_PCI_REG_FLASH_ADDR((hw), (reg)), (value))

#define IGC_WRITE_FLASH_REG16(hw, reg, value) \
	IGC_PCI_REG_WRITE16(IGC_PCI_REG_FLASH_ADDR((hw), (reg)), (value))

#endif /* _IGC_OSDEP_H_ */
