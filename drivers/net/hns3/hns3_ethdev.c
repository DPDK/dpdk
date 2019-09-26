/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2019 Hisilicon Limited.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_dev.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_io.h>
#include <rte_log.h>
#include <rte_pci.h>

#include "hns3_ethdev.h"
#include "hns3_logs.h"
#include "hns3_regs.h"

#define HNS3_DEFAULT_PORT_CONF_BURST_SIZE	32
#define HNS3_DEFAULT_PORT_CONF_QUEUES_NUM	1

int hns3_logtype_init;
int hns3_logtype_driver;

static int
hns3_config_tso(struct hns3_hw *hw, unsigned int tso_mss_min,
		unsigned int tso_mss_max)
{
	struct hns3_cfg_tso_status_cmd *req;
	struct hns3_cmd_desc desc;
	uint16_t tso_mss;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_TSO_GENERIC_CONFIG, false);

	req = (struct hns3_cfg_tso_status_cmd *)desc.data;

	tso_mss = 0;
	hns3_set_field(tso_mss, HNS3_TSO_MSS_MIN_M, HNS3_TSO_MSS_MIN_S,
		       tso_mss_min);
	req->tso_mss_min = rte_cpu_to_le_16(tso_mss);

	tso_mss = 0;
	hns3_set_field(tso_mss, HNS3_TSO_MSS_MIN_M, HNS3_TSO_MSS_MIN_S,
		       tso_mss_max);
	req->tso_mss_max = rte_cpu_to_le_16(tso_mss);

	return hns3_cmd_send(hw, &desc, 1);
}

int
hns3_config_gro(struct hns3_hw *hw, bool en)
{
	struct hns3_cfg_gro_status_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_GRO_GENERIC_CONFIG, false);
	req = (struct hns3_cfg_gro_status_cmd *)desc.data;

	req->gro_en = rte_cpu_to_le_16(en ? 1 : 0);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "GRO hardware config cmd failed, ret = %d", ret);

	return ret;
}

static int
hns3_set_umv_space(struct hns3_hw *hw, uint16_t space_size,
		   uint16_t *allocated_size, bool is_alloc)
{
	struct hns3_umv_spc_alc_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	req = (struct hns3_umv_spc_alc_cmd *)desc.data;
	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_MAC_VLAN_ALLOCATE, false);
	hns3_set_bit(req->allocate, HNS3_UMV_SPC_ALC_B, is_alloc ? 0 : 1);
	req->space_size = rte_cpu_to_le_32(space_size);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		PMD_INIT_LOG(ERR, "%s umv space failed for cmd_send, ret =%d",
			     is_alloc ? "allocate" : "free", ret);
		return ret;
	}

	if (is_alloc && allocated_size)
		*allocated_size = rte_le_to_cpu_32(desc.data[1]);

	return 0;
}

static int
hns3_init_umv_space(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	uint16_t allocated_size = 0;
	int ret;

	ret = hns3_set_umv_space(hw, pf->wanted_umv_size, &allocated_size,
				 true);
	if (ret)
		return ret;

	if (allocated_size < pf->wanted_umv_size)
		PMD_INIT_LOG(WARNING, "Alloc umv space failed, want %u, get %u",
			     pf->wanted_umv_size, allocated_size);

	pf->max_umv_size = (!!allocated_size) ? allocated_size :
						pf->wanted_umv_size;
	pf->used_umv_size = 0;
	return 0;
}

static int
hns3_uninit_umv_space(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	int ret;

	if (pf->max_umv_size == 0)
		return 0;

	ret = hns3_set_umv_space(hw, pf->max_umv_size, NULL, false);
	if (ret)
		return ret;

	pf->max_umv_size = 0;

	return 0;
}

static int
hns3_set_mac_mtu(struct hns3_hw *hw, uint16_t new_mps)
{
	struct hns3_config_max_frm_size_cmd *req;
	struct hns3_cmd_desc desc;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_CONFIG_MAX_FRM_SIZE, false);

	req = (struct hns3_config_max_frm_size_cmd *)desc.data;
	req->max_frm_size = rte_cpu_to_le_16(new_mps);
	req->min_frm_size = HNS3_MIN_FRAME_LEN;

	return hns3_cmd_send(hw, &desc, 1);
}

static int
hns3_config_mtu(struct hns3_hw *hw, uint16_t mps)
{
	int ret;

	ret = hns3_set_mac_mtu(hw, mps);
	if (ret) {
		hns3_err(hw, "Failed to set mtu, ret = %d", ret);
		return ret;
	}

	ret = hns3_buffer_alloc(hw);
	if (ret) {
		hns3_err(hw, "Failed to allocate buffer, ret = %d", ret);
		return ret;
	}

	return 0;
}

static int
hns3_parse_func_status(struct hns3_hw *hw, struct hns3_func_status_cmd *status)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;

	if (!(status->pf_state & HNS3_PF_STATE_DONE))
		return -EINVAL;

	pf->is_main_pf = (status->pf_state & HNS3_PF_STATE_MAIN) ? true : false;

	return 0;
}

static int
hns3_query_function_status(struct hns3_hw *hw)
{
#define HNS3_QUERY_MAX_CNT		10
#define HNS3_QUERY_SLEEP_MSCOEND	1
	struct hns3_func_status_cmd *req;
	struct hns3_cmd_desc desc;
	int timeout = 0;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_QUERY_FUNC_STATUS, true);
	req = (struct hns3_func_status_cmd *)desc.data;

	do {
		ret = hns3_cmd_send(hw, &desc, 1);
		if (ret) {
			PMD_INIT_LOG(ERR, "query function status failed %d",
				     ret);
			return ret;
		}

		/* Check pf reset is done */
		if (req->pf_state)
			break;

		rte_delay_ms(HNS3_QUERY_SLEEP_MSCOEND);
	} while (timeout++ < HNS3_QUERY_MAX_CNT);

	return hns3_parse_func_status(hw, req);
}

static int
hns3_query_pf_resource(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	struct hns3_pf_res_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_QUERY_PF_RSRC, true);
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		PMD_INIT_LOG(ERR, "query pf resource failed %d", ret);
		return ret;
	}

	req = (struct hns3_pf_res_cmd *)desc.data;
	hw->total_tqps_num = rte_le_to_cpu_16(req->tqp_num);
	pf->pkt_buf_size = rte_le_to_cpu_16(req->buf_size) << HNS3_BUF_UNIT_S;
	hw->tqps_num = RTE_MIN(hw->total_tqps_num, HNS3_MAX_TQP_NUM_PER_FUNC);

	if (req->tx_buf_size)
		pf->tx_buf_size =
		    rte_le_to_cpu_16(req->tx_buf_size) << HNS3_BUF_UNIT_S;
	else
		pf->tx_buf_size = HNS3_DEFAULT_TX_BUF;

	pf->tx_buf_size = roundup(pf->tx_buf_size, HNS3_BUF_SIZE_UNIT);

	if (req->dv_buf_size)
		pf->dv_buf_size =
		    rte_le_to_cpu_16(req->dv_buf_size) << HNS3_BUF_UNIT_S;
	else
		pf->dv_buf_size = HNS3_DEFAULT_DV;

	pf->dv_buf_size = roundup(pf->dv_buf_size, HNS3_BUF_SIZE_UNIT);

	hw->num_msi =
	    hns3_get_field(rte_le_to_cpu_16(req->pf_intr_vector_number),
			   HNS3_PF_VEC_NUM_M, HNS3_PF_VEC_NUM_S);

	return 0;
}

