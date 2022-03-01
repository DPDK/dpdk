/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <sys/socket.h>

#include "l3fwd.h"
#include "l3fwd_route.h"

static struct em_rule *em_route_base_v4;
static struct em_rule *em_route_base_v6;

enum {
	CB_FLD_DST_ADDR,
	CB_FLD_SRC_ADDR,
	CB_FLD_DST_PORT,
	CB_FLD_SRC_PORT,
	CB_FLD_PROTO,
	CB_FLD_IF_OUT,
	CB_FLD_MAX
};

static int
em_parse_v6_net(const char *in, uint8_t *v)
{
	int32_t rc;

	/* get address. */
	rc = inet_pton(AF_INET6, in, v);
	if (rc != 1)
		return -EINVAL;

	return 0;
}

static int
em_parse_v6_rule(char *str, struct em_rule *v)
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

	rc = em_parse_v6_net(in[CB_FLD_DST_ADDR], v->v6_key.ip_dst);
	if (rc != 0)
		return rc;
	rc = em_parse_v6_net(in[CB_FLD_SRC_ADDR], v->v6_key.ip_src);
	if (rc != 0)
		return rc;

	/* source port. */
	GET_CB_FIELD(in[CB_FLD_SRC_PORT], v->v6_key.port_src, 0, UINT16_MAX, 0);
	/* destination port. */
	GET_CB_FIELD(in[CB_FLD_DST_PORT], v->v6_key.port_dst, 0, UINT16_MAX, 0);
	/* protocol. */
	GET_CB_FIELD(in[CB_FLD_PROTO], v->v6_key.proto, 0, UINT8_MAX, 0);
	/* out interface. */
	GET_CB_FIELD(in[CB_FLD_IF_OUT], v->if_out, 0, UINT8_MAX, 0);

	return 0;
}

static int
em_parse_v4_rule(char *str, struct em_rule *v)
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

	rc = inet_pton(AF_INET, in[CB_FLD_DST_ADDR], &(v->v4_key.ip_dst));
	v->v4_key.ip_dst = ntohl(v->v4_key.ip_dst);
	if (rc != 1)
		return rc;

	rc = inet_pton(AF_INET, in[CB_FLD_SRC_ADDR], &(v->v4_key.ip_src));
	v->v4_key.ip_src = ntohl(v->v4_key.ip_src);
	if (rc != 1)
		return rc;

	/* source port. */
	GET_CB_FIELD(in[CB_FLD_SRC_PORT], v->v4_key.port_src, 0, UINT16_MAX, 0);
	/* destination port. */
	GET_CB_FIELD(in[CB_FLD_DST_PORT], v->v4_key.port_dst, 0, UINT16_MAX, 0);
	/* protocol. */
	GET_CB_FIELD(in[CB_FLD_PROTO], v->v4_key.proto, 0, UINT8_MAX, 0);
	/* out interface. */
	GET_CB_FIELD(in[CB_FLD_IF_OUT], v->if_out, 0, UINT8_MAX, 0);

	return 0;
}

static int
em_add_rules(const char *rule_path,
		struct em_rule **proute_base,
		int (*parser)(char *, struct em_rule *))
{
	struct em_rule *route_rules;
	struct em_rule *next;
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

static int
em_add_default_v4_rules(void)
{
	/* populate the LPM IPv4 table */
	unsigned int i, rule_size = sizeof(*em_route_base_v4);
	route_num_v4 = RTE_DIM(ipv4_l3fwd_em_route_array);

	em_route_base_v4 = calloc(route_num_v4, rule_size);

	for (i = 0; i < (unsigned int)route_num_v4; i++) {
		em_route_base_v4[i].v4_key.ip_dst = ipv4_l3fwd_em_route_array[i].key.ip_dst;
		em_route_base_v4[i].v4_key.ip_src = ipv4_l3fwd_em_route_array[i].key.ip_src;
		em_route_base_v4[i].v4_key.port_dst = ipv4_l3fwd_em_route_array[i].key.port_dst;
		em_route_base_v4[i].v4_key.port_src = ipv4_l3fwd_em_route_array[i].key.port_src;
		em_route_base_v4[i].v4_key.proto = ipv4_l3fwd_em_route_array[i].key.proto;
		em_route_base_v4[i].if_out = ipv4_l3fwd_em_route_array[i].if_out;
	}
	return 0;
}

static int
em_add_default_v6_rules(void)
{
	/* populate the LPM IPv6 table */
	unsigned int i, rule_size = sizeof(*em_route_base_v6);
	route_num_v6 = RTE_DIM(ipv6_l3fwd_em_route_array);

	em_route_base_v6 = calloc(route_num_v6, rule_size);

	for (i = 0; i < (unsigned int)route_num_v6; i++) {
		memcpy(em_route_base_v6[i].v6_key.ip_dst, ipv6_l3fwd_em_route_array[i].key.ip_dst,
			   sizeof(em_route_base_v6[i].v6_key.ip_dst));
		memcpy(em_route_base_v6[i].v6_key.ip_src, ipv6_l3fwd_em_route_array[i].key.ip_src,
			   sizeof(em_route_base_v6[i].v6_key.ip_src));
		em_route_base_v6[i].v6_key.port_dst = ipv6_l3fwd_em_route_array[i].key.port_dst;
		em_route_base_v6[i].v6_key.port_src = ipv6_l3fwd_em_route_array[i].key.port_src;
		em_route_base_v6[i].v6_key.proto = ipv6_l3fwd_em_route_array[i].key.proto;
		em_route_base_v6[i].if_out = ipv6_l3fwd_em_route_array[i].if_out;
	}
	return 0;
}

void
em_free_routes(void)
{
	free(em_route_base_v4);
	free(em_route_base_v6);
	em_route_base_v4 = NULL;
	em_route_base_v6 = NULL;
	route_num_v4 = 0;
	route_num_v6 = 0;
}

/* Load rules from the input file */
void
read_config_files_em(void)
{
	/* ipv4 check */
	if (parm_config.rule_ipv4_name != NULL &&
			parm_config.rule_ipv6_name != NULL) {
		/* ipv4 check */
		route_num_v4 = em_add_rules(parm_config.rule_ipv4_name,
					&em_route_base_v4, &em_parse_v4_rule);
		if (route_num_v4 < 0) {
			em_free_routes();
			rte_exit(EXIT_FAILURE, "Failed to add EM IPv4 rules\n");
		}

		/* ipv6 check */
		route_num_v6 = em_add_rules(parm_config.rule_ipv6_name,
					&em_route_base_v6, &em_parse_v6_rule);
		if (route_num_v6 < 0) {
			em_free_routes();
			rte_exit(EXIT_FAILURE, "Failed to add EM IPv6 rules\n");
		}
	} else {
		RTE_LOG(INFO, L3FWD, "Missing 1 or more rule files, using default instead\n");
		if (em_add_default_v4_rules() < 0) {
			em_free_routes();
			rte_exit(EXIT_FAILURE, "Failed to add default IPv4 rules\n");
		}
		if (em_add_default_v6_rules() < 0) {
			em_free_routes();
			rte_exit(EXIT_FAILURE, "Failed to add default IPv6 rules\n");
		}
	}
}
