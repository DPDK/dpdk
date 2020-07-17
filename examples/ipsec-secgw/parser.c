/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 */
#include <rte_common.h>
#include <rte_crypto.h>
#include <rte_string_fns.h>

#include <cmdline_parse_string.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_ipaddr.h>
#include <cmdline_socket.h>
#include <cmdline.h>

#include "flow.h"
#include "ipsec.h"
#include "parser.h"

#define PARSE_DELIMITER		" \f\n\r\t\v"
static int
parse_tokenize_string(char *string, char *tokens[], uint32_t *n_tokens)
{
	uint32_t i;

	if ((string == NULL) ||
		(tokens == NULL) ||
		(*n_tokens < 1))
		return -EINVAL;

	for (i = 0; i < *n_tokens; i++) {
		tokens[i] = strtok_r(string, PARSE_DELIMITER, &string);
		if (tokens[i] == NULL)
			break;
	}

	if ((i == *n_tokens) &&
		(NULL != strtok_r(string, PARSE_DELIMITER, &string)))
		return -E2BIG;

	*n_tokens = i;
	return 0;
}

#define INADDRSZ 4
#define IN6ADDRSZ 16

/* int
 * inet_pton4(src, dst)
 *      like inet_aton() but without all the hexadecimal and shorthand.
 * return:
 *      1 if `src' is a valid dotted quad, else 0.
 * notice:
 *      does not touch `dst' unless it's returning 1.
 * author:
 *      Paul Vixie, 1996.
 */
static int
inet_pton4(const char *src, unsigned char *dst)
{
	static const char digits[] = "0123456789";
	int saw_digit, octets, ch;
	unsigned char tmp[INADDRSZ], *tp;

	saw_digit = 0;
	octets = 0;
	*(tp = tmp) = 0;
	while ((ch = *src++) != '\0') {
		const char *pch;

		pch = strchr(digits, ch);
		if (pch != NULL) {
			unsigned int new = *tp * 10 + (pch - digits);

			if (new > 255)
				return 0;
			if (!saw_digit) {
				if (++octets > 4)
					return 0;
				saw_digit = 1;
			}
			*tp = (unsigned char)new;
		} else if (ch == '.' && saw_digit) {
			if (octets == 4)
				return 0;
			*++tp = 0;
			saw_digit = 0;
		} else
			return 0;
	}
	if (octets < 4)
		return 0;

	memcpy(dst, tmp, INADDRSZ);
	return 1;
}

/* int
 * inet_pton6(src, dst)
 *      convert presentation level address to network order binary form.
 * return:
 *      1 if `src' is a valid [RFC1884 2.2] address, else 0.
 * notice:
 *      (1) does not touch `dst' unless it's returning 1.
 *      (2) :: in a full address is silently ignored.
 * credit:
 *      inspired by Mark Andrews.
 * author:
 *      Paul Vixie, 1996.
 */
