/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 ZTE Corporation
 */

#include "cryptodev_pmd.h"
#include "rte_byteorder.h"

#include "zsda_crypto_pmd.h"
#include "zsda_crypto_session.h"

/**************** AES KEY EXPANSION ****************/
/**
 * AES S-boxes
 * Sbox table: 8bits input convert to 8bits output
 **/
static const unsigned char aes_sbox[256] = {
	/* 0     1    2      3     4    5     6     7      8    9     A      B
	 * C     D    E     F
	 */
	0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b,
	0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
	0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26,
	0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
	0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2,
	0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
	0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed,
	0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
	0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f,
	0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
	0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec,
	0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
	0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14,
	0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
	0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d,
	0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
	0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f,
	0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
	0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11,
	0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
	0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f,
	0xb0, 0x54, 0xbb, 0x16};

/**
 * The round constant word array, rcon[i]
 *
 * From Wikipedia's article on the Rijndael key schedule @
 * https://en.wikipedia.org/wiki/Rijndael_key_schedule#rcon "Only the first some
 * of these constants are actually used – up to rcon[10] for AES-128 (as 11
 * round keys are needed), up to rcon[8] for AES-192, up to rcon[7] for AES-256.
 * rcon[0] is not used in AES algorithm."
 */
static const unsigned char rcon[11] = {0x8d, 0x01, 0x02, 0x04, 0x08, 0x10,
				       0x20, 0x40, 0x80, 0x1b, 0x36};

#define GET_AES_SBOX_VAL(num) (aes_sbox[(num)])

/**
 *rotate shift left marco definition
 *
 **/
#define SHL(x, n)  (((x)&0xFFFFFFFF) << n)
#define ROTL(x, n) (SHL((x), n) | ((x) >> (32 - n)))

/**
 * SM4 S-boxes
 * Sbox table: 8bits input convert to 8 bitg288s output
 **/
static unsigned char sm4_sbox[16][16] = {
	{0xd6, 0x90, 0xe9, 0xfe, 0xcc, 0xe1, 0x3d, 0xb7, 0x16, 0xb6, 0x14, 0xc2,
	 0x28, 0xfb, 0x2c, 0x05},
	{0x2b, 0x67, 0x9a, 0x76, 0x2a, 0xbe, 0x04, 0xc3, 0xaa, 0x44, 0x13, 0x26,
	 0x49, 0x86, 0x06, 0x99},
	{0x9c, 0x42, 0x50, 0xf4, 0x91, 0xef, 0x98, 0x7a, 0x33, 0x54, 0x0b, 0x43,
	 0xed, 0xcf, 0xac, 0x62},
	{0xe4, 0xb3, 0x1c, 0xa9, 0xc9, 0x08, 0xe8, 0x95, 0x80, 0xdf, 0x94, 0xfa,
	 0x75, 0x8f, 0x3f, 0xa6},
	{0x47, 0x07, 0xa7, 0xfc, 0xf3, 0x73, 0x17, 0xba, 0x83, 0x59, 0x3c, 0x19,
	 0xe6, 0x85, 0x4f, 0xa8},
	{0x68, 0x6b, 0x81, 0xb2, 0x71, 0x64, 0xda, 0x8b, 0xf8, 0xeb, 0x0f, 0x4b,
	 0x70, 0x56, 0x9d, 0x35},
	{0x1e, 0x24, 0x0e, 0x5e, 0x63, 0x58, 0xd1, 0xa2, 0x25, 0x22, 0x7c, 0x3b,
	 0x01, 0x21, 0x78, 0x87},
	{0xd4, 0x00, 0x46, 0x57, 0x9f, 0xd3, 0x27, 0x52, 0x4c, 0x36, 0x02, 0xe7,
	 0xa0, 0xc4, 0xc8, 0x9e},
	{0xea, 0xbf, 0x8a, 0xd2, 0x40, 0xc7, 0x38, 0xb5, 0xa3, 0xf7, 0xf2, 0xce,
	 0xf9, 0x61, 0x15, 0xa1},
	{0xe0, 0xae, 0x5d, 0xa4, 0x9b, 0x34, 0x1a, 0x55, 0xad, 0x93, 0x32, 0x30,
	 0xf5, 0x8c, 0xb1, 0xe3},
	{0x1d, 0xf6, 0xe2, 0x2e, 0x82, 0x66, 0xca, 0x60, 0xc0, 0x29, 0x23, 0xab,
	 0x0d, 0x53, 0x4e, 0x6f},
	{0xd5, 0xdb, 0x37, 0x45, 0xde, 0xfd, 0x8e, 0x2f, 0x03, 0xff, 0x6a, 0x72,
	 0x6d, 0x6c, 0x5b, 0x51},
	{0x8d, 0x1b, 0xaf, 0x92, 0xbb, 0xdd, 0xbc, 0x7f, 0x11, 0xd9, 0x5c, 0x41,
	 0x1f, 0x10, 0x5a, 0xd8},
	{0x0a, 0xc1, 0x31, 0x88, 0xa5, 0xcd, 0x7b, 0xbd, 0x2d, 0x74, 0xd0, 0x12,
	 0xb8, 0xe5, 0xb4, 0xb0},
	{0x89, 0x69, 0x97, 0x4a, 0x0c, 0x96, 0x77, 0x7e, 0x65, 0xb9, 0xf1, 0x09,
	 0xc5, 0x6e, 0xc6, 0x84},
	{0x18, 0xf0, 0x7d, 0xec, 0x3a, 0xdc, 0x4d, 0x20, 0x79, 0xee, 0x5f, 0x3e,
	 0xd7, 0xcb, 0x39, 0x48},
};

