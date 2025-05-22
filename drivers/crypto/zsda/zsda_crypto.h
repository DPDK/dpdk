/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 ZTE Corporation
 */

#ifndef _ZSDA_CRYPTO_H_
#define _ZSDA_CRYPTO_H_

#include "zsda_crypto_pmd.h"
#include "zsda_crypto_session.h"

struct __rte_packed_begin crypto_cfg {
	uint8_t slba_L[8];
	uint8_t key[ZSDA_CIPHER_KEY_MAX_LEN];
	uint8_t lbads : 4;
	uint8_t resv1 : 4;
	uint8_t resv2[7];
	uint8_t slba_H[8];
	uint8_t resv3[8];
} __rte_packed_end;

struct __rte_packed_begin zsda_wqe_crpt {
	uint8_t valid;
	uint8_t op_code;
	uint16_t sid;
	uint8_t resv[3];
	uint8_t rx_sgl_type : 4;
	uint8_t tx_sgl_type : 4;
	uint64_t rx_addr;
	uint32_t rx_length;
	uint64_t tx_addr;
	uint32_t tx_length;
	struct crypto_cfg cfg;
} __rte_packed_end;

int zsda_encry_match(const void *op_in);
int zsda_decry_match(const void *op_in);
int zsda_hash_match(const void *op_in);

int zsda_crypto_wqe_build(void *op_in, const struct zsda_queue *queue,
			 void **op_cookies, const uint16_t new_tail);

int zsda_hash_wqe_build(void *op_in, const struct zsda_queue *queue,
		       void **op_cookies, const uint16_t new_tail);

#endif /* _ZSDA_CRYPTO_H_ */
