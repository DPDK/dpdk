/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2019 Hisilicon Limited.
 */

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <rte_io.h>
#include <rte_common.h>
#include <rte_ethdev.h>

#include "hns3_logs.h"
#include "hns3_regs.h"
#include "hns3_ethdev.h"
#include "hns3_dcb.h"

#define HNS3_SHAPER_BS_U_DEF	5
#define HNS3_SHAPER_BS_S_DEF	20
#define BW_MAX_PERCENT		100
#define HNS3_ETHER_MAX_RATE	100000

/*
 * hns3_shaper_para_calc: calculate ir parameter for the shaper
 * @ir: Rate to be config, its unit is Mbps
 * @shaper_level: the shaper level. eg: port, pg, priority, queueset
 * @shaper_para: shaper parameter of IR shaper
 *
 * the formula:
 *
 *		IR_b * (2 ^ IR_u) * 8
 * IR(Mbps) = -------------------------  *  CLOCK(1000Mbps)
 *		Tick * (2 ^ IR_s)
 *
 * @return: 0: calculate sucessful, negative: fail
 */
static int
hns3_shaper_para_calc(struct hns3_hw *hw, uint32_t ir, uint8_t shaper_level,
		      struct hns3_shaper_parameter *shaper_para)
{
#define SHAPER_DEFAULT_IR_B	126
#define DIVISOR_CLK		(1000 * 8)
#define DIVISOR_IR_B_126	(126 * DIVISOR_CLK)

	const uint16_t tick_array[HNS3_SHAPER_LVL_CNT] = {
		6 * 256,    /* Prioriy level */
		6 * 32,     /* Prioriy group level */
		6 * 8,      /* Port level */
		6 * 256     /* Qset level */
	};
	uint8_t ir_u_calc = 0;
	uint8_t ir_s_calc = 0;
	uint32_t denominator;
	uint32_t ir_calc;
	uint32_t tick;

	/* Calc tick */
	if (shaper_level >= HNS3_SHAPER_LVL_CNT) {
		hns3_err(hw,
			 "shaper_level(%d) is greater than HNS3_SHAPER_LVL_CNT(%d)",
			 shaper_level, HNS3_SHAPER_LVL_CNT);
		return -EINVAL;
	}

	if (ir > HNS3_ETHER_MAX_RATE) {
		hns3_err(hw, "rate(%d) exceeds the rate driver supported "
			 "HNS3_ETHER_MAX_RATE(%d)", ir, HNS3_ETHER_MAX_RATE);
		return -EINVAL;
	}

	tick = tick_array[shaper_level];

	/*
	 * Calc the speed if ir_b = 126, ir_u = 0 and ir_s = 0
	 * the formula is changed to:
	 *		126 * 1 * 8
	 * ir_calc = ---------------- * 1000
	 *		tick * 1
	 */
	ir_calc = (DIVISOR_IR_B_126 + (tick >> 1) - 1) / tick;

	if (ir_calc == ir) {
		shaper_para->ir_b = SHAPER_DEFAULT_IR_B;
	} else if (ir_calc > ir) {
		/* Increasing the denominator to select ir_s value */
		do {
			ir_s_calc++;
			ir_calc = DIVISOR_IR_B_126 / (tick * (1 << ir_s_calc));
		} while (ir_calc > ir);

		if (ir_calc == ir)
			shaper_para->ir_b = SHAPER_DEFAULT_IR_B;
		else
			shaper_para->ir_b = (ir * tick * (1 << ir_s_calc) +
				 (DIVISOR_CLK >> 1)) / DIVISOR_CLK;
	} else {
		/*
		 * Increasing the numerator to select ir_u value. ir_u_calc will
		 * get maximum value when ir_calc is minimum and ir is maximum.
		 * ir_calc gets minimum value when tick is the maximum value.
		 * At the same time, value of ir_u_calc can only be increased up
		 * to eight after the while loop if the value of ir is equal
		 * to HNS3_ETHER_MAX_RATE.
		 */
		uint32_t numerator;
		do {
			ir_u_calc++;
			numerator = DIVISOR_IR_B_126 * (1 << ir_u_calc);
			ir_calc = (numerator + (tick >> 1)) / tick;
		} while (ir_calc < ir);

		if (ir_calc == ir) {
			shaper_para->ir_b = SHAPER_DEFAULT_IR_B;
		} else {
			--ir_u_calc;

			/*
			 * The maximum value of ir_u_calc in this branch is
			 * seven in all cases. Thus, value of denominator can
			 * not be zero here.
			 */
			denominator = DIVISOR_CLK * (1 << ir_u_calc);
			shaper_para->ir_b =
				(ir * tick + (denominator >> 1)) / denominator;
		}
	}

	shaper_para->ir_u = ir_u_calc;
	shaper_para->ir_s = ir_s_calc;

	return 0;
}

static int
hns3_fill_pri_array(struct hns3_hw *hw, uint8_t *pri, uint8_t pri_id)
{
#define HNS3_HALF_BYTE_BIT_OFFSET 4
	uint8_t tc = hw->dcb_info.prio_tc[pri_id];

	if (tc >= hw->dcb_info.num_tc)
		return -EINVAL;

	/*
	 * The register for priority has four bytes, the first bytes includes
	 *  priority0 and priority1, the higher 4bit stands for priority1
	 *  while the lower 4bit stands for priority0, as below:
	 * first byte:	| pri_1 | pri_0 |
	 * second byte:	| pri_3 | pri_2 |
	 * third byte:	| pri_5 | pri_4 |
	 * fourth byte:	| pri_7 | pri_6 |
	 */
	pri[pri_id >> 1] |= tc << ((pri_id & 1) * HNS3_HALF_BYTE_BIT_OFFSET);

	return 0;
}

static int
hns3_up_to_tc_map(struct hns3_hw *hw)
{
	struct hns3_cmd_desc desc;
	uint8_t *pri = (uint8_t *)desc.data;
	uint8_t pri_id;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_PRI_TO_TC_MAPPING, false);

	for (pri_id = 0; pri_id < HNS3_MAX_USER_PRIO; pri_id++) {
		ret = hns3_fill_pri_array(hw, pri, pri_id);
		if (ret)
			return ret;
	}

	return hns3_cmd_send(hw, &desc, 1);
}

static int
hns3_pg_to_pri_map_cfg(struct hns3_hw *hw, uint8_t pg_id, uint8_t pri_bit_map)
{
	struct hns3_pg_to_pri_link_cmd *map;
	struct hns3_cmd_desc desc;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_TM_PG_TO_PRI_LINK, false);

	map = (struct hns3_pg_to_pri_link_cmd *)desc.data;

	map->pg_id = pg_id;
	map->pri_bit_map = pri_bit_map;

	return hns3_cmd_send(hw, &desc, 1);
}

static int
hns3_pg_to_pri_map(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	struct hns3_pg_info *pg_info;
	int ret, i;

	if (pf->tx_sch_mode != HNS3_FLAG_TC_BASE_SCH_MODE)
		return -EINVAL;

	for (i = 0; i < hw->dcb_info.num_pg; i++) {
		/* Cfg pg to priority mapping */
		pg_info = &hw->dcb_info.pg_info[i];
		ret = hns3_pg_to_pri_map_cfg(hw, i, pg_info->tc_bit_map);
		if (ret)
			return ret;
	}

	return 0;
}

