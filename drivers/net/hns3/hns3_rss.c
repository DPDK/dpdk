/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#include <rte_ethdev.h>
#include <rte_io.h>
#include <rte_malloc.h>

#include "hns3_ethdev.h"
#include "hns3_logs.h"

/* Default hash keys */
const uint8_t hns3_hash_key[HNS3_RSS_KEY_SIZE] = {
	0x6D, 0x5A, 0x56, 0xDA, 0x25, 0x5B, 0x0E, 0xC2,
	0x41, 0x67, 0x25, 0x3D, 0x43, 0xA3, 0x8F, 0xB0,
	0xD0, 0xCA, 0x2B, 0xCB, 0xAE, 0x7B, 0x30, 0xB4,
	0x77, 0xCB, 0x2D, 0xA3, 0x80, 0x30, 0xF2, 0x0C,
	0x6A, 0x42, 0xB7, 0x3B, 0xBE, 0xAC, 0x01, 0xFA
};

const uint8_t hns3_hash_func_map[] = {
	[RTE_ETH_HASH_FUNCTION_DEFAULT] = HNS3_RSS_HASH_ALGO_TOEPLITZ,
	[RTE_ETH_HASH_FUNCTION_TOEPLITZ] = HNS3_RSS_HASH_ALGO_TOEPLITZ,
	[RTE_ETH_HASH_FUNCTION_SIMPLE_XOR] = HNS3_RSS_HASH_ALGO_SIMPLE,
	[RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ] = HNS3_RSS_HASH_ALGO_SYMMETRIC_TOEP,
};

enum hns3_rss_tuple_type {
	HNS3_RSS_IP_TUPLE,
	HNS3_RSS_IP_L4_TUPLE,
};

