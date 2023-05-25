/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_MODEL_H_
#define _ROC_MODEL_H_

#include <stdbool.h>

#include "roc_bits.h"

extern struct roc_model *roc_model;

struct roc_model {
#define ROC_MODEL_CN96xx_A0    BIT_ULL(0)
#define ROC_MODEL_CN96xx_B0    BIT_ULL(1)
#define ROC_MODEL_CN96xx_C0    BIT_ULL(2)
#define ROC_MODEL_CNF95xx_A0   BIT_ULL(4)
#define ROC_MODEL_CNF95xx_B0   BIT_ULL(6)
#define ROC_MODEL_CNF95xxMM_A0 BIT_ULL(8)
#define ROC_MODEL_CNF95xxN_A0  BIT_ULL(12)
#define ROC_MODEL_CNF95xxO_A0  BIT_ULL(13)
#define ROC_MODEL_CNF95xxN_A1  BIT_ULL(14)
#define ROC_MODEL_CNF95xxN_B0  BIT_ULL(15)
#define ROC_MODEL_CN98xx_A0    BIT_ULL(16)
#define ROC_MODEL_CN98xx_A1    BIT_ULL(17)
#define ROC_MODEL_CN106xx_A0   BIT_ULL(20)
#define ROC_MODEL_CNF105xx_A0  BIT_ULL(21)
#define ROC_MODEL_CNF105xxN_A0 BIT_ULL(22)
#define ROC_MODEL_CN103xx_A0   BIT_ULL(23)
#define ROC_MODEL_CN106xx_A1   BIT_ULL(24)
#define ROC_MODEL_CNF105xx_A1  BIT_ULL(25)
#define ROC_MODEL_CN106xx_B0   BIT_ULL(26)
#define ROC_MODEL_CNF105xxN_B0 BIT_ULL(27)
/* Following flags describe platform code is running on */
#define ROC_ENV_HW   BIT_ULL(61)
#define ROC_ENV_EMUL BIT_ULL(62)
#define ROC_ENV_ASIM BIT_ULL(63)

	uint64_t flag;
#define ROC_MODEL_STR_LEN_MAX 128
	char name[ROC_MODEL_STR_LEN_MAX];
	char env[ROC_MODEL_STR_LEN_MAX];
} __plt_cache_aligned;

#define ROC_MODEL_CN96xx_Ax (ROC_MODEL_CN96xx_A0 | ROC_MODEL_CN96xx_B0)
#define ROC_MODEL_CN98xx_Ax (ROC_MODEL_CN98xx_A0 | ROC_MODEL_CN98xx_A1)
#define ROC_MODEL_CN9K                                                         \
	(ROC_MODEL_CN96xx_Ax | ROC_MODEL_CN96xx_C0 | ROC_MODEL_CNF95xx_A0 |    \
	 ROC_MODEL_CNF95xx_B0 | ROC_MODEL_CNF95xxMM_A0 |                       \
	 ROC_MODEL_CNF95xxO_A0 | ROC_MODEL_CNF95xxN_A0 | ROC_MODEL_CN98xx_Ax | \
	 ROC_MODEL_CNF95xxN_A1 | ROC_MODEL_CNF95xxN_B0)
#define ROC_MODEL_CNF9K                                                        \
	(ROC_MODEL_CNF95xx_A0 | ROC_MODEL_CNF95xx_B0 |                         \
	 ROC_MODEL_CNF95xxMM_A0 | ROC_MODEL_CNF95xxO_A0 |                      \
	 ROC_MODEL_CNF95xxN_A0 | ROC_MODEL_CNF95xxN_A1 |                       \
	 ROC_MODEL_CNF95xxN_B0)

#define ROC_MODEL_CN106xx   (ROC_MODEL_CN106xx_A0 | ROC_MODEL_CN106xx_A1 | ROC_MODEL_CN106xx_B0)
#define ROC_MODEL_CNF105xx  (ROC_MODEL_CNF105xx_A0 | ROC_MODEL_CNF105xx_A1)
#define ROC_MODEL_CNF105xxN (ROC_MODEL_CNF105xxN_A0 | ROC_MODEL_CNF105xxN_B0)
#define ROC_MODEL_CN103xx   (ROC_MODEL_CN103xx_A0)
#define ROC_MODEL_CN10K                                                        \
	(ROC_MODEL_CN106xx | ROC_MODEL_CNF105xx | ROC_MODEL_CNF105xxN |        \
	 ROC_MODEL_CN103xx)
#define ROC_MODEL_CNF10K (ROC_MODEL_CNF105xx | ROC_MODEL_CNF105xxN)

/* Runtime variants */
static inline uint64_t
roc_model_runtime_is_cn9k(void)
{
	return (roc_model->flag & (ROC_MODEL_CN9K));
}

static inline uint64_t
roc_model_runtime_is_cn10k(void)
{
	return (roc_model->flag & (ROC_MODEL_CN10K));
}

/* Compile time variants */
#ifdef ROC_PLATFORM_CN9K
#define roc_model_constant_is_cn9k()  1
#define roc_model_constant_is_cn10k() 0
#else
#define roc_model_constant_is_cn9k()  0
#define roc_model_constant_is_cn10k() 1
#endif

