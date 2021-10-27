/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 Marvell.
 */

#include "roc_api.h"

#define lrot32(bits, word) (((word) << (bits)) | ((word) >> (32 - (bits))))
#define rrot32(bits, word) lrot32(32 - (bits), word)
#define lrot64(bits, word) (((word) << (bits)) | ((word) >> (64 - (bits))))
#define rrot64(bits, word) lrot64(64 - (bits), word)

/*
 * Compute a partial hash with the assumption that msg is the first block.
 * Based on implementation from RFC 3174
 */
void
roc_hash_sha1_gen(uint8_t *msg, uint32_t *hash)
{
	const uint32_t _K[] = {/* Round Constants defined in SHA-1   */
			       0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6};

	const uint32_t _H[] = {/* Initial Hash constants defined in SHA-1 */
			       0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476,
			       0xC3D2E1F0};
	int i;
	uint32_t temp = 0;	/* Temporary word value */
	uint32_t W[80];		/* Word sequence */
	uint32_t A, B, C, D, E; /* Word buffers */

	/* Initialize the first 16 words in the array W */
	memcpy(&W[0], msg, 16 * sizeof(W[0]));

	for (i = 0; i < 16; i++)
		W[i] = htobe32(W[i]);

	for (i = 16; i < 80; i++)
		W[i] = lrot32(1, W[i - 3] ^ W[i - 8] ^ W[i - 14] ^ W[i - 16]);

	A = _H[0];
	B = _H[1];
	C = _H[2];
	D = _H[3];
	E = _H[4];

	for (i = 0; i < 80; i++) {
		if (i >= 0 && i <= 19)
			temp = ((B & C) | ((~B) & D)) + _K[0];
		else if (i >= 20 && i <= 39)
			temp = (B ^ C ^ D) + _K[1];
		else if (i >= 40 && i <= 59)
			temp = ((B & C) | (B & D) | (C & D)) + _K[2];
		else if (i >= 60 && i <= 79)
			temp = (B ^ C ^ D) + _K[3];

		temp = lrot32(5, A) + temp + E + W[i];
		E = D;
		D = C;
		C = lrot32(30, B);
		B = A;
		A = temp;
	}

	A += _H[0];
	B += _H[1];
	C += _H[2];
	D += _H[3];
	E += _H[4];
	hash[0] = htobe32(A);
	hash[1] = htobe32(B);
	hash[2] = htobe32(C);
	hash[3] = htobe32(D);
	hash[4] = htobe32(E);
}

/*
 * Compute a partial hash with the assumption that msg is the first block.
 * Based on implementation from RFC 3174
 */
