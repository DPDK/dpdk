/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "base/i40e_prototype.h"
#include "i40e_logs.h"
#include "i40e_ethdev.h"
#include "i40e_hash.h"

#include "../common/flow_check.h"
const uint8_t i40e_rss_key_default[] = {
	0x44, 0x39, 0x79, 0x6b,
	0xb5, 0x4c, 0x50, 0x23,
	0xb6, 0x75, 0xea, 0x5b,
	0x12, 0x4f, 0x9f, 0x30,
	0xb8, 0xa2, 0xc0, 0x3d,
	0xdf, 0xdc, 0x4d, 0x02,
	0xa0, 0x8c, 0x9b, 0x33,
	0x4a, 0xf6, 0x4a, 0x4c,
	0x05, 0xc6, 0xfa, 0x34,
	0x39, 0x58, 0xd8, 0x55,
	0x7d, 0x99, 0x58, 0x3a,
	0xe1, 0x38, 0xc9, 0x2e,
	0x81, 0x15, 0x03, 0x66
};

static int
i40e_hash_config_func(struct i40e_hw *hw, enum rte_eth_hash_function func)
{
	struct i40e_pf *pf;
	uint32_t reg;
	uint8_t symmetric = 0;

	reg = i40e_read_rx_ctl(hw, I40E_GLQF_CTL);

	if (func == RTE_ETH_HASH_FUNCTION_SIMPLE_XOR) {
		if (!(reg & I40E_GLQF_CTL_HTOEP_MASK))
			goto set_symmetric;

		reg &= ~I40E_GLQF_CTL_HTOEP_MASK;
	} else {
		if (func == RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ)
			symmetric = 1;

		if (reg & I40E_GLQF_CTL_HTOEP_MASK)
			goto set_symmetric;

		reg |= I40E_GLQF_CTL_HTOEP_MASK;
	}

	pf = &((struct i40e_adapter *)hw->back)->pf;
	if (pf->support_multi_driver) {
		PMD_DRV_LOG(ERR,
			    "Modify hash function is not permitted when multi-driver enabled");
		return -EPERM;
	}

	PMD_DRV_LOG(INFO, "NIC hash function is setting to %d", func);
	i40e_write_rx_ctl(hw, I40E_GLQF_CTL, reg);
	I40E_WRITE_FLUSH(hw);

set_symmetric:
	i40e_set_symmetric_hash_enable_per_port(hw, symmetric);
	return 0;
}

static int
i40e_hash_config_pctype_symmetric(struct i40e_hw *hw,
				  uint32_t pctype,
				  bool symmetric)
{
	struct i40e_pf *pf = &((struct i40e_adapter *)hw->back)->pf;
	uint32_t reg;

	reg = i40e_read_rx_ctl(hw, I40E_GLQF_HSYM(pctype));
	if (symmetric) {
		if (reg & I40E_GLQF_HSYM_SYMH_ENA_MASK)
			return 0;
		reg |= I40E_GLQF_HSYM_SYMH_ENA_MASK;
	} else {
		if (!(reg & I40E_GLQF_HSYM_SYMH_ENA_MASK))
			return 0;
		reg &= ~I40E_GLQF_HSYM_SYMH_ENA_MASK;
	}

	if (pf->support_multi_driver) {
		PMD_DRV_LOG(ERR,
			    "Enable/Disable symmetric hash is not permitted when multi-driver enabled");
		return -EPERM;
	}

	i40e_write_rx_ctl(hw, I40E_GLQF_HSYM(pctype), reg);
	I40E_WRITE_FLUSH(hw);
	return 0;
}

static void
i40e_hash_enable_pctype(struct i40e_hw *hw,
			uint32_t pctype, bool enable)
{
	uint32_t reg, reg_val, mask;

	if (pctype < 32) {
		mask = BIT(pctype);
		reg = I40E_PFQF_HENA(0);
	} else {
		mask = BIT(pctype - 32);
		reg = I40E_PFQF_HENA(1);
	}

	reg_val = i40e_read_rx_ctl(hw, reg);

	if (enable) {
		if (reg_val & mask)
			return;

		reg_val |= mask;
	} else {
		if (!(reg_val & mask))
			return;

		reg_val &= ~mask;
	}

	i40e_write_rx_ctl(hw, reg, reg_val);
	I40E_WRITE_FLUSH(hw);
}

static int
i40e_hash_config_pctype(struct i40e_hw *hw,
			struct i40e_rss_conf *rss_conf,
			uint32_t pctype)
{
	uint64_t rss_types = rss_conf->types;
	int ret;

	if (rss_types == 0) {
		i40e_hash_enable_pctype(hw, pctype, false);
		return 0;
	}

	if (rss_conf->inset) {
		ret = i40e_set_hash_inset(hw, rss_conf->inset, pctype, false);
		if (ret)
			return ret;
	}

	i40e_hash_enable_pctype(hw, pctype, true);
	return 0;
}