static int
hns3_qs_to_pri_map_cfg(struct hns3_hw *hw, uint16_t qs_id, uint8_t pri)
{
	struct hns3_qs_to_pri_link_cmd *map;
	struct hns3_cmd_desc desc;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_TM_QS_TO_PRI_LINK, false);

	map = (struct hns3_qs_to_pri_link_cmd *)desc.data;

	map->qs_id = rte_cpu_to_le_16(qs_id);
	map->priority = pri;
	map->link_vld = HNS3_DCB_QS_PRI_LINK_VLD_MSK;

	return hns3_cmd_send(hw, &desc, 1);
}

static int
hns3_dcb_qs_weight_cfg(struct hns3_hw *hw, uint16_t qs_id, uint8_t dwrr)
{
	struct hns3_qs_weight_cmd *weight;
	struct hns3_cmd_desc desc;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_TM_QS_WEIGHT, false);

	weight = (struct hns3_qs_weight_cmd *)desc.data;

	weight->qs_id = rte_cpu_to_le_16(qs_id);
	weight->dwrr = dwrr;

	return hns3_cmd_send(hw, &desc, 1);
}

static int
hns3_dcb_ets_tc_dwrr_cfg(struct hns3_hw *hw)
{
#define DEFAULT_TC_WEIGHT	1
#define DEFAULT_TC_OFFSET	14
	struct hns3_ets_tc_weight_cmd *ets_weight;
	struct hns3_cmd_desc desc;
	uint8_t i;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_ETS_TC_WEIGHT, false);
	ets_weight = (struct hns3_ets_tc_weight_cmd *)desc.data;

	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		struct hns3_pg_info *pg_info;

		ets_weight->tc_weight[i] = DEFAULT_TC_WEIGHT;

		if (!(hw->hw_tc_map & BIT(i)))
			continue;

		pg_info = &hw->dcb_info.pg_info[hw->dcb_info.tc_info[i].pgid];
		ets_weight->tc_weight[i] = pg_info->tc_dwrr[i];
	}

	ets_weight->weight_offset = DEFAULT_TC_OFFSET;

	return hns3_cmd_send(hw, &desc, 1);
}

static int
hns3_dcb_pri_weight_cfg(struct hns3_hw *hw, uint8_t pri_id, uint8_t dwrr)
{
	struct hns3_priority_weight_cmd *weight;
	struct hns3_cmd_desc desc;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_TM_PRI_WEIGHT, false);

	weight = (struct hns3_priority_weight_cmd *)desc.data;

	weight->pri_id = pri_id;
	weight->dwrr = dwrr;

	return hns3_cmd_send(hw, &desc, 1);
}

static int
hns3_dcb_pg_weight_cfg(struct hns3_hw *hw, uint8_t pg_id, uint8_t dwrr)
{
	struct hns3_pg_weight_cmd *weight;
	struct hns3_cmd_desc desc;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_TM_PG_WEIGHT, false);

	weight = (struct hns3_pg_weight_cmd *)desc.data;

	weight->pg_id = pg_id;
	weight->dwrr = dwrr;

	return hns3_cmd_send(hw, &desc, 1);
}
static int
hns3_dcb_pg_schd_mode_cfg(struct hns3_hw *hw, uint8_t pg_id)
{
	struct hns3_cmd_desc desc;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_TM_PG_SCH_MODE_CFG, false);

	if (hw->dcb_info.pg_info[pg_id].pg_sch_mode == HNS3_SCH_MODE_DWRR)
		desc.data[1] = rte_cpu_to_le_32(HNS3_DCB_TX_SCHD_DWRR_MSK);
	else
		desc.data[1] = 0;

	desc.data[0] = rte_cpu_to_le_32(pg_id);

	return hns3_cmd_send(hw, &desc, 1);
}

static uint32_t
hns3_dcb_get_shapping_para(uint8_t ir_b, uint8_t ir_u, uint8_t ir_s,
			   uint8_t bs_b, uint8_t bs_s)
{
	uint32_t shapping_para = 0;

	hns3_dcb_set_field(shapping_para, IR_B, ir_b);
	hns3_dcb_set_field(shapping_para, IR_U, ir_u);
	hns3_dcb_set_field(shapping_para, IR_S, ir_s);
	hns3_dcb_set_field(shapping_para, BS_B, bs_b);
	hns3_dcb_set_field(shapping_para, BS_S, bs_s);

	return shapping_para;
}

static int
hns3_dcb_port_shaper_cfg(struct hns3_hw *hw)
{
	struct hns3_port_shapping_cmd *shap_cfg_cmd;
	struct hns3_shaper_parameter shaper_parameter;
	uint32_t shapping_para;
	uint32_t ir_u, ir_b, ir_s;
	struct hns3_cmd_desc desc;
	int ret;

	ret = hns3_shaper_para_calc(hw, hw->mac.link_speed,
				    HNS3_SHAPER_LVL_PORT, &shaper_parameter);
	if (ret) {
		hns3_err(hw, "calculate shaper parameter failed: %d", ret);
		return ret;
	}

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_TM_PORT_SHAPPING, false);
	shap_cfg_cmd = (struct hns3_port_shapping_cmd *)desc.data;

	ir_b = shaper_parameter.ir_b;
	ir_u = shaper_parameter.ir_u;
	ir_s = shaper_parameter.ir_s;
	shapping_para = hns3_dcb_get_shapping_para(ir_b, ir_u, ir_s,
						   HNS3_SHAPER_BS_U_DEF,
						   HNS3_SHAPER_BS_S_DEF);

	shap_cfg_cmd->port_shapping_para = rte_cpu_to_le_32(shapping_para);

	return hns3_cmd_send(hw, &desc, 1);
}

static int
hns3_dcb_pg_shapping_cfg(struct hns3_hw *hw, enum hns3_shap_bucket bucket,
			 uint8_t pg_id, uint32_t shapping_para)
{
	struct hns3_pg_shapping_cmd *shap_cfg_cmd;
	enum hns3_opcode_type opcode;
	struct hns3_cmd_desc desc;

	opcode = bucket ? HNS3_OPC_TM_PG_P_SHAPPING :
		 HNS3_OPC_TM_PG_C_SHAPPING;
	hns3_cmd_setup_basic_desc(&desc, opcode, false);

	shap_cfg_cmd = (struct hns3_pg_shapping_cmd *)desc.data;

	shap_cfg_cmd->pg_id = pg_id;

	shap_cfg_cmd->pg_shapping_para = rte_cpu_to_le_32(shapping_para);

	return hns3_cmd_send(hw, &desc, 1);
}

