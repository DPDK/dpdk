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
#include <netinet/ip.h>

#include <rte_acl.h>
#include <rte_ip.h>

#include "ipsec.h"

#define MAX_ACL_RULE_NUM	1000

/*
 * Rule and trace formats definitions.
 */
enum {
	PROTO_FIELD_IPV4,
	SRC_FIELD_IPV4,
	DST_FIELD_IPV4,
	SRCP_FIELD_IPV4,
	DSTP_FIELD_IPV4,
	NUM_FIELDS_IPV4
};

/*
 * That effectively defines order of IPV4 classifications:
 *  - PROTO
 *  - SRC IP ADDRESS
 *  - DST IP ADDRESS
 *  - PORTS (SRC and DST)
 */
enum {
	RTE_ACL_IPV4_PROTO,
	RTE_ACL_IPV4_SRC,
	RTE_ACL_IPV4_DST,
	RTE_ACL_IPV4_PORTS,
	RTE_ACL_IPV4_NUM
};

struct rte_acl_field_def ip4_defs[NUM_FIELDS_IPV4] = {
	{
	.type = RTE_ACL_FIELD_TYPE_BITMASK,
	.size = sizeof(uint8_t),
	.field_index = PROTO_FIELD_IPV4,
	.input_index = RTE_ACL_IPV4_PROTO,
	.offset = 0,
	},
	{
	.type = RTE_ACL_FIELD_TYPE_MASK,
	.size = sizeof(uint32_t),
	.field_index = SRC_FIELD_IPV4,
	.input_index = RTE_ACL_IPV4_SRC,
	.offset = offsetof(struct ip, ip_src) -	offsetof(struct ip, ip_p)
	},
	{
	.type = RTE_ACL_FIELD_TYPE_MASK,
	.size = sizeof(uint32_t),
	.field_index = DST_FIELD_IPV4,
	.input_index = RTE_ACL_IPV4_DST,
	.offset = offsetof(struct ip, ip_dst) - offsetof(struct ip, ip_p)
	},
	{
	.type = RTE_ACL_FIELD_TYPE_RANGE,
	.size = sizeof(uint16_t),
	.field_index = SRCP_FIELD_IPV4,
	.input_index = RTE_ACL_IPV4_PORTS,
	.offset = sizeof(struct ip) - offsetof(struct ip, ip_p)
	},
	{
	.type = RTE_ACL_FIELD_TYPE_RANGE,
	.size = sizeof(uint16_t),
	.field_index = DSTP_FIELD_IPV4,
	.input_index = RTE_ACL_IPV4_PORTS,
	.offset = sizeof(struct ip) - offsetof(struct ip, ip_p) +
		sizeof(uint16_t)
	},
};

RTE_ACL_RULE_DEF(acl4_rules, RTE_DIM(ip4_defs));