static int
inet_pton6(const char *src, unsigned char *dst)
{
	static const char xdigits_l[] = "0123456789abcdef",
		xdigits_u[] = "0123456789ABCDEF";
	unsigned char tmp[IN6ADDRSZ], *tp = 0, *endp = 0, *colonp = 0;
	const char *xdigits = 0, *curtok = 0;
	int ch = 0, saw_xdigit = 0, count_xdigit = 0;
	unsigned int val = 0;
	unsigned dbloct_count = 0;

	memset((tp = tmp), '\0', IN6ADDRSZ);
	endp = tp + IN6ADDRSZ;
	colonp = NULL;
	/* Leading :: requires some special handling. */
	if (*src == ':')
		if (*++src != ':')
			return 0;
	curtok = src;
	saw_xdigit = count_xdigit = 0;
	val = 0;

	while ((ch = *src++) != '\0') {
		const char *pch;

		pch = strchr((xdigits = xdigits_l), ch);
		if (pch == NULL)
			pch = strchr((xdigits = xdigits_u), ch);
		if (pch != NULL) {
			if (count_xdigit >= 4)
				return 0;
			val <<= 4;
			val |= (pch - xdigits);
			if (val > 0xffff)
				return 0;
			saw_xdigit = 1;
			count_xdigit++;
			continue;
		}
		if (ch == ':') {
			curtok = src;
			if (!saw_xdigit) {
				if (colonp)
					return 0;
				colonp = tp;
				continue;
			} else if (*src == '\0') {
				return 0;
			}
			if (tp + sizeof(int16_t) > endp)
				return 0;
			*tp++ = (unsigned char) ((val >> 8) & 0xff);
			*tp++ = (unsigned char) (val & 0xff);
			saw_xdigit = 0;
			count_xdigit = 0;
			val = 0;
			dbloct_count++;
			continue;
		}
		if (ch == '.' && ((tp + INADDRSZ) <= endp) &&
		    inet_pton4(curtok, tp) > 0) {
			tp += INADDRSZ;
			saw_xdigit = 0;
			dbloct_count += 2;
			break;  /* '\0' was seen by inet_pton4(). */
		}
		return 0;
	}
	if (saw_xdigit) {
		if (tp + sizeof(int16_t) > endp)
			return 0;
		*tp++ = (unsigned char) ((val >> 8) & 0xff);
		*tp++ = (unsigned char) (val & 0xff);
		dbloct_count++;
	}
	if (colonp != NULL) {
		/* if we already have 8 double octets, having a colon
		 * means error */
		if (dbloct_count == 8)
			return 0;

		/*
		 * Since some memmove()'s erroneously fail to handle
		 * overlapping regions, we'll do the shift by hand.
		 */
		const int n = tp - colonp;
		int i;

		for (i = 1; i <= n; i++) {
			endp[-i] = colonp[n - i];
			colonp[n - i] = 0;
		}
		tp = endp;
	}
	if (tp != endp)
		return 0;
	memcpy(dst, tmp, IN6ADDRSZ);
	return 1;
}

int
parse_ipv4_addr(const char *token, struct in_addr *ipv4, uint32_t *mask)
{
	char ip_str[INET_ADDRSTRLEN] = {0};
	char *pch;

	pch = strchr(token, '/');
	if (pch != NULL) {
		strlcpy(ip_str, token,
			RTE_MIN((unsigned int long)(pch - token + 1),
			sizeof(ip_str)));
		pch += 1;
		if (is_str_num(pch) != 0)
			return -EINVAL;
		if (mask)
			*mask = atoi(pch);
	} else {
		strlcpy(ip_str, token, sizeof(ip_str));
		if (mask)
			*mask = 0;
	}
	if (strlen(ip_str) >= INET_ADDRSTRLEN)
		return -EINVAL;

	if (inet_pton4(ip_str, (unsigned char *)ipv4) != 1)
		return -EINVAL;

	return 0;
}

int
parse_ipv6_addr(const char *token, struct in6_addr *ipv6, uint32_t *mask)
{
	char ip_str[256] = {0};
	char *pch;

	pch = strchr(token, '/');
	if (pch != NULL) {
		strlcpy(ip_str, token,
			RTE_MIN((unsigned int long)(pch - token + 1),
					sizeof(ip_str)));
		pch += 1;
		if (is_str_num(pch) != 0)
			return -EINVAL;
		if (mask)
			*mask = atoi(pch);
	} else {
		strlcpy(ip_str, token, sizeof(ip_str));
		if (mask)
			*mask = 0;
	}

	if (strlen(ip_str) >= INET6_ADDRSTRLEN)
		return -EINVAL;

	if (inet_pton6(ip_str, (unsigned char *)ipv6) != 1)
		return -EINVAL;

	return 0;
}

int
parse_range(const char *token, uint16_t *low, uint16_t *high)
{
	char ch;
	char num_str[20];
	uint32_t pos;
	int range_low = -1;
	int range_high = -1;

	if (!low || !high)
		return -1;

	memset(num_str, 0, 20);
	pos = 0;

	while ((ch = *token++) != '\0') {
		if (isdigit(ch)) {
			if (pos >= 19)
				return -1;
			num_str[pos++] = ch;
		} else if (ch == ':') {
			if (range_low != -1)
				return -1;
			range_low = atoi(num_str);
			memset(num_str, 0, 20);
			pos = 0;
		}
	}

	if (strlen(num_str) == 0)
		return -1;

	range_high = atoi(num_str);

	*low = (uint16_t)range_low;
	*high = (uint16_t)range_high;

	return 0;
}

