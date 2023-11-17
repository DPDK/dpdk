/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2020 Intel Corporation
 */

#include <stddef.h>
#include <stdint.h>

#include <rte_cpuflags.h>
#include <rte_common.h>
#include <rte_net_crc.h>
#include <rte_log.h>
#include <rte_vect.h>

#include "net_crc.h"

/** CRC polynomials */
#define CRC32_ETH_POLYNOMIAL 0x04c11db7UL
#define CRC16_CCITT_POLYNOMIAL 0x1021U

#define CRC_LUT_SIZE 256

/* crc tables */
static uint32_t crc32_eth_lut[CRC_LUT_SIZE];
static uint32_t crc16_ccitt_lut[CRC_LUT_SIZE];

static uint32_t
rte_crc16_ccitt_default_handler(const uint8_t *data, uint32_t data_len);

static uint32_t
rte_crc32_eth_default_handler(const uint8_t *data, uint32_t data_len);

static uint32_t
rte_crc16_ccitt_handler(const uint8_t *data, uint32_t data_len);

static uint32_t
rte_crc32_eth_handler(const uint8_t *data, uint32_t data_len);

typedef uint32_t
(*rte_net_crc_handler)(const uint8_t *data, uint32_t data_len);

static rte_net_crc_handler handlers_default[] = {
	[RTE_NET_CRC16_CCITT] = rte_crc16_ccitt_default_handler,
	[RTE_NET_CRC32_ETH] = rte_crc32_eth_default_handler,
};

static const rte_net_crc_handler *handlers = handlers_default;

static const rte_net_crc_handler handlers_scalar[] = {
	[RTE_NET_CRC16_CCITT] = rte_crc16_ccitt_handler,
	[RTE_NET_CRC32_ETH] = rte_crc32_eth_handler,
};
#ifdef CC_X86_64_AVX512_VPCLMULQDQ_SUPPORT
static const rte_net_crc_handler handlers_avx512[] = {
	[RTE_NET_CRC16_CCITT] = rte_crc16_ccitt_avx512_handler,
	[RTE_NET_CRC32_ETH] = rte_crc32_eth_avx512_handler,
};
#endif
#ifdef CC_X86_64_SSE42_PCLMULQDQ_SUPPORT
static const rte_net_crc_handler handlers_sse42[] = {
	[RTE_NET_CRC16_CCITT] = rte_crc16_ccitt_sse42_handler,
	[RTE_NET_CRC32_ETH] = rte_crc32_eth_sse42_handler,
};
#endif
#ifdef CC_ARM64_NEON_PMULL_SUPPORT
static const rte_net_crc_handler handlers_neon[] = {
	[RTE_NET_CRC16_CCITT] = rte_crc16_ccitt_neon_handler,
	[RTE_NET_CRC32_ETH] = rte_crc32_eth_neon_handler,
};
#endif

static uint16_t max_simd_bitwidth;

RTE_LOG_REGISTER_DEFAULT(libnet_logtype, INFO);
#define RTE_LOGTYPE_NET libnet_logtype

