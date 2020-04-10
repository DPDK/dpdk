/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2019 Hisilicon Limited.
 */

#include <stdbool.h>
#include <rte_ethdev.h>
#include <rte_io.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_spinlock.h>

#include "hns3_ethdev.h"
#include "hns3_logs.h"

/*
 * The hash key used for rss initialization.
 */
static const uint8_t hns3_hash_key[] = {
	0x6D, 0x5A, 0x56, 0xDA, 0x25, 0x5B, 0x0E, 0xC2,
	0x41, 0x67, 0x25, 0x3D, 0x43, 0xA3, 0x8F, 0xB0,
	0xD0, 0xCA, 0x2B, 0xCB, 0xAE, 0x7B, 0x30, 0xB4,
	0x77, 0xCB, 0x2D, 0xA3, 0x80, 0x30, 0xF2, 0x0C,
	0x6A, 0x42, 0xB7, 0x3B, 0xBE, 0xAC, 0x01, 0xFA
};

/*
 * rss_generic_config command function, opcode:0x0D01.
 * Used to set algorithm, key_offset and hash key of rss.
 */
int
hns3_set_rss_algo_key(struct hns3_hw *hw, uint8_t hash_algo, const uint8_t *key)
{
#define HNS3_KEY_OFFSET_MAX	3
#define HNS3_SET_HASH_KEY_BYTE_FOUR	2

	struct hns3_rss_generic_config_cmd *req;
	struct hns3_cmd_desc desc;
	uint32_t key_offset, key_size;
	const uint8_t *key_cur;
	uint8_t cur_offset;
	int ret;

	req = (struct hns3_rss_generic_config_cmd *)desc.data;

	/*
	 * key_offset=0, hash key byte0~15 is set to hardware.
	 * key_offset=1, hash key byte16~31 is set to hardware.
	 * key_offset=2, hash key byte32~39 is set to hardware.
	 */
	for (key_offset = 0; key_offset < HNS3_KEY_OFFSET_MAX; key_offset++) {
		hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RSS_GENERIC_CONFIG,
					  false);

		req->hash_config |= (hash_algo & HNS3_RSS_HASH_ALGO_MASK);
		req->hash_config |= (key_offset << HNS3_RSS_HASH_KEY_OFFSET_B);

		if (key_offset == HNS3_SET_HASH_KEY_BYTE_FOUR)
			key_size = HNS3_RSS_KEY_SIZE - HNS3_RSS_HASH_KEY_NUM *
			HNS3_SET_HASH_KEY_BYTE_FOUR;
		else
			key_size = HNS3_RSS_HASH_KEY_NUM;

		cur_offset = key_offset * HNS3_RSS_HASH_KEY_NUM;
		key_cur = key + cur_offset;
		memcpy(req->hash_key, key_cur, key_size);

		ret = hns3_cmd_send(hw, &desc, 1);
		if (ret) {
			hns3_err(hw, "Configure RSS algo key failed %d", ret);
			return ret;
		}
	}
	/* Update the shadow RSS key with user specified */
	memcpy(hw->rss_info.key, key, HNS3_RSS_KEY_SIZE);
	return 0;
}

/*
 * Used to configure the tuple selection for RSS hash input.
 */
static int
hns3_set_rss_input_tuple(struct hns3_hw *hw)
{
	struct hns3_rss_conf *rss_config = &hw->rss_info;
	struct hns3_rss_input_tuple_cmd *req;
	struct hns3_cmd_desc desc_tuple;
	int ret;

	hns3_cmd_setup_basic_desc(&desc_tuple, HNS3_OPC_RSS_INPUT_TUPLE, false);

	req = (struct hns3_rss_input_tuple_cmd *)desc_tuple.data;

	req->ipv4_tcp_en = rss_config->rss_tuple_sets.ipv4_tcp_en;
	req->ipv4_udp_en = rss_config->rss_tuple_sets.ipv4_udp_en;
	req->ipv4_sctp_en = rss_config->rss_tuple_sets.ipv4_sctp_en;
	req->ipv4_fragment_en = rss_config->rss_tuple_sets.ipv4_fragment_en;
	req->ipv6_tcp_en = rss_config->rss_tuple_sets.ipv6_tcp_en;
	req->ipv6_udp_en = rss_config->rss_tuple_sets.ipv6_udp_en;
	req->ipv6_sctp_en = rss_config->rss_tuple_sets.ipv6_sctp_en;
	req->ipv6_fragment_en = rss_config->rss_tuple_sets.ipv6_fragment_en;

	ret = hns3_cmd_send(hw, &desc_tuple, 1);
	if (ret)
		hns3_err(hw, "Configure RSS input tuple mode failed %d", ret);

	return ret;
}

