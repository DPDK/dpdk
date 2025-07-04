/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 NVIDIA Corporation & Affiliates
 */

#ifndef MLX5DR_CRC32_C_
#define MLX5DR_CRC32_C_

/* Ethernet AUTODIN II CRC32 (little-endian)
 * CRC32_POLY 0xedb88320
 */
uint32_t mlx5dr_crc32_calc(uint8_t *p, size_t len);

/* Standard CRC16 calculation using the crc_tab16 param to indicate
 * the pre-calculated polynome hash values.
 */
uint16_t mlx5dr_crc16_calc(uint8_t *p, size_t len, uint16_t crc_tab16[]);

#endif /* MLX5DR_CRC32_C_ */
