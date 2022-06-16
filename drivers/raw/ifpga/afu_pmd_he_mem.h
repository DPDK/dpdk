/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef AFU_PMD_HE_MEM_H
#define AFU_PMD_HE_MEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "afu_pmd_core.h"
#include "rte_pmd_afu.h"

#define HE_MEM_TG_UUID_L  0xa3dc5b831f5cecbb
#define HE_MEM_TG_UUID_H  0x4dadea342c7848cb

#define NUM_MEM_TG_CHANNELS      4
#define MEM_TG_TIMEOUT_MS     5000
#define MEM_TG_POLL_INTERVAL_MS 10

/* MEM-TG registers definition */
#define MEM_TG_SCRATCHPAD   0x28
#define MEM_TG_CTRL         0x30
#define   TGCONTROL(n)      (1 << (n))
#define MEM_TG_STAT         0x38
#define   TGSTATUS(v, n)    (((v) >> (n << 2)) & 0xf)
#define   TGPASS(v, n)      (((v) >> ((n << 2) + 3)) & 0x1)
#define   TGFAIL(v, n)      (((v) >> ((n << 2) + 2)) & 0x1)
#define   TGTIMEOUT(v, n)   (((v) >> ((n << 2) + 1)) & 0x1)
#define   TGACTIVE(v, n)    (((v) >> (n << 2)) & 0x1)

struct he_mem_tg_ctx {
	uint8_t *addr;
};

struct he_mem_tg_priv {
	struct rte_pmd_afu_he_mem_tg_cfg he_mem_tg_cfg;
	struct he_mem_tg_ctx he_mem_tg_ctx;
};

#ifdef __cplusplus
}
#endif

#endif /* AFU_PMD_HE_MEM_H */
