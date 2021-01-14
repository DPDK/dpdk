/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#ifndef TEST_CRYPTODEV_HASH_TEST_VECTORS_H_
#define TEST_CRYPTODEV_HASH_TEST_VECTORS_H_

#ifdef RTE_CRYPTO_AESNI_MB
#include <intel-ipsec-mb.h>
#endif

static const uint8_t plaintext_hash[] = {
	"What a lousy earth! He wondered how many people "
	"were destitute that same night even in his own "
	"prosperous country, how many homes were "
	"shanties, how many husbands were drunk and "
	"wives socked, and how many children were "
	"bullied, abused, or abandoned. How many "
	"families hungered for food they could not "
	"afford to buy? How many hearts were broken? How "
	"many suicides would take place that same night, "
	"how many people would go insane? How many "
	"cockroaches and landlords would triumph? How "
	"many winners were losers, successes failures, "
	"and rich men poor men? How many wise guys were "
	"stupid? How many happy endings were unhappy "
	"endings? How many honest men were liars, brave "
	"men cowards, loyal men traitors, how many "
	"sainted men were corrupt, how many people in "
	"positions of trust had sold their souls to "
	"bodyguards, how many had never had souls? How "
	"many straight-and-narrow paths were crooked "
	"paths? How many best families were worst "
	"families and how many good people were bad "
	"people? When you added them all up and then "
	"subtracted, you might be left with only the "
	"children, and perhaps with Albert Einstein and "
	"an old violinist or sculptor somewhere."
};

static const struct blockcipher_test_data
md5_test_vector = {
	.auth_algo = RTE_CRYPTO_AUTH_MD5,
	.ciphertext = {
		.data = plaintext_hash,
		.len = 512
	},
	.digest = {
		.data = {
			0xB3, 0xE6, 0xBB, 0x50, 0x41, 0x35, 0x3C, 0x6B,
			0x7A, 0xFF, 0xD2, 0x64, 0xAF, 0xD5, 0x1C, 0xB2
		},
		.len = 16
	}
};

static const struct blockcipher_test_data
hmac_md5_test_vector = {
	.auth_algo = RTE_CRYPTO_AUTH_MD5_HMAC,
	.ciphertext = {
		.data = plaintext_hash,
		.len = 512
	},
	.auth_key = {
		.data = {
			0xF8, 0x2A, 0xC7, 0x54, 0xDB, 0x96, 0x18, 0xAA,
			0xC3, 0xA1, 0x53, 0xF6, 0x1F, 0x17, 0x60, 0xBD
		},
		.len = 16
	},
	.digest = {
		.data = {
			0x50, 0xE8, 0xDE, 0xC5, 0xC1, 0x76, 0xAC, 0xAE,
			0x15, 0x4A, 0xF1, 0x7F, 0x7E, 0x04, 0x42, 0x9B
		},
		.len = 16,
		.truncated_len = 12
	}
};

static const struct blockcipher_test_data
sha1_test_vector = {
	.auth_algo = RTE_CRYPTO_AUTH_SHA1,
	.ciphertext = {
		.data = plaintext_hash,
		.len = 512
	},
	.digest = {
		.data = {
			0xA2, 0x8D, 0x40, 0x78, 0xDD, 0x9F, 0xBB, 0xD5,
			0x35, 0x62, 0xFB, 0xFA, 0x93, 0xFD, 0x7D, 0x70,
			0xA6, 0x7D, 0x45, 0xCA
		},
		.len = 20,
		.truncated_len = 20
	}
};

static const struct blockcipher_test_data
hmac_sha1_test_vector = {
	.auth_algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
	.ciphertext = {
		.data = plaintext_hash,
		.len = 512
	},
	.auth_key = {
		.data = {
			0xF8, 0x2A, 0xC7, 0x54, 0xDB, 0x96, 0x18, 0xAA,
			0xC3, 0xA1, 0x53, 0xF6, 0x1F, 0x17, 0x60, 0xBD,
			0xDE, 0xF4, 0xDE, 0xAD
		},
		.len = 20
	},
	.digest = {
		.data = {
			0xC4, 0xB7, 0x0E, 0x6B, 0xDE, 0xD1, 0xE7, 0x77,
			0x7E, 0x2E, 0x8F, 0xFC, 0x48, 0x39, 0x46, 0x17,
			0x3F, 0x91, 0x64, 0x59
		},
		.len = 20,
		.truncated_len = 12
	}
};

