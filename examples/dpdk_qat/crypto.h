/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#ifndef CRYPTO_H_
#define CRYPTO_H_

/* Pass Labels/Values to crypto units */
enum cipher_alg {
	/* Option to not do any cryptography */
	NO_CIPHER,
	CIPHER_DES,
	CIPHER_DES_CBC,
	CIPHER_DES3,
	CIPHER_DES3_CBC,
	CIPHER_AES,
	CIPHER_AES_CBC_128,
	CIPHER_KASUMI_F8,
	NUM_CRYPTO,
};

enum hash_alg {
	/* Option to not do any hash */
	NO_HASH,
	HASH_MD5,
	HASH_SHA1,
	HASH_SHA1_96,
	HASH_SHA224,
	HASH_SHA256,
	HASH_SHA384,
	HASH_SHA512,
	HASH_AES_XCBC,
	HASH_AES_XCBC_96,
	HASH_KASUMI_F9,
	NUM_HMAC,
};

/* Return value from crypto_{encrypt/decrypt} */
enum crypto_result {
	/* Packet was successfully put into crypto queue */
	CRYPTO_RESULT_IN_PROGRESS,
	/* Cryptography has failed in some way */
	CRYPTO_RESULT_FAIL,
};

extern enum crypto_result crypto_encrypt(struct rte_mbuf *pkt, enum cipher_alg c,
		enum hash_alg h);
extern enum crypto_result crypto_decrypt(struct rte_mbuf *pkt, enum cipher_alg c,
		enum hash_alg h);

extern int crypto_init(void);

extern int per_core_crypto_init(uint32_t lcore_id);

extern void crypto_exit(void);

extern void *crypto_get_next_response(void);

extern void crypto_flush_tx_queue(uint32_t lcore_id);

#endif /* CRYPTO_H_ */
