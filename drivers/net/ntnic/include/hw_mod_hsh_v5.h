/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _HW_MOD_HSH_V5_H_
#define _HW_MOD_HSH_V5_H_

#include <stdint.h>

#define HSH_RCP_MAC_PORT_MASK_SIZE 4
#define HSH_RCP_WORD_MASK_SIZE 10
/* Toeplitz hash key size in 32-bit words, e.g. 10 words means 320 bits, i.e. 40 Bytes */
#define HSH_RCP_KEY_SIZE 10

struct hsh_v5_rcp_s {
	uint32_t load_dist_type;
	uint32_t mac_port_mask[HSH_RCP_MAC_PORT_MASK_SIZE];
	uint32_t sort;
	uint32_t qw0_pe;
	int32_t qw0_ofs;
	uint32_t qw4_pe;
	int32_t qw4_ofs;
	uint32_t w8_pe;
	int32_t w8_ofs;
	uint32_t w8_sort;
	uint32_t w9_pe;
	int32_t w9_ofs;
	uint32_t w9_sort;
	uint32_t w9_p;
	uint32_t p_mask;
	uint32_t word_mask[HSH_RCP_WORD_MASK_SIZE];
	uint32_t seed;
	uint32_t tnl_p;
	uint32_t hsh_valid;
	uint32_t hsh_type;
	uint32_t toeplitz;	/* Toeplitz enabled / disabled */
	uint32_t k[HSH_RCP_KEY_SIZE];	/* Toeplitz hash key */
	uint32_t auto_ipv4_mask;
};

struct hw_mod_hsh_v5_s {
	struct hsh_v5_rcp_s *rcp;
};

#endif	/* _HW_MOD_HSH_V5_H_ */
