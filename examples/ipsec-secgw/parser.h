/*   BSD LICENSE
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

#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#ifndef __PARSER_H
#define __PARSER_H

struct parse_status {
	int status;
	char parse_msg[256];
};

#define	APP_CHECK(exp, status, fmt, ...)				\
do {									\
	if (!(exp)) {							\
		sprintf(status->parse_msg, fmt "\n",			\
			## __VA_ARGS__);				\
		status->status = -1;					\
	} else								\
		status->status = 0;					\
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
parse_sp4_tokens(char **tokens, uint32_t n_tokens,
	struct parse_status *status);

void
parse_sp6_tokens(char **tokens, uint32_t n_tokens,
	struct parse_status *status);

void
parse_sa_tokens(char **tokens, uint32_t n_tokens,
	struct parse_status *status);

void
parse_rt_tokens(char **tokens, uint32_t n_tokens,
	struct parse_status *status);

int
parse_cfg_file(const char *cfg_filename);

#endif