static const struct {
	uint64_t rss_types;
	uint16_t tuple_type;
	uint64_t rss_field;
	uint64_t tuple_mask;
} hns3_set_tuple_table[] = {
	/* IPV4-FRAG */
	{ RTE_ETH_RSS_FRAG_IPV4 | RTE_ETH_RSS_L3_SRC_ONLY,
	  HNS3_RSS_IP_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_EN_FRAG_IP_S),
	  HNS3_RSS_TUPLE_IPV4_FLAG_M },
	{ RTE_ETH_RSS_FRAG_IPV4 | RTE_ETH_RSS_L3_DST_ONLY,
	  HNS3_RSS_IP_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_EN_FRAG_IP_D),
	  HNS3_RSS_TUPLE_IPV4_FLAG_M },
	{ RTE_ETH_RSS_FRAG_IPV4,
	  HNS3_RSS_IP_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_EN_FRAG_IP_S) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_EN_FRAG_IP_D),
	  HNS3_RSS_TUPLE_IPV4_FLAG_M },

	/* IPV4 */
	{ RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_L3_SRC_ONLY,
	  HNS3_RSS_IP_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_EN_NONFRAG_IP_S),
	  HNS3_RSS_TUPLE_IPV4_NONF_M },
	{ RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_L3_DST_ONLY,
	  HNS3_RSS_IP_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_EN_NONFRAG_IP_D),
	  HNS3_RSS_TUPLE_IPV4_NONF_M },
	{ RTE_ETH_RSS_IPV4,
	  HNS3_RSS_IP_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_EN_NONFRAG_IP_S) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_EN_NONFRAG_IP_D),
	  HNS3_RSS_TUPLE_IPV4_NONF_M },

	/* IPV4-OTHER */
	{ RTE_ETH_RSS_NONFRAG_IPV4_OTHER | RTE_ETH_RSS_L3_SRC_ONLY,
	  HNS3_RSS_IP_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_EN_NONFRAG_IP_S),
	  HNS3_RSS_TUPLE_IPV4_NONF_M },
	{ RTE_ETH_RSS_NONFRAG_IPV4_OTHER | RTE_ETH_RSS_L3_DST_ONLY,
	  HNS3_RSS_IP_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_EN_NONFRAG_IP_D),
	  HNS3_RSS_TUPLE_IPV4_NONF_M },
	{ RTE_ETH_RSS_NONFRAG_IPV4_OTHER,
	  HNS3_RSS_IP_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_EN_NONFRAG_IP_S) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_EN_NONFRAG_IP_D),
	  HNS3_RSS_TUPLE_IPV4_NONF_M },

	/* IPV4-TCP */
	{ RTE_ETH_RSS_NONFRAG_IPV4_TCP | RTE_ETH_RSS_L3_SRC_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_TCP_EN_IP_S),
	  HNS3_RSS_TUPLE_IPV4_TCP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV4_TCP | RTE_ETH_RSS_L3_DST_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_TCP_EN_IP_D),
	  HNS3_RSS_TUPLE_IPV4_TCP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV4_TCP | RTE_ETH_RSS_L4_SRC_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_TCP_EN_TCP_S),
	  HNS3_RSS_TUPLE_IPV4_TCP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV4_TCP | RTE_ETH_RSS_L4_DST_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_TCP_EN_TCP_D),
	  HNS3_RSS_TUPLE_IPV4_TCP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV4_TCP,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_TCP_EN_IP_S) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_TCP_EN_IP_D) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_TCP_EN_TCP_S) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_TCP_EN_TCP_D),
	  HNS3_RSS_TUPLE_IPV4_TCP_M },

	/* IPV4-UDP */
	{ RTE_ETH_RSS_NONFRAG_IPV4_UDP | RTE_ETH_RSS_L3_SRC_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_UDP_EN_IP_S),
	  HNS3_RSS_TUPLE_IPV4_UDP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV4_UDP | RTE_ETH_RSS_L3_DST_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_UDP_EN_IP_D),
	  HNS3_RSS_TUPLE_IPV4_UDP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV4_UDP | RTE_ETH_RSS_L4_SRC_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_UDP_EN_UDP_S),
	  HNS3_RSS_TUPLE_IPV4_UDP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV4_UDP | RTE_ETH_RSS_L4_DST_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_UDP_EN_UDP_D),
	  HNS3_RSS_TUPLE_IPV4_UDP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV4_UDP,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_UDP_EN_IP_S) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_UDP_EN_IP_D) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_UDP_EN_UDP_S) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_UDP_EN_UDP_D),
	  HNS3_RSS_TUPLE_IPV4_UDP_M },

	/* IPV4-SCTP */
	{ RTE_ETH_RSS_NONFRAG_IPV4_SCTP | RTE_ETH_RSS_L3_SRC_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_SCTP_EN_IP_S),
	  HNS3_RSS_TUPLE_IPV4_SCTP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV4_SCTP | RTE_ETH_RSS_L3_DST_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_SCTP_EN_IP_D),
	  HNS3_RSS_TUPLE_IPV4_SCTP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV4_SCTP | RTE_ETH_RSS_L4_SRC_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_SCTP_EN_SCTP_S),
	  HNS3_RSS_TUPLE_IPV4_SCTP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV4_SCTP | RTE_ETH_RSS_L4_DST_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_SCTP_EN_SCTP_D),
	  HNS3_RSS_TUPLE_IPV4_SCTP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV4_SCTP,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_SCTP_EN_IP_S) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_SCTP_EN_IP_D) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_SCTP_EN_SCTP_S) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_SCTP_EN_SCTP_D) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV4_SCTP_EN_SCTP_VER),
	  HNS3_RSS_TUPLE_IPV4_SCTP_M },

	/* IPV6-FRAG */
	{ RTE_ETH_RSS_FRAG_IPV6 | RTE_ETH_RSS_L3_SRC_ONLY,
	  HNS3_RSS_IP_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_FRAG_IP_S),
	  HNS3_RSS_TUPLE_IPV6_FLAG_M },
	{ RTE_ETH_RSS_FRAG_IPV6 | RTE_ETH_RSS_L3_DST_ONLY,
	  HNS3_RSS_IP_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_FRAG_IP_D),
	  HNS3_RSS_TUPLE_IPV6_FLAG_M },
	{ RTE_ETH_RSS_FRAG_IPV6,
	  HNS3_RSS_IP_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_FRAG_IP_S) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_FRAG_IP_D),
	  HNS3_RSS_TUPLE_IPV6_FLAG_M },

	/* IPV6 */
	{ RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_L3_SRC_ONLY,
	  HNS3_RSS_IP_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_NONFRAG_IP_S),
	  HNS3_RSS_TUPLE_IPV6_NONF_M },
	{ RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_L3_DST_ONLY,
	  HNS3_RSS_IP_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_NONFRAG_IP_D),
	  HNS3_RSS_TUPLE_IPV6_NONF_M },
	{ RTE_ETH_RSS_IPV6,
	  HNS3_RSS_IP_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_NONFRAG_IP_S) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_NONFRAG_IP_D),
	  HNS3_RSS_TUPLE_IPV6_NONF_M },

	/* IPV6-OTHER */
	{ RTE_ETH_RSS_NONFRAG_IPV6_OTHER | RTE_ETH_RSS_L3_SRC_ONLY,
	  HNS3_RSS_IP_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_NONFRAG_IP_S),
	  HNS3_RSS_TUPLE_IPV6_NONF_M },
	{ RTE_ETH_RSS_NONFRAG_IPV6_OTHER | RTE_ETH_RSS_L3_DST_ONLY,
	  HNS3_RSS_IP_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_NONFRAG_IP_D),
	  HNS3_RSS_TUPLE_IPV6_NONF_M },
	{ RTE_ETH_RSS_NONFRAG_IPV6_OTHER,
	  HNS3_RSS_IP_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_NONFRAG_IP_S) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_NONFRAG_IP_D),
	  HNS3_RSS_TUPLE_IPV6_NONF_M },

	/* IPV6-TCP */
	{ RTE_ETH_RSS_NONFRAG_IPV6_TCP | RTE_ETH_RSS_L3_SRC_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_TCP_EN_IP_S),
	  HNS3_RSS_TUPLE_IPV6_TCP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV6_TCP | RTE_ETH_RSS_L3_DST_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_TCP_EN_IP_D),
	  HNS3_RSS_TUPLE_IPV6_TCP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV6_TCP | RTE_ETH_RSS_L4_SRC_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_TCP_EN_TCP_S),
	  HNS3_RSS_TUPLE_IPV6_TCP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV6_TCP | RTE_ETH_RSS_L4_DST_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_TCP_EN_TCP_D),
	  HNS3_RSS_TUPLE_IPV6_TCP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV6_TCP,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_TCP_EN_IP_S) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_TCP_EN_IP_D) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_TCP_EN_TCP_S) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_TCP_EN_TCP_D),
	  HNS3_RSS_TUPLE_IPV6_TCP_M },

	/* IPV6-UDP */
	{ RTE_ETH_RSS_NONFRAG_IPV6_UDP | RTE_ETH_RSS_L3_SRC_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_UDP_EN_IP_S),
	  HNS3_RSS_TUPLE_IPV6_UDP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV6_UDP | RTE_ETH_RSS_L3_DST_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_UDP_EN_IP_D),
	  HNS3_RSS_TUPLE_IPV6_UDP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV6_UDP | RTE_ETH_RSS_L4_SRC_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_UDP_EN_UDP_S),
	  HNS3_RSS_TUPLE_IPV6_UDP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV6_UDP | RTE_ETH_RSS_L4_DST_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_UDP_EN_UDP_D),
	  HNS3_RSS_TUPLE_IPV6_UDP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV6_UDP,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_UDP_EN_IP_S) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_UDP_EN_IP_D) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_UDP_EN_UDP_S) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_UDP_EN_UDP_D),
	  HNS3_RSS_TUPLE_IPV6_UDP_M },

	/* IPV6-SCTP */
	{ RTE_ETH_RSS_NONFRAG_IPV6_SCTP | RTE_ETH_RSS_L3_SRC_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_SCTP_EN_IP_S),
	  HNS3_RSS_TUPLE_IPV6_SCTP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV6_SCTP | RTE_ETH_RSS_L3_DST_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_SCTP_EN_IP_D),
	  HNS3_RSS_TUPLE_IPV6_SCTP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV6_SCTP | RTE_ETH_RSS_L4_SRC_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_SCTP_EN_SCTP_S),
	  HNS3_RSS_TUPLE_IPV6_SCTP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV6_SCTP | RTE_ETH_RSS_L4_DST_ONLY,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_SCTP_EN_SCTP_D),
	  HNS3_RSS_TUPLE_IPV6_SCTP_M },
	{ RTE_ETH_RSS_NONFRAG_IPV6_SCTP,
	  HNS3_RSS_IP_L4_TUPLE,
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_SCTP_EN_IP_S) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_SCTP_EN_IP_D) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_SCTP_EN_SCTP_D) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_SCTP_EN_SCTP_S) |
	  BIT_ULL(HNS3_RSS_FIELD_IPV6_SCTP_EN_SCTP_VER),
	  HNS3_RSS_TUPLE_IPV6_SCTP_M },
};

