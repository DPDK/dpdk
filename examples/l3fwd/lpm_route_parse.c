/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <sys/socket.h>

#include "l3fwd.h"
#include "l3fwd_route.h"

enum {
	CB_FLD_DST_ADDR,
	CB_FLD_IF_OUT,
	CB_FLD_MAX
};

struct lpm_route_rule *route_base_v4;
struct lpm_route_rule *route_base_v6;
int route_num_v4;
int route_num_v6;

/* Bypass comment and empty lines */
int
is_bypass_line(const char *buff)
{
	int i = 0;

	/* comment line */
	if (buff[0] == COMMENT_LEAD_CHAR)
		return 1;
	/* empty line */
	while (buff[i] != '\0') {
		if (!isspace(buff[i]))
			return 0;
		i++;
	}
	return 1;
}

static int
parse_ipv6_addr_mask(char *token, uint32_t *ipv6, uint8_t *mask)
{
	char *sa, *sm, *sv;
	const char *dlm =  "/";

	sv = NULL;
	sa = strtok_r(token, dlm, &sv);
	if (sa == NULL)
		return -EINVAL;
	sm = strtok_r(NULL, dlm, &sv);
	if (sm == NULL)
		return -EINVAL;

	if (inet_pton(AF_INET6, sa, ipv6) != 1)
		return -EINVAL;

	GET_CB_FIELD(sm, *mask, 0, 128, 0);
	return 0;
}

static int
parse_ipv4_addr_mask(char *token, uint32_t *ipv4, uint8_t *mask)
{
	char *sa, *sm, *sv;
	const char *dlm =  "/";

	sv = NULL;
	sa = strtok_r(token, dlm, &sv);
	if (sa == NULL)
		return -EINVAL;
	sm = strtok_r(NULL, dlm, &sv);
	if (sm == NULL)
		return -EINVAL;

	if (inet_pton(AF_INET, sa, ipv4) != 1)
		return -EINVAL;

	GET_CB_FIELD(sm, *mask, 0, 32, 0);
	*ipv4 = ntohl(*ipv4);
	return 0;
}

static int
lpm_parse_v6_net(char *in, uint32_t *v, uint8_t *mask_len)
{
	int32_t rc;

	/* get address. */
	rc = parse_ipv6_addr_mask(in, v, mask_len);

	return rc;
}

static int
lpm_parse_v6_rule(char *str, struct lpm_route_rule *v)
{
	int i, rc;
	char *s, *sp, *in[CB_FLD_MAX];
	static const char *dlm = " \t\n";
	int dim = CB_FLD_MAX;
	s = str;

	for (i = 0; i != dim; i++, s = NULL) {
		in[i] = strtok_r(s, dlm, &sp);
		if (in[i] == NULL)
			return -EINVAL;
	}

	rc = lpm_parse_v6_net(in[CB_FLD_DST_ADDR], v->ip_32, &v->depth);

	GET_CB_FIELD(in[CB_FLD_IF_OUT], v->if_out, 0, UINT8_MAX, 0);

	return rc;
}

static int
lpm_parse_v4_rule(char *str, struct lpm_route_rule *v)
{
	int i, rc;
	char *s, *sp, *in[CB_FLD_MAX];
	static const char *dlm = " \t\n";
	int dim = CB_FLD_MAX;
	s = str;

	for (i = 0; i != dim; i++, s = NULL) {
		in[i] = strtok_r(s, dlm, &sp);
		if (in[i] == NULL)
			return -EINVAL;
	}

	rc = parse_ipv4_addr_mask(in[CB_FLD_DST_ADDR], &v->ip, &v->depth);

	GET_CB_FIELD(in[CB_FLD_IF_OUT], v->if_out, 0, UINT8_MAX, 0);

	return rc;
}

static int
lpm_add_default_v4_rules(void)
{
	/* populate the LPM IPv4 table */
	unsigned int i, rule_size = sizeof(*route_base_v4);
	route_num_v4 = RTE_DIM(ipv4_l3fwd_route_array);

	route_base_v4 = calloc(route_num_v4, rule_size);

	for (i = 0; i < (unsigned int)route_num_v4; i++) {
		route_base_v4[i].ip = ipv4_l3fwd_route_array[i].ip;
		route_base_v4[i].depth = ipv4_l3fwd_route_array[i].depth;
		route_base_v4[i].if_out = ipv4_l3fwd_route_array[i].if_out;
	}
	return 0;
}