static void
hns3_parse_cfg(struct hns3_cfg *cfg, struct hns3_cmd_desc *desc)
{
	struct hns3_cfg_param_cmd *req;
	uint64_t mac_addr_tmp_high;
	uint64_t mac_addr_tmp;
	uint32_t i;

	req = (struct hns3_cfg_param_cmd *)desc[0].data;

	/* get the configuration */
	cfg->vmdq_vport_num = hns3_get_field(rte_le_to_cpu_32(req->param[0]),
					     HNS3_CFG_VMDQ_M, HNS3_CFG_VMDQ_S);
	cfg->tc_num = hns3_get_field(rte_le_to_cpu_32(req->param[0]),
				     HNS3_CFG_TC_NUM_M, HNS3_CFG_TC_NUM_S);
	cfg->tqp_desc_num = hns3_get_field(rte_le_to_cpu_32(req->param[0]),
					   HNS3_CFG_TQP_DESC_N_M,
					   HNS3_CFG_TQP_DESC_N_S);

	cfg->phy_addr = hns3_get_field(rte_le_to_cpu_32(req->param[1]),
				       HNS3_CFG_PHY_ADDR_M,
				       HNS3_CFG_PHY_ADDR_S);
	cfg->media_type = hns3_get_field(rte_le_to_cpu_32(req->param[1]),
					 HNS3_CFG_MEDIA_TP_M,
					 HNS3_CFG_MEDIA_TP_S);
	cfg->rx_buf_len = hns3_get_field(rte_le_to_cpu_32(req->param[1]),
					 HNS3_CFG_RX_BUF_LEN_M,
					 HNS3_CFG_RX_BUF_LEN_S);
	/* get mac address */
	mac_addr_tmp = rte_le_to_cpu_32(req->param[2]);
	mac_addr_tmp_high = hns3_get_field(rte_le_to_cpu_32(req->param[3]),
					   HNS3_CFG_MAC_ADDR_H_M,
					   HNS3_CFG_MAC_ADDR_H_S);

	mac_addr_tmp |= (mac_addr_tmp_high << 31) << 1;

	cfg->default_speed = hns3_get_field(rte_le_to_cpu_32(req->param[3]),
					    HNS3_CFG_DEFAULT_SPEED_M,
					    HNS3_CFG_DEFAULT_SPEED_S);
	cfg->rss_size_max = hns3_get_field(rte_le_to_cpu_32(req->param[3]),
					   HNS3_CFG_RSS_SIZE_M,
					   HNS3_CFG_RSS_SIZE_S);

	for (i = 0; i < RTE_ETHER_ADDR_LEN; i++)
		cfg->mac_addr[i] = (mac_addr_tmp >> (8 * i)) & 0xff;

	req = (struct hns3_cfg_param_cmd *)desc[1].data;
	cfg->numa_node_map = rte_le_to_cpu_32(req->param[0]);

	cfg->speed_ability = hns3_get_field(rte_le_to_cpu_32(req->param[1]),
					    HNS3_CFG_SPEED_ABILITY_M,
					    HNS3_CFG_SPEED_ABILITY_S);
	cfg->umv_space = hns3_get_field(rte_le_to_cpu_32(req->param[1]),
					HNS3_CFG_UMV_TBL_SPACE_M,
					HNS3_CFG_UMV_TBL_SPACE_S);
	if (!cfg->umv_space)
		cfg->umv_space = HNS3_DEFAULT_UMV_SPACE_PER_PF;
}

/* hns3_get_board_cfg: query the static parameter from NCL_config file in flash
 * @hw: pointer to struct hns3_hw
 * @hcfg: the config structure to be getted
 */
static int
hns3_get_board_cfg(struct hns3_hw *hw, struct hns3_cfg *hcfg)
{
	struct hns3_cmd_desc desc[HNS3_PF_CFG_DESC_NUM];
	struct hns3_cfg_param_cmd *req;
	uint32_t offset;
	uint32_t i;
	int ret;

	for (i = 0; i < HNS3_PF_CFG_DESC_NUM; i++) {
		offset = 0;
		req = (struct hns3_cfg_param_cmd *)desc[i].data;
		hns3_cmd_setup_basic_desc(&desc[i], HNS3_OPC_GET_CFG_PARAM,
					  true);
		hns3_set_field(offset, HNS3_CFG_OFFSET_M, HNS3_CFG_OFFSET_S,
			       i * HNS3_CFG_RD_LEN_BYTES);
		/* Len should be divided by 4 when send to hardware */
		hns3_set_field(offset, HNS3_CFG_RD_LEN_M, HNS3_CFG_RD_LEN_S,
			       HNS3_CFG_RD_LEN_BYTES / HNS3_CFG_RD_LEN_UNIT);
		req->offset = rte_cpu_to_le_32(offset);
	}

	ret = hns3_cmd_send(hw, desc, HNS3_PF_CFG_DESC_NUM);
	if (ret) {
		PMD_INIT_LOG(ERR, "get config failed %d.", ret);
		return ret;
	}

	hns3_parse_cfg(hcfg, desc);

	return 0;
}