/*
 * rss_generic_config command function, opcode:0x0D01.
 * Used to set algorithm and hash key of RSS.
 */
static int
hns3_rss_set_algo_key(struct hns3_hw *hw, uint8_t hash_algo,
		      const uint8_t *key, uint8_t key_len)
{
	struct hns3_rss_generic_config_cmd *req;
	struct hns3_cmd_desc desc;
	const uint8_t *cur_key;
	uint16_t cur_key_size;
	uint16_t max_bd_num;
	uint16_t idx;
	int ret;

	req = (struct hns3_rss_generic_config_cmd *)desc.data;

	max_bd_num = DIV_ROUND_UP(key_len, HNS3_RSS_HASH_KEY_NUM);
	for (idx = 0; idx < max_bd_num; idx++) {
		hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RSS_GENERIC_CONFIG,
					  false);

		req->hash_config |= (hash_algo & HNS3_RSS_HASH_ALGO_MASK);
		req->hash_config |= (idx << HNS3_RSS_HASH_KEY_OFFSET_B);

		if (idx == max_bd_num - 1 &&
		    (key_len % HNS3_RSS_HASH_KEY_NUM) != 0)
			cur_key_size = key_len % HNS3_RSS_HASH_KEY_NUM;
		else
			cur_key_size = HNS3_RSS_HASH_KEY_NUM;

		cur_key = key + idx * HNS3_RSS_HASH_KEY_NUM;
		memcpy(req->hash_key, cur_key, cur_key_size);

		ret = hns3_cmd_send(hw, &desc, 1);
		if (ret) {
			hns3_err(hw, "Configure RSS algo key failed %d", ret);
			return ret;
		}
	}

	return 0;
}

static int
hns3_rss_get_algo_key(struct hns3_hw *hw,  uint8_t *hash_algo,
		      uint8_t *key, uint8_t key_len)
{
	struct hns3_rss_generic_config_cmd *req;
	struct hns3_cmd_desc desc;
	uint16_t cur_key_size;
	uint16_t max_bd_num;
	uint8_t *cur_key;
	uint16_t idx;
	int ret;

	req = (struct hns3_rss_generic_config_cmd *)desc.data;
	max_bd_num = DIV_ROUND_UP(key_len, HNS3_RSS_HASH_KEY_NUM);
	for (idx = 0; idx < max_bd_num; idx++) {
		hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RSS_GENERIC_CONFIG,
					  true);

		req->hash_config |= (idx << HNS3_RSS_HASH_KEY_OFFSET_B);
		ret = hns3_cmd_send(hw, &desc, 1);
		if (ret) {
			hns3_err(hw, "fail to obtain RSS algo and key from firmware, ret = %d",
				 ret);
			return ret;
		}

		if (idx == 0)
			*hash_algo = req->hash_config & HNS3_RSS_HASH_ALGO_MASK;

		if (idx == max_bd_num - 1 &&
		    (key_len % HNS3_RSS_HASH_KEY_NUM) != 0)
			cur_key_size = key_len % HNS3_RSS_HASH_KEY_NUM;
		else
			cur_key_size = HNS3_RSS_HASH_KEY_NUM;

		cur_key = key + idx * HNS3_RSS_HASH_KEY_NUM;
		memcpy(cur_key, req->hash_key, cur_key_size);
	}

	return 0;
}

/*
 * rss_indirection_table command function, opcode:0x0D07.
 * Used to configure the indirection table of rss.
 */
