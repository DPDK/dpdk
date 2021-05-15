/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#ifndef _HNS3_DCB_H_
#define _HNS3_DCB_H_

#include <stdint.h>

#include "hns3_cmd.h"

#define HNS3_ETHER_MAX_RATE		100000

/* MAC Pause */
#define HNS3_TX_MAC_PAUSE_EN_MSK	BIT(0)
#define HNS3_RX_MAC_PAUSE_EN_MSK	BIT(1)

#define HNS3_DEFAULT_PAUSE_TRANS_GAP	0x18
#define HNS3_DEFAULT_PAUSE_TRANS_TIME	0xFFFF

/* SP or DWRR */
#define HNS3_DCB_TX_SCHD_DWRR_MSK	BIT(0)
#define HNS3_DCB_TX_SCHD_SP_MSK		0xFE

enum hns3_shap_bucket {
	HNS3_DCB_SHAP_C_BUCKET = 0,
	HNS3_DCB_SHAP_P_BUCKET,
};

struct hns3_priority_weight_cmd {
	uint8_t pri_id;
	uint8_t dwrr;
	uint8_t rsvd[22];
};

struct hns3_qs_weight_cmd {
	uint16_t qs_id;
	uint8_t dwrr;
	uint8_t rsvd[21];
};

struct hns3_pg_weight_cmd {
	uint8_t pg_id;
	uint8_t dwrr;
	uint8_t rsvd[22];
};

struct hns3_ets_tc_weight_cmd {
	uint8_t tc_weight[HNS3_MAX_TC_NUM];
	uint8_t weight_offset;
	uint8_t rsvd[15];
};

struct hns3_qs_to_pri_link_cmd {
	uint16_t qs_id;
	uint16_t rsvd;
	uint8_t priority;
#define HNS3_DCB_QS_PRI_LINK_VLD_MSK	BIT(0)
#define HNS3_DCB_QS_ID_L_MSK		GENMASK(9, 0)
#define HNS3_DCB_QS_ID_L_S		0
#define HNS3_DCB_QS_ID_H_MSK		GENMASK(14, 10)
#define HNS3_DCB_QS_ID_H_S		10
#define HNS3_DCB_QS_ID_H_EXT_S		11
#define HNS3_DCB_QS_ID_H_EXT_MSK	GENMASK(15, 11)
	uint8_t link_vld;
	uint8_t rsvd1[18];
};

struct hns3_nq_to_qs_link_cmd {
	uint16_t nq_id;
	uint16_t rsvd;
#define HNS3_DCB_Q_QS_LINK_VLD_MSK	BIT(10)
	uint16_t qset_id;
	uint8_t rsvd1[18];
};

#define HNS3_DCB_SHAP_IR_B_MSK  GENMASK(7, 0)
#define HNS3_DCB_SHAP_IR_B_LSH	0
#define HNS3_DCB_SHAP_IR_U_MSK  GENMASK(11, 8)
#define HNS3_DCB_SHAP_IR_U_LSH	8
#define HNS3_DCB_SHAP_IR_S_MSK  GENMASK(15, 12)
#define HNS3_DCB_SHAP_IR_S_LSH	12
#define HNS3_DCB_SHAP_BS_B_MSK  GENMASK(20, 16)
#define HNS3_DCB_SHAP_BS_B_LSH	16
#define HNS3_DCB_SHAP_BS_S_MSK  GENMASK(25, 21)
#define HNS3_DCB_SHAP_BS_S_LSH	21

/*
 * For more flexible selection of shapping algorithm in different network
 * engine, the algorithm calculating shapping parameter is moved to firmware to
 * execute. Bit HNS3_TM_RATE_VLD_B of flag field in hns3_pri_shapping_cmd,
 * hns3_pg_shapping_cmd or hns3_port_shapping_cmd is set to 1 to require
 * firmware to recalculate shapping parameters. However, whether the parameters
 * are recalculated depends on the firmware version. If firmware doesn't support
 * the calculation of shapping parameters, such as on network engine with
 * revision id 0x21, the value driver calculated will be used to configure to
 * hardware. On the contrary, firmware ignores configuration of driver
 * and recalculates the parameter.
 */
#define HNS3_TM_RATE_VLD_B	0

