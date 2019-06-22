/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _OTX2_COMMON_H_
#define _OTX2_COMMON_H_

#include <rte_common.h>
#include <rte_io.h>
#include <rte_memory.h>

#include "hw/otx2_rvu.h"
#include "hw/otx2_nix.h"
#include "hw/otx2_npc.h"
#include "hw/otx2_npa.h"
#include "hw/otx2_sso.h"
#include "hw/otx2_ssow.h"
#include "hw/otx2_tim.h"

/* Alignment */
#define OTX2_ALIGN  128

/* Bits manipulation */
#ifndef BIT_ULL
#define BIT_ULL(nr) (1ULL << (nr))
#endif
#ifndef BIT
#define BIT(nr)     (1UL << (nr))
#endif

/* Compiler attributes */
#ifndef __hot
#define __hot   __attribute__((hot))
#endif

/* Log */
extern int otx2_logtype_base;
extern int otx2_logtype_mbox;
extern int otx2_logtype_npa;
extern int otx2_logtype_nix;
extern int otx2_logtype_sso;
extern int otx2_logtype_npc;
extern int otx2_logtype_tm;
extern int otx2_logtype_tim;
extern int otx2_logtype_dpi;

#define OTX2_CLNRM  "\x1b[0m"
#define OTX2_CLRED  "\x1b[31m"

#define otx2_err(fmt, args...)						\
	RTE_LOG(ERR, PMD, ""OTX2_CLRED"%s():%u " fmt OTX2_CLNRM"\n",	\
				__func__, __LINE__, ## args)

#define otx2_info(fmt, args...)						\
	RTE_LOG(INFO, PMD, fmt"\n", ## args)

#define otx2_dbg(subsystem, fmt, args...)				\
	rte_log(RTE_LOG_DEBUG, otx2_logtype_ ## subsystem,		\
		"[%s] %s():%u " fmt "\n",				\
		 #subsystem, __func__, __LINE__, ##args)

#define otx2_base_dbg(fmt, ...) otx2_dbg(base, fmt, ##__VA_ARGS__)
#define otx2_mbox_dbg(fmt, ...) otx2_dbg(mbox, fmt, ##__VA_ARGS__)
#define otx2_npa_dbg(fmt, ...) otx2_dbg(npa, fmt, ##__VA_ARGS__)
#define otx2_nix_dbg(fmt, ...) otx2_dbg(nix, fmt, ##__VA_ARGS__)
#define otx2_sso_dbg(fmt, ...) otx2_dbg(sso, fmt, ##__VA_ARGS__)
#define otx2_npc_dbg(fmt, ...) otx2_dbg(npc, fmt, ##__VA_ARGS__)
#define otx2_tm_dbg(fmt, ...) otx2_dbg(tm, fmt, ##__VA_ARGS__)
#define otx2_tim_dbg(fmt, ...) otx2_dbg(tim, fmt, ##__VA_ARGS__)
#define otx2_dpi_dbg(fmt, ...) otx2_dbg(dpi, fmt, ##__VA_ARGS__)

/* IO Access */
#define otx2_read64(addr) rte_read64_relaxed((void *)(addr))
#define otx2_write64(val, addr) rte_write64_relaxed((val), (void *)(addr))

#if defined(RTE_ARCH_ARM64)
#include "otx2_io_arm64.h"
#else
#include "otx2_io_generic.h"
#endif

#endif /* _OTX2_COMMON_H_ */
