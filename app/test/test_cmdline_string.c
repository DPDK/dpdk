/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <rte_common.h>
#include <rte_string_fns.h>

#include <cmdline_parse.h>
#include <cmdline_parse_string.h>

#include "test_cmdline.h"

/* structures needed to run tests */

struct string_elt_str {
	const char * str;	/* parsed string */
	const char * result;	/* expected string */
	int idx;	/* position at which result is expected to be */
};

struct string_elt_str string_elt_strs[] = {
		{"one#two#three", "three", 2},
		{"one#two with spaces#three", "three", 2},
		{"one#two\twith\ttabs#three", "three", 2},
		{"one#two\rwith\rreturns#three", "three", 2},
		{"one#two\nwith\nnewlines#three", "three", 2},
		{"one#two#three", "one", 0},
		{"one#two#three", "two", 1},
		{"one#two\0three", "two", 1},
		{"one#two with spaces#three", "two with spaces", 1},
		{"one#two\twith\ttabs#three", "two\twith\ttabs", 1},
		{"one#two\rwith\rreturns#three", "two\rwith\rreturns", 1},
		{"one#two\nwith\nnewlines#three", "two\nwith\nnewlines", 1},
};

#if (CMDLINE_TEST_BUFSIZE < STR_TOKEN_SIZE) \
|| (CMDLINE_TEST_BUFSIZE < STR_MULTI_TOKEN_SIZE)
#undef CMDLINE_TEST_BUFSIZE
#define CMDLINE_TEST_BUFSIZE RTE_MAX(STR_TOKEN_SIZE, STR_MULTI_TOKEN_SIZE)
#endif

struct string_nb_str {
	const char * str;	/* parsed string */
	int nb_strs;	/* expected number of strings in str */
};

struct string_nb_str string_nb_strs[] = {
		{"one#two#three", 3},
		{"one", 1},
		{"one# \t two \r # three \n #four", 4},
};



struct string_parse_str {
	const char * str;	/* parsed string */
	const char * fixed_str;	/* parsing mode (any, fixed or multi) */
	const char * result;	/* expected result */
};

struct string_parse_str string_parse_strs[] = {
		{"one", NULL, "one"},	/* any string */
		{"two", "one#two#three", "two"},	/* multiple choice string */
		{"three", "three", "three"},	/* fixed string */
		{"three", "one#two with\rgarbage\tcharacters\n#three", "three"},
		{"two with\rgarbage\tcharacters\n",
				"one#two with\rgarbage\tcharacters\n#three",
				"two with\rgarbage\tcharacters\n"},
		{"one two", "one", "one"}, /* fixed string */
		{"one two", TOKEN_STRING_MULTI, "one two"}, /* multi string */
		{"one two", NULL, "one"}, /* any string */
		{"one two #three", TOKEN_STRING_MULTI, "one two "},
		/* multi string with comment */
};



struct string_invalid_str {
	const char * str;	/* parsed string */
	const char * fixed_str;	/* parsing mode (any, fixed or multi) */
};

struct string_invalid_str string_invalid_strs[] = {
		{"invalid", "one"},	/* fixed string */
		{"invalid", "one#two#three"},	/* multiple choice string */
		{"invalid", "invalidone"},	/* string that starts the same */
		{"invalidone", "invalid"},	/* string that starts the same */
		{"toolong!!!toolong!!!toolong!!!toolong!!!toolong!!!toolong!!!"
		 "toolong!!!toolong!!!toolong!!!toolong!!!toolong!!!toolong!!!"
		 "toolong!!!", NULL },
		{"toolong!!!toolong!!!toolong!!!toolong!!!toolong!!!toolong!!!"
		 "toolong!!!toolong!!!toolong!!!toolong!!!toolong!!!toolong!!!"
		 "toolong!!!", "fixed" },
		{"toolong!!!toolong!!!toolong!!!toolong!!!toolong!!!toolong!!!"
		 "toolong!!!toolong!!!toolong!!!toolong!!!toolong!!!toolong!!!"
		 "toolong!!!", "multi#choice#string" },
		{"invalid",
		 "toolong!!!toolong!!!toolong!!!toolong!!!toolong!!!toolong!!!"
		 "toolong!!!toolong!!!toolong!!!toolong!!!toolong!!!toolong!!!"
		 "toolong!!!" },
		 {"", "invalid"}
};



const char * string_help_strs[] = {
		NULL,
		"fixed_str",
		"multi#str",
};



#define STRING_PARSE_STRS_SIZE \
	(sizeof(string_parse_strs) / sizeof(string_parse_strs[0]))
