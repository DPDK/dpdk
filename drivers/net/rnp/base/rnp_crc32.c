/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#include "rnp_osdep.h"
#include "rnp_crc32.h"

#define RNP_CRC32_POLY_LE 0xedb88320
static inline int get_bitmask_order(u32 count)
{
	int order;

	order = fls(count);

	return order;   /* We could be slightly more clever with -1 here... */
}

u32 rnp_vid_crc32_calc(u32 crc_init, u16 vid_le)
{
	u8 *data = (u8 *)&vid_le;
	u32 crc = crc_init;
	u8 data_byte = 0;
	u32 temp = 0;
	int i, bits;

	bits = get_bitmask_order(VLAN_VID_MASK);
	for (i = 0; i < bits; i++) {
		if ((i % 8) == 0)
			data_byte = data[i / 8];
		temp = ((crc & 1) ^ data_byte) & 1;
		crc >>= 1;
		data_byte >>= 1;
		if (temp)
			crc ^= RNP_CRC32_POLY_LE;
	}

	return crc;
}

u32 rnp_calc_crc32(u32 seed, u8 *mac, u32 len)
{
	u32 crc = seed;
	u32 i;

	while (len--) {
		crc ^= *mac++;
		for (i = 0; i < 8; i++)
			crc = (crc >> 1) ^ ((crc & 1) ?
					RNP_CRC32_POLY_LE : 0);
	}

	return crc;
}