/*
 * rss_indirection_table command function, opcode:0x0D07.
 * Used to configure the indirection table of rss.
 */
int
hns3_set_rss_indir_table(struct hns3_hw *hw, uint8_t *indir, uint16_t size)
{
	struct hns3_rss_indirection_table_cmd *req;
	struct hns3_cmd_desc desc;
	int ret, i, j, num;

	req = (struct hns3_rss_indirection_table_cmd *)desc.data;

	for (i = 0; i < size / HNS3_RSS_CFG_TBL_SIZE; i++) {
		hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RSS_INDIR_TABLE,
					  false);
		req->start_table_index =
				rte_cpu_to_le_16(i * HNS3_RSS_CFG_TBL_SIZE);
		req->rss_set_bitmap = rte_cpu_to_le_16(HNS3_RSS_SET_BITMAP_MSK);
		for (j = 0; j < HNS3_RSS_CFG_TBL_SIZE; j++) {
			num = i * HNS3_RSS_CFG_TBL_SIZE + j;
			req->rss_result[j] = indir[num];
		}
		ret = hns3_cmd_send(hw, &desc, 1);
		if (ret) {
			hns3_err(hw,
				 "Sets RSS indirection table failed %d size %u",
				 ret, size);
			return ret;
		}
	}

	/* Update redirection table of hw */
	memcpy(hw->rss_info.rss_indirection_tbl, indir,	HNS3_RSS_IND_TBL_SIZE);

	return 0;
}

int
hns3_rss_reset_indir_table(struct hns3_hw *hw)
{
	uint8_t *lut;
	int ret;

	lut = rte_zmalloc("hns3_rss_lut", HNS3_RSS_IND_TBL_SIZE, 0);
	if (lut == NULL) {
		hns3_err(hw, "No hns3_rss_lut memory can be allocated");
		return -ENOMEM;
	}

	ret = hns3_set_rss_indir_table(hw, lut, HNS3_RSS_IND_TBL_SIZE);
	if (ret)
		hns3_err(hw, "RSS uninit indir table failed: %d", ret);
	rte_free(lut);

	return ret;
}