static int
hns3_dcb_pg_shaper_cfg(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_shaper_parameter shaper_parameter;
	struct hns3_pf *pf = &hns->pf;
	uint32_t ir_u, ir_b, ir_s;
	uint32_t shaper_para;
	uint8_t i;
	int ret;

	/* Cfg pg schd */
	if (pf->tx_sch_mode != HNS3_FLAG_TC_BASE_SCH_MODE)
		return -EINVAL;

	/* Pg to pri */
	for (i = 0; i < hw->dcb_info.num_pg; i++) {
		/* Calc shaper para */
		ret = hns3_shaper_para_calc(hw,
					    hw->dcb_info.pg_info[i].bw_limit,
					    HNS3_SHAPER_LVL_PG,
					    &shaper_parameter);
		if (ret) {
			hns3_err(hw, "calculate shaper parameter failed: %d",
				 ret);
			return ret;
		}

		shaper_para = hns3_dcb_get_shapping_para(0, 0, 0,
							 HNS3_SHAPER_BS_U_DEF,
							 HNS3_SHAPER_BS_S_DEF);

		ret = hns3_dcb_pg_shapping_cfg(hw, HNS3_DCB_SHAP_C_BUCKET, i,
					       shaper_para);
		if (ret) {
			hns3_err(hw,
				 "config PG CIR shaper parameter failed: %d",
				 ret);
			return ret;
		}

		ir_b = shaper_parameter.ir_b;
		ir_u = shaper_parameter.ir_u;
		ir_s = shaper_parameter.ir_s;
		shaper_para = hns3_dcb_get_shapping_para(ir_b, ir_u, ir_s,
							 HNS3_SHAPER_BS_U_DEF,
							 HNS3_SHAPER_BS_S_DEF);

		ret = hns3_dcb_pg_shapping_cfg(hw, HNS3_DCB_SHAP_P_BUCKET, i,
					       shaper_para);
		if (ret) {
			hns3_err(hw,
				 "config PG PIR shaper parameter failed: %d",
				 ret);
			return ret;
		}
	}

	return 0;
}

static int
hns3_dcb_qs_schd_mode_cfg(struct hns3_hw *hw, uint16_t qs_id, uint8_t mode)
{
	struct hns3_cmd_desc desc;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_TM_QS_SCH_MODE_CFG, false);

	if (mode == HNS3_SCH_MODE_DWRR)
		desc.data[1] = rte_cpu_to_le_32(HNS3_DCB_TX_SCHD_DWRR_MSK);
	else
		desc.data[1] = 0;

	desc.data[0] = rte_cpu_to_le_32(qs_id);

	return hns3_cmd_send(hw, &desc, 1);
}

static int
hns3_dcb_pri_schd_mode_cfg(struct hns3_hw *hw, uint8_t pri_id)
{
	struct hns3_cmd_desc desc;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_TM_PRI_SCH_MODE_CFG, false);

	if (hw->dcb_info.tc_info[pri_id].tc_sch_mode == HNS3_SCH_MODE_DWRR)
		desc.data[1] = rte_cpu_to_le_32(HNS3_DCB_TX_SCHD_DWRR_MSK);
	else
		desc.data[1] = 0;

	desc.data[0] = rte_cpu_to_le_32(pri_id);

	return hns3_cmd_send(hw, &desc, 1);
}

static int
hns3_dcb_pri_shapping_cfg(struct hns3_hw *hw, enum hns3_shap_bucket bucket,
			  uint8_t pri_id, uint32_t shapping_para)
{
	struct hns3_pri_shapping_cmd *shap_cfg_cmd;
	enum hns3_opcode_type opcode;
	struct hns3_cmd_desc desc;

	opcode = bucket ? HNS3_OPC_TM_PRI_P_SHAPPING :
		 HNS3_OPC_TM_PRI_C_SHAPPING;

	hns3_cmd_setup_basic_desc(&desc, opcode, false);

	shap_cfg_cmd = (struct hns3_pri_shapping_cmd *)desc.data;

	shap_cfg_cmd->pri_id = pri_id;

	shap_cfg_cmd->pri_shapping_para = rte_cpu_to_le_32(shapping_para);

	return hns3_cmd_send(hw, &desc, 1);
}

static int
hns3_dcb_pri_tc_base_shaper_cfg(struct hns3_hw *hw)
{
	struct hns3_shaper_parameter shaper_parameter;
	uint32_t ir_u, ir_b, ir_s;
	uint32_t shaper_para;
	int ret, i;

	for (i = 0; i < hw->dcb_info.num_tc; i++) {
		ret = hns3_shaper_para_calc(hw,
					    hw->dcb_info.tc_info[i].bw_limit,
					    HNS3_SHAPER_LVL_PRI,
					    &shaper_parameter);
		if (ret) {
			hns3_err(hw, "calculate shaper parameter failed: %d",
				 ret);
			return ret;
		}

		shaper_para = hns3_dcb_get_shapping_para(0, 0, 0,
							 HNS3_SHAPER_BS_U_DEF,
							 HNS3_SHAPER_BS_S_DEF);

		ret = hns3_dcb_pri_shapping_cfg(hw, HNS3_DCB_SHAP_C_BUCKET, i,
						shaper_para);
		if (ret) {
			hns3_err(hw,
				 "config priority CIR shaper parameter failed: %d",
				 ret);
			return ret;
		}

		ir_b = shaper_parameter.ir_b;
		ir_u = shaper_parameter.ir_u;
		ir_s = shaper_parameter.ir_s;
		shaper_para = hns3_dcb_get_shapping_para(ir_b, ir_u, ir_s,
							 HNS3_SHAPER_BS_U_DEF,
							 HNS3_SHAPER_BS_S_DEF);

		ret = hns3_dcb_pri_shapping_cfg(hw, HNS3_DCB_SHAP_P_BUCKET, i,
						shaper_para);
		if (ret) {
			hns3_err(hw,
				 "config priority PIR shaper parameter failed: %d",
				 ret);
			return ret;
		}
	}

	return 0;
}


static int
hns3_dcb_pri_shaper_cfg(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	int ret;

	if (pf->tx_sch_mode != HNS3_FLAG_TC_BASE_SCH_MODE)
		return -EINVAL;

	ret = hns3_dcb_pri_tc_base_shaper_cfg(hw);
	if (ret)
		hns3_err(hw, "config port shaper failed: %d", ret);

	return ret;
}

void
hns3_set_rss_size(struct hns3_hw *hw, uint16_t nb_rx_q)
{
	struct hns3_rss_conf *rss_cfg = &hw->rss_info;
	uint16_t rx_qnum_per_tc;
	int i;

	rx_qnum_per_tc = nb_rx_q / hw->num_tc;
	rx_qnum_per_tc = RTE_MIN(hw->rss_size_max, rx_qnum_per_tc);
	if (hw->alloc_rss_size != rx_qnum_per_tc) {
		hns3_info(hw, "rss size changes from %u to %u",
			  hw->alloc_rss_size, rx_qnum_per_tc);
		hw->alloc_rss_size = rx_qnum_per_tc;
	}
	hw->used_rx_queues = hw->num_tc * hw->alloc_rss_size;

	/*
	 * When rss size is changed, we need to update rss redirection table
	 * maintained by driver. Besides, during the entire reset process, we
	 * need to ensure that the rss table information are not overwritten
	 * and configured directly to the hardware in the RESET_STAGE_RESTORE
	 * stage of the reset process.
	 */
	if (rte_atomic16_read(&hw->reset.resetting) == 0) {
		for (i = 0; i < HNS3_RSS_IND_TBL_SIZE; i++)
			rss_cfg->rss_indirection_tbl[i] =
							i % hw->alloc_rss_size;
	}
}

