/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Advanced Micro Devices, Inc.
 */

#include "sfc_tbls.h"
#include "sfc_debug.h"

#include <rte_ip.h>

/* Number of bits in uint32_t type */
#define SFC_TBLS_U32_BITS (sizeof(uint32_t) * CHAR_BIT)

static uint32_t
sfc_tbls_field_update(uint32_t in, uint16_t lbn, uint16_t width, uint32_t value)
{
	uint32_t mask;

	SFC_ASSERT(width <= SFC_TBLS_U32_BITS);

	if (width == SFC_TBLS_U32_BITS)
		return value;

	mask = RTE_LEN2MASK(width, uint32_t);
	value &= mask;

	if (lbn != 0) {
		mask <<= lbn;
		value <<= lbn;
	}

	return (in & (~mask)) | value;
}

void
sfc_tbls_field_set_u32(uint32_t data[], __rte_unused unsigned int data_size,
		       uint16_t lbn, uint16_t width, uint32_t value)
{
	uint32_t data_offset = 0;

	if (lbn >= SFC_TBLS_U32_BITS) {
		data_offset = lbn / SFC_TBLS_U32_BITS;

		SFC_ASSERT(data_offset < data_size);

		data += data_offset;
		lbn %= SFC_TBLS_U32_BITS;
	}

	if (lbn + width <= SFC_TBLS_U32_BITS) {
		*data = sfc_tbls_field_update(*data, lbn, width, value);
	} else {
		*data = sfc_tbls_field_update(*data, lbn,
					      SFC_TBLS_U32_BITS - lbn, value);
		value >>= SFC_TBLS_U32_BITS - lbn;

		data_offset++;
		SFC_ASSERT(data_offset < data_size);

		data++;
		*data = sfc_tbls_field_update(*data, 0,
					      width + lbn - SFC_TBLS_U32_BITS,
					      value);
	}
}

void
sfc_tbls_field_set_u16(uint32_t data[], unsigned int data_size, uint16_t lbn,
		       uint16_t width, uint16_t value)
{
	sfc_tbls_field_set_u32(data, data_size, lbn, width, value);
}

void
sfc_tbls_field_set_u8(uint32_t data[], unsigned int data_size, uint16_t lbn,
		      uint16_t width, uint8_t value)
{
	sfc_tbls_field_set_u32(data, data_size, lbn, width, value);
}

void
sfc_tbls_field_set_ip(uint32_t data[], unsigned int data_size, uint16_t lbn,
		      __rte_unused uint16_t width, const uint32_t *ip)
{
	unsigned int i;
	size_t ipv6_addr_len = RTE_SIZEOF_FIELD(struct rte_ipv6_hdr, src_addr);

	/*
	 * The same 128-bit container is used to store either
	 * an IPv4 or an IPv6 address, with an IPv4 address
	 * assumed to have 12 trailing zeroes.
	 */
	SFC_ASSERT(width == ipv6_addr_len * CHAR_BIT);

	for (i = 0; i < ipv6_addr_len / sizeof(*ip); i++) {
		sfc_tbls_field_set_u32(data, data_size, lbn,
				       SFC_TBLS_U32_BITS, ip[i]);
		lbn += SFC_TBLS_U32_BITS;
	}
}

void
sfc_tbls_field_set_u64(uint32_t data[], __rte_unused unsigned int data_size,
		       uint16_t lbn, uint16_t width, uint64_t value)
{
	uint32_t data_offset = 0;

	if (lbn >= SFC_TBLS_U32_BITS) {
		data_offset = lbn / SFC_TBLS_U32_BITS;

		SFC_ASSERT(data_offset < data_size);

		data += data_offset;
		lbn %= SFC_TBLS_U32_BITS;
	}

	*data = sfc_tbls_field_update(*data, lbn, SFC_TBLS_U32_BITS - lbn, value);
	value >>= SFC_TBLS_U32_BITS - lbn;
	width -= SFC_TBLS_U32_BITS - lbn;

	data_offset++;
	SFC_ASSERT(data_offset < data_size);

	data++;

	if (width > SFC_TBLS_U32_BITS) {
		*data = sfc_tbls_field_update(*data, 0, SFC_TBLS_U32_BITS, value);
		value >>= SFC_TBLS_U32_BITS;
		width -= SFC_TBLS_U32_BITS;

		data_offset++;
		SFC_ASSERT(data_offset < data_size);

		data++;
	}

	*data = sfc_tbls_field_update(*data, 0, width, value);
}

void
sfc_tbls_field_set_bit(uint32_t data[], unsigned int data_size, uint16_t lbn,
		       uint16_t width, bool value)
{
	SFC_ASSERT(width == 1);

	sfc_tbls_field_set_u32(data, data_size, lbn, width, value ? 1 : 0);
}
