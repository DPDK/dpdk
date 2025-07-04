/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 Marvell.
 */

#include "roc_api.h"

#define KEY128_ROUNDS		10		/* (Nr+1)*Nb */
#define KEY256_ROUNDS		14		/* (Nr+1)*Nb */
#define KEY_SCHEDULE_LEN(nr)	((nr + 1) * 4)	/* (Nr+1)*Nb words */
#define AES_HASH_KEY_LEN	16

/*
 * AES 128 implementation based on NIST FIPS 197 suitable for LittleEndian
 * https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.197.pdf
 */

/* Sbox from NIST FIPS 197 */
static uint8_t Sbox[] = {
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
	0xb0, 0x54, 0xbb, 0x16,
};

/* Substitute a byte with Sbox[byte]. Do it for a word for 4 bytes */
static uint32_t
sub_word(uint32_t word)
{
	word = (Sbox[(word >> 24) & 0xFF] << 24) |
	       (Sbox[(word >> 16) & 0xFF] << 16) |
	       (Sbox[(word >> 8) & 0xFF] << 8) | Sbox[word & 0xFF];
	return word;
}

/* Rotate a word by one byte */
static uint32_t
rot_word(uint32_t word)
{
	return ((word >> 8) & 0xFFFFFF) | (word << 24);
}

/*
 * Multiply with power of 2 and polynomial reduce the result using AES
 * polynomial
 */
static uint8_t
Xtime(uint8_t byte, uint8_t pow)
{
	uint32_t w = byte;

	while (pow) {
		w = w << 1;
		if (w >> 8)
			w ^= 0x11b;
		pow--;
	}

	return (uint8_t)w;
}

/*
 * Multiply a byte with another number such that the result is polynomial
 * reduced in the GF8 space
 */
static uint8_t
GF8mul(uint8_t byte, uint32_t mp)
{
	uint8_t pow, mul = 0;

	while (mp) {
		pow = ffs(mp) - 1;
		mul ^= Xtime(byte, pow);
		mp ^= (1 << pow);
	}
	return mul;
}

static void
aes_key_expand(const uint8_t *key, uint32_t len, uint32_t *ks)
{
	uint32_t len_words = len / sizeof(uint32_t);
	unsigned int schedule_len;
	unsigned int i = len_words;
	uint32_t temp;

	schedule_len = (len == ROC_CPT_AES128_KEY_LEN) ? KEY_SCHEDULE_LEN(KEY128_ROUNDS) :
							 KEY_SCHEDULE_LEN(KEY256_ROUNDS);
	/* Skip key in ks */
	memcpy(ks, key, len);

	while (i < schedule_len) {
		temp = ks[i - 1];
		if ((i & (len_words - 1)) == 0) {
			temp = rot_word(temp);
			temp = sub_word(temp);
			temp ^= (uint32_t)GF8mul(1, 1 << ((i / len_words) - 1));
		}
		if (len == ROC_CPT_AES256_KEY_LEN) {
			if ((i % len_words) == 4)
				temp = sub_word(temp);
		}
		ks[i] = ks[i - len_words] ^ temp;
		i++;
	}
}

/* Shift Rows(columns in state in this implementation) */
static void
shift_word(uint8_t *sRc, uint8_t c, int count)
{
	/* rotate across non-consecutive locations */
	while (count) {
		uint8_t t = sRc[c];

		sRc[c] = sRc[0x4 + c];
		sRc[0x4 + c] = sRc[0x8 + c];
		sRc[0x8 + c] = sRc[0xc + c];
		sRc[0xc + c] = t;
		count--;
	}
}

/* Mix Columns(rows in state in this implementation) */
static void
mix_columns(uint8_t *sRc)
{
	uint8_t new_st[4];
	int i;

	for (i = 0; i < 4; i++)
		new_st[i] = GF8mul(sRc[i], 0x2) ^
			    GF8mul(sRc[(i + 1) & 0x3], 0x3) ^
			    sRc[(i + 2) & 0x3] ^ sRc[(i + 3) & 0x3];
	for (i = 0; i < 4; i++)
		sRc[i] = new_st[i];
}