static const struct blockcipher_test_data
sha224_test_vector = {
	.auth_algo = RTE_CRYPTO_AUTH_SHA224,
	.ciphertext = {
		.data = plaintext_hash,
		.len = 512
	},
	.digest = {
		.data = {
			0x91, 0xE7, 0xCD, 0x75, 0x14, 0x9C, 0xA9, 0xE9,
			0x2E, 0x46, 0x12, 0x20, 0x22, 0xF9, 0x68, 0x28,
			0x39, 0x26, 0xDF, 0xB5, 0x78, 0x62, 0xB2, 0x6E,
			0x5E, 0x8F, 0x25, 0x84
		},
		.len = 28,
		.truncated_len = 28
	}
};

static const struct blockcipher_test_data
hmac_sha224_test_vector = {
	.auth_algo = RTE_CRYPTO_AUTH_SHA224_HMAC,
	.ciphertext = {
		.data = plaintext_hash,
		.len = 512
	},
	.auth_key = {
		.data = {
			0xF8, 0x2A, 0xC7, 0x54, 0xDB, 0x96, 0x18, 0xAA,
			0xC3, 0xA1, 0x53, 0xF6, 0x1F, 0x17, 0x60, 0xBD,
			0xDE, 0xF4, 0xDE, 0xAD, 0x26, 0xEB, 0xAB, 0x92,
			0xFB, 0xBF, 0xB0, 0x8C
		},
		.len = 28
	},
	.digest = {
		.data = {
			0x70, 0x0F, 0x04, 0x4D, 0x22, 0x02, 0x7D, 0x31,
			0x36, 0xDA, 0x77, 0x19, 0xB9, 0x66, 0x37, 0x7B,
			0xF1, 0x8A, 0x63, 0xBB, 0x5D, 0x1D, 0xE3, 0x9F,
			0x92, 0xF6, 0xAA, 0x19
		},
		.len = 28,
		.truncated_len = 14
	}
};

static const struct blockcipher_test_data
sha256_test_vector = {
	.auth_algo = RTE_CRYPTO_AUTH_SHA256,
	.ciphertext = {
		.data = plaintext_hash,
		.len = 512
	},
	.digest = {
		.data = {
			0x7F, 0xF1, 0x0C, 0xF5, 0x90, 0x97, 0x19, 0x0F,
			0x00, 0xE4, 0x83, 0x01, 0xCA, 0x59, 0x00, 0x2E,
			0x1F, 0xC7, 0x84, 0xEE, 0x76, 0xA6, 0x39, 0x15,
			0x76, 0x2F, 0x87, 0xF9, 0x01, 0x06, 0xF3, 0xB7
		},
		.len = 32,
		.truncated_len = 32
	}
};

static const struct blockcipher_test_data
hmac_sha256_test_vector = {
	.auth_algo = RTE_CRYPTO_AUTH_SHA256_HMAC,
	.ciphertext = {
		.data = plaintext_hash,
		.len = 512
	},
	.auth_key = {
		.data = {
			0xF8, 0x2A, 0xC7, 0x54, 0xDB, 0x96, 0x18, 0xAA,
			0xC3, 0xA1, 0x53, 0xF6, 0x1F, 0x17, 0x60, 0xBD,
			0xDE, 0xF4, 0xDE, 0xAD, 0x26, 0xEB, 0xAB, 0x92,
			0xFB, 0xBF, 0xB0, 0x8C, 0x29, 0x87, 0x90, 0xAC
		},
		.len = 32
	},
	.digest = {
		.data = {
			0xAF, 0x8F, 0x70, 0x1B, 0x4B, 0xAF, 0x34, 0xCB,
			0x02, 0x24, 0x48, 0x45, 0x83, 0x52, 0x8F, 0x22,
			0x06, 0x4D, 0x64, 0x09, 0x0A, 0xCC, 0x02, 0x77,
			0x71, 0x83, 0x48, 0x71, 0x07, 0x02, 0x25, 0x17
		},
		.len = 32,
		.truncated_len = 16
	}
};

