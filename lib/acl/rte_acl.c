/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <rte_eal_memconfig.h>
#include <rte_string_fns.h>
#include <rte_acl.h>
#include <rte_tailq.h>

#include "acl.h"

TAILQ_HEAD(rte_acl_list, rte_tailq_entry);

static struct rte_tailq_elem rte_acl_tailq = {
	.name = "RTE_ACL",
};
EAL_REGISTER_TAILQ(rte_acl_tailq)

#ifndef CC_AVX512_SUPPORT
/*
 * If the compiler doesn't support AVX512 instructions,
 * then the dummy one would be used instead for AVX512 classify method.
 */
int
rte_acl_classify_avx512x16(__rte_unused const struct rte_acl_ctx *ctx,
	__rte_unused const uint8_t **data,
	__rte_unused uint32_t *results,
	__rte_unused uint32_t num,
	__rte_unused uint32_t categories)
{
	return -ENOTSUP;
}

int
rte_acl_classify_avx512x32(__rte_unused const struct rte_acl_ctx *ctx,
	__rte_unused const uint8_t **data,
	__rte_unused uint32_t *results,
	__rte_unused uint32_t num,
	__rte_unused uint32_t categories)
{
	return -ENOTSUP;
}
#endif

#ifndef CC_AVX2_SUPPORT
/*
 * If the compiler doesn't support AVX2 instructions,
 * then the dummy one would be used instead for AVX2 classify method.
 */
int
rte_acl_classify_avx2(__rte_unused const struct rte_acl_ctx *ctx,
	__rte_unused const uint8_t **data,
	__rte_unused uint32_t *results,
	__rte_unused uint32_t num,
	__rte_unused uint32_t categories)
{
	return -ENOTSUP;
}
#endif

#ifndef RTE_ARCH_X86
int
rte_acl_classify_sse(__rte_unused const struct rte_acl_ctx *ctx,
	__rte_unused const uint8_t **data,
	__rte_unused uint32_t *results,
	__rte_unused uint32_t num,
	__rte_unused uint32_t categories)
{
	return -ENOTSUP;
}
#endif

#ifndef RTE_ARCH_ARM
int
rte_acl_classify_neon(__rte_unused const struct rte_acl_ctx *ctx,
	__rte_unused const uint8_t **data,
	__rte_unused uint32_t *results,
	__rte_unused uint32_t num,
	__rte_unused uint32_t categories)
{
	return -ENOTSUP;
}
#endif

#ifndef RTE_ARCH_PPC_64
int
rte_acl_classify_altivec(__rte_unused const struct rte_acl_ctx *ctx,
	__rte_unused const uint8_t **data,
	__rte_unused uint32_t *results,
	__rte_unused uint32_t num,
	__rte_unused uint32_t categories)
{
	return -ENOTSUP;
}
#endif

static const rte_acl_classify_t classify_fns[] = {
	[RTE_ACL_CLASSIFY_DEFAULT] = rte_acl_classify_scalar,
	[RTE_ACL_CLASSIFY_SCALAR] = rte_acl_classify_scalar,
	[RTE_ACL_CLASSIFY_SSE] = rte_acl_classify_sse,
	[RTE_ACL_CLASSIFY_AVX2] = rte_acl_classify_avx2,
	[RTE_ACL_CLASSIFY_NEON] = rte_acl_classify_neon,
	[RTE_ACL_CLASSIFY_ALTIVEC] = rte_acl_classify_altivec,
	[RTE_ACL_CLASSIFY_AVX512X16] = rte_acl_classify_avx512x16,
	[RTE_ACL_CLASSIFY_AVX512X32] = rte_acl_classify_avx512x32,
};

/*
 * Helper function for acl_check_alg.
 * Check support for ARM specific classify methods.
 */
static int
acl_check_alg_arm(enum rte_acl_classify_alg alg)
{
	if (alg == RTE_ACL_CLASSIFY_NEON) {
#if defined(RTE_ARCH_ARM64)
		if (rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_128)
			return 0;
#elif defined(RTE_ARCH_ARM)
		if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_NEON) &&
				rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_128)
			return 0;
#endif
		return -ENOTSUP;
	}

	return -EINVAL;
}

/*
 * Helper function for acl_check_alg.
 * Check support for PPC specific classify methods.
 */
