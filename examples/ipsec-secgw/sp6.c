/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
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

/*
 * Security Policies
 */
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip6.h>

#include <rte_acl.h>
#include <rte_ip.h>

#include "ipsec.h"

#define MAX_ACL_RULE_NUM	1000

enum {
	IP6_PROTO,
	IP6_SRC0,
	IP6_SRC1,
	IP6_SRC2,
	IP6_SRC3,
	IP6_DST0,
	IP6_DST1,
	IP6_DST2,
	IP6_DST3,
	IP6_SRCP,
	IP6_DSTP,
	IP6_NUM
};

#define IP6_ADDR_SIZE 16

struct rte_acl_field_def ip6_defs[IP6_NUM] = {
	{
	.type = RTE_ACL_FIELD_TYPE_BITMASK,
	.size = sizeof(uint8_t),
	.field_index = IP6_PROTO,
	.input_index = IP6_PROTO,
	.offset = 0,
	},
	{
	.type = RTE_ACL_FIELD_TYPE_MASK,
	.size = 4,
	.field_index = IP6_SRC0,
	.input_index = IP6_SRC0,
	.offset = 2
	},
	{
	.type = RTE_ACL_FIELD_TYPE_MASK,
	.size = 4,
	.field_index = IP6_SRC1,
	.input_index = IP6_SRC1,
	.offset = 6
	},
	{
	.type = RTE_ACL_FIELD_TYPE_MASK,
	.size = 4,
	.field_index = IP6_SRC2,
	.input_index = IP6_SRC2,
	.offset = 10
	},
	{
	.type = RTE_ACL_FIELD_TYPE_MASK,
	.size = 4,
	.field_index = IP6_SRC3,
	.input_index = IP6_SRC3,
	.offset = 14
	},
	{
	.type = RTE_ACL_FIELD_TYPE_MASK,
	.size = 4,
	.field_index = IP6_DST0,
	.input_index = IP6_DST0,
	.offset = 18
	},
	{
	.type = RTE_ACL_FIELD_TYPE_MASK,
	.size = 4,
	.field_index = IP6_DST1,
	.input_index = IP6_DST1,
	.offset = 22
	},
	{
	.type = RTE_ACL_FIELD_TYPE_MASK,
	.size = 4,
	.field_index = IP6_DST2,
	.input_index = IP6_DST2,
	.offset = 26
	},
	{
	.type = RTE_ACL_FIELD_TYPE_MASK,
	.size = 4,
	.field_index = IP6_DST3,
	.input_index = IP6_DST3,
	.offset = 30
	},
	{
	.type = RTE_ACL_FIELD_TYPE_RANGE,
	.size = sizeof(uint16_t),
	.field_index = IP6_SRCP,
	.input_index = IP6_SRCP,
	.offset = 34
	},
	{
	.type = RTE_ACL_FIELD_TYPE_RANGE,
	.size = sizeof(uint16_t),
	.field_index = IP6_DSTP,
	.input_index = IP6_SRCP,
	.offset = 36
	}
};

RTE_ACL_RULE_DEF(acl6_rules, RTE_DIM(ip6_defs));

