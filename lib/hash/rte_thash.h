/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2019 Vladimir Medvedkin <medvedkinv@gmail.com>
 * Copyright(c) 2021 Intel Corporation
 */

#ifndef _RTE_THASH_H
#define _RTE_THASH_H

/**
 * @file
 *
 * Software implementation of the Toeplitz hash function used by RSS.
 * Can be used either for packet distribution on single queue NIC
 * or for simulating of RSS computation on specific NIC (for example
 * after GRE header decapsulating)
 */

#include <stdint.h>

#include <rte_byteorder.h>
#include <rte_ip.h>
#include <rte_common.h>
#include <rte_thash_gfni.h>

#if defined(RTE_ARCH_X86) || defined(__ARM_NEON)
#include <rte_vect.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * length in dwords of input tuple to
 * calculate hash of ipv4 header only
 */
#define RTE_THASH_V4_L3_LEN	((sizeof(struct rte_ipv4_tuple) -	\
			sizeof(((struct rte_ipv4_tuple *)0)->sctp_tag)) / 4)

/**
 * length in dwords of input tuple to
 * calculate hash of ipv4 header +
 * transport header
 */
#define RTE_THASH_V4_L4_LEN	 ((sizeof(struct rte_ipv4_tuple)) / 4)

/**
 * length in dwords of input tuple to
 * calculate hash of ipv6 header only
 */
#define RTE_THASH_V6_L3_LEN	((sizeof(struct rte_ipv6_tuple) -       \
			sizeof(((struct rte_ipv6_tuple *)0)->sctp_tag)) / 4)

/**
 * length in dwords of input tuple to
 * calculate hash of ipv6 header +
 * transport header
 */
#define RTE_THASH_V6_L4_LEN	((sizeof(struct rte_ipv6_tuple)) / 4)

/**
 * IPv4 tuple
 * addresses and ports/sctp_tag have to be CPU byte order
 */
struct rte_ipv4_tuple {
	uint32_t	src_addr;
	uint32_t	dst_addr;
	union {
		struct {
			uint16_t dport;
			uint16_t sport;
		};
		uint32_t        sctp_tag;
	};
};

/**
 * IPv6 tuple
 * Addresses have to be filled by rte_thash_load_v6_addr()
 * ports/sctp_tag have to be CPU byte order
 */
struct rte_ipv6_tuple {
	struct rte_ipv6_addr src_addr;
	struct rte_ipv6_addr dst_addr;
	union {
		struct {
			uint16_t dport;
			uint16_t sport;
		};
		uint32_t        sctp_tag;
	};
};

