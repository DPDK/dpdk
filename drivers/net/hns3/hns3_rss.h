/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#ifndef HNS3_RSS_H
#define HNS3_RSS_H

#include <rte_ethdev.h>
#include <rte_flow.h>

#define HNS3_RSS_SUPPORT_L3_SRC_DST	(RTE_ETH_RSS_L3_SRC_ONLY | \
					 RTE_ETH_RSS_L3_DST_ONLY)
#define HNS3_RSS_SUPPORT_L4_SRC_DST	(RTE_ETH_RSS_L4_SRC_ONLY | \
					 RTE_ETH_RSS_L4_DST_ONLY)
#define HNS3_RSS_SUPPORT_L3L4		(HNS3_RSS_SUPPORT_L3_SRC_DST | \
					 HNS3_RSS_SUPPORT_L4_SRC_DST)

#define HNS3_RSS_SUPPORT_FLOW_TYPE	(RTE_ETH_RSS_IPV4 | \
					 RTE_ETH_RSS_FRAG_IPV4 | \
					 RTE_ETH_RSS_NONFRAG_IPV4_TCP | \
					 RTE_ETH_RSS_NONFRAG_IPV4_UDP | \
					 RTE_ETH_RSS_NONFRAG_IPV4_SCTP | \
					 RTE_ETH_RSS_NONFRAG_IPV4_OTHER | \
					 RTE_ETH_RSS_IPV6 | \
					 RTE_ETH_RSS_FRAG_IPV6 | \
					 RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
					 RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
					 RTE_ETH_RSS_NONFRAG_IPV6_SCTP | \
					 RTE_ETH_RSS_NONFRAG_IPV6_OTHER)

#define HNS3_ETH_RSS_SUPPORT		(HNS3_RSS_SUPPORT_FLOW_TYPE | \
					 HNS3_RSS_SUPPORT_L3L4)

enum hns3_tuple_field {
	/* IPV4_TCP ENABLE FIELD */
	HNS3_RSS_FIELD_IPV4_TCP_EN_TCP_D = 0,
	HNS3_RSS_FIELD_IPV4_TCP_EN_TCP_S,
	HNS3_RSS_FIELD_IPV4_TCP_EN_IP_D,
	HNS3_RSS_FIELD_IPV4_TCP_EN_IP_S,

	/* IPV4_UDP ENABLE FIELD */
	HNS3_RSS_FIELD_IPV4_UDP_EN_UDP_D = 8,
	HNS3_RSS_FIELD_IPV4_UDP_EN_UDP_S,
	HNS3_RSS_FIELD_IPV4_UDP_EN_IP_D,
	HNS3_RSS_FIELD_IPV4_UDP_EN_IP_S,

	/* IPV4_SCTP ENABLE FIELD */
	HNS3_RSS_FIELD_IPV4_SCTP_EN_SCTP_D = 16,
	HNS3_RSS_FIELD_IPV4_SCTP_EN_SCTP_S,
	HNS3_RSS_FIELD_IPV4_SCTP_EN_IP_D,
	HNS3_RSS_FIELD_IPV4_SCTP_EN_IP_S,
	HNS3_RSS_FIELD_IPV4_SCTP_EN_SCTP_VER,

	/* IPV4 ENABLE FIELD */
	HNS3_RSS_FIELD_IPV4_EN_NONFRAG_IP_D = 24,
	HNS3_RSS_FIELD_IPV4_EN_NONFRAG_IP_S,
	HNS3_RSS_FIELD_IPV4_EN_FRAG_IP_D,
	HNS3_RSS_FIELD_IPV4_EN_FRAG_IP_S,

	/* IPV6_TCP ENABLE FIELD */
	HNS3_RSS_FIELD_IPV6_TCP_EN_TCP_D = 32,
	HNS3_RSS_FIELD_IPV6_TCP_EN_TCP_S,
	HNS3_RSS_FIELD_IPV6_TCP_EN_IP_D,
	HNS3_RSS_FIELD_IPV6_TCP_EN_IP_S,

	/* IPV6_UDP ENABLE FIELD */
	HNS3_RSS_FIELD_IPV6_UDP_EN_UDP_D = 40,
	HNS3_RSS_FIELD_IPV6_UDP_EN_UDP_S,
	HNS3_RSS_FIELD_IPV6_UDP_EN_IP_D,
	HNS3_RSS_FIELD_IPV6_UDP_EN_IP_S,

	/* IPV6_SCTP ENABLE FIELD */
	HNS3_RSS_FIELD_IPV6_SCTP_EN_SCTP_D = 48,
	HNS3_RSS_FIELD_IPV6_SCTP_EN_SCTP_S,
	HNS3_RSS_FIELD_IPV6_SCTP_EN_IP_D,
	HNS3_RSS_FIELD_IPV6_SCTP_EN_IP_S,
	HNS3_RSS_FIELD_IPV6_SCTP_EN_SCTP_VER,

	/* IPV6 ENABLE FIELD */
	HNS3_RSS_FIELD_IPV6_NONFRAG_IP_D = 56,
	HNS3_RSS_FIELD_IPV6_NONFRAG_IP_S,
	HNS3_RSS_FIELD_IPV6_FRAG_IP_D,
	HNS3_RSS_FIELD_IPV6_FRAG_IP_S
};