static const struct blockcipher_test_data
sha384_test_vector = {
	.auth_algo = RTE_CRYPTO_AUTH_SHA384,
	.ciphertext = {
		.data = plaintext_hash,
		.len = 512
	},
	.digest = {
		.data = {
			0x1D, 0xE7, 0x3F, 0x55, 0x86, 0xFE, 0x48, 0x9F,
			0xAC, 0xC6, 0x85, 0x32, 0xFA, 0x8E, 0xA6, 0x77,
			0x25, 0x84, 0xA5, 0x98, 0x8D, 0x0B, 0x80, 0xF4,
			0xEB, 0x2C, 0xFB, 0x6C, 0xEA, 0x7B, 0xFD, 0xD5,
			0xAD, 0x41, 0xAB, 0x15, 0xB0, 0x03, 0x15, 0xEC,
			0x9E, 0x3D, 0xED, 0xCB, 0x80, 0x7B, 0xF4, 0xB6
		},
		.len = 48,
		.truncated_len = 48
	}
};

static const struct blockcipher_test_data
hmac_sha384_test_vector = {
	.auth_algo = RTE_CRYPTO_AUTH_SHA384_HMAC,
	.ciphertext = {
		.data = plaintext_hash,
		.len = 512
	},
	.auth_key = {
		.data = {
			0xF8, 0x2A, 0xC7, 0x54, 0xDB, 0x96, 0x18, 0xAA,
			0xC3, 0xA1, 0x53, 0xF6, 0x1F, 0x17, 0x60, 0xBD,
			0xDE, 0xF4, 0xDE, 0xAD, 0x26, 0xEB, 0xAB, 0x92,
			0xFB, 0xBF, 0xB0, 0x8C, 0x29, 0x87, 0x90, 0xAC,
			0x39, 0x8B, 0x5C, 0x49, 0x68, 0x1E, 0x3A, 0x05,
			0xCC, 0x68, 0x5C, 0x76, 0xCB, 0x3C, 0x71, 0x89
		},
		.len = 48
	},
	.digest = {
		.data = {
			0xE2, 0x83, 0x18, 0x55, 0xB5, 0x8D, 0x94, 0x9B,
			0x01, 0xB6, 0xE2, 0x57, 0x7A, 0x62, 0xF5, 0xF4,
			0xAB, 0x39, 0xF3, 0x3C, 0x28, 0xA0, 0x0F, 0xCC,
			0xEE, 0x1C, 0xF1, 0xF8, 0x69, 0xF1, 0x24, 0x3B,
			0x10, 0x90, 0x0A, 0xE3, 0xF0, 0x59, 0xDD, 0xC0,
			0x6F, 0xE6, 0x8C, 0x84, 0xD5, 0x03, 0xF8, 0x9E
		},
		.len = 48,
		.truncated_len = 24
	}
};

static const struct blockcipher_test_data
sha512_test_vector = {
	.auth_algo = RTE_CRYPTO_AUTH_SHA512,
	.ciphertext = {
		.data = plaintext_hash,
		.len = 512
	},
	.digest = {
		.data = {
			0xB9, 0xBA, 0x28, 0x48, 0x3C, 0xC2, 0xD3, 0x65,
			0x4A, 0xD6, 0x00, 0x1D, 0xCE, 0x61, 0x64, 0x54,
			0x45, 0x8C, 0x64, 0x0E, 0xED, 0x0E, 0xD8, 0x1C,
			0x72, 0xCE, 0xD2, 0x44, 0x91, 0xC8, 0xEB, 0xC7,
			0x99, 0xC5, 0xCA, 0x89, 0x72, 0x64, 0x96, 0x41,
			0xC8, 0xEA, 0xB2, 0x4E, 0xD1, 0x21, 0x13, 0x49,
			0x64, 0x4E, 0x15, 0x68, 0x12, 0x67, 0x26, 0x0F,
			0x2C, 0x3C, 0x83, 0x25, 0x27, 0x86, 0xF0, 0xDB
		},
		.len = 64,
		.truncated_len = 64
	}
};

