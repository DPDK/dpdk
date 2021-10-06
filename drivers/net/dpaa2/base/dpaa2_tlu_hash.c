/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 NXP
 */
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <rte_pmd_dpaa2.h>

static unsigned int sbox(unsigned int x)
{
	unsigned int a, b, c, d;
	unsigned int oa, ob, oc, od;

	a = x & 0x1;
	b = (x >> 1) & 0x1;
	c = (x >> 2) & 0x1;
	d = (x >> 3) & 0x1;

	oa = ((a & ~b & ~c & d) | (~a & b) | (~a & ~c & ~d) | (b & c)) & 0x1;
	ob = ((a & ~b & d) | (~a & c & ~d) | (b & ~c)) & 0x1;
	oc = ((a & ~b & c) | (a & ~b & ~d) | (~a & b & ~d) | (~a & c & ~d) |
	     (b & c & d)) & 0x1;
	od = ((a & ~b & c) | (~a & b & ~c) | (a & b & ~d) | (~a & c & d)) & 0x1;

	return ((od << 3) | (oc << 2) | (ob << 1) | oa);
}

static unsigned int sbox_tbl[16];

static int pbox_tbl[16] = {5, 9, 0, 13,
			7, 2, 11, 14,
			1, 4, 12, 8,
			3, 15, 6, 10 };

static unsigned int mix_tbl[8][16];

static unsigned int stage(unsigned int input)
{
	int sbox_out = 0;
	int pbox_out = 0;
	int i;

	/* mix */
	input ^= input >> 16; /* xor lower */
	input ^= input << 16; /* move original lower to upper */

	for (i = 0; i < 32; i += 4) /* sbox stage */
		sbox_out |= (sbox_tbl[(input >> i) & 0xf]) << i;

	/* permutation */
	for (i = 0; i < 16; i++)
		pbox_out |= ((sbox_out >> i) & 0x10001) << pbox_tbl[i];

	return pbox_out;
}

static unsigned int fast_stage(unsigned int input)
{
	int pbox_out = 0;
	int i;

	/* mix */
	input ^= input >> 16; /* xor lower */
	input ^= input << 16; /* move original lower to upper */

	for (i = 0; i < 32; i += 4) /* sbox stage */
		pbox_out |= mix_tbl[i >> 2][(input >> i) & 0xf];

	return pbox_out;
}

static unsigned int fast_hash32(unsigned int x)
{
	int i;

	for (i = 0; i < 4; i++)
		x = fast_stage(x);
	return x;
}

static unsigned int
byte_crc32(unsigned char data /* new byte for the crc calculation */,
	   unsigned old_crc /* crc result of the last iteration */)
{
	int i;
	unsigned int crc, polynom = 0xedb88320;
	/* the polynomial is built on the reversed version of
	 * the CRC polynomial with out the x64 element.
	 */

	crc = old_crc;
	for (i = 0; i < 8; i++, data >>= 1)
		crc = (crc >> 1) ^ (((crc ^ data) & 0x1) ? polynom : 0);
		/* xor with polynomial is lsb of crc^data is 1 */

	return crc;
}

static unsigned int crc32_table[256];

static void init_crc32_table(void)
{
	int i;

	for (i = 0; i < 256; i++)
		crc32_table[i] = byte_crc32((unsigned char)i, 0LL);
}

static unsigned int
crc32_string(unsigned char *data,
	     int size, unsigned int old_crc)
{
	unsigned int crc;
	int i;

	crc = old_crc;
	for (i = 0; i < size; i++)
		crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xff];

	return crc;
}

static void hash_init(void)
{
	init_crc32_table();
	int i, j;

	for (i = 0; i < 16; i++)
		sbox_tbl[i] = sbox(i);

	for (i = 0; i < 32; i += 4)
		for (j = 0; j < 16; j++) {
			/* (a,b)
			 * (b,a^b)=(X,Y)
			 * (X^Y,X)
			 */
			unsigned int input = (0x88888888 ^ (8 << i)) | (j << i);

			input ^= input << 16; /* (X^Y,Y) */
			input ^= input >> 16; /* (X^Y,X) */
			mix_tbl[i >> 2][j] = stage(input);
		}
}

uint32_t rte_pmd_dpaa2_get_tlu_hash(uint8_t *data, int size)
{
	static int init;

	if (~init)
		hash_init();
	init = 1;
	return fast_hash32(crc32_string(data, size, 0x0));
}