static int
hns3_parse_speed(int speed_cmd, uint32_t *speed)
{
	switch (speed_cmd) {
	case HNS3_CFG_SPEED_10M:
		*speed = ETH_SPEED_NUM_10M;
		break;
	case HNS3_CFG_SPEED_100M:
		*speed = ETH_SPEED_NUM_100M;
		break;
	case HNS3_CFG_SPEED_1G:
		*speed = ETH_SPEED_NUM_1G;
		break;
	case HNS3_CFG_SPEED_10G:
		*speed = ETH_SPEED_NUM_10G;
		break;
	case HNS3_CFG_SPEED_25G:
		*speed = ETH_SPEED_NUM_25G;
		break;
	case HNS3_CFG_SPEED_40G:
		*speed = ETH_SPEED_NUM_40G;
		break;
	case HNS3_CFG_SPEED_50G:
		*speed = ETH_SPEED_NUM_50G;
		break;
	case HNS3_CFG_SPEED_100G:
		*speed = ETH_SPEED_NUM_100G;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int
hns3_get_board_configuration(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	struct hns3_cfg cfg;
	int ret;

	ret = hns3_get_board_cfg(hw, &cfg);
	if (ret) {
		PMD_INIT_LOG(ERR, "get board config failed %d", ret);
		return ret;
	}

	if (cfg.media_type == HNS3_MEDIA_TYPE_COPPER) {
		PMD_INIT_LOG(ERR, "media type is copper, not supported.");
		return -EOPNOTSUPP;
	}

	hw->mac.media_type = cfg.media_type;
	hw->rss_size_max = cfg.rss_size_max;
	hw->rx_buf_len = cfg.rx_buf_len;
	memcpy(hw->mac.mac_addr, cfg.mac_addr, RTE_ETHER_ADDR_LEN);
	hw->mac.phy_addr = cfg.phy_addr;
	hw->mac.default_addr_setted = false;
	hw->num_tx_desc = cfg.tqp_desc_num;
	hw->num_rx_desc = cfg.tqp_desc_num;
	hw->dcb_info.num_pg = 1;
	hw->dcb_info.hw_pfc_map = 0;

	ret = hns3_parse_speed(cfg.default_speed, &hw->mac.link_speed);
	if (ret) {
		PMD_INIT_LOG(ERR, "Get wrong speed %d, ret = %d",
			     cfg.default_speed, ret);
		return ret;
	}

	pf->tc_max = cfg.tc_num;
	if (pf->tc_max > HNS3_MAX_TC_NUM || pf->tc_max < 1) {
		PMD_INIT_LOG(WARNING,
			     "Get TC num(%u) from flash, set TC num to 1",
			     pf->tc_max);
		pf->tc_max = 1;
	}

	/* Dev does not support DCB */
	if (!hns3_dev_dcb_supported(hw)) {
		pf->tc_max = 1;
		pf->pfc_max = 0;
	} else
		pf->pfc_max = pf->tc_max;

	hw->dcb_info.num_tc = 1;
	hw->alloc_rss_size = RTE_MIN(hw->rss_size_max,
				     hw->tqps_num / hw->dcb_info.num_tc);
	hns3_set_bit(hw->hw_tc_map, 0, 1);
	pf->tx_sch_mode = HNS3_FLAG_TC_BASE_SCH_MODE;

	pf->wanted_umv_size = cfg.umv_space;

	return ret;
}

static int
hns3_get_configuration(struct hns3_hw *hw)
{
	int ret;

	ret = hns3_query_function_status(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to query function status: %d.", ret);
		return ret;
	}

	/* Get pf resource */
	ret = hns3_query_pf_resource(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to query pf resource: %d", ret);
		return ret;
	}

	ret = hns3_get_board_configuration(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to get board configuration: %d", ret);
		return ret;
	}

	return 0;
}

static int
hns3_map_tqps_to_func(struct hns3_hw *hw, uint16_t func_id, uint16_t tqp_pid,
		      uint16_t tqp_vid, bool is_pf)
{
	struct hns3_tqp_map_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_SET_TQP_MAP, false);

	req = (struct hns3_tqp_map_cmd *)desc.data;
	req->tqp_id = rte_cpu_to_le_16(tqp_pid);
	req->tqp_vf = func_id;
	req->tqp_flag = 1 << HNS3_TQP_MAP_EN_B;
	if (!is_pf)
		req->tqp_flag |= (1 << HNS3_TQP_MAP_TYPE_B);
	req->tqp_vid = rte_cpu_to_le_16(tqp_vid);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		PMD_INIT_LOG(ERR, "TQP map failed %d", ret);

	return ret;
}

static int
hns3_map_tqp(struct hns3_hw *hw)
{
	uint16_t tqps_num = hw->total_tqps_num;
	uint16_t func_id;
	uint16_t tqp_id;
	int num;
	int ret;
	int i;

	/*
	 * In current version VF is not supported when PF is driven by DPDK
	 * driver, so we allocate tqps to PF as much as possible.
	 */
	tqp_id = 0;
	num = DIV_ROUND_UP(hw->total_tqps_num, HNS3_MAX_TQP_NUM_PER_FUNC);
	for (func_id = 0; func_id < num; func_id++) {
		for (i = 0;
		     i < HNS3_MAX_TQP_NUM_PER_FUNC && tqp_id < tqps_num; i++) {
			ret = hns3_map_tqps_to_func(hw, func_id, tqp_id++, i,
						    true);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int
hns3_cfg_mac_speed_dup_hw(struct hns3_hw *hw, uint32_t speed, uint8_t duplex)
{
	struct hns3_config_mac_speed_dup_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	req = (struct hns3_config_mac_speed_dup_cmd *)desc.data;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_CONFIG_SPEED_DUP, false);

	hns3_set_bit(req->speed_dup, HNS3_CFG_DUPLEX_B, !!duplex ? 1 : 0);

	switch (speed) {
	case ETH_SPEED_NUM_10M:
		hns3_set_field(req->speed_dup, HNS3_CFG_SPEED_M,
			       HNS3_CFG_SPEED_S, HNS3_CFG_SPEED_10M);
		break;
	case ETH_SPEED_NUM_100M:
		hns3_set_field(req->speed_dup, HNS3_CFG_SPEED_M,
			       HNS3_CFG_SPEED_S, HNS3_CFG_SPEED_100M);
		break;
	case ETH_SPEED_NUM_1G:
		hns3_set_field(req->speed_dup, HNS3_CFG_SPEED_M,
			       HNS3_CFG_SPEED_S, HNS3_CFG_SPEED_1G);
		break;
	case ETH_SPEED_NUM_10G:
		hns3_set_field(req->speed_dup, HNS3_CFG_SPEED_M,
			       HNS3_CFG_SPEED_S, HNS3_CFG_SPEED_10G);
		break;
	case ETH_SPEED_NUM_25G:
		hns3_set_field(req->speed_dup, HNS3_CFG_SPEED_M,
			       HNS3_CFG_SPEED_S, HNS3_CFG_SPEED_25G);
		break;
	case ETH_SPEED_NUM_40G:
		hns3_set_field(req->speed_dup, HNS3_CFG_SPEED_M,
			       HNS3_CFG_SPEED_S, HNS3_CFG_SPEED_40G);
		break;
	case ETH_SPEED_NUM_50G:
		hns3_set_field(req->speed_dup, HNS3_CFG_SPEED_M,
			       HNS3_CFG_SPEED_S, HNS3_CFG_SPEED_50G);
		break;
	case ETH_SPEED_NUM_100G:
		hns3_set_field(req->speed_dup, HNS3_CFG_SPEED_M,
			       HNS3_CFG_SPEED_S, HNS3_CFG_SPEED_100G);
		break;
	default:
		PMD_INIT_LOG(ERR, "invalid speed (%u)", speed);
		return -EINVAL;
	}

	hns3_set_bit(req->mac_change_fec_en, HNS3_CFG_MAC_SPEED_CHANGE_EN_B, 1);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		PMD_INIT_LOG(ERR, "mac speed/duplex config cmd failed %d", ret);

	return ret;
}

static int
hns3_tx_buffer_calc(struct hns3_hw *hw, struct hns3_pkt_buf_alloc *buf_alloc)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	struct hns3_priv_buf *priv;
	uint32_t i, total_size;

	total_size = pf->pkt_buf_size;

	/* alloc tx buffer for all enabled tc */
	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		priv = &buf_alloc->priv_buf[i];

		if (hw->hw_tc_map & BIT(i)) {
			if (total_size < pf->tx_buf_size)
				return -ENOMEM;

			priv->tx_buf_size = pf->tx_buf_size;
		} else
			priv->tx_buf_size = 0;

		total_size -= priv->tx_buf_size;
	}

	return 0;
}

static int
hns3_tx_buffer_alloc(struct hns3_hw *hw, struct hns3_pkt_buf_alloc *buf_alloc)
{
/* TX buffer size is unit by 128 byte */
#define HNS3_BUF_SIZE_UNIT_SHIFT	7
#define HNS3_BUF_SIZE_UPDATE_EN_MSK	BIT(15)
	struct hns3_tx_buff_alloc_cmd *req;
	struct hns3_cmd_desc desc;
	uint32_t buf_size;
	uint32_t i;
	int ret;

	req = (struct hns3_tx_buff_alloc_cmd *)desc.data;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_TX_BUFF_ALLOC, 0);
	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		buf_size = buf_alloc->priv_buf[i].tx_buf_size;

		buf_size = buf_size >> HNS3_BUF_SIZE_UNIT_SHIFT;
		req->tx_pkt_buff[i] = rte_cpu_to_le_16(buf_size |
						HNS3_BUF_SIZE_UPDATE_EN_MSK);
	}

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		PMD_INIT_LOG(ERR, "tx buffer alloc cmd failed %d", ret);

	return ret;
}