const struct acl4_rules acl4_rules_out[] = {
	{
	.data = {.userdata = PROTECT(5), .category_mask = 1, .priority = 1},
	/* destination IPv4 */
	.field[2] = {.value.u32 = IPv4(192, 168, 105, 0),
				.mask_range.u32 = 24,},
	/* source port */
	.field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(6), .category_mask = 1, .priority = 1},
	/* destination IPv4 */
	.field[2] = {.value.u32 = IPv4(192, 168, 106, 0),
				.mask_range.u32 = 24,},
	/* source port */
	.field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(10), .category_mask = 1, .priority = 1},
	/* destination IPv4 */
	.field[2] = {.value.u32 = IPv4(192, 168, 175, 0),
				.mask_range.u32 = 24,},
	/* source port */
	.field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(11), .category_mask = 1, .priority = 1},
	/* destination IPv4 */
	.field[2] = {.value.u32 = IPv4(192, 168, 176, 0),
				.mask_range.u32 = 24,},
	/* source port */
	.field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(15), .category_mask = 1, .priority = 1},
	/* destination IPv4 */
	.field[2] = {.value.u32 = IPv4(192, 168, 200, 0),
				.mask_range.u32 = 24,},
	/* source port */
	.field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(16), .category_mask = 1, .priority = 1},
	/* destination IPv4 */
	.field[2] = {.value.u32 = IPv4(192, 168, 201, 0),
				.mask_range.u32 = 24,},
	/* source port */
	.field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(25), .category_mask = 1, .priority = 1},
	/* destination IPv4 */
	.field[2] = {.value.u32 = IPv4(192, 168, 55, 0),
				.mask_range.u32 = 24,},
	/* source port */
	.field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(26), .category_mask = 1, .priority = 1},
	/* destination IPv4 */
	.field[2] = {.value.u32 = IPv4(192, 168, 56, 0),
				.mask_range.u32 = 24,},
	/* source port */
	.field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = BYPASS, .category_mask = 1, .priority = 1},
	/* destination IPv4 */
	.field[2] = {.value.u32 = IPv4(192, 168, 240, 0),
				.mask_range.u32 = 24,},
	/* source port */
	.field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = BYPASS, .category_mask = 1, .priority = 1},
	/* destination IPv4 */
	.field[2] = {.value.u32 = IPv4(192, 168, 241, 0),
				.mask_range.u32 = 24,},
	/* source port */
	.field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	}
};

const struct acl4_rules acl4_rules_in[] = {
	{
	.data = {.userdata = PROTECT(105), .category_mask = 1, .priority = 1},
	/* destination IPv4 */
	.field[2] = {.value.u32 = IPv4(192, 168, 115, 0),
				.mask_range.u32 = 24,},
	/* source port */
	.field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(106), .category_mask = 1, .priority = 1},
	/* destination IPv4 */
	.field[2] = {.value.u32 = IPv4(192, 168, 116, 0),
				.mask_range.u32 = 24,},
	/* source port */
	.field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(110), .category_mask = 1, .priority = 1},
	/* destination IPv4 */
	.field[2] = {.value.u32 = IPv4(192, 168, 185, 0),
				.mask_range.u32 = 24,},
	/* source port */
	.field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(111), .category_mask = 1, .priority = 1},
	/* destination IPv4 */
	.field[2] = {.value.u32 = IPv4(192, 168, 186, 0),
				.mask_range.u32 = 24,},
	/* source port */
	.field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(115), .category_mask = 1, .priority = 1},
	/* destination IPv4 */
	.field[2] = {.value.u32 = IPv4(192, 168, 210, 0),
				.mask_range.u32 = 24,},
	/* source port */
	.field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(116), .category_mask = 1, .priority = 1},
	/* destination IPv4 */
	.field[2] = {.value.u32 = IPv4(192, 168, 211, 0),
				.mask_range.u32 = 24,},
	/* source port */
	.field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(125), .category_mask = 1, .priority = 1},
	/* destination IPv4 */
	.field[2] = {.value.u32 = IPv4(192, 168, 65, 0),
				.mask_range.u32 = 24,},
	/* source port */
	.field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = PROTECT(126), .category_mask = 1, .priority = 1},
	/* destination IPv4 */
	.field[2] = {.value.u32 = IPv4(192, 168, 66, 0),
				.mask_range.u32 = 24,},
	/* source port */
	.field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = BYPASS, .category_mask = 1, .priority = 1},
	/* destination IPv4 */
	.field[2] = {.value.u32 = IPv4(192, 168, 245, 0),
				.mask_range.u32 = 24,},
	/* source port */
	.field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	},
	{
	.data = {.userdata = BYPASS, .category_mask = 1, .priority = 1},
	/* destination IPv4 */
	.field[2] = {.value.u32 = IPv4(192, 168, 246, 0),
				.mask_range.u32 = 24,},
	/* source port */
	.field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
	/* destination port */
	.field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,}
	}
};

