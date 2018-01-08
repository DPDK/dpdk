/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#ifndef _THUNDERX_NICVF_H
#define _THUNDERX_NICVF_H

/* Platform/OS/arch specific abstractions */

/* log */
#include <rte_log.h>
#include "../nicvf_logs.h"

#define nicvf_log_error(s, ...) PMD_DRV_LOG(ERR, s, ##__VA_ARGS__)

#define nicvf_log_debug(s, ...) PMD_DRV_LOG(DEBUG, s, ##__VA_ARGS__)

#define nicvf_mbox_log(s, ...) PMD_MBOX_LOG(DEBUG, s, ##__VA_ARGS__)

#define nicvf_log(s, ...) fprintf(stderr, s, ##__VA_ARGS__)

/* delay */
#include <rte_cycles.h>
#define nicvf_delay_us(x) rte_delay_us(x)

/* barrier */
#include <rte_atomic.h>
#define nicvf_smp_wmb() rte_smp_wmb()
#define nicvf_smp_rmb() rte_smp_rmb()

/* utils */
#include <rte_common.h>
#define nicvf_min(x, y) RTE_MIN(x, y)
#define nicvf_log2_u32(x) rte_log2_u32(x)

/* byte order */
#include <rte_byteorder.h>
#define nicvf_cpu_to_be_64(x) rte_cpu_to_be_64(x)
#define nicvf_be_to_cpu_64(x) rte_be_to_cpu_64(x)

#define NICVF_BYTE_ORDER RTE_BYTE_ORDER
#define NICVF_BIG_ENDIAN RTE_BIG_ENDIAN
#define NICVF_LITTLE_ENDIAN RTE_LITTLE_ENDIAN

/* Constants */
#include <rte_ether.h>
#define NICVF_MAC_ADDR_SIZE ETHER_ADDR_LEN

#include <rte_io.h>
#define nicvf_addr_write(addr, val) rte_write64_relaxed((val), (void *)(addr))
#define nicvf_addr_read(addr) rte_read64_relaxed((void *)(addr))

/* ARM64 specific functions */
#if defined(RTE_ARCH_ARM64)
#define nicvf_prefetch_store_keep(_ptr) ({\
	asm volatile("prfm pstl1keep, [%x0]\n" : : "r" (_ptr)); })


#define NICVF_LOAD_PAIR(reg1, reg2, addr) ({		\
			asm volatile(			\
			"ldp %x[x1], %x[x0], [%x[p1]]"	\
			: [x1]"=r"(reg1), [x0]"=r"(reg2)\
			: [p1]"r"(addr)			\
			); })

#else /* non optimized functions for building on non arm64 arch */

#define nicvf_prefetch_store_keep(_ptr) do {} while (0)

#define NICVF_LOAD_PAIR(reg1, reg2, addr)		\
do {							\
	reg1 = nicvf_addr_read((uintptr_t)addr);	\
	reg2 = nicvf_addr_read((uintptr_t)addr + 8);	\
} while (0)

#endif

#include "nicvf_hw.h"
#include "nicvf_mbox.h"

#endif /* _THUNDERX_NICVF_H */