static int
hns3_get_tc_num(struct hns3_hw *hw)
{
	int cnt = 0;
	uint8_t i;

	for (i = 0; i < HNS3_MAX_TC_NUM; i++)
		if (hw->hw_tc_map & BIT(i))
			cnt++;
	return cnt;
}

static uint32_t
hns3_get_rx_priv_buff_alloced(struct hns3_pkt_buf_alloc *buf_alloc)
{
	struct hns3_priv_buf *priv;
	uint32_t rx_priv = 0;
	int i;

	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		priv = &buf_alloc->priv_buf[i];
		if (priv->enable)
			rx_priv += priv->buf_size;
	}
	return rx_priv;
}

static uint32_t
hns3_get_tx_buff_alloced(struct hns3_pkt_buf_alloc *buf_alloc)
{
	uint32_t total_tx_size = 0;
	uint32_t i;

	for (i = 0; i < HNS3_MAX_TC_NUM; i++)
		total_tx_size += buf_alloc->priv_buf[i].tx_buf_size;

	return total_tx_size;
}

/* Get the number of pfc enabled TCs, which have private buffer */
static int
hns3_get_pfc_priv_num(struct hns3_hw *hw, struct hns3_pkt_buf_alloc *buf_alloc)
{
	struct hns3_priv_buf *priv;
	int cnt = 0;
	uint8_t i;

	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		priv = &buf_alloc->priv_buf[i];
		if ((hw->dcb_info.hw_pfc_map & BIT(i)) && priv->enable)
			cnt++;
	}

	return cnt;
}

/* Get the number of pfc disabled TCs, which have private buffer */
static int
hns3_get_no_pfc_priv_num(struct hns3_hw *hw,
			 struct hns3_pkt_buf_alloc *buf_alloc)
{
	struct hns3_priv_buf *priv;
	int cnt = 0;
	uint8_t i;

	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		priv = &buf_alloc->priv_buf[i];
		if (hw->hw_tc_map & BIT(i) &&
		    !(hw->dcb_info.hw_pfc_map & BIT(i)) && priv->enable)
			cnt++;
	}

	return cnt;
}

static bool
hns3_is_rx_buf_ok(struct hns3_hw *hw, struct hns3_pkt_buf_alloc *buf_alloc,
		  uint32_t rx_all)
{
	uint32_t shared_buf_min, shared_buf_tc, shared_std, hi_thrd, lo_thrd;
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	uint32_t shared_buf, aligned_mps;
	uint32_t rx_priv;
	uint8_t tc_num;
	uint8_t i;

	tc_num = hns3_get_tc_num(hw);
	aligned_mps = roundup(pf->mps, HNS3_BUF_SIZE_UNIT);

	if (hns3_dev_dcb_supported(hw))
		shared_buf_min = HNS3_BUF_MUL_BY * aligned_mps +
					pf->dv_buf_size;
	else
		shared_buf_min = aligned_mps + HNS3_NON_DCB_ADDITIONAL_BUF
					+ pf->dv_buf_size;

	shared_buf_tc = tc_num * aligned_mps + aligned_mps;
	shared_std = roundup(max_t(uint32_t, shared_buf_min, shared_buf_tc),
			     HNS3_BUF_SIZE_UNIT);

	rx_priv = hns3_get_rx_priv_buff_alloced(buf_alloc);
	if (rx_all < rx_priv + shared_std)
		return false;

	shared_buf = rounddown(rx_all - rx_priv, HNS3_BUF_SIZE_UNIT);
	buf_alloc->s_buf.buf_size = shared_buf;
	if (hns3_dev_dcb_supported(hw)) {
		buf_alloc->s_buf.self.high = shared_buf - pf->dv_buf_size;
		buf_alloc->s_buf.self.low = buf_alloc->s_buf.self.high
			- roundup(aligned_mps / HNS3_BUF_DIV_BY,
				  HNS3_BUF_SIZE_UNIT);
	} else {
		buf_alloc->s_buf.self.high =
			aligned_mps + HNS3_NON_DCB_ADDITIONAL_BUF;
		buf_alloc->s_buf.self.low = aligned_mps;
	}

	if (hns3_dev_dcb_supported(hw)) {
		hi_thrd = shared_buf - pf->dv_buf_size;

		if (tc_num <= NEED_RESERVE_TC_NUM)
			hi_thrd = hi_thrd * BUF_RESERVE_PERCENT
					/ BUF_MAX_PERCENT;

		if (tc_num)
			hi_thrd = hi_thrd / tc_num;

		hi_thrd = max_t(uint32_t, hi_thrd,
				HNS3_BUF_MUL_BY * aligned_mps);
		hi_thrd = rounddown(hi_thrd, HNS3_BUF_SIZE_UNIT);
		lo_thrd = hi_thrd - aligned_mps / HNS3_BUF_DIV_BY;
	} else {
		hi_thrd = aligned_mps + HNS3_NON_DCB_ADDITIONAL_BUF;
		lo_thrd = aligned_mps;
	}

	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		buf_alloc->s_buf.tc_thrd[i].low = lo_thrd;
		buf_alloc->s_buf.tc_thrd[i].high = hi_thrd;
	}

	return true;
}

