/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell International Ltd.
 */

#include "cnxk_ethdev.h"

#define CNXK_NIX_CMAN_RED_MIN_THRESH 75
#define CNXK_NIX_CMAN_RED_MAX_THRESH 95

int
cnxk_nix_cman_info_get(struct rte_eth_dev *dev, struct rte_eth_cman_info *info)
{
	RTE_SET_USED(dev);

	info->modes_supported = RTE_CMAN_RED;
	info->objs_supported = RTE_ETH_CMAN_OBJ_RX_QUEUE | RTE_ETH_CMAN_OBJ_RX_QUEUE_MEMPOOL;

	return 0;
}

int
cnxk_nix_cman_config_init(struct rte_eth_dev *dev, struct rte_eth_cman_config *config)
{
	RTE_SET_USED(dev);

	memset(config, 0, sizeof(struct rte_eth_cman_config));

	config->obj = RTE_ETH_CMAN_OBJ_RX_QUEUE;
	config->mode = RTE_CMAN_RED;
	config->mode_param.red.min_th = CNXK_NIX_CMAN_RED_MIN_THRESH;
	config->mode_param.red.max_th = CNXK_NIX_CMAN_RED_MAX_THRESH;
	return 0;
}

static int
nix_cman_config_validate(struct rte_eth_dev *eth_dev, const struct rte_eth_cman_config *config)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct rte_eth_cman_info info;

	memset(&info, 0, sizeof(struct rte_eth_cman_info));
	cnxk_nix_cman_info_get(eth_dev, &info);

	if (!(config->obj & info.objs_supported)) {
		plt_err("Invalid object");
		return -EINVAL;
	}

	if (!(config->mode & info.modes_supported)) {
		plt_err("Invalid mode");
		return -EINVAL;
	}

	if (config->obj_param.rx_queue >= dev->nb_rxq) {
		plt_err("Invalid queue ID. Queue = %u", config->obj_param.rx_queue);
		return -EINVAL;
	}

	if (config->mode_param.red.min_th > CNXK_NIX_CMAN_RED_MAX_THRESH) {
		plt_err("Invalid RED minimum threshold. min_th = %u",
			config->mode_param.red.min_th);
		return -EINVAL;
	}

	if (config->mode_param.red.max_th > CNXK_NIX_CMAN_RED_MAX_THRESH) {
		plt_err("Invalid RED maximum threshold. max_th = %u",
			config->mode_param.red.max_th);
		return -EINVAL;
	}

	if (config->mode_param.red.min_th > config->mode_param.red.max_th) {
		plt_err("RED minimum threshold must be less or equal to maximum threshold");
		return -EINVAL;
	}

	return 0;
}

int
cnxk_nix_cman_config_set(struct rte_eth_dev *eth_dev, const struct rte_eth_cman_config *config)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	uint8_t drop, pass, shift;
	uint8_t min_th, max_th;
	struct roc_nix_cq *cq;
	struct roc_nix_rq *rq;
	bool is_mempool;
	uint64_t buf_cnt;
	int rc;

	rc = nix_cman_config_validate(eth_dev, config);
	if (rc)
		return rc;

	cq = &dev->cqs[config->obj_param.rx_queue];
	rq = &dev->rqs[config->obj_param.rx_queue];
	is_mempool = config->obj & RTE_ETH_CMAN_OBJ_RX_QUEUE_MEMPOOL ? true : false;
	min_th = config->mode_param.red.min_th;
	max_th = config->mode_param.red.max_th;

	if (is_mempool) {
		buf_cnt = roc_npa_aura_op_limit_get(rq->aura_handle);
		shift = plt_log2_u32(buf_cnt);
		shift = shift < 8 ? 0 : shift - 8;
		pass = (buf_cnt >> shift) - ((buf_cnt * min_th / 100) >> shift);
		drop = (buf_cnt >> shift) - ((buf_cnt * max_th / 100) >> shift);
		rq->red_pass = pass;
		rq->red_drop = drop;

		if (rq->spb_ena) {
			buf_cnt = roc_npa_aura_op_limit_get(rq->spb_aura_handle);
			shift = plt_log2_u32(buf_cnt);
			shift = shift < 8 ? 0 : shift - 8;
			pass = (buf_cnt >> shift) - ((buf_cnt * min_th / 100) >> shift);
			drop = (buf_cnt >> shift) - ((buf_cnt * max_th / 100) >> shift);
			rq->spb_red_pass = pass;
			rq->spb_red_drop = drop;
		}
	} else {
		shift = plt_log2_u32(cq->nb_desc);
		shift = shift < 8 ? 0 : shift - 8;
		pass = 256 - ((cq->nb_desc * min_th / 100) >> shift);
		drop = 256 - ((cq->nb_desc * max_th / 100) >> shift);

		rq->xqe_red_pass = pass;
		rq->xqe_red_drop = drop;
	}

	rc = roc_nix_rq_cman_config(nix, rq);
	if (rc)
		return rc;

	memcpy(&dev->cman_cfg, config, sizeof(struct rte_eth_cman_config));
	return 0;
}

int
cnxk_nix_cman_config_get(struct rte_eth_dev *eth_dev, struct rte_eth_cman_config *config)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);

	memcpy(config, &dev->cman_cfg, sizeof(struct rte_eth_cman_config));
	return 0;
}