static int
acl_check_alg_ppc(enum rte_acl_classify_alg alg)
{
	if (alg == RTE_ACL_CLASSIFY_ALTIVEC) {
#if defined(RTE_ARCH_PPC_64)
		if (rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_128)
			return 0;
#endif
		return -ENOTSUP;
	}

	return -EINVAL;
}

#ifdef CC_AVX512_SUPPORT
static int
acl_check_avx512_cpu_flags(void)
{
	return (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512F) &&
			rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512VL) &&
			rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512CD) &&
			rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512BW));
}
#endif

/*
 * Helper function for acl_check_alg.
 * Check support for x86 specific classify methods.
 */
static int
acl_check_alg_x86(enum rte_acl_classify_alg alg)
{
	if (alg == RTE_ACL_CLASSIFY_AVX512X32) {
#ifdef CC_AVX512_SUPPORT
		if (acl_check_avx512_cpu_flags() != 0 &&
			rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_512)
			return 0;
#endif
		return -ENOTSUP;
	}

	if (alg == RTE_ACL_CLASSIFY_AVX512X16) {
#ifdef CC_AVX512_SUPPORT
		if (acl_check_avx512_cpu_flags() != 0 &&
			rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_256)
			return 0;
#endif
		return -ENOTSUP;
	}

	if (alg == RTE_ACL_CLASSIFY_AVX2) {
#ifdef CC_AVX2_SUPPORT
		if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX2) &&
				rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_256)
			return 0;
#endif
		return -ENOTSUP;
	}

	if (alg == RTE_ACL_CLASSIFY_SSE) {
#ifdef RTE_ARCH_X86
		if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_SSE4_1) &&
				rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_128)
			return 0;
#endif
		return -ENOTSUP;
	}

	return -EINVAL;
}

/*
 * Check if input alg is supported by given platform/binary.
 * Note that both conditions should be met:
 * - at build time compiler supports ISA used by given methods
 * - at run time target cpu supports necessary ISA.
 */
static int
acl_check_alg(enum rte_acl_classify_alg alg)
{
	switch (alg) {
	case RTE_ACL_CLASSIFY_NEON:
		return acl_check_alg_arm(alg);
	case RTE_ACL_CLASSIFY_ALTIVEC:
		return acl_check_alg_ppc(alg);
	case RTE_ACL_CLASSIFY_AVX512X32:
	case RTE_ACL_CLASSIFY_AVX512X16:
	case RTE_ACL_CLASSIFY_AVX2:
	case RTE_ACL_CLASSIFY_SSE:
		return acl_check_alg_x86(alg);
	/* scalar method is supported on all platforms */
	case RTE_ACL_CLASSIFY_SCALAR:
		return 0;
	default:
		return -EINVAL;
	}
}

/*
 * Get preferred alg for given platform.
 */
static enum rte_acl_classify_alg
acl_get_best_alg(void)
{
	/*
	 * array of supported methods for each platform.
	 * Note that order is important - from most to less preferable.
	 */
	static const enum rte_acl_classify_alg alg[] = {
#if defined(RTE_ARCH_ARM)
		RTE_ACL_CLASSIFY_NEON,
#elif defined(RTE_ARCH_PPC_64)
		RTE_ACL_CLASSIFY_ALTIVEC,
#elif defined(RTE_ARCH_X86)
		RTE_ACL_CLASSIFY_AVX512X32,
		RTE_ACL_CLASSIFY_AVX512X16,
		RTE_ACL_CLASSIFY_AVX2,
		RTE_ACL_CLASSIFY_SSE,
#endif
		RTE_ACL_CLASSIFY_SCALAR,
	};

	uint32_t i;

	/* find best possible alg */
	for (i = 0; i != RTE_DIM(alg) && acl_check_alg(alg[i]) != 0; i++)
		;

	/* we always have to find something suitable */
	RTE_VERIFY(i != RTE_DIM(alg));
	return alg[i];
}