static bool
hns3_rx_buf_calc_all(struct hns3_hw *hw, bool max,
		     struct hns3_pkt_buf_alloc *buf_alloc)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	struct hns3_priv_buf *priv;
	uint32_t aligned_mps;
	uint32_t rx_all;
	uint8_t i;

	rx_all = pf->pkt_buf_size - hns3_get_tx_buff_alloced(buf_alloc);
	aligned_mps = roundup(pf->mps, HNS3_BUF_SIZE_UNIT);

	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		priv = &buf_alloc->priv_buf[i];

		priv->enable = 0;
		priv->wl.low = 0;
		priv->wl.high = 0;
		priv->buf_size = 0;

		if (!(hw->hw_tc_map & BIT(i)))
			continue;

		priv->enable = 1;
		if (hw->dcb_info.hw_pfc_map & BIT(i)) {
			priv->wl.low = max ? aligned_mps : HNS3_BUF_SIZE_UNIT;
			priv->wl.high = roundup(priv->wl.low + aligned_mps,
						HNS3_BUF_SIZE_UNIT);
		} else {
			priv->wl.low = 0;
			priv->wl.high = max ? (aligned_mps * HNS3_BUF_MUL_BY) :
					aligned_mps;
		}

		priv->buf_size = priv->wl.high + pf->dv_buf_size;
	}

	return hns3_is_rx_buf_ok(hw, buf_alloc, rx_all);
}

static bool
hns3_drop_nopfc_buf_till_fit(struct hns3_hw *hw,
			     struct hns3_pkt_buf_alloc *buf_alloc)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	struct hns3_priv_buf *priv;
	int no_pfc_priv_num;
	uint32_t rx_all;
	uint8_t mask;
	int i;

	rx_all = pf->pkt_buf_size - hns3_get_tx_buff_alloced(buf_alloc);
	no_pfc_priv_num = hns3_get_no_pfc_priv_num(hw, buf_alloc);

	/* let the last to be cleared first */
	for (i = HNS3_MAX_TC_NUM - 1; i >= 0; i--) {
		priv = &buf_alloc->priv_buf[i];
		mask = BIT((uint8_t)i);

		if (hw->hw_tc_map & mask &&
		    !(hw->dcb_info.hw_pfc_map & mask)) {
			/* Clear the no pfc TC private buffer */
			priv->wl.low = 0;
			priv->wl.high = 0;
			priv->buf_size = 0;
			priv->enable = 0;
			no_pfc_priv_num--;
		}

		if (hns3_is_rx_buf_ok(hw, buf_alloc, rx_all) ||
		    no_pfc_priv_num == 0)
			break;
	}

	return hns3_is_rx_buf_ok(hw, buf_alloc, rx_all);
}

static bool
hns3_drop_pfc_buf_till_fit(struct hns3_hw *hw,
			   struct hns3_pkt_buf_alloc *buf_alloc)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	struct hns3_priv_buf *priv;
	uint32_t rx_all;
	int pfc_priv_num;
	uint8_t mask;
	int i;

	rx_all = pf->pkt_buf_size - hns3_get_tx_buff_alloced(buf_alloc);
	pfc_priv_num = hns3_get_pfc_priv_num(hw, buf_alloc);

	/* let the last to be cleared first */
	for (i = HNS3_MAX_TC_NUM - 1; i >= 0; i--) {
		priv = &buf_alloc->priv_buf[i];
		mask = BIT((uint8_t)i);

		if (hw->hw_tc_map & mask &&
		    hw->dcb_info.hw_pfc_map & mask) {
			/* Reduce the number of pfc TC with private buffer */
			priv->wl.low = 0;
			priv->enable = 0;
			priv->wl.high = 0;
			priv->buf_size = 0;
			pfc_priv_num--;
		}
		if (hns3_is_rx_buf_ok(hw, buf_alloc, rx_all) ||
		    pfc_priv_num == 0)
			break;
	}

	return hns3_is_rx_buf_ok(hw, buf_alloc, rx_all);
}

static bool
hns3_only_alloc_priv_buff(struct hns3_hw *hw,
			  struct hns3_pkt_buf_alloc *buf_alloc)
{
#define COMPENSATE_BUFFER	0x3C00
#define COMPENSATE_HALF_MPS_NUM	5
#define PRIV_WL_GAP		0x1800
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	uint32_t tc_num = hns3_get_tc_num(hw);
	uint32_t half_mps = pf->mps >> 1;
	struct hns3_priv_buf *priv;
	uint32_t min_rx_priv;
	uint32_t rx_priv;
	uint8_t i;

	rx_priv = pf->pkt_buf_size - hns3_get_tx_buff_alloced(buf_alloc);
	if (tc_num)
		rx_priv = rx_priv / tc_num;

	if (tc_num <= NEED_RESERVE_TC_NUM)
		rx_priv = rx_priv * BUF_RESERVE_PERCENT / BUF_MAX_PERCENT;

	/*
	 * Minimum value of private buffer in rx direction (min_rx_priv) is
	 * equal to "DV + 2.5 * MPS + 15KB". Driver only allocates rx private
	 * buffer if rx_priv is greater than min_rx_priv.
	 */
	min_rx_priv = pf->dv_buf_size + COMPENSATE_BUFFER +
			COMPENSATE_HALF_MPS_NUM * half_mps;
	min_rx_priv = roundup(min_rx_priv, HNS3_BUF_SIZE_UNIT);
	rx_priv = rounddown(rx_priv, HNS3_BUF_SIZE_UNIT);

	if (rx_priv < min_rx_priv)
		return false;

	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		priv = &buf_alloc->priv_buf[i];

		priv->enable = 0;
		priv->wl.low = 0;
		priv->wl.high = 0;
		priv->buf_size = 0;

		if (!(hw->hw_tc_map & BIT(i)))
			continue;

		priv->enable = 1;
		priv->buf_size = rx_priv;
		priv->wl.high = rx_priv - pf->dv_buf_size;
		priv->wl.low = priv->wl.high - PRIV_WL_GAP;
	}

	buf_alloc->s_buf.buf_size = 0;

	return true;
}

/*
 * hns3_rx_buffer_calc: calculate the rx private buffer size for all TCs
 * @hw: pointer to struct hns3_hw
 * @buf_alloc: pointer to buffer calculation data
 * @return: 0: calculate sucessful, negative: fail
 */
static int
hns3_rx_buffer_calc(struct hns3_hw *hw, struct hns3_pkt_buf_alloc *buf_alloc)
{
	/* When DCB is not supported, rx private buffer is not allocated. */
	if (!hns3_dev_dcb_supported(hw)) {
		struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
		struct hns3_pf *pf = &hns->pf;
		uint32_t rx_all = pf->pkt_buf_size;

		rx_all -= hns3_get_tx_buff_alloced(buf_alloc);
		if (!hns3_is_rx_buf_ok(hw, buf_alloc, rx_all))
			return -ENOMEM;

		return 0;
	}

	/*
	 * Try to allocate privated packet buffer for all TCs without share
	 * buffer.
	 */
	if (hns3_only_alloc_priv_buff(hw, buf_alloc))
		return 0;

	/*
	 * Try to allocate privated packet buffer for all TCs with share
	 * buffer.
	 */
	if (hns3_rx_buf_calc_all(hw, true, buf_alloc))
		return 0;

	/*
	 * For different application scenes, the enabled port number, TC number
	 * and no_drop TC number are different. In order to obtain the better
	 * performance, software could allocate the buffer size and configure
	 * the waterline by tring to decrease the private buffer size according
	 * to the order, namely, waterline of valided tc, pfc disabled tc, pfc
	 * enabled tc.
	 */
	if (hns3_rx_buf_calc_all(hw, false, buf_alloc))
		return 0;

	if (hns3_drop_nopfc_buf_till_fit(hw, buf_alloc))
		return 0;

	if (hns3_drop_pfc_buf_till_fit(hw, buf_alloc))
		return 0;

	return -ENOMEM;
}

