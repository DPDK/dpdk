/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 */

#ifndef __PARSER_H
#define __PARSER_H

#include <ctype.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>

struct parse_status {
	int status;
	char parse_msg[256];
};

#define	APP_CHECK(exp, st, fmt, ...)					\
do {									\
	if (!(exp)) {							\
		sprintf((st)->parse_msg, fmt "\n",			\
			## __VA_ARGS__);				\
		(st)->status = -1;					\
	} else								\
		(st)->status = 0;					\
} while (0)

#define APP_CHECK_PRESENCE(val, str, status)				\
	APP_CHECK(val == 0, status,					\
		"item \"%s\" already present", str)

#define APP_CHECK_TOKEN_EQUAL(tokens, index, ref, status)		\
	APP_CHECK(strcmp(tokens[index], ref) == 0, status,		\
		"unrecognized input \"%s\": expect \"%s\"\n",		\
		tokens[index], ref)

static inline int
is_str_num(const char *str)
{
	uint32_t i;

	for (i = 0; i < strlen(str); i++)
		if (!isdigit(str[i]))
			return -1;

	return 0;
}

#define APP_CHECK_TOKEN_IS_NUM(tokens, index, status)			\
	APP_CHECK(is_str_num(tokens[index]) == 0, status,		\
	"input \"%s\" is not valid number string", tokens[index])


#define INCREMENT_TOKEN_INDEX(index, max_num, status)			\
do {									\
	APP_CHECK(index + 1 < max_num, status, "reaching the end of "	\
		"the token array");					\
	index++;							\
} while (0)

int
parse_ipv4_addr(const char *token, struct in_addr *ipv4, uint32_t *mask);

int
parse_ipv6_addr(const char *token, struct in6_addr *ipv6, uint32_t *mask);

int
parse_range(const char *token, uint16_t *low, uint16_t *high);

void
sp4_sort_arr(void);

void
parse_sp4_tokens(char **tokens, uint32_t n_tokens,
	struct parse_status *status);

void
sp6_sort_arr(void);

void
parse_sp6_tokens(char **tokens, uint32_t n_tokens,
	struct parse_status *status);

void
sa_sort_arr(void);

void
parse_sa_tokens(char **tokens, uint32_t n_tokens,
	struct parse_status *status);

void
parse_rt_tokens(char **tokens, uint32_t n_tokens,
	struct parse_status *status);

int
parse_cfg_file(const char *cfg_filename);

#endif
