/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _HW_MOD_TPE_V3_H_
#define _HW_MOD_TPE_V3_H_

#include <stdint.h>

struct tpe_v1_rpp_v0_rcp_s {
	uint32_t exp;
};

struct tpe_v1_ins_v1_rcp_s {
	uint32_t dyn;
	uint32_t ofs;
	uint32_t len;
};

struct tpe_v3_rpl_v4_rcp_s {
	uint32_t dyn;
	uint32_t ofs;
	uint32_t len;
	uint32_t rpl_ptr;
	uint32_t ext_prio;
	uint32_t eth_type_wr;
};

struct tpe_v1_rpl_v2_ext_s {
	uint32_t rpl_ptr;
	uint32_t meta_rpl_len;	/* SW only */
};

struct tpe_v1_rpl_v2_rpl_s {
	uint32_t value[4];
};

struct tpe_v1_cpy_v1_rcp_s {
	uint32_t reader_select;
	uint32_t dyn;
	uint32_t ofs;
	uint32_t len;
};

struct tpe_v1_hfu_v1_rcp_s {
	uint32_t len_a_wr;
	uint32_t len_a_outer_l4_len;
	uint32_t len_a_pos_dyn;
	uint32_t len_a_pos_ofs;
	uint32_t len_a_add_dyn;
	uint32_t len_a_add_ofs;
	uint32_t len_a_sub_dyn;

	uint32_t len_b_wr;
	uint32_t len_b_pos_dyn;
	uint32_t len_b_pos_ofs;
	uint32_t len_b_add_dyn;
	uint32_t len_b_add_ofs;
	uint32_t len_b_sub_dyn;

	uint32_t len_c_wr;
	uint32_t len_c_pos_dyn;
	uint32_t len_c_pos_ofs;
	uint32_t len_c_add_dyn;
	uint32_t len_c_add_ofs;
	uint32_t len_c_sub_dyn;

	uint32_t ttl_wr;
	uint32_t ttl_pos_dyn;
	uint32_t ttl_pos_ofs;

	uint32_t cs_inf;
	uint32_t l3_prt;
	uint32_t l3_frag;
	uint32_t tunnel;
	uint32_t l4_prt;
	uint32_t outer_l3_ofs;
	uint32_t outer_l4_ofs;
	uint32_t inner_l3_ofs;
	uint32_t inner_l4_ofs;
};

struct tpe_v1_csu_v0_rcp_s {
	uint32_t ol3_cmd;
	uint32_t ol4_cmd;
	uint32_t il3_cmd;
	uint32_t il4_cmd;
};

struct tpe_v2_rpp_v1_ifr_rcp_s {
	uint32_t ipv4_en;
	uint32_t ipv4_df_drop;
	uint32_t ipv6_en;
	uint32_t ipv6_drop;
	uint32_t mtu;
};

struct tpe_v2_ifr_v1_rcp_s {
	uint32_t ipv4_en;
	uint32_t ipv4_df_drop;
	uint32_t ipv6_en;
	uint32_t ipv6_drop;
	uint32_t mtu;
};

struct tpe_v2_ifr_v1_counters_s {
	uint32_t drop;
};

struct hw_mod_tpe_v3_s {
	struct tpe_v1_rpp_v0_rcp_s *rpp_rcp;

	struct tpe_v1_ins_v1_rcp_s *ins_rcp;

	struct tpe_v3_rpl_v4_rcp_s *rpl_rcp;
	struct tpe_v1_rpl_v2_ext_s *rpl_ext;
	struct tpe_v1_rpl_v2_rpl_s *rpl_rpl;

	struct tpe_v1_cpy_v1_rcp_s *cpy_rcp;

	struct tpe_v1_hfu_v1_rcp_s *hfu_rcp;

	struct tpe_v1_csu_v0_rcp_s *csu_rcp;

	struct tpe_v2_rpp_v1_ifr_rcp_s *rpp_ifr_rcp;
	struct tpe_v2_ifr_v1_rcp_s *ifr_rcp;
	struct tpe_v2_ifr_v1_counters_s *ifr_counters;
};

#endif	/* _HW_MOD_TPE_V3_H_ */