static int
hns3_rx_priv_buf_alloc(struct hns3_hw *hw, struct hns3_pkt_buf_alloc *buf_alloc)
{
	struct hns3_rx_priv_buff_cmd *req;
	struct hns3_cmd_desc desc;
	uint32_t buf_size;
	int ret;
	int i;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RX_PRIV_BUFF_ALLOC, false);
	req = (struct hns3_rx_priv_buff_cmd *)desc.data;

	/* Alloc private buffer TCs */
	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		struct hns3_priv_buf *priv = &buf_alloc->priv_buf[i];

		req->buf_num[i] =
			rte_cpu_to_le_16(priv->buf_size >> HNS3_BUF_UNIT_S);
		req->buf_num[i] |= rte_cpu_to_le_16(1 << HNS3_TC0_PRI_BUF_EN_B);
	}

	buf_size = buf_alloc->s_buf.buf_size;
	req->shared_buf = rte_cpu_to_le_16((buf_size >> HNS3_BUF_UNIT_S) |
					   (1 << HNS3_TC0_PRI_BUF_EN_B));

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		PMD_INIT_LOG(ERR, "rx private buffer alloc cmd failed %d", ret);

	return ret;
}

static int
hns3_rx_priv_wl_config(struct hns3_hw *hw, struct hns3_pkt_buf_alloc *buf_alloc)
{
#define HNS3_RX_PRIV_WL_ALLOC_DESC_NUM 2
	struct hns3_rx_priv_wl_buf *req;
	struct hns3_priv_buf *priv;
	struct hns3_cmd_desc desc[HNS3_RX_PRIV_WL_ALLOC_DESC_NUM];
	int i, j;
	int ret;

	for (i = 0; i < HNS3_RX_PRIV_WL_ALLOC_DESC_NUM; i++) {
		hns3_cmd_setup_basic_desc(&desc[i], HNS3_OPC_RX_PRIV_WL_ALLOC,
					  false);
		req = (struct hns3_rx_priv_wl_buf *)desc[i].data;

		/* The first descriptor set the NEXT bit to 1 */
		if (i == 0)
			desc[i].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
		else
			desc[i].flag &= ~rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);

		for (j = 0; j < HNS3_TC_NUM_ONE_DESC; j++) {
			uint32_t idx = i * HNS3_TC_NUM_ONE_DESC + j;

			priv = &buf_alloc->priv_buf[idx];
			req->tc_wl[j].high = rte_cpu_to_le_16(priv->wl.high >>
							HNS3_BUF_UNIT_S);
			req->tc_wl[j].high |=
				rte_cpu_to_le_16(BIT(HNS3_RX_PRIV_EN_B));
			req->tc_wl[j].low = rte_cpu_to_le_16(priv->wl.low >>
							HNS3_BUF_UNIT_S);
			req->tc_wl[j].low |=
				rte_cpu_to_le_16(BIT(HNS3_RX_PRIV_EN_B));
		}
	}

	/* Send 2 descriptor at one time */
	ret = hns3_cmd_send(hw, desc, HNS3_RX_PRIV_WL_ALLOC_DESC_NUM);
	if (ret)
		PMD_INIT_LOG(ERR, "rx private waterline config cmd failed %d",
			     ret);
	return ret;
}

static int
hns3_common_thrd_config(struct hns3_hw *hw,
			struct hns3_pkt_buf_alloc *buf_alloc)
{
#define HNS3_RX_COM_THRD_ALLOC_DESC_NUM 2
	struct hns3_shared_buf *s_buf = &buf_alloc->s_buf;
	struct hns3_rx_com_thrd *req;
	struct hns3_cmd_desc desc[HNS3_RX_COM_THRD_ALLOC_DESC_NUM];
	struct hns3_tc_thrd *tc;
	int tc_idx;
	int i, j;
	int ret;

	for (i = 0; i < HNS3_RX_COM_THRD_ALLOC_DESC_NUM; i++) {
		hns3_cmd_setup_basic_desc(&desc[i], HNS3_OPC_RX_COM_THRD_ALLOC,
					  false);
		req = (struct hns3_rx_com_thrd *)&desc[i].data;

		/* The first descriptor set the NEXT bit to 1 */
		if (i == 0)
			desc[i].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
		else
			desc[i].flag &= ~rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);

		for (j = 0; j < HNS3_TC_NUM_ONE_DESC; j++) {
			tc_idx = i * HNS3_TC_NUM_ONE_DESC + j;
			tc = &s_buf->tc_thrd[tc_idx];

			req->com_thrd[j].high =
				rte_cpu_to_le_16(tc->high >> HNS3_BUF_UNIT_S);
			req->com_thrd[j].high |=
				 rte_cpu_to_le_16(BIT(HNS3_RX_PRIV_EN_B));
			req->com_thrd[j].low =
				rte_cpu_to_le_16(tc->low >> HNS3_BUF_UNIT_S);
			req->com_thrd[j].low |=
				 rte_cpu_to_le_16(BIT(HNS3_RX_PRIV_EN_B));
		}
	}

	/* Send 2 descriptors at one time */
	ret = hns3_cmd_send(hw, desc, HNS3_RX_COM_THRD_ALLOC_DESC_NUM);
	if (ret)
		PMD_INIT_LOG(ERR, "common threshold config cmd failed %d", ret);

	return ret;
}

static int
hns3_common_wl_config(struct hns3_hw *hw, struct hns3_pkt_buf_alloc *buf_alloc)
{
	struct hns3_shared_buf *buf = &buf_alloc->s_buf;
	struct hns3_rx_com_wl *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RX_COM_WL_ALLOC, false);

	req = (struct hns3_rx_com_wl *)desc.data;
	req->com_wl.high = rte_cpu_to_le_16(buf->self.high >> HNS3_BUF_UNIT_S);
	req->com_wl.high |= rte_cpu_to_le_16(BIT(HNS3_RX_PRIV_EN_B));

	req->com_wl.low = rte_cpu_to_le_16(buf->self.low >> HNS3_BUF_UNIT_S);
	req->com_wl.low |= rte_cpu_to_le_16(BIT(HNS3_RX_PRIV_EN_B));

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		PMD_INIT_LOG(ERR, "common waterline config cmd failed %d", ret);

	return ret;
}