int
hns3_set_rss_indir_table(struct hns3_hw *hw, uint16_t *indir, uint16_t size)
{
	struct hns3_rss_indirection_table_cmd *req;
	uint16_t max_bd_num, cfg_tbl_size;
	struct hns3_cmd_desc desc;
	uint8_t qid_msb_off;
	uint8_t qid_msb_val;
	uint16_t q_id;
	uint16_t i, j;
	int ret;

	req = (struct hns3_rss_indirection_table_cmd *)desc.data;
	max_bd_num = DIV_ROUND_UP(size, HNS3_RSS_CFG_TBL_SIZE);
	for (i = 0; i < max_bd_num; i++) {
		hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RSS_INDIR_TABLE,
					  false);
		req->start_table_index =
				rte_cpu_to_le_16(i * HNS3_RSS_CFG_TBL_SIZE);
		req->rss_set_bitmap = rte_cpu_to_le_16(HNS3_RSS_SET_BITMAP_MSK);

		if (i == max_bd_num - 1 && (size % HNS3_RSS_CFG_TBL_SIZE) != 0)
			cfg_tbl_size = size % HNS3_RSS_CFG_TBL_SIZE;
		else
			cfg_tbl_size = HNS3_RSS_CFG_TBL_SIZE;

		for (j = 0; j < cfg_tbl_size; j++) {
			q_id = indir[i * HNS3_RSS_CFG_TBL_SIZE + j];
			req->rss_result_l[j] = q_id & 0xff;

			qid_msb_off =
				j * HNS3_RSS_CFG_TBL_BW_H / HNS3_BITS_PER_BYTE;
			qid_msb_val = (q_id >> HNS3_RSS_CFG_TBL_BW_L & 0x1)
					<< (j * HNS3_RSS_CFG_TBL_BW_H %
					HNS3_BITS_PER_BYTE);
			req->rss_result_h[qid_msb_off] |= qid_msb_val;
		}

		ret = hns3_cmd_send(hw, &desc, 1);
		if (ret) {
			hns3_err(hw,
				 "Sets RSS indirection table failed %d size %u",
				 ret, size);
			return ret;
		}
	}

	return 0;
}

static int
hns3_get_rss_indir_table(struct hns3_hw *hw, uint16_t *indir, uint16_t size)
{
	struct hns3_rss_indirection_table_cmd *req;
	uint16_t max_bd_num, cfg_tbl_size;
	uint8_t qid_msb_off, qid_msb_idx;
	struct hns3_cmd_desc desc;
	uint16_t q_id, q_hi, q_lo;
	uint8_t rss_result_h;
	uint16_t i, j;
	int ret;

	req = (struct hns3_rss_indirection_table_cmd *)desc.data;
	max_bd_num = DIV_ROUND_UP(size, HNS3_RSS_CFG_TBL_SIZE);
	for (i = 0; i < max_bd_num; i++) {
		hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RSS_INDIR_TABLE,
					  true);
		req->start_table_index =
				rte_cpu_to_le_16(i * HNS3_RSS_CFG_TBL_SIZE);
		ret = hns3_cmd_send(hw, &desc, 1);
		if (ret) {
			hns3_err(hw, "fail to get RSS indirection table from firmware, ret = %d",
				 ret);
			return ret;
		}

		if (i == max_bd_num - 1 && (size % HNS3_RSS_CFG_TBL_SIZE) != 0)
			cfg_tbl_size = size % HNS3_RSS_CFG_TBL_SIZE;
		else
			cfg_tbl_size = HNS3_RSS_CFG_TBL_SIZE;

		for (j = 0; j < cfg_tbl_size; j++) {
			qid_msb_idx =
				j * HNS3_RSS_CFG_TBL_BW_H / HNS3_BITS_PER_BYTE;
			rss_result_h = req->rss_result_h[qid_msb_idx];
			qid_msb_off =
				j * HNS3_RSS_CFG_TBL_BW_H % HNS3_BITS_PER_BYTE;
			q_hi = (rss_result_h >> qid_msb_off) &
						HNS3_RSS_CFG_TBL_BW_H_M;
			q_lo = req->rss_result_l[j];
			q_id = (q_hi << HNS3_RSS_CFG_TBL_BW_L) | q_lo;
			indir[i * HNS3_RSS_CFG_TBL_SIZE + j] = q_id;
		}
	}

	return 0;
}

int
hns3_rss_reset_indir_table(struct hns3_hw *hw)
{
	uint16_t *lut;
	int ret;

	lut = rte_zmalloc("hns3_rss_lut",
			  hw->rss_ind_tbl_size * sizeof(uint16_t), 0);
	if (lut == NULL) {
		hns3_err(hw, "No hns3_rss_lut memory can be allocated");
		return -ENOMEM;
	}

	ret = hns3_set_rss_indir_table(hw, lut, hw->rss_ind_tbl_size);
	if (ret != 0)
		hns3_err(hw, "RSS uninit indir table failed, ret = %d.", ret);
	else
		memcpy(hw->rss_info.rss_indirection_tbl, lut,
		       sizeof(uint16_t) * hw->rss_ind_tbl_size);
	rte_free(lut);

	return ret;
}