static const struct blockcipher_test_data
hmac_sha512_test_vector = {
	.auth_algo = RTE_CRYPTO_AUTH_SHA512_HMAC,
	.ciphertext = {
		.data = plaintext_hash,
		.len = 512
	},
	.auth_key = {
		.data = {
			0xF8, 0x2A, 0xC7, 0x54, 0xDB, 0x96, 0x18, 0xAA,
			0xC3, 0xA1, 0x53, 0xF6, 0x1F, 0x17, 0x60, 0xBD,
			0xDE, 0xF4, 0xDE, 0xAD, 0x26, 0xEB, 0xAB, 0x92,
			0xFB, 0xBF, 0xB0, 0x8C, 0x29, 0x87, 0x90, 0xAC,
			0x39, 0x8B, 0x5C, 0x49, 0x68, 0x1E, 0x3A, 0x05,
			0xCC, 0x68, 0x5C, 0x76, 0xCB, 0x3C, 0x71, 0x89,
			0xDE, 0xAA, 0x36, 0x44, 0x98, 0x93, 0x97, 0x1E,
			0x6D, 0x53, 0x83, 0x87, 0xB3, 0xB7, 0x56, 0x41
		},
		.len = 64
	},
	.digest = {
		.data = {
			0xB8, 0x0B, 0x35, 0x97, 0x3F, 0x24, 0x3F, 0x05,
			0x2A, 0x7F, 0x2F, 0xD8, 0xD7, 0x56, 0x58, 0xAD,
			0x6F, 0x8D, 0x1F, 0x4C, 0x30, 0xF9, 0xA8, 0x29,
			0x7A, 0xE0, 0x8D, 0x88, 0xF5, 0x2E, 0x94, 0xF5,
			0x06, 0xF7, 0x5D, 0x57, 0x32, 0xA8, 0x49, 0x29,
			0xEA, 0x6B, 0x6D, 0x95, 0xBD, 0x76, 0xF5, 0x79,
			0x97, 0x37, 0x0F, 0xBE, 0xC2, 0x45, 0xA0, 0x87,
			0xAF, 0x24, 0x27, 0x0C, 0x78, 0xBA, 0xBE, 0x20
		},
		.len = 64,
		.truncated_len = 32
	}
};

static const struct blockcipher_test_data
cmac_test_vector = {
	.auth_algo = RTE_CRYPTO_AUTH_AES_CMAC,
	.ciphertext = {
		.data = plaintext_hash,
		.len = 512
	},
	.auth_key = {
		.data = {
			0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
			0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
		},
		.len = 16
	},
	.digest = {
		.data = {
			0x4C, 0x77, 0x87, 0xA0, 0x78, 0x8E, 0xEA, 0x96,
			0xC1, 0xEB, 0x1E, 0x4E, 0x95, 0x8F, 0xED, 0x27
		},
		.len = 16,
		.truncated_len = 16
	}
};

static const struct blockcipher_test_data
aes_xcbc_mac_test_vector = {
	.auth_algo = RTE_CRYPTO_AUTH_AES_XCBC_MAC,
	.ciphertext = {
		.data = plaintext_hash,
		.len = 512
	},
	.auth_key = {
		.data = {
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
		},
		.len = 16
	},
	.digest = {
		.data = {
			0x07, 0xf1, 0xf5, 0x80, 0x5a, 0xbc, 0x1d, 0x1c,
			0x58, 0x43, 0x99, 0xbe

		},
		.len = 12,
		.truncated_len = 12
	}
};

static const struct blockcipher_test_data
null_auth_test_vector = {
	.auth_algo = RTE_CRYPTO_AUTH_NULL,
	.ciphertext = {		/* arbitrary data - shouldn't be used */
		.data = plaintext_hash,
		.len = 512
	},
	.auth_key = {		/* arbitrary data - shouldn't be used */
		.data = {
			0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
			0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
		},
		.len = 16
	},
	.digest = {
		.data = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00
		},
		.len = 20,
		.truncated_len = 12
	}
};

static const struct blockcipher_test_data
cmac_test_vector_12 = {
	.auth_algo = RTE_CRYPTO_AUTH_AES_CMAC,
	.ciphertext = {
		.data = plaintext_hash,
		.len = 512
	},
	.auth_key = {
		.data = {
			0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
			0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
		},
		.len = 16
	},
	.digest = {
		.data = {
			0x4C, 0x77, 0x87, 0xA0, 0x78, 0x8E, 0xEA, 0x96,
			0xC1, 0xEB, 0x1E, 0x4E, 0x95, 0x8F, 0xED, 0x27
		},
		.len = 12,
		.truncated_len = 12
	}
};

