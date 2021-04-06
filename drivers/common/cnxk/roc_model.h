/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_MODEL_H_
#define _ROC_MODEL_H_

#include <stdbool.h>

extern struct roc_model *roc_model;

struct roc_model {
#define ROC_MODEL_CN96xx_A0    BIT_ULL(0)
#define ROC_MODEL_CN96xx_B0    BIT_ULL(1)
#define ROC_MODEL_CN96xx_C0    BIT_ULL(2)
#define ROC_MODEL_CNF95xx_A0   BIT_ULL(4)
#define ROC_MODEL_CNF95xx_B0   BIT_ULL(6)
#define ROC_MODEL_CNF95XXMM_A0 BIT_ULL(8)
#define ROC_MODEL_CNF95XXN_A0  BIT_ULL(12)
#define ROC_MODEL_CNF95XXO_A0  BIT_ULL(13)
#define ROC_MODEL_CN98xx_A0    BIT_ULL(16)
#define ROC_MODEL_CN106XX      BIT_ULL(20)
#define ROC_MODEL_CNF105XX     BIT_ULL(21)
#define ROC_MODEL_CNF105XXN    BIT_ULL(22)

	uint64_t flag;
#define ROC_MODEL_STR_LEN_MAX 128
	char name[ROC_MODEL_STR_LEN_MAX];
} __plt_cache_aligned;

#define ROC_MODEL_CN96xx_Ax (ROC_MODEL_CN96xx_A0 | ROC_MODEL_CN96xx_B0)
#define ROC_MODEL_CN9K                                                         \
	(ROC_MODEL_CN96xx_Ax | ROC_MODEL_CN96xx_C0 | ROC_MODEL_CNF95xx_A0 |    \
	 ROC_MODEL_CNF95xx_B0 | ROC_MODEL_CNF95XXMM_A0 |                       \
	 ROC_MODEL_CNF95XXO_A0 | ROC_MODEL_CNF95XXN_A0 | ROC_MODEL_CN98xx_A0)

#define ROC_MODEL_CN10K                                                        \
	(ROC_MODEL_CN106XX | ROC_MODEL_CNF105XX | ROC_MODEL_CNF105XXN)

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
roc_model_is_cn96_A0(void)
{
	return roc_model->flag & ROC_MODEL_CN96xx_A0;
}

static inline uint64_t
roc_model_is_cn96_Ax(void)
{
	return (roc_model->flag & ROC_MODEL_CN96xx_Ax);
}

static inline uint64_t
roc_model_is_cn95_A0(void)
{
	return roc_model->flag & ROC_MODEL_CNF95xx_A0;
}

static inline uint64_t
roc_model_is_cn10ka(void)
{
	return roc_model->flag & ROC_MODEL_CN106XX;
}

static inline uint64_t
roc_model_is_cnf10ka(void)
{
	return roc_model->flag & ROC_MODEL_CNF105XX;
}

static inline uint64_t
roc_model_is_cnf10kb(void)
{
	return roc_model->flag & ROC_MODEL_CNF105XXN;
}

int roc_model_init(struct roc_model *model);

#endif
