/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#include <rte_acl.h>
#include "acl.h"

#define	BIT_SIZEOF(x)	(sizeof(x) * CHAR_BIT)

TAILQ_HEAD(rte_acl_list, rte_tailq_entry);

static struct rte_tailq_elem rte_acl_tailq = {
	.name = "RTE_ACL",
};
EAL_REGISTER_TAILQ(rte_acl_tailq)

/*
 * If the compiler doesn't support AVX2 instructions,
 * then the dummy one would be used instead for AVX2 classify method.
 */
int __attribute__ ((weak))
rte_acl_classify_avx2(__rte_unused const struct rte_acl_ctx *ctx,
	__rte_unused const uint8_t **data,
	__rte_unused uint32_t *results,
	__rte_unused uint32_t num,
	__rte_unused uint32_t categories)
{
	return -ENOTSUP;
}

static const rte_acl_classify_t classify_fns[] = {
	[RTE_ACL_CLASSIFY_DEFAULT] = rte_acl_classify_scalar,
	[RTE_ACL_CLASSIFY_SCALAR] = rte_acl_classify_scalar,
	[RTE_ACL_CLASSIFY_SSE] = rte_acl_classify_sse,
	[RTE_ACL_CLASSIFY_AVX2] = rte_acl_classify_avx2,
};

/* by default, use always available scalar code path. */
static enum rte_acl_classify_alg rte_acl_default_classify =
	RTE_ACL_CLASSIFY_SCALAR;

static void
rte_acl_set_default_classify(enum rte_acl_classify_alg alg)
{
	rte_acl_default_classify = alg;
}

extern int
rte_acl_set_ctx_classify(struct rte_acl_ctx *ctx, enum rte_acl_classify_alg alg)
{
	if (ctx == NULL || (uint32_t)alg >= RTE_DIM(classify_fns))
		return -EINVAL;

	ctx->alg = alg;
	return 0;
}

/*
 * Select highest available classify method as default one.
 * Note that CLASSIFY_AVX2 should be set as a default only
 * if both conditions are met:
 * at build time compiler supports AVX2 and target cpu supports AVX2.
 */
static void __attribute__((constructor))
rte_acl_init(void)
{
	enum rte_acl_classify_alg alg = RTE_ACL_CLASSIFY_DEFAULT;

#ifdef CC_AVX2_SUPPORT
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX2))
		alg = RTE_ACL_CLASSIFY_AVX2;
	else if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_SSE4_1))
#else
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_SSE4_1))
#endif
		alg = RTE_ACL_CLASSIFY_SSE;

	rte_acl_set_default_classify(alg);
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

	rte_rwlock_read_lock(RTE_EAL_TAILQ_RWLOCK);
	TAILQ_FOREACH(te, acl_list, next) {
		ctx = (struct rte_acl_ctx *) te->data;
		if (strncmp(name, ctx->name, sizeof(ctx->name)) == 0)
			break;
	}
	rte_rwlock_read_unlock(RTE_EAL_TAILQ_RWLOCK);

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

	rte_rwlock_write_lock(RTE_EAL_TAILQ_RWLOCK);

	/* find our tailq entry */
	TAILQ_FOREACH(te, acl_list, next) {
		if (te->data == (void *) ctx)
			break;
	}
	if (te == NULL) {
		rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);
		return;
	}

	TAILQ_REMOVE(acl_list, te, next);

	rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);

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
	rte_rwlock_write_lock(RTE_EAL_TAILQ_RWLOCK);

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
		ctx->alg = rte_acl_default_classify;
		snprintf(ctx->name, sizeof(ctx->name), "%s", param->name);

		te->data = (void *) ctx;

		TAILQ_INSERT_TAIL(acl_list, te, next);
	}

exit:
	rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);
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
	if ((rd->category_mask & LEN2MASK(RTE_ACL_MAX_CATEGORIES)) == 0 ||
			rd->priority > RTE_ACL_MAX_PRIORITY ||
			rd->priority < RTE_ACL_MIN_PRIORITY ||
			rd->userdata == RTE_ACL_INVALID_USERDATA)
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

	rte_rwlock_read_lock(RTE_EAL_TAILQ_RWLOCK);
	TAILQ_FOREACH(te, acl_list, next) {
		ctx = (struct rte_acl_ctx *) te->data;
		rte_acl_dump(ctx);
	}
	rte_rwlock_read_unlock(RTE_EAL_TAILQ_RWLOCK);
}