static void
print_one_ip4_rule(const struct acl4_rules *rule, int32_t extra)
{
	uint8_t a, b, c, d;

	uint32_t_to_char(rule->field[SRC_FIELD_IPV4].value.u32,
			&a, &b, &c, &d);
	printf("%hhu.%hhu.%hhu.%hhu/%u ", a, b, c, d,
			rule->field[SRC_FIELD_IPV4].mask_range.u32);
	uint32_t_to_char(rule->field[DST_FIELD_IPV4].value.u32,
			&a, &b, &c, &d);
	printf("%hhu.%hhu.%hhu.%hhu/%u ", a, b, c, d,
			rule->field[DST_FIELD_IPV4].mask_range.u32);
	printf("%hu : %hu %hu : %hu 0x%hhx/0x%hhx ",
		rule->field[SRCP_FIELD_IPV4].value.u16,
		rule->field[SRCP_FIELD_IPV4].mask_range.u16,
		rule->field[DSTP_FIELD_IPV4].value.u16,
		rule->field[DSTP_FIELD_IPV4].mask_range.u16,
		rule->field[PROTO_FIELD_IPV4].value.u8,
		rule->field[PROTO_FIELD_IPV4].mask_range.u8);
	if (extra)
		printf("0x%x-0x%x-0x%x ",
			rule->data.category_mask,
			rule->data.priority,
			rule->data.userdata);
}

static inline void
dump_ip4_rules(const struct acl4_rules *rule, int32_t num, int32_t extra)
{
	int32_t i;

	for (i = 0; i < num; i++, rule++) {
		printf("\t%d:", i + 1);
		print_one_ip4_rule(rule, extra);
		printf("\n");
	}
}

static struct rte_acl_ctx *
acl4_init(const char *name, int32_t socketid, const struct acl4_rules *rules,
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
	dump_ip4_rules(rules, rules_nb, 1);

	acl_param.name = s;
	acl_param.socket_id = socketid;
	acl_param.rule_size = RTE_ACL_RULE_SZ(RTE_DIM(ip4_defs));
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
	acl_build_param.num_fields = RTE_DIM(ip4_defs);
	memcpy(&acl_build_param.defs, ip4_defs, sizeof(ip4_defs));

	if (rte_acl_build(ctx, &acl_build_param) != 0)
		rte_exit(EXIT_FAILURE, "Failed to build ACL trie\n");

	rte_acl_dump(ctx);

	return ctx;
}

void
sp4_init(struct socket_ctx *ctx, int32_t socket_id, uint32_t ep)
{
	const char *name;
	const struct acl4_rules *rules_out, *rules_in;
	uint32_t nb_out_rules, nb_in_rules;

	if (ctx == NULL)
		rte_exit(EXIT_FAILURE, "NULL context.\n");

	if (ctx->sp_ip4_in != NULL)
		rte_exit(EXIT_FAILURE, "Inbound SP DB for socket %u already "
				"initialized\n", socket_id);

	if (ctx->sp_ip4_out != NULL)
		rte_exit(EXIT_FAILURE, "Outbound SP DB for socket %u already "
				"initialized\n", socket_id);

	if (ep == 0) {
		rules_out = acl4_rules_out;
		nb_out_rules = RTE_DIM(acl4_rules_out);
		rules_in = acl4_rules_in;
		nb_in_rules = RTE_DIM(acl4_rules_in);
	} else if (ep == 1) {
		rules_out = acl4_rules_in;
		nb_out_rules = RTE_DIM(acl4_rules_in);
		rules_in = acl4_rules_out;
		nb_in_rules = RTE_DIM(acl4_rules_out);
	} else
		rte_exit(EXIT_FAILURE, "Invalid EP value %u. "
				"Only 0 or 1 supported.\n", ep);

	name = "sp_ip4_in";
	ctx->sp_ip4_in = (struct sp_ctx *)acl4_init(name, socket_id,
			rules_in, nb_in_rules);

	name = "sp_ip4_out";
	ctx->sp_ip4_out = (struct sp_ctx *)acl4_init(name, socket_id,
			rules_out, nb_out_rules);
}