static int
i40e_hash_config_region(struct i40e_pf *pf,
			const struct i40e_rss_conf *rss_conf)
{
	struct i40e_hw *hw = &pf->adapter->hw;
	struct rte_eth_dev *dev = &rte_eth_devices[pf->dev_data->port_id];
	struct i40e_queue_region_info *regions = pf->queue_region.region;
	uint32_t num = pf->queue_region.queue_region_number;
	uint32_t i, region_id_mask = 0;

	/* Use a 32 bit variable to represent all regions */
	RTE_BUILD_BUG_ON(I40E_REGION_MAX_INDEX > 31);

	/* Re-configure the region if it existed */
	for (i = 0; i < num; i++) {
		if (rss_conf->region_queue_start ==
		    regions[i].queue_start_index &&
		    rss_conf->region_queue_num == regions[i].queue_num) {
			uint32_t j;

			for (j = 0; j < regions[i].user_priority_num; j++) {
				if (regions[i].user_priority[j] ==
				    rss_conf->region_priority)
					return 0;
			}

			if (j >= I40E_MAX_USER_PRIORITY) {
				PMD_DRV_LOG(ERR,
					    "Priority number exceed the maximum %d",
					    I40E_MAX_USER_PRIORITY);
				return -ENOSPC;
			}

			regions[i].user_priority[j] = rss_conf->region_priority;
			regions[i].user_priority_num++;
			return i40e_flush_queue_region_all_conf(dev, hw, pf, 1);
		}

		region_id_mask |= BIT(regions[i].region_id);
	}

	if (num > I40E_REGION_MAX_INDEX) {
		PMD_DRV_LOG(ERR, "Queue region resource used up");
		return -ENOSPC;
	}

	/* Add a new region */

	pf->queue_region.queue_region_number++;
	memset(&regions[num], 0, sizeof(regions[0]));

	regions[num].region_id = rte_bsf32(~region_id_mask);
	regions[num].queue_num = rss_conf->region_queue_num;
	regions[num].queue_start_index = rss_conf->region_queue_start;
	regions[num].user_priority[0] = rss_conf->region_priority;
	regions[num].user_priority_num = 1;

	return i40e_flush_queue_region_all_conf(dev, hw, pf, 1);
}

int
i40e_hash_program_hw(struct i40e_pf *pf,
		     struct i40e_rss_conf *rss_conf)
{
	struct i40e_hw *hw = &pf->adapter->hw;
	uint64_t pctypes;
	int ret;

	if (rss_conf->func != RTE_ETH_HASH_FUNCTION_DEFAULT) {
		ret = i40e_hash_config_func(hw, rss_conf->func);
		if (ret)
			return ret;
	}

	if (rss_conf->region_queue_num > 0) {
		ret = i40e_hash_config_region(pf, rss_conf);
		if (ret)
			return ret;
	}

	if (rss_conf->key_len > 0) {
		ret = i40e_set_rss_key(pf->main_vsi, rss_conf->key,
				       rss_conf->key_len);
		if (ret)
			return ret;
	}

	/* Update lookup table */
	if (rss_conf->queue_num > 0) {
		uint8_t lut[RTE_ETH_RSS_RETA_SIZE_512];
		uint32_t i, j = 0;

		for (i = 0; i < hw->func_caps.rss_table_size; i++) {
			lut[i] = (uint8_t)rss_conf->queue[j];
			j = (j == rss_conf->queue_num - 1) ? 0 : (j + 1);
		}

		ret = i40e_set_rss_lut(pf->main_vsi, lut, (uint16_t)i);
		if (ret)
			return ret;

		pf->hash_enabled_queues = 0;
		for (i = 0; i < rss_conf->queue_num; i++)
			pf->hash_enabled_queues |= BIT_ULL(lut[i]);

		pf->adapter->rss_reta_updated = 0;
	}

	/* The codes behind configure the input sets and symmetric hash
	 * function of the packet types and enable hash on them.
	 */
	pctypes = rss_conf->config_pctypes;
	if (!pctypes)
		return 0;

	/* For first flow that will enable hash on any packet type, we clean
	 * the RSS sets that by legacy configuration commands and parameters.
	 */
	if (!pf->hash_filter_enabled) {
		i40e_pf_disable_rss(pf);
		pf->hash_filter_enabled = true;
	}

	do {
		uint32_t idx = rte_bsf64(pctypes);

		if (rss_conf->symmetric_enable) {
			ret = i40e_hash_config_pctype_symmetric(hw, idx, true);
			if (ret)
				return ret;
		}

		ret = i40e_hash_config_pctype(hw, rss_conf, idx);
		if (ret)
			return ret;

		pctypes &= ~BIT_ULL(idx);
	} while (pctypes);

