/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015 Intel Corporation. All rights reserved.
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

/* Functions to compare multiple of 16 byte keys (up to 128 bytes) */
static int
rte_hash_k16_cmp_eq(const void *key1, const void *key2, size_t key_len __rte_unused)
{
	const __m128i k1 = _mm_loadu_si128((const __m128i *) key1);
	const __m128i k2 = _mm_loadu_si128((const __m128i *) key2);
#ifdef RTE_MACHINE_CPUFLAG_SSE4_1
	const __m128i x = _mm_xor_si128(k1, k2);

	return !_mm_test_all_zeros(x, x);
#else
	const __m128i x = _mm_cmpeq_epi32(k1, k2);

	return _mm_movemask_epi8(x) != 0xffff;
#endif
}

static int
rte_hash_k32_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k16_cmp_eq(key1, key2, key_len) ||
		rte_hash_k16_cmp_eq((const char *) key1 + 16,
				(const char *) key2 + 16, key_len);
}

static int
rte_hash_k48_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k16_cmp_eq(key1, key2, key_len) ||
		rte_hash_k16_cmp_eq((const char *) key1 + 16,
				(const char *) key2 + 16, key_len) ||
		rte_hash_k16_cmp_eq((const char *) key1 + 32,
				(const char *) key2 + 32, key_len);
}

static int
rte_hash_k64_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k32_cmp_eq(key1, key2, key_len) ||
		rte_hash_k32_cmp_eq((const char *) key1 + 32,
				(const char *) key2 + 32, key_len);
}

static int
rte_hash_k80_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k64_cmp_eq(key1, key2, key_len) ||
		rte_hash_k16_cmp_eq((const char *) key1 + 64,
				(const char *) key2 + 64, key_len);
}

static int
rte_hash_k96_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k64_cmp_eq(key1, key2, key_len) ||
		rte_hash_k32_cmp_eq((const char *) key1 + 64,
				(const char *) key2 + 64, key_len);
}

static int
rte_hash_k112_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k64_cmp_eq(key1, key2, key_len) ||
		rte_hash_k32_cmp_eq((const char *) key1 + 64,
				(const char *) key2 + 64, key_len) ||
		rte_hash_k16_cmp_eq((const char *) key1 + 96,
				(const char *) key2 + 96, key_len);
}

static int
rte_hash_k128_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k64_cmp_eq(key1, key2, key_len) ||
		rte_hash_k64_cmp_eq((const char *) key1 + 64,
				(const char *) key2 + 64, key_len);
}