/* System parameter */
static const unsigned int fk[4] = {0xa3b1bac6, 0x56aa3350, 0x677d9197,
				   0xb27022dc};

/* fixed parameter */
static const unsigned int ck[32] = {
	0x00070e15, 0x1c232a31, 0x383f464d, 0x545b6269, 0x70777e85, 0x8c939aa1,
	0xa8afb6bd, 0xc4cbd2d9, 0xe0e7eef5, 0xfc030a11, 0x181f262d, 0x343b4249,
	0x50575e65, 0x6c737a81, 0x888f969d, 0xa4abb2b9, 0xc0c7ced5, 0xdce3eaf1,
	0xf8ff060d, 0x141b2229, 0x30373e45, 0x4c535a61, 0x686f767d, 0x848b9299,
	0xa0a7aeb5, 0xbcc3cad1, 0xd8dfe6ed, 0xf4fb0209, 0x10171e25, 0x2c333a41,
	0x484f565d, 0x646b7279};

/*
 * private function:
 * look up in SM4 S-boxes and get the related value.
 * args:    [in] inch: 0x00~0xFF (8 bits unsigned value).
 */
static unsigned char
sm4_sbox_cal(unsigned char inch)
{
	unsigned char *ptable = (unsigned char *)sm4_sbox;
	unsigned char ret = (unsigned char)(ptable[inch]);
	return ret;
}

/* private function:
 * Calculating round encryption key.
 * args:    [in] ka: ka is a 32 bits unsigned value;
 * return:  sk[i]: i{0,1,2,3,...31}.
 */
static unsigned int
sm4_cirk_cal(unsigned int ka)
{
	unsigned int bb = 0;
	unsigned int rk = 0;
	unsigned char a[4] = {0};
	unsigned char b[4];

	uint32_t be_ka = rte_cpu_to_be_32(ka);
	*(uint32_t *)&a[0] = be_ka;
	b[0] = sm4_sbox_cal(a[0]);
	b[1] = sm4_sbox_cal(a[1]);
	b[2] = sm4_sbox_cal(a[2]);
	b[3] = sm4_sbox_cal(a[3]);
	bb = rte_be_to_cpu_32(*(uint32_t *)&b[0]);
	rk = bb ^ (ROTL(bb, 13)) ^ (ROTL(bb, 23));
	return rk;
}

