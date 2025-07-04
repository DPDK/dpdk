/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CN9K_CRYPTODEV_OPS_H_
#define _CN9K_CRYPTODEV_OPS_H_

#include <rte_compat.h>
#include <cryptodev_pmd.h>

#include <hw/cpt.h>

#if defined(__aarch64__)
#include "roc_io.h"
#else
#include "roc_io_generic.h"
#endif

extern struct rte_cryptodev_ops cn9k_cpt_ops;

static inline void
cn9k_cpt_inst_submit(struct cpt_inst_s *inst, uint64_t lmtline, uint64_t io_addr)
{
	uint64_t lmt_status;

	do {
		/* Copy CPT command to LMTLINE */
		roc_lmt_mov64((void *)lmtline, inst);

		/*
		 * Make sure compiler does not reorder memcpy and ldeor.
		 * LMTST transactions are always flushed from the write
		 * buffer immediately, a DMB is not required to push out
		 * LMTSTs.
		 */
		rte_io_wmb();
		lmt_status = roc_lmt_submit_ldeor(io_addr);
	} while (lmt_status == 0);
}

static __plt_always_inline void
cn9k_cpt_inst_submit_dual(struct cpt_inst_s *inst, uint64_t lmtline, uint64_t io_addr)
{
	uint64_t lmt_status;

	do {
		/* Copy 2 CPT inst_s to LMTLINE */
#if defined(RTE_ARCH_ARM64)
		volatile const __uint128_t *src128 = (const __uint128_t *)inst;
		volatile __uint128_t *dst128 = (__uint128_t *)lmtline;

		dst128[0] = src128[0];
		dst128[1] = src128[1];
		dst128[2] = src128[2];
		dst128[3] = src128[3];
		dst128[4] = src128[4];
		dst128[5] = src128[5];
		dst128[6] = src128[6];
		dst128[7] = src128[7];
#else
		roc_lmt_mov_seg((void *)lmtline, inst, 8);
#endif

		/*
		 * Make sure compiler does not reorder memcpy and ldeor.
		 * LMTST transactions are always flushed from the write
		 * buffer immediately, a DMB is not required to push out
		 * LMTSTs.
		 */
		rte_io_wmb();
		lmt_status = roc_lmt_submit_ldeor(io_addr);
	} while (lmt_status == 0);
}

void cn9k_cpt_set_enqdeq_fns(struct rte_cryptodev *dev);

__rte_internal
uint16_t cn9k_cpt_crypto_adapter_enqueue(uintptr_t base,
					 struct rte_crypto_op *op);
__rte_internal
uintptr_t cn9k_cpt_crypto_adapter_dequeue(uintptr_t get_work1);

#endif /* _CN9K_CRYPTODEV_OPS_H_ */