static const struct blockcipher_test_case hash_test_cases[] = {
	{
		.test_descr = "MD5 Digest",
		.test_data = &md5_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_GEN,
	},
	{
		.test_descr = "MD5 Digest Verify",
		.test_data = &md5_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_VERIFY,
	},
	{
		.test_descr = "HMAC-MD5 Digest",
		.test_data = &hmac_md5_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_GEN,
	},
	{
		.test_descr = "HMAC-MD5 Digest Verify",
		.test_data = &hmac_md5_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_VERIFY,
	},
	{
		.test_descr = "SHA1 Digest",
		.test_data = &sha1_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_GEN,
	},
	{
		.test_descr = "SHA1 Digest Verify",
		.test_data = &sha1_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_VERIFY,
	},
	{
		.test_descr = "HMAC-SHA1 Digest",
		.test_data = &hmac_sha1_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_GEN,
	},
	{
		.test_descr = "HMAC-SHA1 Digest Scatter Gather",
		.test_data = &hmac_sha1_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_GEN,
		.feature_mask = BLOCKCIPHER_TEST_FEATURE_SG,
	},
	{
		.test_descr = "HMAC-SHA1 Digest Verify",
		.test_data = &hmac_sha1_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_VERIFY,
	},
	{
		.test_descr = "HMAC-SHA1 Digest Verify Scatter Gather",
		.test_data = &hmac_sha1_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_VERIFY,
		.feature_mask = BLOCKCIPHER_TEST_FEATURE_SG,
	},
	{
		.test_descr = "SHA224 Digest",
		.test_data = &sha224_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_GEN,
	},
	{
		.test_descr = "SHA224 Digest Verify",
		.test_data = &sha224_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_VERIFY,
	},
	{
		.test_descr = "HMAC-SHA224 Digest",
		.test_data = &hmac_sha224_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_GEN,
	},
	{
		.test_descr = "HMAC-SHA224 Digest Verify",
		.test_data = &hmac_sha224_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_VERIFY,
	},
	{
		.test_descr = "SHA256 Digest",
		.test_data = &sha256_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_GEN,
	},
	{
		.test_descr = "SHA256 Digest Verify",
		.test_data = &sha256_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_VERIFY,
	},
	{
		.test_descr = "HMAC-SHA256 Digest",
		.test_data = &hmac_sha256_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_GEN,
	},
	{
		.test_descr = "HMAC-SHA256 Digest Verify",
		.test_data = &hmac_sha256_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_VERIFY,
	},
	{
		.test_descr = "SHA384 Digest",
		.test_data = &sha384_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_GEN,
	},
	{
		.test_descr = "SHA384 Digest Verify",
		.test_data = &sha384_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_VERIFY,
	},
	{
		.test_descr = "HMAC-SHA384 Digest",
		.test_data = &hmac_sha384_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_GEN,
	},
	{
		.test_descr = "HMAC-SHA384 Digest Verify",
		.test_data = &hmac_sha384_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_VERIFY,
	},
	{
		.test_descr = "SHA512 Digest",
		.test_data = &sha512_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_GEN,
	},
	{
		.test_descr = "SHA512 Digest Verify",
		.test_data = &sha512_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_VERIFY,
	},
	{
		.test_descr = "HMAC-SHA512 Digest",
		.test_data = &hmac_sha512_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_GEN,
	},
	{
		.test_descr = "HMAC-SHA512 Digest Verify",
		.test_data = &hmac_sha512_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_VERIFY,
	},
	{
		.test_descr = "CMAC Digest 12B",
		.test_data = &cmac_test_vector_12,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_GEN,
	},
	{
		.test_descr = "CMAC Digest Verify 12B",
		.test_data = &cmac_test_vector_12,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_VERIFY,
	},
	{
		.test_descr = "CMAC Digest 16B",
		.test_data = &cmac_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_GEN,
	},
	{
		.test_descr = "CMAC Digest Verify 16B",
		.test_data = &cmac_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_VERIFY,
	},
	{
		.test_descr = "NULL algo - auth generate",
		.test_data = &null_auth_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_GEN,
	},
	{
		.test_descr = "NULL algo - auth verify",
		.test_data = &null_auth_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_VERIFY,
	},
	{
		.test_descr = "NULL algo - auth generate - OOP",
		.test_data = &null_auth_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_GEN,
		.feature_mask = BLOCKCIPHER_TEST_FEATURE_OOP,
	},
	{
		.test_descr = "NULL algo - auth verify - OOP",
		.test_data = &null_auth_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_VERIFY,
		.feature_mask = BLOCKCIPHER_TEST_FEATURE_OOP,
	},
	{
		.test_descr = "AES-XCBC-MAC Digest 16B",
		.test_data = &aes_xcbc_mac_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_GEN,
	},
	{
		.test_descr = "AES-XCBC-MAC Digest Verify 16B",
		.test_data = &aes_xcbc_mac_test_vector,
		.op_mask = BLOCKCIPHER_TEST_OP_AUTH_VERIFY,
	},

};

#endif /* TEST_CRYPTODEV_HASH_TEST_VECTORS_H_ */
