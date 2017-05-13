/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation.
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

#include <stddef.h>
#include <string.h>
#include <stdint.h>

#include <rte_cpuflags.h>
#include <rte_common.h>
#include <rte_net_crc.h>

#if defined(RTE_ARCH_X86_64)				\
	&& defined(RTE_MACHINE_CPUFLAG_SSE4_2)		\
	&& defined(RTE_MACHINE_CPUFLAG_PCLMULQDQ)
#define X86_64_SSE42_PCLMULQDQ     1
#endif

#ifdef X86_64_SSE42_PCLMULQDQ
#include <net_crc_sse.h>
#endif

/* crc tables */
static uint32_t crc32_eth_lut[CRC_LUT_SIZE];
static uint32_t crc16_ccitt_lut[CRC_LUT_SIZE];

static uint32_t
rte_crc16_ccitt_handler(const uint8_t *data, uint32_t data_len);

static uint32_t
rte_crc32_eth_handler(const uint8_t *data, uint32_t data_len);

typedef uint32_t
(*rte_net_crc_handler)(const uint8_t *data, uint32_t data_len);

static rte_net_crc_handler *handlers;

static rte_net_crc_handler handlers_scalar[] = {
	[RTE_NET_CRC16_CCITT] = rte_crc16_ccitt_handler,
	[RTE_NET_CRC32_ETH] = rte_crc32_eth_handler,
};

#ifdef X86_64_SSE42_PCLMULQDQ
static rte_net_crc_handler handlers_sse42[] = {
	[RTE_NET_CRC16_CCITT] = rte_crc16_ccitt_sse42_handler,
	[RTE_NET_CRC32_ETH] = rte_crc32_eth_sse42_handler,
};
#endif

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
		if ((val & (1 << i)) != 0)
			res |= (uint32_t)(1 << (31 - i));

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

void
rte_net_crc_set_alg(enum rte_net_crc_alg alg)
{
	switch (alg) {
	case RTE_NET_CRC_SSE42:
#ifdef X86_64_SSE42_PCLMULQDQ
		handlers = handlers_sse42;
#else
		alg = RTE_NET_CRC_SCALAR;
#endif
		break;
	case RTE_NET_CRC_SCALAR:
	default:
		handlers = handlers_scalar;
		break;
	}
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

/* Select highest available crc algorithm as default one */
static inline void __attribute__((constructor))
rte_net_crc_init(void)
{
	enum rte_net_crc_alg alg = RTE_NET_CRC_SCALAR;

	rte_net_crc_scalar_init();

#ifdef X86_64_SSE42_PCLMULQDQ
		alg = RTE_NET_CRC_SSE42;
		rte_net_crc_sse42_init();
#endif

	rte_net_crc_set_alg(alg);
}