/*
 * Support for legacy ipv4vlan rules.
 */

RTE_ACL_RULE_DEF(acl_ipv4vlan_rule, RTE_ACL_IPV4VLAN_NUM_FIELDS);

static int
acl_ipv4vlan_check_rule(const struct rte_acl_ipv4vlan_rule *rule)
{
	if (rule->src_port_low > rule->src_port_high ||
			rule->dst_port_low > rule->dst_port_high ||
			rule->src_mask_len > BIT_SIZEOF(rule->src_addr) ||
			rule->dst_mask_len > BIT_SIZEOF(rule->dst_addr))
		return -EINVAL;

	return acl_check_rule(&rule->data);
}

static void
acl_ipv4vlan_convert_rule(const struct rte_acl_ipv4vlan_rule *ri,
	struct acl_ipv4vlan_rule *ro)
{
	ro->data = ri->data;

	ro->field[RTE_ACL_IPV4VLAN_PROTO_FIELD].value.u8 = ri->proto;
	ro->field[RTE_ACL_IPV4VLAN_VLAN1_FIELD].value.u16 = ri->vlan;
	ro->field[RTE_ACL_IPV4VLAN_VLAN2_FIELD].value.u16 = ri->domain;
	ro->field[RTE_ACL_IPV4VLAN_SRC_FIELD].value.u32 = ri->src_addr;
	ro->field[RTE_ACL_IPV4VLAN_DST_FIELD].value.u32 = ri->dst_addr;
	ro->field[RTE_ACL_IPV4VLAN_SRCP_FIELD].value.u16 = ri->src_port_low;
	ro->field[RTE_ACL_IPV4VLAN_DSTP_FIELD].value.u16 = ri->dst_port_low;

	ro->field[RTE_ACL_IPV4VLAN_PROTO_FIELD].mask_range.u8 = ri->proto_mask;
	ro->field[RTE_ACL_IPV4VLAN_VLAN1_FIELD].mask_range.u16 = ri->vlan_mask;
	ro->field[RTE_ACL_IPV4VLAN_VLAN2_FIELD].mask_range.u16 =
		ri->domain_mask;
	ro->field[RTE_ACL_IPV4VLAN_SRC_FIELD].mask_range.u32 =
		ri->src_mask_len;
	ro->field[RTE_ACL_IPV4VLAN_DST_FIELD].mask_range.u32 = ri->dst_mask_len;
	ro->field[RTE_ACL_IPV4VLAN_SRCP_FIELD].mask_range.u16 =
		ri->src_port_high;
	ro->field[RTE_ACL_IPV4VLAN_DSTP_FIELD].mask_range.u16 =
		ri->dst_port_high;
}

int
rte_acl_ipv4vlan_add_rules(struct rte_acl_ctx *ctx,
	const struct rte_acl_ipv4vlan_rule *rules,
	uint32_t num)
{
	int32_t rc;
	uint32_t i;
	struct acl_ipv4vlan_rule rv;

	if (ctx == NULL || rules == NULL || ctx->rule_sz != sizeof(rv))
		return -EINVAL;

	/* check input rules. */
	for (i = 0; i != num; i++) {
		rc = acl_ipv4vlan_check_rule(rules + i);
		if (rc != 0) {
			RTE_LOG(ERR, ACL, "%s(%s): rule #%u is invalid\n",
				__func__, ctx->name, i + 1);
			return rc;
		}
	}

	if (num + ctx->num_rules > ctx->max_rules)
		return -ENOMEM;

	/* perform conversion to the internal format and add to the context. */
	for (i = 0, rc = 0; i != num && rc == 0; i++) {
		acl_ipv4vlan_convert_rule(rules + i, &rv);
		rc = acl_add_rules(ctx, &rv, 1);
	}

	return rc;
}

