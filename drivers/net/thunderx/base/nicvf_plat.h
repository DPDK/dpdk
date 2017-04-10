/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium networks Ltd. 2016.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Cavium networks nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
	asm volatile("prfm pstl1keep, %a0\n" : : "p" (_ptr)); })


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