#define HNS3_RSS_PCTYPE_IPV4_TCP	BIT_ULL(0)
#define HNS3_RSS_PCTYPE_IPV4_UDP	BIT_ULL(8)
#define HNS3_RSS_PCTYPE_IPV4_SCTP	BIT_ULL(16)
#define HNS3_RSS_PCTYPE_IPV4_NONF	BIT_ULL(24)
#define HNS3_RSS_PCTYPE_IPV4_FLAG	BIT_ULL(26)
#define HNS3_RSS_PCTYPE_IPV6_TCP	BIT_ULL(32)
#define HNS3_RSS_PCTYPE_IPV6_UDP	BIT_ULL(40)
#define HNS3_RSS_PCTYPE_IPV6_SCTP	BIT_ULL(48)
#define HNS3_RSS_PCTYPE_IPV6_NONF	BIT_ULL(56)
#define HNS3_RSS_PCTYPE_IPV6_FLAG	BIT_ULL(58)

#define HNS3_RSS_TUPLE_IPV4_TCP_M	GENMASK(3, 0)
#define HNS3_RSS_TUPLE_IPV4_UDP_M	GENMASK(11, 8)
#define HNS3_RSS_TUPLE_IPV4_SCTP_M	GENMASK(20, 16)
#define HNS3_RSS_TUPLE_IPV4_NONF_M	GENMASK(25, 24)
#define HNS3_RSS_TUPLE_IPV4_FLAG_M	GENMASK(27, 26)
#define HNS3_RSS_TUPLE_IPV6_TCP_M	GENMASK(35, 32)
#define HNS3_RSS_TUPLE_IPV6_UDP_M	GENMASK(43, 40)
#define HNS3_RSS_TUPLE_IPV6_SCTP_M	GENMASK(52, 48)
#define HNS3_RSS_TUPLE_IPV6_NONF_M	GENMASK(57, 56)
#define HNS3_RSS_TUPLE_IPV6_FLAG_M	GENMASK(59, 58)

#define HNS3_RSS_IND_TBL_SIZE	512 /* The size of hash lookup table */
#define HNS3_RSS_IND_TBL_SIZE_MAX 2048
#define HNS3_RSS_KEY_SIZE	40
#define HNS3_RSS_KEY_SIZE_MAX	128
#define HNS3_RSS_SET_BITMAP_MSK	0xffff

#define HNS3_RSS_HASH_ALGO_TOEPLITZ	0
#define HNS3_RSS_HASH_ALGO_SIMPLE	1
#define HNS3_RSS_HASH_ALGO_SYMMETRIC_TOEP 2
#define HNS3_RSS_HASH_ALGO_MASK		0xf

/* Same as the Max queue num under TC */
#define HNS3_RSS_QUEUES_BUFFER_NUM	512
struct hns3_rss_conf {
	uint64_t rss_hf;
	uint8_t hash_algo; /* hash function type defined by hardware */
	uint8_t key[HNS3_RSS_KEY_SIZE_MAX];  /* Hash key */
	uint16_t rss_indirection_tbl[HNS3_RSS_IND_TBL_SIZE_MAX];
	/*
	 * For IPv6 SCTP packets type, check whether the NIC hardware support
	 * RSS hash using the src/dst port as the input tuple. For Kunpeng920
	 * NIC hardware, it is not supported
	 */
	bool ipv6_sctp_offload_supported;
};

#ifndef ilog2
static inline int rss_ilog2(uint32_t x)
{
	int log = 0;
	x >>= 1;

	while (x) {
		log++;
		x >>= 1;
	}
	return log;
}
#define ilog2(x) rss_ilog2(x)
#endif

static inline uint32_t fls(uint32_t x)
{
	uint32_t position;
	uint32_t i;

	if (x == 0)
		return 0;

	for (i = (x >> 1), position = 0; i != 0; ++position)
		i >>= 1;

	return position + 1;
}

static inline uint32_t roundup_pow_of_two(uint32_t x)
{
	return 1UL << fls(x - 1);
}

extern const uint8_t hns3_hash_key[HNS3_RSS_KEY_SIZE];

struct hns3_adapter;
struct hns3_hw;

int hns3_dev_rss_hash_update(struct rte_eth_dev *dev,
			     struct rte_eth_rss_conf *rss_conf);
int hns3_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
			       struct rte_eth_rss_conf *rss_conf);
int hns3_dev_rss_reta_update(struct rte_eth_dev *dev,
			     struct rte_eth_rss_reta_entry64 *reta_conf,
			     uint16_t reta_size);
int hns3_dev_rss_reta_query(struct rte_eth_dev *dev,
			    struct rte_eth_rss_reta_entry64 *reta_conf,
			    uint16_t reta_size);
void hns3_rss_set_default_args(struct hns3_hw *hw);
int hns3_set_rss_indir_table(struct hns3_hw *hw, uint16_t *indir,
			     uint16_t size);
int hns3_rss_reset_indir_table(struct hns3_hw *hw);
int hns3_config_rss(struct hns3_adapter *hns);
void hns3_rss_uninit(struct hns3_adapter *hns);
bool hns3_check_rss_types_valid(struct hns3_hw *hw, uint64_t types);
int hns3_set_rss_tuple_by_rss_hf(struct hns3_hw *hw, uint64_t rss_hf);
int hns3_set_rss_tuple_field(struct hns3_hw *hw, uint64_t tuple_fields);
int hns3_get_rss_tuple_field(struct hns3_hw *hw, uint64_t *tuple_fields);
int hns3_rss_set_algo_key(struct hns3_hw *hw, uint8_t hash_algo,
			  const uint8_t *key, uint8_t key_len);
int hns3_rss_get_algo_key(struct hns3_hw *hw,  uint8_t *hash_algo,
			  uint8_t *key, uint8_t key_len);
uint64_t hns3_rss_calc_tuple_filed(uint64_t rss_hf);
int hns3_update_rss_algo_key(struct hns3_hw *hw, uint8_t hash_algo,
			     uint8_t *key, uint8_t key_len);

#endif /* HNS3_RSS_H */
