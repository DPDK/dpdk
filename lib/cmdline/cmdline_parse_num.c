/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <rte_string_fns.h>

#include "cmdline_parse.h"
#include "cmdline_parse_num.h"

#ifdef RTE_LIBRTE_CMDLINE_DEBUG
#define debug_printf(args...) printf(args)
#else
#define debug_printf(args...) do {} while(0)
#endif

struct cmdline_token_ops cmdline_token_num_ops = {
	.parse = cmdline_parse_num,
	.complete_get_nb = NULL,
	.complete_get_elt = NULL,
	.get_help = cmdline_get_help_num,
};


enum num_parse_state_t {
	START,
	DEC_NEG,
	BIN,
	HEX,

	ERROR,

	FIRST_OK, /* not used */
	ZERO_OK,
	HEX_OK,
	OCTAL_OK,
	BIN_OK,
	DEC_NEG_OK,
	DEC_POS_OK,
};

/* Keep it sync with enum in .h */
static const char * num_help[] = {
	"UINT8", "UINT16", "UINT32", "UINT64",
	"INT8", "INT16", "INT32", "INT64",
};

static inline int
add_to_res(unsigned int c, uint64_t *res, unsigned int base)
{
	/* overflow */
	if ((UINT64_MAX - c) / base < *res)
		return -1;

	*res = (uint64_t) (*res * base + c);
	return 0;
}

static int
check_res_size(struct cmdline_token_num_data *nd, unsigned ressize)
{
	switch (nd->type) {
	case RTE_INT8:
	case RTE_UINT8:
		if (ressize < sizeof(int8_t))
			return -1;
		break;
	case RTE_INT16:
	case RTE_UINT16:
		if (ressize < sizeof(int16_t))
			return -1;
		break;
	case RTE_INT32:
	case RTE_UINT32:
		if (ressize < sizeof(int32_t))
			return -1;
		break;
	case RTE_INT64:
	case RTE_UINT64:
		if (ressize < sizeof(int64_t))
			return -1;
		break;
	default:
		return -1;
	}
	return 0;
}