#define STRING_HELP_STRS_SIZE \
	(sizeof(string_help_strs) / sizeof(string_help_strs[0]))
#define STRING_ELT_STRS_SIZE \
	(sizeof(string_elt_strs) / sizeof(string_elt_strs[0]))
#define STRING_NB_STRS_SIZE \
	(sizeof(string_nb_strs) / sizeof(string_nb_strs[0]))
#define STRING_INVALID_STRS_SIZE \
	(sizeof(string_invalid_strs) / sizeof(string_invalid_strs[0]))

#define SMALL_BUF 8

/* test invalid parameters */
int
test_parse_string_invalid_param(void)
{
	cmdline_parse_token_string_t token;
	int result;
	char buf[CMDLINE_TEST_BUFSIZE];

	memset(&token, 0, sizeof(token));

	snprintf(buf, sizeof(buf), "buffer");

	/* test null token */
	if (cmdline_get_help_string(
		NULL, buf, 0) != -1) {
		printf("Error: function accepted null token!\n");
		return -1;
	}
	if (cmdline_complete_get_elt_string(
			NULL, 0, buf, 0) != -1) {
		printf("Error: function accepted null token!\n");
		return -1;
	}
	if (cmdline_complete_get_nb_string(NULL) != -1) {
		printf("Error: function accepted null token!\n");
		return -1;
	}
	if (cmdline_parse_string(NULL, buf, NULL, 0) != -1) {
		printf("Error: function accepted null token!\n");
		return -1;
	}
	/* test null buffer */
	if (cmdline_complete_get_elt_string(
			(cmdline_parse_token_hdr_t*)&token, 0, NULL, 0) != -1) {
		printf("Error: function accepted null buffer!\n");
		return -1;
	}
	if (cmdline_parse_string(
			(cmdline_parse_token_hdr_t*)&token, NULL,
			(void*)&result, sizeof(result)) != -1) {
		printf("Error: function accepted null buffer!\n");
		return -1;
	}
	if (cmdline_get_help_string(
			(cmdline_parse_token_hdr_t*)&token, NULL, 0) != -1) {
		printf("Error: function accepted null buffer!\n");
		return -1;
	}
	/* test null result */
	if (cmdline_parse_string(
			(cmdline_parse_token_hdr_t*)&token, buf, NULL, 0) == -1) {
		printf("Error: function rejected null result!\n");
		return -1;
	}
	/* test negative index */
	if (cmdline_complete_get_elt_string(
			(cmdline_parse_token_hdr_t*)&token, -1, buf, 0) != -1) {
		printf("Error: function accepted negative index!\n");
		return -1;
	}
	return 0;
}

/* test valid parameters but invalid data */
int
test_parse_string_invalid_data(void)
{
	cmdline_parse_token_string_t token;
	cmdline_parse_token_string_t help_token;
	char buf[CMDLINE_TEST_BUFSIZE];
	char help_str[CMDLINE_TEST_BUFSIZE];
	char small_buf[SMALL_BUF];
	unsigned i;

	/* test parsing invalid strings */
	for (i = 0; i < STRING_INVALID_STRS_SIZE; i++) {
		memset(&token, 0, sizeof(token));
		memset(buf, 0, sizeof(buf));

		/* prepare test token data */
		token.string_data.str = string_invalid_strs[i].fixed_str;

		if (cmdline_parse_string((cmdline_parse_token_hdr_t*)&token,
				string_invalid_strs[i].str, (void*)buf,
				sizeof(buf)) != -1) {
			memset(help_str, 0, sizeof(help_str));
			memset(&help_token, 0, sizeof(help_token));

			help_token.string_data.str = string_invalid_strs[i].fixed_str;

			/* get parse type so we can give a good error message */
			cmdline_get_help_string((cmdline_parse_token_hdr_t*)&token, help_str,
					sizeof(help_str));

			printf("Error: parsing %s as %s succeeded!\n",
					string_invalid_strs[i].str, help_str);
			return -1;
		}
	}

	/* misc tests (big comments signify test cases) */
	memset(&token, 0, sizeof(token));
	memset(small_buf, 0, sizeof(small_buf));

	/*
	 * try to get element from a null token
	 */
	token.string_data.str = NULL;
	if (cmdline_complete_get_elt_string(
			(cmdline_parse_token_hdr_t*)&token, 1,
			buf, sizeof(buf)) != -1) {
		printf("Error: getting token from null token string!\n");
		return -1;
	}

	/*
	 * try to get element into a buffer that is too small
	 */
	token.string_data.str = "too_small_buffer";
	if (cmdline_complete_get_elt_string(
			(cmdline_parse_token_hdr_t*)&token, 0,
			small_buf, sizeof(small_buf)) != -1) {
		printf("Error: writing token into too small a buffer succeeded!\n");
		return -1;
	}

	/*
	 * get help string written into a buffer smaller than help string
	 * truncation should occur
	 */
	token.string_data.str = NULL;
	if (cmdline_get_help_string(
			(cmdline_parse_token_hdr_t*)&token,
			small_buf, sizeof(small_buf)) == -1) {
		printf("Error: writing help string into too small a buffer failed!\n");
		return -1;
	}
	/* get help string for "any string" so we can compare it with small_buf */
	cmdline_get_help_string((cmdline_parse_token_hdr_t*)&token, help_str,
			sizeof(help_str));
	if (strncmp(small_buf, help_str, sizeof(small_buf) - 1)) {
		printf("Error: help string mismatch!\n");
		return -1;
	}
	/* check null terminator */
	if (small_buf[sizeof(small_buf) - 1] != '\0') {
		printf("Error: small buffer doesn't have a null terminator!\n");
		return -1;
	}

	/*
	 * try to count tokens in a null token
	 */
	token.string_data.str = NULL;
	if (cmdline_complete_get_nb_string(
			(cmdline_parse_token_hdr_t*)&token) != 0) {
		printf("Error: getting token count from null token succeeded!\n");
		return -1;
	}

	return 0;
}