const struct acl6_rules acl6_rules_out[] = {
	{
	.data = {.userdata = PROTECT(5), .category_mask = 1, .priority = 1},
	/* destination IPv6 */
	.field[5] = {.value.u32 = 0x0, .mask_range.u32 = 32,},
	.field[6] = {.value.u32 = 0x0, .mask_range.u32 = 32,},
	.field[7] = {.value.u32 = 0x55555555, .mask_range.u32 = 32,},
	.field[8] = {.value.u32 = 0x0, .mask_range.u32 = 0,},
	/* source port */
	.field[9] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[10] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(6), .category_mask = 1, .priority = 1},
	/* destination IPv6 */
	.field[5] = {.value.u32 = 0x0, .mask_range.u32 = 32,},
	.field[6] = {.value.u32 = 0x0, .mask_range.u32 = 32,},
	.field[7] = {.value.u32 = 0x66666666, .mask_range.u32 = 32,},
	.field[8] = {.value.u32 = 0x0, .mask_range.u32 = 0,},
	/* source port */
	.field[9] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[10] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(10), .category_mask = 1, .priority = 1},
	/* destination IPv6 */
	.field[5] = {.value.u32 = 0x0, .mask_range.u32 = 32,},
	.field[6] = {.value.u32 = 0x11111111, .mask_range.u32 = 32,},
	.field[7] = {.value.u32 = 0x00000000, .mask_range.u32 = 32,},
	.field[8] = {.value.u32 = 0x0, .mask_range.u32 = 0,},
	/* source port */
	.field[9] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[10] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(11), .category_mask = 1, .priority = 1},
	/* destination IPv6 */
	.field[5] = {.value.u32 = 0x0, .mask_range.u32 = 32,},
	.field[6] = {.value.u32 = 0x11111111, .mask_range.u32 = 32,},
	.field[7] = {.value.u32 = 0x11111111, .mask_range.u32 = 32,},
	.field[8] = {.value.u32 = 0x0, .mask_range.u32 = 0,},
	/* source port */
	.field[9] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[10] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(25), .category_mask = 1, .priority = 1},
	/* destination IPv6 */
	.field[5] = {.value.u32 = 0x0, .mask_range.u32 = 32,},
	.field[6] = {.value.u32 = 0x0, .mask_range.u32 = 32,},
	.field[7] = {.value.u32 = 0xaaaaaaaa, .mask_range.u32 = 32,},
	.field[8] = {.value.u32 = 0x0, .mask_range.u32 = 0,},
	/* source port */
	.field[9] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[10] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(26), .category_mask = 1, .priority = 1},
	/* destination IPv6 */
	.field[5] = {.value.u32 = 0x0, .mask_range.u32 = 32,},
	.field[6] = {.value.u32 = 0x0, .mask_range.u32 = 32,},
	.field[7] = {.value.u32 = 0xbbbbbbbb, .mask_range.u32 = 32,},
	.field[8] = {.value.u32 = 0x0, .mask_range.u32 = 0,},
	/* source port */
	.field[9] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[10] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	}
};

const struct acl6_rules acl6_rules_in[] = {
	{
	.data = {.userdata = PROTECT(15), .category_mask = 1, .priority = 1},
	/* destination IPv6 */
	.field[5] = {.value.u32 = 0xffff0000, .mask_range.u32 = 32,},
	.field[6] = {.value.u32 = 0x0, .mask_range.u32 = 32,},
	.field[7] = {.value.u32 = 0x55555555, .mask_range.u32 = 32,},
	.field[8] = {.value.u32 = 0x0, .mask_range.u32 = 0,},
	/* source port */
	.field[9] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[10] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(16), .category_mask = 1, .priority = 1},
	/* destination IPv6 */
	.field[5] = {.value.u32 = 0xffff0000, .mask_range.u32 = 32,},
	.field[6] = {.value.u32 = 0x0, .mask_range.u32 = 32,},
	.field[7] = {.value.u32 = 0x66666666, .mask_range.u32 = 32,},
	.field[8] = {.value.u32 = 0x0, .mask_range.u32 = 0,},
	/* source port */
	.field[9] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[10] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(110), .category_mask = 1, .priority = 1},
	/* destination IPv6 */
	.field[5] = {.value.u32 = 0xffff0000, .mask_range.u32 = 32,},
	.field[6] = {.value.u32 = 0x11111111, .mask_range.u32 = 32,},
	.field[7] = {.value.u32 = 0x00000000, .mask_range.u32 = 32,},
	.field[8] = {.value.u32 = 0x0, .mask_range.u32 = 0,},
	/* source port */
	.field[9] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[10] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(111), .category_mask = 1, .priority = 1},
	/* destination IPv6 */
	.field[5] = {.value.u32 = 0xffff0000, .mask_range.u32 = 32,},
	.field[6] = {.value.u32 = 0x11111111, .mask_range.u32 = 32,},
	.field[7] = {.value.u32 = 0x11111111, .mask_range.u32 = 32,},
	.field[8] = {.value.u32 = 0x0, .mask_range.u32 = 0,},
	/* source port */
	.field[9] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[10] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(125), .category_mask = 1, .priority = 1},
	/* destination IPv6 */
	.field[5] = {.value.u32 = 0xffff0000, .mask_range.u32 = 32,},
	.field[6] = {.value.u32 = 0x0, .mask_range.u32 = 32,},
	.field[7] = {.value.u32 = 0xaaaaaaaa, .mask_range.u32 = 32,},
	.field[8] = {.value.u32 = 0x0, .mask_range.u32 = 0,},
	/* source port */
	.field[9] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[10] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(126), .category_mask = 1, .priority = 1},
	/* destination IPv6 */
	.field[5] = {.value.u32 = 0xffff0000, .mask_range.u32 = 32,},
	.field[6] = {.value.u32 = 0x0, .mask_range.u32 = 32,},
	.field[7] = {.value.u32 = 0xbbbbbbbb, .mask_range.u32 = 32,},
	.field[8] = {.value.u32 = 0x0, .mask_range.u32 = 0,},
	/* source port */
	.field[9] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[10] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	}
};

