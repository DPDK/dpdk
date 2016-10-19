/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef __ECORE_HSI_ETH__
#define __ECORE_HSI_ETH__
/************************************************************************/
/* Add include to common eth target for both eCore and protocol driver */
/************************************************************************/
#include "eth_common.h"

/*
 * The eth storm context for the Tstorm
 */
struct tstorm_eth_conn_st_ctx {
	__le32 reserved[4];
};

/*
 * The eth storm context for the Pstorm
 */
struct pstorm_eth_conn_st_ctx {
	__le32 reserved[8];
};

/*
 * The eth storm context for the Xstorm
 */
struct xstorm_eth_conn_st_ctx {
	__le32 reserved[60];
};

struct xstorm_eth_conn_ag_ctx {
	u8 reserved0 /* cdu_validation */;
	u8 eth_state /* state */;
	u8 flags0;
/* exist_in_qm0 */
#define XSTORM_ETH_CONN_AG_CTX_EXIST_IN_QM0_MASK            0x1
#define XSTORM_ETH_CONN_AG_CTX_EXIST_IN_QM0_SHIFT           0
/* exist_in_qm1 */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED1_MASK               0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED1_SHIFT              1
/* exist_in_qm2 */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED2_MASK               0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED2_SHIFT              2
/* exist_in_qm3 */
#define XSTORM_ETH_CONN_AG_CTX_EXIST_IN_QM3_MASK            0x1
#define XSTORM_ETH_CONN_AG_CTX_EXIST_IN_QM3_SHIFT           3
/* bit4 */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED3_MASK               0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED3_SHIFT              4
/* cf_array_active */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED4_MASK               0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED4_SHIFT              5
/* bit6 */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED5_MASK               0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED5_SHIFT              6
/* bit7 */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED6_MASK               0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED6_SHIFT              7
	u8 flags1;
/* bit8 */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED7_MASK               0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED7_SHIFT              0
/* bit9 */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED8_MASK               0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED8_SHIFT              1
/* bit10 */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED9_MASK               0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED9_SHIFT              2
/* bit11 */
#define XSTORM_ETH_CONN_AG_CTX_BIT11_MASK                   0x1
#define XSTORM_ETH_CONN_AG_CTX_BIT11_SHIFT                  3
/* bit12 */
#define XSTORM_ETH_CONN_AG_CTX_BIT12_MASK                   0x1
#define XSTORM_ETH_CONN_AG_CTX_BIT12_SHIFT                  4
/* bit13 */
#define XSTORM_ETH_CONN_AG_CTX_BIT13_MASK                   0x1
#define XSTORM_ETH_CONN_AG_CTX_BIT13_SHIFT                  5
/* bit14 */
#define XSTORM_ETH_CONN_AG_CTX_TX_RULE_ACTIVE_MASK          0x1
#define XSTORM_ETH_CONN_AG_CTX_TX_RULE_ACTIVE_SHIFT         6
/* bit15 */
#define XSTORM_ETH_CONN_AG_CTX_DQ_CF_ACTIVE_MASK            0x1
#define XSTORM_ETH_CONN_AG_CTX_DQ_CF_ACTIVE_SHIFT           7
	u8 flags2;
/* timer0cf */
#define XSTORM_ETH_CONN_AG_CTX_CF0_MASK                     0x3
#define XSTORM_ETH_CONN_AG_CTX_CF0_SHIFT                    0
/* timer1cf */
#define XSTORM_ETH_CONN_AG_CTX_CF1_MASK                     0x3
#define XSTORM_ETH_CONN_AG_CTX_CF1_SHIFT                    2
/* timer2cf */
#define XSTORM_ETH_CONN_AG_CTX_CF2_MASK                     0x3
#define XSTORM_ETH_CONN_AG_CTX_CF2_SHIFT                    4
/* timer_stop_all */
#define XSTORM_ETH_CONN_AG_CTX_CF3_MASK                     0x3
#define XSTORM_ETH_CONN_AG_CTX_CF3_SHIFT                    6
	u8 flags3;
/* cf4 */
#define XSTORM_ETH_CONN_AG_CTX_CF4_MASK                     0x3
#define XSTORM_ETH_CONN_AG_CTX_CF4_SHIFT                    0
/* cf5 */
#define XSTORM_ETH_CONN_AG_CTX_CF5_MASK                     0x3
#define XSTORM_ETH_CONN_AG_CTX_CF5_SHIFT                    2
/* cf6 */
#define XSTORM_ETH_CONN_AG_CTX_CF6_MASK                     0x3
#define XSTORM_ETH_CONN_AG_CTX_CF6_SHIFT                    4
/* cf7 */
#define XSTORM_ETH_CONN_AG_CTX_CF7_MASK                     0x3
#define XSTORM_ETH_CONN_AG_CTX_CF7_SHIFT                    6
	u8 flags4;
/* cf8 */
#define XSTORM_ETH_CONN_AG_CTX_CF8_MASK                     0x3
#define XSTORM_ETH_CONN_AG_CTX_CF8_SHIFT                    0
/* cf9 */
#define XSTORM_ETH_CONN_AG_CTX_CF9_MASK                     0x3
#define XSTORM_ETH_CONN_AG_CTX_CF9_SHIFT                    2
/* cf10 */
#define XSTORM_ETH_CONN_AG_CTX_CF10_MASK                    0x3
#define XSTORM_ETH_CONN_AG_CTX_CF10_SHIFT                   4
/* cf11 */
#define XSTORM_ETH_CONN_AG_CTX_CF11_MASK                    0x3
#define XSTORM_ETH_CONN_AG_CTX_CF11_SHIFT                   6
	u8 flags5;
/* cf12 */
#define XSTORM_ETH_CONN_AG_CTX_CF12_MASK                    0x3
#define XSTORM_ETH_CONN_AG_CTX_CF12_SHIFT                   0
/* cf13 */
#define XSTORM_ETH_CONN_AG_CTX_CF13_MASK                    0x3
#define XSTORM_ETH_CONN_AG_CTX_CF13_SHIFT                   2
/* cf14 */
#define XSTORM_ETH_CONN_AG_CTX_CF14_MASK                    0x3
#define XSTORM_ETH_CONN_AG_CTX_CF14_SHIFT                   4
/* cf15 */
#define XSTORM_ETH_CONN_AG_CTX_CF15_MASK                    0x3
#define XSTORM_ETH_CONN_AG_CTX_CF15_SHIFT                   6
	u8 flags6;
/* cf16 */
#define XSTORM_ETH_CONN_AG_CTX_GO_TO_BD_CONS_CF_MASK        0x3
#define XSTORM_ETH_CONN_AG_CTX_GO_TO_BD_CONS_CF_SHIFT       0
/* cf_array_cf */
#define XSTORM_ETH_CONN_AG_CTX_MULTI_UNICAST_CF_MASK        0x3
#define XSTORM_ETH_CONN_AG_CTX_MULTI_UNICAST_CF_SHIFT       2
/* cf18 */
#define XSTORM_ETH_CONN_AG_CTX_DQ_CF_MASK                   0x3
#define XSTORM_ETH_CONN_AG_CTX_DQ_CF_SHIFT                  4
/* cf19 */
#define XSTORM_ETH_CONN_AG_CTX_TERMINATE_CF_MASK            0x3
#define XSTORM_ETH_CONN_AG_CTX_TERMINATE_CF_SHIFT           6
	u8 flags7;
/* cf20 */
#define XSTORM_ETH_CONN_AG_CTX_FLUSH_Q0_MASK                0x3
#define XSTORM_ETH_CONN_AG_CTX_FLUSH_Q0_SHIFT               0
/* cf21 */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED10_MASK              0x3
#define XSTORM_ETH_CONN_AG_CTX_RESERVED10_SHIFT             2
/* cf22 */
#define XSTORM_ETH_CONN_AG_CTX_SLOW_PATH_MASK               0x3
#define XSTORM_ETH_CONN_AG_CTX_SLOW_PATH_SHIFT              4
/* cf0en */
#define XSTORM_ETH_CONN_AG_CTX_CF0EN_MASK                   0x1
#define XSTORM_ETH_CONN_AG_CTX_CF0EN_SHIFT                  6
/* cf1en */
#define XSTORM_ETH_CONN_AG_CTX_CF1EN_MASK                   0x1
#define XSTORM_ETH_CONN_AG_CTX_CF1EN_SHIFT                  7
	u8 flags8;
/* cf2en */
#define XSTORM_ETH_CONN_AG_CTX_CF2EN_MASK                   0x1
#define XSTORM_ETH_CONN_AG_CTX_CF2EN_SHIFT                  0
/* cf3en */
#define XSTORM_ETH_CONN_AG_CTX_CF3EN_MASK                   0x1
#define XSTORM_ETH_CONN_AG_CTX_CF3EN_SHIFT                  1
/* cf4en */
#define XSTORM_ETH_CONN_AG_CTX_CF4EN_MASK                   0x1
#define XSTORM_ETH_CONN_AG_CTX_CF4EN_SHIFT                  2
/* cf5en */
#define XSTORM_ETH_CONN_AG_CTX_CF5EN_MASK                   0x1
#define XSTORM_ETH_CONN_AG_CTX_CF5EN_SHIFT                  3
/* cf6en */
#define XSTORM_ETH_CONN_AG_CTX_CF6EN_MASK                   0x1
#define XSTORM_ETH_CONN_AG_CTX_CF6EN_SHIFT                  4
/* cf7en */
#define XSTORM_ETH_CONN_AG_CTX_CF7EN_MASK                   0x1
#define XSTORM_ETH_CONN_AG_CTX_CF7EN_SHIFT                  5
/* cf8en */
#define XSTORM_ETH_CONN_AG_CTX_CF8EN_MASK                   0x1
#define XSTORM_ETH_CONN_AG_CTX_CF8EN_SHIFT                  6
/* cf9en */
#define XSTORM_ETH_CONN_AG_CTX_CF9EN_MASK                   0x1
#define XSTORM_ETH_CONN_AG_CTX_CF9EN_SHIFT                  7
	u8 flags9;
/* cf10en */
#define XSTORM_ETH_CONN_AG_CTX_CF10EN_MASK                  0x1
#define XSTORM_ETH_CONN_AG_CTX_CF10EN_SHIFT                 0
/* cf11en */
#define XSTORM_ETH_CONN_AG_CTX_CF11EN_MASK                  0x1
#define XSTORM_ETH_CONN_AG_CTX_CF11EN_SHIFT                 1
/* cf12en */
#define XSTORM_ETH_CONN_AG_CTX_CF12EN_MASK                  0x1
#define XSTORM_ETH_CONN_AG_CTX_CF12EN_SHIFT                 2
/* cf13en */
#define XSTORM_ETH_CONN_AG_CTX_CF13EN_MASK                  0x1
#define XSTORM_ETH_CONN_AG_CTX_CF13EN_SHIFT                 3
/* cf14en */
#define XSTORM_ETH_CONN_AG_CTX_CF14EN_MASK                  0x1
#define XSTORM_ETH_CONN_AG_CTX_CF14EN_SHIFT                 4
/* cf15en */
#define XSTORM_ETH_CONN_AG_CTX_CF15EN_MASK                  0x1
#define XSTORM_ETH_CONN_AG_CTX_CF15EN_SHIFT                 5
/* cf16en */
#define XSTORM_ETH_CONN_AG_CTX_GO_TO_BD_CONS_CF_EN_MASK     0x1
#define XSTORM_ETH_CONN_AG_CTX_GO_TO_BD_CONS_CF_EN_SHIFT    6
/* cf_array_cf_en */
#define XSTORM_ETH_CONN_AG_CTX_MULTI_UNICAST_CF_EN_MASK     0x1
#define XSTORM_ETH_CONN_AG_CTX_MULTI_UNICAST_CF_EN_SHIFT    7
	u8 flags10;
/* cf18en */
#define XSTORM_ETH_CONN_AG_CTX_DQ_CF_EN_MASK                0x1
#define XSTORM_ETH_CONN_AG_CTX_DQ_CF_EN_SHIFT               0
/* cf19en */
#define XSTORM_ETH_CONN_AG_CTX_TERMINATE_CF_EN_MASK         0x1
#define XSTORM_ETH_CONN_AG_CTX_TERMINATE_CF_EN_SHIFT        1
/* cf20en */
#define XSTORM_ETH_CONN_AG_CTX_FLUSH_Q0_EN_MASK             0x1
#define XSTORM_ETH_CONN_AG_CTX_FLUSH_Q0_EN_SHIFT            2
/* cf21en */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED11_MASK              0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED11_SHIFT             3
/* cf22en */
#define XSTORM_ETH_CONN_AG_CTX_SLOW_PATH_EN_MASK            0x1
#define XSTORM_ETH_CONN_AG_CTX_SLOW_PATH_EN_SHIFT           4
/* cf23en */
#define XSTORM_ETH_CONN_AG_CTX_TPH_ENABLE_EN_RESERVED_MASK  0x1
#define XSTORM_ETH_CONN_AG_CTX_TPH_ENABLE_EN_RESERVED_SHIFT 5
/* rule0en */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED12_MASK              0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED12_SHIFT             6
/* rule1en */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED13_MASK              0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED13_SHIFT             7
	u8 flags11;
/* rule2en */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED14_MASK              0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED14_SHIFT             0
/* rule3en */
#define XSTORM_ETH_CONN_AG_CTX_RESERVED15_MASK              0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED15_SHIFT             1
/* rule4en */
#define XSTORM_ETH_CONN_AG_CTX_TX_DEC_RULE_EN_MASK          0x1
#define XSTORM_ETH_CONN_AG_CTX_TX_DEC_RULE_EN_SHIFT         2
/* rule5en */
#define XSTORM_ETH_CONN_AG_CTX_RULE5EN_MASK                 0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE5EN_SHIFT                3
/* rule6en */
#define XSTORM_ETH_CONN_AG_CTX_RULE6EN_MASK                 0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE6EN_SHIFT                4
/* rule7en */
#define XSTORM_ETH_CONN_AG_CTX_RULE7EN_MASK                 0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE7EN_SHIFT                5
/* rule8en */
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED1_MASK            0x1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED1_SHIFT           6
/* rule9en */
#define XSTORM_ETH_CONN_AG_CTX_RULE9EN_MASK                 0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE9EN_SHIFT                7
	u8 flags12;
/* rule10en */
#define XSTORM_ETH_CONN_AG_CTX_RULE10EN_MASK                0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE10EN_SHIFT               0
/* rule11en */
#define XSTORM_ETH_CONN_AG_CTX_RULE11EN_MASK                0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE11EN_SHIFT               1
/* rule12en */
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED2_MASK            0x1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED2_SHIFT           2
/* rule13en */
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED3_MASK            0x1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED3_SHIFT           3
/* rule14en */
#define XSTORM_ETH_CONN_AG_CTX_RULE14EN_MASK                0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE14EN_SHIFT               4
/* rule15en */
#define XSTORM_ETH_CONN_AG_CTX_RULE15EN_MASK                0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE15EN_SHIFT               5
/* rule16en */
#define XSTORM_ETH_CONN_AG_CTX_RULE16EN_MASK                0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE16EN_SHIFT               6
/* rule17en */
#define XSTORM_ETH_CONN_AG_CTX_RULE17EN_MASK                0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE17EN_SHIFT               7
	u8 flags13;
/* rule18en */
#define XSTORM_ETH_CONN_AG_CTX_RULE18EN_MASK                0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE18EN_SHIFT               0
/* rule19en */
#define XSTORM_ETH_CONN_AG_CTX_RULE19EN_MASK                0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE19EN_SHIFT               1
/* rule20en */
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED4_MASK            0x1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED4_SHIFT           2
/* rule21en */
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED5_MASK            0x1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED5_SHIFT           3
/* rule22en */
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED6_MASK            0x1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED6_SHIFT           4
/* rule23en */
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED7_MASK            0x1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED7_SHIFT           5
/* rule24en */
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED8_MASK            0x1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED8_SHIFT           6
/* rule25en */
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED9_MASK            0x1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED9_SHIFT           7
	u8 flags14;
/* bit16 */
#define XSTORM_ETH_CONN_AG_CTX_EDPM_USE_EXT_HDR_MASK        0x1
#define XSTORM_ETH_CONN_AG_CTX_EDPM_USE_EXT_HDR_SHIFT       0
/* bit17 */
#define XSTORM_ETH_CONN_AG_CTX_EDPM_SEND_RAW_L3L4_MASK      0x1
#define XSTORM_ETH_CONN_AG_CTX_EDPM_SEND_RAW_L3L4_SHIFT     1
/* bit18 */
#define XSTORM_ETH_CONN_AG_CTX_EDPM_INBAND_PROP_HDR_MASK    0x1
#define XSTORM_ETH_CONN_AG_CTX_EDPM_INBAND_PROP_HDR_SHIFT   2
/* bit19 */
#define XSTORM_ETH_CONN_AG_CTX_EDPM_SEND_EXT_TUNNEL_MASK    0x1
#define XSTORM_ETH_CONN_AG_CTX_EDPM_SEND_EXT_TUNNEL_SHIFT   3
/* bit20 */
#define XSTORM_ETH_CONN_AG_CTX_L2_EDPM_ENABLE_MASK          0x1
#define XSTORM_ETH_CONN_AG_CTX_L2_EDPM_ENABLE_SHIFT         4
/* bit21 */
#define XSTORM_ETH_CONN_AG_CTX_ROCE_EDPM_ENABLE_MASK        0x1
#define XSTORM_ETH_CONN_AG_CTX_ROCE_EDPM_ENABLE_SHIFT       5
/* cf23 */
#define XSTORM_ETH_CONN_AG_CTX_TPH_ENABLE_MASK              0x3
#define XSTORM_ETH_CONN_AG_CTX_TPH_ENABLE_SHIFT             6
	u8 edpm_event_id /* byte2 */;
	__le16 physical_q0 /* physical_q0 */;
	__le16 quota /* physical_q1 */;
	__le16 edpm_num_bds /* physical_q2 */;
	__le16 tx_bd_cons /* word3 */;
	__le16 tx_bd_prod /* word4 */;
	__le16 tx_class /* word5 */;
	__le16 conn_dpi /* conn_dpi */;
	u8 byte3 /* byte3 */;
	u8 byte4 /* byte4 */;
	u8 byte5 /* byte5 */;
	u8 byte6 /* byte6 */;
	__le32 reg0 /* reg0 */;
	__le32 reg1 /* reg1 */;
	__le32 reg2 /* reg2 */;
	__le32 reg3 /* reg3 */;
	__le32 reg4 /* reg4 */;
	__le32 reg5 /* cf_array0 */;
	__le32 reg6 /* cf_array1 */;
	__le16 word7 /* word7 */;
	__le16 word8 /* word8 */;
	__le16 word9 /* word9 */;
	__le16 word10 /* word10 */;
	__le32 reg7 /* reg7 */;
	__le32 reg8 /* reg8 */;
	__le32 reg9 /* reg9 */;
	u8 byte7 /* byte7 */;
	u8 byte8 /* byte8 */;
	u8 byte9 /* byte9 */;
	u8 byte10 /* byte10 */;
	u8 byte11 /* byte11 */;
	u8 byte12 /* byte12 */;
	u8 byte13 /* byte13 */;
	u8 byte14 /* byte14 */;
	u8 byte15 /* byte15 */;
	u8 byte16 /* byte16 */;
	__le16 word11 /* word11 */;
	__le32 reg10 /* reg10 */;
	__le32 reg11 /* reg11 */;
	__le32 reg12 /* reg12 */;
	__le32 reg13 /* reg13 */;
	__le32 reg14 /* reg14 */;
	__le32 reg15 /* reg15 */;
	__le32 reg16 /* reg16 */;
	__le32 reg17 /* reg17 */;
	__le32 reg18 /* reg18 */;
	__le32 reg19 /* reg19 */;
	__le16 word12 /* word12 */;
	__le16 word13 /* word13 */;
	__le16 word14 /* word14 */;
	__le16 word15 /* word15 */;
};

