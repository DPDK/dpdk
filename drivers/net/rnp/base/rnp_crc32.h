/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef _RNP_CRC32_H_
#define _RNP_CRC32_H_

u32 rnp_vid_crc32_calc(u32 crc_init, u16 vid_le);
u32 rnp_calc_crc32(u32 seed, u8 *mac, u32 len);

#endif /* _RNP_CRC32_H_ */
