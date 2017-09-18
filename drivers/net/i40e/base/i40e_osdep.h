/******************************************************************************

  Copyright (c) 2001-2015, Intel Corporation
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

#ifndef _I40E_OSDEP_H_
#define _I40E_OSDEP_H_

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

#include <rte_common.h>
#include <rte_memcpy.h>
#include <rte_byteorder.h>
#include <rte_cycles.h>
#include <rte_spinlock.h>
#include <rte_log.h>
#include <rte_io.h>

#include "../i40e_logs.h"

#define INLINE inline
#define STATIC static

typedef uint8_t         u8;
typedef int8_t          s8;
typedef uint16_t        u16;
typedef uint32_t        u32;
typedef int32_t         s32;
typedef uint64_t        u64;

typedef enum i40e_status_code i40e_status;
#define __iomem
#define hw_dbg(hw, S, A...) do {} while (0)
#define upper_32_bits(n) ((u32)(((n) >> 16) >> 16))
#define lower_32_bits(n) ((u32)(n))
#define low_16_bits(x)   ((x) & 0xFFFF)
#define high_16_bits(x)  (((x) & 0xFFFF0000) >> 16)

#ifndef ETH_ADDR_LEN
#define ETH_ADDR_LEN                  6
#endif

#ifndef __le16
#define __le16          uint16_t
#endif
#ifndef __le32
#define __le32          uint32_t
#endif
#ifndef __le64
#define __le64          uint64_t
#endif
#ifndef __be16
#define __be16          uint16_t
#endif
#ifndef __be32
#define __be32          uint32_t
#endif
#ifndef __be64
#define __be64          uint64_t
#endif

#define FALSE           0
#define TRUE            1
#define false           0
#define true            1

#define min(a,b) RTE_MIN(a,b)
#define max(a,b) RTE_MAX(a,b)

#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))

#define DEBUGOUT(S)        PMD_DRV_LOG_RAW(DEBUG, S)
#define DEBUGOUT1(S, A...) PMD_DRV_LOG_RAW(DEBUG, S, ##A)

#define DEBUGFUNC(F) DEBUGOUT(F "\n")
#define DEBUGOUT2 DEBUGOUT1
#define DEBUGOUT3 DEBUGOUT2
#define DEBUGOUT6 DEBUGOUT3
#define DEBUGOUT7 DEBUGOUT6

#define i40e_debug(h, m, s, ...)                                \
do {                                                            \
	if (((m) & (h)->debug_mask))                            \
		PMD_DRV_LOG_RAW(DEBUG, "i40e %02x.%x " s,       \
			(h)->bus.device, (h)->bus.func,         \
					##__VA_ARGS__);         \
} while (0)

/* AQ commands based interfaces of i40e_read_rx_ctl() and i40e_write_rx_ctl()
 * are required for reading/writing below registers, as reading/writing it
 * directly may not function correctly if the device is under heavy small
 * packet traffic. Note that those interfaces are available from FVL5 and not
 * suitable before the AdminQ is ready during initialization.
 *
 * I40E_PFQF_CTL_0
 * I40E_PFQF_HENA
 * I40E_PFQF_FDALLOC
 * I40E_PFQF_HREGION
 * I40E_PFLAN_QALLOC
 * I40E_VPQF_CTL
 * I40E_VFQF_HENA
 * I40E_VFQF_HREGION
 * I40E_VSIQF_CTL
 * I40E_VSILAN_QBASE
 * I40E_VSILAN_QTABLE
 * I40E_VSIQF_TCREGION
 * I40E_PFQF_HKEY
 * I40E_VFQF_HKEY
 * I40E_PRTQF_CTL_0
 * I40E_GLFCOE_RCTL
 * I40E_GLFCOE_RSOF
 * I40E_GLQF_CTL
 * I40E_GLQF_SWAP
 * I40E_GLQF_HASH_MSK
 * I40E_GLQF_HASH_INSET
 * I40E_GLQF_HSYM
 * I40E_GLQF_FC_MSK
 * I40E_GLQF_FC_INSET
 * I40E_GLQF_FD_MSK
 * I40E_PRTQF_FD_INSET
 * I40E_PRTQF_FD_FLXINSET
 * I40E_PRTQF_FD_MSK
 */

#define I40E_PCI_REG(reg)		rte_read32(reg)
#define I40E_PCI_REG_ADDR(a, reg) \
	((volatile uint32_t *)((char *)(a)->hw_addr + (reg)))