struct hns3_pri_shapping_cmd {
	uint8_t pri_id;
	uint8_t rsvd[3];
	uint32_t pri_shapping_para;
	uint8_t flag;
	uint8_t rsvd1[3];
	uint32_t pri_rate;  /* Unit Mbps */
	uint8_t rsvd2[8];
};

struct hns3_pg_shapping_cmd {
	uint8_t pg_id;
	uint8_t rsvd[3];
	uint32_t pg_shapping_para;
	uint8_t flag;
	uint8_t rsvd1[3];
	uint32_t pg_rate; /* Unit Mbps */
	uint8_t rsvd2[8];
};

struct hns3_port_shapping_cmd {
	uint32_t port_shapping_para;
	uint8_t flag;
	uint8_t rsvd[3];
	uint32_t port_rate;   /* Unit Mbps */
	uint8_t rsvd1[12];
};

#define HNS3_BP_GRP_NUM			32
#define HNS3_BP_SUB_GRP_ID_S		0
#define HNS3_BP_SUB_GRP_ID_M		GENMASK(4, 0)
#define HNS3_BP_GRP_ID_S		5
#define HNS3_BP_GRP_ID_M		GENMASK(9, 5)

struct hns3_bp_to_qs_map_cmd {
	uint8_t tc_id;
	uint8_t rsvd[2];
	uint8_t qs_group_id;
	uint32_t qs_bit_map;
	uint32_t rsvd1[4];
};

struct hns3_pfc_en_cmd {
	uint8_t tx_rx_en_bitmap;
	uint8_t pri_en_bitmap;
	uint8_t rsvd[22];
};

struct hns3_cfg_pause_param_cmd {
	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];
	uint8_t pause_trans_gap;
	uint8_t rsvd;
	uint16_t pause_trans_time;
	uint8_t rsvd1[6];
	/* extra mac address to do double check for pause frame */
	uint8_t mac_addr_extra[RTE_ETHER_ADDR_LEN];
	uint16_t rsvd2;
};

struct hns3_pg_to_pri_link_cmd {
	uint8_t pg_id;
	uint8_t rsvd1[3];
	uint8_t pri_bit_map;
	uint8_t rsvd2[19];
};

enum hns3_shaper_level {
	HNS3_SHAPER_LVL_PRI	= 0,
	HNS3_SHAPER_LVL_PG	= 1,
	HNS3_SHAPER_LVL_PORT	= 2,
	HNS3_SHAPER_LVL_QSET	= 3,
	HNS3_SHAPER_LVL_CNT	= 4,
	HNS3_SHAPER_LVL_VF	= 0,
	HNS3_SHAPER_LVL_PF	= 1,
};

struct hns3_shaper_parameter {
	uint32_t ir_b;  /* IR_B parameter of IR shaper */
	uint32_t ir_u;  /* IR_U parameter of IR shaper */
	uint32_t ir_s;  /* IR_S parameter of IR shaper */
};

#define hns3_dcb_set_field(dest, string, val) \
			   hns3_set_field((dest), \
			   (HNS3_DCB_SHAP_##string##_MSK), \
			   (HNS3_DCB_SHAP_##string##_LSH), val)
#define hns3_dcb_get_field(src, string) \
			hns3_get_field((src), (HNS3_DCB_SHAP_##string##_MSK), \
				       (HNS3_DCB_SHAP_##string##_LSH))

int hns3_pause_addr_cfg(struct hns3_hw *hw, const uint8_t *mac_addr);

int hns3_dcb_configure(struct hns3_adapter *hns);

int hns3_dcb_init(struct hns3_hw *hw);

int hns3_dcb_init_hw(struct hns3_hw *hw);

int hns3_dcb_info_init(struct hns3_hw *hw);

int hns3_fc_enable(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf);

int hns3_dcb_pfc_enable(struct rte_eth_dev *dev,
			struct rte_eth_pfc_conf *pfc_conf);

int hns3_queue_to_tc_mapping(struct hns3_hw *hw, uint16_t nb_rx_q,
			     uint16_t nb_tx_q);

int hns3_update_queue_map_configure(struct hns3_adapter *hns);
int hns3_dcb_port_shaper_cfg(struct hns3_hw *hw);

#endif /* _HNS3_DCB_H_ */