#define NET_LOG(level, fmt, args...) \
	RTE_LOG_LINE(level, NET, "%s(): " fmt, __func__, ## args)

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

static const rte_net_crc_handler *
avx512_vpclmulqdq_get_handlers(void)
{
#ifdef CC_X86_64_AVX512_VPCLMULQDQ_SUPPORT
	if (AVX512_VPCLMULQDQ_CPU_SUPPORTED &&
			max_simd_bitwidth >= RTE_VECT_SIMD_512)
		return handlers_avx512;
#endif
	NET_LOG(INFO, "Requirements not met, can't use AVX512");
	return NULL;
}

static void
avx512_vpclmulqdq_init(void)
{
#ifdef CC_X86_64_AVX512_VPCLMULQDQ_SUPPORT
	if (AVX512_VPCLMULQDQ_CPU_SUPPORTED)
		rte_net_crc_avx512_init();
#endif
}

/* SSE4.2/PCLMULQDQ handling */

#define SSE42_PCLMULQDQ_CPU_SUPPORTED \
	rte_cpu_get_flag_enabled(RTE_CPUFLAG_PCLMULQDQ)

static const rte_net_crc_handler *
sse42_pclmulqdq_get_handlers(void)
{
#ifdef CC_X86_64_SSE42_PCLMULQDQ_SUPPORT
	if (SSE42_PCLMULQDQ_CPU_SUPPORTED &&
			max_simd_bitwidth >= RTE_VECT_SIMD_128)
		return handlers_sse42;
#endif
	NET_LOG(INFO, "Requirements not met, can't use SSE");
	return NULL;
}

static void
sse42_pclmulqdq_init(void)
{
#ifdef CC_X86_64_SSE42_PCLMULQDQ_SUPPORT
	if (SSE42_PCLMULQDQ_CPU_SUPPORTED)
		rte_net_crc_sse42_init();
#endif
}

/* NEON/PMULL handling */

#define NEON_PMULL_CPU_SUPPORTED \
	rte_cpu_get_flag_enabled(RTE_CPUFLAG_PMULL)

static const rte_net_crc_handler *
neon_pmull_get_handlers(void)
{
#ifdef CC_ARM64_NEON_PMULL_SUPPORT
	if (NEON_PMULL_CPU_SUPPORTED &&
			max_simd_bitwidth >= RTE_VECT_SIMD_128)
		return handlers_neon;
#endif
	NET_LOG(INFO, "Requirements not met, can't use NEON");
	return NULL;
}

static void
neon_pmull_init(void)
{
#ifdef CC_ARM64_NEON_PMULL_SUPPORT
	if (NEON_PMULL_CPU_SUPPORTED)
		rte_net_crc_neon_init();
#endif
}

/* Default handling */

static uint32_t
rte_crc16_ccitt_default_handler(const uint8_t *data, uint32_t data_len)
{
	handlers = NULL;
	if (max_simd_bitwidth == 0)
		max_simd_bitwidth = rte_vect_get_max_simd_bitwidth();

	handlers = avx512_vpclmulqdq_get_handlers();
	if (handlers != NULL)
		return handlers[RTE_NET_CRC16_CCITT](data, data_len);
	handlers = sse42_pclmulqdq_get_handlers();
	if (handlers != NULL)
		return handlers[RTE_NET_CRC16_CCITT](data, data_len);
	handlers = neon_pmull_get_handlers();
	if (handlers != NULL)
		return handlers[RTE_NET_CRC16_CCITT](data, data_len);
	handlers = handlers_scalar;
	return handlers[RTE_NET_CRC16_CCITT](data, data_len);
}

static uint32_t
rte_crc32_eth_default_handler(const uint8_t *data, uint32_t data_len)
{
	handlers = NULL;
	if (max_simd_bitwidth == 0)
		max_simd_bitwidth = rte_vect_get_max_simd_bitwidth();

	handlers = avx512_vpclmulqdq_get_handlers();
	if (handlers != NULL)
		return handlers[RTE_NET_CRC32_ETH](data, data_len);
	handlers = sse42_pclmulqdq_get_handlers();
	if (handlers != NULL)
		return handlers[RTE_NET_CRC32_ETH](data, data_len);
	handlers = neon_pmull_get_handlers();
	if (handlers != NULL)
		return handlers[RTE_NET_CRC32_ETH](data, data_len);
	handlers = handlers_scalar;
	return handlers[RTE_NET_CRC32_ETH](data, data_len);
}

/* Public API */

void
rte_net_crc_set_alg(enum rte_net_crc_alg alg)
{
	handlers = NULL;
	if (max_simd_bitwidth == 0)
		max_simd_bitwidth = rte_vect_get_max_simd_bitwidth();

	switch (alg) {
	case RTE_NET_CRC_AVX512:
		handlers = avx512_vpclmulqdq_get_handlers();
		if (handlers != NULL)
			break;
		/* fall-through */
	case RTE_NET_CRC_SSE42:
		handlers = sse42_pclmulqdq_get_handlers();
		break; /* for x86, always break here */
	case RTE_NET_CRC_NEON:
		handlers = neon_pmull_get_handlers();
		/* fall-through */
	case RTE_NET_CRC_SCALAR:
		/* fall-through */
	default:
		break;
	}

	if (handlers == NULL)
		handlers = handlers_scalar;
}

uint32_t
rte_net_crc_calc(const void *data,
	uint32_t data_len,
	enum rte_net_crc_type type)
{
	uint32_t ret;
	rte_net_crc_handler f_handle;

	f_handle = handlers[type];
	ret = f_handle(data, data_len);

	return ret;
}

/* Call initialisation helpers for all crc algorithm handlers */
RTE_INIT(rte_net_crc_init)
{
	rte_net_crc_scalar_init();
	sse42_pclmulqdq_init();
	avx512_vpclmulqdq_init();
	neon_pmull_init();
}
