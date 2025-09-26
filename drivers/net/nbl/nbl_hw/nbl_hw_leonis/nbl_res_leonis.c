/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#include "nbl_res_leonis.h"

static struct nbl_resource_ops nbl_res_ops = {
};

static bool is_ops_inited;

static void nbl_res_remove_ops(struct nbl_resource_ops_tbl **res_ops_tbl)
{
	free(*res_ops_tbl);
	*res_ops_tbl = NULL;
}

static int nbl_res_setup_ops(struct nbl_resource_ops_tbl **res_ops_tbl,
			     struct nbl_resource_mgt_leonis *res_mgt_leonis)
{
	int ret = 0;

	*res_ops_tbl = calloc(1, sizeof(struct nbl_resource_ops_tbl));
	if (!*res_ops_tbl)
		return -ENOMEM;

	if (!is_ops_inited) {
		ret = nbl_txrx_setup_ops(&nbl_res_ops);
		if (ret)
			goto set_ops_failed;

		is_ops_inited = true;
	}

	NBL_RES_OPS_TBL_TO_OPS(*res_ops_tbl) = &nbl_res_ops;
	NBL_RES_OPS_TBL_TO_PRIV(*res_ops_tbl) = res_mgt_leonis;

	return ret;

set_ops_failed:
	rte_free(*res_ops_tbl);
	return ret;
}

static void nbl_res_stop(struct nbl_resource_mgt_leonis *res_mgt_leonis)
{
	struct nbl_resource_mgt *res_mgt = &res_mgt_leonis->res_mgt;
	nbl_txrx_mgt_stop(res_mgt);
}

static int nbl_res_start(struct nbl_resource_mgt_leonis *res_mgt_leonis)
{
	struct nbl_resource_mgt *res_mgt = &res_mgt_leonis->res_mgt;
	int ret;

	res_mgt->res_info.base_qid = 0;

	ret = nbl_txrx_mgt_start(res_mgt);
	if (ret)
		goto txrx_failed;

	return 0;

txrx_failed:
	return ret;
}

static void
nbl_res_remove_res_mgt(struct nbl_resource_mgt_leonis **res_mgt_leonis)
{
	rte_free(*res_mgt_leonis);
	*res_mgt_leonis = NULL;
}

static int
nbl_res_setup_res_mgt(struct nbl_resource_mgt_leonis **res_mgt_leonis)
{
	*res_mgt_leonis = rte_zmalloc("nbl_res_mgt",
				      sizeof(struct nbl_resource_mgt_leonis),
				      0);
	if (!*res_mgt_leonis)
		return -ENOMEM;

	return 0;
}

int nbl_res_init_leonis(void *p, struct rte_eth_dev *eth_dev)
{
	struct nbl_resource_mgt_leonis **res_mgt_leonis;
	struct nbl_resource_ops_tbl **res_ops_tbl;
	struct nbl_hw_ops_tbl *hw_ops_tbl;
	struct nbl_channel_ops_tbl *chan_ops_tbl;
	struct nbl_adapter *adapter = (struct nbl_adapter *)p;
	int ret = 0;

	res_mgt_leonis = (struct nbl_resource_mgt_leonis **)&NBL_ADAPTER_TO_RES_MGT(adapter);
	res_ops_tbl = &NBL_ADAPTER_TO_RES_OPS_TBL(adapter);
	hw_ops_tbl = NBL_ADAPTER_TO_HW_OPS_TBL(adapter);
	chan_ops_tbl = NBL_ADAPTER_TO_CHAN_OPS_TBL(adapter);

	ret = nbl_res_setup_res_mgt(res_mgt_leonis);
	if (ret)
		goto setup_mgt_fail;

	NBL_RES_MGT_TO_CHAN_OPS_TBL(&(*res_mgt_leonis)->res_mgt) = chan_ops_tbl;
	NBL_RES_MGT_TO_HW_OPS_TBL(&(*res_mgt_leonis)->res_mgt) = hw_ops_tbl;
	NBL_RES_MGT_TO_ETH_DEV(&(*res_mgt_leonis)->res_mgt) = eth_dev;
	NBL_RES_MGT_TO_COMMON(&(*res_mgt_leonis)->res_mgt) = &adapter->common;

	ret = nbl_res_start(*res_mgt_leonis);
	if (ret)
		goto start_fail;

	ret = nbl_res_setup_ops(res_ops_tbl, *res_mgt_leonis);
	if (ret)
		goto setup_ops_fail;

	return 0;

setup_ops_fail:
	nbl_res_stop(*res_mgt_leonis);
start_fail:
	nbl_res_remove_res_mgt(res_mgt_leonis);
setup_mgt_fail:
	return ret;
}

void nbl_res_remove_leonis(void *p)
{
	struct nbl_resource_mgt_leonis **res_mgt_leonis;
	struct nbl_resource_ops_tbl **res_ops_tbl;
	struct nbl_adapter *adapter = (struct nbl_adapter *)p;

	res_mgt_leonis = (struct nbl_resource_mgt_leonis **)&NBL_ADAPTER_TO_RES_MGT(adapter);
	res_ops_tbl = &NBL_ADAPTER_TO_RES_OPS_TBL(adapter);

	nbl_res_remove_ops(res_ops_tbl);
	nbl_res_stop(*res_mgt_leonis);
	nbl_res_remove_res_mgt(res_mgt_leonis);
}
