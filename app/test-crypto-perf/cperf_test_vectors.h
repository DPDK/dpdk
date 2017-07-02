/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
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

#ifndef _CPERF_TEST_VECTRORS_
#define _CPERF_TEST_VECTRORS_

#include "cperf_options.h"

struct cperf_test_vector {
	struct {
		uint8_t *data;
		uint32_t length;
	} plaintext;

	struct {
		uint8_t *data;
		uint16_t length;
	} cipher_key;

	struct {
		uint8_t *data;
		uint16_t length;
	} auth_key;

	struct {
		uint8_t *data;
		uint16_t length;
	} aead_key;

	struct {
		uint8_t *data;
		uint16_t length;
	} cipher_iv;

	struct {
		uint8_t *data;
		uint16_t length;
	} auth_iv;

	struct {
		uint8_t *data;
		uint16_t length;
	} aead_iv;

	struct {
		uint8_t *data;
		uint32_t length;
	} ciphertext;

	struct {
		uint8_t *data;
		phys_addr_t phys_addr;
		uint16_t length;
	} aad;

	struct {
		uint8_t *data;
		phys_addr_t phys_addr;
		uint16_t length;
	} digest;

	struct {
		uint32_t auth_offset;
		uint32_t auth_length;
		uint32_t cipher_offset;
		uint32_t cipher_length;
		uint32_t aead_offset;
		uint32_t aead_length;
	} data;
};

struct cperf_test_vector*
cperf_test_vector_get_dummy(struct cperf_options *options);

extern uint8_t ciphertext[2048];

extern uint8_t cipher_key[];
extern uint8_t auth_key[];

extern uint8_t iv[];
extern uint8_t aad[];

extern uint8_t digest[2048];

#endif