static void
zsda_sm4_key_expansion(unsigned int sk[32], const uint8_t key[16])
{
	unsigned int mk[4];
	unsigned int k[36];
	unsigned int i = 0;

	mk[0] = rte_be_to_cpu_32(*(const uint32_t *)&key[0]);
	mk[1] = rte_be_to_cpu_32(*(const uint32_t *)&key[4]);
	mk[2] = rte_be_to_cpu_32(*(const uint32_t *)&key[8]);
	mk[3] = rte_be_to_cpu_32(*(const uint32_t *)&key[12]);

	k[0] = mk[0] ^ fk[0];
	k[1] = mk[1] ^ fk[1];
	k[2] = mk[2] ^ fk[2];
	k[3] = mk[3] ^ fk[3];
	for (; i < 32; i++) {
		k[i + 4] = k[i] ^
			   (sm4_cirk_cal(k[i + 1] ^ k[i + 2] ^ k[i + 3] ^ ck[i]));
		sk[i] = k[i + 4];
	}
}

static void
u32_to_u8(uint32_t *u_int32_t_data, uint8_t *u8_data)
{
	uint32_t be_data = rte_cpu_to_be_32(*u_int32_t_data);

	rte_memcpy(u8_data, &be_data, sizeof(be_data));
}

static void
zsda_aes_key_expansion(uint8_t *round_key, uint32_t round_num,
		       const uint8_t *key, uint32_t key_len)
{
	uint32_t i, j, k, nk, nr;
	uint8_t tempa[4];

	nk = key_len >> 2;
	nr = round_num;

	/* The first round key is the key itself. */
	for (i = 0; i < nk; ++i) {
		round_key[(i * 4) + 0] = key[(i * 4) + 0];

		round_key[(i * 4) + 1] = key[(i * 4) + 1];

		round_key[(i * 4) + 2] = key[(i * 4) + 2];
		round_key[(i * 4) + 3] = key[(i * 4) + 3];
	}

	/* All other round keys are found from the previous round keys. */
	for (i = nk; i < (4 * (nr + 1)); ++i) {
		k = (i - 1) * 4;
		tempa[0] = round_key[k + 0];
		tempa[1] = round_key[k + 1];
		tempa[2] = round_key[k + 2];
		tempa[3] = round_key[k + 3];

		if ((nk != 0) && ((i % nk) == 0)) {
			/* This function shifts the 4 bytes in a word to the
			 * left once. [a0,a1,a2,a3] becomes [a1,a2,a3,a0]
			 * Function RotWord()
			 */
			{
				const uint8_t u8tmp = tempa[0];

				tempa[0] = tempa[1];
				tempa[1] = tempa[2];
				tempa[2] = tempa[3];
				tempa[3] = u8tmp;
			}

			/* SubWord() is a function that takes a four-byte input
			 * word and applies the S-box to each of the four bytes
			 * to produce an output word. Function Subword()
			 */
			{
				tempa[0] = GET_AES_SBOX_VAL(tempa[0]);
				tempa[1] = GET_AES_SBOX_VAL(tempa[1]);
				tempa[2] = GET_AES_SBOX_VAL(tempa[2]);
				tempa[3] = GET_AES_SBOX_VAL(tempa[3]);
			}

			tempa[0] = tempa[0] ^ rcon[i / nk];
		}

		if (nk == 8) {
			if ((i % nk) == 4) {
				/* Function Subword() */
				{
					tempa[0] = GET_AES_SBOX_VAL(tempa[0]);
					tempa[1] = GET_AES_SBOX_VAL(tempa[1]);
					tempa[2] = GET_AES_SBOX_VAL(tempa[2]);
					tempa[3] = GET_AES_SBOX_VAL(tempa[3]);
				}
			}
		}

		j = i * 4;
		k = (i - nk) * 4;
		round_key[j + 0] = round_key[k + 0] ^ tempa[0];
		round_key[j + 1] = round_key[k + 1] ^ tempa[1];
		round_key[j + 2] = round_key[k + 2] ^ tempa[2];
		round_key[j + 3] = round_key[k + 3] ^ tempa[3];
	}
}

void
zsda_reverse_memcpy(uint8_t *dst, const uint8_t *src, size_t n)
{
	size_t i;

	for (i = 0; i < n; ++i)
		dst[n - 1 - i] = src[i];
}

static void
zsda_decry_key_set(uint8_t key[64], const uint8_t *key1_ptr, uint8_t skey_len,
	      enum rte_crypto_cipher_algorithm algo)
{
	uint8_t round_num;
	uint8_t dec_key1[ZSDA_AES_MAX_KEY_BYTE_LEN] = {0};
	uint8_t aes_round_key[ZSDA_AES_MAX_EXP_BYTE_SIZE] = {0};
	uint32_t sm4_round_key[ZSDA_SM4_MAX_EXP_DWORD_SIZE] = {0};