/*
 * helper function for parse_mac, parse one section of the ether addr.
 */
static const char *
parse_uint8x16(const char *s, uint8_t *v, uint8_t ls)
{
	char *end;
	unsigned long t;

	errno = 0;
	t = strtoul(s, &end, 16);
	if (errno != 0 || end[0] != ls || t > UINT8_MAX)
		return NULL;
	v[0] = t;
	return end + 1;
}

static int
parse_mac(const char *str, struct rte_ether_addr *addr)
{
	uint32_t i;

	static const uint8_t stop_sym[RTE_DIM(addr->addr_bytes)] = {
		[0] = ':',
		[1] = ':',
		[2] = ':',
		[3] = ':',
		[4] = ':',
		[5] = 0,
	};

	for (i = 0; i != RTE_DIM(addr->addr_bytes); i++) {
		str = parse_uint8x16(str, addr->addr_bytes + i, stop_sym[i]);
		if (str == NULL)
			return -EINVAL;
	}

	return 0;
}

/** sp add parse */
struct cfg_sp_add_cfg_item {
	cmdline_fixed_string_t sp_keyword;
	cmdline_multi_string_t multi_string;
};

static void
cfg_sp_add_cfg_item_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, void *data)
{
	struct cfg_sp_add_cfg_item *params = parsed_result;
	char *tokens[32];
	uint32_t n_tokens = RTE_DIM(tokens);
	struct parse_status *status = (struct parse_status *)data;

	APP_CHECK((parse_tokenize_string(params->multi_string, tokens,
		&n_tokens) == 0), status, "too many arguments");

	if (status->status < 0)
		return;

	if (strcmp(tokens[0], "ipv4") == 0) {
		parse_sp4_tokens(tokens, n_tokens, status);
		if (status->status < 0)
			return;
	} else if (strcmp(tokens[0], "ipv6") == 0) {
		parse_sp6_tokens(tokens, n_tokens, status);
		if (status->status < 0)
			return;
	} else {
		APP_CHECK(0, status, "unrecognizable input %s\n",
			tokens[0]);
		return;
	}
}

static cmdline_parse_token_string_t cfg_sp_add_sp_str =
	TOKEN_STRING_INITIALIZER(struct cfg_sp_add_cfg_item,
		sp_keyword, "sp");

static cmdline_parse_token_string_t cfg_sp_add_multi_str =
	TOKEN_STRING_INITIALIZER(struct cfg_sp_add_cfg_item, multi_string,
		TOKEN_STRING_MULTI);

cmdline_parse_inst_t cfg_sp_add_rule = {
	.f = cfg_sp_add_cfg_item_parsed,
	.data = NULL,
	.help_str = "",
	.tokens = {
		(void *) &cfg_sp_add_sp_str,
		(void *) &cfg_sp_add_multi_str,
		NULL,
	},
};

/* sa add parse */
struct cfg_sa_add_cfg_item {
	cmdline_fixed_string_t sa_keyword;
	cmdline_multi_string_t multi_string;
};

static void
cfg_sa_add_cfg_item_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, void *data)
{
	struct cfg_sa_add_cfg_item *params = parsed_result;
	char *tokens[32];
	uint32_t n_tokens = RTE_DIM(tokens);
	struct parse_status *status = (struct parse_status *)data;

	APP_CHECK(parse_tokenize_string(params->multi_string, tokens,
		&n_tokens) == 0, status, "too many arguments\n");

	parse_sa_tokens(tokens, n_tokens, status);
}

static cmdline_parse_token_string_t cfg_sa_add_sa_str =
	TOKEN_STRING_INITIALIZER(struct cfg_sa_add_cfg_item,
		sa_keyword, "sa");

static cmdline_parse_token_string_t cfg_sa_add_multi_str =
	TOKEN_STRING_INITIALIZER(struct cfg_sa_add_cfg_item, multi_string,
		TOKEN_STRING_MULTI);

cmdline_parse_inst_t cfg_sa_add_rule = {
	.f = cfg_sa_add_cfg_item_parsed,
	.data = NULL,
	.help_str = "",
	.tokens = {
		(void *) &cfg_sa_add_sa_str,
		(void *) &cfg_sa_add_multi_str,
		NULL,
	},
};