void
hns3_tc_queue_mapping_cfg(struct hns3_hw *hw, uint16_t nb_queue)
{
	struct hns3_tc_queue_info *tc_queue;
	uint8_t i;

	hw->tx_qnum_per_tc = nb_queue / hw->num_tc;
	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		tc_queue = &hw->tc_queue[i];
		if (hw->hw_tc_map & BIT(i) && i < hw->num_tc) {
			tc_queue->enable = true;
			tc_queue->tqp_offset = i * hw->tx_qnum_per_tc;
			tc_queue->tqp_count = hw->tx_qnum_per_tc;
			tc_queue->tc = i;
		} else {
			/* Set to default queue if TC is disable */
			tc_queue->enable = false;
			tc_queue->tqp_offset = 0;
			tc_queue->tqp_count = 0;
			tc_queue->tc = 0;
		}
	}
	hw->used_tx_queues = hw->num_tc * hw->tx_qnum_per_tc;
}

static void
hns3_dcb_update_tc_queue_mapping(struct hns3_hw *hw, uint16_t nb_rx_q,
				 uint16_t nb_tx_q)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;

	hw->num_tc = hw->dcb_info.num_tc;
	hns3_set_rss_size(hw, nb_rx_q);
	hns3_tc_queue_mapping_cfg(hw, nb_tx_q);

	if (!hns->is_vf)
		memcpy(pf->prio_tc, hw->dcb_info.prio_tc, HNS3_MAX_USER_PRIO);
}

int
hns3_dcb_info_init(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	int i, k;

	if (pf->tx_sch_mode != HNS3_FLAG_TC_BASE_SCH_MODE &&
	    hw->dcb_info.num_pg != 1)
		return -EINVAL;

	/* Initializing PG information */
	memset(hw->dcb_info.pg_info, 0,
	       sizeof(struct hns3_pg_info) * HNS3_PG_NUM);
	for (i = 0; i < hw->dcb_info.num_pg; i++) {
		hw->dcb_info.pg_dwrr[i] = i ? 0 : BW_MAX_PERCENT;
		hw->dcb_info.pg_info[i].pg_id = i;
		hw->dcb_info.pg_info[i].pg_sch_mode = HNS3_SCH_MODE_DWRR;
		hw->dcb_info.pg_info[i].bw_limit = HNS3_ETHER_MAX_RATE;

		if (i != 0)
			continue;

		hw->dcb_info.pg_info[i].tc_bit_map = hw->hw_tc_map;
		for (k = 0; k < hw->dcb_info.num_tc; k++)
			hw->dcb_info.pg_info[i].tc_dwrr[k] = BW_MAX_PERCENT;
	}

	/* All UPs mapping to TC0 */
	for (i = 0; i < HNS3_MAX_USER_PRIO; i++)
		hw->dcb_info.prio_tc[i] = 0;

	/* Initializing tc information */
	memset(hw->dcb_info.tc_info, 0,
	       sizeof(struct hns3_tc_info) * HNS3_MAX_TC_NUM);
	for (i = 0; i < hw->dcb_info.num_tc; i++) {
		hw->dcb_info.tc_info[i].tc_id = i;
		hw->dcb_info.tc_info[i].tc_sch_mode = HNS3_SCH_MODE_DWRR;
		hw->dcb_info.tc_info[i].pgid = 0;
		hw->dcb_info.tc_info[i].bw_limit =
			hw->dcb_info.pg_info[0].bw_limit;
	}

	return 0;
}

static int
hns3_dcb_lvl2_schd_mode_cfg(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	int ret, i;

	/* Only being config on TC-Based scheduler mode */
	if (pf->tx_sch_mode == HNS3_FLAG_VNET_BASE_SCH_MODE)
		return -EINVAL;

	for (i = 0; i < hw->dcb_info.num_pg; i++) {
		ret = hns3_dcb_pg_schd_mode_cfg(hw, i);
		if (ret)
			return ret;
	}

	return 0;
}