int
hns3_buffer_alloc(struct hns3_hw *hw)
{
	struct hns3_pkt_buf_alloc pkt_buf;
	int ret;

	memset(&pkt_buf, 0, sizeof(pkt_buf));
	ret = hns3_tx_buffer_calc(hw, &pkt_buf);
	if (ret) {
		PMD_INIT_LOG(ERR,
			     "could not calc tx buffer size for all TCs %d",
			     ret);
		return ret;
	}

	ret = hns3_tx_buffer_alloc(hw, &pkt_buf);
	if (ret) {
		PMD_INIT_LOG(ERR, "could not alloc tx buffers %d", ret);
		return ret;
	}

	ret = hns3_rx_buffer_calc(hw, &pkt_buf);
	if (ret) {
		PMD_INIT_LOG(ERR,
			     "could not calc rx priv buffer size for all TCs %d",
			     ret);
		return ret;
	}

	ret = hns3_rx_priv_buf_alloc(hw, &pkt_buf);
	if (ret) {
		PMD_INIT_LOG(ERR, "could not alloc rx priv buffer %d", ret);
		return ret;
	}

	if (hns3_dev_dcb_supported(hw)) {
		ret = hns3_rx_priv_wl_config(hw, &pkt_buf);
		if (ret) {
			PMD_INIT_LOG(ERR,
				     "could not configure rx private waterline %d",
				     ret);
			return ret;
		}

		ret = hns3_common_thrd_config(hw, &pkt_buf);
		if (ret) {
			PMD_INIT_LOG(ERR,
				     "could not configure common threshold %d",
				     ret);
			return ret;
		}
	}

	ret = hns3_common_wl_config(hw, &pkt_buf);
	if (ret)
		PMD_INIT_LOG(ERR, "could not configure common waterline %d",
			     ret);

	return ret;
}

static int
hns3_mac_init(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_mac *mac = &hw->mac;
	struct hns3_pf *pf = &hns->pf;
	int ret;

	pf->support_sfp_query = true;
	mac->link_duplex = ETH_LINK_FULL_DUPLEX;
	ret = hns3_cfg_mac_speed_dup_hw(hw, mac->link_speed, mac->link_duplex);
	if (ret) {
		PMD_INIT_LOG(ERR, "Config mac speed dup fail ret = %d", ret);
		return ret;
	}

	mac->link_status = ETH_LINK_DOWN;

	return hns3_config_mtu(hw, pf->mps);
}

static int
hns3_get_mac_ethertype_cmd_status(uint16_t cmdq_resp, uint8_t resp_code)
{
#define HNS3_ETHERTYPE_SUCCESS_ADD		0
#define HNS3_ETHERTYPE_ALREADY_ADD		1
#define HNS3_ETHERTYPE_MGR_TBL_OVERFLOW		2
#define HNS3_ETHERTYPE_KEY_CONFLICT		3
	int return_status;

	if (cmdq_resp) {
		PMD_INIT_LOG(ERR,
			     "cmdq execute failed for get_mac_ethertype_cmd_status, status=%d.\n",
			     cmdq_resp);
		return -EIO;
	}

	switch (resp_code) {
	case HNS3_ETHERTYPE_SUCCESS_ADD:
	case HNS3_ETHERTYPE_ALREADY_ADD:
		return_status = 0;
		break;
	case HNS3_ETHERTYPE_MGR_TBL_OVERFLOW:
		PMD_INIT_LOG(ERR,
			     "add mac ethertype failed for manager table overflow.");
		return_status = -EIO;
		break;
	case HNS3_ETHERTYPE_KEY_CONFLICT:
		PMD_INIT_LOG(ERR, "add mac ethertype failed for key conflict.");
		return_status = -EIO;
		break;
	default:
		PMD_INIT_LOG(ERR,
			     "add mac ethertype failed for undefined, code=%d.",
			     resp_code);
		return_status = -EIO;
	}

	return return_status;
}

static int
hns3_add_mgr_tbl(struct hns3_hw *hw,
		 const struct hns3_mac_mgr_tbl_entry_cmd *req)
{
	struct hns3_cmd_desc desc;
	uint8_t resp_code;
	uint16_t retval;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_MAC_ETHTYPE_ADD, false);
	memcpy(desc.data, req, sizeof(struct hns3_mac_mgr_tbl_entry_cmd));

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		PMD_INIT_LOG(ERR,
			     "add mac ethertype failed for cmd_send, ret =%d.",
			     ret);
		return ret;
	}

	resp_code = (rte_le_to_cpu_32(desc.data[0]) >> 8) & 0xff;
	retval = rte_le_to_cpu_16(desc.retval);

	return hns3_get_mac_ethertype_cmd_status(retval, resp_code);
}

static void
hns3_prepare_mgr_tbl(struct hns3_mac_mgr_tbl_entry_cmd *mgr_table,
		     int *table_item_num)
{
	struct hns3_mac_mgr_tbl_entry_cmd *tbl;

	/*
	 * In current version, we add one item in management table as below:
	 * 0x0180C200000E -- LLDP MC address
	 */
	tbl = mgr_table;
	tbl->flags = HNS3_MAC_MGR_MASK_VLAN_B;
	tbl->ethter_type = rte_cpu_to_le_16(HNS3_MAC_ETHERTYPE_LLDP);
	tbl->mac_addr_hi32 = rte_cpu_to_le_32(htonl(0x0180C200));
	tbl->mac_addr_lo16 = rte_cpu_to_le_16(htons(0x000E));
	tbl->i_port_bitmap = 0x1;
	*table_item_num = 1;
}

static int
hns3_init_mgr_tbl(struct hns3_hw *hw)
{
#define HNS_MAC_MGR_TBL_MAX_SIZE	16
	struct hns3_mac_mgr_tbl_entry_cmd mgr_table[HNS_MAC_MGR_TBL_MAX_SIZE];
	int table_item_num;
	int ret;
	int i;

	memset(mgr_table, 0, sizeof(mgr_table));
	hns3_prepare_mgr_tbl(mgr_table, &table_item_num);
	for (i = 0; i < table_item_num; i++) {
		ret = hns3_add_mgr_tbl(hw, &mgr_table[i]);
		if (ret) {
			PMD_INIT_LOG(ERR, "add mac ethertype failed, ret =%d",
				     ret);
			return ret;
		}
	}

	return 0;
}

static void
hns3_promisc_param_init(struct hns3_promisc_param *param, bool en_uc,
			bool en_mc, bool en_bc, int vport_id)
{
	if (!param)
		return;

	memset(param, 0, sizeof(struct hns3_promisc_param));
	if (en_uc)
		param->enable = HNS3_PROMISC_EN_UC;
	if (en_mc)
		param->enable |= HNS3_PROMISC_EN_MC;
	if (en_bc)
		param->enable |= HNS3_PROMISC_EN_BC;
	param->vf_id = vport_id;
}

static int
hns3_cmd_set_promisc_mode(struct hns3_hw *hw, struct hns3_promisc_param *param)
{
	struct hns3_promisc_cfg_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_CFG_PROMISC_MODE, false);

	req = (struct hns3_promisc_cfg_cmd *)desc.data;
	req->vf_id = param->vf_id;
	req->flag = (param->enable << HNS3_PROMISC_EN_B) |
	    HNS3_PROMISC_TX_EN_B | HNS3_PROMISC_RX_EN_B;

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		PMD_INIT_LOG(ERR, "Set promisc mode fail, status is %d", ret);

	return ret;
}