bool
hns3_check_rss_types_valid(struct hns3_hw *hw, uint64_t types)
{
	uint64_t ip_mask = RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4 |
			   RTE_ETH_RSS_NONFRAG_IPV4_OTHER |
			   RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_FRAG_IPV6 |
			   RTE_ETH_RSS_NONFRAG_IPV6_OTHER;
	uint64_t ip_l4_mask = RTE_ETH_RSS_NONFRAG_IPV4_TCP |
			      RTE_ETH_RSS_NONFRAG_IPV4_UDP |
			      RTE_ETH_RSS_NONFRAG_IPV4_SCTP |
			      RTE_ETH_RSS_NONFRAG_IPV6_TCP |
			      RTE_ETH_RSS_NONFRAG_IPV6_UDP |
			      RTE_ETH_RSS_NONFRAG_IPV6_SCTP;
	bool has_l4_src_dst = !!(types & HNS3_RSS_SUPPORT_L4_SRC_DST);
	bool has_ip_pkt = !!(types & ip_mask);
	uint64_t final_types;

	if (types == 0)
		return true;

	if ((types & HNS3_ETH_RSS_SUPPORT) == 0) {
		hns3_err(hw, "specified types(0x%" PRIx64 ") are unsupported.",
			 types);
		return false;
	}

	if ((types & HNS3_RSS_SUPPORT_L3_SRC_DST) != 0 &&
	    (types & HNS3_RSS_SUPPORT_FLOW_TYPE) == 0) {
		hns3_err(hw, "IP or IP-TCP/UDP/SCTP packet type isn't specified, L3_SRC/DST_ONLY cannot be set.");
		return false;
	}

	if (has_l4_src_dst && (types & ip_l4_mask) == 0) {
		if (!has_ip_pkt) {
			hns3_err(hw, "IP-TCP/UDP/SCTP packet type isn't specified, L4_SRC/DST_ONLY cannot be set.");
			return false;
		}
		/*
		 * For the case that the types has L4_SRC/DST_ONLY but hasn't
		 * IP-TCP/UDP/SCTP packet type, this types is considered valid
		 * if it also has IP packet type.
		 */
		hns3_warn(hw, "L4_SRC/DST_ONLY is ignored because of no including L4 packet.");
	}

	if ((types & ~HNS3_ETH_RSS_SUPPORT) != 0) {
		final_types = types & HNS3_ETH_RSS_SUPPORT;
		hns3_warn(hw, "set RSS types based on hardware support, requested:0x%" PRIx64 " configured:0x%" PRIx64 "",
			  types, final_types);
	}

	return true;
}

uint64_t
hns3_rss_calc_tuple_filed(uint64_t rss_hf)
{
	uint64_t l3_only_mask = RTE_ETH_RSS_L3_SRC_ONLY |
				RTE_ETH_RSS_L3_DST_ONLY;
	uint64_t l4_only_mask = RTE_ETH_RSS_L4_SRC_ONLY |
				RTE_ETH_RSS_L4_DST_ONLY;
	uint64_t l3_l4_only_mask = l3_only_mask | l4_only_mask;
	bool has_l3_l4_only = !!(rss_hf & l3_l4_only_mask);
	bool has_l3_only = !!(rss_hf & l3_only_mask);
	uint64_t tuple = 0;
	uint32_t i;

	for (i = 0; i < RTE_DIM(hns3_set_tuple_table); i++) {
		if ((rss_hf & hns3_set_tuple_table[i].rss_types) !=
		    hns3_set_tuple_table[i].rss_types)
			continue;

		if (hns3_set_tuple_table[i].tuple_type == HNS3_RSS_IP_TUPLE) {
			if (hns3_set_tuple_table[i].rss_types & l3_only_mask ||
			    !has_l3_only)
				tuple |= hns3_set_tuple_table[i].rss_field;
			continue;
		}

		/* For IP types with L4, we need check both L3 and L4 */
		if (hns3_set_tuple_table[i].rss_types & l3_l4_only_mask ||
		    !has_l3_l4_only)
			tuple |= hns3_set_tuple_table[i].rss_field;
	}

	return tuple;
}

int
hns3_set_rss_tuple_field(struct hns3_hw *hw, uint64_t tuple_fields)
{
	struct hns3_rss_input_tuple_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RSS_INPUT_TUPLE, false);
	req = (struct hns3_rss_input_tuple_cmd *)desc.data;
	req->tuple_field = rte_cpu_to_le_64(tuple_fields);
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret != 0)
		hns3_err(hw, "set RSS hash tuple fields failed ret = %d", ret);

	return ret;
}

int
hns3_set_rss_tuple_by_rss_hf(struct hns3_hw *hw, uint64_t rss_hf)
{
	uint64_t tuple_fields;
	int ret;

	tuple_fields = hns3_rss_calc_tuple_filed(rss_hf);
	ret = hns3_set_rss_tuple_field(hw, tuple_fields);
	if (ret != 0)
		hns3_err(hw, "Update RSS flow types tuples failed, ret = %d",
			 ret);

	return ret;
}

/*
 * Configure RSS hash protocols and hash key.
 * @param dev
 *   Pointer to Ethernet device.
 * @praram rss_conf
 *   The configuration select of  rss key size and tuple flow_types.
 * @return
 *   0 on success, a negative errno value otherwise is set.
 */
int
hns3_dev_rss_hash_update(struct rte_eth_dev *dev,
			 struct rte_eth_rss_conf *rss_conf)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint64_t rss_hf_bk = hw->rss_info.rss_hf;
	uint8_t key_len = rss_conf->rss_key_len;
	uint64_t rss_hf = rss_conf->rss_hf;
	uint8_t *key = rss_conf->rss_key;
	int ret;

	if (key && key_len != hw->rss_key_size) {
		hns3_err(hw, "the hash key len(%u) is invalid, must be %u",
			 key_len, hw->rss_key_size);
		return -EINVAL;
	}

	if (!hns3_check_rss_types_valid(hw, rss_hf))
		return -EINVAL;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_set_rss_tuple_by_rss_hf(hw, rss_hf);
	if (ret)
		goto set_tuple_fail;

	ret = hns3_update_rss_algo_key(hw, rss_conf->algorithm, key, key_len);
	if (ret != 0)
		goto set_algo_key_fail;

	if (rss_conf->algorithm != RTE_ETH_HASH_FUNCTION_DEFAULT)
		hw->rss_info.hash_algo = hns3_hash_func_map[rss_conf->algorithm];
	if (key != NULL)
		memcpy(hw->rss_info.key, key, hw->rss_key_size);
	hw->rss_info.rss_hf = rss_hf;
	rte_spinlock_unlock(&hw->lock);

	return 0;