/* test valid parameters and data */
int
test_parse_string_valid(void)
{
	cmdline_parse_token_string_t token;
	cmdline_parse_token_string_t help_token;
	char buf[CMDLINE_TEST_BUFSIZE];
	char help_str[CMDLINE_TEST_BUFSIZE];
	unsigned i;

	/* test parsing strings */
	for (i = 0; i < STRING_PARSE_STRS_SIZE; i++) {
		memset(&token, 0, sizeof(token));
		memset(buf, 0, sizeof(buf));

		token.string_data.str = string_parse_strs[i].fixed_str;

		if (cmdline_parse_string((cmdline_parse_token_hdr_t*)&token,
				string_parse_strs[i].str, (void*)buf,
				sizeof(buf)) < 0) {

			/* clean help data */
			memset(&help_token, 0, sizeof(help_token));
			memset(help_str, 0, sizeof(help_str));

			/* prepare help token */
			help_token.string_data.str = string_parse_strs[i].fixed_str;

			/* get help string so that we get an informative error message */
			cmdline_get_help_string((cmdline_parse_token_hdr_t*)&token, help_str,
					sizeof(help_str));

			printf("Error: parsing %s as %s failed!\n",
					string_parse_strs[i].str, help_str);
			return -1;
		}
		if (strcmp(buf, string_parse_strs[i].result) != 0) {
			printf("Error: result mismatch!\n");
			return -1;
		}
	}

	/* get number of string tokens and verify it's correct */
	for (i = 0; i < STRING_NB_STRS_SIZE; i++) {
		memset(&token, 0, sizeof(token));

		token.string_data.str = string_nb_strs[i].str;

		if (cmdline_complete_get_nb_string(
				(cmdline_parse_token_hdr_t*)&token) <
				string_nb_strs[i].nb_strs) {
			printf("Error: strings count mismatch!\n");
			return -1;
		}
	}

	/* get token at specified position and verify it's correct */
	for (i = 0; i < STRING_ELT_STRS_SIZE; i++) {
		memset(&token, 0, sizeof(token));
		memset(buf, 0, sizeof(buf));

		token.string_data.str = string_elt_strs[i].str;

		if (cmdline_complete_get_elt_string(
				(cmdline_parse_token_hdr_t*)&token, string_elt_strs[i].idx,
				buf, sizeof(buf)) < 0) {
			printf("Error: getting string element failed!\n");
			return -1;
		}
		if (strncmp(buf, string_elt_strs[i].result,
				sizeof(buf)) != 0) {
			printf("Error: result mismatch!\n");
			return -1;
		}
	}

	/* cover all cases with help strings */
	for (i = 0; i < STRING_HELP_STRS_SIZE; i++) {
		memset(&help_token, 0, sizeof(help_token));
		memset(help_str, 0, sizeof(help_str));
		help_token.string_data.str = string_help_strs[i];
		if (cmdline_get_help_string((cmdline_parse_token_hdr_t*)&help_token,
				help_str, sizeof(help_str)) < 0) {
			printf("Error: help operation failed!\n");
			return -1;
		}
	}

	return 0;
}