static inline void
print_one_ip6_rule(const struct acl6_rules *rule, int32_t extra)
{
	uint8_t a, b, c, d;

	uint32_t_to_char(rule->field[IP6_SRC0].value.u32,
		&a, &b, &c, &d);
	printf("%.2x%.2x:%.2x%.2x", a, b, c, d);
	uint32_t_to_char(rule->field[IP6_SRC1].value.u32,
		&a, &b, &c, &d);
	printf(":%.2x%.2x:%.2x%.2x", a, b, c, d);
	uint32_t_to_char(rule->field[IP6_SRC2].value.u32,
		&a, &b, &c, &d);
	printf(":%.2x%.2x:%.2x%.2x", a, b, c, d);
	uint32_t_to_char(rule->field[IP6_SRC3].value.u32,
		&a, &b, &c, &d);
	printf(":%.2x%.2x:%.2x%.2x/%u ", a, b, c, d,
			rule->field[IP6_SRC0].mask_range.u32
			+ rule->field[IP6_SRC1].mask_range.u32
			+ rule->field[IP6_SRC2].mask_range.u32
			+ rule->field[IP6_SRC3].mask_range.u32);

	uint32_t_to_char(rule->field[IP6_DST0].value.u32,
		&a, &b, &c, &d);
	printf("%.2x%.2x:%.2x%.2x", a, b, c, d);
	uint32_t_to_char(rule->field[IP6_DST1].value.u32,
		&a, &b, &c, &d);
	printf(":%.2x%.2x:%.2x%.2x", a, b, c, d);
	uint32_t_to_char(rule->field[IP6_DST2].value.u32,
		&a, &b, &c, &d);
	printf(":%.2x%.2x:%.2x%.2x", a, b, c, d);
	uint32_t_to_char(rule->field[IP6_DST3].value.u32,
		&a, &b, &c, &d);
	printf(":%.2x%.2x:%.2x%.2x/%u ", a, b, c, d,
			rule->field[IP6_DST0].mask_range.u32
			+ rule->field[IP6_DST1].mask_range.u32
			+ rule->field[IP6_DST2].mask_range.u32
			+ rule->field[IP6_DST3].mask_range.u32);

	printf("%hu : %hu %hu : %hu 0x%hhx/0x%hhx ",
		rule->field[IP6_SRCP].value.u16,
		rule->field[IP6_SRCP].mask_range.u16,
		rule->field[IP6_DSTP].value.u16,
		rule->field[IP6_DSTP].mask_range.u16,
		rule->field[IP6_PROTO].value.u8,
		rule->field[IP6_PROTO].mask_range.u8);
	if (extra)
		printf("0x%x-0x%x-0x%x ",
			rule->data.category_mask,
			rule->data.priority,
			rule->data.userdata);
}