set_algo_key_fail:
	(void)hns3_set_rss_tuple_by_rss_hf(hw, rss_hf_bk);
set_tuple_fail:
	rte_spinlock_unlock(&hw->lock);
	return ret;
}

int
hns3_get_rss_tuple_field(struct hns3_hw *hw, uint64_t *tuple_fields)
{
	struct hns3_rss_input_tuple_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RSS_INPUT_TUPLE, true);
	req = (struct hns3_rss_input_tuple_cmd *)desc.data;
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret != 0) {
		hns3_err(hw, "fail to get RSS hash tuple fields from firmware, ret = %d",
			 ret);
		return ret;
	}

	*tuple_fields = rte_le_to_cpu_64(req->tuple_field);

	return 0;
}

static uint64_t
hns3_rss_tuple_fields_to_rss_hf(struct hns3_hw *hw, uint64_t tuple_fields)
{
	uint64_t ipv6_sctp_l4_mask =
				BIT_ULL(HNS3_RSS_FIELD_IPV6_SCTP_EN_SCTP_D) |
				BIT_ULL(HNS3_RSS_FIELD_IPV6_SCTP_EN_SCTP_S);
	uint64_t rss_hf = 0;
	uint64_t tuple_mask;
	uint32_t i;

	for (i = 0; i < RTE_DIM(hns3_set_tuple_table); i++) {
		tuple_mask = hns3_set_tuple_table[i].tuple_mask;
		/*
		 * The RSS hash of the packet type is disabled if its tuples is
		 * zero.
		 */
		if ((tuple_fields & tuple_mask) == 0)
			continue;

		/*
		 * Some hardware don't support to use src/dst port fields to
		 * hash for IPV6-SCTP packet.
		 */
		if ((hns3_set_tuple_table[i].rss_types &
					RTE_ETH_RSS_NONFRAG_IPV6_SCTP) &&
		    !hw->rss_info.ipv6_sctp_offload_supported)
			tuple_mask &= ~ipv6_sctp_l4_mask;

		/*
		 * The framework (ethdev ops) or driver (rte flow API) ensure
		 * that both L3_SRC/DST_ONLY and L4_SRC/DST_ONLY cannot be set
		 * to driver at the same time. But if user doesn't specify
		 * anything L3/L4_SRC/DST_ONLY, driver enables all tuple fields.
		 * In this case, driver should not report L3/L4_SRC/DST_ONLY.
		 */
		if ((tuple_fields & tuple_mask) == tuple_mask) {
			/* Skip the item enabled part tuples. */
			if ((tuple_fields & hns3_set_tuple_table[i].rss_field) !=
					tuple_mask)
				continue;

			rss_hf |= hns3_set_tuple_table[i].rss_types;
			continue;
		}

		/* Match the item enabled part tuples.*/
		if ((tuple_fields & hns3_set_tuple_table[i].rss_field) ==
					hns3_set_tuple_table[i].rss_field)
			rss_hf |= hns3_set_tuple_table[i].rss_types;
	}

	return rss_hf;
}

static int
hns3_rss_hash_get_rss_hf(struct hns3_hw *hw, uint64_t *rss_hf)
{
	uint64_t tuple_fields;
	int ret;

	ret = hns3_get_rss_tuple_field(hw, &tuple_fields);
	if (ret != 0)
		return ret;

	*rss_hf = hns3_rss_tuple_fields_to_rss_hf(hw, tuple_fields);

	return 0;
}

/*
 * Get rss key and rss_hf types set of RSS hash configuration.
 * @param dev
 *   Pointer to Ethernet device.
 * @praram rss_conf
 *   The buffer to get rss key size and tuple types.
 * @return
 *   0 on success.
 */
int
hns3_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
			   struct rte_eth_rss_conf *rss_conf)
{
	const uint8_t hash_func_map[] = {
		[HNS3_RSS_HASH_ALGO_TOEPLITZ] = RTE_ETH_HASH_FUNCTION_TOEPLITZ,
		[HNS3_RSS_HASH_ALGO_SIMPLE] = RTE_ETH_HASH_FUNCTION_SIMPLE_XOR,
		[HNS3_RSS_HASH_ALGO_SYMMETRIC_TOEP] = RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ,
	};
	struct hns3_adapter *hns = dev->data->dev_private;
	uint8_t rss_key[HNS3_RSS_KEY_SIZE_MAX] = {0};
	struct hns3_hw *hw = &hns->hw;
	uint8_t hash_algo = 0;
	int ret;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_rss_hash_get_rss_hf(hw, &rss_conf->rss_hf);
	if (ret != 0) {
		rte_spinlock_unlock(&hw->lock);
		hns3_err(hw, "obtain hash tuples failed, ret = %d", ret);
		return ret;
	}

	ret = hns3_rss_get_algo_key(hw, &hash_algo, rss_key, hw->rss_key_size);
	if (ret != 0) {
		rte_spinlock_unlock(&hw->lock);
		hns3_err(hw, "obtain hash algo and key failed, ret = %d", ret);
		return ret;
	}
	rte_spinlock_unlock(&hw->lock);

