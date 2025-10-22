/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#include "nbl_res_leonis.h"

/* store statistics names */
struct nbl_xstats_name {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
};

static const struct nbl_xstats_name nbl_stats_strings[] = {
	{"eth_frames_tx"},
	{"eth_frames_tx_ok"},
	{"eth_frames_tx_badfcs"},
	{"eth_unicast_frames_tx_ok"},
	{"eth_multicast_frames_tx_ok"},
	{"eth_broadcast_frames_tx_ok"},
	{"eth_macctrl_frames_tx_ok"},
	{"eth_fragment_frames_tx"},
	{"eth_fragment_frames_tx_ok"},
	{"eth_pause_frames_tx"},
	{"eth_pause_macctrl_frames_tx"},
	{"eth_pfc_frames_tx"},
	{"eth_pfc_frames_tx_prio0"},
	{"eth_pfc_frames_tx_prio1"},
	{"eth_pfc_frames_tx_prio2"},
	{"eth_pfc_frames_tx_prio3"},
	{"eth_pfc_frames_tx_prio4"},
	{"eth_pfc_frames_tx_prio5"},
	{"eth_pfc_frames_tx_prio6"},
	{"eth_pfc_frames_tx_prio7"},
	{"eth_verify_frames_tx"},
	{"eth_respond_frames_tx"},
	{"eth_frames_tx_64B"},
	{"eth_frames_tx_65_to_127B"},
	{"eth_frames_tx_128_to_255B"},
	{"eth_frames_tx_256_to_511B"},
	{"eth_frames_tx_512_to_1023B"},
	{"eth_frames_tx_1024_to_1535B"},
	{"eth_frames_tx_1536_to_2047B"},
	{"eth_frames_tx_2048_to_MAXB"},
	{"eth_undersize_frames_tx_goodfcs"},
	{"eth_oversize_frames_tx_goodfcs"},
	{"eth_undersize_frames_tx_badfcs"},
	{"eth_oversize_frames_tx_badfcs"},
	{"eth_octets_tx"},
	{"eth_octets_tx_ok"},
	{"eth_octets_tx_badfcs"},
	{"eth_frames_rx"},
	{"eth_frames_rx_ok"},
	{"eth_frames_rx_badfcs"},
	{"eth_undersize_frames_rx_goodfcs"},
	{"eth_undersize_frames_rx_badfcs"},
	{"eth_oversize_frames_rx_goodfcs"},
	{"eth_oversize_frames_rx_badfcs"},
	{"eth_frames_rx_misc_error"},
	{"eth_frames_rx_misc_dropped"},
	{"eth_unicast_frames_rx_ok"},
	{"eth_multicast_frames_rx_ok"},
	{"eth_broadcast_frames_rx_ok"},
	{"eth_pause_frames_rx"},
	{"eth_pfc_frames_rx"},
	{"eth_pfc_frames_rx_prio0"},
	{"eth_pfc_frames_rx_prio1"},
	{"eth_pfc_frames_rx_prio2"},
	{"eth_pfc_frames_rx_prio3"},
	{"eth_pfc_frames_rx_prio4"},
	{"eth_pfc_frames_rx_prio5"},
	{"eth_pfc_frames_rx_prio6"},
	{"eth_pfc_frames_rx_prio7"},
	{"eth_macctrl_frames_rx"},
	{"eth_verify_frames_rx_ok"},
	{"eth_respond_frames_rx_ok"},
	{"eth_fragment_frames_rx_ok"},
	{"eth_fragment_rx_smdc_nocontext"},
	{"eth_fragment_rx_smds_seq_error"},
	{"eth_fragment_rx_smdc_seq_error"},
	{"eth_fragment_rx_frag_cnt_error"},
	{"eth_frames_assembled_ok"},
	{"eth_frames_assembled_error"},
	{"eth_frames_rx_64B"},
	{"eth_frames_rx_65_to_127B"},
	{"eth_frames_rx_128_to_255B"},
	{"eth_frames_rx_256_to_511B"},
	{"eth_frames_rx_512_to_1023B"},
	{"eth_frames_rx_1024_to_1535B"},
	{"eth_frames_rx_1536_to_2047B"},
	{"eth_frames_rx_2048_to_MAXB"},
	{"eth_octets_rx"},
	{"eth_octets_rx_ok"},
	{"eth_octets_rx_badfcs"},
	{"eth_octets_rx_dropped"},
};

static int nbl_get_xstats_cnt(__rte_unused void *priv, u16 *xstats_cnt)
{
	*xstats_cnt = ARRAY_SIZE(nbl_stats_strings);
	return 0;
}

static int nbl_get_xstats_names(__rte_unused void *priv,
				struct rte_eth_xstat_name *xstats_names,
				u16 need_xstats_cnt, u16 *xstats_cnt)
{
	u32 i = 0;
	u16 count = *xstats_cnt;

	for (i = 0; i < need_xstats_cnt; i++) {
		strlcpy(xstats_names[count].name, nbl_stats_strings[i].name,
			sizeof(nbl_stats_strings[count].name));
		count++;
	}
	*xstats_cnt = count;
	return 0;
}

static struct nbl_resource_ops nbl_res_ops = {
	.get_hw_xstats_cnt = nbl_get_xstats_cnt,
	.get_hw_xstats_names = nbl_get_xstats_names,
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
