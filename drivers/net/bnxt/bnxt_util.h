/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_UTIL_H_
#define _BNXT_UTIL_H_

#ifndef BIT
#define BIT(n)	(1UL << (n))
#endif /* BIT */
#ifndef BIT_MASK
#define BIT_MASK(len) (BIT(len) - 1)
#endif /* BIT_MASK */

int bnxt_check_zero_bytes(const uint8_t *bytes, int len);
void bnxt_eth_hw_addr_random(uint8_t *mac_addr);
uint8_t hweight32(uint32_t word32);
#endif /* _BNXT_UTIL_H_ */