extern int
rte_acl_set_ctx_classify(struct rte_acl_ctx *ctx, enum rte_acl_classify_alg alg)
{
	int32_t rc;

	/* formal parameters check */
	if (ctx == NULL || (uint32_t)alg >= RTE_DIM(classify_fns))
		return -EINVAL;

	/* user asked us to select the *best* one */
	if (alg == RTE_ACL_CLASSIFY_DEFAULT)
		alg = acl_get_best_alg();

	/* check that given alg is supported */
	rc = acl_check_alg(alg);
	if (rc != 0)
		return rc;

	ctx->alg = alg;
	return 0;
}

int
rte_acl_classify_alg(const struct rte_acl_ctx *ctx, const uint8_t **data,
	uint32_t *results, uint32_t num, uint32_t categories,
	enum rte_acl_classify_alg alg)
{
	if (categories != 1 &&
			((RTE_ACL_RESULTS_MULTIPLIER - 1) & categories) != 0)
		return -EINVAL;

	return classify_fns[alg](ctx, data, results, num, categories);
}

int
rte_acl_classify(const struct rte_acl_ctx *ctx, const uint8_t **data,
	uint32_t *results, uint32_t num, uint32_t categories)
{
	return rte_acl_classify_alg(ctx, data, results, num, categories,
		ctx->alg);
}

struct rte_acl_ctx *
rte_acl_find_existing(const char *name)
{
	struct rte_acl_ctx *ctx = NULL;
	struct rte_acl_list *acl_list;
	struct rte_tailq_entry *te;

	acl_list = RTE_TAILQ_CAST(rte_acl_tailq.head, rte_acl_list);

	rte_mcfg_tailq_read_lock();
	TAILQ_FOREACH(te, acl_list, next) {
		ctx = (struct rte_acl_ctx *) te->data;
		if (strncmp(name, ctx->name, sizeof(ctx->name)) == 0)
			break;
	}
	rte_mcfg_tailq_read_unlock();

	if (te == NULL) {
		rte_errno = ENOENT;
		return NULL;
	}
	return ctx;
}

void
rte_acl_free(struct rte_acl_ctx *ctx)
{
	struct rte_acl_list *acl_list;
	struct rte_tailq_entry *te;

	if (ctx == NULL)
		return;

	acl_list = RTE_TAILQ_CAST(rte_acl_tailq.head, rte_acl_list);

	rte_mcfg_tailq_write_lock();

	/* find our tailq entry */
	TAILQ_FOREACH(te, acl_list, next) {
		if (te->data == (void *) ctx)
			break;
	}
	if (te == NULL) {
		rte_mcfg_tailq_write_unlock();
		return;
	}

	TAILQ_REMOVE(acl_list, te, next);

	rte_mcfg_tailq_write_unlock();

	rte_free(ctx->mem);
	rte_free(ctx);
	rte_free(te);
}

struct rte_acl_ctx *
rte_acl_create(const struct rte_acl_param *param)
{
	size_t sz;
	struct rte_acl_ctx *ctx;
	struct rte_acl_list *acl_list;
	struct rte_tailq_entry *te;
	char name[sizeof(ctx->name)];

	acl_list = RTE_TAILQ_CAST(rte_acl_tailq.head, rte_acl_list);

	/* check that input parameters are valid. */
	if (param == NULL || param->name == NULL) {
		rte_errno = EINVAL;
		return NULL;
	}

	snprintf(name, sizeof(name), "ACL_%s", param->name);

	/* calculate amount of memory required for pattern set. */
	sz = sizeof(*ctx) + param->max_rule_num * param->rule_size;

	/* get EAL TAILQ lock. */
	rte_mcfg_tailq_write_lock();

	/* if we already have one with that name */
	TAILQ_FOREACH(te, acl_list, next) {
		ctx = (struct rte_acl_ctx *) te->data;
		if (strncmp(param->name, ctx->name, sizeof(ctx->name)) == 0)
			break;
	}

	/* if ACL with such name doesn't exist, then create a new one. */
	if (te == NULL) {
		ctx = NULL;
		te = rte_zmalloc("ACL_TAILQ_ENTRY", sizeof(*te), 0);

		if (te == NULL) {
			RTE_LOG(ERR, ACL, "Cannot allocate tailq entry!\n");
			goto exit;
		}

		ctx = rte_zmalloc_socket(name, sz, RTE_CACHE_LINE_SIZE, param->socket_id);

		if (ctx == NULL) {
			RTE_LOG(ERR, ACL,
				"allocation of %zu bytes on socket %d for %s failed\n",
				sz, param->socket_id, name);
			rte_free(te);
			goto exit;
		}
		/* init new allocated context. */
		ctx->rules = ctx + 1;
		ctx->max_rules = param->max_rule_num;
		ctx->rule_sz = param->rule_size;
		ctx->socket_id = param->socket_id;
		ctx->alg = acl_get_best_alg();
		strlcpy(ctx->name, param->name, sizeof(ctx->name));

		te->data = (void *) ctx;

		TAILQ_INSERT_TAIL(acl_list, te, next);
	}

exit:
	rte_mcfg_tailq_write_unlock();
	return ctx;
}

