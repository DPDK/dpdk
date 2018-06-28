/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#include <inttypes.h>

#include "bnxt_util.h"

int bnxt_check_zero_bytes(const uint8_t *bytes, int len)
{
	int i;

	for (i = 0; i < len; i++)
		if (bytes[i] != 0x00)
			return 0;
	return 1;
}
