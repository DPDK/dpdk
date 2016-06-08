/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
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

#ifndef __INCLUDE_PARSER_H__
#define __INCLUDE_PARSER_H__

#include <stdint.h>

#include <rte_ip.h>
#include <rte_ether.h>

#define PARSE_DELIMITER				" \f\n\r\t\v"

#define skip_white_spaces(pos)			\
({						\
	__typeof__(pos) _p = (pos);		\
	for ( ; isspace(*_p); _p++)		\
		;				\
	_p;					\
})

static inline size_t
skip_digits(const char *src)
{
	size_t i;

	for (i = 0; isdigit(src[i]); i++)
		;

	return i;
}

int parser_read_arg_bool(const char *p);

int parser_read_uint64(uint64_t *value, const char *p);
int parser_read_uint32(uint32_t *value, const char *p);
int parser_read_uint16(uint16_t *value, const char *p);
int parser_read_uint8(uint8_t *value, const char *p);

int parser_read_uint64_hex(uint64_t *value, const char *p);
int parser_read_uint32_hex(uint32_t *value, const char *p);
int parser_read_uint16_hex(uint16_t *value, const char *p);
int parser_read_uint8_hex(uint8_t *value, const char *p);

int parse_hex_string(char *src, uint8_t *dst, uint32_t *size);

int parse_ipv4_addr(const char *token, struct in_addr *ipv4);
int parse_ipv6_addr(const char *token, struct in6_addr *ipv6);
int parse_mac_addr(const char *token, struct ether_addr *addr);
int parse_mpls_labels(char *string, uint32_t *labels, uint32_t *n_labels);

int parse_tokenize_string(char *string, char *tokens[], uint32_t *n_tokens);

#endif