int
hns3_set_rss_tuple_by_rss_hf(struct hns3_hw *hw,
			     struct hns3_rss_tuple_cfg *tuple, uint64_t rss_hf)
{
	struct hns3_rss_input_tuple_cmd *req;
	struct hns3_cmd_desc desc;
	uint32_t i;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RSS_INPUT_TUPLE, false);

	req = (struct hns3_rss_input_tuple_cmd *)desc.data;

	/* Enable ipv4 or ipv6 tuple by flow type */
	for (i = 0; i < RTE_ETH_FLOW_MAX; i++) {
		switch (rss_hf & (1ULL << i)) {
		case ETH_RSS_NONFRAG_IPV4_TCP:
			req->ipv4_tcp_en = HNS3_RSS_INPUT_TUPLE_OTHER;
			break;
		case ETH_RSS_NONFRAG_IPV4_UDP:
			req->ipv4_udp_en = HNS3_RSS_INPUT_TUPLE_OTHER;
			break;
		case ETH_RSS_NONFRAG_IPV4_SCTP:
			req->ipv4_sctp_en = HNS3_RSS_INPUT_TUPLE_SCTP;
			break;
		case ETH_RSS_FRAG_IPV4:
			req->ipv4_fragment_en |= HNS3_IP_FRAG_BIT_MASK;
			break;
		case ETH_RSS_NONFRAG_IPV4_OTHER:
			req->ipv4_fragment_en |= HNS3_IP_OTHER_BIT_MASK;
			break;
		case ETH_RSS_NONFRAG_IPV6_TCP:
			req->ipv6_tcp_en = HNS3_RSS_INPUT_TUPLE_OTHER;
			break;
		case ETH_RSS_NONFRAG_IPV6_UDP:
			req->ipv6_udp_en = HNS3_RSS_INPUT_TUPLE_OTHER;
			break;
		case ETH_RSS_NONFRAG_IPV6_SCTP:
			req->ipv6_sctp_en = HNS3_RSS_INPUT_TUPLE_SCTP;
			break;
		case ETH_RSS_FRAG_IPV6:
			req->ipv6_fragment_en |= HNS3_IP_FRAG_BIT_MASK;
			break;
		case ETH_RSS_NONFRAG_IPV6_OTHER:
			req->ipv6_fragment_en |= HNS3_IP_OTHER_BIT_MASK;
			break;
		default:
			/*
			 * rss_hf doesn't include unsupported flow types
			 * because the API framework has checked it, and
			 * this branch will never go unless rss_hf is zero.
			 */
			break;
		}
	}

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "Update RSS flow types tuples failed %d", ret);
		return ret;
	}

	tuple->ipv4_tcp_en = req->ipv4_tcp_en;
	tuple->ipv4_udp_en = req->ipv4_udp_en;
	tuple->ipv4_sctp_en = req->ipv4_sctp_en;
	tuple->ipv4_fragment_en = req->ipv4_fragment_en;
	tuple->ipv6_tcp_en = req->ipv6_tcp_en;
	tuple->ipv6_udp_en = req->ipv6_udp_en;
	tuple->ipv6_sctp_en = req->ipv6_sctp_en;
	tuple->ipv6_fragment_en = req->ipv6_fragment_en;

	return 0;
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
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_rss_tuple_cfg *tuple = &hw->rss_info.rss_tuple_sets;
	struct hns3_rss_conf *rss_cfg = &hw->rss_info;
	uint8_t key_len = rss_conf->rss_key_len;
	uint8_t algo;
	uint64_t rss_hf = rss_conf->rss_hf;
	uint8_t *key = rss_conf->rss_key;
	int ret;

	if (hw->rss_dis_flag)
		return -EINVAL;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_set_rss_tuple_by_rss_hf(hw, tuple, rss_hf);
	if (ret)
		goto conf_err;

	if (rss_cfg->conf.types && rss_hf == 0) {
		/* Disable RSS, reset indirection table by local variable */
		ret = hns3_rss_reset_indir_table(hw);
		if (ret)
			goto conf_err;
	} else if (rss_hf && rss_cfg->conf.types == 0) {
		/* Enable RSS, restore indirection table by hw's config */
		ret = hns3_set_rss_indir_table(hw, rss_cfg->rss_indirection_tbl,
					       HNS3_RSS_IND_TBL_SIZE);
		if (ret)
			goto conf_err;
	}

	/* Update supported flow types when set tuple success */
	rss_cfg->conf.types = rss_hf;

	if (key) {
		if (key_len != HNS3_RSS_KEY_SIZE) {
			hns3_err(hw, "The hash key len(%u) is invalid",
				 key_len);
			ret = -EINVAL;
			goto conf_err;
		}
		algo = rss_cfg->conf.func == RTE_ETH_HASH_FUNCTION_SIMPLE_XOR ?
			HNS3_RSS_HASH_ALGO_SIMPLE : HNS3_RSS_HASH_ALGO_TOEPLITZ;
		ret = hns3_set_rss_algo_key(hw, algo, key);
		if (ret)
			goto conf_err;
	}
	rte_spinlock_unlock(&hw->lock);

	return 0;

