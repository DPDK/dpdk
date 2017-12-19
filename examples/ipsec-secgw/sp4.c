/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
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
#include "parser.h"

#define MAX_ACL_RULE_NUM	1024

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

struct acl4_rules acl4_rules_out[MAX_ACL_RULE_NUM];
uint32_t nb_acl4_rules_out;

struct acl4_rules acl4_rules_in[MAX_ACL_RULE_NUM];
uint32_t nb_acl4_rules_in;

void
parse_sp4_tokens(char **tokens, uint32_t n_tokens,
	struct parse_status *status)
{
	struct acl4_rules *rule_ipv4 = NULL;

	uint32_t *ri = NULL; /* rule index */
	uint32_t ti = 0; /* token index */

	uint32_t esp_p = 0;
	uint32_t protect_p = 0;
	uint32_t bypass_p = 0;
	uint32_t discard_p = 0;
	uint32_t pri_p = 0;
	uint32_t src_p = 0;
	uint32_t dst_p = 0;
	uint32_t proto_p = 0;
	uint32_t sport_p = 0;
	uint32_t dport_p = 0;

	if (strcmp(tokens[1], "in") == 0) {
		ri = &nb_acl4_rules_in;

		APP_CHECK(*ri <= MAX_ACL_RULE_NUM - 1, status,
			"too many sp rules, abort insertion\n");
		if (status->status < 0)
			return;

		rule_ipv4 = &acl4_rules_in[*ri];

	} else if (strcmp(tokens[1], "out") == 0) {
		ri = &nb_acl4_rules_out;

		APP_CHECK(*ri <= MAX_ACL_RULE_NUM - 1, status,
			"too many sp rules, abort insertion\n");
		if (status->status < 0)
			return;

		rule_ipv4 = &acl4_rules_out[*ri];
	} else {
		APP_CHECK(0, status, "unrecognized input \"%s\", expect"
			" \"in\" or \"out\"\n", tokens[ti]);
		return;
	}

	rule_ipv4->data.category_mask = 1;