/*
 * The eth storm context for the Ystorm
 */
struct ystorm_eth_conn_st_ctx {
	__le32 reserved[8];
};

struct ystorm_eth_conn_ag_ctx {
	u8 byte0 /* cdu_validation */;
	u8 state /* state */;
	u8 flags0;
/* exist_in_qm0 */
#define YSTORM_ETH_CONN_AG_CTX_BIT0_MASK                  0x1
#define YSTORM_ETH_CONN_AG_CTX_BIT0_SHIFT                 0
/* exist_in_qm1 */
#define YSTORM_ETH_CONN_AG_CTX_BIT1_MASK                  0x1
#define YSTORM_ETH_CONN_AG_CTX_BIT1_SHIFT                 1
#define YSTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_MASK     0x3 /* cf0 */
#define YSTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_SHIFT    2
#define YSTORM_ETH_CONN_AG_CTX_PMD_TERMINATE_CF_MASK      0x3 /* cf1 */
#define YSTORM_ETH_CONN_AG_CTX_PMD_TERMINATE_CF_SHIFT     4
#define YSTORM_ETH_CONN_AG_CTX_CF2_MASK                   0x3 /* cf2 */
#define YSTORM_ETH_CONN_AG_CTX_CF2_SHIFT                  6
	u8 flags1;
/* cf0en */
#define YSTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_EN_MASK  0x1
#define YSTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_EN_SHIFT 0
/* cf1en */
#define YSTORM_ETH_CONN_AG_CTX_PMD_TERMINATE_CF_EN_MASK   0x1
#define YSTORM_ETH_CONN_AG_CTX_PMD_TERMINATE_CF_EN_SHIFT  1
/* cf2en */
#define YSTORM_ETH_CONN_AG_CTX_CF2EN_MASK                 0x1
#define YSTORM_ETH_CONN_AG_CTX_CF2EN_SHIFT                2
/* rule0en */
#define YSTORM_ETH_CONN_AG_CTX_RULE0EN_MASK               0x1
#define YSTORM_ETH_CONN_AG_CTX_RULE0EN_SHIFT              3
/* rule1en */
#define YSTORM_ETH_CONN_AG_CTX_RULE1EN_MASK               0x1
#define YSTORM_ETH_CONN_AG_CTX_RULE1EN_SHIFT              4
/* rule2en */
#define YSTORM_ETH_CONN_AG_CTX_RULE2EN_MASK               0x1
#define YSTORM_ETH_CONN_AG_CTX_RULE2EN_SHIFT              5
/* rule3en */
#define YSTORM_ETH_CONN_AG_CTX_RULE3EN_MASK               0x1
#define YSTORM_ETH_CONN_AG_CTX_RULE3EN_SHIFT              6
/* rule4en */
#define YSTORM_ETH_CONN_AG_CTX_RULE4EN_MASK               0x1
#define YSTORM_ETH_CONN_AG_CTX_RULE4EN_SHIFT              7
	u8 tx_q0_int_coallecing_timeset /* byte2 */;
	u8 byte3 /* byte3 */;
	__le16 word0 /* word0 */;
	__le32 terminate_spqe /* reg0 */;
	__le32 reg1 /* reg1 */;
	__le16 tx_bd_cons_upd /* word1 */;
	__le16 word2 /* word2 */;
	__le16 word3 /* word3 */;
	__le16 word4 /* word4 */;
	__le32 reg2 /* reg2 */;
	__le32 reg3 /* reg3 */;
};

struct tstorm_eth_conn_ag_ctx {
	u8 byte0 /* cdu_validation */;
	u8 byte1 /* state */;
	u8 flags0;
#define TSTORM_ETH_CONN_AG_CTX_BIT0_MASK      0x1 /* exist_in_qm0 */
#define TSTORM_ETH_CONN_AG_CTX_BIT0_SHIFT     0
#define TSTORM_ETH_CONN_AG_CTX_BIT1_MASK      0x1 /* exist_in_qm1 */
#define TSTORM_ETH_CONN_AG_CTX_BIT1_SHIFT     1
#define TSTORM_ETH_CONN_AG_CTX_BIT2_MASK      0x1 /* bit2 */
#define TSTORM_ETH_CONN_AG_CTX_BIT2_SHIFT     2
#define TSTORM_ETH_CONN_AG_CTX_BIT3_MASK      0x1 /* bit3 */
#define TSTORM_ETH_CONN_AG_CTX_BIT3_SHIFT     3
#define TSTORM_ETH_CONN_AG_CTX_BIT4_MASK      0x1 /* bit4 */
#define TSTORM_ETH_CONN_AG_CTX_BIT4_SHIFT     4
#define TSTORM_ETH_CONN_AG_CTX_BIT5_MASK      0x1 /* bit5 */
#define TSTORM_ETH_CONN_AG_CTX_BIT5_SHIFT     5
#define TSTORM_ETH_CONN_AG_CTX_CF0_MASK       0x3 /* timer0cf */
#define TSTORM_ETH_CONN_AG_CTX_CF0_SHIFT      6
	u8 flags1;
#define TSTORM_ETH_CONN_AG_CTX_CF1_MASK       0x3 /* timer1cf */
#define TSTORM_ETH_CONN_AG_CTX_CF1_SHIFT      0
#define TSTORM_ETH_CONN_AG_CTX_CF2_MASK       0x3 /* timer2cf */
#define TSTORM_ETH_CONN_AG_CTX_CF2_SHIFT      2
#define TSTORM_ETH_CONN_AG_CTX_CF3_MASK       0x3 /* timer_stop_all */
#define TSTORM_ETH_CONN_AG_CTX_CF3_SHIFT      4
#define TSTORM_ETH_CONN_AG_CTX_CF4_MASK       0x3 /* cf4 */
#define TSTORM_ETH_CONN_AG_CTX_CF4_SHIFT      6
	u8 flags2;
#define TSTORM_ETH_CONN_AG_CTX_CF5_MASK       0x3 /* cf5 */
#define TSTORM_ETH_CONN_AG_CTX_CF5_SHIFT      0
#define TSTORM_ETH_CONN_AG_CTX_CF6_MASK       0x3 /* cf6 */
#define TSTORM_ETH_CONN_AG_CTX_CF6_SHIFT      2
#define TSTORM_ETH_CONN_AG_CTX_CF7_MASK       0x3 /* cf7 */
#define TSTORM_ETH_CONN_AG_CTX_CF7_SHIFT      4
#define TSTORM_ETH_CONN_AG_CTX_CF8_MASK       0x3 /* cf8 */
#define TSTORM_ETH_CONN_AG_CTX_CF8_SHIFT      6
	u8 flags3;
#define TSTORM_ETH_CONN_AG_CTX_CF9_MASK       0x3 /* cf9 */
#define TSTORM_ETH_CONN_AG_CTX_CF9_SHIFT      0
#define TSTORM_ETH_CONN_AG_CTX_CF10_MASK      0x3 /* cf10 */
#define TSTORM_ETH_CONN_AG_CTX_CF10_SHIFT     2
#define TSTORM_ETH_CONN_AG_CTX_CF0EN_MASK     0x1 /* cf0en */
#define TSTORM_ETH_CONN_AG_CTX_CF0EN_SHIFT    4
#define TSTORM_ETH_CONN_AG_CTX_CF1EN_MASK     0x1 /* cf1en */
#define TSTORM_ETH_CONN_AG_CTX_CF1EN_SHIFT    5
#define TSTORM_ETH_CONN_AG_CTX_CF2EN_MASK     0x1 /* cf2en */
#define TSTORM_ETH_CONN_AG_CTX_CF2EN_SHIFT    6
#define TSTORM_ETH_CONN_AG_CTX_CF3EN_MASK     0x1 /* cf3en */
#define TSTORM_ETH_CONN_AG_CTX_CF3EN_SHIFT    7
	u8 flags4;
#define TSTORM_ETH_CONN_AG_CTX_CF4EN_MASK     0x1 /* cf4en */
#define TSTORM_ETH_CONN_AG_CTX_CF4EN_SHIFT    0
#define TSTORM_ETH_CONN_AG_CTX_CF5EN_MASK     0x1 /* cf5en */
#define TSTORM_ETH_CONN_AG_CTX_CF5EN_SHIFT    1
#define TSTORM_ETH_CONN_AG_CTX_CF6EN_MASK     0x1 /* cf6en */
#define TSTORM_ETH_CONN_AG_CTX_CF6EN_SHIFT    2
#define TSTORM_ETH_CONN_AG_CTX_CF7EN_MASK     0x1 /* cf7en */
#define TSTORM_ETH_CONN_AG_CTX_CF7EN_SHIFT    3
#define TSTORM_ETH_CONN_AG_CTX_CF8EN_MASK     0x1 /* cf8en */
#define TSTORM_ETH_CONN_AG_CTX_CF8EN_SHIFT    4
#define TSTORM_ETH_CONN_AG_CTX_CF9EN_MASK     0x1 /* cf9en */
#define TSTORM_ETH_CONN_AG_CTX_CF9EN_SHIFT    5
#define TSTORM_ETH_CONN_AG_CTX_CF10EN_MASK    0x1 /* cf10en */
#define TSTORM_ETH_CONN_AG_CTX_CF10EN_SHIFT   6
#define TSTORM_ETH_CONN_AG_CTX_RULE0EN_MASK   0x1 /* rule0en */
#define TSTORM_ETH_CONN_AG_CTX_RULE0EN_SHIFT  7
	u8 flags5;
#define TSTORM_ETH_CONN_AG_CTX_RULE1EN_MASK   0x1 /* rule1en */
#define TSTORM_ETH_CONN_AG_CTX_RULE1EN_SHIFT  0
#define TSTORM_ETH_CONN_AG_CTX_RULE2EN_MASK   0x1 /* rule2en */
#define TSTORM_ETH_CONN_AG_CTX_RULE2EN_SHIFT  1
#define TSTORM_ETH_CONN_AG_CTX_RULE3EN_MASK   0x1 /* rule3en */
#define TSTORM_ETH_CONN_AG_CTX_RULE3EN_SHIFT  2
#define TSTORM_ETH_CONN_AG_CTX_RULE4EN_MASK   0x1 /* rule4en */
#define TSTORM_ETH_CONN_AG_CTX_RULE4EN_SHIFT  3
#define TSTORM_ETH_CONN_AG_CTX_RULE5EN_MASK   0x1 /* rule5en */
#define TSTORM_ETH_CONN_AG_CTX_RULE5EN_SHIFT  4
#define TSTORM_ETH_CONN_AG_CTX_RX_BD_EN_MASK  0x1 /* rule6en */
#define TSTORM_ETH_CONN_AG_CTX_RX_BD_EN_SHIFT 5
#define TSTORM_ETH_CONN_AG_CTX_RULE7EN_MASK   0x1 /* rule7en */
#define TSTORM_ETH_CONN_AG_CTX_RULE7EN_SHIFT  6
#define TSTORM_ETH_CONN_AG_CTX_RULE8EN_MASK   0x1 /* rule8en */
#define TSTORM_ETH_CONN_AG_CTX_RULE8EN_SHIFT  7
	__le32 reg0 /* reg0 */;
	__le32 reg1 /* reg1 */;
	__le32 reg2 /* reg2 */;
	__le32 reg3 /* reg3 */;
	__le32 reg4 /* reg4 */;
	__le32 reg5 /* reg5 */;
	__le32 reg6 /* reg6 */;
	__le32 reg7 /* reg7 */;
	__le32 reg8 /* reg8 */;
	u8 byte2 /* byte2 */;
	u8 byte3 /* byte3 */;
	__le16 rx_bd_cons /* word0 */;
	u8 byte4 /* byte4 */;
	u8 byte5 /* byte5 */;
	__le16 rx_bd_prod /* word1 */;
	__le16 word2 /* conn_dpi */;
	__le16 word3 /* word3 */;
	__le32 reg9 /* reg9 */;
	__le32 reg10 /* reg10 */;
};