/* parse an int */
int
cmdline_parse_num(cmdline_parse_token_hdr_t *tk, const char *srcbuf, void *res,
	unsigned ressize)
{
	struct cmdline_token_num_data nd;
	enum num_parse_state_t st = START;
	const char * buf;
	char c;
	uint64_t res1 = 0;

	if (!tk)
		return -1;

	if (!srcbuf || !*srcbuf)
		return -1;

	buf = srcbuf;
	c = *buf;

	memcpy(&nd, &((struct cmdline_token_num *)tk)->num_data, sizeof(nd));

	/* check that we have enough room in res */
	if (res) {
		if (check_res_size(&nd, ressize) < 0)
			return -1;
	}

	while (st != ERROR && c && !cmdline_isendoftoken(c)) {
		debug_printf("%c %x -> ", c, c);
		switch (st) {
		case START:
			if (c == '-') {
				st = DEC_NEG;
			}
			else if (c == '0') {
				st = ZERO_OK;
			}
			else if (c >= '1' && c <= '9') {
				if (add_to_res(c - '0', &res1, 10) < 0)
					st = ERROR;
				else
					st = DEC_POS_OK;
			}
			else  {
				st = ERROR;
			}
			break;

		case ZERO_OK:
			if (c == 'x') {
				st = HEX;
			}
			else if (c == 'b') {
				st = BIN;
			}
			else if (c >= '0' && c <= '7') {
				if (add_to_res(c - '0', &res1, 10) < 0)
					st = ERROR;
				else
					st = OCTAL_OK;
			}
			else  {
				st = ERROR;
			}
			break;

		case DEC_NEG:
			if (c >= '0' && c <= '9') {
				if (add_to_res(c - '0', &res1, 10) < 0)
					st = ERROR;
				else
					st = DEC_NEG_OK;
			}
			else {
				st = ERROR;
			}
			break;

		case DEC_NEG_OK:
			if (c >= '0' && c <= '9') {
				if (add_to_res(c - '0', &res1, 10) < 0)
					st = ERROR;
			}
			else {
				st = ERROR;
			}
			break;

		case DEC_POS_OK:
			if (c >= '0' && c <= '9') {
				if (add_to_res(c - '0', &res1, 10) < 0)
					st = ERROR;
			}
			else {
				st = ERROR;
			}
			break;

		case HEX:
			st = HEX_OK;
			/* fall-through */
		case HEX_OK:
			if (c >= '0' && c <= '9') {
				if (add_to_res(c - '0', &res1, 16) < 0)
					st = ERROR;
			}
			else if (c >= 'a' && c <= 'f') {
				if (add_to_res(c - 'a' + 10, &res1, 16) < 0)
					st = ERROR;
			}
			else if (c >= 'A' && c <= 'F') {
				if (add_to_res(c - 'A' + 10, &res1, 16) < 0)
					st = ERROR;
			}
			else {
				st = ERROR;
			}
			break;


		case OCTAL_OK:
			if (c >= '0' && c <= '7') {
				if (add_to_res(c - '0', &res1, 8) < 0)
					st = ERROR;
			}
			else {
				st = ERROR;
			}
			break;

		case BIN:
			st = BIN_OK;
			/* fall-through */
		case BIN_OK:
			if (c >= '0' && c <= '1') {
				if (add_to_res(c - '0', &res1, 2) < 0)
					st = ERROR;
			}
			else {
				st = ERROR;
			}
			break;
		default:
			debug_printf("not impl ");

		}

		debug_printf("(%"PRIu64")\n", res1);

		buf ++;
		c = *buf;

		/* token too long */
		if (buf-srcbuf > 127)
			return -1;
	}

	switch (st) {
	case ZERO_OK:
	case DEC_POS_OK:
	case HEX_OK:
	case OCTAL_OK:
	case BIN_OK:
		if (nd.type == RTE_INT8 && res1 <= INT8_MAX) {
			if (res) *(int8_t *)res = (int8_t) res1;
			return buf-srcbuf;
		} else if (nd.type == RTE_INT16 && res1 <= INT16_MAX) {
			if (res) *(int16_t *)res = (int16_t) res1;
			return buf-srcbuf;
		} else if (nd.type == RTE_INT32 && res1 <= INT32_MAX) {
			if (res) *(int32_t *)res = (int32_t) res1;
			return buf-srcbuf;
		} else if (nd.type == RTE_INT64 && res1 <= INT64_MAX) {
			if (res) *(int64_t *)res = (int64_t) res1;
			return buf-srcbuf;
		} else if (nd.type == RTE_UINT8 && res1 <= UINT8_MAX) {
			if (res) *(uint8_t *)res = (uint8_t) res1;
			return buf-srcbuf;
		} else if (nd.type == RTE_UINT16  && res1 <= UINT16_MAX) {
			if (res) *(uint16_t *)res = (uint16_t) res1;
			return buf-srcbuf;
		} else if (nd.type == RTE_UINT32 && res1 <= UINT32_MAX) {
			if (res) *(uint32_t *)res = (uint32_t) res1;
			return buf-srcbuf;
		} else if (nd.type == RTE_UINT64) {
			if (res) *(uint64_t *)res = res1;
			return buf-srcbuf;
		} else {
			return -1;
		}
		break;

	case DEC_NEG_OK:
		if (nd.type == RTE_INT8 &&
				res1 <= INT8_MAX + 1) {
			if (res) *(int8_t *)res = (int8_t) (-res1);
			return buf-srcbuf;
		} else if (nd.type == RTE_INT16 &&
				res1 <= (uint16_t)INT16_MAX + 1) {
			if (res) *(int16_t *)res = (int16_t) (-res1);
			return buf-srcbuf;
		} else if (nd.type == RTE_INT32 &&
				res1 <= (uint32_t)INT32_MAX + 1) {
			if (res) *(int32_t *)res = (int32_t) (-res1);
			return buf-srcbuf;
		} else if (nd.type == RTE_INT64 &&
				res1 <= (uint64_t)INT64_MAX + 1) {
			if (res) *(int64_t *)res = (int64_t) (-res1);
			return buf-srcbuf;
		} else {
			return -1;
		}
		break;
	default:
		debug_printf("error\n");
		return -1;
	}
}


/* parse an int */
int
cmdline_get_help_num(cmdline_parse_token_hdr_t *tk, char *dstbuf, unsigned int size)
{
	struct cmdline_token_num_data nd;
	int ret;

	if (!tk)
		return -1;

	memcpy(&nd, &((struct cmdline_token_num *)tk)->num_data, sizeof(nd));

	/* should not happen.... don't so this test */
	/* if (nd.type >= (sizeof(num_help)/sizeof(const char *))) */
	/* return -1; */

	ret = strlcpy(dstbuf, num_help[nd.type], size);
	if (ret < 0)
		return -1;
	dstbuf[size-1] = '\0';
	return 0;
}