/*
 * Compile time variants to enable optimized version check when the library
 * configured for specific platform version else to fallback to runtime.
 */
static inline uint64_t
roc_model_is_cn9k(void)
{
#ifdef ROC_PLATFORM_CN9K
	return 1;
#endif
#ifdef ROC_PLATFORM_CN10K
	return 0;
#endif
	return roc_model_runtime_is_cn9k();
}

static inline uint64_t
roc_model_is_cn10k(void)
{
#ifdef ROC_PLATFORM_CN10K
	return 1;
#endif
#ifdef ROC_PLATFORM_CN9K
	return 0;
#endif
	return roc_model_runtime_is_cn10k();
}

static inline uint64_t
roc_model_is_cn98xx(void)
{
	return (roc_model->flag & ROC_MODEL_CN98xx_Ax);
}

static inline uint64_t
roc_model_is_cn98xx_a0(void)
{
	return (roc_model->flag & ROC_MODEL_CN98xx_A0);
}

static inline uint64_t
roc_model_is_cn98xx_a1(void)
{
	return (roc_model->flag & ROC_MODEL_CN98xx_A1);
}

static inline uint64_t
roc_model_is_cn96_a0(void)
{
	return roc_model->flag & ROC_MODEL_CN96xx_A0;
}

static inline uint64_t
roc_model_is_cn96_ax(void)
{
	return (roc_model->flag & ROC_MODEL_CN96xx_Ax);
}

static inline uint64_t
roc_model_is_cn96_b0(void)
{
	return (roc_model->flag & ROC_MODEL_CN96xx_B0);
}

static inline uint64_t
roc_model_is_cn96_cx(void)
{
	return (roc_model->flag & ROC_MODEL_CN96xx_C0);
}

static inline uint64_t
roc_model_is_cn95_a0(void)
{
	return roc_model->flag & ROC_MODEL_CNF95xx_A0;
}

static inline uint64_t
roc_model_is_cnf95xxn_a0(void)
{
	return roc_model->flag & ROC_MODEL_CNF95xxN_A0;
}

static inline uint64_t
roc_model_is_cnf95xxn_a1(void)
{
	return roc_model->flag & ROC_MODEL_CNF95xxN_A1;
}

static inline uint64_t
roc_model_is_cnf95xxn_b0(void)
{
	return roc_model->flag & ROC_MODEL_CNF95xxN_B0;
}

static inline uint64_t
roc_model_is_cnf95xxo_a0(void)
{
	return roc_model->flag & ROC_MODEL_CNF95xxO_A0;
}

static inline uint16_t
roc_model_is_cn95xxn_a0(void)
{
	return roc_model->flag & ROC_MODEL_CNF95xxN_A0;
}

static inline uint64_t
roc_model_is_cn10ka(void)
{
	return roc_model->flag & ROC_MODEL_CN106xx;
}

static inline uint64_t
roc_model_is_cnf10ka(void)
{
	return roc_model->flag & ROC_MODEL_CNF105xx;
}

static inline uint64_t
roc_model_is_cnf10kb(void)
{
	return roc_model->flag & ROC_MODEL_CNF105xxN;
}

static inline uint64_t
roc_model_is_cn10kb_a0(void)
{
	return roc_model->flag & ROC_MODEL_CN103xx_A0;
}

static inline uint64_t
roc_model_is_cn10ka_a0(void)
{
	return roc_model->flag & ROC_MODEL_CN106xx_A0;
}

static inline uint64_t
roc_model_is_cn10ka_a1(void)
{
	return roc_model->flag & ROC_MODEL_CN106xx_A1;
}

static inline uint64_t
roc_model_is_cn10ka_b0(void)
{
	return roc_model->flag & ROC_MODEL_CN106xx_B0;
}

static inline uint64_t
roc_model_is_cnf10ka_a0(void)
{
	return roc_model->flag & ROC_MODEL_CNF105xx_A0;
}

static inline uint64_t
roc_model_is_cnf10ka_a1(void)
{
	return roc_model->flag & ROC_MODEL_CNF105xx_A1;
}

static inline uint64_t
roc_model_is_cnf10kb_a0(void)
{
	return roc_model->flag & ROC_MODEL_CNF105xxN_A0;
}

static inline uint64_t
roc_model_is_cnf10kb_b0(void)
{
	return roc_model->flag & ROC_MODEL_CNF105xxN_B0;
}

static inline uint64_t
roc_model_is_cn10kb(void)
{
	return roc_model->flag & ROC_MODEL_CN103xx;
}

static inline bool
roc_env_is_hw(void)
{
	return roc_model->flag & ROC_ENV_HW;
}

static inline bool
roc_env_is_emulator(void)
{
	return roc_model->flag & ROC_ENV_EMUL;
}

static inline bool
roc_env_is_asim(void)
{
	return roc_model->flag & ROC_ENV_ASIM;
}

static inline const char *
roc_env_get(void)
{
	return roc_model->env;
}

int roc_model_init(struct roc_model *model);

#endif