struct ustorm_eth_conn_ag_ctx {
	u8 byte0 /* cdu_validation */;
	u8 byte1 /* state */;
	u8 flags0;
/* exist_in_qm0 */
#define USTORM_ETH_CONN_AG_CTX_BIT0_MASK                    0x1
#define USTORM_ETH_CONN_AG_CTX_BIT0_SHIFT                   0
/* exist_in_qm1 */
#define USTORM_ETH_CONN_AG_CTX_BIT1_MASK                    0x1
#define USTORM_ETH_CONN_AG_CTX_BIT1_SHIFT                   1
/* timer0cf */
#define USTORM_ETH_CONN_AG_CTX_TX_PMD_TERMINATE_CF_MASK     0x3
#define USTORM_ETH_CONN_AG_CTX_TX_PMD_TERMINATE_CF_SHIFT    2
/* timer1cf */
#define USTORM_ETH_CONN_AG_CTX_RX_PMD_TERMINATE_CF_MASK     0x3
#define USTORM_ETH_CONN_AG_CTX_RX_PMD_TERMINATE_CF_SHIFT    4
/* timer2cf */
#define USTORM_ETH_CONN_AG_CTX_CF2_MASK                     0x3
#define USTORM_ETH_CONN_AG_CTX_CF2_SHIFT                    6
	u8 flags1;
/* timer_stop_all */
#define USTORM_ETH_CONN_AG_CTX_CF3_MASK                     0x3
#define USTORM_ETH_CONN_AG_CTX_CF3_SHIFT                    0
/* cf4 */
#define USTORM_ETH_CONN_AG_CTX_TX_ARM_CF_MASK               0x3
#define USTORM_ETH_CONN_AG_CTX_TX_ARM_CF_SHIFT              2
/* cf5 */
#define USTORM_ETH_CONN_AG_CTX_RX_ARM_CF_MASK               0x3
#define USTORM_ETH_CONN_AG_CTX_RX_ARM_CF_SHIFT              4
/* cf6 */
#define USTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_MASK       0x3
#define USTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_SHIFT      6
	u8 flags2;
/* cf0en */
#define USTORM_ETH_CONN_AG_CTX_TX_PMD_TERMINATE_CF_EN_MASK  0x1
#define USTORM_ETH_CONN_AG_CTX_TX_PMD_TERMINATE_CF_EN_SHIFT 0
/* cf1en */
#define USTORM_ETH_CONN_AG_CTX_RX_PMD_TERMINATE_CF_EN_MASK  0x1
#define USTORM_ETH_CONN_AG_CTX_RX_PMD_TERMINATE_CF_EN_SHIFT 1
/* cf2en */
#define USTORM_ETH_CONN_AG_CTX_CF2EN_MASK                   0x1
#define USTORM_ETH_CONN_AG_CTX_CF2EN_SHIFT                  2
/* cf3en */
#define USTORM_ETH_CONN_AG_CTX_CF3EN_MASK                   0x1
#define USTORM_ETH_CONN_AG_CTX_CF3EN_SHIFT                  3
/* cf4en */
#define USTORM_ETH_CONN_AG_CTX_TX_ARM_CF_EN_MASK            0x1
#define USTORM_ETH_CONN_AG_CTX_TX_ARM_CF_EN_SHIFT           4
/* cf5en */
#define USTORM_ETH_CONN_AG_CTX_RX_ARM_CF_EN_MASK            0x1
#define USTORM_ETH_CONN_AG_CTX_RX_ARM_CF_EN_SHIFT           5
/* cf6en */
#define USTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_EN_MASK    0x1
#define USTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_EN_SHIFT   6
/* rule0en */
#define USTORM_ETH_CONN_AG_CTX_RULE0EN_MASK                 0x1
#define USTORM_ETH_CONN_AG_CTX_RULE0EN_SHIFT                7
	u8 flags3;
/* rule1en */
#define USTORM_ETH_CONN_AG_CTX_RULE1EN_MASK                 0x1
#define USTORM_ETH_CONN_AG_CTX_RULE1EN_SHIFT                0
/* rule2en */
#define USTORM_ETH_CONN_AG_CTX_RULE2EN_MASK                 0x1
#define USTORM_ETH_CONN_AG_CTX_RULE2EN_SHIFT                1
/* rule3en */
#define USTORM_ETH_CONN_AG_CTX_RULE3EN_MASK                 0x1
#define USTORM_ETH_CONN_AG_CTX_RULE3EN_SHIFT                2
/* rule4en */
#define USTORM_ETH_CONN_AG_CTX_RULE4EN_MASK                 0x1
#define USTORM_ETH_CONN_AG_CTX_RULE4EN_SHIFT                3
/* rule5en */
#define USTORM_ETH_CONN_AG_CTX_RULE5EN_MASK                 0x1
#define USTORM_ETH_CONN_AG_CTX_RULE5EN_SHIFT                4
/* rule6en */
#define USTORM_ETH_CONN_AG_CTX_RULE6EN_MASK                 0x1
#define USTORM_ETH_CONN_AG_CTX_RULE6EN_SHIFT                5
/* rule7en */
#define USTORM_ETH_CONN_AG_CTX_RULE7EN_MASK                 0x1
#define USTORM_ETH_CONN_AG_CTX_RULE7EN_SHIFT                6
/* rule8en */
#define USTORM_ETH_CONN_AG_CTX_RULE8EN_MASK                 0x1
#define USTORM_ETH_CONN_AG_CTX_RULE8EN_SHIFT                7
	u8 byte2 /* byte2 */;
	u8 byte3 /* byte3 */;
	__le16 word0 /* conn_dpi */;
	__le16 tx_bd_cons /* word1 */;
	__le32 reg0 /* reg0 */;
	__le32 reg1 /* reg1 */;
	__le32 reg2 /* reg2 */;
	__le32 tx_int_coallecing_timeset /* reg3 */;
	__le16 tx_drv_bd_cons /* word2 */;
	__le16 rx_drv_cqe_cons /* word3 */;
};

/*
 * The eth storm context for the Ustorm
 */
struct ustorm_eth_conn_st_ctx {
	__le32 reserved[40];
};

/*
 * The eth storm context for the Mstorm
 */
struct mstorm_eth_conn_st_ctx {
	__le32 reserved[8];
};

/*
 * eth connection context
 */
struct eth_conn_context {
/* tstorm storm context */
	struct tstorm_eth_conn_st_ctx tstorm_st_context;
	struct regpair tstorm_st_padding[2] /* padding */;
/* pstorm storm context */
	struct pstorm_eth_conn_st_ctx pstorm_st_context;
/* xstorm storm context */
	struct xstorm_eth_conn_st_ctx xstorm_st_context;
/* xstorm aggregative context */
	struct xstorm_eth_conn_ag_ctx xstorm_ag_context;
/* ystorm storm context */
	struct ystorm_eth_conn_st_ctx ystorm_st_context;
/* ystorm aggregative context */
	struct ystorm_eth_conn_ag_ctx ystorm_ag_context;
/* tstorm aggregative context */
	struct tstorm_eth_conn_ag_ctx tstorm_ag_context;
/* ustorm aggregative context */
	struct ustorm_eth_conn_ag_ctx ustorm_ag_context;
/* ustorm storm context */
	struct ustorm_eth_conn_st_ctx ustorm_st_context;
/* mstorm storm context */
	struct mstorm_eth_conn_st_ctx mstorm_st_context;
};


/*
 * Ethernet filter types: mac/vlan/pair
 */
enum eth_error_code {
	ETH_OK = 0x00 /* command succeeded */,
/* mac add filters command failed due to cam full state */
	ETH_FILTERS_MAC_ADD_FAIL_FULL,
/* mac add filters command failed due to mtt2 full state */
	ETH_FILTERS_MAC_ADD_FAIL_FULL_MTT2,
/* mac add filters command failed due to duplicate mac address */
	ETH_FILTERS_MAC_ADD_FAIL_DUP_MTT2,
/* mac add filters command failed due to duplicate mac address */
	ETH_FILTERS_MAC_ADD_FAIL_DUP_STT2,
/* mac delete filters command failed due to not found state */
	ETH_FILTERS_MAC_DEL_FAIL_NOF,
/* mac delete filters command failed due to not found state */
	ETH_FILTERS_MAC_DEL_FAIL_NOF_MTT2,
/* mac delete filters command failed due to not found state */
	ETH_FILTERS_MAC_DEL_FAIL_NOF_STT2,
/* mac add filters command failed due to MAC Address of 00:00:00:00:00:00 */
	ETH_FILTERS_MAC_ADD_FAIL_ZERO_MAC,
/* vlan add filters command failed due to cam full state */
	ETH_FILTERS_VLAN_ADD_FAIL_FULL,
/* vlan add filters command failed due to duplicate VLAN filter */
	ETH_FILTERS_VLAN_ADD_FAIL_DUP,
/* vlan delete filters command failed due to not found state */
	ETH_FILTERS_VLAN_DEL_FAIL_NOF,
/* vlan delete filters command failed due to not found state */
	ETH_FILTERS_VLAN_DEL_FAIL_NOF_TT1,
/* pair add filters command failed due to duplicate request */
	ETH_FILTERS_PAIR_ADD_FAIL_DUP,
/* pair add filters command failed due to full state */
	ETH_FILTERS_PAIR_ADD_FAIL_FULL,
/* pair add filters command failed due to full state */
	ETH_FILTERS_PAIR_ADD_FAIL_FULL_MAC,
/* pair add filters command failed due not found state */
	ETH_FILTERS_PAIR_DEL_FAIL_NOF,
/* pair add filters command failed due not found state */
	ETH_FILTERS_PAIR_DEL_FAIL_NOF_TT1,
/* pair add filters command failed due to MAC Address of 00:00:00:00:00:00 */
	ETH_FILTERS_PAIR_ADD_FAIL_ZERO_MAC,
/* vni add filters command failed due to cam full state */
	ETH_FILTERS_VNI_ADD_FAIL_FULL,
/* vni add filters command failed due to duplicate VNI filter */
	ETH_FILTERS_VNI_ADD_FAIL_DUP,
	MAX_ETH_ERROR_CODE
};


/*
 * opcodes for the event ring
 */
enum eth_event_opcode {
	ETH_EVENT_UNUSED,
	ETH_EVENT_VPORT_START,
	ETH_EVENT_VPORT_UPDATE,
	ETH_EVENT_VPORT_STOP,
	ETH_EVENT_TX_QUEUE_START,
	ETH_EVENT_TX_QUEUE_STOP,
	ETH_EVENT_RX_QUEUE_START,
	ETH_EVENT_RX_QUEUE_UPDATE,
	ETH_EVENT_RX_QUEUE_STOP,
	ETH_EVENT_FILTERS_UPDATE,
	ETH_EVENT_RX_ADD_OPENFLOW_FILTER,
	ETH_EVENT_RX_DELETE_OPENFLOW_FILTER,
	ETH_EVENT_RX_CREATE_OPENFLOW_ACTION,
	ETH_EVENT_RX_ADD_UDP_FILTER,
	ETH_EVENT_RX_DELETE_UDP_FILTER,
	ETH_EVENT_RX_CREATE_GFT_ACTION,
	ETH_EVENT_RX_GFT_UPDATE_FILTER,
	MAX_ETH_EVENT_OPCODE
};


