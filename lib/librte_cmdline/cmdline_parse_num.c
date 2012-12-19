/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
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
 * 
 */

/*
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University of California, Berkeley nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
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
	FLOAT_POS,
	FLOAT_NEG,

	ERROR,

	FIRST_OK, /* not used */
	ZERO_OK,
	HEX_OK,
	OCTAL_OK,
	BIN_OK,
	DEC_NEG_OK,
	DEC_POS_OK,
	FLOAT_POS_OK,
	FLOAT_NEG_OK
};

/* Keep it sync with enum in .h */
static const char * num_help[] = {
	"UINT8", "UINT16", "UINT32", "UINT64",
	"INT8", "INT16", "INT32", "INT64",
#ifdef CMDLINE_HAVE_FLOAT
	"FLOAT",
#endif
};

static inline int
add_to_res(unsigned int c, uint64_t *res, unsigned int base)
{
	/* overflow */
	if ( (UINT64_MAX - c) / base < *res ) {
		return -1;
	}

	*res = (uint64_t) (*res * base + c);
	return 0;
}


/* parse an int or a float */
int
cmdline_parse_num(cmdline_parse_token_hdr_t *tk, const char *srcbuf, void *res)
{
	struct cmdline_token_num_data nd;
	enum num_parse_state_t st = START;
	const char * buf = srcbuf;
	char c = *buf;
	uint64_t res1 = 0;
#ifdef CMDLINE_HAVE_FLOAT
	uint64_t res2 = 0, res3 = 1;
#endif

	memcpy(&nd, &((struct cmdline_token_num *)tk)->num_data, sizeof(nd));

	while ( st != ERROR && c && ! cmdline_isendoftoken(c) ) {
		debug_printf("%c %x -> ", c, c);
		switch (st) {
		case START:
			if (c == '-') {
				st = DEC_NEG;
			}
			else if (c == '0') {
				st = ZERO_OK;
			}
#ifdef CMDLINE_HAVE_FLOAT
			else if (c == '.') {
				st = FLOAT_POS;
				res1 = 0;
			}
#endif
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
#ifdef CMDLINE_HAVE_FLOAT
			else if (c == '.') {
				st = FLOAT_POS;
				res1 = 0;
			}
#endif
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
#ifdef CMDLINE_HAVE_FLOAT
			else if (c == '.') {
				res1 = 0;
				st = FLOAT_NEG;
			}
#endif
			else {
				st = ERROR;
			}
			break;

		case DEC_NEG_OK:
			if (c >= '0' && c <= '9') {
				if (add_to_res(c - '0', &res1, 10) < 0)
					st = ERROR;
			}
#ifdef CMDLINE_HAVE_FLOAT
			else if (c == '.') {
				st = FLOAT_NEG;
			}
#endif
			else {
				st = ERROR;
			}
			break;

		case DEC_POS_OK:
			if (c >= '0' && c <= '9') {
				if (add_to_res(c - '0', &res1, 10) < 0)
					st = ERROR;
			}
#ifdef CMDLINE_HAVE_FLOAT
			else if (c == '.') {
				st = FLOAT_POS;
			}
#endif
			else {
				st = ERROR;
			}
			break;

		case HEX:
			st = HEX_OK;
			/* no break */
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
			/* no break */
		case BIN_OK:
			if (c >= '0' && c <= '1') {
				if (add_to_res(c - '0', &res1, 2) < 0)
					st = ERROR;
			}
			else {
				st = ERROR;
			}
			break;

#ifdef CMDLINE_HAVE_FLOAT
		case FLOAT_POS:
			if (c >= '0' && c <= '9') {
				if (add_to_res(c - '0', &res2, 10) < 0)
					st = ERROR;
				else
					st = FLOAT_POS_OK;
				res3 = 10;
			}
			else {
				st = ERROR;
			}
			break;

		case FLOAT_NEG:
			if (c >= '0' && c <= '9') {
				if (add_to_res(c - '0', &res2, 10) < 0)
					st = ERROR;
				else
					st = FLOAT_NEG_OK;
				res3 = 10;
			}
			else {
				st = ERROR;
			}
			break;

		case FLOAT_POS_OK:
			if (c >= '0' && c <= '9') {
				if (add_to_res(c - '0', &res2, 10) < 0)
					st = ERROR;
				if (add_to_res(0, &res3, 10) < 0)
					st = ERROR;
			}
			else {
				st = ERROR;
			}
			break;

		case FLOAT_NEG_OK:
			if (c >= '0' && c <= '9') {
				if (add_to_res(c - '0', &res2, 10) < 0)
					st = ERROR;
				if (add_to_res(0, &res3, 10) < 0)
					st = ERROR;
			}
			else {
				st = ERROR;
			}
			break;
#endif

		default:
			debug_printf("not impl ");

		}

#ifdef CMDLINE_HAVE_FLOAT
		debug_printf("(%"PRIu32")  (%"PRIu32")  (%"PRIu32")\n",
			     res1, res2, res3);
#else
		debug_printf("(%"PRIu32")\n", res1);
#endif

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
		if ( nd.type == INT8 && res1 <= INT8_MAX ) {
			if (res)
				*(int8_t *)res = (int8_t) res1;
			return (buf-srcbuf);
		}
		else if ( nd.type == INT16 && res1 <= INT16_MAX ) {
			if (res)
				*(int16_t *)res = (int16_t) res1;
			return (buf-srcbuf);
		}
		else if ( nd.type == INT32 && res1 <= INT32_MAX ) {
			if (res)
				*(int32_t *)res = (int32_t) res1;
			return (buf-srcbuf);
		}
		else if ( nd.type == UINT8 && res1 <= UINT8_MAX ) {
			if (res)
				*(uint8_t *)res = (uint8_t) res1;
			return (buf-srcbuf);
		}
		else if (nd.type == UINT16  && res1 <= UINT16_MAX ) {
			if (res)
				*(uint16_t *)res = (uint16_t) res1;
			return (buf-srcbuf);
		}
		else if ( nd.type == UINT32 ) {
			if (res)
				*(uint32_t *)res = (uint32_t) res1;
			return (buf-srcbuf);
		}
		else if ( nd.type == UINT64 ) {
			if (res)
				*(uint64_t *)res = res1;
			return (buf-srcbuf);
		}
#ifdef CMDLINE_HAVE_FLOAT
		else if ( nd.type == FLOAT ) {
			if (res)
				*(float *)res = (float)res1;
			return (buf-srcbuf);
		}
#endif
		else {
			return -1;
		}
		break;

	case DEC_NEG_OK:
		if ( nd.type == INT8 && res1 <= INT8_MAX + 1 ) {
			if (res)
				*(int8_t *)res = (int8_t) (-res1);
			return (buf-srcbuf);
		}
		else if ( nd.type == INT16 && res1 <= (uint16_t)INT16_MAX + 1 ) {
			if (res)
				*(int16_t *)res = (int16_t) (-res1);
			return (buf-srcbuf);
		}
		else if ( nd.type == INT32 && res1 <= (uint32_t)INT32_MAX + 1 ) {
			if (res)
				*(int32_t *)res = (int32_t) (-res1);
			return (buf-srcbuf);
		}
#ifdef CMDLINE_HAVE_FLOAT
		else if ( nd.type == FLOAT ) {
			if (res)
				*(float *)res = - (float)res1;
			return (buf-srcbuf);
		}
#endif
		else {
			return -1;
		}
		break;

#ifdef CMDLINE_HAVE_FLOAT
	case FLOAT_POS:
	case FLOAT_POS_OK:
		if ( nd.type == FLOAT ) {
			if (res)
				*(float *)res = (float)res1 + ((float)res2 / (float)res3);
			return (buf-srcbuf);

		}
		else {
			return -1;
		}
		break;

	case FLOAT_NEG:
	case FLOAT_NEG_OK:
		if ( nd.type == FLOAT ) {
			if (res)
				*(float *)res = - ((float)res1 + ((float)res2 / (float)res3));
			return (buf-srcbuf);

		}
		else {
			return -1;
		}
		break;
#endif
	default:
		debug_printf("error\n");
		return -1;
	}
}


/* parse an int or a float */
int
cmdline_get_help_num(cmdline_parse_token_hdr_t *tk, char *dstbuf, unsigned int size)
{
	struct cmdline_token_num_data nd;

	memcpy(&nd, &((struct cmdline_token_num *)tk)->num_data, sizeof(nd));

	/* should not happen.... don't so this test */
	/* if (nd.type >= (sizeof(num_help)/sizeof(const char *))) */
	/* return -1; */

	rte_snprintf(dstbuf, size, "%s", num_help[nd.type]);
	dstbuf[size-1] = '\0';
	return 0;
}