	return 0;
}

/*
 * Compute the reset flags a filter would own if it were the only RSS filter
 * installed. Pure function of rss_conf; used to reconstruct the ownership of
 * shared RSS state across all currently registered filters.
 */
void
i40e_hash_compute_reset_flags(const struct i40e_rss_conf *rss_conf,
			      struct i40e_rss_filter_data *filter_data)
{
	uint64_t pctypes;

	*filter_data = (struct i40e_rss_filter_data){0};

	if (rss_conf->func != RTE_ETH_HASH_FUNCTION_DEFAULT &&
	    rss_conf->func != RTE_ETH_HASH_FUNCTION_TOEPLITZ)
		filter_data->misc_reset_flags |= I40E_HASH_FLOW_RESET_FLAG_FUNC;

	if (rss_conf->region_queue_num > 0)
		filter_data->misc_reset_flags |= I40E_HASH_FLOW_RESET_FLAG_REGION;

	if (rss_conf->key_len > 0)
		filter_data->misc_reset_flags |= I40E_HASH_FLOW_RESET_FLAG_KEY;

	if (rss_conf->queue_num > 0)
		filter_data->misc_reset_flags |= I40E_HASH_FLOW_RESET_FLAG_QUEUE;

	pctypes = rss_conf->config_pctypes;
	while (pctypes) {
		uint32_t idx = rte_bsf64(pctypes);
		uint64_t bit = BIT_ULL(idx);

		filter_data->reset_config_pctypes |= bit;
		if (rss_conf->symmetric_enable)
			filter_data->reset_symmetric_pctypes |= bit;

		pctypes &= ~bit;
	}
}

/*
 * Revert to defaults only the registers this filter's filter_data records as
 * owned. This is the inverse of i40e_hash_program_hw, but only undoes the
 * owned subset -- it is not a full RSS wipe.
 */
int
i40e_hash_deprogram_hw(struct i40e_pf *pf,
		       struct i40e_rss_filter_data *filter_data)
{
	struct i40e_hw *hw = &pf->adapter->hw;
	struct rte_eth_dev *dev;
	uint64_t inset;
	uint32_t idx;
	int ret;

	if (filter_data->misc_reset_flags & I40E_HASH_FLOW_RESET_FLAG_FUNC) {
		ret = i40e_hash_config_func(hw, RTE_ETH_HASH_FUNCTION_TOEPLITZ);
		if (ret)
			return ret;

		filter_data->misc_reset_flags &= ~I40E_HASH_FLOW_RESET_FLAG_FUNC;
	}

	if (filter_data->misc_reset_flags & I40E_HASH_FLOW_RESET_FLAG_REGION) {
		dev = &rte_eth_devices[pf->dev_data->port_id];
		ret = i40e_flush_queue_region_all_conf(dev, hw, pf, 0);
		if (ret)
			return ret;

		filter_data->misc_reset_flags &= ~I40E_HASH_FLOW_RESET_FLAG_REGION;
	}

	if (filter_data->misc_reset_flags & I40E_HASH_FLOW_RESET_FLAG_KEY) {
		ret = i40e_pf_reset_rss_key(pf);
		if (ret)
			return ret;

		filter_data->misc_reset_flags &= ~I40E_HASH_FLOW_RESET_FLAG_KEY;
	}

	if (filter_data->misc_reset_flags & I40E_HASH_FLOW_RESET_FLAG_QUEUE) {
		if (!pf->adapter->rss_reta_updated) {
			ret = i40e_pf_reset_rss_reta(pf);
			if (ret)
				return ret;
		}

		pf->hash_enabled_queues = 0;
		filter_data->misc_reset_flags &= ~I40E_HASH_FLOW_RESET_FLAG_QUEUE;
	}

	while (filter_data->reset_config_pctypes) {
		idx = rte_bsf64(filter_data->reset_config_pctypes);

		i40e_hash_enable_pctype(hw, idx, false);
		inset = i40e_get_default_input_set(idx);
		if (inset) {
			ret = i40e_set_hash_inset(hw, inset, idx, false);
			if (ret)
				return ret;
		}

		filter_data->reset_config_pctypes &= ~BIT_ULL(idx);
	}

	while (filter_data->reset_symmetric_pctypes) {
		idx = rte_bsf64(filter_data->reset_symmetric_pctypes);

		ret = i40e_hash_config_pctype_symmetric(hw, idx, false);
		if (ret)
			return ret;

		filter_data->reset_symmetric_pctypes &= ~BIT_ULL(idx);
	}

	return 0;
}