/*
 * Classify rule types in E2/E3
 */
enum eth_filter_action {
	ETH_FILTER_ACTION_UNUSED,
	ETH_FILTER_ACTION_REMOVE,
	ETH_FILTER_ACTION_ADD,
/* Remove all filters of given type and vport ID. */
	ETH_FILTER_ACTION_REMOVE_ALL,
	MAX_ETH_FILTER_ACTION
};


/*
 * Command for adding/removing a classification rule $$KEEP_ENDIANNESS$$
 */
struct eth_filter_cmd {
	u8 type /* Filter Type (MAC/VLAN/Pair/VNI) */;
	u8 vport_id /* the vport id */;
	u8 action /* filter command action: add/remove/replace */;
	u8 reserved0;
	__le32 vni;
	__le16 mac_lsb;
	__le16 mac_mid;
	__le16 mac_msb;
	__le16 vlan_id;
};


/*
 *  $$KEEP_ENDIANNESS$$
 */
struct eth_filter_cmd_header {
	u8 rx /* If set, apply these commands to the RX path */;
	u8 tx /* If set, apply these commands to the TX path */;
	u8 cmd_cnt /* Number of filter commands */;
/* 0 - dont assert in case of filter configuration error. Just return an error
 * code. 1 - assert in case of filter configuration error.
 */
	u8 assert_on_error;
	u8 reserved1[4];
};


/*
 * Ethernet filter types: mac/vlan/pair
 */
enum eth_filter_type {
	ETH_FILTER_TYPE_UNUSED,
	ETH_FILTER_TYPE_MAC /* Add/remove a MAC address */,
	ETH_FILTER_TYPE_VLAN /* Add/remove a VLAN */,
	ETH_FILTER_TYPE_PAIR /* Add/remove a MAC-VLAN pair */,
	ETH_FILTER_TYPE_INNER_MAC /* Add/remove a inner MAC address */,
	ETH_FILTER_TYPE_INNER_VLAN /* Add/remove a inner VLAN */,
	ETH_FILTER_TYPE_INNER_PAIR /* Add/remove a inner MAC-VLAN pair */,
/* Add/remove a inner MAC-VNI pair */
	ETH_FILTER_TYPE_INNER_MAC_VNI_PAIR,
	ETH_FILTER_TYPE_MAC_VNI_PAIR /* Add/remove a MAC-VNI pair */,
	ETH_FILTER_TYPE_VNI /* Add/remove a VNI */,
	MAX_ETH_FILTER_TYPE
};


/*
 * eth IPv4 Fragment Type
 */
enum eth_ipv4_frag_type {
	ETH_IPV4_NOT_FRAG /* IPV4 Packet Not Fragmented */,
/* First Fragment of IPv4 Packet (contains headers) */
	ETH_IPV4_FIRST_FRAG,
/* Non-First Fragment of IPv4 Packet (does not contain headers) */
	ETH_IPV4_NON_FIRST_FRAG,
	MAX_ETH_IPV4_FRAG_TYPE
};


/*
 * eth IPv4 Fragment Type
 */
enum eth_ip_type {
	ETH_IPV4 /* IPv4 */,
	ETH_IPV6 /* IPv6 */,
	MAX_ETH_IP_TYPE
};


/*
 * Ethernet Ramrod Command IDs
 */
enum eth_ramrod_cmd_id {
	ETH_RAMROD_UNUSED,
	ETH_RAMROD_VPORT_START /* VPort Start Ramrod */,
	ETH_RAMROD_VPORT_UPDATE /* VPort Update Ramrod */,
	ETH_RAMROD_VPORT_STOP /* VPort Stop Ramrod */,
	ETH_RAMROD_RX_QUEUE_START /* RX Queue Start Ramrod */,
	ETH_RAMROD_RX_QUEUE_STOP /* RX Queue Stop Ramrod */,
	ETH_RAMROD_TX_QUEUE_START /* TX Queue Start Ramrod */,
	ETH_RAMROD_TX_QUEUE_STOP /* TX Queue Stop Ramrod */,
	ETH_RAMROD_FILTERS_UPDATE /* Add or Remove Mac/Vlan/Pair filters */,
	ETH_RAMROD_RX_QUEUE_UPDATE /* RX Queue Update Ramrod */,
/* RX - Create an Openflow Action */
	ETH_RAMROD_RX_CREATE_OPENFLOW_ACTION,
/* RX - Add an Openflow Filter to the Searcher */
	ETH_RAMROD_RX_ADD_OPENFLOW_FILTER,
/* RX - Delete an Openflow Filter to the Searcher */
	ETH_RAMROD_RX_DELETE_OPENFLOW_FILTER,
/* RX - Add a UDP Filter to the Searcher */
	ETH_RAMROD_RX_ADD_UDP_FILTER,
/* RX - Delete a UDP Filter to the Searcher */
	ETH_RAMROD_RX_DELETE_UDP_FILTER,
	ETH_RAMROD_RX_CREATE_GFT_ACTION /* RX - Create a Gft Action */,
/* RX - Add/Delete a GFT Filter to the Searcher */
	ETH_RAMROD_GFT_UPDATE_FILTER,
	MAX_ETH_RAMROD_CMD_ID
};


/*
 * return code from eth sp ramrods
 */
struct eth_return_code {
	u8 value;
/* error code (use enum eth_error_code) */
#define ETH_RETURN_CODE_ERR_CODE_MASK  0x1F
#define ETH_RETURN_CODE_ERR_CODE_SHIFT 0
#define ETH_RETURN_CODE_RESERVED_MASK  0x3
#define ETH_RETURN_CODE_RESERVED_SHIFT 5
/* rx path - 0, tx path - 1 */
#define ETH_RETURN_CODE_RX_TX_MASK     0x1
#define ETH_RETURN_CODE_RX_TX_SHIFT    7
};


/*
 * What to do in case an error occurs
 */
enum eth_tx_err {
	ETH_TX_ERR_DROP /* Drop erroneous packet. */,
/* Assert an interrupt for PF, declare as malicious for VF */
	ETH_TX_ERR_ASSERT_MALICIOUS,
	MAX_ETH_TX_ERR
};


/*
 * Array of the different error type behaviors
 */
struct eth_tx_err_vals {
	__le16 values;
/* Wrong VLAN insertion mode (use enum eth_tx_err) */
#define ETH_TX_ERR_VALS_ILLEGAL_VLAN_MODE_MASK            0x1
#define ETH_TX_ERR_VALS_ILLEGAL_VLAN_MODE_SHIFT           0
/* Packet is below minimal size (use enum eth_tx_err) */
#define ETH_TX_ERR_VALS_PACKET_TOO_SMALL_MASK             0x1
#define ETH_TX_ERR_VALS_PACKET_TOO_SMALL_SHIFT            1
/* Vport has sent spoofed packet (use enum eth_tx_err) */
#define ETH_TX_ERR_VALS_ANTI_SPOOFING_ERR_MASK            0x1
#define ETH_TX_ERR_VALS_ANTI_SPOOFING_ERR_SHIFT           2
/* Packet with illegal type of inband tag (use enum eth_tx_err) */
#define ETH_TX_ERR_VALS_ILLEGAL_INBAND_TAGS_MASK          0x1
#define ETH_TX_ERR_VALS_ILLEGAL_INBAND_TAGS_SHIFT         3
/* Packet marked for VLAN insertion when inband tag is present
 * (use enum eth_tx_err)
 */
#define ETH_TX_ERR_VALS_VLAN_INSERTION_W_INBAND_TAG_MASK  0x1
#define ETH_TX_ERR_VALS_VLAN_INSERTION_W_INBAND_TAG_SHIFT 4
/* Non LSO packet larger than MTU (use enum eth_tx_err) */
#define ETH_TX_ERR_VALS_MTU_VIOLATION_MASK                0x1
#define ETH_TX_ERR_VALS_MTU_VIOLATION_SHIFT               5
/* VF/PF has sent LLDP/PFC or any other type of control packet which is not
 * allowed to (use enum eth_tx_err)
 */
#define ETH_TX_ERR_VALS_ILLEGAL_CONTROL_FRAME_MASK        0x1
#define ETH_TX_ERR_VALS_ILLEGAL_CONTROL_FRAME_SHIFT       6
#define ETH_TX_ERR_VALS_RESERVED_MASK                     0x1FF
#define ETH_TX_ERR_VALS_RESERVED_SHIFT                    7
};


/*
 * vport rss configuration data
 */
struct eth_vport_rss_config {
	__le16 capabilities;
/* configuration of the IpV4 2-tuple capability */
#define ETH_VPORT_RSS_CONFIG_IPV4_CAPABILITY_MASK        0x1
#define ETH_VPORT_RSS_CONFIG_IPV4_CAPABILITY_SHIFT       0
/* configuration of the IpV6 2-tuple capability */
#define ETH_VPORT_RSS_CONFIG_IPV6_CAPABILITY_MASK        0x1
#define ETH_VPORT_RSS_CONFIG_IPV6_CAPABILITY_SHIFT       1
/* configuration of the IpV4 4-tuple capability for TCP */
#define ETH_VPORT_RSS_CONFIG_IPV4_TCP_CAPABILITY_MASK    0x1
#define ETH_VPORT_RSS_CONFIG_IPV4_TCP_CAPABILITY_SHIFT   2
/* configuration of the IpV6 4-tuple capability for TCP */
#define ETH_VPORT_RSS_CONFIG_IPV6_TCP_CAPABILITY_MASK    0x1
#define ETH_VPORT_RSS_CONFIG_IPV6_TCP_CAPABILITY_SHIFT   3
/* configuration of the IpV4 4-tuple capability for UDP */
#define ETH_VPORT_RSS_CONFIG_IPV4_UDP_CAPABILITY_MASK    0x1
#define ETH_VPORT_RSS_CONFIG_IPV4_UDP_CAPABILITY_SHIFT   4
/* configuration of the IpV6 4-tuple capability for UDP */
#define ETH_VPORT_RSS_CONFIG_IPV6_UDP_CAPABILITY_MASK    0x1
#define ETH_VPORT_RSS_CONFIG_IPV6_UDP_CAPABILITY_SHIFT   5
/* configuration of the 5-tuple capability */
#define ETH_VPORT_RSS_CONFIG_EN_5_TUPLE_CAPABILITY_MASK  0x1
#define ETH_VPORT_RSS_CONFIG_EN_5_TUPLE_CAPABILITY_SHIFT 6
/* if set update the rss keys */
#define ETH_VPORT_RSS_CONFIG_RESERVED0_MASK              0x1FF
#define ETH_VPORT_RSS_CONFIG_RESERVED0_SHIFT             7
/* The RSS engine ID. Must be allocated to each vport with RSS enabled.
 * Total number of RSS engines is ETH_RSS_ENGINE_NUM_ , according to chip type.
 */
	u8 rss_id;
	u8 rss_mode /* The RSS mode for this function */;
	u8 update_rss_key /* if set update the rss key */;
	u8 update_rss_ind_table /* if set update the indirection table */;
	u8 update_rss_capabilities /* if set update the capabilities */;
	u8 tbl_size /* rss mask (Tbl size) */;
	__le32 reserved2[2];
/* RSS indirection table */
	__le16 indirection_table[ETH_RSS_IND_TABLE_ENTRIES_NUM];
/* RSS key supplied to us by OS */
	__le32 rss_key[ETH_RSS_KEY_SIZE_REGS];
	__le32 reserved3[2];
};


/*
 * eth vport RSS mode
 */
enum eth_vport_rss_mode {
	ETH_VPORT_RSS_MODE_DISABLED /* RSS Disabled */,
	ETH_VPORT_RSS_MODE_REGULAR /* Regular (ndis-like) RSS */,
	MAX_ETH_VPORT_RSS_MODE
};


/*
 * Command for setting classification flags for a vport $$KEEP_ENDIANNESS$$
 */
struct eth_vport_rx_mode {
	__le16 state;
/* drop all unicast packets */
#define ETH_VPORT_RX_MODE_UCAST_DROP_ALL_MASK          0x1
#define ETH_VPORT_RX_MODE_UCAST_DROP_ALL_SHIFT         0
/* accept all unicast packets (subject to vlan) */
#define ETH_VPORT_RX_MODE_UCAST_ACCEPT_ALL_MASK        0x1
#define ETH_VPORT_RX_MODE_UCAST_ACCEPT_ALL_SHIFT       1
/* accept all unmatched unicast packets */
#define ETH_VPORT_RX_MODE_UCAST_ACCEPT_UNMATCHED_MASK  0x1
#define ETH_VPORT_RX_MODE_UCAST_ACCEPT_UNMATCHED_SHIFT 2
/* drop all multicast packets */
#define ETH_VPORT_RX_MODE_MCAST_DROP_ALL_MASK          0x1
#define ETH_VPORT_RX_MODE_MCAST_DROP_ALL_SHIFT         3
/* accept all multicast packets (subject to vlan) */
#define ETH_VPORT_RX_MODE_MCAST_ACCEPT_ALL_MASK        0x1
#define ETH_VPORT_RX_MODE_MCAST_ACCEPT_ALL_SHIFT       4
/* accept all broadcast packets (subject to vlan) */
#define ETH_VPORT_RX_MODE_BCAST_ACCEPT_ALL_MASK        0x1
#define ETH_VPORT_RX_MODE_BCAST_ACCEPT_ALL_SHIFT       5
#define ETH_VPORT_RX_MODE_RESERVED1_MASK               0x3FF
#define ETH_VPORT_RX_MODE_RESERVED1_SHIFT              6
	__le16 reserved2[3];
};


/*
 * Command for setting tpa parameters
 */
struct eth_vport_tpa_param {
	u8 tpa_ipv4_en_flg /* Enable TPA for IPv4 packets */;
	u8 tpa_ipv6_en_flg /* Enable TPA for IPv6 packets */;
	u8 tpa_ipv4_tunn_en_flg /* Enable TPA for IPv4 over tunnel */;
	u8 tpa_ipv6_tunn_en_flg /* Enable TPA for IPv6 over tunnel */;
/* If set, start each tpa segment on new SGE (GRO mode). One SGE per segment
 * allowed
 */
	u8 tpa_pkt_split_flg;
/* If set, put header of first TPA segment on bd and data on SGE */
	u8 tpa_hdr_data_split_flg;
/* If set, GRO data consistent will checked for TPA continue */
	u8 tpa_gro_consistent_flg;
/* maximum number of opened aggregations per v-port  */
	u8 tpa_max_aggs_num;
	__le16 tpa_max_size /* maximal size for the aggregated TPA packets */;
/* minimum TCP payload size for a packet to start aggregation */
	__le16 tpa_min_size_to_start;
/* minimum TCP payload size for a packet to continue aggregation */
	__le16 tpa_min_size_to_cont;
/* maximal number of buffers that can be used for one aggregation */
	u8 max_buff_num;
	u8 reserved;
};