void
roc_hash_sha256_gen(uint8_t *msg, uint32_t *hash)
{
	const uint32_t _K[] = {
		/* Round Constants defined in SHA-256   */
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b,
		0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
		0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7,
		0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
		0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152,
		0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
		0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
		0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
		0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
		0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
		0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
		0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

	const uint32_t _H[] = {/* Initial Hash constants defined in SHA-256 */
			       0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
			       0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
	int i;
	uint32_t temp[4], S0, S1;	 /* Temporary word value */
	uint32_t W[64];			 /* Word sequence */
	uint32_t A, B, C, D, E, F, G, H; /* Word buffers */

	/* Initialize the first 16 words in the array W */
	memcpy(&W[0], msg, 16 * sizeof(W[0]));

	for (i = 0; i < 16; i++)
		W[i] = htobe32(W[i]);

	for (i = 16; i < 64; i++) {
		S0 = rrot32(7, W[i - 15]) ^ rrot32(18, W[i - 15]) ^
		     (W[i - 15] >> 3);
		S1 = rrot32(17, W[i - 2]) ^ rrot32(19, W[i - 2]) ^
		     (W[i - 2] >> 10);
		W[i] = W[i - 16] + S0 + W[i - 7] + S1;
	}

	A = _H[0];
	B = _H[1];
	C = _H[2];
	D = _H[3];
	E = _H[4];
	F = _H[5];
	G = _H[6];
	H = _H[7];

	for (i = 0; i < 64; i++) {
		S1 = rrot32(6, E) ^ rrot32(11, E) ^ rrot32(25, E);
		temp[0] = (E & F) ^ ((~E) & G);
		temp[1] = H + S1 + temp[0] + _K[i] + W[i];
		S0 = rrot32(2, A) ^ rrot32(13, A) ^ rrot32(22, A);
		temp[2] = (A & B) ^ (A & C) ^ (B & C);
		temp[3] = S0 + temp[2];

		H = G;
		G = F;
		F = E;
		E = D + temp[1];
		D = C;
		C = B;
		B = A;
		A = temp[1] + temp[3];
	}

	A += _H[0];
	B += _H[1];
	C += _H[2];
	D += _H[3];
	E += _H[4];
	F += _H[5];
	G += _H[6];
	H += _H[7];
	hash[0] = htobe32(A);
	hash[1] = htobe32(B);
	hash[2] = htobe32(C);
	hash[3] = htobe32(D);
	hash[4] = htobe32(E);
	hash[5] = htobe32(F);
	hash[6] = htobe32(G);
	hash[7] = htobe32(H);
}

/*
 * Compute a partial hash with the assumption that msg is the first block.
 * Based on implementation from RFC 3174
 */
void
roc_hash_sha512_gen(uint8_t *msg, uint64_t *hash, int hash_size)
{
	const uint64_t _K[] = {
		/* Round Constants defined in SHA-512   */
		0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f,
		0xe9b5dba58189dbbc, 0x3956c25bf348b538, 0x59f111f1b605d019,
		0x923f82a4af194f9b, 0xab1c5ed5da6d8118, 0xd807aa98a3030242,
		0x12835b0145706fbe, 0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2,
		0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235,
		0xc19bf174cf692694, 0xe49b69c19ef14ad2, 0xefbe4786384f25e3,
		0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65, 0x2de92c6f592b0275,
		0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5,
		0x983e5152ee66dfab, 0xa831c66d2db43210, 0xb00327c898fb213f,
		0xbf597fc7beef0ee4, 0xc6e00bf33da88fc2, 0xd5a79147930aa725,
		0x06ca6351e003826f, 0x142929670a0e6e70, 0x27b70a8546d22ffc,
		0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed, 0x53380d139d95b3df,
		0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6,
		0x92722c851482353b, 0xa2bfe8a14cf10364, 0xa81a664bbc423001,
		0xc24b8b70d0f89791, 0xc76c51a30654be30, 0xd192e819d6ef5218,
		0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8,
		0x19a4c116b8d2d0c8, 0x1e376c085141ab53, 0x2748774cdf8eeb99,
		0x34b0bcb5e19b48a8, 0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb,
		0x5b9cca4f7763e373, 0x682e6ff3d6b2b8a3, 0x748f82ee5defb2fc,
		0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec,
		0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915,
		0xc67178f2e372532b, 0xca273eceea26619c, 0xd186b8c721c0c207,
		0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178, 0x06f067aa72176fba,
		0x0a637dc5a2c898a6, 0x113f9804bef90dae, 0x1b710b35131c471b,
		0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc,
		0x431d67c49c100d4c, 0x4cc5d4becb3e42b6, 0x597f299cfc657e2a,
		0x5fcb6fab3ad6faec, 0x6c44198c4a475817};

	const uint64_t _H384[] = {/* Initial Hash constants defined in SHA384 */
				  0xcbbb9d5dc1059ed8, 0x629a292a367cd507,
				  0x9159015a3070dd17, 0x152fecd8f70e5939,
				  0x67332667ffc00b31, 0x8eb44a8768581511,
				  0xdb0c2e0d64f98fa7, 0x47b5481dbefa4fa4};
	const uint64_t _H512[] = {/* Initial Hash constants defined in SHA512 */
				  0x6a09e667f3bcc908, 0xbb67ae8584caa73b,
				  0x3c6ef372fe94f82b, 0xa54ff53a5f1d36f1,
				  0x510e527fade682d1, 0x9b05688c2b3e6c1f,
				  0x1f83d9abfb41bd6b, 0x5be0cd19137e2179};
	int i;
	uint64_t temp[4], S0, S1;	 /* Temporary word value */
	uint64_t W[80];			 /* Word sequence */
	uint64_t A, B, C, D, E, F, G, H; /* Word buffers */
	const uint64_t *_H = (hash_size == 384) ? _H384 : _H512;

	/* Initialize the first 16 words in the array W */
	memcpy(&W[0], msg, 16 * sizeof(W[0]));

	for (i = 0; i < 16; i++)
		W[i] = htobe64(W[i]);

	for (i = 16; i < 80; i++) {
		S0 = rrot64(1, W[i - 15]) ^ rrot64(8, W[i - 15]) ^
		     (W[i - 15] >> 7);
		S1 = rrot64(19, W[i - 2]) ^ rrot64(61, W[i - 2]) ^
		     (W[i - 2] >> 6);
		W[i] = W[i - 16] + S0 + W[i - 7] + S1;
	}

	A = _H[0];
	B = _H[1];
	C = _H[2];
	D = _H[3];
	E = _H[4];
	F = _H[5];
	G = _H[6];
	H = _H[7];

	for (i = 0; i < 80; i++) {
		S1 = rrot64(14, E) ^ rrot64(18, E) ^ rrot64(41, E);
		temp[0] = (E & F) ^ ((~E) & G);
		temp[1] = H + S1 + temp[0] + _K[i] + W[i];
		S0 = rrot64(28, A) ^ rrot64(34, A) ^ rrot64(39, A);
		temp[2] = (A & B) ^ (A & C) ^ (B & C);
		temp[3] = S0 + temp[2];

		H = G;
		G = F;
		F = E;
		E = D + temp[1];
		D = C;
		C = B;
		B = A;
		A = temp[1] + temp[3];
	}

	A += _H[0];
	B += _H[1];
	C += _H[2];
	D += _H[3];
	E += _H[4];
	F += _H[5];
	G += _H[6];
	H += _H[7];
	hash[0] = htobe64(A);
	hash[1] = htobe64(B);
	hash[2] = htobe64(C);
	hash[3] = htobe64(D);
	hash[4] = htobe64(E);
	hash[5] = htobe64(F);
	hash[6] = htobe64(G);
	hash[7] = htobe64(H);
}
