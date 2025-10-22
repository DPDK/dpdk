/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#ifndef _NBL_DEF_COMMON_H_
#define _NBL_DEF_COMMON_H_

#include "nbl_include.h"

#define NBL_OPS_CALL(func, para)	\
	({ typeof(func) _func = (func);	\
	 (!_func) ? 0 : _func para; })

struct nbl_dma_mem {
	void *va;
	uint64_t pa;
	uint32_t size;
	const struct rte_memzone *mz;
};

struct nbl_work {
	TAILQ_ENTRY(nbl_work) next;
	void *params;
	void (*handler)(void *priv);
	uint32_t tick;
	uint32_t random;
	bool run_once;
	bool no_run;
	uint8_t resv[2];
};

void *nbl_alloc_dma_mem(struct nbl_dma_mem *mem, uint32_t size);
void nbl_free_dma_mem(struct nbl_dma_mem *mem);

#endif