/*
 * Command for setting classification flags for a vport $$KEEP_ENDIANNESS$$
 */
struct eth_vport_tx_mode {
	__le16 state;
/* drop all unicast packets */
#define ETH_VPORT_TX_MODE_UCAST_DROP_ALL_MASK    0x1
#define ETH_VPORT_TX_MODE_UCAST_DROP_ALL_SHIFT   0
/* accept all unicast packets (subject to vlan) */
#define ETH_VPORT_TX_MODE_UCAST_ACCEPT_ALL_MASK  0x1
#define ETH_VPORT_TX_MODE_UCAST_ACCEPT_ALL_SHIFT 1
/* drop all multicast packets */
#define ETH_VPORT_TX_MODE_MCAST_DROP_ALL_MASK    0x1
#define ETH_VPORT_TX_MODE_MCAST_DROP_ALL_SHIFT   2
/* accept all multicast packets (subject to vlan) */
#define ETH_VPORT_TX_MODE_MCAST_ACCEPT_ALL_MASK  0x1
#define ETH_VPORT_TX_MODE_MCAST_ACCEPT_ALL_SHIFT 3
/* accept all broadcast packets (subject to vlan) */
#define ETH_VPORT_TX_MODE_BCAST_ACCEPT_ALL_MASK  0x1
#define ETH_VPORT_TX_MODE_BCAST_ACCEPT_ALL_SHIFT 4
#define ETH_VPORT_TX_MODE_RESERVED1_MASK         0x7FF
#define ETH_VPORT_TX_MODE_RESERVED1_SHIFT        5
	__le16 reserved2[3];
};


/*
 * Ramrod data for rx create gft action
 */
enum gft_filter_update_action {
	GFT_ADD_FILTER,
	GFT_DELETE_FILTER,
	MAX_GFT_FILTER_UPDATE_ACTION
};


/*
 * Ramrod data for rx create gft action
 */
enum gft_logic_filter_type {
	GFT_FILTER_TYPE /* flow FW is GFT-logic as well */,
	RFS_FILTER_TYPE /* flow FW is A-RFS-logic */,
	MAX_GFT_LOGIC_FILTER_TYPE
};




/*
 * Ramrod data for rx add openflow filter
 */
struct rx_add_openflow_filter_data {
	__le16 action_icid /* CID of Action to run for this filter */;
	u8 priority /* Searcher String - Packet priority */;
	u8 reserved0;
	__le32 tenant_id /* Searcher String - Tenant ID */;
/* Searcher String - Destination Mac Bytes 0 to 1 */
	__le16 dst_mac_hi;
/* Searcher String - Destination Mac Bytes 2 to 3 */
	__le16 dst_mac_mid;
/* Searcher String - Destination Mac Bytes 4 to 5 */
	__le16 dst_mac_lo;
	__le16 src_mac_hi /* Searcher String - Source Mac 0 to 1 */;
	__le16 src_mac_mid /* Searcher String - Source Mac 2 to 3 */;
	__le16 src_mac_lo /* Searcher String - Source Mac 4 to 5 */;
	__le16 vlan_id /* Searcher String - Vlan ID */;
	__le16 l2_eth_type /* Searcher String - Last L2 Ethertype */;
	u8 ipv4_dscp /* Searcher String - IPv4 6 MSBs of the TOS Field */;
	u8 ipv4_frag_type /* Searcher String - IPv4 Fragmentation Type */;
	u8 ipv4_over_ip /* Searcher String - IPv4 Over IP Type */;
	u8 tenant_id_exists /* Searcher String - Tenant ID Exists */;
	__le32 ipv4_dst_addr /* Searcher String - IPv4 Destination Address */;
	__le32 ipv4_src_addr /* Searcher String - IPv4 Source Address */;
	__le16 l4_dst_port /* Searcher String - TCP/UDP Destination Port */;
	__le16 l4_src_port /* Searcher String - TCP/UDP Source Port */;
};


/*
 * Ramrod data for rx create gft action
 */
struct rx_create_gft_action_data {
	u8 vport_id /* Vport Id of GFT Action  */;
	u8 reserved[7];
};


/*
 * Ramrod data for rx create openflow action
 */
struct rx_create_openflow_action_data {
	u8 vport_id /* ID of RX queue */;
	u8 reserved[7];
};


/*
 * Ramrod data for rx queue start ramrod
 */
struct rx_queue_start_ramrod_data {
	__le16 rx_queue_id /* ID of RX queue */;
	__le16 num_of_pbl_pages /* Num of pages in CQE PBL */;
	__le16 bd_max_bytes /* maximal bytes that can be places on the bd */;
	__le16 sb_id /* Status block ID */;
	u8 sb_index /* index of the protocol index */;
	u8 vport_id /* ID of virtual port */;
	u8 default_rss_queue_flg /* set queue as default rss queue if set */;
	u8 complete_cqe_flg /* post completion to the CQE ring if set */;
	u8 complete_event_flg /* post completion to the event ring if set */;
	u8 stats_counter_id /* Statistics counter ID */;
	u8 pin_context /* Pin context in CCFC to improve performance */;
	u8 pxp_tph_valid_bd /* PXP command TPH Valid - for BD/SGE fetch */;
/* PXP command TPH Valid - for packet placement */
	u8 pxp_tph_valid_pkt;
/* PXP command Steering tag hint. Use enum pxp_tph_st_hint */
	u8 pxp_st_hint;
	__le16 pxp_st_index /* PXP command Steering tag index */;
/* Indicates that current queue belongs to poll-mode driver */
	u8 pmd_mode;
/* Indicates that the current queue is using the TX notification queue
 * mechanism - should be set only for PMD queue
 */
	u8 notify_en;
/* Initial value for the toggle valid bit - used in PMD mode */
	u8 toggle_val;
/* Index of RX producers in VF zone. Used for VF only. */
	u8 vf_rx_prod_index;
/* Backward compatibility mode. If set, unprotected mStorm queue zone will used
 * for VF RX producers instead of VF zone.
 */
	u8 vf_rx_prod_use_zone_a;
	u8 reserved[5];
	__le16 reserved1 /* FW reserved. */;
	struct regpair cqe_pbl_addr /* Base address on host of CQE PBL */;
	struct regpair bd_base /* bd address of the first bd page */;
	struct regpair reserved2 /* FW reserved. */;
};


/*
 * Ramrod data for rx queue stop ramrod
 */
struct rx_queue_stop_ramrod_data {
	__le16 rx_queue_id /* ID of RX queue */;
	u8 complete_cqe_flg /* post completion to the CQE ring if set */;
	u8 complete_event_flg /* post completion to the event ring if set */;
	u8 vport_id /* ID of virtual port */;
	u8 reserved[3];
};


/*
 * Ramrod data for rx queue update ramrod
 */
struct rx_queue_update_ramrod_data {
	__le16 rx_queue_id /* ID of RX queue */;
	u8 complete_cqe_flg /* post completion to the CQE ring if set */;
	u8 complete_event_flg /* post completion to the event ring if set */;
	u8 vport_id /* ID of virtual port */;
	u8 reserved[4];
	u8 reserved1 /* FW reserved. */;
	u8 reserved2 /* FW reserved. */;
	u8 reserved3 /* FW reserved. */;
	__le16 reserved4 /* FW reserved. */;
	__le16 reserved5 /* FW reserved. */;
	struct regpair reserved6 /* FW reserved. */;
};


/*
 * Ramrod data for rx Add UDP Filter
 */
struct rx_udp_filter_data {
	__le16 action_icid /* CID of Action to run for this filter */;
	__le16 vlan_id /* Searcher String - Vlan ID */;
	u8 ip_type /* Searcher String - IP Type */;
	u8 tenant_id_exists /* Searcher String - Tenant ID Exists */;
	__le16 reserved1;
/* Searcher String - IP Destination Address, for IPv4 use ip_dst_addr[0] only */
	__le32 ip_dst_addr[4];
/* Searcher String - IP Source Address, for IPv4 use ip_dst_addr[0] only */
	__le32 ip_src_addr[4];
	__le16 udp_dst_port /* Searcher String - UDP Destination Port */;
	__le16 udp_src_port /* Searcher String - UDP Source Port */;
	__le32 tenant_id /* Searcher String - Tenant ID */;
};


/*
 * Ramrod to add filter - filter is packet headr of type of packet wished to
 * pass certin FW flow
 */
struct rx_update_gft_filter_data {
/* Pointer to Packet Header That Defines GFT Filter */
	struct regpair pkt_hdr_addr;
	__le16 pkt_hdr_length /* Packet Header Length */;
/* If is_rfs flag is set: Queue Id to associate filter with else: action icid */
	__le16 rx_qid_or_action_icid;
/* Field is used if is_rfs flag is set: vport Id of which to associate filter
 * with
 */
	u8 vport_id;
/* Use enum to set type of flow using gft HW logic blocks */
	u8 filter_type;
	u8 filter_action /* Use to set type of action on filter */;
	u8 reserved;
};



/*
 * Ramrod data for tx queue start ramrod
 */
struct tx_queue_start_ramrod_data {
	__le16 sb_id /* Status block ID */;
	u8 sb_index /* Status block protocol index */;
	u8 vport_id /* VPort ID */;
	u8 reserved0 /* FW reserved. (qcn_rl_en) */;
	u8 stats_counter_id /* Statistics counter ID to use */;
	__le16 qm_pq_id /* QM PQ ID */;
	u8 flags;
/* 0: Enable QM opportunistic flow. 1: Disable QM opportunistic flow */
#define TX_QUEUE_START_RAMROD_DATA_DISABLE_OPPORTUNISTIC_MASK  0x1
#define TX_QUEUE_START_RAMROD_DATA_DISABLE_OPPORTUNISTIC_SHIFT 0
/* If set, Test Mode - packets will be duplicated by Xstorm handler */
#define TX_QUEUE_START_RAMROD_DATA_TEST_MODE_PKT_DUP_MASK      0x1
#define TX_QUEUE_START_RAMROD_DATA_TEST_MODE_PKT_DUP_SHIFT     1
/* If set, Test Mode - packets destination will be determined by dest_port_mode
 * field from Tx BD
 */
#define TX_QUEUE_START_RAMROD_DATA_TEST_MODE_TX_DEST_MASK      0x1
#define TX_QUEUE_START_RAMROD_DATA_TEST_MODE_TX_DEST_SHIFT     2
/* Indicates that current queue belongs to poll-mode driver */
#define TX_QUEUE_START_RAMROD_DATA_PMD_MODE_MASK               0x1
#define TX_QUEUE_START_RAMROD_DATA_PMD_MODE_SHIFT              3
/* Indicates that the current queue is using the TX notification queue
 * mechanism - should be set only for PMD queue
 */
#define TX_QUEUE_START_RAMROD_DATA_NOTIFY_EN_MASK              0x1
#define TX_QUEUE_START_RAMROD_DATA_NOTIFY_EN_SHIFT             4
/* Pin context in CCFC to improve performance */
#define TX_QUEUE_START_RAMROD_DATA_PIN_CONTEXT_MASK            0x1
#define TX_QUEUE_START_RAMROD_DATA_PIN_CONTEXT_SHIFT           5
#define TX_QUEUE_START_RAMROD_DATA_RESERVED1_MASK              0x3
#define TX_QUEUE_START_RAMROD_DATA_RESERVED1_SHIFT             6
	u8 pxp_st_hint /* PXP command Steering tag hint */;
	u8 pxp_tph_valid_bd /* PXP command TPH Valid - for BD fetch */;
	u8 pxp_tph_valid_pkt /* PXP command TPH Valid - for packet fetch */;
	__le16 pxp_st_index /* PXP command Steering tag index */;
/* TX completion min agg size - for PMD queues */
	__le16 comp_agg_size;
	__le16 queue_zone_id /* queue zone ID to use */;
	__le16 reserved2 /* FW reserved. (test_dup_count) */;
	__le16 pbl_size /* Number of BD pages pointed by PBL */;
/* unique Queue ID - currently used only by PMD flow */
	__le16 tx_queue_id;
/* Unique Same-As-Last Resource ID - improves performance for same-as-last
 * packets per connection (range 0..ETH_TX_NUM_SAME_AS_LAST_ENTRIES-1 IDs
 * available)
 */
	__le16 same_as_last_id;
	__le16 reserved[3];
	struct regpair pbl_base_addr /* address of the pbl page */;
/* BD consumer address in host - for PMD queues */
	struct regpair bd_cons_address;
};


/*
 * Ramrod data for tx queue stop ramrod
 */
struct tx_queue_stop_ramrod_data {
	__le16 reserved[4];
};



/*
 * Ramrod data for vport update ramrod
 */
struct vport_filter_update_ramrod_data {
/* Header for Filter Commands (RX/TX, Add/Remove/Replace, etc) */
	struct eth_filter_cmd_header filter_cmd_hdr;
/* Filter Commands */
	struct eth_filter_cmd filter_cmds[ETH_FILTER_RULES_COUNT];
};


/*
 * Ramrod data for vport start ramrod
 */
struct vport_start_ramrod_data {
	u8 vport_id;
	u8 sw_fid;
	__le16 mtu;
	u8 drop_ttl0_en /* if set, drop packet with ttl=0 */;
	u8 inner_vlan_removal_en;
	struct eth_vport_rx_mode rx_mode /* Rx filter data */;
	struct eth_vport_tx_mode tx_mode /* Tx filter data */;
/* TPA configuration parameters */
	struct eth_vport_tpa_param tpa_param;
	__le16 default_vlan /* Default Vlan value to be forced by FW */;
	u8 tx_switching_en /* Tx switching is enabled for current Vport */;
/* Anti-spoofing verification is set for current Vport */
	u8 anti_spoofing_en;
/* If set, the default Vlan value is forced by the FW */
	u8 default_vlan_en;
/* If set, the vport handles PTP Timesync Packets */
	u8 handle_ptp_pkts;
/* If enable then innerVlan will be striped and not written to cqe */
	u8 silent_vlan_removal_en;
/* If set untagged filter (vlan0) is added to current Vport, otherwise port is
 * marked as any-vlan
 */
	u8 untagged;
/* Desired behavior per TX error type */
	struct eth_tx_err_vals tx_err_behav;
/* If set, ETH header padding will not inserted. placement_offset will be zero.
 */
	u8 zero_placement_offset;
/* If set, Contorl frames will be filtered according to MAC check. */
	u8 ctl_frame_mac_check_en;
/* If set, Contorl frames will be filtered according to ethtype check. */
	u8 ctl_frame_ethtype_check_en;
	u8 reserved[5];
};


