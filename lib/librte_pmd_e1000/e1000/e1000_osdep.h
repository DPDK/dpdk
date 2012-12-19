/******************************************************************************

  Copyright (c) 2001-2011, Intel Corporation 
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:
  
   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
  
   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.
  
   3. Neither the name of the Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived from 
      this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/
/*$FreeBSD$*/

#ifndef _E1000_OSDEP_H_
#define _E1000_OSDEP_H_

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_log.h>
#include <rte_debug.h>

#include "../e1000_logs.h"

/* Remove some compiler warnings for the files in this dir */
#ifdef __INTEL_COMPILER
#pragma warning(disable:2259) /* conversion may lose significant bits */
#pragma warning(disable:869)  /* Parameter was never referenced */
#pragma warning(disable:181)  /* Arg incompatible with format string */
#pragma warning(disable:188)  /* enumerated type mixed with another type */
#pragma warning(disable:1599) /* declaration hides variable */
#pragma warning(disable:177)  /* declared but never referenced */
#else
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wuninitialized"
#pragma GCC diagnostic ignored "-Wunused-variable"
#if (((__GNUC__) >= 4) && ((__GNUC_MINOR__) >= 7))
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#endif

#define DELAY(x) rte_delay_us(x)
#define usec_delay(x) DELAY(x)
#define msec_delay(x) DELAY(1000*(x))
#define msec_delay_irq(x) DELAY(1000*(x))

#define DEBUGFUNC(F)            DEBUGOUT(F);
#define DEBUGOUT(S, args...)    PMD_DRV_LOG(DEBUG, S, ##args)
#define DEBUGOUT1(S, args...)   DEBUGOUT(S, ##args)
#define DEBUGOUT2(S, args...)   DEBUGOUT(S, ##args)
#define DEBUGOUT3(S, args...)   DEBUGOUT(S, ##args)
#define DEBUGOUT6(S, args...)   DEBUGOUT(S, ##args)
#define DEBUGOUT7(S, args...)   DEBUGOUT(S, ##args)

#define FALSE			0
#define TRUE			1

#define	CMD_MEM_WRT_INVALIDATE	0x0010  /* BIT_4 */

/* Mutex used in the shared code */
#define E1000_MUTEX                     uintptr_t
#define E1000_MUTEX_INIT(mutex)         (*(mutex) = 0)
#define E1000_MUTEX_LOCK(mutex)         (*(mutex) = 1)
#define E1000_MUTEX_UNLOCK(mutex)       (*(mutex) = 0)

typedef uint64_t	u64;
typedef uint32_t	u32;
typedef uint16_t	u16;
typedef uint8_t		u8;
typedef int64_t		s64;
typedef int32_t		s32;
typedef int16_t		s16;
typedef int8_t		s8;
typedef int		bool;

#define __le16		u16
#define __le32		u32
#define __le64		u64

#define E1000_WRITE_FLUSH(a) E1000_READ_REG(a, E1000_STATUS)

#define E1000_PCI_REG(reg) (*((volatile uint32_t *)(reg)))

#define E1000_PCI_REG_WRITE(reg, value) do { \
	E1000_PCI_REG((reg)) = (value); \
} while (0)

#define E1000_PCI_REG_ADDR(hw, reg) \
	((volatile uint32_t *)((char *)(hw)->hw_addr + (reg)))

#define E1000_PCI_REG_ARRAY_ADDR(hw, reg, index) \
	E1000_PCI_REG_ADDR((hw), (reg) + ((index) << 2))

static inline uint32_t e1000_read_addr(volatile void* addr)
{
	return E1000_PCI_REG(addr);
}

/* Register READ/WRITE macros */

#define E1000_READ_REG(hw, reg) \
	e1000_read_addr(E1000_PCI_REG_ADDR((hw), (reg)))

#define E1000_WRITE_REG(hw, reg, value) \
	E1000_PCI_REG_WRITE(E1000_PCI_REG_ADDR((hw), (reg)), (value))

#define E1000_READ_REG_ARRAY(hw, reg, index) \
	E1000_PCI_REG(E1000_PCI_REG_ARRAY_ADDR((hw), (reg), (index)))

#define E1000_WRITE_REG_ARRAY(hw, reg, index, value) \
	E1000_PCI_REG_WRITE(E1000_PCI_REG_ARRAY_ADDR((hw), (reg), (index)), (value))

#define E1000_READ_REG_ARRAY_DWORD E1000_READ_REG_ARRAY
#define E1000_WRITE_REG_ARRAY_DWORD E1000_WRITE_REG_ARRAY

#define	E1000_ACCESS_PANIC(x, hw, reg, value) \
	rte_panic("%s:%u\t" RTE_STR(x) "(%p, 0x%x, 0x%x)", \
		__FILE__, __LINE__, (hw), (reg), (value))

/*
 * To be able to do IO write, we need to map IO BAR
 * (bar 2/4 depending on device).
 * Right now mapping multiple BARs is not supported by DPDK.
 * Fortunatelly we need it only for legacy hw support.
 */

#define E1000_WRITE_REG_IO(hw, reg, value) \
	E1000_WRITE_REG(hw, reg, value)

/*
 * Not implemented.
 */

#define E1000_READ_FLASH_REG(hw, reg) \
	(E1000_ACCESS_PANIC(E1000_READ_FLASH_REG, hw, reg, 0), 0)

#define E1000_READ_FLASH_REG16(hw, reg)  \
	(E1000_ACCESS_PANIC(E1000_READ_FLASH_REG16, hw, reg, 0), 0)

#define E1000_WRITE_FLASH_REG(hw, reg, value)  \
	E1000_ACCESS_PANIC(E1000_WRITE_FLASH_REG, hw, reg, value)

#define E1000_WRITE_FLASH_REG16(hw, reg, value) \
	E1000_ACCESS_PANIC(E1000_WRITE_FLASH_REG16, hw, reg, value)

#define STATIC static

#ifndef ETH_ADDR_LEN
#define ETH_ADDR_LEN                  6
#endif

#define false                         FALSE
#define true                          TRUE

#endif /* _E1000_OSDEP_H_ */