static int
acl_add_rules(struct rte_acl_ctx *ctx, const void *rules, uint32_t num)
{
	uint8_t *pos;

	if (num + ctx->num_rules > ctx->max_rules)
		return -ENOMEM;

	pos = ctx->rules;
	pos += ctx->rule_sz * ctx->num_rules;
	memcpy(pos, rules, num * ctx->rule_sz);
	ctx->num_rules += num;

	return 0;
}

static int
acl_check_rule(const struct rte_acl_rule_data *rd)
{
	if ((RTE_LEN2MASK(RTE_ACL_MAX_CATEGORIES, typeof(rd->category_mask)) &
			rd->category_mask) == 0 ||
			rd->priority > RTE_ACL_MAX_PRIORITY ||
			rd->priority < RTE_ACL_MIN_PRIORITY)
		return -EINVAL;
	return 0;
}

int
rte_acl_add_rules(struct rte_acl_ctx *ctx, const struct rte_acl_rule *rules,
	uint32_t num)
{
	const struct rte_acl_rule *rv;
	uint32_t i;
	int32_t rc;

	if (ctx == NULL || rules == NULL || 0 == ctx->rule_sz)
		return -EINVAL;

	for (i = 0; i != num; i++) {
		rv = (const struct rte_acl_rule *)
			((uintptr_t)rules + i * ctx->rule_sz);
		rc = acl_check_rule(&rv->data);
		if (rc != 0) {
			RTE_LOG(ERR, ACL, "%s(%s): rule #%u is invalid\n",
				__func__, ctx->name, i + 1);
			return rc;
		}
	}

	return acl_add_rules(ctx, rules, num);
}

/*
 * Reset all rules.
 * Note that RT structures are not affected.
 */
void
rte_acl_reset_rules(struct rte_acl_ctx *ctx)
{
	if (ctx != NULL)
		ctx->num_rules = 0;
}

/*
 * Reset all rules and destroys RT structures.
 */
void
rte_acl_reset(struct rte_acl_ctx *ctx)
{
	if (ctx != NULL) {
		rte_acl_reset_rules(ctx);
		rte_acl_build(ctx, &ctx->config);
	}
}

/*
 * Dump ACL context to the stdout.
 */
void
rte_acl_dump(const struct rte_acl_ctx *ctx)
{
	if (!ctx)
		return;
	printf("acl context <%s>@%p\n", ctx->name, ctx);
	printf("  socket_id=%"PRId32"\n", ctx->socket_id);
	printf("  alg=%"PRId32"\n", ctx->alg);
	printf("  first_load_sz=%"PRIu32"\n", ctx->first_load_sz);
	printf("  max_rules=%"PRIu32"\n", ctx->max_rules);
	printf("  rule_size=%"PRIu32"\n", ctx->rule_sz);
	printf("  num_rules=%"PRIu32"\n", ctx->num_rules);
	printf("  num_categories=%"PRIu32"\n", ctx->num_categories);
	printf("  num_tries=%"PRIu32"\n", ctx->num_tries);
}

/*
 * Dump all ACL contexts to the stdout.
 */
void
rte_acl_list_dump(void)
{
	struct rte_acl_ctx *ctx;
	struct rte_acl_list *acl_list;
	struct rte_tailq_entry *te;

	acl_list = RTE_TAILQ_CAST(rte_acl_tailq.head, rte_acl_list);

	rte_mcfg_tailq_read_lock();
	TAILQ_FOREACH(te, acl_list, next) {
		ctx = (struct rte_acl_ctx *) te->data;
		rte_acl_dump(ctx);
	}
	rte_mcfg_tailq_read_unlock();
}