/*
 * Ramrod data for vport stop ramrod
 */
struct vport_stop_ramrod_data {
	u8 vport_id;
	u8 reserved[7];
};


/*
 * Ramrod data for vport update ramrod
 */
struct vport_update_ramrod_data_cmn {
	u8 vport_id;
	u8 update_rx_active_flg /* set if rx active flag should be handled */;
	u8 rx_active_flg /* rx active flag value */;
	u8 update_tx_active_flg /* set if tx active flag should be handled */;
	u8 tx_active_flg /* tx active flag value */;
	u8 update_rx_mode_flg /* set if rx state data should be handled */;
	u8 update_tx_mode_flg /* set if tx state data should be handled */;
/* set if approx. mcast data should be handled */
	u8 update_approx_mcast_flg;
	u8 update_rss_flg /* set if rss data should be handled  */;
/* set if inner_vlan_removal_en should be handled */
	u8 update_inner_vlan_removal_en_flg;
	u8 inner_vlan_removal_en;
/* set if tpa parameters should be handled, TPA must be disable before */
	u8 update_tpa_param_flg;
	u8 update_tpa_en_flg /* set if tpa enable changes */;
/* set if tx switching en flag should be handled */
	u8 update_tx_switching_en_flg;
	u8 tx_switching_en /* tx switching en value */;
/* set if anti spoofing flag should be handled */
	u8 update_anti_spoofing_en_flg;
	u8 anti_spoofing_en /* Anti-spoofing verification en value */;
/* set if handle_ptp_pkts should be handled. */
	u8 update_handle_ptp_pkts;
/* If set, the vport handles PTP Timesync Packets */
	u8 handle_ptp_pkts;
/* If set, the default Vlan enable flag is updated */
	u8 update_default_vlan_en_flg;
/* If set, the default Vlan value is forced by the FW */
	u8 default_vlan_en;
/* If set, the default Vlan value is updated */
	u8 update_default_vlan_flg;
	__le16 default_vlan /* Default Vlan value to be forced by FW */;
/* set if accept_any_vlan should be handled */
	u8 update_accept_any_vlan_flg;
	u8 accept_any_vlan /* accept_any_vlan updated value */;
/* Set to remove vlan silently, update_inner_vlan_removal_en_flg must be enabled
 * as well. If Rx is in noSgl mode send rx_queue_update_ramrod_data
 */
	u8 silent_vlan_removal_en;
/* If set, MTU will be updated. Vport must be not active. */
	u8 update_mtu_flg;
	__le16 mtu /* New MTU value. Used if update_mtu_flg are set */;
	u8 reserved[2];
};

struct vport_update_ramrod_mcast {
	__le32 bins[ETH_MULTICAST_MAC_BINS_IN_REGS] /* multicast bins */;
};

/*
 * Ramrod data for vport update ramrod
 */
struct vport_update_ramrod_data {
/* Common data for all vport update ramrods */
	struct vport_update_ramrod_data_cmn common;
	struct eth_vport_rx_mode rx_mode /* vport rx mode bitmap */;
	struct eth_vport_tx_mode tx_mode /* vport tx mode bitmap */;
/* TPA configuration parameters */
	struct eth_vport_tpa_param tpa_param;
	struct vport_update_ramrod_mcast approx_mcast;
	struct eth_vport_rss_config rss_config /* rss config data */;
};






/*
 * GFT CAM line struct
 */
struct gft_cam_line {
	__le32 camline;
/* Indication if the line is valid. */
#define GFT_CAM_LINE_VALID_MASK      0x1
#define GFT_CAM_LINE_VALID_SHIFT     0
/* Data bits, the word that compared with the profile key */
#define GFT_CAM_LINE_DATA_MASK       0x3FFF
#define GFT_CAM_LINE_DATA_SHIFT      1
/* Mask bits, indicate the bits in the data that are Dont-Care */
#define GFT_CAM_LINE_MASK_BITS_MASK  0x3FFF
#define GFT_CAM_LINE_MASK_BITS_SHIFT 15
#define GFT_CAM_LINE_RESERVED1_MASK  0x7
#define GFT_CAM_LINE_RESERVED1_SHIFT 29
};


/*
 * GFT CAM line struct (for driversim use)
 */
struct gft_cam_line_mapped {
	__le32 camline;
/* Indication if the line is valid. */
#define GFT_CAM_LINE_MAPPED_VALID_MASK                     0x1
#define GFT_CAM_LINE_MAPPED_VALID_SHIFT                    0
/* use enum gft_profile_ip_version (use enum gft_profile_ip_version) */
#define GFT_CAM_LINE_MAPPED_IP_VERSION_MASK                0x1
#define GFT_CAM_LINE_MAPPED_IP_VERSION_SHIFT               1
/* use enum gft_profile_ip_version (use enum gft_profile_ip_version) */
#define GFT_CAM_LINE_MAPPED_TUNNEL_IP_VERSION_MASK         0x1
#define GFT_CAM_LINE_MAPPED_TUNNEL_IP_VERSION_SHIFT        2
/* use enum gft_profile_upper_protocol_type
 * (use enum gft_profile_upper_protocol_type)
 */
#define GFT_CAM_LINE_MAPPED_UPPER_PROTOCOL_TYPE_MASK       0xF
#define GFT_CAM_LINE_MAPPED_UPPER_PROTOCOL_TYPE_SHIFT      3
/* use enum gft_profile_tunnel_type (use enum gft_profile_tunnel_type) */
#define GFT_CAM_LINE_MAPPED_TUNNEL_TYPE_MASK               0xF
#define GFT_CAM_LINE_MAPPED_TUNNEL_TYPE_SHIFT              7
#define GFT_CAM_LINE_MAPPED_PF_ID_MASK                     0xF
#define GFT_CAM_LINE_MAPPED_PF_ID_SHIFT                    11
/* use enum gft_profile_ip_version (use enum gft_profile_ip_version) */
#define GFT_CAM_LINE_MAPPED_IP_VERSION_MASK_MASK           0x1
#define GFT_CAM_LINE_MAPPED_IP_VERSION_MASK_SHIFT          15
/* use enum gft_profile_ip_version (use enum gft_profile_ip_version) */
#define GFT_CAM_LINE_MAPPED_TUNNEL_IP_VERSION_MASK_MASK    0x1
#define GFT_CAM_LINE_MAPPED_TUNNEL_IP_VERSION_MASK_SHIFT   16
/* use enum gft_profile_upper_protocol_type
 * (use enum gft_profile_upper_protocol_type)
 */
#define GFT_CAM_LINE_MAPPED_UPPER_PROTOCOL_TYPE_MASK_MASK  0xF
#define GFT_CAM_LINE_MAPPED_UPPER_PROTOCOL_TYPE_MASK_SHIFT 17
/* use enum gft_profile_tunnel_type (use enum gft_profile_tunnel_type) */
#define GFT_CAM_LINE_MAPPED_TUNNEL_TYPE_MASK_MASK          0xF
#define GFT_CAM_LINE_MAPPED_TUNNEL_TYPE_MASK_SHIFT         21
#define GFT_CAM_LINE_MAPPED_PF_ID_MASK_MASK                0xF
#define GFT_CAM_LINE_MAPPED_PF_ID_MASK_SHIFT               25
#define GFT_CAM_LINE_MAPPED_RESERVED1_MASK                 0x7
#define GFT_CAM_LINE_MAPPED_RESERVED1_SHIFT                29
};


union gft_cam_line_union {
	struct gft_cam_line cam_line;
	struct gft_cam_line_mapped cam_line_mapped;
};


/*
 * Used in gft_profile_key: Indication for ip version
 */
enum gft_profile_ip_version {
	GFT_PROFILE_IPV4 = 0,
	GFT_PROFILE_IPV6 = 1,
	MAX_GFT_PROFILE_IP_VERSION
};


/*
 * Profile key stucr fot GFT logic in Prs
 */
struct gft_profile_key {
	__le16 profile_key;
/* use enum gft_profile_ip_version (use enum gft_profile_ip_version) */
#define GFT_PROFILE_KEY_IP_VERSION_MASK           0x1
#define GFT_PROFILE_KEY_IP_VERSION_SHIFT          0
/* use enum gft_profile_ip_version (use enum gft_profile_ip_version) */
#define GFT_PROFILE_KEY_TUNNEL_IP_VERSION_MASK    0x1
#define GFT_PROFILE_KEY_TUNNEL_IP_VERSION_SHIFT   1
/* use enum gft_profile_upper_protocol_type
 * (use enum gft_profile_upper_protocol_type)
 */
#define GFT_PROFILE_KEY_UPPER_PROTOCOL_TYPE_MASK  0xF
#define GFT_PROFILE_KEY_UPPER_PROTOCOL_TYPE_SHIFT 2
/* use enum gft_profile_tunnel_type (use enum gft_profile_tunnel_type) */
#define GFT_PROFILE_KEY_TUNNEL_TYPE_MASK          0xF
#define GFT_PROFILE_KEY_TUNNEL_TYPE_SHIFT         6
#define GFT_PROFILE_KEY_PF_ID_MASK                0xF
#define GFT_PROFILE_KEY_PF_ID_SHIFT               10
#define GFT_PROFILE_KEY_RESERVED0_MASK            0x3
#define GFT_PROFILE_KEY_RESERVED0_SHIFT           14
};


/*
 * Used in gft_profile_key: Indication for tunnel type
 */
enum gft_profile_tunnel_type {
	GFT_PROFILE_NO_TUNNEL = 0,
	GFT_PROFILE_VXLAN_TUNNEL = 1,
	GFT_PROFILE_GRE_MAC_OR_NVGRE_TUNNEL = 2,
	GFT_PROFILE_GRE_IP_TUNNEL = 3,
	GFT_PROFILE_GENEVE_MAC_TUNNEL = 4,
	GFT_PROFILE_GENEVE_IP_TUNNEL = 5,
	MAX_GFT_PROFILE_TUNNEL_TYPE
};


/*
 * Used in gft_profile_key: Indication for protocol type
 */
enum gft_profile_upper_protocol_type {
	GFT_PROFILE_ROCE_PROTOCOL = 0,
	GFT_PROFILE_RROCE_PROTOCOL = 1,
	GFT_PROFILE_FCOE_PROTOCOL = 2,
	GFT_PROFILE_ICMP_PROTOCOL = 3,
	GFT_PROFILE_ARP_PROTOCOL = 4,
	GFT_PROFILE_USER_TCP_SRC_PORT_1_INNER = 5,
	GFT_PROFILE_USER_TCP_DST_PORT_1_INNER = 6,
	GFT_PROFILE_TCP_PROTOCOL = 7,
	GFT_PROFILE_USER_UDP_DST_PORT_1_INNER = 8,
	GFT_PROFILE_USER_UDP_DST_PORT_2_OUTER = 9,
	GFT_PROFILE_UDP_PROTOCOL = 10,
	GFT_PROFILE_USER_IP_1_INNER = 11,
	GFT_PROFILE_USER_IP_2_OUTER = 12,
	GFT_PROFILE_USER_ETH_1_INNER = 13,
	GFT_PROFILE_USER_ETH_2_OUTER = 14,
	GFT_PROFILE_RAW = 15,
	MAX_GFT_PROFILE_UPPER_PROTOCOL_TYPE
};


/*
 * GFT RAM line struct
 */