static int
hns3_dcb_lvl34_schd_mode_cfg(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	uint8_t i;
	int ret;

	if (pf->tx_sch_mode == HNS3_FLAG_TC_BASE_SCH_MODE) {
		for (i = 0; i < hw->dcb_info.num_tc; i++) {
			ret = hns3_dcb_pri_schd_mode_cfg(hw, i);
			if (ret)
				return ret;

			ret = hns3_dcb_qs_schd_mode_cfg(hw, i,
							HNS3_SCH_MODE_DWRR);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int
hns3_dcb_schd_mode_cfg(struct hns3_hw *hw)
{
	int ret;

	ret = hns3_dcb_lvl2_schd_mode_cfg(hw);
	if (ret) {
		hns3_err(hw, "config lvl2_schd_mode failed: %d", ret);
		return ret;
	}

	ret = hns3_dcb_lvl34_schd_mode_cfg(hw);
	if (ret)
		hns3_err(hw, "config lvl34_schd_mode failed: %d", ret);

	return ret;
}

static int
hns3_dcb_pri_tc_base_dwrr_cfg(struct hns3_hw *hw)
{
	struct hns3_pg_info *pg_info;
	uint8_t dwrr;
	int ret, i;

	for (i = 0; i < hw->dcb_info.num_tc; i++) {
		pg_info = &hw->dcb_info.pg_info[hw->dcb_info.tc_info[i].pgid];
		dwrr = pg_info->tc_dwrr[i];

		ret = hns3_dcb_pri_weight_cfg(hw, i, dwrr);
		if (ret) {
			hns3_err(hw,
			       "fail to send priority weight cmd: %d, ret = %d",
			       i, ret);
			return ret;
		}

		ret = hns3_dcb_qs_weight_cfg(hw, i, BW_MAX_PERCENT);
		if (ret) {
			hns3_err(hw, "fail to send qs_weight cmd: %d, ret = %d",
				 i, ret);
			return ret;
		}
	}

	return 0;
}

static int
hns3_dcb_pri_dwrr_cfg(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	uint32_t version;
	int ret;

	if (pf->tx_sch_mode != HNS3_FLAG_TC_BASE_SCH_MODE)
		return -EINVAL;

	ret = hns3_dcb_pri_tc_base_dwrr_cfg(hw);
	if (ret)
		return ret;

	if (!hns3_dev_dcb_supported(hw))
		return 0;

	ret = hns3_dcb_ets_tc_dwrr_cfg(hw);
	if (ret == -EOPNOTSUPP) {
		version = hw->fw_version;
		hns3_warn(hw,
			  "fw %lu.%lu.%lu.%lu doesn't support ets tc weight cmd",
			  hns3_get_field(version, HNS3_FW_VERSION_BYTE3_M,
					 HNS3_FW_VERSION_BYTE3_S),
			  hns3_get_field(version, HNS3_FW_VERSION_BYTE2_M,
					 HNS3_FW_VERSION_BYTE2_S),
			  hns3_get_field(version, HNS3_FW_VERSION_BYTE1_M,
					 HNS3_FW_VERSION_BYTE1_S),
			  hns3_get_field(version, HNS3_FW_VERSION_BYTE0_M,
					 HNS3_FW_VERSION_BYTE0_S));
		ret = 0;
	}

	return ret;
}

static int
hns3_dcb_pg_dwrr_cfg(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	int ret, i;

	/* Cfg pg schd */
	if (pf->tx_sch_mode != HNS3_FLAG_TC_BASE_SCH_MODE)
		return -EINVAL;

	/* Cfg pg to prio */
	for (i = 0; i < hw->dcb_info.num_pg; i++) {
		/* Cfg dwrr */
		ret = hns3_dcb_pg_weight_cfg(hw, i, hw->dcb_info.pg_dwrr[i]);
		if (ret)
			return ret;
	}

	return 0;
}

static int
hns3_dcb_dwrr_cfg(struct hns3_hw *hw)
{
	int ret;

	ret = hns3_dcb_pg_dwrr_cfg(hw);
	if (ret) {
		hns3_err(hw, "config pg_dwrr failed: %d", ret);
		return ret;
	}

	ret = hns3_dcb_pri_dwrr_cfg(hw);
	if (ret)
		hns3_err(hw, "config pri_dwrr failed: %d", ret);

	return ret;
}

static int
hns3_dcb_shaper_cfg(struct hns3_hw *hw)
{
	int ret;

	ret = hns3_dcb_port_shaper_cfg(hw);
	if (ret) {
		hns3_err(hw, "config port shaper failed: %d", ret);
		return ret;
	}

	ret = hns3_dcb_pg_shaper_cfg(hw);
	if (ret) {
		hns3_err(hw, "config pg shaper failed: %d", ret);
		return ret;
	}

	return hns3_dcb_pri_shaper_cfg(hw);
}

static int
hns3_q_to_qs_map_cfg(struct hns3_hw *hw, uint16_t q_id, uint16_t qs_id)
{
	struct hns3_nq_to_qs_link_cmd *map;
	struct hns3_cmd_desc desc;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_TM_NQ_TO_QS_LINK, false);

	map = (struct hns3_nq_to_qs_link_cmd *)desc.data;

	map->nq_id = rte_cpu_to_le_16(q_id);
	map->qset_id = rte_cpu_to_le_16(qs_id | HNS3_DCB_Q_QS_LINK_VLD_MSK);

	return hns3_cmd_send(hw, &desc, 1);
}

static int
hns3_q_to_qs_map(struct hns3_hw *hw)
{
	struct hns3_tc_queue_info *tc_queue;
	uint16_t q_id;
	uint32_t i, j;
	int ret;

	for (i = 0; i < hw->num_tc; i++) {
		tc_queue = &hw->tc_queue[i];
		for (j = 0; j < tc_queue->tqp_count; j++) {
			q_id = tc_queue->tqp_offset + j;
			ret = hns3_q_to_qs_map_cfg(hw, q_id, i);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int
hns3_pri_q_qs_cfg(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	uint32_t i;
	int ret;

	if (pf->tx_sch_mode != HNS3_FLAG_TC_BASE_SCH_MODE)
		return -EINVAL;

	/* Cfg qs -> pri mapping */
	for (i = 0; i < hw->num_tc; i++) {
		ret = hns3_qs_to_pri_map_cfg(hw, i, i);
		if (ret) {
			hns3_err(hw, "qs_to_pri mapping fail: %d", ret);
			return ret;
		}
	}

	/* Cfg q -> qs mapping */
	ret = hns3_q_to_qs_map(hw);
	if (ret)
		hns3_err(hw, "nq_to_qs mapping fail: %d", ret);

	return ret;
}

static int
hns3_dcb_map_cfg(struct hns3_hw *hw)
{
	int ret;

	ret = hns3_up_to_tc_map(hw);
	if (ret) {
		hns3_err(hw, "up_to_tc mapping fail: %d", ret);
		return ret;
	}

	ret = hns3_pg_to_pri_map(hw);
	if (ret) {
		hns3_err(hw, "pri_to_pg mapping fail: %d", ret);
		return ret;
	}

	return hns3_pri_q_qs_cfg(hw);
}

static int
hns3_dcb_schd_setup_hw(struct hns3_hw *hw)
{
	int ret;

	/* Cfg dcb mapping  */
	ret = hns3_dcb_map_cfg(hw);
	if (ret)
		return ret;

	/* Cfg dcb shaper */
	ret = hns3_dcb_shaper_cfg(hw);
	if (ret)
		return ret;

	/* Cfg dwrr */
	ret = hns3_dcb_dwrr_cfg(hw);
	if (ret)
		return ret;

	/* Cfg schd mode for each level schd */
	return hns3_dcb_schd_mode_cfg(hw);
}

static int
hns3_pause_param_cfg(struct hns3_hw *hw, const uint8_t *addr,
		     uint8_t pause_trans_gap, uint16_t pause_trans_time)
{
	struct hns3_cfg_pause_param_cmd *pause_param;
	struct hns3_cmd_desc desc;

	pause_param = (struct hns3_cfg_pause_param_cmd *)desc.data;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_CFG_MAC_PARA, false);

	memcpy(pause_param->mac_addr, addr, RTE_ETHER_ADDR_LEN);
	memcpy(pause_param->mac_addr_extra, addr, RTE_ETHER_ADDR_LEN);
	pause_param->pause_trans_gap = pause_trans_gap;
	pause_param->pause_trans_time = rte_cpu_to_le_16(pause_trans_time);

	return hns3_cmd_send(hw, &desc, 1);
}

int
hns3_pause_addr_cfg(struct hns3_hw *hw, const uint8_t *mac_addr)
{
	struct hns3_cfg_pause_param_cmd *pause_param;
	struct hns3_cmd_desc desc;
	uint16_t trans_time;
	uint8_t trans_gap;
	int ret;

	pause_param = (struct hns3_cfg_pause_param_cmd *)desc.data;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_CFG_MAC_PARA, true);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		return ret;

	trans_gap = pause_param->pause_trans_gap;
	trans_time = rte_le_to_cpu_16(pause_param->pause_trans_time);

	return hns3_pause_param_cfg(hw, mac_addr, trans_gap, trans_time);
}

static int
hns3_pause_param_setup_hw(struct hns3_hw *hw, uint16_t pause_time)
{
#define PAUSE_TIME_DIV_BY	2
#define PAUSE_TIME_MIN_VALUE	0x4

	struct hns3_mac *mac = &hw->mac;
	uint8_t pause_trans_gap;

	/*
	 * Pause transmit gap must be less than "pause_time / 2", otherwise
	 * the behavior of MAC is undefined.
	 */
	if (pause_time > PAUSE_TIME_DIV_BY * HNS3_DEFAULT_PAUSE_TRANS_GAP)
		pause_trans_gap = HNS3_DEFAULT_PAUSE_TRANS_GAP;
	else if (pause_time >= PAUSE_TIME_MIN_VALUE &&
		 pause_time <= PAUSE_TIME_DIV_BY * HNS3_DEFAULT_PAUSE_TRANS_GAP)
		pause_trans_gap = pause_time / PAUSE_TIME_DIV_BY - 1;
	else {
		hns3_warn(hw, "pause_time(%d) is adjusted to 4", pause_time);
		pause_time = PAUSE_TIME_MIN_VALUE;
		pause_trans_gap = pause_time / PAUSE_TIME_DIV_BY - 1;
	}

	return hns3_pause_param_cfg(hw, mac->mac_addr,
				    pause_trans_gap, pause_time);
}

static int
hns3_mac_pause_en_cfg(struct hns3_hw *hw, bool tx, bool rx)
{
	struct hns3_cmd_desc desc;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_CFG_MAC_PAUSE_EN, false);

	desc.data[0] = rte_cpu_to_le_32((tx ? HNS3_TX_MAC_PAUSE_EN_MSK : 0) |
		(rx ? HNS3_RX_MAC_PAUSE_EN_MSK : 0));

	return hns3_cmd_send(hw, &desc, 1);
}

static int
hns3_pfc_pause_en_cfg(struct hns3_hw *hw, uint8_t pfc_bitmap, bool tx, bool rx)
{
	struct hns3_cmd_desc desc;
	struct hns3_pfc_en_cmd *pfc = (struct hns3_pfc_en_cmd *)desc.data;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_CFG_PFC_PAUSE_EN, false);

	pfc->tx_rx_en_bitmap = (uint8_t)((tx ? HNS3_TX_MAC_PAUSE_EN_MSK : 0) |
					(rx ? HNS3_RX_MAC_PAUSE_EN_MSK : 0));

	pfc->pri_en_bitmap = pfc_bitmap;

	return hns3_cmd_send(hw, &desc, 1);
}

static int
hns3_qs_bp_cfg(struct hns3_hw *hw, uint8_t tc, uint8_t grp_id, uint32_t bit_map)
{
	struct hns3_bp_to_qs_map_cmd *bp_to_qs_map_cmd;
	struct hns3_cmd_desc desc;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_TM_BP_TO_QSET_MAPPING, false);

	bp_to_qs_map_cmd = (struct hns3_bp_to_qs_map_cmd *)desc.data;

	bp_to_qs_map_cmd->tc_id = tc;
	bp_to_qs_map_cmd->qs_group_id = grp_id;
	bp_to_qs_map_cmd->qs_bit_map = rte_cpu_to_le_32(bit_map);

	return hns3_cmd_send(hw, &desc, 1);
}