static int
lpm_add_default_v6_rules(void)
{
	/* populate the LPM IPv6 table */
	unsigned int i, rule_size = sizeof(*route_base_v6);
	route_num_v6 = RTE_DIM(ipv6_l3fwd_route_array);

	route_base_v6 = calloc(route_num_v6, rule_size);

	for (i = 0; i < (unsigned int)route_num_v6; i++) {
		memcpy(route_base_v6[i].ip_8, ipv6_l3fwd_route_array[i].ip,
			   sizeof(route_base_v6[i].ip_8));
		route_base_v6[i].depth = ipv6_l3fwd_route_array[i].depth;
		route_base_v6[i].if_out = ipv6_l3fwd_route_array[i].if_out;
	}
	return 0;
}

static int
lpm_add_rules(const char *rule_path,
		struct lpm_route_rule **proute_base,
		int (*parser)(char *, struct lpm_route_rule *))
{
	struct lpm_route_rule *route_rules;
	struct lpm_route_rule *next;
	unsigned int route_num = 0;
	unsigned int route_cnt = 0;
	char buff[LINE_MAX];
	FILE *fh;
	unsigned int i = 0, rule_size = sizeof(*next);
	int val;

	*proute_base = NULL;
	fh = fopen(rule_path, "rb");
	if (fh == NULL)
		return -EINVAL;

	while ((fgets(buff, LINE_MAX, fh) != NULL)) {
		if (buff[0] == ROUTE_LEAD_CHAR)
			route_num++;
	}

	if (route_num == 0) {
		fclose(fh);
		return -EINVAL;
	}

	val = fseek(fh, 0, SEEK_SET);
	if (val < 0) {
		fclose(fh);
		return -EINVAL;
	}

	route_rules = calloc(route_num, rule_size);

	if (route_rules == NULL) {
		fclose(fh);
		return -EINVAL;
	}

	i = 0;
	while (fgets(buff, LINE_MAX, fh) != NULL) {
		i++;
		if (is_bypass_line(buff))
			continue;

		char s = buff[0];

		/* Route entry */
		if (s == ROUTE_LEAD_CHAR)
			next = &route_rules[route_cnt];

		/* Illegal line */
		else {
			RTE_LOG(ERR, L3FWD,
				"%s Line %u: should start with leading "
				"char %c\n",
				rule_path, i, ROUTE_LEAD_CHAR);
			fclose(fh);
			free(route_rules);
			return -EINVAL;
		}

		if (parser(buff + 1, next) != 0) {
			RTE_LOG(ERR, L3FWD,
				"%s Line %u: parse rules error\n",
				rule_path, i);
			fclose(fh);
			free(route_rules);
			return -EINVAL;
		}

		route_cnt++;
	}

	fclose(fh);

	*proute_base = route_rules;

	return route_cnt;
}

void
lpm_free_routes(void)
{
	free(route_base_v4);
	free(route_base_v6);
	route_base_v4 = NULL;
	route_base_v6 = NULL;
	route_num_v4 = 0;
	route_num_v6 = 0;
}

/* Load rules from the input file */
void
read_config_files_lpm(void)
{
	if (parm_config.rule_ipv4_name != NULL &&
			parm_config.rule_ipv6_name != NULL) {
		/* ipv4 check */
		route_num_v4 = lpm_add_rules(parm_config.rule_ipv4_name,
					&route_base_v4, &lpm_parse_v4_rule);
		if (route_num_v4 < 0) {
			lpm_free_routes();
			rte_exit(EXIT_FAILURE, "Failed to add IPv4 rules\n");
		}

		/* ipv6 check */
		route_num_v6 = lpm_add_rules(parm_config.rule_ipv6_name,
					&route_base_v6, &lpm_parse_v6_rule);
		if (route_num_v6 < 0) {
			lpm_free_routes();
			rte_exit(EXIT_FAILURE, "Failed to add IPv6 rules\n");
		}
	} else {
		RTE_LOG(INFO, L3FWD, "Missing 1 or more rule files, using default instead\n");
		if (lpm_add_default_v4_rules() < 0) {
			lpm_free_routes();
			rte_exit(EXIT_FAILURE, "Failed to add default IPv4 rules\n");
		}
		if (lpm_add_default_v6_rules() < 0) {
			lpm_free_routes();
			rte_exit(EXIT_FAILURE, "Failed to add default IPv6 rules\n");
		}
	}
}