struct gft_ram_line {
	__le32 low32bits;
/*  (use enum gft_vlan_select) */
#define GFT_RAM_LINE_VLAN_SELECT_MASK              0x3
#define GFT_RAM_LINE_VLAN_SELECT_SHIFT             0
#define GFT_RAM_LINE_TUNNEL_ENTROPHY_MASK          0x1
#define GFT_RAM_LINE_TUNNEL_ENTROPHY_SHIFT         2
#define GFT_RAM_LINE_TUNNEL_TTL_EQUAL_ONE_MASK     0x1
#define GFT_RAM_LINE_TUNNEL_TTL_EQUAL_ONE_SHIFT    3
#define GFT_RAM_LINE_TUNNEL_TTL_MASK               0x1
#define GFT_RAM_LINE_TUNNEL_TTL_SHIFT              4
#define GFT_RAM_LINE_TUNNEL_ETHERTYPE_MASK         0x1
#define GFT_RAM_LINE_TUNNEL_ETHERTYPE_SHIFT        5
#define GFT_RAM_LINE_TUNNEL_DST_PORT_MASK          0x1
#define GFT_RAM_LINE_TUNNEL_DST_PORT_SHIFT         6
#define GFT_RAM_LINE_TUNNEL_SRC_PORT_MASK          0x1
#define GFT_RAM_LINE_TUNNEL_SRC_PORT_SHIFT         7
#define GFT_RAM_LINE_TUNNEL_DSCP_MASK              0x1
#define GFT_RAM_LINE_TUNNEL_DSCP_SHIFT             8
#define GFT_RAM_LINE_TUNNEL_OVER_IP_PROTOCOL_MASK  0x1
#define GFT_RAM_LINE_TUNNEL_OVER_IP_PROTOCOL_SHIFT 9
#define GFT_RAM_LINE_TUNNEL_DST_IP_MASK            0x1
#define GFT_RAM_LINE_TUNNEL_DST_IP_SHIFT           10
#define GFT_RAM_LINE_TUNNEL_SRC_IP_MASK            0x1
#define GFT_RAM_LINE_TUNNEL_SRC_IP_SHIFT           11
#define GFT_RAM_LINE_TUNNEL_PRIORITY_MASK          0x1
#define GFT_RAM_LINE_TUNNEL_PRIORITY_SHIFT         12
#define GFT_RAM_LINE_TUNNEL_PROVIDER_VLAN_MASK     0x1
#define GFT_RAM_LINE_TUNNEL_PROVIDER_VLAN_SHIFT    13
#define GFT_RAM_LINE_TUNNEL_VLAN_MASK              0x1
#define GFT_RAM_LINE_TUNNEL_VLAN_SHIFT             14
#define GFT_RAM_LINE_TUNNEL_DST_MAC_MASK           0x1
#define GFT_RAM_LINE_TUNNEL_DST_MAC_SHIFT          15
#define GFT_RAM_LINE_TUNNEL_SRC_MAC_MASK           0x1
#define GFT_RAM_LINE_TUNNEL_SRC_MAC_SHIFT          16
#define GFT_RAM_LINE_TTL_EQUAL_ONE_MASK            0x1
#define GFT_RAM_LINE_TTL_EQUAL_ONE_SHIFT           17
#define GFT_RAM_LINE_TTL_MASK                      0x1
#define GFT_RAM_LINE_TTL_SHIFT                     18
#define GFT_RAM_LINE_ETHERTYPE_MASK                0x1
#define GFT_RAM_LINE_ETHERTYPE_SHIFT               19
#define GFT_RAM_LINE_RESERVED0_MASK                0x1
#define GFT_RAM_LINE_RESERVED0_SHIFT               20
#define GFT_RAM_LINE_TCP_FLAG_FIN_MASK             0x1
#define GFT_RAM_LINE_TCP_FLAG_FIN_SHIFT            21
#define GFT_RAM_LINE_TCP_FLAG_SYN_MASK             0x1
#define GFT_RAM_LINE_TCP_FLAG_SYN_SHIFT            22
#define GFT_RAM_LINE_TCP_FLAG_RST_MASK             0x1
#define GFT_RAM_LINE_TCP_FLAG_RST_SHIFT            23
#define GFT_RAM_LINE_TCP_FLAG_PSH_MASK             0x1
#define GFT_RAM_LINE_TCP_FLAG_PSH_SHIFT            24
#define GFT_RAM_LINE_TCP_FLAG_ACK_MASK             0x1
#define GFT_RAM_LINE_TCP_FLAG_ACK_SHIFT            25
#define GFT_RAM_LINE_TCP_FLAG_URG_MASK             0x1
#define GFT_RAM_LINE_TCP_FLAG_URG_SHIFT            26
#define GFT_RAM_LINE_TCP_FLAG_ECE_MASK             0x1
#define GFT_RAM_LINE_TCP_FLAG_ECE_SHIFT            27
#define GFT_RAM_LINE_TCP_FLAG_CWR_MASK             0x1
#define GFT_RAM_LINE_TCP_FLAG_CWR_SHIFT            28
#define GFT_RAM_LINE_TCP_FLAG_NS_MASK              0x1
#define GFT_RAM_LINE_TCP_FLAG_NS_SHIFT             29
#define GFT_RAM_LINE_DST_PORT_MASK                 0x1
#define GFT_RAM_LINE_DST_PORT_SHIFT                30
#define GFT_RAM_LINE_SRC_PORT_MASK                 0x1
#define GFT_RAM_LINE_SRC_PORT_SHIFT                31
	__le32 high32bits;
#define GFT_RAM_LINE_DSCP_MASK                     0x1
#define GFT_RAM_LINE_DSCP_SHIFT                    0
#define GFT_RAM_LINE_OVER_IP_PROTOCOL_MASK         0x1
#define GFT_RAM_LINE_OVER_IP_PROTOCOL_SHIFT        1
#define GFT_RAM_LINE_DST_IP_MASK                   0x1
#define GFT_RAM_LINE_DST_IP_SHIFT                  2
#define GFT_RAM_LINE_SRC_IP_MASK                   0x1
#define GFT_RAM_LINE_SRC_IP_SHIFT                  3
#define GFT_RAM_LINE_PRIORITY_MASK                 0x1
#define GFT_RAM_LINE_PRIORITY_SHIFT                4
#define GFT_RAM_LINE_PROVIDER_VLAN_MASK            0x1
#define GFT_RAM_LINE_PROVIDER_VLAN_SHIFT           5
#define GFT_RAM_LINE_VLAN_MASK                     0x1
#define GFT_RAM_LINE_VLAN_SHIFT                    6
#define GFT_RAM_LINE_DST_MAC_MASK                  0x1
#define GFT_RAM_LINE_DST_MAC_SHIFT                 7
#define GFT_RAM_LINE_SRC_MAC_MASK                  0x1
#define GFT_RAM_LINE_SRC_MAC_SHIFT                 8
#define GFT_RAM_LINE_TENANT_ID_MASK                0x1
#define GFT_RAM_LINE_TENANT_ID_SHIFT               9
#define GFT_RAM_LINE_RESERVED1_MASK                0x3FFFFF
#define GFT_RAM_LINE_RESERVED1_SHIFT               10
};


/*
 * Used in the first 2 bits for gft_ram_line: Indication for vlan mask
 */
enum gft_vlan_select {
	INNER_PROVIDER_VLAN = 0,
	INNER_VLAN = 1,
	OUTER_PROVIDER_VLAN = 2,
	OUTER_VLAN = 3,
	MAX_GFT_VLAN_SELECT
};


struct mstorm_eth_conn_ag_ctx {
	u8 byte0 /* cdu_validation */;
	u8 byte1 /* state */;
	u8 flags0;
/* exist_in_qm0 */
#define MSTORM_ETH_CONN_AG_CTX_EXIST_IN_QM0_MASK  0x1
#define MSTORM_ETH_CONN_AG_CTX_EXIST_IN_QM0_SHIFT 0
/* exist_in_qm1 */
#define MSTORM_ETH_CONN_AG_CTX_BIT1_MASK          0x1
#define MSTORM_ETH_CONN_AG_CTX_BIT1_SHIFT         1
#define MSTORM_ETH_CONN_AG_CTX_CF0_MASK           0x3 /* cf0 */
#define MSTORM_ETH_CONN_AG_CTX_CF0_SHIFT          2
#define MSTORM_ETH_CONN_AG_CTX_CF1_MASK           0x3 /* cf1 */
#define MSTORM_ETH_CONN_AG_CTX_CF1_SHIFT          4
#define MSTORM_ETH_CONN_AG_CTX_CF2_MASK           0x3 /* cf2 */
#define MSTORM_ETH_CONN_AG_CTX_CF2_SHIFT          6
	u8 flags1;
#define MSTORM_ETH_CONN_AG_CTX_CF0EN_MASK         0x1 /* cf0en */
#define MSTORM_ETH_CONN_AG_CTX_CF0EN_SHIFT        0
#define MSTORM_ETH_CONN_AG_CTX_CF1EN_MASK         0x1 /* cf1en */
#define MSTORM_ETH_CONN_AG_CTX_CF1EN_SHIFT        1
#define MSTORM_ETH_CONN_AG_CTX_CF2EN_MASK         0x1 /* cf2en */
#define MSTORM_ETH_CONN_AG_CTX_CF2EN_SHIFT        2
#define MSTORM_ETH_CONN_AG_CTX_RULE0EN_MASK       0x1 /* rule0en */
#define MSTORM_ETH_CONN_AG_CTX_RULE0EN_SHIFT      3
#define MSTORM_ETH_CONN_AG_CTX_RULE1EN_MASK       0x1 /* rule1en */
#define MSTORM_ETH_CONN_AG_CTX_RULE1EN_SHIFT      4
#define MSTORM_ETH_CONN_AG_CTX_RULE2EN_MASK       0x1 /* rule2en */
#define MSTORM_ETH_CONN_AG_CTX_RULE2EN_SHIFT      5
#define MSTORM_ETH_CONN_AG_CTX_RULE3EN_MASK       0x1 /* rule3en */
#define MSTORM_ETH_CONN_AG_CTX_RULE3EN_SHIFT      6
#define MSTORM_ETH_CONN_AG_CTX_RULE4EN_MASK       0x1 /* rule4en */
#define MSTORM_ETH_CONN_AG_CTX_RULE4EN_SHIFT      7
	__le16 word0 /* word0 */;
	__le16 word1 /* word1 */;
	__le32 reg0 /* reg0 */;
	__le32 reg1 /* reg1 */;
};




struct xstormEthConnAgCtxDqExtLdPart {
	u8 reserved0 /* cdu_validation */;
	u8 eth_state /* state */;
	u8 flags0;
/* exist_in_qm0 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_EXIST_IN_QM0_MASK            0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_EXIST_IN_QM0_SHIFT           0
/* exist_in_qm1 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED1_MASK               0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED1_SHIFT              1
/* exist_in_qm2 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED2_MASK               0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED2_SHIFT              2
/* exist_in_qm3 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_EXIST_IN_QM3_MASK            0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_EXIST_IN_QM3_SHIFT           3
/* bit4 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED3_MASK               0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED3_SHIFT              4
/* cf_array_active */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED4_MASK               0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED4_SHIFT              5
/* bit6 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED5_MASK               0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED5_SHIFT              6
/* bit7 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED6_MASK               0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED6_SHIFT              7
	u8 flags1;
/* bit8 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED7_MASK               0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED7_SHIFT              0
/* bit9 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED8_MASK               0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED8_SHIFT              1
/* bit10 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED9_MASK               0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED9_SHIFT              2
/* bit11 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_BIT11_MASK                   0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_BIT11_SHIFT                  3
/* bit12 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_BIT12_MASK                   0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_BIT12_SHIFT                  4
/* bit13 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_BIT13_MASK                   0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_BIT13_SHIFT                  5
/* bit14 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_TX_RULE_ACTIVE_MASK          0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_TX_RULE_ACTIVE_SHIFT         6
/* bit15 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_DQ_CF_ACTIVE_MASK            0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_DQ_CF_ACTIVE_SHIFT           7
	u8 flags2;
/* timer0cf */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF0_MASK                     0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF0_SHIFT                    0
/* timer1cf */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF1_MASK                     0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF1_SHIFT                    2
/* timer2cf */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF2_MASK                     0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF2_SHIFT                    4
/* timer_stop_all */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF3_MASK                     0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF3_SHIFT                    6
	u8 flags3;
/* cf4 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF4_MASK                     0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF4_SHIFT                    0
/* cf5 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF5_MASK                     0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF5_SHIFT                    2
/* cf6 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF6_MASK                     0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF6_SHIFT                    4
/* cf7 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF7_MASK                     0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF7_SHIFT                    6
	u8 flags4;
/* cf8 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF8_MASK                     0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF8_SHIFT                    0
/* cf9 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF9_MASK                     0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF9_SHIFT                    2
/* cf10 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF10_MASK                    0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF10_SHIFT                   4
/* cf11 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF11_MASK                    0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF11_SHIFT                   6
	u8 flags5;
/* cf12 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF12_MASK                    0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF12_SHIFT                   0
/* cf13 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF13_MASK                    0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF13_SHIFT                   2
/* cf14 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF14_MASK                    0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF14_SHIFT                   4
/* cf15 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF15_MASK                    0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF15_SHIFT                   6
	u8 flags6;
/* cf16 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_GO_TO_BD_CONS_CF_MASK        0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_GO_TO_BD_CONS_CF_SHIFT       0
/* cf_array_cf */
#define XSTORMETHCONNAGCTXDQEXTLDPART_MULTI_UNICAST_CF_MASK        0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_MULTI_UNICAST_CF_SHIFT       2
/* cf18 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_DQ_CF_MASK                   0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_DQ_CF_SHIFT                  4
/* cf19 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_TERMINATE_CF_MASK            0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_TERMINATE_CF_SHIFT           6
	u8 flags7;
/* cf20 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_FLUSH_Q0_MASK                0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_FLUSH_Q0_SHIFT               0
/* cf21 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED10_MASK              0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED10_SHIFT             2
/* cf22 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_SLOW_PATH_MASK               0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_SLOW_PATH_SHIFT              4
/* cf0en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF0EN_MASK                   0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF0EN_SHIFT                  6
/* cf1en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF1EN_MASK                   0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF1EN_SHIFT                  7
	u8 flags8;
/* cf2en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF2EN_MASK                   0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF2EN_SHIFT                  0
/* cf3en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF3EN_MASK                   0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF3EN_SHIFT                  1
/* cf4en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF4EN_MASK                   0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF4EN_SHIFT                  2
/* cf5en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF5EN_MASK                   0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF5EN_SHIFT                  3
/* cf6en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF6EN_MASK                   0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF6EN_SHIFT                  4
/* cf7en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF7EN_MASK                   0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF7EN_SHIFT                  5
/* cf8en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF8EN_MASK                   0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF8EN_SHIFT                  6
/* cf9en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF9EN_MASK                   0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF9EN_SHIFT                  7
	u8 flags9;
/* cf10en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF10EN_MASK                  0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF10EN_SHIFT                 0
/* cf11en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF11EN_MASK                  0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF11EN_SHIFT                 1
/* cf12en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF12EN_MASK                  0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF12EN_SHIFT                 2
/* cf13en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF13EN_MASK                  0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF13EN_SHIFT                 3
/* cf14en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF14EN_MASK                  0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF14EN_SHIFT                 4
/* cf15en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF15EN_MASK                  0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_CF15EN_SHIFT                 5
/* cf16en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_GO_TO_BD_CONS_CF_EN_MASK     0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_GO_TO_BD_CONS_CF_EN_SHIFT    6
/* cf_array_cf_en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_MULTI_UNICAST_CF_EN_MASK     0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_MULTI_UNICAST_CF_EN_SHIFT    7
	u8 flags10;
/* cf18en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_DQ_CF_EN_MASK                0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_DQ_CF_EN_SHIFT               0
/* cf19en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_TERMINATE_CF_EN_MASK         0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_TERMINATE_CF_EN_SHIFT        1
/* cf20en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_FLUSH_Q0_EN_MASK             0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_FLUSH_Q0_EN_SHIFT            2
/* cf21en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED11_MASK              0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED11_SHIFT             3
/* cf22en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_SLOW_PATH_EN_MASK            0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_SLOW_PATH_EN_SHIFT           4
/* cf23en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_TPH_ENABLE_EN_RESERVED_MASK  0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_TPH_ENABLE_EN_RESERVED_SHIFT 5
/* rule0en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED12_MASK              0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED12_SHIFT             6
/* rule1en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED13_MASK              0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED13_SHIFT             7
	u8 flags11;
/* rule2en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED14_MASK              0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED14_SHIFT             0
/* rule3en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED15_MASK              0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED15_SHIFT             1
/* rule4en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_TX_DEC_RULE_EN_MASK          0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_TX_DEC_RULE_EN_SHIFT         2
/* rule5en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE5EN_MASK                 0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE5EN_SHIFT                3
/* rule6en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE6EN_MASK                 0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE6EN_SHIFT                4
/* rule7en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE7EN_MASK                 0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE7EN_SHIFT                5
/* rule8en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED1_MASK            0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED1_SHIFT           6
/* rule9en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE9EN_MASK                 0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE9EN_SHIFT                7
	u8 flags12;
/* rule10en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE10EN_MASK                0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE10EN_SHIFT               0
/* rule11en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE11EN_MASK                0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE11EN_SHIFT               1
/* rule12en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED2_MASK            0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED2_SHIFT           2
/* rule13en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED3_MASK            0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED3_SHIFT           3
/* rule14en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE14EN_MASK                0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE14EN_SHIFT               4
/* rule15en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE15EN_MASK                0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE15EN_SHIFT               5
/* rule16en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE16EN_MASK                0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE16EN_SHIFT               6
/* rule17en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE17EN_MASK                0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE17EN_SHIFT               7
	u8 flags13;
/* rule18en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE18EN_MASK                0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE18EN_SHIFT               0
/* rule19en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE19EN_MASK                0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_RULE19EN_SHIFT               1
/* rule20en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED4_MASK            0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED4_SHIFT           2
/* rule21en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED5_MASK            0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED5_SHIFT           3
/* rule22en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED6_MASK            0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED6_SHIFT           4
/* rule23en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED7_MASK            0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED7_SHIFT           5
/* rule24en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED8_MASK            0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED8_SHIFT           6
/* rule25en */
#define XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED9_MASK            0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED9_SHIFT           7
	u8 flags14;
/* bit16 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_USE_EXT_HDR_MASK        0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_USE_EXT_HDR_SHIFT       0
/* bit17 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_SEND_RAW_L3L4_MASK      0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_SEND_RAW_L3L4_SHIFT     1
/* bit18 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_INBAND_PROP_HDR_MASK    0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_INBAND_PROP_HDR_SHIFT   2
/* bit19 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_SEND_EXT_TUNNEL_MASK    0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_SEND_EXT_TUNNEL_SHIFT   3
/* bit20 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_L2_EDPM_ENABLE_MASK          0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_L2_EDPM_ENABLE_SHIFT         4
/* bit21 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_ROCE_EDPM_ENABLE_MASK        0x1
#define XSTORMETHCONNAGCTXDQEXTLDPART_ROCE_EDPM_ENABLE_SHIFT       5
/* cf23 */
#define XSTORMETHCONNAGCTXDQEXTLDPART_TPH_ENABLE_MASK              0x3
#define XSTORMETHCONNAGCTXDQEXTLDPART_TPH_ENABLE_SHIFT             6
	u8 edpm_event_id /* byte2 */;
	__le16 physical_q0 /* physical_q0 */;
	__le16 quota /* physical_q1 */;
	__le16 edpm_num_bds /* physical_q2 */;
	__le16 tx_bd_cons /* word3 */;
	__le16 tx_bd_prod /* word4 */;
	__le16 tx_class /* word5 */;
	__le16 conn_dpi /* conn_dpi */;
	u8 byte3 /* byte3 */;
	u8 byte4 /* byte4 */;
	u8 byte5 /* byte5 */;
	u8 byte6 /* byte6 */;
	__le32 reg0 /* reg0 */;
	__le32 reg1 /* reg1 */;
	__le32 reg2 /* reg2 */;
	__le32 reg3 /* reg3 */;
	__le32 reg4 /* reg4 */;
};



