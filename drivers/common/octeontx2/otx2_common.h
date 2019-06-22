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

/* IO Access */
#define otx2_read64(addr) rte_read64_relaxed((void *)(addr))
#define otx2_write64(val, addr) rte_write64_relaxed((val), (void *)(addr))

#if defined(RTE_ARCH_ARM64)
#include "otx2_io_arm64.h"
#else
#include "otx2_io_generic.h"
#endif

#endif /* _OTX2_COMMON_H_ */
