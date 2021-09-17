/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2021 Intel Corporation
 */

#ifndef _ICE_TMATCH_H_
#define _ICE_TMATCH_H_

static inline
bool ice_ternary_match_byte(u8 key, u8 key_inv, u8 pat)
{
	u8 k1, k2, v;
	int i;

	for (i = 0; i < 8; i++) {
		k1 = (u8)(key & (1 << i));
		k2 = (u8)(key_inv & (1 << i));
		v = (u8)(pat & (1 << i));

		if (k1 != 0 && k2 != 0)
			continue;
		if (k1 == 0 && k2 == 0)
			return false;

		if (k1 == v)
			return false;
	}

	return true;
}

static inline
bool ice_ternary_match(const u8 *key, const u8 *key_inv,
		       const u8 *pat, int len)
{
	int i;

	for (i = 0; i < len; i++)
		if (!ice_ternary_match_byte(key[i], key_inv[i], pat[i]))
			return false;

	return true;
}

#endif /* _ICE_TMATCH_H_ */
