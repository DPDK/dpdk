/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2020 Intel Corporation
 */

#include <stddef.h>
#include <stdint.h>

#include <eal_export.h>
#include <rte_cpuflags.h>
#include <rte_common.h>
#include <rte_net_crc.h>
#include <rte_vect.h>
#include <rte_malloc.h>

#include "net_crc.h"

/** CRC polynomials */
#define CRC32_ETH_POLYNOMIAL 0x04c11db7UL
#define CRC16_CCITT_POLYNOMIAL 0x1021U

#define CRC_LUT_SIZE 256

/* crc tables */
static uint32_t crc32_eth_lut[CRC_LUT_SIZE];
static uint32_t crc16_ccitt_lut[CRC_LUT_SIZE];

static uint32_t
rte_crc16_ccitt_handler(const uint8_t *data, uint32_t data_len);

static uint32_t
rte_crc32_eth_handler(const uint8_t *data, uint32_t data_len);

typedef uint32_t
(*rte_net_crc_handler)(const uint8_t *data, uint32_t data_len);

struct rte_net_crc {
	enum rte_net_crc_alg alg;
	enum rte_net_crc_type type;
};

static struct {
	rte_net_crc_handler f[RTE_NET_CRC_REQS];
} handlers[RTE_NET_CRC_AVX512 + 1];

/* Scalar handling */

/**
 * Reflect the bits about the middle
 *
 * @param val
 *   value to be reflected
 *
 * @return
 *   reflected value
 */
static uint32_t
reflect_32bits(uint32_t val)
{
	uint32_t i, res = 0;

	for (i = 0; i < 32; i++)
		if ((val & (1U << i)) != 0)
			res |= (uint32_t)(1U << (31 - i));

	return res;
}

static void
crc32_eth_init_lut(uint32_t poly,
	uint32_t *lut)
{
	uint32_t i, j;

	for (i = 0; i < CRC_LUT_SIZE; i++) {
		uint32_t crc = reflect_32bits(i);

		for (j = 0; j < 8; j++) {
			if (crc & 0x80000000L)
				crc = (crc << 1) ^ poly;
			else
				crc <<= 1;
		}
		lut[i] = reflect_32bits(crc);
	}
}

static __rte_always_inline uint32_t
crc32_eth_calc_lut(const uint8_t *data,
	uint32_t data_len,
	uint32_t crc,
	const uint32_t *lut)
{
	while (data_len--)
		crc = lut[(crc ^ *data++) & 0xffL] ^ (crc >> 8);

	return crc;
}

static void
rte_net_crc_scalar_init(void)
{
	/* 32-bit crc init */
	crc32_eth_init_lut(CRC32_ETH_POLYNOMIAL, crc32_eth_lut);

	/* 16-bit CRC init */
	crc32_eth_init_lut(CRC16_CCITT_POLYNOMIAL << 16, crc16_ccitt_lut);
}

static inline uint32_t
rte_crc16_ccitt_handler(const uint8_t *data, uint32_t data_len)
{
	/* return 16-bit CRC value */
	return (uint16_t)~crc32_eth_calc_lut(data,
		data_len,
		0xffff,
		crc16_ccitt_lut);
}

static inline uint32_t
rte_crc32_eth_handler(const uint8_t *data, uint32_t data_len)
{
	/* return 32-bit CRC value */
	return ~crc32_eth_calc_lut(data,
		data_len,
		0xffffffffUL,
		crc32_eth_lut);
}

/* AVX512/VPCLMULQDQ handling */

#define AVX512_VPCLMULQDQ_CPU_SUPPORTED ( \
	rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512F) && \
	rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512BW) && \
	rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512DQ) && \
	rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512VL) && \
	rte_cpu_get_flag_enabled(RTE_CPUFLAG_PCLMULQDQ) && \
	rte_cpu_get_flag_enabled(RTE_CPUFLAG_VPCLMULQDQ) \
)

static void
avx512_vpclmulqdq_init(void)
{
#ifdef CC_AVX512_SUPPORT
	if (AVX512_VPCLMULQDQ_CPU_SUPPORTED)
		rte_net_crc_avx512_init();
#endif
}

/* SSE4.2/PCLMULQDQ handling */

#define SSE42_PCLMULQDQ_CPU_SUPPORTED \
	rte_cpu_get_flag_enabled(RTE_CPUFLAG_PCLMULQDQ)

static void
sse42_pclmulqdq_init(void)
{
#ifdef RTE_ARCH_X86_64
	if (SSE42_PCLMULQDQ_CPU_SUPPORTED)
		rte_net_crc_sse42_init();
#endif
}