	/* Get the RSS Key if user required. */
	if (rss_conf->rss_key && rss_conf->rss_key_len >= hw->rss_key_size) {
		memcpy(rss_conf->rss_key, rss_key, hw->rss_key_size);
		rss_conf->rss_key_len = hw->rss_key_size;
	}
	rss_conf->algorithm = hash_func_map[hash_algo];

	return 0;
}

/*
 * Update rss redirection table of RSS.
 * @param dev
 *   Pointer to Ethernet device.
 * @praram reta_conf
 *   Pointer to the configuration select of mask and redirection tables.
 * @param reta_size
 *   Redirection table size.
 * @return
 *   0 on success, a negative errno value otherwise is set.
 */
int
hns3_dev_rss_reta_update(struct rte_eth_dev *dev,
			 struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_rss_conf *rss_cfg = &hw->rss_info;
	uint16_t indirection_tbl[HNS3_RSS_IND_TBL_SIZE_MAX];
	uint16_t idx, shift;
	uint16_t i;
	int ret;

	if (reta_size != hw->rss_ind_tbl_size) {
		hns3_err(hw, "The size of hash lookup table configured (%u)"
			 "doesn't match the number hardware can supported"
			 "(%u)", reta_size, hw->rss_ind_tbl_size);
		return -EINVAL;
	}
	rte_spinlock_lock(&hw->lock);
	memcpy(indirection_tbl, rss_cfg->rss_indirection_tbl,
	       sizeof(rss_cfg->rss_indirection_tbl));
	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		if (reta_conf[idx].reta[shift] >= hw->alloc_rss_size) {
			hns3_err(hw, "queue id(%u) set to redirection table "
				 "exceeds queue number(%u) allocated to a TC",
				 reta_conf[idx].reta[shift],
				 hw->alloc_rss_size);
			ret = -EINVAL;
			goto out;
		}

		if (reta_conf[idx].mask & (1ULL << shift))
			indirection_tbl[i] = reta_conf[idx].reta[shift];
	}

	ret = hns3_set_rss_indir_table(hw, indirection_tbl,
				       hw->rss_ind_tbl_size);
	if (ret != 0)
		goto out;

	memcpy(rss_cfg->rss_indirection_tbl, indirection_tbl,
	       sizeof(uint16_t) * hw->rss_ind_tbl_size);

out:
	rte_spinlock_unlock(&hw->lock);
	return ret;
}

/*
 * Get rss redirection table of RSS hash configuration.
 * @param dev
 *   Pointer to Ethernet device.
 * @praram reta_conf
 *   Pointer to the configuration select of mask and redirection tables.
 * @param reta_size
 *   Redirection table size.
 * @return
 *   0 on success, a negative errno value otherwise is set.
 */
int
hns3_dev_rss_reta_query(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	uint16_t reta_table[HNS3_RSS_IND_TBL_SIZE_MAX];
	struct hns3_hw *hw = &hns->hw;
	uint16_t idx, shift;
	uint16_t i;
	int ret;

	if (reta_size != hw->rss_ind_tbl_size) {
		hns3_err(hw, "The size of hash lookup table configured (%u)"
			 " doesn't match the number hardware can supported"
			 "(%u)", reta_size, hw->rss_ind_tbl_size);
		return -EINVAL;
	}
	rte_spinlock_lock(&hw->lock);
	ret = hns3_get_rss_indir_table(hw, reta_table, reta_size);
	if (ret != 0) {
		rte_spinlock_unlock(&hw->lock);
		hns3_err(hw, "query RSS redirection table failed, ret = %d.",
			 ret);
		return ret;
	}
	rte_spinlock_unlock(&hw->lock);

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			reta_conf[idx].reta[shift] = reta_table[i];
	}

	return 0;
}

static void
hns3_set_rss_tc_mode_entry(struct hns3_hw *hw, uint8_t *tc_valid,
			   uint16_t *tc_size, uint16_t *tc_offset,
			   uint8_t tc_num)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	uint16_t rss_size = hw->alloc_rss_size;
	uint16_t roundup_size;
	uint16_t i;

	roundup_size = roundup_pow_of_two(rss_size);
	roundup_size = ilog2(roundup_size);

	for (i = 0; i < tc_num; i++) {
		if (hns->is_vf) {
			/*
			 * For packets with VLAN priorities destined for the VF,
			 * hardware still assign Rx queue based on the Up-to-TC
			 * mapping PF configured. But VF has only one TC. If
			 * other TC don't enable, it causes that the priority
			 * packets that aren't destined for TC0 aren't received
			 * by RSS hash but is destined for queue 0. So driver
			 * has to enable the unused TC by using TC0 queue
			 * mapping configuration.
			 */
			tc_valid[i] = (hw->hw_tc_map & BIT(i)) ?
					!!(hw->hw_tc_map & BIT(i)) : 1;
			tc_size[i] = roundup_size;
			tc_offset[i] = (hw->hw_tc_map & BIT(i)) ?
					rss_size * i : 0;
		} else {
			tc_valid[i] = !!(hw->hw_tc_map & BIT(i));
			tc_size[i] = tc_valid[i] ? roundup_size : 0;
			tc_offset[i] = tc_valid[i] ? rss_size * i : 0;
		}
	}
}