static void
cipher(uint8_t *in, uint8_t *out, uint32_t *ks, uint32_t key_rounds, uint8_t in_len)
{
	uint8_t data_word_len = in_len / sizeof(uint32_t);
	uint32_t state[data_word_len];
	unsigned int i, round;

	memcpy(state, in, sizeof(state));

	/* AddRoundKey(state, w[0, Nb-1]) // See Sec. 5.1.4 */
	for (i = 0; i < data_word_len; i++)
		state[i] ^= ks[i];

	for (round = 1; round < key_rounds; round++) {
		/* SubBytes(state) // See Sec. 5.1.1 */
		for (i = 0; i < data_word_len; i++)
			state[i] = sub_word(state[i]);

		/* ShiftRows(state) // See Sec. 5.1.2 */
		for (i = 0; i < data_word_len; i++)
			shift_word((uint8_t *)state, i, i);

		/* MixColumns(state) // See Sec. 5.1.3 */
		for (i = 0; i < data_word_len; i++)
			mix_columns((uint8_t *)&state[i]);

		/* AddRoundKey(state, w[round*Nb, (round+1)*Nb-1]) */
		for (i = 0; i < data_word_len; i++)
			state[i] ^= ks[round * data_word_len + i];
	}

	/* SubBytes(state) */
	for (i = 0; i < data_word_len; i++)
		state[i] = sub_word(state[i]);

	/* ShiftRows(state) */
	for (i = 0; i < data_word_len; i++)
		shift_word((uint8_t *)state, i, i);

	/* AddRoundKey(state, w[Nr*Nb, (Nr+1)*Nb-1]) */
	for (i = 0; i < data_word_len; i++)
		state[i] ^= ks[key_rounds * data_word_len + i];
	memcpy(out, state, data_word_len * sizeof(uint32_t));
}

void
roc_aes_xcbc_key_derive(const uint8_t *auth_key, uint8_t *derived_key)
{
	uint32_t aes_ks[KEY_SCHEDULE_LEN(KEY128_ROUNDS)] = {0};
	uint8_t k1[16] = {[0 ... 15] = 0x01};
	uint8_t k2[16] = {[0 ... 15] = 0x02};
	uint8_t k3[16] = {[0 ... 15] = 0x03};

	aes_key_expand(auth_key, ROC_CPT_AES_XCBC_KEY_LENGTH, aes_ks);

	cipher(k1, derived_key, aes_ks, KEY128_ROUNDS, sizeof(k1));
	derived_key += sizeof(k1);

	cipher(k2, derived_key, aes_ks, KEY128_ROUNDS, sizeof(k2));
	derived_key += sizeof(k2);

	cipher(k3, derived_key, aes_ks, KEY128_ROUNDS, sizeof(k3));
}

void
roc_aes_hash_key_derive(const uint8_t *key, uint16_t len, uint8_t hash_key[])
{
	uint8_t data[AES_HASH_KEY_LEN] = {0x0};

	if (len == ROC_CPT_AES128_KEY_LEN) {
		uint32_t aes_ks[KEY_SCHEDULE_LEN(KEY128_ROUNDS)] = {0};

		aes_key_expand(key, ROC_CPT_AES128_KEY_LEN, aes_ks);
		cipher(data, hash_key, aes_ks, KEY128_ROUNDS, sizeof(data));
	} else {
		uint32_t aes_ks[KEY_SCHEDULE_LEN(KEY256_ROUNDS)] = {0};

		aes_key_expand(key, ROC_CPT_AES256_KEY_LEN, aes_ks);
		cipher(data, hash_key, aes_ks, KEY256_ROUNDS, sizeof(data));
	}
}