static inline uint32_t i40e_read_addr(volatile void *addr)
{
	return rte_le_to_cpu_32(I40E_PCI_REG(addr));
}

#define I40E_PCI_REG_WRITE(reg, value)		\
	rte_write32((rte_cpu_to_le_32(value)), reg)
#define I40E_PCI_REG_WRITE_RELAXED(reg, value)	\
	rte_write32_relaxed((rte_cpu_to_le_32(value)), reg)

#define I40E_WRITE_FLUSH(a) I40E_READ_REG(a, I40E_GLGEN_STAT)
#define I40EVF_WRITE_FLUSH(a) I40E_READ_REG(a, I40E_VFGEN_RSTAT)

#define I40E_READ_REG(hw, reg) i40e_read_addr(I40E_PCI_REG_ADDR((hw), (reg)))
#define I40E_WRITE_REG(hw, reg, value) \
	I40E_PCI_REG_WRITE(I40E_PCI_REG_ADDR((hw), (reg)), (value))

#define rd32(a, reg) i40e_read_addr(I40E_PCI_REG_ADDR((a), (reg)))
#define wr32(a, reg, value) \
	I40E_PCI_REG_WRITE(I40E_PCI_REG_ADDR((a), (reg)), (value))
#define flush(a) i40e_read_addr(I40E_PCI_REG_ADDR((a), (I40E_GLGEN_STAT)))

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))

/* memory allocation tracking */
struct i40e_dma_mem {
	void *va;
	u64 pa;
	u32 size;
	const void *zone;
} __attribute__((packed));

#define i40e_allocate_dma_mem(h, m, unused, s, a) \
			i40e_allocate_dma_mem_d(h, m, s, a)
#define i40e_free_dma_mem(h, m) i40e_free_dma_mem_d(h, m)

struct i40e_virt_mem {
	void *va;
	u32 size;
} __attribute__((packed));

#define i40e_allocate_virt_mem(h, m, s) i40e_allocate_virt_mem_d(h, m, s)
#define i40e_free_virt_mem(h, m) i40e_free_virt_mem_d(h, m)

#define CPU_TO_LE16(o) rte_cpu_to_le_16(o)
#define CPU_TO_LE32(s) rte_cpu_to_le_32(s)
#define CPU_TO_LE64(h) rte_cpu_to_le_64(h)
#define LE16_TO_CPU(a) rte_le_to_cpu_16(a)
#define LE32_TO_CPU(c) rte_le_to_cpu_32(c)
#define LE64_TO_CPU(k) rte_le_to_cpu_64(k)

#define cpu_to_le16(o) rte_cpu_to_le_16(o)
#define cpu_to_le32(s) rte_cpu_to_le_32(s)
#define cpu_to_le64(h) rte_cpu_to_le_64(h)
#define le16_to_cpu(a) rte_le_to_cpu_16(a)
#define le32_to_cpu(c) rte_le_to_cpu_32(c)
#define le64_to_cpu(k) rte_le_to_cpu_64(k)

/* SW spinlock */
struct i40e_spinlock {
	rte_spinlock_t spinlock;
};

#define i40e_init_spinlock(_sp) i40e_init_spinlock_d(_sp)
#define i40e_acquire_spinlock(_sp) i40e_acquire_spinlock_d(_sp)
#define i40e_release_spinlock(_sp) i40e_release_spinlock_d(_sp)
#define i40e_destroy_spinlock(_sp) i40e_destroy_spinlock_d(_sp)

#define I40E_NTOHS(a) rte_be_to_cpu_16(a)
#define I40E_NTOHL(a) rte_be_to_cpu_32(a)
#define I40E_HTONS(a) rte_cpu_to_be_16(a)
#define I40E_HTONL(a) rte_cpu_to_be_32(a)

#define i40e_memset(a, b, c, d) memset((a), (b), (c))
#define i40e_memcpy(a, b, c, d) rte_memcpy((a), (b), (c))

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define DELAY(x) rte_delay_us(x)
#define i40e_usec_delay(x) rte_delay_us(x)
#define i40e_msec_delay(x) rte_delay_us(1000*(x))
#define udelay(x) DELAY(x)
#define msleep(x) DELAY(1000*(x))
#define usleep_range(min, max) msleep(DIV_ROUND_UP(min, 1000))

#endif /* _I40E_OSDEP_H_ */
