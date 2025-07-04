/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#include <inttypes.h>
#include <rte_ether.h>

#include "bnxt_util.h"

int bnxt_check_zero_bytes(const uint8_t *bytes, int len)
{
	int i;

	for (i = 0; i < len; i++)
		if (bytes[i] != 0x00)
			return 0;
	return 1;
}

void bnxt_eth_hw_addr_random(uint8_t *mac_addr)
{
	rte_eth_random_addr(mac_addr);

	/* Set Organizationally Unique Identifier (OUI) prefix */
	mac_addr[0] = 0x00;
	mac_addr[1] = 0x0a;
	mac_addr[2] = 0xf7;
}

uint8_t hweight32(uint32_t word32)
{
	uint32_t res = word32 - ((word32 >> 1) & 0x55555555);

	res = (res & 0x33333333) + ((res >> 2) & 0x33333333);
	res = (res + (res >> 4)) & 0x0F0F0F0F;
	res = res + (res >> 8);
	return (res + (res >> 16)) & 0x000000FF;
}