static void
hns3_get_rx_tx_en_status(struct hns3_hw *hw, bool *tx_en, bool *rx_en)
{
	switch (hw->current_mode) {
	case HNS3_FC_NONE:
		*tx_en = false;
		*rx_en = false;
		break;
	case HNS3_FC_RX_PAUSE:
		*tx_en = false;
		*rx_en = true;
		break;
	case HNS3_FC_TX_PAUSE:
		*tx_en = true;
		*rx_en = false;
		break;
	case HNS3_FC_FULL:
		*tx_en = true;
		*rx_en = true;
		break;
	default:
		*tx_en = false;
		*rx_en = false;
		break;
	}
}

static int
hns3_mac_pause_setup_hw(struct hns3_hw *hw)
{
	bool tx_en, rx_en;

	if (hw->current_fc_status == HNS3_FC_STATUS_MAC_PAUSE)
		hns3_get_rx_tx_en_status(hw, &tx_en, &rx_en);
	else {
		tx_en = false;
		rx_en = false;
	}

	return hns3_mac_pause_en_cfg(hw, tx_en, rx_en);
}

static int
hns3_pfc_setup_hw(struct hns3_hw *hw)
{
	bool tx_en, rx_en;

	if (hw->current_fc_status == HNS3_FC_STATUS_PFC)
		hns3_get_rx_tx_en_status(hw, &tx_en, &rx_en);
	else {
		tx_en = false;
		rx_en = false;
	}

	return hns3_pfc_pause_en_cfg(hw, hw->dcb_info.pfc_en, tx_en, rx_en);
}

/*
 * Each Tc has a 1024 queue sets to backpress, it divides to
 * 32 group, each group contains 32 queue sets, which can be
 * represented by uint32_t bitmap.
 */
static int
hns3_bp_setup_hw(struct hns3_hw *hw, uint8_t tc)
{
	uint32_t qs_bitmap;
	int ret;
	int i;

	for (i = 0; i < HNS3_BP_GRP_NUM; i++) {
		uint8_t grp, sub_grp;
		qs_bitmap = 0;

		grp = hns3_get_field(tc, HNS3_BP_GRP_ID_M, HNS3_BP_GRP_ID_S);
		sub_grp = hns3_get_field(tc, HNS3_BP_SUB_GRP_ID_M,
					 HNS3_BP_SUB_GRP_ID_S);
		if (i == grp)
			qs_bitmap |= (1 << sub_grp);

		ret = hns3_qs_bp_cfg(hw, tc, i, qs_bitmap);
		if (ret)
			return ret;
	}

	return 0;
}

static int
hns3_dcb_bp_setup(struct hns3_hw *hw)
{
	int ret, i;

	for (i = 0; i < hw->dcb_info.num_tc; i++) {
		ret = hns3_bp_setup_hw(hw, i);
		if (ret)
			return ret;
	}

	return 0;
}

static int
hns3_dcb_pause_setup_hw(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	int ret;

	ret = hns3_pause_param_setup_hw(hw, pf->pause_time);
	if (ret) {
		hns3_err(hw, "Fail to set pause parameter. ret = %d", ret);
		return ret;
	}

	ret = hns3_mac_pause_setup_hw(hw);
	if (ret) {
		hns3_err(hw, "Fail to setup MAC pause. ret = %d", ret);
		return ret;
	}

	/* Only DCB-supported dev supports qset back pressure and pfc cmd */
	if (!hns3_dev_dcb_supported(hw))
		return 0;

	ret = hns3_pfc_setup_hw(hw);
	if (ret) {
		hns3_err(hw, "config pfc failed! ret = %d", ret);
		return ret;
	}

	return hns3_dcb_bp_setup(hw);
}

static uint8_t
hns3_dcb_undrop_tc_map(struct hns3_hw *hw, uint8_t pfc_en)
{
	uint8_t pfc_map = 0;
	uint8_t *prio_tc;
	uint8_t i, j;

	prio_tc = hw->dcb_info.prio_tc;
	for (i = 0; i < hw->dcb_info.num_tc; i++) {
		for (j = 0; j < HNS3_MAX_USER_PRIO; j++) {
			if (prio_tc[j] == i && pfc_en & BIT(j)) {
				pfc_map |= BIT(i);
				break;
			}
		}
	}

	return pfc_map;
}

static void
hns3_dcb_cfg_validate(struct hns3_adapter *hns, uint8_t *tc, bool *changed)
{
	struct rte_eth_dcb_rx_conf *dcb_rx_conf;
	struct hns3_hw *hw = &hns->hw;
	uint8_t max_tc = 0;
	uint8_t pfc_en;
	int i;

	dcb_rx_conf = &hw->data->dev_conf.rx_adv_conf.dcb_rx_conf;
	for (i = 0; i < HNS3_MAX_USER_PRIO; i++) {
		if (dcb_rx_conf->dcb_tc[i] != hw->dcb_info.prio_tc[i])
			*changed = true;

		if (dcb_rx_conf->dcb_tc[i] > max_tc)
			max_tc = dcb_rx_conf->dcb_tc[i];
	}
	*tc = max_tc + 1;
	if (*tc != hw->dcb_info.num_tc)
		*changed = true;

	/*
	 * We ensure that dcb information can be reconfigured
	 * after the hns3_priority_flow_ctrl_set function called.
	 */
	if (hw->current_mode != HNS3_FC_FULL)
		*changed = true;
	pfc_en = RTE_LEN2MASK((uint8_t)dcb_rx_conf->nb_tcs, uint8_t);
	if (hw->dcb_info.pfc_en != pfc_en)
		*changed = true;
}