	for (ti = 2; ti < n_tokens; ti++) {
		if (strcmp(tokens[ti], "esp") == 0) {
			/* currently do nothing */
			APP_CHECK_PRESENCE(esp_p, tokens[ti], status);
			if (status->status < 0)
				return;
			esp_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "protect") == 0) {
			APP_CHECK_PRESENCE(protect_p, tokens[ti], status);
			if (status->status < 0)
				return;
			APP_CHECK(bypass_p == 0, status, "conflict item "
				"between \"%s\" and \"%s\"", tokens[ti],
				"bypass");
			if (status->status < 0)
				return;
			APP_CHECK(discard_p == 0, status, "conflict item "
				"between \"%s\" and \"%s\"", tokens[ti],
				"discard");
			if (status->status < 0)
				return;
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;
			APP_CHECK_TOKEN_IS_NUM(tokens, ti, status);
			if (status->status < 0)
				return;

			rule_ipv4->data.userdata =
				PROTECT(atoi(tokens[ti]));

			protect_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "bypass") == 0) {
			APP_CHECK_PRESENCE(bypass_p, tokens[ti], status);
			if (status->status < 0)
				return;
			APP_CHECK(protect_p == 0, status, "conflict item "
				"between \"%s\" and \"%s\"", tokens[ti],
				"protect");
			if (status->status < 0)
				return;
			APP_CHECK(discard_p == 0, status, "conflict item "
				"between \"%s\" and \"%s\"", tokens[ti],
				"discard");
			if (status->status < 0)
				return;

			rule_ipv4->data.userdata = BYPASS;

			bypass_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "discard") == 0) {
			APP_CHECK_PRESENCE(discard_p, tokens[ti], status);
			if (status->status < 0)
				return;
			APP_CHECK(protect_p == 0, status, "conflict item "
				"between \"%s\" and \"%s\"", tokens[ti],
				"protect");
			if (status->status < 0)
				return;
			APP_CHECK(bypass_p == 0, status, "conflict item "
				"between \"%s\" and \"%s\"", tokens[ti],
				"discard");
			if (status->status < 0)
				return;

			rule_ipv4->data.userdata = DISCARD;

			discard_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "pri") == 0) {
			APP_CHECK_PRESENCE(pri_p, tokens[ti], status);
			if (status->status < 0)
				return;
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;
			APP_CHECK_TOKEN_IS_NUM(tokens, ti, status);
			if (status->status < 0)
				return;

			rule_ipv4->data.priority = atoi(tokens[ti]);

			pri_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "src") == 0) {
			struct in_addr ip;
			uint32_t depth;

			APP_CHECK_PRESENCE(src_p, tokens[ti], status);
			if (status->status < 0)
				return;
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			APP_CHECK(parse_ipv4_addr(tokens[ti], &ip,
				&depth) == 0, status, "unrecognized "
				"input \"%s\", expect valid ipv4 addr",
				tokens[ti]);
			if (status->status < 0)
				return;

			rule_ipv4->field[1].value.u32 =
				rte_bswap32(ip.s_addr);
			rule_ipv4->field[1].mask_range.u32 =
				depth;

			src_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "dst") == 0) {
			struct in_addr ip;
			uint32_t depth;

			APP_CHECK_PRESENCE(dst_p, tokens[ti], status);
			if (status->status < 0)
				return;
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;
			APP_CHECK(parse_ipv4_addr(tokens[ti], &ip,
				&depth) == 0, status, "unrecognized "
				"input \"%s\", expect valid ipv4 addr",
				tokens[ti]);
			if (status->status < 0)
				return;

			rule_ipv4->field[2].value.u32 =
				rte_bswap32(ip.s_addr);
			rule_ipv4->field[2].mask_range.u32 =
				depth;

			dst_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "proto") == 0) {
			uint16_t low, high;

			APP_CHECK_PRESENCE(proto_p, tokens[ti], status);
			if (status->status < 0)
				return;
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			APP_CHECK(parse_range(tokens[ti], &low, &high)
				== 0, status, "unrecognized input \"%s\""
				", expect \"from:to\"", tokens[ti]);
			if (status->status < 0)
				return;
			APP_CHECK(low <= 0xff, status, "proto low "
				"over-limit");
			if (status->status < 0)
				return;
			APP_CHECK(high <= 0xff, status, "proto high "
				"over-limit");
			if (status->status < 0)
				return;

			rule_ipv4->field[0].value.u8 = (uint8_t)low;
			rule_ipv4->field[0].mask_range.u8 = (uint8_t)high;

			proto_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "sport") == 0) {
			uint16_t port_low, port_high;

			APP_CHECK_PRESENCE(sport_p, tokens[ti], status);
			if (status->status < 0)
				return;
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			APP_CHECK(parse_range(tokens[ti], &port_low,
				&port_high) == 0, status, "unrecognized "
				"input \"%s\", expect \"port_from:"
				"port_to\"", tokens[ti]);
			if (status->status < 0)
				return;

			rule_ipv4->field[3].value.u16 = port_low;
			rule_ipv4->field[3].mask_range.u16 = port_high;

			sport_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "dport") == 0) {
			uint16_t port_low, port_high;

			APP_CHECK_PRESENCE(dport_p, tokens[ti], status);
			if (status->status < 0)
				return;
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			APP_CHECK(parse_range(tokens[ti], &port_low,
				&port_high) == 0, status, "unrecognized "
				"input \"%s\", expect \"port_from:"
				"port_to\"", tokens[ti]);
			if (status->status < 0)
				return;

			rule_ipv4->field[4].value.u16 = port_low;
			rule_ipv4->field[4].mask_range.u16 = port_high;

			dport_p = 1;
			continue;
		}

		/* unrecognizeable input */
		APP_CHECK(0, status, "unrecognized input \"%s\"",
			tokens[ti]);
		return;
	}

	/* check if argument(s) are missing */
	APP_CHECK(esp_p == 1, status, "missing argument \"esp\"");
	if (status->status < 0)
		return;

	APP_CHECK(protect_p | bypass_p | discard_p, status, "missing "
		"argument \"protect\", \"bypass\", or \"discard\"");
	if (status->status < 0)
		return;

	*ri = *ri + 1;
}

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
sp4_init(struct socket_ctx *ctx, int32_t socket_id)
{
	const char *name;

	if (ctx == NULL)
		rte_exit(EXIT_FAILURE, "NULL context.\n");

	if (ctx->sp_ip4_in != NULL)
		rte_exit(EXIT_FAILURE, "Inbound SP DB for socket %u already "
				"initialized\n", socket_id);

	if (ctx->sp_ip4_out != NULL)
		rte_exit(EXIT_FAILURE, "Outbound SP DB for socket %u already "
				"initialized\n", socket_id);

	if (nb_acl4_rules_in > 0) {
		name = "sp_ip4_in";
		ctx->sp_ip4_in = (struct sp_ctx *)acl4_init(name,
			socket_id, acl4_rules_in, nb_acl4_rules_in);
	} else
		RTE_LOG(WARNING, IPSEC, "No IPv4 SP Inbound rule "
			"specified\n");

	if (nb_acl4_rules_out > 0) {
		name = "sp_ip4_out";
		ctx->sp_ip4_out = (struct sp_ctx *)acl4_init(name,
			socket_id, acl4_rules_out, nb_acl4_rules_out);
	} else
		RTE_LOG(WARNING, IPSEC, "No IPv4 SP Outbound rule "
			"specified\n");
}