conf_err:
	rte_spinlock_unlock(&hw->lock);
	return ret;
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
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_rss_conf *rss_cfg = &hw->rss_info;

	rte_spinlock_lock(&hw->lock);
	rss_conf->rss_hf = rss_cfg->conf.types;

	/* Get the RSS Key required by the user */
	if (rss_conf->rss_key && rss_conf->rss_key_len >= HNS3_RSS_KEY_SIZE) {
		memcpy(rss_conf->rss_key, rss_cfg->key, HNS3_RSS_KEY_SIZE);
		rss_conf->rss_key_len = HNS3_RSS_KEY_SIZE;
	}
	rte_spinlock_unlock(&hw->lock);

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
	uint16_t i, indir_size = HNS3_RSS_IND_TBL_SIZE; /* Table size is 512 */
	uint8_t indirection_tbl[HNS3_RSS_IND_TBL_SIZE];
	uint16_t idx, shift, allow_rss_queues;
	int ret;

	if (reta_size != indir_size || reta_size > ETH_RSS_RETA_SIZE_512) {
		hns3_err(hw, "The size of hash lookup table configured (%u)"
			 "doesn't match the number hardware can supported"
			 "(%u)", reta_size, indir_size);
		return -EINVAL;
	}
	rte_spinlock_lock(&hw->lock);
	memcpy(indirection_tbl, rss_cfg->rss_indirection_tbl,
		HNS3_RSS_IND_TBL_SIZE);
	allow_rss_queues = RTE_MIN(dev->data->nb_rx_queues, hw->rss_size_max);
	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		if (reta_conf[idx].reta[shift] >= allow_rss_queues) {
			rte_spinlock_unlock(&hw->lock);
			hns3_err(hw, "Invalid queue id(%u) to be set in "
				 "redirection table, max number of rss "
				 "queues: %u", reta_conf[idx].reta[shift],
				 allow_rss_queues);
			return -EINVAL;
		}

		if (reta_conf[idx].mask & (1ULL << shift))
			indirection_tbl[i] = reta_conf[idx].reta[shift];
	}

	ret = hns3_set_rss_indir_table(hw, indirection_tbl,
				       HNS3_RSS_IND_TBL_SIZE);

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
	struct hns3_hw *hw = &hns->hw;
	struct hns3_rss_conf *rss_cfg = &hw->rss_info;
	uint16_t i, indir_size = HNS3_RSS_IND_TBL_SIZE; /* Table size is 512 */
	uint16_t idx, shift;

	if (reta_size != indir_size || reta_size > ETH_RSS_RETA_SIZE_512) {
		hns3_err(hw, "The size of hash lookup table configured (%u)"
			 " doesn't match the number hardware can supported"
			 "(%u)", reta_size, indir_size);
		return -EINVAL;
	}
	rte_spinlock_lock(&hw->lock);
	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			reta_conf[idx].reta[shift] =
						rss_cfg->rss_indirection_tbl[i];
	}
	rte_spinlock_unlock(&hw->lock);
	return 0;
}

/*
 * Used to configure the tc_size and tc_offset.
 */
static int
hns3_set_rss_tc_mode(struct hns3_hw *hw)
{
	uint16_t rss_size = hw->alloc_rss_size;
	struct hns3_rss_tc_mode_cmd *req;
	uint16_t tc_offset[HNS3_MAX_TC_NUM];
	uint8_t tc_valid[HNS3_MAX_TC_NUM];
	uint16_t tc_size[HNS3_MAX_TC_NUM];
	struct hns3_cmd_desc desc;
	uint16_t roundup_size;
	uint16_t i;
	int ret;

	req = (struct hns3_rss_tc_mode_cmd *)desc.data;

	roundup_size = roundup_pow_of_two(rss_size);
	roundup_size = ilog2(roundup_size);

	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		tc_valid[i] = !!(hw->hw_tc_map & BIT(i));
		tc_size[i] = roundup_size;
		tc_offset[i] = rss_size * i;
	}

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RSS_TC_MODE, false);
	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		uint16_t mode = 0;

		hns3_set_bit(mode, HNS3_RSS_TC_VALID_B, (tc_valid[i] & 0x1));
		hns3_set_field(mode, HNS3_RSS_TC_SIZE_M, HNS3_RSS_TC_SIZE_S,
			       tc_size[i]);
		hns3_set_field(mode, HNS3_RSS_TC_OFFSET_M, HNS3_RSS_TC_OFFSET_S,
			       tc_offset[i]);

		req->rss_tc_mode[i] = rte_cpu_to_le_16(mode);
	}
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "Sets rss tc mode failed %d", ret);

	return ret;
}

