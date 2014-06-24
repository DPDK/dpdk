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

TAILQ_HEAD(rte_acl_list, rte_acl_ctx);

struct rte_acl_ctx *
rte_acl_find_existing(const char *name)
{
	struct rte_acl_ctx *ctx;
	struct rte_acl_list *acl_list;

	/* check that we have an initialised tail queue */
	acl_list = RTE_TAILQ_LOOKUP_BY_IDX(RTE_TAILQ_ACL, rte_acl_list);
	if (acl_list == NULL) {
		rte_errno = E_RTE_NO_TAILQ;
		return NULL;
	}

	rte_rwlock_read_lock(RTE_EAL_TAILQ_RWLOCK);
	TAILQ_FOREACH(ctx, acl_list, next) {
		if (strncmp(name, ctx->name, sizeof(ctx->name)) == 0)
			break;
	}
	rte_rwlock_read_unlock(RTE_EAL_TAILQ_RWLOCK);

	if (ctx == NULL)
		rte_errno = ENOENT;
	return ctx;
}

void
rte_acl_free(struct rte_acl_ctx *ctx)
{
	if (ctx == NULL)
		return;

	RTE_EAL_TAILQ_REMOVE(RTE_TAILQ_ACL, rte_acl_list, ctx);

	rte_free(ctx->mem);
	rte_free(ctx);
}

struct rte_acl_ctx *
rte_acl_create(const struct rte_acl_param *param)
{
	size_t sz;
	struct rte_acl_ctx *ctx;
	struct rte_acl_list *acl_list;
	char name[sizeof(ctx->name)];

	/* check that we have an initialised tail queue */
	acl_list = RTE_TAILQ_LOOKUP_BY_IDX(RTE_TAILQ_ACL, rte_acl_list);
	if (acl_list == NULL) {
		rte_errno = E_RTE_NO_TAILQ;
		return NULL;
	}

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
	TAILQ_FOREACH(ctx, acl_list, next) {
		if (strncmp(param->name, ctx->name, sizeof(ctx->name)) == 0)
			break;
	}

	/* if ACL with such name doesn't exist, then create a new one. */
	if (ctx == NULL && (ctx = rte_zmalloc_socket(name, sz, CACHE_LINE_SIZE,
			param->socket_id)) != NULL) {

		/* init new allocated context. */
		ctx->rules = ctx + 1;
		ctx->max_rules = param->max_rule_num;
		ctx->rule_sz = param->rule_size;
		ctx->socket_id = param->socket_id;
		snprintf(ctx->name, sizeof(ctx->name), "%s", param->name);

		TAILQ_INSERT_TAIL(acl_list, ctx, next);

	} else if (ctx == NULL) {
		RTE_LOG(ERR, ACL,
			"allocation of %zu bytes on socket %d for %s failed\n",
			sz, param->socket_id, name);
	}

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

	/* check that we have an initialised tail queue */
	acl_list = RTE_TAILQ_LOOKUP_BY_IDX(RTE_TAILQ_ACL, rte_acl_list);
	if (acl_list == NULL) {
		rte_errno = E_RTE_NO_TAILQ;
		return;
	}

	rte_rwlock_read_lock(RTE_EAL_TAILQ_RWLOCK);
	TAILQ_FOREACH(ctx, acl_list, next) {
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

	acl_ipv4vlan_config(&cfg, layout, num_categories);
	return rte_acl_build(ctx, &cfg);
}
