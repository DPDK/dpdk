/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2019 Vladimir Medvedkin <medvedkinv@gmail.com>
 */

#ifndef _RTE_THASH_H
#define _RTE_THASH_H

/**
 * @file
 *
 * toeplitz hash functions.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Software implementation of the Toeplitz hash function used by RSS.
 * Can be used either for packet distribution on single queue NIC
 * or for simulating of RSS computation on specific NIC (for example
 * after GRE header decapsulating)
 */

#include <stdint.h>
#include <rte_byteorder.h>
#include <rte_config.h>
#include <rte_ip.h>
#include <rte_common.h>

#if defined(RTE_ARCH_X86) || defined(RTE_MACHINE_CPUFLAG_NEON)
#include <rte_vect.h>
#endif

#ifdef RTE_ARCH_X86
/* Byte swap mask used for converting IPv6 address
 * 4-byte chunks to CPU byte order
 */
static const __m128i rte_thash_ipv6_bswap_mask = {
		0x0405060700010203ULL, 0x0C0D0E0F08090A0BULL};
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
	RTE_STD_C11
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
	uint8_t		src_addr[16];
	uint8_t		dst_addr[16];
	RTE_STD_C11
	union {
		struct {
			uint16_t dport;
			uint16_t sport;
		};
		uint32_t        sctp_tag;
	};
};

union rte_thash_tuple {
	struct rte_ipv4_tuple	v4;
	struct rte_ipv6_tuple	v6;
#ifdef RTE_ARCH_X86
} __rte_aligned(XMM_SIZE);
#else
};
#endif

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
	__m128i ipv6 = _mm_loadu_si128((const __m128i *)orig->src_addr);
	*(__m128i *)targ->v6.src_addr =
			_mm_shuffle_epi8(ipv6, rte_thash_ipv6_bswap_mask);
	ipv6 = _mm_loadu_si128((const __m128i *)orig->dst_addr);
	*(__m128i *)targ->v6.dst_addr =
			_mm_shuffle_epi8(ipv6, rte_thash_ipv6_bswap_mask);
#elif defined(RTE_MACHINE_CPUFLAG_NEON)
	uint8x16_t ipv6 = vld1q_u8((uint8_t const *)orig->src_addr);
	vst1q_u8((uint8_t *)targ->v6.src_addr, vrev32q_u8(ipv6));
	ipv6 = vld1q_u8((uint8_t const *)orig->dst_addr);
	vst1q_u8((uint8_t *)targ->v6.dst_addr, vrev32q_u8(ipv6));
#else
	int i;
	for (i = 0; i < 4; i++) {
		*((uint32_t *)targ->v6.src_addr + i) =
			rte_be_to_cpu_32(*((const uint32_t *)orig->src_addr + i));
		*((uint32_t *)targ->v6.dst_addr + i) =
			rte_be_to_cpu_32(*((const uint32_t *)orig->dst_addr + i));
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

#ifdef __cplusplus
}
#endif

#endif /* _RTE_THASH_H */