static int
hns3_set_rss_tc_mode(struct hns3_hw *hw)
{
	struct hns3_rss_tc_mode_cmd *req;
	uint16_t tc_offset[HNS3_MAX_TC_NUM];
	uint8_t tc_valid[HNS3_MAX_TC_NUM];
	uint16_t tc_size[HNS3_MAX_TC_NUM];
	struct hns3_cmd_desc desc;
	uint16_t i;
	int ret;

	hns3_set_rss_tc_mode_entry(hw, tc_valid, tc_size,
				   tc_offset, HNS3_MAX_TC_NUM);

	req = (struct hns3_rss_tc_mode_cmd *)desc.data;
	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RSS_TC_MODE, false);
	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		uint16_t mode = 0;

		hns3_set_bit(mode, HNS3_RSS_TC_VALID_B, (tc_valid[i] & 0x1));
		hns3_set_field(mode, HNS3_RSS_TC_SIZE_M, HNS3_RSS_TC_SIZE_S,
			       tc_size[i]);
		if (tc_size[i] >> HNS3_RSS_TC_SIZE_MSB_OFFSET > 0)
			hns3_set_bit(mode, HNS3_RSS_TC_SIZE_MSB_S, 1);
		hns3_set_field(mode, HNS3_RSS_TC_OFFSET_M, HNS3_RSS_TC_OFFSET_S,
			       tc_offset[i]);

		req->rss_tc_mode[i] = rte_cpu_to_le_16(mode);
	}
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "Sets rss tc mode failed %d", ret);

	return ret;
}

/*
 * Note: the 'hash_algo' is defined by enum rte_eth_hash_function.
 */
int
hns3_update_rss_algo_key(struct hns3_hw *hw, uint8_t hash_func,
			 uint8_t *key, uint8_t key_len)
{
	uint8_t rss_key[HNS3_RSS_KEY_SIZE_MAX] = {0};
	bool modify_key, modify_algo;
	uint8_t hash_algo = 0;
	int ret;

	modify_key = (key != NULL && key_len > 0);
	modify_algo = hash_func != RTE_ETH_HASH_FUNCTION_DEFAULT;
	if (!modify_key && !modify_algo)
		return 0;

	if (modify_algo && hash_func >= RTE_DIM(hns3_hash_func_map)) {
		hns3_err(hw, "hash func (%u) is unsupported.", hash_func);
		return -ENOTSUP;
	}
	if (modify_key && key_len != hw->rss_key_size) {
		hns3_err(hw, "hash key length (%u) is invalid.", key_len);
		return -EINVAL;
	}

	ret = hns3_rss_get_algo_key(hw, &hash_algo, rss_key, hw->rss_key_size);
	if (ret != 0) {
		hns3_err(hw, "fail to get RSS hash algorithm and key, ret = %d",
			 ret);
		return ret;
	}

	if (modify_algo)
		hash_algo = hns3_hash_func_map[hash_func];
	if (modify_key)
		memcpy(rss_key, key, key_len);

	ret = hns3_rss_set_algo_key(hw, hash_algo, rss_key, hw->rss_key_size);
	if (ret != 0)
		hns3_err(hw, "fail to set RSS hash algorithm and key, ret = %d",
			 ret);

	return ret;
}

static void
hns3_rss_tuple_uninit(struct hns3_hw *hw)
{
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RSS_INPUT_TUPLE, false);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "RSS uninit tuple failed %d", ret);
		return;
	}
}

/*
 * Set the default rss configuration in the init of driver.
 */
void
hns3_rss_set_default_args(struct hns3_hw *hw)
{
	struct hns3_rss_conf *rss_cfg = &hw->rss_info;
	uint16_t queue_num = hw->alloc_rss_size;
	uint16_t i;

	/* Default hash algorithm */
	rss_cfg->hash_algo = HNS3_RSS_HASH_ALGO_TOEPLITZ;

	hw->rss_info.rss_hf = 0;
	memcpy(rss_cfg->key, hns3_hash_key,
		RTE_MIN(sizeof(hns3_hash_key), hw->rss_key_size));

	/* Initialize RSS indirection table */
	for (i = 0; i < hw->rss_ind_tbl_size; i++)
		rss_cfg->rss_indirection_tbl[i] = i % queue_num;
}

/*
 * RSS initialization for hns3 PMD.
 */
int
hns3_config_rss(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_rss_conf *rss_cfg = &hw->rss_info;
	uint8_t *hash_key = rss_cfg->key;
	uint64_t rss_hf;
	int ret;

	enum rte_eth_rx_mq_mode mq_mode = hw->data->dev_conf.rxmode.mq_mode;

	ret = hns3_rss_set_algo_key(hw, rss_cfg->hash_algo,
				    hash_key, hw->rss_key_size);
	if (ret)
		return ret;

	ret = hns3_set_rss_indir_table(hw, rss_cfg->rss_indirection_tbl,
				       hw->rss_ind_tbl_size);
	if (ret)
		return ret;

	ret = hns3_set_rss_tc_mode(hw);
	if (ret)
		return ret;

	/*
	 * When multi-queue RSS mode flag is not set or unsupported tuples are
	 * set, disable all tuples.
	 */
	rss_hf = hw->rss_info.rss_hf;
	if (!((uint32_t)mq_mode & RTE_ETH_MQ_RX_RSS_FLAG) ||
	    !(rss_hf & HNS3_ETH_RSS_SUPPORT))
		rss_hf = 0;

	ret = hns3_set_rss_tuple_by_rss_hf(hw, rss_hf);
	if (ret != 0) {
		hns3_err(hw, "set RSS tuples failed, ret = %d.", ret);
		return ret;
	}
	hw->rss_info.rss_hf = rss_hf;

	return 0;
}

/*
 * RSS uninitialization for hns3 PMD.
 */
void
hns3_rss_uninit(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	int ret;

	hns3_rss_tuple_uninit(hw);
	ret = hns3_rss_reset_indir_table(hw);
	if (ret != 0)
		return;

	/* Disable RSS */
	hw->rss_info.rss_hf = 0;
}