static void
hns3_dcb_info_cfg(struct hns3_adapter *hns)
{
	struct rte_eth_dcb_rx_conf *dcb_rx_conf;
	struct hns3_pf *pf = &hns->pf;
	struct hns3_hw *hw = &hns->hw;
	uint8_t tc_bw, bw_rest;
	uint8_t i, j;

	dcb_rx_conf = &hw->data->dev_conf.rx_adv_conf.dcb_rx_conf;
	pf->local_max_tc = (uint8_t)dcb_rx_conf->nb_tcs;
	pf->pfc_max = (uint8_t)dcb_rx_conf->nb_tcs;

	/* Config pg0 */
	memset(hw->dcb_info.pg_info, 0,
	       sizeof(struct hns3_pg_info) * HNS3_PG_NUM);
	hw->dcb_info.pg_dwrr[0] = BW_MAX_PERCENT;
	hw->dcb_info.pg_info[0].pg_id = 0;
	hw->dcb_info.pg_info[0].pg_sch_mode = HNS3_SCH_MODE_DWRR;
	hw->dcb_info.pg_info[0].bw_limit = HNS3_ETHER_MAX_RATE;
	hw->dcb_info.pg_info[0].tc_bit_map = hw->hw_tc_map;

	/* Each tc has same bw for valid tc by default */
	tc_bw = BW_MAX_PERCENT / hw->dcb_info.num_tc;
	for (i = 0; i < hw->dcb_info.num_tc; i++)
		hw->dcb_info.pg_info[0].tc_dwrr[i] = tc_bw;
	/* To ensure the sum of tc_dwrr is equal to 100 */
	bw_rest = BW_MAX_PERCENT % hw->dcb_info.num_tc;
	for (j = 0; j < bw_rest; j++)
		hw->dcb_info.pg_info[0].tc_dwrr[j]++;
	for (; i < dcb_rx_conf->nb_tcs; i++)
		hw->dcb_info.pg_info[0].tc_dwrr[i] = 0;

	/* All tcs map to pg0 */
	memset(hw->dcb_info.tc_info, 0,
	       sizeof(struct hns3_tc_info) * HNS3_MAX_TC_NUM);
	for (i = 0; i < hw->dcb_info.num_tc; i++) {
		hw->dcb_info.tc_info[i].tc_id = i;
		hw->dcb_info.tc_info[i].tc_sch_mode = HNS3_SCH_MODE_DWRR;
		hw->dcb_info.tc_info[i].pgid = 0;
		hw->dcb_info.tc_info[i].bw_limit =
					hw->dcb_info.pg_info[0].bw_limit;
	}

	for (i = 0; i < HNS3_MAX_USER_PRIO; i++)
		hw->dcb_info.prio_tc[i] = dcb_rx_conf->dcb_tc[i];

	hns3_dcb_update_tc_queue_mapping(hw, hw->data->nb_rx_queues,
					 hw->data->nb_tx_queues);
}

static int
hns3_dcb_info_update(struct hns3_adapter *hns, uint8_t num_tc)
{
	struct hns3_pf *pf = &hns->pf;
	struct hns3_hw *hw = &hns->hw;
	uint16_t nb_rx_q = hw->data->nb_rx_queues;
	uint16_t nb_tx_q = hw->data->nb_tx_queues;
	uint8_t bit_map = 0;
	uint8_t i;

	if (pf->tx_sch_mode != HNS3_FLAG_TC_BASE_SCH_MODE &&
	    hw->dcb_info.num_pg != 1)
		return -EINVAL;

	if (nb_rx_q < num_tc) {
		hns3_err(hw, "number of Rx queues(%d) is less than tcs(%d).",
			 nb_rx_q, num_tc);
		return -EINVAL;
	}

	if (nb_tx_q < num_tc) {
		hns3_err(hw, "number of Tx queues(%d) is less than tcs(%d).",
			 nb_tx_q, num_tc);
		return -EINVAL;
	}

	/* Currently not support uncontinuous tc */
	hw->dcb_info.num_tc = num_tc;
	for (i = 0; i < hw->dcb_info.num_tc; i++)
		bit_map |= BIT(i);

	if (!bit_map) {
		bit_map = 1;
		hw->dcb_info.num_tc = 1;
	}
	hw->hw_tc_map = bit_map;
	hns3_dcb_info_cfg(hns);

	return 0;
}

static int
hns3_dcb_hw_configure(struct hns3_adapter *hns)
{
	struct rte_eth_dcb_rx_conf *dcb_rx_conf;
	struct hns3_pf *pf = &hns->pf;
	struct hns3_hw *hw = &hns->hw;
	enum hns3_fc_status fc_status = hw->current_fc_status;
	enum hns3_fc_mode current_mode = hw->current_mode;
	uint8_t hw_pfc_map = hw->dcb_info.hw_pfc_map;
	int ret, status;

	if (pf->tx_sch_mode != HNS3_FLAG_TC_BASE_SCH_MODE &&
	    pf->tx_sch_mode != HNS3_FLAG_VNET_BASE_SCH_MODE)
		return -ENOTSUP;

	ret = hns3_dcb_schd_setup_hw(hw);
	if (ret) {
		hns3_err(hw, "dcb schdule configure failed! ret = %d", ret);
		return ret;
	}

	if (hw->data->dev_conf.dcb_capability_en & ETH_DCB_PFC_SUPPORT) {
		dcb_rx_conf = &hw->data->dev_conf.rx_adv_conf.dcb_rx_conf;
		if (dcb_rx_conf->nb_tcs == 0)
			hw->dcb_info.pfc_en = 1; /* tc0 only */
		else
			hw->dcb_info.pfc_en =
			RTE_LEN2MASK((uint8_t)dcb_rx_conf->nb_tcs, uint8_t);

		hw->dcb_info.hw_pfc_map =
				hns3_dcb_undrop_tc_map(hw, hw->dcb_info.pfc_en);

		ret = hns3_buffer_alloc(hw);
		if (ret)
			return ret;

		hw->current_fc_status = HNS3_FC_STATUS_PFC;
		hw->current_mode = HNS3_FC_FULL;
		ret = hns3_dcb_pause_setup_hw(hw);
		if (ret) {
			hns3_err(hw, "setup pfc failed! ret = %d", ret);
			goto pfc_setup_fail;
		}
	} else {
		/*
		 * Although dcb_capability_en is lack of ETH_DCB_PFC_SUPPORT
		 * flag, the DCB information is configured, such as tc numbers.
		 * Therefore, refreshing the allocation of packet buffer is
		 * necessary.
		 */
		ret = hns3_buffer_alloc(hw);
		if (ret)
			return ret;
	}

	return 0;

pfc_setup_fail:
	hw->current_mode = current_mode;
	hw->current_fc_status = fc_status;
	hw->dcb_info.hw_pfc_map = hw_pfc_map;
	status = hns3_buffer_alloc(hw);
	if (status)
		hns3_err(hw, "recover packet buffer fail! status = %d", status);

	return ret;
}

