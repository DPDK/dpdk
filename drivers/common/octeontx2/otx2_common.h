/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _OTX2_COMMON_H_
#define _OTX2_COMMON_H_

#include <rte_common.h>

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

#endif /* _OTX2_COMMON_H_ */