/* NEON/PMULL handling */

#define NEON_PMULL_CPU_SUPPORTED \
	rte_cpu_get_flag_enabled(RTE_CPUFLAG_PMULL)

static void
neon_pmull_init(void)
{
#ifdef CC_ARM64_NEON_PMULL_SUPPORT
	if (NEON_PMULL_CPU_SUPPORTED)
		rte_net_crc_neon_init();
#endif
}

static void
handlers_init(enum rte_net_crc_alg alg)
{
	handlers[alg].f[RTE_NET_CRC16_CCITT] = rte_crc16_ccitt_handler;
	handlers[alg].f[RTE_NET_CRC32_ETH] = rte_crc32_eth_handler;

	switch (alg) {
	case RTE_NET_CRC_AVX512:
#ifdef CC_AVX512_SUPPORT
		if (AVX512_VPCLMULQDQ_CPU_SUPPORTED) {
			handlers[alg].f[RTE_NET_CRC16_CCITT] = rte_crc16_ccitt_avx512_handler;
			handlers[alg].f[RTE_NET_CRC32_ETH] = rte_crc32_eth_avx512_handler;
			break;
		}
#endif
		/* fall-through */
	case RTE_NET_CRC_SSE42:
#ifdef RTE_ARCH_X86_64
		if (SSE42_PCLMULQDQ_CPU_SUPPORTED) {
			handlers[alg].f[RTE_NET_CRC16_CCITT] = rte_crc16_ccitt_sse42_handler;
			handlers[alg].f[RTE_NET_CRC32_ETH] = rte_crc32_eth_sse42_handler;
		}
#endif
		break;
	case RTE_NET_CRC_NEON:
#ifdef CC_ARM64_NEON_PMULL_SUPPORT
		if (NEON_PMULL_CPU_SUPPORTED) {
			handlers[alg].f[RTE_NET_CRC16_CCITT] = rte_crc16_ccitt_neon_handler;
			handlers[alg].f[RTE_NET_CRC32_ETH] = rte_crc32_eth_neon_handler;
			break;
		}
#endif
		/* fall-through */
	case RTE_NET_CRC_SCALAR:
		/* fall-through */
	default:
		break;
	}
}

/* Public API */

RTE_EXPORT_SYMBOL(rte_net_crc_set_alg)
struct rte_net_crc *rte_net_crc_set_alg(enum rte_net_crc_alg alg, enum rte_net_crc_type type)
{
	uint16_t max_simd_bitwidth;
	struct rte_net_crc *crc;

	crc = rte_zmalloc(NULL, sizeof(struct rte_net_crc), 0);
	if (crc == NULL)
		return NULL;
	max_simd_bitwidth = rte_vect_get_max_simd_bitwidth();
	crc->type = type;
	crc->alg = RTE_NET_CRC_SCALAR;

	switch (alg) {
	case RTE_NET_CRC_AVX512:
		if (max_simd_bitwidth >= RTE_VECT_SIMD_512) {
			crc->alg = RTE_NET_CRC_AVX512;
			return crc;
		}
		/* fall-through */
	case RTE_NET_CRC_SSE42:
		if (max_simd_bitwidth >= RTE_VECT_SIMD_128) {
			crc->alg = RTE_NET_CRC_SSE42;
			return crc;
		}
		break;
	case RTE_NET_CRC_NEON:
		if (max_simd_bitwidth >= RTE_VECT_SIMD_128) {
			crc->alg = RTE_NET_CRC_NEON;
			return crc;
		}
		break;
	case RTE_NET_CRC_SCALAR:
		/* fall-through */
	default:
		break;
	}
	return crc;
}

RTE_EXPORT_SYMBOL(rte_net_crc_free)
void rte_net_crc_free(struct rte_net_crc *crc)
{
	rte_free(crc);
}

RTE_EXPORT_SYMBOL(rte_net_crc_calc)
uint32_t rte_net_crc_calc(const struct rte_net_crc *ctx, const void *data, const uint32_t data_len)
{
	return handlers[ctx->alg].f[ctx->type](data, data_len);
}

/* Call initialisation helpers for all crc algorithm handlers */
RTE_INIT(rte_net_crc_init)
{
	rte_net_crc_scalar_init();
	sse42_pclmulqdq_init();
	avx512_vpclmulqdq_init();
	neon_pmull_init();
	handlers_init(RTE_NET_CRC_SCALAR);
	handlers_init(RTE_NET_CRC_NEON);
	handlers_init(RTE_NET_CRC_SSE42);
	handlers_init(RTE_NET_CRC_AVX512);
}