	switch (algo) {
	case RTE_CRYPTO_CIPHER_AES_XTS:
		round_num = (skey_len == ZSDA_SYM_XTS_256_SKEY_LEN)
				    ? ZSDA_AES256_ROUND_NUM
				    : ZSDA_AES512_ROUND_NUM;
		zsda_aes_key_expansion(aes_round_key, round_num, key1_ptr,
				       skey_len);
		rte_memcpy(dec_key1,
			   ((uint8_t *)aes_round_key + (16 * round_num)), 16);

		if (skey_len == ZSDA_SYM_XTS_512_SKEY_LEN &&
			(16 * round_num) <= ZSDA_AES_MAX_EXP_BYTE_SIZE) {
			for (int i = 0; i < 16; i++) {
				dec_key1[i + 16] =
					aes_round_key[(16 * (round_num - 1)) + i];
			}
		}
		break;
	case RTE_CRYPTO_CIPHER_SM4_XTS:
		zsda_sm4_key_expansion(sm4_round_key, key1_ptr);
		for (size_t i = 0; i < 4; i++)
			u32_to_u8((uint32_t *)sm4_round_key +
					  ZSDA_SM4_MAX_EXP_DWORD_SIZE - 1 - i,
				  dec_key1 + (4 * i));
		break;
	default:
		ZSDA_LOG(ERR, "unknown cipher algo!");
		return;
	}

	if (skey_len == ZSDA_SYM_XTS_256_SKEY_LEN) {
		zsda_reverse_memcpy((uint8_t *)key + ZSDA_SYM_XTS_256_KEY2_OFF,
			       key1_ptr + skey_len, skey_len);
		zsda_reverse_memcpy((uint8_t *)key + ZSDA_SYM_XTS_256_KEY1_OFF,
			       dec_key1, skey_len);
	} else {
		zsda_reverse_memcpy(key, key1_ptr + skey_len, skey_len);
		zsda_reverse_memcpy((uint8_t *)key + ZSDA_SYM_XTS_512_KEY1_OFF,
			       dec_key1, skey_len);
	}
}

static uint8_t
zsda_sym_lbads(uint32_t dataunit_len)
{
	uint8_t lbads;

	switch (dataunit_len) {
	case ZSDA_AES_LBADS_512:
		lbads = ZSDA_AES_LBADS_INDICATE_512;
		break;
	case ZSDA_AES_LBADS_4096:
		lbads = ZSDA_AES_LBADS_INDICATE_4096;
		break;
	case ZSDA_AES_LBADS_8192:
		lbads = ZSDA_AES_LBADS_INDICATE_8192;
		break;
	case ZSDA_AES_LBADS_0:
		lbads = ZSDA_AES_LBADS_INDICATE_0;
		break;
	default:
		ZSDA_LOG(ERR, "dataunit_len should be 0/512/4096/8192 - %d.",
			 dataunit_len);
		lbads = ZSDA_AES_LBADS_INDICATE_INVALID;
		break;
	}
	return lbads;
}

static int
zsda_session_cipher_set(struct zsda_sym_session *sess,
				   struct rte_crypto_cipher_xform *cipher_xform)
{
	uint8_t skey_len = 0;
	const uint8_t *key1_ptr = NULL;

	if (cipher_xform->key.length > ZSDA_CIPHER_KEY_MAX_LEN) {
		ZSDA_LOG(ERR, "key length not supported");
		return -EINVAL;
	}

	sess->chain_order = ZSDA_SYM_CHAIN_ONLY_CIPHER;
	sess->cipher.iv.offset = cipher_xform->iv.offset;
	sess->cipher.iv.length = cipher_xform->iv.length;
	sess->cipher.op = cipher_xform->op;
	sess->cipher.algo = cipher_xform->algo;
	sess->cipher.dataunit_len = cipher_xform->dataunit_len;
	sess->cipher.lbads = zsda_sym_lbads(cipher_xform->dataunit_len);
	if (sess->cipher.lbads == 0xff) {
		ZSDA_LOG(ERR, "dataunit_len wrong!");
		return -EINVAL;
	}

