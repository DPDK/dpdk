/*******************************************************************************

Copyright (c) 2013-2015, Intel Corporation
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

***************************************************************************/

#ifndef _FM10K_OSDEP_H_
#define _FM10K_OSDEP_H_

#include <stdint.h>
#include <string.h>
#include <rte_atomic.h>
#include <rte_byteorder.h>
#include <rte_cycles.h>
#include "../fm10k_logs.h"

/* TODO: this does not look like it should be used... */
#define ERROR_REPORT2(v1, v2, v3)   do { } while (0)

#ifndef BOULDER_RAPIDS_HW
#define BOULDER_RAPIDS_HW
#endif

#define STATIC                  static
#define DEBUGFUNC(F)            DEBUGOUT(F "\n");
#define DEBUGOUT(S, args...)    PMD_DRV_LOG_RAW(DEBUG, S, ##args)
#define DEBUGOUT1(S, args...)   DEBUGOUT(S, ##args)
#define DEBUGOUT2(S, args...)   DEBUGOUT(S, ##args)
#define DEBUGOUT3(S, args...)   DEBUGOUT(S, ##args)
#define DEBUGOUT6(S, args...)   DEBUGOUT(S, ##args)
#define DEBUGOUT7(S, args...)   DEBUGOUT(S, ##args)

#define FALSE      0
#define TRUE       1
#ifndef false
#define false      FALSE
#endif
#ifndef true
#define true       TRUE
#endif

typedef uint8_t    u8;
typedef int8_t     s8;
typedef uint16_t   u16;
typedef int16_t    s16;
typedef uint32_t   u32;
typedef int32_t    s32;
typedef int64_t    s64;
typedef uint64_t   u64;
typedef int        bool;

#ifndef __le16
#define __le16     u16
#define __le32     u32
#define __le64     u64
#endif
#ifndef __be16
#define __be16     u16
#define __be32     u32
#define __be64     u64
#endif

/* offsets are WORD offsets, not BYTE offsets */
#define FM10K_WRITE_REG(hw, reg, val)    \
	((((volatile uint32_t *)(hw)->hw_addr)[(reg)]) = ((uint32_t)(val)))
#define FM10K_READ_REG(hw, reg)          \
	(((volatile uint32_t *)(hw)->hw_addr)[(reg)])
#define FM10K_WRITE_FLUSH(a) FM10K_READ_REG(a, FM10K_CTRL)

#define FM10K_PCI_REG(reg) (*((volatile uint32_t *)(reg)))

#define FM10K_PCI_REG_WRITE(reg, value) do { \
	FM10K_PCI_REG((reg)) = (value); \
} while (0)

/* not implemented */
#define FM10K_READ_PCI_WORD(hw, reg)     0

#define FM10K_WRITE_MBX(hw, reg, value) FM10K_WRITE_REG(hw, reg, value)
#define FM10K_READ_MBX(hw, reg) FM10K_READ_REG(hw, reg)

#define FM10K_LE16_TO_CPU    rte_le_to_cpu_16
#define FM10K_LE32_TO_CPU    rte_le_to_cpu_32
#define FM10K_CPU_TO_LE32    rte_cpu_to_le_32
#define FM10K_CPU_TO_LE16    rte_cpu_to_le_16
#define le16_to_cpu          rte_le_to_cpu_16

#define FM10K_RMB            rte_rmb
#define FM10K_WMB            rte_wmb

#define usec_delay           rte_delay_us

#define FM10K_REMOVED(hw_addr) (!(hw_addr))

#ifndef FM10K_IS_ZERO_ETHER_ADDR
/* make certain address is not 0 */
#define FM10K_IS_ZERO_ETHER_ADDR(addr) \
(!((addr)[0] | (addr)[1] | (addr)[2] | (addr)[3] | (addr)[4] | (addr)[5]))
#endif

#ifndef FM10K_IS_MULTICAST_ETHER_ADDR
#define FM10K_IS_MULTICAST_ETHER_ADDR(addr) ((addr)[0] & 0x1)
#endif

#ifndef FM10K_IS_VALID_ETHER_ADDR
/* make certain address is not multicast or 0 */
#define FM10K_IS_VALID_ETHER_ADDR(addr) \
(!FM10K_IS_MULTICAST_ETHER_ADDR(addr) && !FM10K_IS_ZERO_ETHER_ADDR(addr))
#endif

#ifndef do_div
#define do_div(n, base) ({\
	(n) = (n) / (base);\
})
#endif /* do_div */

/* DPDK can't access IOMEM directly */
#ifndef FM10K_WRITE_SW_REG
#define FM10K_WRITE_SW_REG(v1, v2, v3)   do { } while (0)
#endif

#ifndef fm10k_read_reg
#define fm10k_read_reg FM10K_READ_REG
#endif

#define FM10K_INTEL_VENDOR_ID       0x8086
#define FM10K_DMA_CTRL_MINMSS_SHIFT		9
#define FM10K_EICR_PCA_FAULT			0x00000001
#define FM10K_EICR_THI_FAULT			0x00000004
#define FM10K_EICR_FUM_FAULT			0x00000020
#define FM10K_EICR_SRAMERROR			0x00000400
#define FM10K_SRAM_IP		0x13003
#define FM10K_RXINT_TIMER_SHIFT			8
#define FM10K_TXINT_TIMER_SHIFT			8
#define FM10K_RXD_PKTTYPE_MASK		0x03F0
#define FM10K_RXD_PKTTYPE_SHIFT		4

enum fm10k_rdesc_pkt_type {
	/* L3 type */
	FM10K_PKTTYPE_OTHER	= 0x00,
	FM10K_PKTTYPE_IPV4	= 0x01,
	FM10K_PKTTYPE_IPV4_EX	= 0x02,
	FM10K_PKTTYPE_IPV6	= 0x03,
	FM10K_PKTTYPE_IPV6_EX	= 0x04,

	/* L4 type */
	FM10K_PKTTYPE_TCP	= 0x08,
	FM10K_PKTTYPE_UDP	= 0x10,
	FM10K_PKTTYPE_GRE	= 0x18,
	FM10K_PKTTYPE_VXLAN	= 0x20,
	FM10K_PKTTYPE_NVGRE	= 0x28,
	FM10K_PKTTYPE_GENEVE	= 0x30
};

#define FM10K_RXD_STATUS_IPCS		0x0008 /* Indicates IPv4 csum */
#define FM10K_RXD_STATUS_HBO		0x0400 /* header buffer overrun */

#define FM10K_TSO_MINMSS \
	(FM10K_DMA_CTRL_MINMSS_64 >> FM10K_DMA_CTRL_MINMSS_SHIFT)
#define FM10K_TSO_MIN_HEADERLEN			54
#define FM10K_TSO_MAX_HEADERLEN			192

#endif /* _FM10K_OSDEP_H_ */
