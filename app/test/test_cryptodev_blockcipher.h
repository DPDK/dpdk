/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#ifndef TEST_CRYPTODEV_BLOCKCIPHER_H_
#define TEST_CRYPTODEV_BLOCKCIPHER_H_

#ifndef BLOCKCIPHER_TEST_MSG_LEN
#define BLOCKCIPHER_TEST_MSG_LEN		256
#endif

#define BLOCKCIPHER_TEST_OP_ENCRYPT		0x01
#define BLOCKCIPHER_TEST_OP_DECRYPT		0x02
#define BLOCKCIPHER_TEST_OP_AUTH_GEN	0x04
#define BLOCKCIPHER_TEST_OP_AUTH_VERIFY	0x08

#define BLOCKCIPHER_TEST_FEATURE_OOP			0x01
#define BLOCKCIPHER_TEST_FEATURE_SESSIONLESS	0x02
#define BLOCKCIPHER_TEST_FEATURE_STOPPER	0x04 /* stop upon failing */
#define BLOCKCIPHER_TEST_FEATURE_SG		0x08 /* Scatter Gather */

#define BLOCKCIPHER_TEST_TARGET_PMD_MB		0x0001 /* Multi-buffer flag */
#define BLOCKCIPHER_TEST_TARGET_PMD_QAT			0x0002 /* QAT flag */
#define BLOCKCIPHER_TEST_TARGET_PMD_OPENSSL	0x0004 /* SW OPENSSL flag */
#define BLOCKCIPHER_TEST_TARGET_PMD_ARMV8	0x0008 /* ARMv8 flag */
#define BLOCKCIPHER_TEST_TARGET_PMD_SCHEDULER	0x0010 /* Scheduler */
#define BLOCKCIPHER_TEST_TARGET_PMD_DPAA2_SEC	0x0020 /* DPAA2_SEC flag */
#define BLOCKCIPHER_TEST_TARGET_PMD_DPAA_SEC	0x0040 /* DPAA_SEC flag */
#define BLOCKCIPHER_TEST_TARGET_PMD_MVSAM	0x0080 /* Marvell flag */
#define BLOCKCIPHER_TEST_TARGET_PMD_OCTEONTX	0x0100 /* OCTEON TX flag */
#define BLOCKCIPHER_TEST_TARGET_PMD_VIRTIO	0x0200 /* VIRTIO flag */
#define BLOCKCIPHER_TEST_TARGET_PMD_CAAM_JR	0x0400 /* CAAM_JR flag */
#define BLOCKCIPHER_TEST_TARGET_PMD_CCP		0x0800 /* CCP flag */

#define BLOCKCIPHER_TEST_OP_CIPHER	(BLOCKCIPHER_TEST_OP_ENCRYPT | \
					BLOCKCIPHER_TEST_OP_DECRYPT)

#define BLOCKCIPHER_TEST_OP_AUTH	(BLOCKCIPHER_TEST_OP_AUTH_GEN | \
					BLOCKCIPHER_TEST_OP_AUTH_VERIFY)

#define BLOCKCIPHER_TEST_OP_ENC_AUTH_GEN	(BLOCKCIPHER_TEST_OP_ENCRYPT | \
					BLOCKCIPHER_TEST_OP_AUTH_GEN)

#define BLOCKCIPHER_TEST_OP_AUTH_VERIFY_DEC	(BLOCKCIPHER_TEST_OP_DECRYPT | \
					BLOCKCIPHER_TEST_OP_AUTH_VERIFY)

enum blockcipher_test_type {
	BLKCIPHER_AES_CHAIN_TYPE,	/* use aes_chain_test_cases[] */
	BLKCIPHER_AES_CIPHERONLY_TYPE,	/* use aes_cipheronly_test_cases[] */
	BLKCIPHER_AES_DOCSIS_TYPE,	/* use aes_docsis_test_cases[] */
	BLKCIPHER_3DES_CHAIN_TYPE,	/* use triple_des_chain_test_cases[] */
	BLKCIPHER_3DES_CIPHERONLY_TYPE,	/* triple_des_cipheronly_test_cases[] */
	BLKCIPHER_AUTHONLY_TYPE,	/* use hash_test_cases[] */
	BLKCIPHER_DES_CIPHERONLY_TYPE,	/* use des_cipheronly_test_cases[] */
	BLKCIPHER_DES_DOCSIS_TYPE	/* use des_docsis_test_cases[] */
};

struct blockcipher_test_case {
	const char *test_descr; /* test description */
	const struct blockcipher_test_data *test_data;
	uint8_t op_mask; /* operation mask */
	uint8_t feature_mask;
	uint32_t pmd_mask;
};

struct blockcipher_test_data {
	enum rte_crypto_cipher_algorithm crypto_algo;

	struct {
		uint8_t data[64];
		unsigned int len;
	} cipher_key;

	struct {
		uint8_t data[64] __rte_aligned(16);
		unsigned int len;
	} iv;

	struct {
		const uint8_t *data;
		unsigned int len;
	} plaintext;

	struct {
		const uint8_t *data;
		unsigned int len;
	} ciphertext;

	enum rte_crypto_auth_algorithm auth_algo;

	struct {
		uint8_t data[128];
		unsigned int len;
	} auth_key;

	struct {
		uint8_t data[128];
		unsigned int len;		/* for qat */
		unsigned int truncated_len;	/* for mb */
	} digest;

	unsigned int cipher_offset;
	unsigned int auth_offset;
};

int
test_blockcipher_all_tests(struct rte_mempool *mbuf_pool,
	struct rte_mempool *op_mpool,
	struct rte_mempool *sess_mpool,
	struct rte_mempool *sess_priv_mpool,
	uint8_t dev_id,
	int driver_id,
	enum blockcipher_test_type test_type);

#endif /* TEST_CRYPTODEV_BLOCKCIPHER_H_ */