/* rt add parse */
struct cfg_rt_add_cfg_item {
	cmdline_fixed_string_t rt_keyword;
	cmdline_multi_string_t multi_string;
};

static void
cfg_rt_add_cfg_item_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, void *data)
{
	struct cfg_rt_add_cfg_item *params = parsed_result;
	char *tokens[32];
	uint32_t n_tokens = RTE_DIM(tokens);
	struct parse_status *status = (struct parse_status *)data;

	APP_CHECK(parse_tokenize_string(
		params->multi_string, tokens, &n_tokens) == 0,
		status, "too many arguments\n");
	if (status->status < 0)
		return;

	parse_rt_tokens(tokens, n_tokens, status);
}

static cmdline_parse_token_string_t cfg_rt_add_rt_str =
	TOKEN_STRING_INITIALIZER(struct cfg_rt_add_cfg_item,
		rt_keyword, "rt");

static cmdline_parse_token_string_t cfg_rt_add_multi_str =
	TOKEN_STRING_INITIALIZER(struct cfg_rt_add_cfg_item, multi_string,
		TOKEN_STRING_MULTI);

cmdline_parse_inst_t cfg_rt_add_rule = {
	.f = cfg_rt_add_cfg_item_parsed,
	.data = NULL,
	.help_str = "",
	.tokens = {
		(void *) &cfg_rt_add_rt_str,
		(void *) &cfg_rt_add_multi_str,
		NULL,
	},
};

/* flow add parse */
struct cfg_flow_add_cfg_item {
	cmdline_fixed_string_t flow_keyword;
	cmdline_multi_string_t multi_string;
};

static void
cfg_flow_add_cfg_item_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, void *data)
{
	struct cfg_flow_add_cfg_item *params = parsed_result;
	char *tokens[32];
	uint32_t n_tokens = RTE_DIM(tokens);
	struct parse_status *status = (struct parse_status *)data;

	APP_CHECK(parse_tokenize_string(
		params->multi_string, tokens, &n_tokens) == 0,
		status, "too many arguments\n");
	if (status->status < 0)
		return;

	parse_flow_tokens(tokens, n_tokens, status);
}

static cmdline_parse_token_string_t cfg_flow_add_flow_str =
	TOKEN_STRING_INITIALIZER(struct cfg_flow_add_cfg_item,
		flow_keyword, "flow");

static cmdline_parse_token_string_t cfg_flow_add_multi_str =
	TOKEN_STRING_INITIALIZER(struct cfg_flow_add_cfg_item, multi_string,
		TOKEN_STRING_MULTI);

cmdline_parse_inst_t cfg_flow_add_rule = {
	.f = cfg_flow_add_cfg_item_parsed,
	.data = NULL,
	.help_str = "",
	.tokens = {
		(void *) &cfg_flow_add_flow_str,
		(void *) &cfg_flow_add_multi_str,
		NULL,
	},
};

/* neigh add parse */
struct cfg_neigh_add_item {
	cmdline_fixed_string_t neigh;
	cmdline_fixed_string_t pstr;
	uint16_t port;
	cmdline_fixed_string_t mac;
};

static void
cfg_parse_neigh(void *parsed_result, __rte_unused struct cmdline *cl,
	void *data)
{
	int32_t rc;
	struct cfg_neigh_add_item *res;
	struct parse_status *st;
	struct rte_ether_addr mac;

	st = data;
	res = parsed_result;
	rc = parse_mac(res->mac, &mac);
	APP_CHECK(rc == 0, st, "invalid ether addr:%s", res->mac);
	rc = add_dst_ethaddr(res->port, &mac);
	APP_CHECK(rc == 0, st, "invalid port numer:%hu", res->port);
	if (st->status < 0)
		return;
}

cmdline_parse_token_string_t cfg_add_neigh_start =
	TOKEN_STRING_INITIALIZER(struct cfg_neigh_add_item, neigh, "neigh");
cmdline_parse_token_string_t cfg_add_neigh_pstr =
	TOKEN_STRING_INITIALIZER(struct cfg_neigh_add_item, pstr, "port");
