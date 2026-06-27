/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef __SXE2_RSS_H__
#define __SXE2_RSS_H__
#include <ethdev_driver.h>
#include "sxe2_osal.h"
#include "sxe2_drv_cmd.h"
#include "sxe2_flow_define.h"

#define SXE2_FLOW_END (0xFFFF)

struct sxe2_rss_context {
	enum rte_eth_hash_function hash_func;
	uint16_t rss_key_size;
	uint16_t rss_lut_size;
	uint8_t  *rss_key;
	uint8_t  *rss_lut;
	uint64_t rss_hf;
	uint8_t symm;
	bool inited;
};

struct sxe2_rss_hf_config {
	uint64_t rss_hf;
	uint16_t hdrs[SXE2_FLOW_HDR_MAX];
	uint16_t flds[SXE2_FLOW_FLD_ID_MAX];
	uint8_t symm;
};

#define SXE2_RSS_HF_SUPPORT_ALL ( \
	RTE_ETH_RSS_IPV4 | \
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
	RTE_ETH_RSS_NONFRAG_IPV6_OTHER | \
	RTE_ETH_RSS_L2_PAYLOAD)

struct sxe2_adapter;

void sxe2_sw_rss_ctx_hw_cap_set(struct sxe2_adapter *adapter,
		struct sxe2_drv_rss_hash_caps *rss_caps);

int32_t sxe2_rss_hash_key_init(struct rte_eth_dev *dev);

void sxe2_rss_hash_key_uninit(struct rte_eth_dev *dev);

int32_t sxe2_rss_lut_init(struct rte_eth_dev *dev);

void sxe2_rss_lut_uninit(struct rte_eth_dev *dev);

int32_t sxe2_rss_init(struct rte_eth_dev *dev);

int32_t sxe2_rss_hash_function_set(struct rte_eth_dev *dev, enum rte_eth_hash_function func);

int32_t sxe2_rss_hf_type_set(struct rte_eth_dev *dev, uint64_t rss_hf);

int32_t sxe2_rss_disable(struct rte_eth_dev *dev);

int32_t sxe2_dev_rss_reta_update(struct rte_eth_dev *dev,
		struct rte_eth_rss_reta_entry64 *reta_conf,
		uint16_t reta_size);

int32_t sxe2_dev_rss_reta_query(struct rte_eth_dev *dev,
		struct rte_eth_rss_reta_entry64 *reta_conf,
		uint16_t reta_size);

int32_t sxe2_dev_rss_hash_update(struct rte_eth_dev *dev,
		struct rte_eth_rss_conf *rss_conf);

int32_t sxe2_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
		struct rte_eth_rss_conf *rss_conf);
#endif /* __SXE2_RSS_H__ */