static void
acl_ipv4vlan_config(struct rte_acl_config *cfg,
	const uint32_t layout[RTE_ACL_IPV4VLAN_NUM],
	uint32_t num_categories)
{
	static const struct rte_acl_field_def
		ipv4_defs[RTE_ACL_IPV4VLAN_NUM_FIELDS] = {
		{
			.type = RTE_ACL_FIELD_TYPE_BITMASK,
			.size = sizeof(uint8_t),
			.field_index = RTE_ACL_IPV4VLAN_PROTO_FIELD,
			.input_index = RTE_ACL_IPV4VLAN_PROTO,
		},
		{
			.type = RTE_ACL_FIELD_TYPE_BITMASK,
			.size = sizeof(uint16_t),
			.field_index = RTE_ACL_IPV4VLAN_VLAN1_FIELD,
			.input_index = RTE_ACL_IPV4VLAN_VLAN,
		},
		{
			.type = RTE_ACL_FIELD_TYPE_BITMASK,
			.size = sizeof(uint16_t),
			.field_index = RTE_ACL_IPV4VLAN_VLAN2_FIELD,
			.input_index = RTE_ACL_IPV4VLAN_VLAN,
		},
		{
			.type = RTE_ACL_FIELD_TYPE_MASK,
			.size = sizeof(uint32_t),
			.field_index = RTE_ACL_IPV4VLAN_SRC_FIELD,
			.input_index = RTE_ACL_IPV4VLAN_SRC,
		},
		{
			.type = RTE_ACL_FIELD_TYPE_MASK,
			.size = sizeof(uint32_t),
			.field_index = RTE_ACL_IPV4VLAN_DST_FIELD,
			.input_index = RTE_ACL_IPV4VLAN_DST,
		},
		{
			.type = RTE_ACL_FIELD_TYPE_RANGE,
			.size = sizeof(uint16_t),
			.field_index = RTE_ACL_IPV4VLAN_SRCP_FIELD,
			.input_index = RTE_ACL_IPV4VLAN_PORTS,
		},
		{
			.type = RTE_ACL_FIELD_TYPE_RANGE,
			.size = sizeof(uint16_t),
			.field_index = RTE_ACL_IPV4VLAN_DSTP_FIELD,
			.input_index = RTE_ACL_IPV4VLAN_PORTS,
		},
	};

	memcpy(&cfg->defs, ipv4_defs, sizeof(ipv4_defs));
	cfg->num_fields = RTE_DIM(ipv4_defs);

	cfg->defs[RTE_ACL_IPV4VLAN_PROTO_FIELD].offset =
		layout[RTE_ACL_IPV4VLAN_PROTO];
	cfg->defs[RTE_ACL_IPV4VLAN_VLAN1_FIELD].offset =
		layout[RTE_ACL_IPV4VLAN_VLAN];
	cfg->defs[RTE_ACL_IPV4VLAN_VLAN2_FIELD].offset =
		layout[RTE_ACL_IPV4VLAN_VLAN] +
		cfg->defs[RTE_ACL_IPV4VLAN_VLAN1_FIELD].size;
	cfg->defs[RTE_ACL_IPV4VLAN_SRC_FIELD].offset =
		layout[RTE_ACL_IPV4VLAN_SRC];
	cfg->defs[RTE_ACL_IPV4VLAN_DST_FIELD].offset =
		layout[RTE_ACL_IPV4VLAN_DST];
	cfg->defs[RTE_ACL_IPV4VLAN_SRCP_FIELD].offset =
		layout[RTE_ACL_IPV4VLAN_PORTS];
	cfg->defs[RTE_ACL_IPV4VLAN_DSTP_FIELD].offset =
		layout[RTE_ACL_IPV4VLAN_PORTS] +
		cfg->defs[RTE_ACL_IPV4VLAN_SRCP_FIELD].size;

	cfg->num_categories = num_categories;
}

int
rte_acl_ipv4vlan_build(struct rte_acl_ctx *ctx,
	const uint32_t layout[RTE_ACL_IPV4VLAN_NUM],
	uint32_t num_categories)
{
	struct rte_acl_config cfg;

	if (ctx == NULL || layout == NULL)
		return -EINVAL;

	memset(&cfg, 0, sizeof(cfg));
	acl_ipv4vlan_config(&cfg, layout, num_categories);
	return rte_acl_build(ctx, &cfg);
}
