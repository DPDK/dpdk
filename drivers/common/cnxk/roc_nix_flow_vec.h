/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_NIX_FLOW_VEC_H_
#define _ROC_NIX_FLOW_VEC_H_

#define IPV4_SOV_OFFSET 6
#define IPV4_SOV_MASK	0x1FFF
#define IPV4_EOV_OFFSET 6
#define IPV4_EOV_MASK	0x2000

#define IPV6_SOV_OFFSET 0x2A
#define IPV6_SOV_MASK	0xFFF8
#define IPV6_EOV_OFFSET 0x2A
#define IPV6_EOV_MASK	0x1

#define CUSTOM_SOV_OFFSET 4
#define CUSTOM_SOV_MASK	  0x7F
#define CUSTOM_EOV_OFFSET 5
#define CUSTOM_EOV_MASK	  0x80

#define IPV4_VER_OFFSET	  0
#define IPV4_VER_MATCH	  0x4
#define IPV4_VER_MASK	  0xF
#define IPV6_VER_OFFSET	  0
#define IPV6_VER_MATCH	  0x6
#define IPV6_VER_MASK	  0xF
#define CUSTOM_VER_OFFSET 0
#define CUSTOM_VER_MATCH  0x2
#define CUSTOM_VER_MASK	  0xF

static union nix_af_rx_flow_vec_ctrl0x nix_flow_ctrl0[NIX_AF_RX_FLOW_VEC_CTRL_MAX] = {
	{.s.eov_inv = 0,
	 .s.eov_mask = 0,
	 .s.eov_offset = 0,
	 .s.sov_inv = 0,
	 .s.sov_mask = 0,
	 .s.sov_offset = 0,
	 .s.reserved = 0},
	{.s.eov_inv = 1,
	 .s.eov_mask = IPV4_EOV_MASK,
	 .s.eov_offset = IPV4_EOV_OFFSET,
	 .s.sov_inv = 1,
	 .s.sov_mask = IPV4_SOV_MASK,
	 .s.sov_offset = IPV4_SOV_OFFSET,
	 .s.reserved = 0},
	{.s.eov_inv = 1,
	 .s.eov_mask = IPV6_EOV_MASK,
	 .s.eov_offset = IPV6_EOV_OFFSET,
	 .s.sov_inv = 1,
	 .s.sov_mask = IPV6_SOV_MASK,
	 .s.sov_offset = IPV6_SOV_OFFSET,
	 .s.reserved = 0},
	{.s.eov_inv = 1,
	 .s.eov_mask = CUSTOM_EOV_MASK,
	 .s.eov_offset = CUSTOM_EOV_OFFSET,
	 .s.sov_inv = 1,
	 .s.sov_mask = CUSTOM_SOV_MASK,
	 .s.sov_offset = CUSTOM_SOV_OFFSET,
	 .s.reserved = 0},
};

static union nix_af_rx_flow_vec_ctrl1x nix_flow_ctrl1[NIX_AF_RX_FLOW_VEC_CTRL_MAX] = {
	{.s.ver_mask = 0,
	 .s.ver_match = 0,
	 .s.ver_offset = 0,
	 .s.ltype_mask = 0,
	 .s.ltype_match = 0,
	 .s.lid = 0,
	 .s.valid = 0,
	 .s.reserved = 0},
	{.s.ver_mask = IPV4_VER_MASK,
	 .s.ver_match = IPV4_VER_MATCH,
	 .s.ver_offset = IPV4_VER_OFFSET,
	 .s.ltype_mask = 0x3,
	 .s.ltype_match = NPC_LT_LC_IP,
	 .s.lid = NPC_LID_LC,
	 .s.valid = 1,
	 .s.reserved = 0},
	{.s.ver_mask = IPV6_VER_MASK,
	 .s.ver_match = IPV6_VER_MATCH,
	 .s.ver_offset = IPV6_VER_OFFSET,
	 .s.ltype_mask = 0x7,
	 .s.ltype_match = NPC_LT_LC_IP6,
	 .s.lid = NPC_LID_LC,
	 .s.valid = 1,
	 .s.reserved = 0},
	{.s.ver_mask = CUSTOM_VER_MASK,
	 .s.ver_match = CUSTOM_VER_MATCH,
	 .s.ver_offset = CUSTOM_VER_OFFSET,
	 .s.ltype_mask = 0xF,
	 .s.ltype_match = NPC_LT_LC_CUSTOM0,
	 .s.lid = NPC_LID_LC,
	 .s.valid = 1,
	 .s.reserved = 0},
};

#endif /* _ROC_NIX_FLOW_VEC_H_ */
