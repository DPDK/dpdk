/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#ifndef _HNS3_RSS_H_
#define _HNS3_RSS_H_
#include <rte_ethdev.h>
#include <rte_flow.h>

#define HNS3_ETH_RSS_SUPPORT ( \
	RTE_ETH_RSS_FRAG_IPV4 | \
	RTE_ETH_RSS_NONFRAG_IPV4_TCP | \
	RTE_ETH_RSS_NONFRAG_IPV4_UDP | \
	RTE_ETH_RSS_NONFRAG_IPV4_SCTP | \
	RTE_ETH_RSS_NONFRAG_IPV4_OTHER | \
	RTE_ETH_RSS_FRAG_IPV6 | \
	RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
	RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
	RTE_ETH_RSS_NONFRAG_IPV6_SCTP | \
	RTE_ETH_RSS_NONFRAG_IPV6_OTHER | \
	RTE_ETH_RSS_L3_SRC_ONLY | \
	RTE_ETH_RSS_L3_DST_ONLY | \
	RTE_ETH_RSS_L4_SRC_ONLY | \
	RTE_ETH_RSS_L4_DST_ONLY)

#define HNS3_RSS_IND_TBL_SIZE	512 /* The size of hash lookup table */
#define HNS3_RSS_IND_TBL_SIZE_MAX 2048
#define HNS3_RSS_KEY_SIZE	40
#define HNS3_RSS_SET_BITMAP_MSK	0xffff

#define HNS3_RSS_HASH_ALGO_TOEPLITZ	0
#define HNS3_RSS_HASH_ALGO_SIMPLE	1
#define HNS3_RSS_HASH_ALGO_SYMMETRIC_TOEP 2
#define HNS3_RSS_HASH_ALGO_MASK		0xf

struct hns3_rss_tuple_cfg {
	uint64_t rss_tuple_fields;
};

#define HNS3_RSS_QUEUES_BUFFER_NUM	64 /* Same as the Max rx/tx queue num */
struct hns3_rss_conf {
	/* RSS parameters :algorithm, flow_types,  key, queue */
	struct rte_flow_action_rss conf;
	uint8_t hash_algo; /* hash function type defined by hardware */
	uint8_t key[HNS3_RSS_KEY_SIZE];  /* Hash key */
	struct hns3_rss_tuple_cfg rss_tuple_sets;
	uint16_t rss_indirection_tbl[HNS3_RSS_IND_TBL_SIZE_MAX];
	uint16_t queue[HNS3_RSS_QUEUES_BUFFER_NUM]; /* Queues indices to use */
	bool valid; /* check if RSS rule is valid */
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

struct hns3_adapter;

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
int hns3_set_rss_tuple_by_rss_hf(struct hns3_hw *hw,
				 struct hns3_rss_tuple_cfg *tuple,
				 uint64_t rss_hf);
int hns3_rss_set_algo_key(struct hns3_hw *hw, const uint8_t *key);
int hns3_restore_rss_filter(struct rte_eth_dev *dev);

#endif /* _HNS3_RSS_H_ */