/*
 * hns3_dcb_configure - setup dcb related config
 * @hns: pointer to hns3 adapter
 * Returns 0 on success, negative value on failure.
 */
int
hns3_dcb_configure(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	bool map_changed = false;
	uint8_t num_tc = 0;
	int ret;

	hns3_dcb_cfg_validate(hns, &num_tc, &map_changed);
	if (map_changed || rte_atomic16_read(&hw->reset.resetting)) {
		ret = hns3_dcb_info_update(hns, num_tc);
		if (ret) {
			hns3_err(hw, "dcb info update failed: %d", ret);
			return ret;
		}

		ret = hns3_dcb_hw_configure(hns);
		if (ret) {
			hns3_err(hw, "dcb sw configure failed: %d", ret);
			return ret;
		}
	}

	return 0;
}

int
hns3_dcb_init_hw(struct hns3_hw *hw)
{
	int ret;

	ret = hns3_dcb_schd_setup_hw(hw);
	if (ret) {
		hns3_err(hw, "dcb schedule setup failed: %d", ret);
		return ret;
	}

	ret = hns3_dcb_pause_setup_hw(hw);
	if (ret)
		hns3_err(hw, "PAUSE setup failed: %d", ret);

	return ret;
}

int
hns3_dcb_init(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	int ret;

	PMD_INIT_FUNC_TRACE();

	/*
	 * According to the 'adapter_state' identifier, the following branch
	 * is only executed to initialize default configurations of dcb during
	 * the initializing driver process. Due to driver saving dcb-related
	 * information before reset triggered, the reinit dev stage of the
	 * reset process can not access to the branch, or those information
	 * will be changed.
	 */
	if (hw->adapter_state == HNS3_NIC_UNINITIALIZED) {
		hw->requested_mode = HNS3_FC_NONE;
		hw->current_mode = hw->requested_mode;
		pf->pause_time = HNS3_DEFAULT_PAUSE_TRANS_TIME;
		hw->current_fc_status = HNS3_FC_STATUS_NONE;

		ret = hns3_dcb_info_init(hw);
		if (ret) {
			hns3_err(hw, "dcb info init failed: %d", ret);
			return ret;
		}
		hns3_dcb_update_tc_queue_mapping(hw, hw->tqps_num,
						 hw->tqps_num);
	}

	/*
	 * DCB hardware will be configured by following the function during
	 * the initializing driver process and the reset process. However,
	 * driver will restore directly configurations of dcb hardware based
	 * on dcb-related information soft maintained when driver
	 * initialization has finished and reset is coming.
	 */
	ret = hns3_dcb_init_hw(hw);
	if (ret) {
		hns3_err(hw, "dcb init hardware failed: %d", ret);
		return ret;
	}

	return 0;
}

static int
hns3_update_queue_map_configure(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	uint16_t nb_rx_q = hw->data->nb_rx_queues;
	uint16_t nb_tx_q = hw->data->nb_tx_queues;
	int ret;

	hns3_dcb_update_tc_queue_mapping(hw, nb_rx_q, nb_tx_q);
	ret = hns3_q_to_qs_map(hw);
	if (ret)
		hns3_err(hw, "failed to map nq to qs! ret = %d", ret);

	return ret;
}

int
hns3_dcb_cfg_update(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	enum rte_eth_rx_mq_mode mq_mode = hw->data->dev_conf.rxmode.mq_mode;
	int ret;

	if ((uint32_t)mq_mode & ETH_MQ_RX_DCB_FLAG) {
		ret = hns3_dcb_configure(hns);
		if (ret)
			hns3_err(hw, "Failed to config dcb: %d", ret);
	} else {
		/*
		 * Update queue map without PFC configuration,
		 * due to queues reconfigured by user.
		 */
		ret = hns3_update_queue_map_configure(hns);
		if (ret)
			hns3_err(hw,
				 "Failed to update queue mapping configure: %d",
				 ret);
	}

	return ret;
}

/*
 * hns3_dcb_pfc_enable - Enable priority flow control
 * @dev: pointer to ethernet device
 *
 * Configures the pfc settings for one porority.
 */
int
hns3_dcb_pfc_enable(struct rte_eth_dev *dev, struct rte_eth_pfc_conf *pfc_conf)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	enum hns3_fc_status fc_status = hw->current_fc_status;
	enum hns3_fc_mode current_mode = hw->current_mode;
	uint8_t hw_pfc_map = hw->dcb_info.hw_pfc_map;
	uint8_t pfc_en = hw->dcb_info.pfc_en;
	uint8_t priority = pfc_conf->priority;
	uint16_t pause_time = pf->pause_time;
	int ret, status;

	pf->pause_time = pfc_conf->fc.pause_time;
	hw->current_mode = hw->requested_mode;
	hw->current_fc_status = HNS3_FC_STATUS_PFC;
	hw->dcb_info.pfc_en |= BIT(priority);
	hw->dcb_info.hw_pfc_map =
			hns3_dcb_undrop_tc_map(hw, hw->dcb_info.pfc_en);
	ret = hns3_buffer_alloc(hw);
	if (ret)
		goto pfc_setup_fail;

	/*
	 * The flow control mode of all UPs will be changed based on
	 * current_mode coming from user.
	 */
	ret = hns3_dcb_pause_setup_hw(hw);
	if (ret) {
		hns3_err(hw, "enable pfc failed! ret = %d", ret);
		goto pfc_setup_fail;
	}

	return 0;

pfc_setup_fail:
	hw->current_mode = current_mode;
	hw->current_fc_status = fc_status;
	pf->pause_time = pause_time;
	hw->dcb_info.pfc_en = pfc_en;
	hw->dcb_info.hw_pfc_map = hw_pfc_map;
	status = hns3_buffer_alloc(hw);
	if (status)
		hns3_err(hw, "recover packet buffer fail: %d", status);

	return ret;
}

/*
 * hns3_fc_enable - Enable MAC pause
 * @dev: pointer to ethernet device
 *
 * Configures the MAC pause settings.
 */
int
hns3_fc_enable(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	enum hns3_fc_status fc_status = hw->current_fc_status;
	enum hns3_fc_mode current_mode = hw->current_mode;
	uint16_t pause_time = pf->pause_time;
	int ret;

	pf->pause_time = fc_conf->pause_time;
	hw->current_mode = hw->requested_mode;

	/*
	 * In fact, current_fc_status is HNS3_FC_STATUS_NONE when mode
	 * of flow control is configured to be HNS3_FC_NONE.
	 */
	if (hw->current_mode == HNS3_FC_NONE)
		hw->current_fc_status = HNS3_FC_STATUS_NONE;
	else
		hw->current_fc_status = HNS3_FC_STATUS_MAC_PAUSE;

	ret = hns3_dcb_pause_setup_hw(hw);
	if (ret) {
		hns3_err(hw, "enable MAC Pause failed! ret = %d", ret);
		goto setup_fc_fail;
	}

	return 0;

setup_fc_fail:
	hw->current_mode = current_mode;
	hw->current_fc_status = fc_status;
	pf->pause_time = pause_time;

	return ret;
}
