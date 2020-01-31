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

#define INIT_ACL_RULE_NUM	128

#define IPV4_DST_FROM_SP(acr) \
		(rte_cpu_to_be_32((acr).field[DST_FIELD_IPV4].value.u32))

#define IPV4_SRC_FROM_SP(acr) \
		(rte_cpu_to_be_32((acr).field[SRC_FIELD_IPV4].value.u32))

#define IPV4_DST_MASK_FROM_SP(acr) \
		((acr).field[DST_FIELD_IPV4].mask_range.u32)

#define IPV4_SRC_MASK_FROM_SP(acr) \
		((acr).field[SRC_FIELD_IPV4].mask_range.u32)

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

static struct rte_acl_field_def ip4_defs[NUM_FIELDS_IPV4] = {
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

static struct acl4_rules *acl4_rules_out;
static uint32_t nb_acl4_rules_out;
static uint32_t sp_out_sz;

static struct acl4_rules *acl4_rules_in;
static uint32_t nb_acl4_rules_in;
static uint32_t sp_in_sz;

static int
extend_sp_arr(struct acl4_rules **sp_tbl, uint32_t cur_cnt, uint32_t *cur_sz)
{
	if (*sp_tbl == NULL) {
		*sp_tbl = calloc(INIT_ACL_RULE_NUM, sizeof(struct acl4_rules));
		if (*sp_tbl == NULL)
			return -1;
		*cur_sz = INIT_ACL_RULE_NUM;
		return 0;
	}

	if (cur_cnt >= *cur_sz) {
		*sp_tbl = realloc(*sp_tbl,
			*cur_sz * sizeof(struct acl4_rules) * 2);
		if (*sp_tbl == NULL)
			return -1;
		/* clean reallocated extra space */
		memset(&(*sp_tbl)[*cur_sz], 0,
			*cur_sz * sizeof(struct acl4_rules));
		*cur_sz *= 2;
	}

	return 0;
}


void
parse_sp4_tokens(char **tokens, uint32_t n_tokens,
	struct parse_status *status)
{
	struct acl4_rules *rule_ipv4 = NULL;

	uint32_t *ri = NULL; /* rule index */
	uint32_t ti = 0; /* token index */
	uint32_t tv;

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

		if (extend_sp_arr(&acl4_rules_in, nb_acl4_rules_in,
				&sp_in_sz) < 0)
			return;

		rule_ipv4 = &acl4_rules_in[*ri];

	} else if (strcmp(tokens[1], "out") == 0) {
		ri = &nb_acl4_rules_out;

		if (extend_sp_arr(&acl4_rules_out, nb_acl4_rules_out,
				&sp_out_sz) < 0)
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

			tv = atoi(tokens[ti]);
			APP_CHECK(tv != DISCARD && tv != BYPASS, status,
				"invalid SPI: %s", tokens[ti]);
			if (status->status < 0)
				return;
			rule_ipv4->data.userdata = tv;

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

	printf("Creating SP context with %u rules\n", rules_nb);

	memset(&acl_param, 0, sizeof(acl_param));

	/* Create ACL contexts */
	snprintf(s, sizeof(s), "%s_%d", name, socketid);

	printf("IPv4 %s entries [%u]:\n", s, rules_nb);
	dump_ip4_rules(rules, rules_nb, 1);

	acl_param.name = s;
	acl_param.socket_id = socketid;
	acl_param.rule_size = RTE_ACL_RULE_SZ(RTE_DIM(ip4_defs));
	acl_param.max_rule_num = rules_nb;

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

/*
 * check that for each rule it's SPI has a correspondent entry in SAD
 */
static int
check_spi_value(struct sa_ctx *sa_ctx, int inbound)
{
	uint32_t i, num, spi;
	int32_t spi_idx;
	struct acl4_rules *acr;

	if (inbound != 0) {
		acr = acl4_rules_in;
		num = nb_acl4_rules_in;
	} else {
		acr = acl4_rules_out;
		num = nb_acl4_rules_out;
	}

	for (i = 0; i != num; i++) {
		spi = acr[i].data.userdata;
		if (spi != DISCARD && spi != BYPASS) {
			spi_idx = sa_spi_present(sa_ctx, spi, inbound);
			if (spi_idx < 0) {
				RTE_LOG(ERR, IPSEC,
					"SPI %u is not present in SAD\n",
					spi);
				return -ENOENT;
			}
			/* Update userdata with spi index */
			acr[i].data.userdata = spi_idx + 1;
		}
	}

	return 0;
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

	if (check_spi_value(ctx->sa_in, 1) < 0)
		rte_exit(EXIT_FAILURE,
			"Inbound IPv4 SP DB has unmatched in SAD SPIs\n");

	if (check_spi_value(ctx->sa_out, 0) < 0)
		rte_exit(EXIT_FAILURE,
			"Outbound IPv4 SP DB has unmatched in SAD SPIs\n");

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

static int
sp_cmp(const void *p, const void *q)
{
	uint32_t spi1 = ((const struct acl4_rules *)p)->data.userdata;
	uint32_t spi2 = ((const struct acl4_rules *)q)->data.userdata;

	return (int)(spi1 - spi2);
}


/*
 * Search though SP rules for given SPI.
 */
int
sp4_spi_present(uint32_t spi, int inbound, struct ip_addr ip_addr[2],
			uint32_t mask[2])
{
	uint32_t num;
	struct acl4_rules *rule;
	const struct acl4_rules *acr;
	struct acl4_rules tmpl;

	if (inbound != 0) {
		acr = acl4_rules_in;
		num = nb_acl4_rules_in;
	} else {
		acr = acl4_rules_out;
		num = nb_acl4_rules_out;
	}

	tmpl.data.userdata = spi;

	rule = bsearch(&tmpl, acr, num, sizeof(struct acl4_rules), sp_cmp);
	if (rule != NULL) {
		if (NULL != ip_addr && NULL != mask) {
			ip_addr[0].ip.ip4 = IPV4_SRC_FROM_SP(*rule);
			ip_addr[1].ip.ip4 = IPV4_DST_FROM_SP(*rule);
			mask[0] = IPV4_SRC_MASK_FROM_SP(*rule);
			mask[1] = IPV4_DST_MASK_FROM_SP(*rule);
		}
		return RTE_PTR_DIFF(rule, acr) / sizeof(struct acl4_rules);
	}

	return -ENOENT;
}

void
sp4_sort_arr(void)
{
	qsort(acl4_rules_in, nb_acl4_rules_in, sizeof(struct acl4_rules),
		sp_cmp);
	qsort(acl4_rules_out, nb_acl4_rules_out, sizeof(struct acl4_rules),
		sp_cmp);
}