#ifdef RTE_ARCH_X86
union __rte_aligned(XMM_SIZE) rte_thash_tuple {
#else
union rte_thash_tuple {
#endif
	struct rte_ipv4_tuple	v4;
	struct rte_ipv6_tuple	v6;
};

/** @internal
 *  @brief Generates a random polynomial
 *
 * @param poly_degree
 *   degree of the polynomial
 *
 * @return
 *   random polynomial
 */
__rte_internal
uint32_t
thash_get_rand_poly(uint32_t poly_degree);

/**
 * Longest RSS hash key currently supported
 */
#define RTE_THASH_KEY_LEN_MAX	52

#define RTE_THASH_TUPLE_LEN_MAX (RTE_THASH_KEY_LEN_MAX - sizeof(uint32_t))

/**
 * Prepare special converted key to use with rte_softrss_be()
 * @param orig
 *   pointer to original RSS key
 * @param targ
 *   pointer to target RSS key
 * @param len
 *   RSS key length
 */
static inline void
rte_convert_rss_key(const uint32_t *orig, uint32_t *targ, int len)
{
	int i;

	for (i = 0; i < (len >> 2); i++)
		targ[i] = rte_be_to_cpu_32(orig[i]);
}

/**
 * Prepare and load IPv6 addresses (src and dst)
 * into target tuple
 * @param orig
 *   Pointer to ipv6 header of the original packet
 * @param targ
 *   Pointer to rte_ipv6_tuple structure
 */
static inline void
rte_thash_load_v6_addrs(const struct rte_ipv6_hdr *orig,
			union rte_thash_tuple *targ)
{
#ifdef RTE_ARCH_X86
	/* Byte swap mask used for converting IPv6 address
	 * 4-byte chunks to CPU byte order
	 */
	const __m128i rte_thash_ipv6_bswap_mask = _mm_set_epi64x(
		0x0C0D0E0F08090A0BULL, 0x0405060700010203ULL);
	__m128i ipv6 = _mm_loadu_si128((const __m128i *)&orig->src_addr);
	*(__m128i *)&targ->v6.src_addr =
			_mm_shuffle_epi8(ipv6, rte_thash_ipv6_bswap_mask);
	ipv6 = _mm_loadu_si128((const __m128i *)&orig->dst_addr);
	*(__m128i *)&targ->v6.dst_addr =
			_mm_shuffle_epi8(ipv6, rte_thash_ipv6_bswap_mask);
#elif defined(__ARM_NEON)
	uint8x16_t ipv6 = vld1q_u8(orig->src_addr.a);
	vst1q_u8(targ->v6.src_addr.a, vrev32q_u8(ipv6));
	ipv6 = vld1q_u8(orig->dst_addr.a);
	vst1q_u8(targ->v6.dst_addr.a, vrev32q_u8(ipv6));
#else
	int i;
	for (i = 0; i < 4; i++) {
		*((uint32_t *)&targ->v6.src_addr + i) =
			rte_be_to_cpu_32(*((const uint32_t *)&orig->src_addr + i));
		*((uint32_t *)&targ->v6.dst_addr + i) =
			rte_be_to_cpu_32(*((const uint32_t *)&orig->dst_addr + i));
	}
#endif
}

/**
 * Generic implementation. Can be used with original rss_key
 * @param input_tuple
 *   Pointer to input tuple
 * @param input_len
 *   Length of input_tuple in 4-bytes chunks
 * @param rss_key
 *   Pointer to RSS hash key.
 * @return
 *   Calculated hash value.
 */
static inline uint32_t
rte_softrss(uint32_t *input_tuple, uint32_t input_len,
		const uint8_t *rss_key)
{
	uint32_t i, j, map, ret = 0;

	for (j = 0; j < input_len; j++) {
		for (map = input_tuple[j]; map;	map &= (map - 1)) {
			i = rte_bsf32(map);
			ret ^= rte_cpu_to_be_32(((const uint32_t *)rss_key)[j]) << (31 - i) |
					(uint32_t)((uint64_t)(rte_cpu_to_be_32(((const uint32_t *)rss_key)[j + 1])) >>
					(i + 1));
		}
	}
	return ret;
}

/**
 * Optimized implementation.
 * If you want the calculated hash value matches NIC RSS value
 * you have to use special converted key with rte_convert_rss_key() fn.
 * @param input_tuple
 *   Pointer to input tuple
 * @param input_len
 *   Length of input_tuple in 4-bytes chunks
 * @param *rss_key
 *   Pointer to RSS hash key.
 * @return
 *   Calculated hash value.
 */
static inline uint32_t
rte_softrss_be(uint32_t *input_tuple, uint32_t input_len,
		const uint8_t *rss_key)
{
	uint32_t i, j, map, ret = 0;

	for (j = 0; j < input_len; j++) {
		for (map = input_tuple[j]; map;	map &= (map - 1)) {
			i = rte_bsf32(map);
			ret ^= ((const uint32_t *)rss_key)[j] << (31 - i) |
				(uint32_t)((uint64_t)(((const uint32_t *)rss_key)[j + 1]) >> (i + 1));
		}
	}
	return ret;
}

/**
 * Indicates if GFNI implementations of the Toeplitz hash are supported.
 *
 * @return
 *  1 if GFNI is supported
 *  0 otherwise
 */
int
rte_thash_gfni_supported(void);

/**
 * Converts Toeplitz hash key (RSS key) into matrixes required
 * for GFNI implementation
 *
 * @param matrixes
 *  pointer to the memory where matrices will be written.
 *  Note: the size of this memory must be equal to size * 8
 * @param rss_key
 *  pointer to the Toeplitz hash key
 * @param size
 *  Size of the rss_key in bytes.
 */
void
rte_thash_complete_matrix(uint64_t *matrixes, const uint8_t *rss_key,
	int size);

/** @internal Logarithm of minimum size of the RSS ReTa */
#define	RTE_THASH_RETA_SZ_MIN	2U
/** @internal Logarithm of maximum size of the RSS ReTa */
#define	RTE_THASH_RETA_SZ_MAX	16U

/**
 * LFSR will ignore if generated m-sequence has more than 2^n -1 bits,
 * where n is the logarithm of the RSS ReTa size.
 */
#define RTE_THASH_IGNORE_PERIOD_OVERFLOW	0x1
/**
 * Generate minimal required bit (equal to ReTa LSB) sequence into
 * the hash_key
 */
#define RTE_THASH_MINIMAL_SEQ			0x2

/** @internal thash context structure. */
struct rte_thash_ctx;
/** @internal thash helper structure. */
struct rte_thash_subtuple_helper;

/**
 * Create a new thash context.
 *
 * @param name
 *  Context name
 * @param key_len
 *  Length of the toeplitz hash key
 * @param reta_sz
 *  Logarithm of the NIC's Redirection Table (ReTa) size,
 *  i.e. number of the LSBs if the hash used to determine
 *  the reta entry.
 * @param key
 *  Pointer to the key used to init an internal key state.
 *  Could be NULL, in this case internal key will be inited with random.
 * @param flags
 *  Supported flags are:
 *   RTE_THASH_IGNORE_PERIOD_OVERFLOW
 *   RTE_THASH_MINIMAL_SEQ
 * @return
 *  A pointer to the created context on success
 *  NULL otherwise
 */
struct rte_thash_ctx *
rte_thash_init_ctx(const char *name, uint32_t key_len, uint32_t reta_sz,
	uint8_t *key, uint32_t flags);

/**
 * Find an existing thash context and return a pointer to it.
 *
 * @param name
 *  Name of the thash context
 * @return
 *  Pointer to the thash context or NULL if it was not found with rte_errno
 *  set appropriately. Possible rte_errno values include:
 *   - ENOENT - required entry not available to return.
 */
struct rte_thash_ctx *
rte_thash_find_existing(const char *name);

/**
 * Free a thash context object
 *
 * @param ctx
 *  Thash context
 */
void
rte_thash_free_ctx(struct rte_thash_ctx *ctx);

/**
 * Add a special properties to the toeplitz hash key inside a thash context.
 * Creates an internal helper struct which has a complementary table
 * to calculate toeplitz hash collisions.
 * This function is not multi-thread safe.
 *
 * @param ctx
 *  Thash context
 * @param name
 *  Name of the helper
 * @param len
 *  Length in bits of the target subtuple
 *  Must be no shorter than reta_sz passed on rte_thash_init_ctx().
 * @param offset
 *  Offset in bits of the subtuple
 * @return
 *  0 on success
 *  negative on error
 */
int
rte_thash_add_helper(struct rte_thash_ctx *ctx, const char *name, uint32_t len,
	uint32_t offset);

/**
 * Find a helper in the context by the given name
 *
 * @param ctx
 *  Thash context
 * @param name
 *  Name of the helper
 * @return
 *  Pointer to the thash helper or NULL if it was not found.
 */
struct rte_thash_subtuple_helper *
rte_thash_get_helper(struct rte_thash_ctx *ctx, const char *name);

/**
 * Get a complementary value for the subtuple to produce a
 * partial toeplitz hash collision. It must be XOR'ed with the
 * subtuple to produce the hash value with the desired hash LSB's
 * This function is multi-thread safe.
 *
 * @param h
 *  Pointer to the helper struct
 * @param hash
 *  Toeplitz hash value calculated for the given tuple
 * @param desired_hash
 *  Desired hash value to find a collision for
 * @return
 *  A complementary value which must be xored with the corresponding subtuple
 */
uint32_t
rte_thash_get_complement(struct rte_thash_subtuple_helper *h,
	uint32_t hash, uint32_t desired_hash);

/**
 * Get a pointer to the toeplitz hash contained in the context.
 * It changes after each addition of a helper. It should be installed to
 * the NIC.
 *
 * @param ctx
 *  Thash context
 * @return
 *  A pointer to the toeplitz hash key
 */
const uint8_t *
rte_thash_get_key(struct rte_thash_ctx *ctx);

/**
 * Get a pointer to the toeplitz hash matrices contained in the context.
 * These matrices could be used with fast toeplitz hash implementation if
 * CPU supports GFNI.
 * Matrices changes after each addition of a helper.
 *
 * @param ctx
 *  Thash context
 * @return
 *  A pointer to the toeplitz hash key matrices on success
 *  NULL if GFNI is not supported.
 */
const uint64_t *
rte_thash_get_gfni_matrices(struct rte_thash_ctx *ctx);

/**
 * Function prototype for the rte_thash_adjust_tuple
 * to check if adjusted tuple could be used.
 * Generally it is some kind of lookup function to check
 * if adjusted tuple is already in use.
 *
 * @param userdata
 *  Pointer to the userdata. It could be a pointer to the
 *  table with used tuples to search.
 * @param tuple
 *  Pointer to the tuple to check
 *
 * @return
 *  1 on success
 *  0 otherwise
 */
typedef int (*rte_thash_check_tuple_t)(void *userdata, uint8_t *tuple);

/**
 * Adjusts tuple in the way to make Toeplitz hash has
 * desired least significant bits.
 * This function is multi-thread safe.
 *
 * @param ctx
 *  Thash context
 * @param h
 *  Pointer to the helper struct
 * @param tuple
 *  Pointer to the tuple to be adjusted
 * @param tuple_len
 *  Length of the tuple. Must be multiple of 4.
 * @param desired_value
 *  Desired value of least significant bits of the hash
 * @param attempts
 *  Number of attempts to adjust tuple with fn() calling
 * @param fn
 *  Callback function to check adjusted tuple. Could be NULL
 * @param userdata
 *  Pointer to the userdata to be passed to fn(). Could be NULL
 *
 * @return
 *  0 on success
 *  negative otherwise
 */
int
rte_thash_adjust_tuple(struct rte_thash_ctx *ctx,
	struct rte_thash_subtuple_helper *h,
	uint8_t *tuple, unsigned int tuple_len,
	uint32_t desired_value, unsigned int attempts,
	rte_thash_check_tuple_t fn, void *userdata);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Modify RSS hash key such that subtuple bits corresponding to `entropy_sz`
 * bits starting from `entropy_start` will have the most even distribution with
 * this key with a given ReTa size.
 *
 * @param key
 *  Pointer to the RSS hash key.
 * @param key_len
 *  Length of the key.
 * @param reta_sz_log
 *  Log2 of the size of RSS redirection table,
 *  i.e. number of bits of the RSS hash value used to identify RSS ReTa entry.
 * @param entropy_start
 *  Bit offset from the beginning of the tuple
 *  where user expects best distribution of the subtuple values.
 * @param entropy_sz
 *  Size in bits of the part of subtuple.
 *
 * @return
 *  0 on success negative otherwise
 */
__rte_experimental
int
rte_thash_gen_key(uint8_t *key, size_t key_len, size_t reta_sz_log,
	uint32_t entropy_start, size_t entropy_sz);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_THASH_H */
