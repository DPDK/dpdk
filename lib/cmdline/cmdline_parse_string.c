/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

#include <stdio.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <rte_string_fns.h>

#include "cmdline_parse.h"
#include "cmdline_parse_string.h"

struct cmdline_token_ops cmdline_token_string_ops = {
	.parse = cmdline_parse_string,
	.complete_get_nb = cmdline_complete_get_nb_string,
	.complete_get_elt = cmdline_complete_get_elt_string,
	.get_help = cmdline_get_help_string,
};

#define CHOICESTRING_HELP "Mul-choice STRING"
#define ANYSTRING_HELP    "Any STRING"
#define ANYSTRINGS_HELP   "Any STRINGS"
#define FIXEDSTRING_HELP  "Fixed STRING"

static unsigned int
get_token_len(const char *s)
{
	char c;
	unsigned int i=0;

	c = s[i];
	while (c!='#' && c!='\0') {
		i++;
		c = s[i];
	}
	return i;
}

static const char *
get_next_token(const char *s)
{
	unsigned int i;
	i = get_token_len(s);
	if (s[i] == '#')
		return s+i+1;
	return NULL;
}

int
cmdline_parse_string(cmdline_parse_token_hdr_t *tk, const char *buf, void *res,
	unsigned ressize)
{
	struct cmdline_token_string *tk2;
	struct cmdline_token_string_data *sd;
	unsigned int token_len;
	const char *str;

	if (res && ressize < STR_TOKEN_SIZE)
		return -1;

	if (!tk || !buf || ! *buf)
		return -1;

	tk2 = (struct cmdline_token_string *)tk;

	sd = &tk2->string_data;

	/* fixed string (known single token) */
	if ((sd->str != NULL) && (strcmp(sd->str, TOKEN_STRING_MULTI) != 0)) {
		str = sd->str;
		do {
			token_len = get_token_len(str);

			/* if token is too big... */
			if (token_len >= STR_TOKEN_SIZE - 1) {
				continue;
			}

			if ( strncmp(buf, str, token_len) ) {
				continue;
			}

			if ( !cmdline_isendoftoken(*(buf+token_len)) ) {
				continue;
			}

			break;
		} while ( (str = get_next_token(str)) != NULL );

		if (!str)
			return -1;
	}
	/* multi string */
	else if (sd->str != NULL) {
		if (ressize < STR_MULTI_TOKEN_SIZE)
			return -1;

		token_len = 0;
		while (!cmdline_isendofcommand(buf[token_len]) &&
		      token_len < (STR_MULTI_TOKEN_SIZE - 1))
			token_len++;

		/* return if token too long */
		if (token_len >= (STR_MULTI_TOKEN_SIZE - 1))
			return -1;
	}
	/* unspecified string (unknown single token) */
	else {
		token_len = 0;
		while(!cmdline_isendoftoken(buf[token_len]) &&
		      token_len < (STR_TOKEN_SIZE-1))
			token_len++;

		/* return if token too long */
		if (token_len >= STR_TOKEN_SIZE - 1) {
			return -1;
		}
	}

	if (res) {
		if ((sd->str != NULL) && (strcmp(sd->str, TOKEN_STRING_MULTI) == 0))
			/* we are sure that token_len is < STR_MULTI_TOKEN_SIZE-1 */
			strlcpy(res, buf, STR_MULTI_TOKEN_SIZE);
		else
			/* we are sure that token_len is < STR_TOKEN_SIZE-1 */
			strlcpy(res, buf, STR_TOKEN_SIZE);

		*((char *)res + token_len) = 0;
	}

	return token_len;
}

int cmdline_complete_get_nb_string(cmdline_parse_token_hdr_t *tk)
{
	struct cmdline_token_string *tk2;
	struct cmdline_token_string_data *sd;
	const char *str;
	int ret = 1;

	if (!tk)
		return -1;

	tk2 = (struct cmdline_token_string *)tk;
	sd = &tk2->string_data;

	if (!sd->str)
		return 0;

	str = sd->str;
	while( (str = get_next_token(str)) != NULL ) {
		ret++;
	}
	return ret;
}

int cmdline_complete_get_elt_string(cmdline_parse_token_hdr_t *tk, int idx,
				    char *dstbuf, unsigned int size)
{
	struct cmdline_token_string *tk2;
	struct cmdline_token_string_data *sd;
	const char *s;
	unsigned int len;

	if (!tk || !dstbuf || idx < 0)
		return -1;

	tk2 = (struct cmdline_token_string *)tk;
	sd = &tk2->string_data;

	s = sd->str;

	while (idx-- && s)
		s = get_next_token(s);

	if (!s)
		return -1;

	len = get_token_len(s);
	if (len > size - 1)
		return -1;

	memcpy(dstbuf, s, len);
	dstbuf[len] = '\0';
	return 0;
}


int cmdline_get_help_string(cmdline_parse_token_hdr_t *tk, char *dstbuf,
			    unsigned int size)
{
	struct cmdline_token_string *tk2;
	struct cmdline_token_string_data *sd;
	const char *s;

	if (!tk || !dstbuf)
		return -1;

	tk2 = (struct cmdline_token_string *)tk;
	sd = &tk2->string_data;

	s = sd->str;

	if (s) {
		if (strcmp(s, TOKEN_STRING_MULTI) == 0)
			snprintf(dstbuf, size, ANYSTRINGS_HELP);
		else if (get_next_token(s))
			snprintf(dstbuf, size, CHOICESTRING_HELP);
		else
			snprintf(dstbuf, size, FIXEDSTRING_HELP);
	} else
		snprintf(dstbuf, size, ANYSTRING_HELP);

	return 0;
}