static void
hns3_rss_tuple_uninit(struct hns3_hw *hw)
{
	struct hns3_rss_input_tuple_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RSS_INPUT_TUPLE, false);

	req = (struct hns3_rss_input_tuple_cmd *)desc.data;

	memset(req, 0, sizeof(struct hns3_rss_tuple_cfg));

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
hns3_set_default_rss_args(struct hns3_hw *hw)
{
	struct hns3_rss_conf *rss_cfg = &hw->rss_info;
	uint16_t queue_num = hw->alloc_rss_size;
	int i;

	/* Default hash algorithm */
	rss_cfg->conf.func = RTE_ETH_HASH_FUNCTION_TOEPLITZ;

	/* Default RSS key */
	memcpy(rss_cfg->key, hns3_hash_key, HNS3_RSS_KEY_SIZE);

	/* Initialize RSS indirection table */
	for (i = 0; i < HNS3_RSS_IND_TBL_SIZE; i++)
		rss_cfg->rss_indirection_tbl[i] = i % queue_num;
}

/*
 * RSS initialization for hns3 pmd driver.
 */
int
hns3_config_rss(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_rss_conf *rss_cfg = &hw->rss_info;
	uint8_t hash_algo =
		(hw->rss_info.conf.func == RTE_ETH_HASH_FUNCTION_TOEPLITZ ?
		 HNS3_RSS_HASH_ALGO_TOEPLITZ : HNS3_RSS_HASH_ALGO_SIMPLE);
	uint8_t *hash_key = rss_cfg->key;
	int ret, ret1;

	enum rte_eth_rx_mq_mode mq_mode = hw->data->dev_conf.rxmode.mq_mode;

	/* When RSS is off, redirect the packet queue 0 */
	if (((uint32_t)mq_mode & ETH_MQ_RX_RSS_FLAG) == 0)
		hns3_rss_uninit(hns);

	/* Configure RSS hash algorithm and hash key offset */
	ret = hns3_set_rss_algo_key(hw, hash_algo, hash_key);
	if (ret)
		return ret;

	/* Configure the tuple selection for RSS hash input */
	ret = hns3_set_rss_input_tuple(hw);
	if (ret)
		return ret;

	/*
	 * When RSS is off, it doesn't need to configure rss redirection table
	 * to hardware.
	 */
	if (((uint32_t)mq_mode & ETH_MQ_RX_RSS_FLAG)) {
		ret = hns3_set_rss_indir_table(hw, rss_cfg->rss_indirection_tbl,
					       HNS3_RSS_IND_TBL_SIZE);
		if (ret)
			goto rss_tuple_uninit;
	}

	ret = hns3_set_rss_tc_mode(hw);
	if (ret)
		goto rss_indir_table_uninit;

	return ret;

rss_indir_table_uninit:
	if (((uint32_t)mq_mode & ETH_MQ_RX_RSS_FLAG)) {
		ret1 = hns3_rss_reset_indir_table(hw);
		if (ret1 != 0)
			return ret;
	}

rss_tuple_uninit:
	hns3_rss_tuple_uninit(hw);

	/* Disable RSS */
	hw->rss_info.conf.types = 0;

	return ret;
}

/*
 * RSS uninitialization for hns3 pmd driver.
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
	hw->rss_info.conf.types = 0;
}