	skey_len = (cipher_xform->key.length / 2) & 0xff;

	if (sess->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
		sess->cipher.key_encry.length = cipher_xform->key.length;
		if (skey_len == ZSDA_SYM_XTS_256_SKEY_LEN) {
			zsda_reverse_memcpy((uint8_t *)sess->cipher.key_encry.data +
					       ZSDA_SYM_XTS_256_KEY2_OFF,
				       (cipher_xform->key.data + skey_len),
				       skey_len);
			zsda_reverse_memcpy(((uint8_t *)sess->cipher.key_encry.data +
					ZSDA_SYM_XTS_256_KEY1_OFF),
				       cipher_xform->key.data, skey_len);
		} else
			zsda_reverse_memcpy((uint8_t *)sess->cipher.key_encry.data,
				       cipher_xform->key.data,
				       cipher_xform->key.length);
	} else if (sess->cipher.op == RTE_CRYPTO_CIPHER_OP_DECRYPT) {
		sess->cipher.key_decry.length = cipher_xform->key.length;
		key1_ptr = cipher_xform->key.data;
		zsda_decry_key_set(sess->cipher.key_decry.data, key1_ptr, skey_len,
			      sess->cipher.algo);
	}

	return ZSDA_SUCCESS;
}

static int
zsda_session_auth_set(struct zsda_sym_session *sess,
				 struct rte_crypto_auth_xform *xform)
{
	sess->auth.op = xform->op;
	sess->auth.algo = xform->algo;
	sess->auth.digest_length = xform->digest_length;

	return ZSDA_SUCCESS;
}

static struct rte_crypto_auth_xform *
zsda_auth_xform_get(struct rte_crypto_sym_xform *xform)
{
	do {
		if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH)
			return &xform->auth;

		xform = xform->next;
	} while (xform);

	return NULL;
}

static struct rte_crypto_cipher_xform *
zsda_cipher_xform_get(struct rte_crypto_sym_xform *xform)
{
	do {
		if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER)
			return &xform->cipher;

		xform = xform->next;
	} while (xform);

	return NULL;
}

/** Configure the session from a crypto xform chain */
static enum zsda_sym_chain_order
zsda_crypto_chain_order_get(const struct rte_crypto_sym_xform *xform)
{
	enum zsda_sym_chain_order res = ZSDA_SYM_CHAIN_NOT_SUPPORTED;

	if (xform != NULL) {
		if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
			if (xform->next == NULL)
				res = ZSDA_SYM_CHAIN_ONLY_AUTH;
			else if (xform->next->type ==
					RTE_CRYPTO_SYM_XFORM_CIPHER)
				res = ZSDA_SYM_CHAIN_AUTH_CIPHER;
		}
		if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
			if (xform->next == NULL)
				res = ZSDA_SYM_CHAIN_ONLY_CIPHER;
			else if (xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH)
				res = ZSDA_SYM_CHAIN_CIPHER_AUTH;
		}
	}

	return res;
}

/* Set session cipher parameters */
int
zsda_crypto_session_parameters_set(void *sess_priv,
			 struct rte_crypto_sym_xform *xform)
{

	struct zsda_sym_session *sess = sess_priv;
	struct rte_crypto_cipher_xform *cipher_xform =
			zsda_cipher_xform_get(xform);
	struct rte_crypto_auth_xform *auth_xform =
			zsda_auth_xform_get(xform);

	int ret = ZSDA_SUCCESS;

	sess->chain_order = zsda_crypto_chain_order_get(xform);
	switch (sess->chain_order) {
	case ZSDA_SYM_CHAIN_ONLY_CIPHER:
		if (!cipher_xform) {
			ZSDA_LOG(ERR, "Failed! cipher_xform is NULL");
			return -EINVAL;
		}
		ret = zsda_session_cipher_set(sess, cipher_xform);
		break;
	case ZSDA_SYM_CHAIN_ONLY_AUTH:
		if (!auth_xform) {
			ZSDA_LOG(ERR, "Failed! auth_xform is NULL");
			return -EINVAL;
		}
		ret = zsda_session_auth_set(sess, auth_xform);
		break;

	default:
		ZSDA_LOG(ERR, "Invalid chain order");
		ret = -EINVAL;
		break;
	}

	return ret;
}