static int
hns3_set_promisc_mode(struct hns3_hw *hw, bool en_uc_pmc, bool en_mc_pmc)
{
	struct hns3_promisc_param param;
	bool en_bc_pmc = true;
	uint8_t vf_id;
	int ret;

	/*
	 * In current version VF is not supported when PF is driven by DPDK
	 * driver, the PF-related vf_id is 0, just need to configure parameters
	 * for vf_id 0.
	 */
	vf_id = 0;

	hns3_promisc_param_init(&param, en_uc_pmc, en_mc_pmc, en_bc_pmc, vf_id);
	ret = hns3_cmd_set_promisc_mode(hw, &param);
	if (ret)
		return ret;

	return 0;
}

static int
hns3_init_hardware(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	int ret;

	ret = hns3_map_tqp(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to map tqp: %d", ret);
		return ret;
	}

	ret = hns3_init_umv_space(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init umv space: %d", ret);
		return ret;
	}

	ret = hns3_mac_init(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init MAC: %d", ret);
		goto err_mac_init;
	}

	ret = hns3_init_mgr_tbl(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init manager table: %d", ret);
		goto err_mac_init;
	}

	ret = hns3_set_promisc_mode(hw, false, false);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to set promisc mode: %d", ret);
		goto err_mac_init;
	}

	ret = hns3_config_tso(hw, HNS3_TSO_MSS_MIN, HNS3_TSO_MSS_MAX);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to config tso: %d", ret);
		goto err_mac_init;
	}

	ret = hns3_config_gro(hw, false);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to config gro: %d", ret);
		goto err_mac_init;
	}
	return 0;

err_mac_init:
	hns3_uninit_umv_space(hw);
	return ret;
}

static int
hns3_init_pf(struct rte_eth_dev *eth_dev)
{
	struct rte_device *dev = eth_dev->device;
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev);
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	PMD_INIT_FUNC_TRACE();

	/* Get hardware io base address from pcie BAR2 IO space */
	hw->io_base = pci_dev->mem_resource[2].addr;

	/* Firmware command queue initialize */
	ret = hns3_cmd_init_queue(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init cmd queue: %d", ret);
		goto err_cmd_init_queue;
	}

	/* Firmware command initialize */
	ret = hns3_cmd_init(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init cmd: %d", ret);
		goto err_cmd_init;
	}

	/* Get configuration */
	ret = hns3_get_configuration(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to fetch configuration: %d", ret);
		goto err_get_config;
	}

	ret = hns3_init_hardware(hns);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init hardware: %d", ret);
		goto err_get_config;
	}

	return 0;

err_get_config:
	hns3_cmd_uninit(hw);

err_cmd_init:
	hns3_cmd_destroy_queue(hw);

err_cmd_init_queue:
	hw->io_base = NULL;

	return ret;
}

static void
hns3_uninit_pf(struct rte_eth_dev *eth_dev)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	PMD_INIT_FUNC_TRACE();

	hns3_uninit_umv_space(hw);
	hns3_cmd_uninit(hw);
	hns3_cmd_destroy_queue(hw);
	hw->io_base = NULL;
}

static void
hns3_dev_close(struct rte_eth_dev *eth_dev)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	hw->adapter_state = HNS3_NIC_CLOSING;
	hns3_uninit_pf(eth_dev);
	hw->adapter_state = HNS3_NIC_CLOSED;
}

static const struct eth_dev_ops hns3_eth_dev_ops = {
	.dev_close          = hns3_dev_close,
};

static int
hns3_dev_init(struct rte_eth_dev *eth_dev)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	PMD_INIT_FUNC_TRACE();

	eth_dev->dev_ops = &hns3_eth_dev_ops;
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	hw->adapter_state = HNS3_NIC_UNINITIALIZED;
	hns->is_vf = false;
	hw->data = eth_dev->data;

	/*
	 * Set default max packet size according to the mtu
	 * default vale in DPDK frame.
	 */
	hns->pf.mps = hw->data->mtu + HNS3_ETH_OVERHEAD;

	ret = hns3_init_pf(eth_dev);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init pf: %d", ret);
		goto err_init_pf;
	}

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("hns3-mac",
					       sizeof(struct rte_ether_addr) *
					       HNS3_UC_MACADDR_NUM, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate %zx bytes needed "
			     "to store MAC addresses",
			     sizeof(struct rte_ether_addr) *
			     HNS3_UC_MACADDR_NUM);
		ret = -ENOMEM;
		goto err_rte_zmalloc;
	}

	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac.mac_addr,
			    &eth_dev->data->mac_addrs[0]);

	hw->adapter_state = HNS3_NIC_INITIALIZED;
	/*
	 * Pass the information to the rte_eth_dev_close() that it should also
	 * release the private port resources.
	 */
	eth_dev->data->dev_flags |= RTE_ETH_DEV_CLOSE_REMOVE;

	hns3_info(hw, "hns3 dev initialization successful!");
	return 0;

err_rte_zmalloc:
	hns3_uninit_pf(eth_dev);

err_init_pf:
	eth_dev->dev_ops = NULL;
	eth_dev->rx_pkt_burst = NULL;
	eth_dev->tx_pkt_burst = NULL;
	eth_dev->tx_pkt_prepare = NULL;
	return ret;
}

static int
hns3_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -EPERM;

	eth_dev->dev_ops = NULL;
	eth_dev->rx_pkt_burst = NULL;
	eth_dev->tx_pkt_burst = NULL;
	eth_dev->tx_pkt_prepare = NULL;
	if (hw->adapter_state < HNS3_NIC_CLOSING)
		hns3_dev_close(eth_dev);

	hw->adapter_state = HNS3_NIC_REMOVED;
	return 0;
}

static int
eth_hns3_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		   struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
					     sizeof(struct hns3_adapter),
					     hns3_dev_init);
}

static int
eth_hns3_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, hns3_dev_uninit);
}

static const struct rte_pci_id pci_id_hns3_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HNS3_DEV_ID_GE) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HNS3_DEV_ID_25GE) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HNS3_DEV_ID_25GE_RDMA) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HNS3_DEV_ID_50GE_RDMA) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HNS3_DEV_ID_100G_RDMA_MACSEC) },
	{ .vendor_id = 0, /* sentinel */ },
};

static struct rte_pci_driver rte_hns3_pmd = {
	.id_table = pci_id_hns3_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = eth_hns3_pci_probe,
	.remove = eth_hns3_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_hns3, rte_hns3_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_hns3, pci_id_hns3_map);
RTE_PMD_REGISTER_KMOD_DEP(net_hns3, "* igb_uio | vfio-pci");

RTE_INIT(hns3_init_log)
{
	hns3_logtype_init = rte_log_register("pmd.net.hns3.init");
	if (hns3_logtype_init >= 0)
		rte_log_set_level(hns3_logtype_init, RTE_LOG_NOTICE);
	hns3_logtype_driver = rte_log_register("pmd.net.hns3.driver");
	if (hns3_logtype_driver >= 0)
		rte_log_set_level(hns3_logtype_driver, RTE_LOG_NOTICE);
}