struct xstorm_eth_hw_conn_ag_ctx {
	u8 reserved0 /* cdu_validation */;
	u8 eth_state /* state */;
	u8 flags0;
/* exist_in_qm0 */
#define XSTORM_ETH_HW_CONN_AG_CTX_EXIST_IN_QM0_MASK            0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_EXIST_IN_QM0_SHIFT           0
/* exist_in_qm1 */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED1_MASK               0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED1_SHIFT              1
/* exist_in_qm2 */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED2_MASK               0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED2_SHIFT              2
/* exist_in_qm3 */
#define XSTORM_ETH_HW_CONN_AG_CTX_EXIST_IN_QM3_MASK            0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_EXIST_IN_QM3_SHIFT           3
/* bit4 */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED3_MASK               0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED3_SHIFT              4
/* cf_array_active */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED4_MASK               0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED4_SHIFT              5
/* bit6 */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED5_MASK               0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED5_SHIFT              6
/* bit7 */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED6_MASK               0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED6_SHIFT              7
	u8 flags1;
/* bit8 */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED7_MASK               0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED7_SHIFT              0
/* bit9 */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED8_MASK               0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED8_SHIFT              1
/* bit10 */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED9_MASK               0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED9_SHIFT              2
/* bit11 */
#define XSTORM_ETH_HW_CONN_AG_CTX_BIT11_MASK                   0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_BIT11_SHIFT                  3
/* bit12 */
#define XSTORM_ETH_HW_CONN_AG_CTX_BIT12_MASK                   0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_BIT12_SHIFT                  4
/* bit13 */
#define XSTORM_ETH_HW_CONN_AG_CTX_BIT13_MASK                   0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_BIT13_SHIFT                  5
/* bit14 */
#define XSTORM_ETH_HW_CONN_AG_CTX_TX_RULE_ACTIVE_MASK          0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_TX_RULE_ACTIVE_SHIFT         6
/* bit15 */
#define XSTORM_ETH_HW_CONN_AG_CTX_DQ_CF_ACTIVE_MASK            0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_DQ_CF_ACTIVE_SHIFT           7
	u8 flags2;
/* timer0cf */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF0_MASK                     0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF0_SHIFT                    0
/* timer1cf */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF1_MASK                     0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF1_SHIFT                    2
/* timer2cf */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF2_MASK                     0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF2_SHIFT                    4
/* timer_stop_all */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF3_MASK                     0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF3_SHIFT                    6
	u8 flags3;
/* cf4 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF4_MASK                     0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF4_SHIFT                    0
/* cf5 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF5_MASK                     0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF5_SHIFT                    2
/* cf6 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF6_MASK                     0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF6_SHIFT                    4
/* cf7 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF7_MASK                     0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF7_SHIFT                    6
	u8 flags4;
/* cf8 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF8_MASK                     0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF8_SHIFT                    0
/* cf9 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF9_MASK                     0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF9_SHIFT                    2
/* cf10 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF10_MASK                    0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF10_SHIFT                   4
/* cf11 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF11_MASK                    0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF11_SHIFT                   6
	u8 flags5;
/* cf12 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF12_MASK                    0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF12_SHIFT                   0
/* cf13 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF13_MASK                    0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF13_SHIFT                   2
/* cf14 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF14_MASK                    0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF14_SHIFT                   4
/* cf15 */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF15_MASK                    0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF15_SHIFT                   6
	u8 flags6;
/* cf16 */
#define XSTORM_ETH_HW_CONN_AG_CTX_GO_TO_BD_CONS_CF_MASK        0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_GO_TO_BD_CONS_CF_SHIFT       0
/* cf_array_cf */
#define XSTORM_ETH_HW_CONN_AG_CTX_MULTI_UNICAST_CF_MASK        0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_MULTI_UNICAST_CF_SHIFT       2
/* cf18 */
#define XSTORM_ETH_HW_CONN_AG_CTX_DQ_CF_MASK                   0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_DQ_CF_SHIFT                  4
/* cf19 */
#define XSTORM_ETH_HW_CONN_AG_CTX_TERMINATE_CF_MASK            0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_TERMINATE_CF_SHIFT           6
	u8 flags7;
/* cf20 */
#define XSTORM_ETH_HW_CONN_AG_CTX_FLUSH_Q0_MASK                0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_FLUSH_Q0_SHIFT               0
/* cf21 */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED10_MASK              0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED10_SHIFT             2
/* cf22 */
#define XSTORM_ETH_HW_CONN_AG_CTX_SLOW_PATH_MASK               0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_SLOW_PATH_SHIFT              4
/* cf0en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF0EN_MASK                   0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF0EN_SHIFT                  6
/* cf1en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF1EN_MASK                   0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF1EN_SHIFT                  7
	u8 flags8;
/* cf2en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF2EN_MASK                   0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF2EN_SHIFT                  0
/* cf3en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF3EN_MASK                   0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF3EN_SHIFT                  1
/* cf4en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF4EN_MASK                   0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF4EN_SHIFT                  2
/* cf5en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF5EN_MASK                   0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF5EN_SHIFT                  3
/* cf6en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF6EN_MASK                   0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF6EN_SHIFT                  4
/* cf7en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF7EN_MASK                   0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF7EN_SHIFT                  5
/* cf8en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF8EN_MASK                   0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF8EN_SHIFT                  6
/* cf9en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF9EN_MASK                   0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF9EN_SHIFT                  7
	u8 flags9;
/* cf10en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF10EN_MASK                  0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF10EN_SHIFT                 0
/* cf11en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF11EN_MASK                  0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF11EN_SHIFT                 1
/* cf12en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF12EN_MASK                  0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF12EN_SHIFT                 2
/* cf13en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF13EN_MASK                  0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF13EN_SHIFT                 3
/* cf14en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF14EN_MASK                  0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF14EN_SHIFT                 4
/* cf15en */
#define XSTORM_ETH_HW_CONN_AG_CTX_CF15EN_MASK                  0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF15EN_SHIFT                 5
/* cf16en */
#define XSTORM_ETH_HW_CONN_AG_CTX_GO_TO_BD_CONS_CF_EN_MASK     0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_GO_TO_BD_CONS_CF_EN_SHIFT    6
/* cf_array_cf_en */
#define XSTORM_ETH_HW_CONN_AG_CTX_MULTI_UNICAST_CF_EN_MASK     0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_MULTI_UNICAST_CF_EN_SHIFT    7
	u8 flags10;
/* cf18en */
#define XSTORM_ETH_HW_CONN_AG_CTX_DQ_CF_EN_MASK                0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_DQ_CF_EN_SHIFT               0
/* cf19en */
#define XSTORM_ETH_HW_CONN_AG_CTX_TERMINATE_CF_EN_MASK         0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_TERMINATE_CF_EN_SHIFT        1
/* cf20en */
#define XSTORM_ETH_HW_CONN_AG_CTX_FLUSH_Q0_EN_MASK             0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_FLUSH_Q0_EN_SHIFT            2
/* cf21en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED11_MASK              0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED11_SHIFT             3
/* cf22en */
#define XSTORM_ETH_HW_CONN_AG_CTX_SLOW_PATH_EN_MASK            0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_SLOW_PATH_EN_SHIFT           4
/* cf23en */
#define XSTORM_ETH_HW_CONN_AG_CTX_TPH_ENABLE_EN_RESERVED_MASK  0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_TPH_ENABLE_EN_RESERVED_SHIFT 5
/* rule0en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED12_MASK              0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED12_SHIFT             6
/* rule1en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED13_MASK              0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED13_SHIFT             7
	u8 flags11;
/* rule2en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED14_MASK              0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED14_SHIFT             0
/* rule3en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED15_MASK              0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED15_SHIFT             1
/* rule4en */
#define XSTORM_ETH_HW_CONN_AG_CTX_TX_DEC_RULE_EN_MASK          0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_TX_DEC_RULE_EN_SHIFT         2
/* rule5en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE5EN_MASK                 0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE5EN_SHIFT                3
/* rule6en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE6EN_MASK                 0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE6EN_SHIFT                4
/* rule7en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE7EN_MASK                 0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE7EN_SHIFT                5
/* rule8en */
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED1_MASK            0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED1_SHIFT           6
/* rule9en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE9EN_MASK                 0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE9EN_SHIFT                7
	u8 flags12;
/* rule10en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE10EN_MASK                0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE10EN_SHIFT               0
/* rule11en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE11EN_MASK                0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE11EN_SHIFT               1
/* rule12en */
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED2_MASK            0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED2_SHIFT           2
/* rule13en */
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED3_MASK            0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED3_SHIFT           3
/* rule14en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE14EN_MASK                0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE14EN_SHIFT               4
/* rule15en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE15EN_MASK                0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE15EN_SHIFT               5
/* rule16en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE16EN_MASK                0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE16EN_SHIFT               6
/* rule17en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE17EN_MASK                0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE17EN_SHIFT               7
	u8 flags13;
/* rule18en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE18EN_MASK                0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE18EN_SHIFT               0
/* rule19en */
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE19EN_MASK                0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE19EN_SHIFT               1
/* rule20en */
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED4_MASK            0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED4_SHIFT           2
/* rule21en */
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED5_MASK            0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED5_SHIFT           3
/* rule22en */
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED6_MASK            0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED6_SHIFT           4
/* rule23en */
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED7_MASK            0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED7_SHIFT           5
/* rule24en */
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED8_MASK            0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED8_SHIFT           6
/* rule25en */
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED9_MASK            0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED9_SHIFT           7
	u8 flags14;
/* bit16 */
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_USE_EXT_HDR_MASK        0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_USE_EXT_HDR_SHIFT       0
/* bit17 */
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_SEND_RAW_L3L4_MASK      0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_SEND_RAW_L3L4_SHIFT     1
/* bit18 */
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_INBAND_PROP_HDR_MASK    0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_INBAND_PROP_HDR_SHIFT   2
/* bit19 */
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_SEND_EXT_TUNNEL_MASK    0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_SEND_EXT_TUNNEL_SHIFT   3
/* bit20 */
#define XSTORM_ETH_HW_CONN_AG_CTX_L2_EDPM_ENABLE_MASK          0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_L2_EDPM_ENABLE_SHIFT         4
/* bit21 */
#define XSTORM_ETH_HW_CONN_AG_CTX_ROCE_EDPM_ENABLE_MASK        0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_ROCE_EDPM_ENABLE_SHIFT       5
/* cf23 */
#define XSTORM_ETH_HW_CONN_AG_CTX_TPH_ENABLE_MASK              0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_TPH_ENABLE_SHIFT             6
	u8 edpm_event_id /* byte2 */;
	__le16 physical_q0 /* physical_q0 */;
	__le16 quota /* physical_q1 */;
	__le16 edpm_num_bds /* physical_q2 */;
	__le16 tx_bd_cons /* word3 */;
	__le16 tx_bd_prod /* word4 */;
	__le16 tx_class /* word5 */;
	__le16 conn_dpi /* conn_dpi */;
};


#endif /* __ECORE_HSI_ETH__ */
