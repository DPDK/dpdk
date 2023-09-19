/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <inttypes.h>

#include "nfp_crc.h"

static inline uint32_t
nfp_crc32_be_generic(uint32_t crc,
		unsigned char const *p,
		size_t len,
		uint32_t polynomial)
{
	uint32_t i;

	while (len--) {
		crc ^= *p++ << 24;
		for (i = 0; i < 8; i++)
			crc = (crc << 1) ^ ((crc & 0x80000000) ? polynomial : 0);
	}

	return crc;
}

static inline uint32_t
nfp_crc32_be(uint32_t crc,
		unsigned char const *p,
		size_t len)
{
	return nfp_crc32_be_generic(crc, p, len, CRCPOLY_BE);
}

static uint32_t
nfp_crc32_posix_end(uint32_t crc,
		size_t total_len)
{
	/* Extend with the length of the string. */
	while (total_len != 0) {
		uint8_t c = total_len & 0xff;

		crc = nfp_crc32_be(crc, &c, 1);
		total_len >>= 8;
	}

	return ~crc;
}

uint32_t
nfp_crc32_posix(const void *buff,
		size_t len)
{
	return nfp_crc32_posix_end(nfp_crc32_be(0, buff, len), len);
}
