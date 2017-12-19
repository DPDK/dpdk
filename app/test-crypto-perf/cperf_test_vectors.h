/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
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
		rte_iova_t phys_addr;
		uint16_t length;
	} aad;

	struct {
		uint8_t *data;
		rte_iova_t phys_addr;
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