cmdline_parse_token_num_t cfg_add_neigh_port =
	TOKEN_NUM_INITIALIZER(struct cfg_neigh_add_item, port, UINT16);
cmdline_parse_token_string_t cfg_add_neigh_mac =
	TOKEN_STRING_INITIALIZER(struct cfg_neigh_add_item, mac, NULL);

cmdline_parse_inst_t cfg_neigh_add_rule = {
	.f = cfg_parse_neigh,
	.data = NULL,
	.help_str = "",
	.tokens = {
		(void *)&cfg_add_neigh_start,
		(void *)&cfg_add_neigh_pstr,
		(void *)&cfg_add_neigh_port,
		(void *)&cfg_add_neigh_mac,
		NULL,
	},
};

/** set of cfg items */
cmdline_parse_ctx_t ipsec_ctx[] = {
	(cmdline_parse_inst_t *)&cfg_sp_add_rule,
	(cmdline_parse_inst_t *)&cfg_sa_add_rule,
	(cmdline_parse_inst_t *)&cfg_rt_add_rule,
	(cmdline_parse_inst_t *)&cfg_flow_add_rule,
	(cmdline_parse_inst_t *)&cfg_neigh_add_rule,
	NULL,
};

int
parse_cfg_file(const char *cfg_filename)
{
	struct cmdline *cl = cmdline_stdin_new(ipsec_ctx, "");
	FILE *f = fopen(cfg_filename, "r");
	char str[1024] = {0}, *get_s = NULL;
	uint32_t line_num = 0;
	struct parse_status status = {0};

	if (f == NULL) {
		rte_panic("Error: invalid file descriptor %s\n", cfg_filename);
		goto error_exit;
	}

	if (cl == NULL) {
		rte_panic("Error: cannot create cmdline instance\n");
		goto error_exit;
	}

	cfg_sp_add_rule.data = &status;
	cfg_sa_add_rule.data = &status;
	cfg_rt_add_rule.data = &status;
	cfg_flow_add_rule.data = &status;
	cfg_neigh_add_rule.data = &status;

	do {
		char oneline[1024];
		char *pos;
		get_s = fgets(oneline, 1024, f);

		if (!get_s)
			break;

		line_num++;

		if (strlen(oneline) > 1022) {
			rte_panic("%s:%u: error: "
				"the line contains more characters the parser can handle\n",
				cfg_filename, line_num);
			goto error_exit;
		}

		/* process comment char '#' */
		if (oneline[0] == '#')
			continue;

		pos = strchr(oneline, '#');
		if (pos != NULL)
			*pos = '\0';

		/* process line concatenator '\' */
		pos = strchr(oneline, 92);
		if (pos != NULL) {
			if (pos != oneline+strlen(oneline) - 2) {
				rte_panic("%s:%u: error: "
					"no character should exist after '\\'\n",
					cfg_filename, line_num);
				goto error_exit;
			}

			*pos = '\0';

			if (strlen(oneline) + strlen(str) > 1022) {
				rte_panic("%s:%u: error: "
					"the concatenated line contains more characters the parser can handle\n",
					cfg_filename, line_num);
				goto error_exit;
			}

			strcpy(str + strlen(str), oneline);
			continue;
		}

		/* copy the line to str and process */
		if (strlen(oneline) + strlen(str) > 1022) {
			rte_panic("%s:%u: error: "
				"the line contains more characters the parser can handle\n",
				cfg_filename, line_num);
			goto error_exit;
		}
		strcpy(str + strlen(str), oneline);

		str[strlen(str)] = '\n';
		if (cmdline_parse(cl, str) < 0) {
			rte_panic("%s:%u: error: parsing \"%s\" failed\n",
				cfg_filename, line_num, str);
			goto error_exit;
		}

		if (status.status < 0) {
			rte_panic("%s:%u: error: %s", cfg_filename,
				line_num, status.parse_msg);
			goto error_exit;
		}

		memset(str, 0, 1024);
	} while (1);

	cmdline_stdin_exit(cl);
	fclose(f);

	sa_sort_arr();
	sp4_sort_arr();
	sp6_sort_arr();

	return 0;

error_exit:
	if (cl)
		cmdline_stdin_exit(cl);
	if (f)
		fclose(f);

	return -1;
}