static inline void
dump_ip6_rules(const struct acl6_rules *rule, int32_t num, int32_t extra)
{
	int32_t i;

	for (i = 0; i < num; i++, rule++) {
		printf("\t%d:", i + 1);
		print_one_ip6_rule(rule, extra);
		printf("\n");
	}
}

static struct rte_acl_ctx *
acl6_init(const char *name, int32_t socketid, const struct acl6_rules *rules,
		uint32_t rules_nb)
{
	char s[PATH_MAX];
	struct rte_acl_param acl_param;
	struct rte_acl_config acl_build_param;
	struct rte_acl_ctx *ctx;

	printf("Creating SP context with %u max rules\n", MAX_ACL_RULE_NUM);

	memset(&acl_param, 0, sizeof(acl_param));

	/* Create ACL contexts */
	snprintf(s, sizeof(s), "%s_%d", name, socketid);

	printf("IPv4 %s entries [%u]:\n", s, rules_nb);
	dump_ip6_rules(rules, rules_nb, 1);

	acl_param.name = s;
	acl_param.socket_id = socketid;
	acl_param.rule_size = RTE_ACL_RULE_SZ(RTE_DIM(ip6_defs));
	acl_param.max_rule_num = MAX_ACL_RULE_NUM;

	ctx = rte_acl_create(&acl_param);
	if (ctx == NULL)
		rte_exit(EXIT_FAILURE, "Failed to create ACL context\n");

	if (rte_acl_add_rules(ctx, (const struct rte_acl_rule *)rules,
				rules_nb) < 0)
		rte_exit(EXIT_FAILURE, "add rules failed\n");

	/* Perform builds */
	memset(&acl_build_param, 0, sizeof(acl_build_param));

	acl_build_param.num_categories = DEFAULT_MAX_CATEGORIES;
	acl_build_param.num_fields = RTE_DIM(ip6_defs);
	memcpy(&acl_build_param.defs, ip6_defs, sizeof(ip6_defs));

	if (rte_acl_build(ctx, &acl_build_param) != 0)
		rte_exit(EXIT_FAILURE, "Failed to build ACL trie\n");

	rte_acl_dump(ctx);

	return ctx;
}

void
sp6_init(struct socket_ctx *ctx, int32_t socket_id, uint32_t ep)
{
	const char *name;
	const struct acl6_rules *rules_out, *rules_in;
	uint32_t nb_out_rules, nb_in_rules;

	if (ctx == NULL)
		rte_exit(EXIT_FAILURE, "NULL context.\n");

	if (ctx->sp_ip6_in != NULL)
		rte_exit(EXIT_FAILURE, "Inbound IPv6 SP DB for socket %u "
				"already initialized\n", socket_id);

	if (ctx->sp_ip6_out != NULL)
		rte_exit(EXIT_FAILURE, "Outbound IPv6 SP DB for socket %u "
				"already initialized\n", socket_id);

	if (ep == 0) {
		rules_out = acl6_rules_out;
		nb_out_rules = RTE_DIM(acl6_rules_out);
		rules_in = acl6_rules_in;
		nb_in_rules = RTE_DIM(acl6_rules_in);
	} else if (ep == 1) {
		rules_out = acl6_rules_in;
		nb_out_rules = RTE_DIM(acl6_rules_in);
		rules_in = acl6_rules_out;
		nb_in_rules = RTE_DIM(acl6_rules_out);
	} else
		rte_exit(EXIT_FAILURE, "Invalid EP value %u. "
				"Only 0 or 1 supported.\n", ep);

	name = "sp_ip6_in";
	ctx->sp_ip6_in = (struct sp_ctx *)acl6_init(name, socket_id,
			rules_in, nb_in_rules);

	name = "sp_ip6_out";
	ctx->sp_ip6_out = (struct sp_ctx *)acl6_init(name, socket_id,
			rules_out, nb_out_rules);
}
