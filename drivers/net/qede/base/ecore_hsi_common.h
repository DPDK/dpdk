/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef __ECORE_HSI_COMMON__
#define __ECORE_HSI_COMMON__
/********************************/
/* Add include to common target */
/********************************/
#include "common_hsi.h"

/*
 * opcodes for the event ring
 */
enum common_event_opcode {
	COMMON_EVENT_PF_START,
	COMMON_EVENT_PF_STOP,
	COMMON_EVENT_VF_START,
	COMMON_EVENT_VF_STOP,
	COMMON_EVENT_VF_PF_CHANNEL,
	COMMON_EVENT_VF_FLR,
	COMMON_EVENT_PF_UPDATE,
	COMMON_EVENT_MALICIOUS_VF,
	COMMON_EVENT_EMPTY,
	MAX_COMMON_EVENT_OPCODE
};

/*
 * Common Ramrod Command IDs
 */
enum common_ramrod_cmd_id {
	COMMON_RAMROD_UNUSED,
	COMMON_RAMROD_PF_START /* PF Function Start Ramrod */,
	COMMON_RAMROD_PF_STOP /* PF Function Stop Ramrod */,
	COMMON_RAMROD_VF_START /* VF Function Start */,
	COMMON_RAMROD_VF_STOP /* VF Function Stop Ramrod */,
	COMMON_RAMROD_PF_UPDATE /* PF update Ramrod */,
	COMMON_RAMROD_EMPTY /* Empty Ramrod */,
	MAX_COMMON_RAMROD_CMD_ID
};

/*
 * The core storm context for the Ystorm
 */
struct ystorm_core_conn_st_ctx {
	__le32 reserved[4];
};

/*
 * The core storm context for the Pstorm
 */
struct pstorm_core_conn_st_ctx {
	__le32 reserved[4];
};

/*
 * Core Slowpath Connection storm context of Xstorm
 */
struct xstorm_core_conn_st_ctx {
	__le32 spq_base_lo /* SPQ Ring Base Address low dword */;
	__le32 spq_base_hi /* SPQ Ring Base Address high dword */;
	struct regpair consolid_base_addr /* Consolidation Ring Base Address */
	  ;
	__le16 spq_cons /* SPQ Ring Consumer */;
	__le16 consolid_cons /* Consolidation Ring Consumer */;
	__le32 reserved0[55] /* Pad to 15 cycles */;
};

struct xstorm_core_conn_ag_ctx {
	u8 reserved0 /* cdu_validation */;
	u8 core_state /* state */;
	u8 flags0;
#define XSTORM_CORE_CONN_AG_CTX_EXIST_IN_QM0_MASK         0x1
#define XSTORM_CORE_CONN_AG_CTX_EXIST_IN_QM0_SHIFT        0
#define XSTORM_CORE_CONN_AG_CTX_RESERVED1_MASK            0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED1_SHIFT           1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED2_MASK            0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED2_SHIFT           2
#define XSTORM_CORE_CONN_AG_CTX_EXIST_IN_QM3_MASK         0x1
#define XSTORM_CORE_CONN_AG_CTX_EXIST_IN_QM3_SHIFT        3
#define XSTORM_CORE_CONN_AG_CTX_RESERVED3_MASK            0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED3_SHIFT           4
#define XSTORM_CORE_CONN_AG_CTX_RESERVED4_MASK            0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED4_SHIFT           5
#define XSTORM_CORE_CONN_AG_CTX_RESERVED5_MASK            0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED5_SHIFT           6
#define XSTORM_CORE_CONN_AG_CTX_RESERVED6_MASK            0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED6_SHIFT           7
	u8 flags1;
#define XSTORM_CORE_CONN_AG_CTX_RESERVED7_MASK            0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED7_SHIFT           0
#define XSTORM_CORE_CONN_AG_CTX_RESERVED8_MASK            0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED8_SHIFT           1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED9_MASK            0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED9_SHIFT           2
#define XSTORM_CORE_CONN_AG_CTX_BIT11_MASK                0x1
#define XSTORM_CORE_CONN_AG_CTX_BIT11_SHIFT               3
#define XSTORM_CORE_CONN_AG_CTX_BIT12_MASK                0x1
#define XSTORM_CORE_CONN_AG_CTX_BIT12_SHIFT               4
#define XSTORM_CORE_CONN_AG_CTX_BIT13_MASK                0x1
#define XSTORM_CORE_CONN_AG_CTX_BIT13_SHIFT               5
#define XSTORM_CORE_CONN_AG_CTX_TX_RULE_ACTIVE_MASK       0x1
#define XSTORM_CORE_CONN_AG_CTX_TX_RULE_ACTIVE_SHIFT      6
#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_ACTIVE_MASK         0x1
#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_ACTIVE_SHIFT        7
	u8 flags2;
#define XSTORM_CORE_CONN_AG_CTX_CF0_MASK                  0x3
#define XSTORM_CORE_CONN_AG_CTX_CF0_SHIFT                 0
#define XSTORM_CORE_CONN_AG_CTX_CF1_MASK                  0x3
#define XSTORM_CORE_CONN_AG_CTX_CF1_SHIFT                 2
#define XSTORM_CORE_CONN_AG_CTX_CF2_MASK                  0x3
#define XSTORM_CORE_CONN_AG_CTX_CF2_SHIFT                 4
#define XSTORM_CORE_CONN_AG_CTX_CF3_MASK                  0x3
#define XSTORM_CORE_CONN_AG_CTX_CF3_SHIFT                 6
	u8 flags3;
#define XSTORM_CORE_CONN_AG_CTX_CF4_MASK                  0x3
#define XSTORM_CORE_CONN_AG_CTX_CF4_SHIFT                 0
#define XSTORM_CORE_CONN_AG_CTX_CF5_MASK                  0x3
#define XSTORM_CORE_CONN_AG_CTX_CF5_SHIFT                 2
#define XSTORM_CORE_CONN_AG_CTX_CF6_MASK                  0x3
#define XSTORM_CORE_CONN_AG_CTX_CF6_SHIFT                 4
#define XSTORM_CORE_CONN_AG_CTX_CF7_MASK                  0x3
#define XSTORM_CORE_CONN_AG_CTX_CF7_SHIFT                 6
	u8 flags4;
#define XSTORM_CORE_CONN_AG_CTX_CF8_MASK                  0x3
#define XSTORM_CORE_CONN_AG_CTX_CF8_SHIFT                 0
#define XSTORM_CORE_CONN_AG_CTX_CF9_MASK                  0x3
#define XSTORM_CORE_CONN_AG_CTX_CF9_SHIFT                 2
#define XSTORM_CORE_CONN_AG_CTX_CF10_MASK                 0x3
#define XSTORM_CORE_CONN_AG_CTX_CF10_SHIFT                4
#define XSTORM_CORE_CONN_AG_CTX_CF11_MASK                 0x3
#define XSTORM_CORE_CONN_AG_CTX_CF11_SHIFT                6
	u8 flags5;
#define XSTORM_CORE_CONN_AG_CTX_CF12_MASK                 0x3
#define XSTORM_CORE_CONN_AG_CTX_CF12_SHIFT                0
#define XSTORM_CORE_CONN_AG_CTX_CF13_MASK                 0x3
#define XSTORM_CORE_CONN_AG_CTX_CF13_SHIFT                2
#define XSTORM_CORE_CONN_AG_CTX_CF14_MASK                 0x3
#define XSTORM_CORE_CONN_AG_CTX_CF14_SHIFT                4
#define XSTORM_CORE_CONN_AG_CTX_CF15_MASK                 0x3
#define XSTORM_CORE_CONN_AG_CTX_CF15_SHIFT                6
	u8 flags6;
#define XSTORM_CORE_CONN_AG_CTX_CONSOLID_PROD_CF_MASK     0x3
#define XSTORM_CORE_CONN_AG_CTX_CONSOLID_PROD_CF_SHIFT    0
#define XSTORM_CORE_CONN_AG_CTX_CF17_MASK                 0x3
#define XSTORM_CORE_CONN_AG_CTX_CF17_SHIFT                2
#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_MASK                0x3
#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_SHIFT               4
#define XSTORM_CORE_CONN_AG_CTX_TERMINATE_CF_MASK         0x3
#define XSTORM_CORE_CONN_AG_CTX_TERMINATE_CF_SHIFT        6
	u8 flags7;
#define XSTORM_CORE_CONN_AG_CTX_FLUSH_Q0_MASK             0x3
#define XSTORM_CORE_CONN_AG_CTX_FLUSH_Q0_SHIFT            0
#define XSTORM_CORE_CONN_AG_CTX_RESERVED10_MASK           0x3
#define XSTORM_CORE_CONN_AG_CTX_RESERVED10_SHIFT          2
#define XSTORM_CORE_CONN_AG_CTX_SLOW_PATH_MASK            0x3
#define XSTORM_CORE_CONN_AG_CTX_SLOW_PATH_SHIFT           4
#define XSTORM_CORE_CONN_AG_CTX_CF0EN_MASK                0x1
#define XSTORM_CORE_CONN_AG_CTX_CF0EN_SHIFT               6
#define XSTORM_CORE_CONN_AG_CTX_CF1EN_MASK                0x1
#define XSTORM_CORE_CONN_AG_CTX_CF1EN_SHIFT               7
	u8 flags8;
#define XSTORM_CORE_CONN_AG_CTX_CF2EN_MASK                0x1
#define XSTORM_CORE_CONN_AG_CTX_CF2EN_SHIFT               0
#define XSTORM_CORE_CONN_AG_CTX_CF3EN_MASK                0x1
#define XSTORM_CORE_CONN_AG_CTX_CF3EN_SHIFT               1
#define XSTORM_CORE_CONN_AG_CTX_CF4EN_MASK                0x1
#define XSTORM_CORE_CONN_AG_CTX_CF4EN_SHIFT               2
#define XSTORM_CORE_CONN_AG_CTX_CF5EN_MASK                0x1
#define XSTORM_CORE_CONN_AG_CTX_CF5EN_SHIFT               3
#define XSTORM_CORE_CONN_AG_CTX_CF6EN_MASK                0x1
#define XSTORM_CORE_CONN_AG_CTX_CF6EN_SHIFT               4
#define XSTORM_CORE_CONN_AG_CTX_CF7EN_MASK                0x1
#define XSTORM_CORE_CONN_AG_CTX_CF7EN_SHIFT               5
#define XSTORM_CORE_CONN_AG_CTX_CF8EN_MASK                0x1
#define XSTORM_CORE_CONN_AG_CTX_CF8EN_SHIFT               6
#define XSTORM_CORE_CONN_AG_CTX_CF9EN_MASK                0x1
#define XSTORM_CORE_CONN_AG_CTX_CF9EN_SHIFT               7
	u8 flags9;
#define XSTORM_CORE_CONN_AG_CTX_CF10EN_MASK               0x1
#define XSTORM_CORE_CONN_AG_CTX_CF10EN_SHIFT              0
#define XSTORM_CORE_CONN_AG_CTX_CF11EN_MASK               0x1
#define XSTORM_CORE_CONN_AG_CTX_CF11EN_SHIFT              1
#define XSTORM_CORE_CONN_AG_CTX_CF12EN_MASK               0x1
#define XSTORM_CORE_CONN_AG_CTX_CF12EN_SHIFT              2
#define XSTORM_CORE_CONN_AG_CTX_CF13EN_MASK               0x1
#define XSTORM_CORE_CONN_AG_CTX_CF13EN_SHIFT              3
#define XSTORM_CORE_CONN_AG_CTX_CF14EN_MASK               0x1
#define XSTORM_CORE_CONN_AG_CTX_CF14EN_SHIFT              4
#define XSTORM_CORE_CONN_AG_CTX_CF15EN_MASK               0x1
#define XSTORM_CORE_CONN_AG_CTX_CF15EN_SHIFT              5
#define XSTORM_CORE_CONN_AG_CTX_CONSOLID_PROD_CF_EN_MASK  0x1
#define XSTORM_CORE_CONN_AG_CTX_CONSOLID_PROD_CF_EN_SHIFT 6
#define XSTORM_CORE_CONN_AG_CTX_CF17EN_MASK               0x1
#define XSTORM_CORE_CONN_AG_CTX_CF17EN_SHIFT              7
	u8 flags10;
#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_EN_MASK             0x1
#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_EN_SHIFT            0
#define XSTORM_CORE_CONN_AG_CTX_TERMINATE_CF_EN_MASK      0x1
#define XSTORM_CORE_CONN_AG_CTX_TERMINATE_CF_EN_SHIFT     1
#define XSTORM_CORE_CONN_AG_CTX_FLUSH_Q0_EN_MASK          0x1
#define XSTORM_CORE_CONN_AG_CTX_FLUSH_Q0_EN_SHIFT         2
#define XSTORM_CORE_CONN_AG_CTX_RESERVED11_MASK           0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED11_SHIFT          3
#define XSTORM_CORE_CONN_AG_CTX_SLOW_PATH_EN_MASK         0x1
#define XSTORM_CORE_CONN_AG_CTX_SLOW_PATH_EN_SHIFT        4
#define XSTORM_CORE_CONN_AG_CTX_CF23EN_MASK               0x1
#define XSTORM_CORE_CONN_AG_CTX_CF23EN_SHIFT              5
#define XSTORM_CORE_CONN_AG_CTX_RESERVED12_MASK           0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED12_SHIFT          6
#define XSTORM_CORE_CONN_AG_CTX_RESERVED13_MASK           0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED13_SHIFT          7
	u8 flags11;
#define XSTORM_CORE_CONN_AG_CTX_RESERVED14_MASK           0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED14_SHIFT          0
#define XSTORM_CORE_CONN_AG_CTX_RESERVED15_MASK           0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED15_SHIFT          1
#define XSTORM_CORE_CONN_AG_CTX_TX_DEC_RULE_EN_MASK       0x1
#define XSTORM_CORE_CONN_AG_CTX_TX_DEC_RULE_EN_SHIFT      2
#define XSTORM_CORE_CONN_AG_CTX_RULE5EN_MASK              0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE5EN_SHIFT             3
#define XSTORM_CORE_CONN_AG_CTX_RULE6EN_MASK              0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE6EN_SHIFT             4
#define XSTORM_CORE_CONN_AG_CTX_RULE7EN_MASK              0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE7EN_SHIFT             5
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED1_MASK         0x1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED1_SHIFT        6
#define XSTORM_CORE_CONN_AG_CTX_RULE9EN_MASK              0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE9EN_SHIFT             7
	u8 flags12;
#define XSTORM_CORE_CONN_AG_CTX_RULE10EN_MASK             0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE10EN_SHIFT            0
#define XSTORM_CORE_CONN_AG_CTX_RULE11EN_MASK             0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE11EN_SHIFT            1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED2_MASK         0x1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED2_SHIFT        2
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED3_MASK         0x1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED3_SHIFT        3
#define XSTORM_CORE_CONN_AG_CTX_RULE14EN_MASK             0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE14EN_SHIFT            4
#define XSTORM_CORE_CONN_AG_CTX_RULE15EN_MASK             0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE15EN_SHIFT            5
#define XSTORM_CORE_CONN_AG_CTX_RULE16EN_MASK             0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE16EN_SHIFT            6
#define XSTORM_CORE_CONN_AG_CTX_RULE17EN_MASK             0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE17EN_SHIFT            7
	u8 flags13;
#define XSTORM_CORE_CONN_AG_CTX_RULE18EN_MASK             0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE18EN_SHIFT            0
#define XSTORM_CORE_CONN_AG_CTX_RULE19EN_MASK             0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE19EN_SHIFT            1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED4_MASK         0x1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED4_SHIFT        2
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED5_MASK         0x1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED5_SHIFT        3
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED6_MASK         0x1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED6_SHIFT        4
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED7_MASK         0x1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED7_SHIFT        5
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED8_MASK         0x1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED8_SHIFT        6
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED9_MASK         0x1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED9_SHIFT        7
	u8 flags14;
#define XSTORM_CORE_CONN_AG_CTX_BIT16_MASK                0x1
#define XSTORM_CORE_CONN_AG_CTX_BIT16_SHIFT               0
#define XSTORM_CORE_CONN_AG_CTX_BIT17_MASK                0x1
#define XSTORM_CORE_CONN_AG_CTX_BIT17_SHIFT               1
#define XSTORM_CORE_CONN_AG_CTX_BIT18_MASK                0x1
#define XSTORM_CORE_CONN_AG_CTX_BIT18_SHIFT               2
#define XSTORM_CORE_CONN_AG_CTX_BIT19_MASK                0x1
#define XSTORM_CORE_CONN_AG_CTX_BIT19_SHIFT               3
#define XSTORM_CORE_CONN_AG_CTX_BIT20_MASK                0x1
#define XSTORM_CORE_CONN_AG_CTX_BIT20_SHIFT               4
#define XSTORM_CORE_CONN_AG_CTX_BIT21_MASK                0x1
#define XSTORM_CORE_CONN_AG_CTX_BIT21_SHIFT               5
#define XSTORM_CORE_CONN_AG_CTX_CF23_MASK                 0x3
#define XSTORM_CORE_CONN_AG_CTX_CF23_SHIFT                6
	u8 byte2 /* byte2 */;
	__le16 physical_q0 /* physical_q0 */;
	__le16 consolid_prod /* physical_q1 */;
	__le16 reserved16 /* physical_q2 */;
	__le16 tx_bd_cons /* word3 */;
	__le16 tx_bd_or_spq_prod /* word4 */;
	__le16 word5 /* word5 */;
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

struct tstorm_core_conn_ag_ctx {
	u8 byte0 /* cdu_validation */;
	u8 byte1 /* state */;
	u8 flags0;
#define TSTORM_CORE_CONN_AG_CTX_BIT0_MASK     0x1
#define TSTORM_CORE_CONN_AG_CTX_BIT0_SHIFT    0
#define TSTORM_CORE_CONN_AG_CTX_BIT1_MASK     0x1
#define TSTORM_CORE_CONN_AG_CTX_BIT1_SHIFT    1
#define TSTORM_CORE_CONN_AG_CTX_BIT2_MASK     0x1
#define TSTORM_CORE_CONN_AG_CTX_BIT2_SHIFT    2
#define TSTORM_CORE_CONN_AG_CTX_BIT3_MASK     0x1
#define TSTORM_CORE_CONN_AG_CTX_BIT3_SHIFT    3
#define TSTORM_CORE_CONN_AG_CTX_BIT4_MASK     0x1
#define TSTORM_CORE_CONN_AG_CTX_BIT4_SHIFT    4
#define TSTORM_CORE_CONN_AG_CTX_BIT5_MASK     0x1
#define TSTORM_CORE_CONN_AG_CTX_BIT5_SHIFT    5
#define TSTORM_CORE_CONN_AG_CTX_CF0_MASK      0x3
#define TSTORM_CORE_CONN_AG_CTX_CF0_SHIFT     6
	u8 flags1;
#define TSTORM_CORE_CONN_AG_CTX_CF1_MASK      0x3
#define TSTORM_CORE_CONN_AG_CTX_CF1_SHIFT     0
#define TSTORM_CORE_CONN_AG_CTX_CF2_MASK      0x3
#define TSTORM_CORE_CONN_AG_CTX_CF2_SHIFT     2
#define TSTORM_CORE_CONN_AG_CTX_CF3_MASK      0x3
#define TSTORM_CORE_CONN_AG_CTX_CF3_SHIFT     4
#define TSTORM_CORE_CONN_AG_CTX_CF4_MASK      0x3
#define TSTORM_CORE_CONN_AG_CTX_CF4_SHIFT     6
	u8 flags2;
#define TSTORM_CORE_CONN_AG_CTX_CF5_MASK      0x3
#define TSTORM_CORE_CONN_AG_CTX_CF5_SHIFT     0
#define TSTORM_CORE_CONN_AG_CTX_CF6_MASK      0x3
#define TSTORM_CORE_CONN_AG_CTX_CF6_SHIFT     2
#define TSTORM_CORE_CONN_AG_CTX_CF7_MASK      0x3
#define TSTORM_CORE_CONN_AG_CTX_CF7_SHIFT     4
#define TSTORM_CORE_CONN_AG_CTX_CF8_MASK      0x3
#define TSTORM_CORE_CONN_AG_CTX_CF8_SHIFT     6
	u8 flags3;
#define TSTORM_CORE_CONN_AG_CTX_CF9_MASK      0x3
#define TSTORM_CORE_CONN_AG_CTX_CF9_SHIFT     0
#define TSTORM_CORE_CONN_AG_CTX_CF10_MASK     0x3
#define TSTORM_CORE_CONN_AG_CTX_CF10_SHIFT    2
#define TSTORM_CORE_CONN_AG_CTX_CF0EN_MASK    0x1
#define TSTORM_CORE_CONN_AG_CTX_CF0EN_SHIFT   4
#define TSTORM_CORE_CONN_AG_CTX_CF1EN_MASK    0x1
#define TSTORM_CORE_CONN_AG_CTX_CF1EN_SHIFT   5
#define TSTORM_CORE_CONN_AG_CTX_CF2EN_MASK    0x1
#define TSTORM_CORE_CONN_AG_CTX_CF2EN_SHIFT   6
#define TSTORM_CORE_CONN_AG_CTX_CF3EN_MASK    0x1
#define TSTORM_CORE_CONN_AG_CTX_CF3EN_SHIFT   7
	u8 flags4;
#define TSTORM_CORE_CONN_AG_CTX_CF4EN_MASK    0x1
#define TSTORM_CORE_CONN_AG_CTX_CF4EN_SHIFT   0
#define TSTORM_CORE_CONN_AG_CTX_CF5EN_MASK    0x1
#define TSTORM_CORE_CONN_AG_CTX_CF5EN_SHIFT   1
#define TSTORM_CORE_CONN_AG_CTX_CF6EN_MASK    0x1
#define TSTORM_CORE_CONN_AG_CTX_CF6EN_SHIFT   2
#define TSTORM_CORE_CONN_AG_CTX_CF7EN_MASK    0x1
#define TSTORM_CORE_CONN_AG_CTX_CF7EN_SHIFT   3
#define TSTORM_CORE_CONN_AG_CTX_CF8EN_MASK    0x1
#define TSTORM_CORE_CONN_AG_CTX_CF8EN_SHIFT   4
#define TSTORM_CORE_CONN_AG_CTX_CF9EN_MASK    0x1
#define TSTORM_CORE_CONN_AG_CTX_CF9EN_SHIFT   5
#define TSTORM_CORE_CONN_AG_CTX_CF10EN_MASK   0x1
#define TSTORM_CORE_CONN_AG_CTX_CF10EN_SHIFT  6
#define TSTORM_CORE_CONN_AG_CTX_RULE0EN_MASK  0x1
#define TSTORM_CORE_CONN_AG_CTX_RULE0EN_SHIFT 7
	u8 flags5;
#define TSTORM_CORE_CONN_AG_CTX_RULE1EN_MASK  0x1
#define TSTORM_CORE_CONN_AG_CTX_RULE1EN_SHIFT 0
#define TSTORM_CORE_CONN_AG_CTX_RULE2EN_MASK  0x1
#define TSTORM_CORE_CONN_AG_CTX_RULE2EN_SHIFT 1
#define TSTORM_CORE_CONN_AG_CTX_RULE3EN_MASK  0x1
#define TSTORM_CORE_CONN_AG_CTX_RULE3EN_SHIFT 2
#define TSTORM_CORE_CONN_AG_CTX_RULE4EN_MASK  0x1
#define TSTORM_CORE_CONN_AG_CTX_RULE4EN_SHIFT 3
#define TSTORM_CORE_CONN_AG_CTX_RULE5EN_MASK  0x1
#define TSTORM_CORE_CONN_AG_CTX_RULE5EN_SHIFT 4
#define TSTORM_CORE_CONN_AG_CTX_RULE6EN_MASK  0x1
#define TSTORM_CORE_CONN_AG_CTX_RULE6EN_SHIFT 5
#define TSTORM_CORE_CONN_AG_CTX_RULE7EN_MASK  0x1
#define TSTORM_CORE_CONN_AG_CTX_RULE7EN_SHIFT 6
#define TSTORM_CORE_CONN_AG_CTX_RULE8EN_MASK  0x1
#define TSTORM_CORE_CONN_AG_CTX_RULE8EN_SHIFT 7
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
	__le16 word0 /* word0 */;
	u8 byte4 /* byte4 */;
	u8 byte5 /* byte5 */;
	__le16 word1 /* word1 */;
	__le16 word2 /* conn_dpi */;
	__le16 word3 /* word3 */;
	__le32 reg9 /* reg9 */;
	__le32 reg10 /* reg10 */;
};

struct ustorm_core_conn_ag_ctx {
	u8 reserved /* cdu_validation */;
	u8 byte1 /* state */;
	u8 flags0;
#define USTORM_CORE_CONN_AG_CTX_BIT0_MASK     0x1
#define USTORM_CORE_CONN_AG_CTX_BIT0_SHIFT    0
#define USTORM_CORE_CONN_AG_CTX_BIT1_MASK     0x1
#define USTORM_CORE_CONN_AG_CTX_BIT1_SHIFT    1
#define USTORM_CORE_CONN_AG_CTX_CF0_MASK      0x3
#define USTORM_CORE_CONN_AG_CTX_CF0_SHIFT     2
#define USTORM_CORE_CONN_AG_CTX_CF1_MASK      0x3
#define USTORM_CORE_CONN_AG_CTX_CF1_SHIFT     4
#define USTORM_CORE_CONN_AG_CTX_CF2_MASK      0x3
#define USTORM_CORE_CONN_AG_CTX_CF2_SHIFT     6
	u8 flags1;
#define USTORM_CORE_CONN_AG_CTX_CF3_MASK      0x3
#define USTORM_CORE_CONN_AG_CTX_CF3_SHIFT     0
#define USTORM_CORE_CONN_AG_CTX_CF4_MASK      0x3
#define USTORM_CORE_CONN_AG_CTX_CF4_SHIFT     2
#define USTORM_CORE_CONN_AG_CTX_CF5_MASK      0x3
#define USTORM_CORE_CONN_AG_CTX_CF5_SHIFT     4
#define USTORM_CORE_CONN_AG_CTX_CF6_MASK      0x3
#define USTORM_CORE_CONN_AG_CTX_CF6_SHIFT     6
	u8 flags2;
#define USTORM_CORE_CONN_AG_CTX_CF0EN_MASK    0x1
#define USTORM_CORE_CONN_AG_CTX_CF0EN_SHIFT   0
#define USTORM_CORE_CONN_AG_CTX_CF1EN_MASK    0x1
#define USTORM_CORE_CONN_AG_CTX_CF1EN_SHIFT   1
#define USTORM_CORE_CONN_AG_CTX_CF2EN_MASK    0x1
#define USTORM_CORE_CONN_AG_CTX_CF2EN_SHIFT   2
#define USTORM_CORE_CONN_AG_CTX_CF3EN_MASK    0x1
#define USTORM_CORE_CONN_AG_CTX_CF3EN_SHIFT   3
#define USTORM_CORE_CONN_AG_CTX_CF4EN_MASK    0x1
#define USTORM_CORE_CONN_AG_CTX_CF4EN_SHIFT   4
#define USTORM_CORE_CONN_AG_CTX_CF5EN_MASK    0x1
#define USTORM_CORE_CONN_AG_CTX_CF5EN_SHIFT   5
#define USTORM_CORE_CONN_AG_CTX_CF6EN_MASK    0x1
#define USTORM_CORE_CONN_AG_CTX_CF6EN_SHIFT   6
#define USTORM_CORE_CONN_AG_CTX_RULE0EN_MASK  0x1
#define USTORM_CORE_CONN_AG_CTX_RULE0EN_SHIFT 7
	u8 flags3;
#define USTORM_CORE_CONN_AG_CTX_RULE1EN_MASK  0x1
#define USTORM_CORE_CONN_AG_CTX_RULE1EN_SHIFT 0
#define USTORM_CORE_CONN_AG_CTX_RULE2EN_MASK  0x1
#define USTORM_CORE_CONN_AG_CTX_RULE2EN_SHIFT 1
#define USTORM_CORE_CONN_AG_CTX_RULE3EN_MASK  0x1
#define USTORM_CORE_CONN_AG_CTX_RULE3EN_SHIFT 2
#define USTORM_CORE_CONN_AG_CTX_RULE4EN_MASK  0x1
#define USTORM_CORE_CONN_AG_CTX_RULE4EN_SHIFT 3
#define USTORM_CORE_CONN_AG_CTX_RULE5EN_MASK  0x1
#define USTORM_CORE_CONN_AG_CTX_RULE5EN_SHIFT 4
#define USTORM_CORE_CONN_AG_CTX_RULE6EN_MASK  0x1
#define USTORM_CORE_CONN_AG_CTX_RULE6EN_SHIFT 5
#define USTORM_CORE_CONN_AG_CTX_RULE7EN_MASK  0x1
#define USTORM_CORE_CONN_AG_CTX_RULE7EN_SHIFT 6
#define USTORM_CORE_CONN_AG_CTX_RULE8EN_MASK  0x1
#define USTORM_CORE_CONN_AG_CTX_RULE8EN_SHIFT 7
	u8 byte2 /* byte2 */;
	u8 byte3 /* byte3 */;
	__le16 word0 /* conn_dpi */;
	__le16 word1 /* word1 */;
	__le32 rx_producers /* reg0 */;
	__le32 reg1 /* reg1 */;
	__le32 reg2 /* reg2 */;
	__le32 reg3 /* reg3 */;
	__le16 word2 /* word2 */;
	__le16 word3 /* word3 */;
};

/*
 * The core storm context for the Mstorm
 */
struct mstorm_core_conn_st_ctx {
	__le32 reserved[24];
};

/*
 * The core storm context for the Ustorm
 */
struct ustorm_core_conn_st_ctx {
	__le32 reserved[4];
};

/*
 * core connection context
 */
struct core_conn_context {
	struct ystorm_core_conn_st_ctx ystorm_st_context
	    /* ystorm storm context */;
	struct regpair ystorm_st_padding[2] /* padding */;
	struct pstorm_core_conn_st_ctx pstorm_st_context
	    /* pstorm storm context */;
	struct regpair pstorm_st_padding[2] /* padding */;
	struct xstorm_core_conn_st_ctx xstorm_st_context
	    /* xstorm storm context */;
	struct xstorm_core_conn_ag_ctx xstorm_ag_context
	    /* xstorm aggregative context */;
	struct tstorm_core_conn_ag_ctx tstorm_ag_context
	    /* tstorm aggregative context */;
	struct ustorm_core_conn_ag_ctx ustorm_ag_context
	    /* ustorm aggregative context */;
	struct mstorm_core_conn_st_ctx mstorm_st_context
	    /* mstorm storm context */;
	struct ustorm_core_conn_st_ctx ustorm_st_context
	    /* ustorm storm context */;
	struct regpair ustorm_st_padding[2] /* padding */;
};

/*
 * How ll2 should deal with packet upon errors
 */
enum core_error_handle {
	LL2_DROP_PACKET /* If error occurs drop packet */,
	LL2_DO_NOTHING /* If error occurs do nothing */,
	LL2_ASSERT /* If error occurs assert */,
	MAX_CORE_ERROR_HANDLE
};

/*
 * opcodes for the event ring
 */
enum core_event_opcode {
	CORE_EVENT_TX_QUEUE_START,
	CORE_EVENT_TX_QUEUE_STOP,
	CORE_EVENT_RX_QUEUE_START,
	CORE_EVENT_RX_QUEUE_STOP,
	MAX_CORE_EVENT_OPCODE
};

/*
 * The L4 pseudo checksum mode for Core
 */
enum core_l4_pseudo_checksum_mode {
	CORE_L4_PSEUDO_CSUM_CORRECT_LENGTH
	    ,
	CORE_L4_PSEUDO_CSUM_ZERO_LENGTH
	    /* Pseudo Checksum on packet is calculated with zero length. */,
	MAX_CORE_L4_PSEUDO_CHECKSUM_MODE
};

/*
 * Light-L2 RX Producers in Tstorm RAM
 */
struct core_ll2_port_stats {
	struct regpair gsi_invalid_hdr;
	struct regpair gsi_invalid_pkt_length;
	struct regpair gsi_unsupported_pkt_typ;
	struct regpair gsi_crcchksm_error;
};

/*
 * Ethernet TX Per Queue Stats
 */
struct core_ll2_pstorm_per_queue_stat {
	struct regpair sent_ucast_bytes
	    /* number of total bytes sent without errors */;
	struct regpair sent_mcast_bytes
	    /* number of total bytes sent without errors */;
	struct regpair sent_bcast_bytes
	    /* number of total bytes sent without errors */;
	struct regpair sent_ucast_pkts
	    /* number of total packets sent without errors */;
	struct regpair sent_mcast_pkts
	    /* number of total packets sent without errors */;
	struct regpair sent_bcast_pkts
	    /* number of total packets sent without errors */;
};

/*
 * Light-L2 RX Producers in Tstorm RAM
 */
struct core_ll2_rx_prod {
	__le16 bd_prod /* BD Producer */;
	__le16 cqe_prod /* CQE Producer */;
	__le32 reserved;
};

struct core_ll2_tstorm_per_queue_stat {
	struct regpair packet_too_big_discard
	    /* Number of packets discarded because they are bigger than MTU */;
	struct regpair no_buff_discard
	    /* Number of packets discarded due to lack of host buffers */;
};

struct core_ll2_ustorm_per_queue_stat {
	struct regpair rcv_ucast_bytes;
	struct regpair rcv_mcast_bytes;
	struct regpair rcv_bcast_bytes;
	struct regpair rcv_ucast_pkts;
	struct regpair rcv_mcast_pkts;
	struct regpair rcv_bcast_pkts;
};

/*
 * Core Ramrod Command IDs (light L2)
 */
enum core_ramrod_cmd_id {
	CORE_RAMROD_UNUSED,
	CORE_RAMROD_RX_QUEUE_START /* RX Queue Start Ramrod */,
	CORE_RAMROD_TX_QUEUE_START /* TX Queue Start Ramrod */,
	CORE_RAMROD_RX_QUEUE_STOP /* RX Queue Stop Ramrod */,
	CORE_RAMROD_TX_QUEUE_STOP /* TX Queue Stop Ramrod */,
	MAX_CORE_RAMROD_CMD_ID
};

/*
 * Specifies how ll2 should deal with packets errors: packet_too_big and no_buff
 */
struct core_rx_action_on_error {
	u8 error_type;
#define CORE_RX_ACTION_ON_ERROR_PACKET_TOO_BIG_MASK  0x3
#define CORE_RX_ACTION_ON_ERROR_PACKET_TOO_BIG_SHIFT 0
#define CORE_RX_ACTION_ON_ERROR_NO_BUFF_MASK         0x3
#define CORE_RX_ACTION_ON_ERROR_NO_BUFF_SHIFT        2
#define CORE_RX_ACTION_ON_ERROR_RESERVED_MASK        0xF
#define CORE_RX_ACTION_ON_ERROR_RESERVED_SHIFT       4
};

/*
 * Core RX BD for Light L2
 */
struct core_rx_bd {
	struct regpair addr;
	__le16 reserved[4];
};

/*
 * Core RX CM offload BD for Light L2
 */
struct core_rx_bd_with_buff_len {
	struct regpair addr;
	__le16 buff_length;
	__le16 reserved[3];
};

/*
 * Core RX CM offload BD for Light L2
 */
union core_rx_bd_union {
	struct core_rx_bd rx_bd /* Core Rx Bd static buffer size */;
	struct core_rx_bd_with_buff_len rx_bd_with_len
	    /* Core Rx Bd with dynamic buffer length */;
};

/*
 * Opaque Data for Light L2 RX CQE .
 */
struct core_rx_cqe_opaque_data {
	__le32 data[2] /* Opaque CQE Data */;
};

/*
 * Core RX CQE Type for Light L2
 */
enum core_rx_cqe_type {
	CORE_RX_CQE_ILLIGAL_TYPE /* Bad RX Cqe type */,
	CORE_RX_CQE_TYPE_REGULAR /* Regular Core RX CQE */,
	CORE_RX_CQE_TYPE_GSI_OFFLOAD /* Fp Gsi offload RX CQE */,
	CORE_RX_CQE_TYPE_SLOW_PATH /* Slow path Core RX CQE */,
	MAX_CORE_RX_CQE_TYPE
};

/*
 * Core RX CQE for Light L2 .
 */
struct core_rx_fast_path_cqe {
	u8 type /* CQE type */;
	u8 placement_offset
	    /* Offset (in bytes) of the packet from start of the buffer */;
	struct parsing_and_err_flags parse_flags
	    /* Parsing and error flags from the parser */;
	__le16 packet_length /* Total packet length (from the parser) */;
	__le16 vlan /* 802.1q VLAN tag */;
	struct core_rx_cqe_opaque_data opaque_data /* Opaque Data */;
	__le32 reserved[4];
};

/*
 * Core Rx CM offload CQE .
 */
struct core_rx_gsi_offload_cqe {
	u8 type /* CQE type */;
	u8 data_length_error /* set if gsi data is bigger than buff */;
	struct parsing_and_err_flags parse_flags
	    /* Parsing and error flags from the parser */;
	__le16 data_length /* Total packet length (from the parser) */;
	__le16 vlan /* 802.1q VLAN tag */;
	__le32 src_mac_addrhi /* hi 4 bytes source mac address */;
	__le16 src_mac_addrlo /* lo 2 bytes of source mac address */;
	u8 reserved1[2];
	__le32 gid_dst[4] /* Gid destination address */;
};

/*
 * Core RX CQE for Light L2 .
 */
struct core_rx_slow_path_cqe {
	u8 type /* CQE type */;
	u8 ramrod_cmd_id;
	__le16 echo;
	__le32 reserved1[7];
};

/*
 * Core RX CM offload BD for Light L2
 */
union core_rx_cqe_union {
	struct core_rx_fast_path_cqe rx_cqe_fp /* Fast path CQE */;
	struct core_rx_gsi_offload_cqe rx_cqe_gsi /* GSI offload CQE */;
	struct core_rx_slow_path_cqe rx_cqe_sp /* Slow path CQE */;
};

/*
 * Ramrod data for rx queue start ramrod
 */
struct core_rx_start_ramrod_data {
	struct regpair bd_base /* bd address of the first bd page */;
	struct regpair cqe_pbl_addr /* Base address on host of CQE PBL */;
	__le16 mtu /* Maximum transmission unit */;
	__le16 sb_id /* Status block ID */;
	u8 sb_index /* index of the protocol index */;
	u8 complete_cqe_flg /* post completion to the CQE ring if set */;
	u8 complete_event_flg /* post completion to the event ring if set */;
	u8 drop_ttl0_flg /* drop packet with ttl0 if set */;
	__le16 num_of_pbl_pages /* Num of pages in CQE PBL */;
	u8 inner_vlan_removal_en
	    /* if set, 802.1q tags will be removed and copied to CQE */;
	u8 queue_id /* Light L2 RX Queue ID */;
	u8 main_func_queue /* Is this the main queue for the PF */;
	u8 mf_si_bcast_accept_all;
	u8 mf_si_mcast_accept_all;
	struct core_rx_action_on_error action_on_error;
	u8 gsi_offload_flag
	    /* set when in GSI offload mode on ROCE connection */;
	u8 reserved[7];
};

/*
 * Ramrod data for rx queue stop ramrod
 */
struct core_rx_stop_ramrod_data {
	u8 complete_cqe_flg /* post completion to the CQE ring if set */;
	u8 complete_event_flg /* post completion to the event ring if set */;
	u8 queue_id /* Light L2 RX Queue ID */;
	u8 reserved1;
	__le16 reserved2[2];
};

/*
 * Flags for Core TX BD
 */
struct core_tx_bd_flags {
	u8 as_bitfield;
#define CORE_TX_BD_FLAGS_FORCE_VLAN_MODE_MASK      0x1
#define CORE_TX_BD_FLAGS_FORCE_VLAN_MODE_SHIFT     0
#define CORE_TX_BD_FLAGS_VLAN_INSERTION_MASK       0x1
#define CORE_TX_BD_FLAGS_VLAN_INSERTION_SHIFT      1
#define CORE_TX_BD_FLAGS_START_BD_MASK             0x1
#define CORE_TX_BD_FLAGS_START_BD_SHIFT            2
#define CORE_TX_BD_FLAGS_IP_CSUM_MASK              0x1
#define CORE_TX_BD_FLAGS_IP_CSUM_SHIFT             3
#define CORE_TX_BD_FLAGS_L4_CSUM_MASK              0x1
#define CORE_TX_BD_FLAGS_L4_CSUM_SHIFT             4
#define CORE_TX_BD_FLAGS_IPV6_EXT_MASK             0x1
#define CORE_TX_BD_FLAGS_IPV6_EXT_SHIFT            5
#define CORE_TX_BD_FLAGS_L4_PROTOCOL_MASK          0x1
#define CORE_TX_BD_FLAGS_L4_PROTOCOL_SHIFT         6
#define CORE_TX_BD_FLAGS_L4_PSEUDO_CSUM_MODE_MASK  0x1
#define CORE_TX_BD_FLAGS_L4_PSEUDO_CSUM_MODE_SHIFT 7
};

/*
 * Core TX BD for Light L2
 */
struct core_tx_bd {
	struct regpair addr /* Buffer Address */;
	__le16 nbytes /* Number of Bytes in Buffer */;
	__le16 vlan /* VLAN to insert to packet (if insertion flag set) */;
	u8 nbds /* Number of BDs that make up one packet */;
	struct core_tx_bd_flags bd_flags /* BD Flags */;
	__le16 l4_hdr_offset_w;
};

/*
 * Light L2 TX Destination
 */
enum core_tx_dest {
	CORE_TX_DEST_NW /* Light L2 TX Destination to the Network */,
	CORE_TX_DEST_LB /* Light L2 TX Destination to the Loopback */,
	MAX_CORE_TX_DEST
};

/*
 * Ramrod data for rx queue start ramrod
 */
struct core_tx_start_ramrod_data {
	struct regpair pbl_base_addr /* Address of the pbl page */;
	__le16 mtu /* Maximum transmission unit */;
	__le16 sb_id /* Status block ID */;
	u8 sb_index /* Status block protocol index */;
	u8 tx_dest /* TX Destination (either Network or LB) */;
	u8 stats_en /* Statistics Enable */;
	u8 stats_id /* Statistics Counter ID */;
	__le16 pbl_size /* Number of BD pages pointed by PBL */;
	__le16 qm_pq_id /* QM PQ ID */;
	u8 conn_type /* connection type that loaded ll2 */;
	u8 gsi_offload_flag
	    /* set when in GSI offload mode on ROCE connection */;
	u8 resrved[2];
};

/*
 * Ramrod data for tx queue stop ramrod
 */
struct core_tx_stop_ramrod_data {
	__le32 reserved0[2];
};

struct eth_mstorm_per_queue_stat {
	struct regpair ttl0_discard;
	struct regpair packet_too_big_discard;
	struct regpair no_buff_discard;
	struct regpair not_active_discard;
	struct regpair tpa_coalesced_pkts;
	struct regpair tpa_coalesced_events;
	struct regpair tpa_aborts_num;
	struct regpair tpa_coalesced_bytes;
};

/*
 * Ethernet TX Per Queue Stats
 */
struct eth_pstorm_per_queue_stat {
	struct regpair sent_ucast_bytes
	    /* number of total bytes sent without errors */;
	struct regpair sent_mcast_bytes
	    /* number of total bytes sent without errors */;
	struct regpair sent_bcast_bytes
	    /* number of total bytes sent without errors */;
	struct regpair sent_ucast_pkts
	    /* number of total packets sent without errors */;
	struct regpair sent_mcast_pkts
	    /* number of total packets sent without errors */;
	struct regpair sent_bcast_pkts
	    /* number of total packets sent without errors */;
	struct regpair error_drop_pkts
	    /* number of total packets dropped due to errors */;
};

/*
 * ETH Rx producers data
 */
struct eth_rx_rate_limit {
	__le16 mult;
	__le16 cnst
	    /* Constant term to add (or subtract from number of cycles) */;
	u8 add_sub_cnst /* Add (1) or subtract (0) constant term */;
	u8 reserved0;
	__le16 reserved1;
};

struct eth_ustorm_per_queue_stat {
	struct regpair rcv_ucast_bytes;
	struct regpair rcv_mcast_bytes;
	struct regpair rcv_bcast_bytes;
	struct regpair rcv_ucast_pkts;
	struct regpair rcv_mcast_pkts;
	struct regpair rcv_bcast_pkts;
};

/*
 * Event Ring Next Page Address
 */
struct event_ring_next_addr {
	struct regpair addr /* Next Page Address */;
	__le32 reserved[2] /* Reserved */;
};

/*
 * Event Ring Element
 */
union event_ring_element {
	struct event_ring_entry entry /* Event Ring Entry */;
	struct event_ring_next_addr next_addr /* Event Ring Next Page Address */
	  ;
};

/*
 * Ports mode
 */
enum fw_flow_ctrl_mode {
	flow_ctrl_pause,
	flow_ctrl_pfc,
	MAX_FW_FLOW_CTRL_MODE
};

/*
 * Integration Phase
 */
enum integ_phase {
	INTEG_PHASE_BB_A0_LATEST = 3 /* BB A0 latest integration phase */,
	INTEG_PHASE_BB_B0_NO_MCP = 10 /* BB B0 without MCP */,
	INTEG_PHASE_BB_B0_WITH_MCP = 11 /* BB B0 with MCP */,
	MAX_INTEG_PHASE
};

/*
 * Malicious VF error ID
 */
enum malicious_vf_error_id {
	MALICIOUS_VF_NO_ERROR /* Zero placeholder value */,
	VF_PF_CHANNEL_NOT_READY
	    /* Writing to VF/PF channel when it is not ready */,
	VF_ZONE_MSG_NOT_VALID /* VF channel message is not valid */,
	VF_ZONE_FUNC_NOT_ENABLED /* Parent PF of VF channel is not active */,
	ETH_PACKET_TOO_SMALL
	    /* TX packet is shorter then reported on BDs or from minimal size */
	    ,
	ETH_ILLEGAL_VLAN_MODE
	    /* Tx packet with marked as insert VLAN when its illegal */,
	ETH_MTU_VIOLATION /* TX packet is greater then MTU */,
	ETH_ILLEGAL_INBAND_TAGS /* TX packet has illegal inband tags marked */,
	ETH_VLAN_INSERT_AND_INBAND_VLAN /* Vlan cant be added to inband tag */,
	ETH_ILLEGAL_NBDS /* indicated number of BDs for the packet is illegal */
	    ,
	ETH_FIRST_BD_WO_SOP /* 1st BD must have start_bd flag set */,
	ETH_INSUFFICIENT_BDS
	    /* There are not enough BDs for transmission of even one packet */,
	ETH_ILLEGAL_LSO_HDR_NBDS /* Header NBDs value is illegal */,
	ETH_ILLEGAL_LSO_MSS /* LSO MSS value is more than allowed */,
	ETH_ZERO_SIZE_BD
	    /* empty BD (which not contains control flags) is illegal  */,
	ETH_ILLEGAL_LSO_HDR_LEN /* LSO header size is above the limit  */,
	ETH_INSUFFICIENT_PAYLOAD
	    ,
	ETH_EDPM_OUT_OF_SYNC /* Valid BDs on local ring after EDPM L2 sync */,
	ETH_TUNN_IPV6_EXT_NBD_ERR
	    /* Tunneled packet with IPv6+Ext without a proper number of BDs */,
	MAX_MALICIOUS_VF_ERROR_ID
};

/*
 * Mstorm non-triggering VF zone
 */
struct mstorm_non_trigger_vf_zone {
	struct eth_mstorm_per_queue_stat eth_queue_stat
	    /* VF statistic bucket */;
};

/*
 * Mstorm VF zone
 */
struct mstorm_vf_zone {
	struct mstorm_non_trigger_vf_zone non_trigger
	    /* non-interrupt-triggering zone */;
};

/*
 * personality per PF
 */
enum personality_type {
	BAD_PERSONALITY_TYP,
	PERSONALITY_ISCSI /* iSCSI and LL2 */,
	PERSONALITY_FCOE /* Fcoe and LL2 */,
	PERSONALITY_RDMA_AND_ETH /* Roce or Iwarp, Eth and LL2 */,
	PERSONALITY_RDMA /* Roce and LL2 */,
	PERSONALITY_CORE /* CORE(LL2) */,
	PERSONALITY_ETH /* Ethernet */,
	PERSONALITY_TOE /* Toe and LL2 */,
	MAX_PERSONALITY_TYPE
};

/*
 * tunnel configuration
 */
struct pf_start_tunnel_config {
	u8 set_vxlan_udp_port_flg /* Set VXLAN tunnel UDP destination port. */;
	u8 set_geneve_udp_port_flg /* Set GENEVE tunnel UDP destination port. */
	  ;
	u8 tx_enable_vxlan /* If set, enable VXLAN tunnel in TX path. */;
	u8 tx_enable_l2geneve /* If set, enable l2 GENEVE tunnel in TX path. */
	  ;
	u8 tx_enable_ipgeneve /* If set, enable IP GENEVE tunnel in TX path. */
	  ;
	u8 tx_enable_l2gre /* If set, enable l2 GRE tunnel in TX path. */;
	u8 tx_enable_ipgre /* If set, enable IP GRE tunnel in TX path. */;
	u8 tunnel_clss_vxlan /* Classification scheme for VXLAN tunnel. */;
	u8 tunnel_clss_l2geneve
	    /* Classification scheme for l2 GENEVE tunnel. */;
	u8 tunnel_clss_ipgeneve
	    /* Classification scheme for ip GENEVE tunnel. */;
	u8 tunnel_clss_l2gre /* Classification scheme for l2 GRE tunnel. */;
	u8 tunnel_clss_ipgre /* Classification scheme for ip GRE tunnel. */;
	__le16 vxlan_udp_port /* VXLAN tunnel UDP destination port. */;
	__le16 geneve_udp_port /* GENEVE tunnel UDP destination port. */;
};

/*
 * Ramrod data for PF start ramrod
 */
struct pf_start_ramrod_data {
	struct regpair event_ring_pbl_addr /* Address of event ring PBL */;
	struct regpair consolid_q_pbl_addr
	    /* PBL address of consolidation queue */;
	struct pf_start_tunnel_config tunnel_config /* tunnel configuration. */
	  ;
	__le16 event_ring_sb_id /* Status block ID */;
	u8 base_vf_id;
	  ;
	u8 num_vfs /* Amount of vfs owned by PF */;
	u8 event_ring_num_pages /* Number of PBL pages in event ring */;
	u8 event_ring_sb_index /* Status block index */;
	u8 path_id /* HW path ID (engine ID) */;
	u8 warning_as_error /* In FW asserts, treat warning as error */;
	u8 dont_log_ramrods
	    /* If not set - throw a warning for each ramrod (for debug) */;
	u8 personality /* define what type of personality is new PF */;
	__le16 log_type_mask;
	u8 mf_mode /* Multi function mode */;
	u8 integ_phase /* Integration phase */;
	u8 allow_npar_tx_switching;
	u8 inner_to_outer_pri_map[8];
	u8 pri_map_valid
	    /* If inner_to_outer_pri_map is initialize then set pri_map_valid */
	  ;
	__le32 outer_tag;
	u8 reserved0[4];
};

/*
 * Data for port update ramrod
 */
struct protocol_dcb_data {
	u8 dcb_enable_flag /* dcbEnable flag value */;
	u8 dcb_priority /* dcbPri flag value */;
	u8 dcb_tc /* dcb TC value */;
	u8 reserved;
};

/*
 * tunnel configuration
 */
struct pf_update_tunnel_config {
	u8 update_rx_pf_clss;
	u8 update_tx_pf_clss;
	u8 set_vxlan_udp_port_flg
	    /* Update VXLAN tunnel UDP destination port. */;
	u8 set_geneve_udp_port_flg
	    /* Update GENEVE tunnel UDP destination port. */;
	u8 tx_enable_vxlan /* If set, enable VXLAN tunnel in TX path. */;
	u8 tx_enable_l2geneve /* If set, enable l2 GENEVE tunnel in TX path. */
	  ;
	u8 tx_enable_ipgeneve /* If set, enable IP GENEVE tunnel in TX path. */
	  ;
	u8 tx_enable_l2gre /* If set, enable l2 GRE tunnel in TX path. */;
	u8 tx_enable_ipgre /* If set, enable IP GRE tunnel in TX path. */;
	u8 tunnel_clss_vxlan /* Classification scheme for VXLAN tunnel. */;
	u8 tunnel_clss_l2geneve
	    /* Classification scheme for l2 GENEVE tunnel. */;
	u8 tunnel_clss_ipgeneve
	    /* Classification scheme for ip GENEVE tunnel. */;
	u8 tunnel_clss_l2gre /* Classification scheme for l2 GRE tunnel. */;
	u8 tunnel_clss_ipgre /* Classification scheme for ip GRE tunnel. */;
	__le16 vxlan_udp_port /* VXLAN tunnel UDP destination port. */;
	__le16 geneve_udp_port /* GENEVE tunnel UDP destination port. */;
	__le16 reserved[3];
};

/*
 * Data for port update ramrod
 */
struct pf_update_ramrod_data {
	u8 pf_id;
	u8 update_eth_dcb_data_flag /* Update Eth DCB  data indication */;
	u8 update_fcoe_dcb_data_flag /* Update FCOE DCB  data indication */;
	u8 update_iscsi_dcb_data_flag /* Update iSCSI DCB  data indication */;
	u8 update_roce_dcb_data_flag /* Update ROCE DCB  data indication */;
	u8 update_iwarp_dcb_data_flag /* Update IWARP DCB  data indication */;
	u8 update_mf_vlan_flag /* Update MF outer vlan Id */;
	u8 reserved;
	struct protocol_dcb_data eth_dcb_data /* core eth related fields */;
	struct protocol_dcb_data fcoe_dcb_data /* core fcoe related fields */;
	struct protocol_dcb_data iscsi_dcb_data /* core iscsi related fields */
	  ;
	struct protocol_dcb_data roce_dcb_data /* core roce related fields */;
	struct protocol_dcb_data iwarp_dcb_data /* core iwarp related fields */
	  ;
	__le16 mf_vlan /* new outer vlan id value */;
	__le16 reserved2;
	struct pf_update_tunnel_config tunnel_config /* tunnel configuration. */
	  ;
};

/*
 * Ports mode
 */
enum ports_mode {
	ENGX2_PORTX1 /* 2 engines x 1 port */,
	ENGX2_PORTX2 /* 2 engines x 2 ports */,
	ENGX1_PORTX1 /* 1 engine  x 1 port */,
	ENGX1_PORTX2 /* 1 engine  x 2 ports */,
	ENGX1_PORTX4 /* 1 engine  x 4 ports */,
	MAX_PORTS_MODE
};

/*
 * RDMA TX Stats
 */
struct rdma_sent_stats {
	struct regpair sent_bytes /* number of total RDMA bytes sent */;
	struct regpair sent_pkts /* number of total RDMA packets sent */;
};

/*
 * Pstorm non-triggering VF zone
 */
struct pstorm_non_trigger_vf_zone {
	struct eth_pstorm_per_queue_stat eth_queue_stat
	    /* VF statistic bucket */;
	struct rdma_sent_stats rdma_stats /* RoCE sent statistics */;
};

/*
 * Pstorm VF zone
 */
struct pstorm_vf_zone {
	struct pstorm_non_trigger_vf_zone non_trigger
	    /* non-interrupt-triggering zone */;
	struct regpair reserved[7] /* vf_zone size mus be power of 2 */;
};

/*
 * Ramrod Header of SPQE
 */
struct ramrod_header {
	__le32 cid /* Slowpath Connection CID */;
	u8 cmd_id /* Ramrod Cmd (Per Protocol Type) */;
	u8 protocol_id /* Ramrod Protocol ID */;
	__le16 echo /* Ramrod echo */;
};

/*
 * RDMA RX Stats
 */
struct rdma_rcv_stats {
	struct regpair rcv_bytes /* number of total RDMA bytes received */;
	struct regpair rcv_pkts /* number of total RDMA packets received */;
};

/*
 * Slowpath Element (SPQE)
 */
struct slow_path_element {
	struct ramrod_header hdr /* Ramrod Header */;
	struct regpair data_ptr /* Pointer to the Ramrod Data on the Host */;
};

/*
 * Tstorm non-triggering VF zone
 */
struct tstorm_non_trigger_vf_zone {
	struct rdma_rcv_stats rdma_stats /* RoCE received statistics */;
};

struct tstorm_per_port_stat {
	struct regpair trunc_error_discard
	    /* packet is dropped because it was truncated in NIG */;
	struct regpair mac_error_discard
	    /* packet is dropped because of Ethernet FCS error */;
	struct regpair mftag_filter_discard
	    /* packet is dropped because classification was unsuccessful */;
	struct regpair eth_mac_filter_discard;
	struct regpair ll2_mac_filter_discard;
	struct regpair ll2_conn_disabled_discard;
	struct regpair iscsi_irregular_pkt
	    /* packet is an ISCSI irregular packet */;
	struct regpair fcoe_irregular_pkt
	    /* packet is an FCOE irregular packet */;
	struct regpair roce_irregular_pkt
	    /* packet is an ROCE irregular packet */;
	struct regpair eth_irregular_pkt /* packet is an ETH irregular packet */
	  ;
	struct regpair toe_irregular_pkt /* packet is an TOE irregular packet */
	  ;
	struct regpair preroce_irregular_pkt
	    /* packet is an PREROCE irregular packet */;
};

/*
 * Tstorm VF zone
 */
struct tstorm_vf_zone {
	struct tstorm_non_trigger_vf_zone non_trigger
	    /* non-interrupt-triggering zone */;
};

/*
 * Tunnel classification scheme
 */
enum tunnel_clss {
	TUNNEL_CLSS_MAC_VLAN =
	    0
	    /* Use MAC & VLAN from first L2 header for vport classification. */
	    ,
	TUNNEL_CLSS_MAC_VNI
	    ,
	TUNNEL_CLSS_INNER_MAC_VLAN
	    /* Use MAC and VLAN from last L2 header for vport classification */
	    ,
	TUNNEL_CLSS_INNER_MAC_VNI
	    ,
	MAX_TUNNEL_CLSS
};

/*
 * Ustorm non-triggering VF zone
 */
struct ustorm_non_trigger_vf_zone {
	struct eth_ustorm_per_queue_stat eth_queue_stat
	    /* VF statistic bucket */;
	struct regpair vf_pf_msg_addr /* VF-PF message address */;
};

/*
 * Ustorm triggering VF zone
 */
struct ustorm_trigger_vf_zone {
	u8 vf_pf_msg_valid /* VF-PF message valid flag */;
	u8 reserved[7];
};

/*
 * Ustorm VF zone
 */
struct ustorm_vf_zone {
	struct ustorm_non_trigger_vf_zone non_trigger
	    /* non-interrupt-triggering zone */;
	struct ustorm_trigger_vf_zone trigger /* interrupt triggering zone */;
};

/*
 * VF-PF channel data
 */
struct vf_pf_channel_data {
	__le32 ready;
	u8 valid;
	u8 reserved0;
	__le16 reserved1;
};

/*
 * Ramrod data for VF start ramrod
 */
struct vf_start_ramrod_data {
	u8 vf_id /* VF ID */;
	u8 enable_flr_ack;
	__le16 opaque_fid /* VF opaque FID */;
	u8 personality /* define what type of personality is new VF */;
	u8 reserved[3];
};

/*
 * Ramrod data for VF start ramrod
 */
struct vf_stop_ramrod_data {
	u8 vf_id /* VF ID */;
	u8 reserved0;
	__le16 reserved1;
	__le32 reserved2;
};

/*
 * Attentions status block
 */
struct atten_status_block {
	__le32 atten_bits;
	__le32 atten_ack;
	__le16 reserved0;
	__le16 sb_index /* status block running index */;
	__le32 reserved1;
};

enum block_addr {
	GRCBASE_GRC = 0x50000,
	GRCBASE_MISCS = 0x9000,
	GRCBASE_MISC = 0x8000,
	GRCBASE_DBU = 0xa000,
	GRCBASE_PGLUE_B = 0x2a8000,
	GRCBASE_CNIG = 0x218000,
	GRCBASE_CPMU = 0x30000,
	GRCBASE_NCSI = 0x40000,
	GRCBASE_OPTE = 0x53000,
	GRCBASE_BMB = 0x540000,
	GRCBASE_PCIE = 0x54000,
	GRCBASE_MCP = 0xe00000,
	GRCBASE_MCP2 = 0x52000,
	GRCBASE_PSWHST = 0x2a0000,
	GRCBASE_PSWHST2 = 0x29e000,
	GRCBASE_PSWRD = 0x29c000,
	GRCBASE_PSWRD2 = 0x29d000,
	GRCBASE_PSWWR = 0x29a000,
	GRCBASE_PSWWR2 = 0x29b000,
	GRCBASE_PSWRQ = 0x280000,
	GRCBASE_PSWRQ2 = 0x240000,
	GRCBASE_PGLCS = 0x0,
	GRCBASE_DMAE = 0xc000,
	GRCBASE_PTU = 0x560000,
	GRCBASE_TCM = 0x1180000,
	GRCBASE_MCM = 0x1200000,
	GRCBASE_UCM = 0x1280000,
	GRCBASE_XCM = 0x1000000,
	GRCBASE_YCM = 0x1080000,
	GRCBASE_PCM = 0x1100000,
	GRCBASE_QM = 0x2f0000,
	GRCBASE_TM = 0x2c0000,
	GRCBASE_DORQ = 0x100000,
	GRCBASE_BRB = 0x340000,
	GRCBASE_SRC = 0x238000,
	GRCBASE_PRS = 0x1f0000,
	GRCBASE_TSDM = 0xfb0000,
	GRCBASE_MSDM = 0xfc0000,
	GRCBASE_USDM = 0xfd0000,
	GRCBASE_XSDM = 0xf80000,
	GRCBASE_YSDM = 0xf90000,
	GRCBASE_PSDM = 0xfa0000,
	GRCBASE_TSEM = 0x1700000,
	GRCBASE_MSEM = 0x1800000,
	GRCBASE_USEM = 0x1900000,
	GRCBASE_XSEM = 0x1400000,
	GRCBASE_YSEM = 0x1500000,
	GRCBASE_PSEM = 0x1600000,
	GRCBASE_RSS = 0x238800,
	GRCBASE_TMLD = 0x4d0000,
	GRCBASE_MULD = 0x4e0000,
	GRCBASE_YULD = 0x4c8000,
	GRCBASE_XYLD = 0x4c0000,
	GRCBASE_PRM = 0x230000,
	GRCBASE_PBF_PB1 = 0xda0000,
	GRCBASE_PBF_PB2 = 0xda4000,
	GRCBASE_RPB = 0x23c000,
	GRCBASE_BTB = 0xdb0000,
	GRCBASE_PBF = 0xd80000,
	GRCBASE_RDIF = 0x300000,
	GRCBASE_TDIF = 0x310000,
	GRCBASE_CDU = 0x580000,
	GRCBASE_CCFC = 0x2e0000,
	GRCBASE_TCFC = 0x2d0000,
	GRCBASE_IGU = 0x180000,
	GRCBASE_CAU = 0x1c0000,
	GRCBASE_UMAC = 0x51000,
	GRCBASE_XMAC = 0x210000,
	GRCBASE_DBG = 0x10000,
	GRCBASE_NIG = 0x500000,
	GRCBASE_WOL = 0x600000,
	GRCBASE_BMBN = 0x610000,
	GRCBASE_IPC = 0x20000,
	GRCBASE_NWM = 0x800000,
	GRCBASE_NWS = 0x700000,
	GRCBASE_MS = 0x6a0000,
	GRCBASE_PHY_PCIE = 0x620000,
	GRCBASE_MISC_AEU = 0x8000,
	GRCBASE_BAR0_MAP = 0x1c00000,
	MAX_BLOCK_ADDR
};

enum block_id {
	BLOCK_GRC,
	BLOCK_MISCS,
	BLOCK_MISC,
	BLOCK_DBU,
	BLOCK_PGLUE_B,
	BLOCK_CNIG,
	BLOCK_CPMU,
	BLOCK_NCSI,
	BLOCK_OPTE,
	BLOCK_BMB,
	BLOCK_PCIE,
	BLOCK_MCP,
	BLOCK_MCP2,
	BLOCK_PSWHST,
	BLOCK_PSWHST2,
	BLOCK_PSWRD,
	BLOCK_PSWRD2,
	BLOCK_PSWWR,
	BLOCK_PSWWR2,
	BLOCK_PSWRQ,
	BLOCK_PSWRQ2,
	BLOCK_PGLCS,
	BLOCK_DMAE,
	BLOCK_PTU,
	BLOCK_TCM,
	BLOCK_MCM,
	BLOCK_UCM,
	BLOCK_XCM,
	BLOCK_YCM,
	BLOCK_PCM,
	BLOCK_QM,
	BLOCK_TM,
	BLOCK_DORQ,
	BLOCK_BRB,
	BLOCK_SRC,
	BLOCK_PRS,
	BLOCK_TSDM,
	BLOCK_MSDM,
	BLOCK_USDM,
	BLOCK_XSDM,
	BLOCK_YSDM,
	BLOCK_PSDM,
	BLOCK_TSEM,
	BLOCK_MSEM,
	BLOCK_USEM,
	BLOCK_XSEM,
	BLOCK_YSEM,
	BLOCK_PSEM,
	BLOCK_RSS,
	BLOCK_TMLD,
	BLOCK_MULD,
	BLOCK_YULD,
	BLOCK_XYLD,
	BLOCK_PRM,
	BLOCK_PBF_PB1,
	BLOCK_PBF_PB2,
	BLOCK_RPB,
	BLOCK_BTB,
	BLOCK_PBF,
	BLOCK_RDIF,
	BLOCK_TDIF,
	BLOCK_CDU,
	BLOCK_CCFC,
	BLOCK_TCFC,
	BLOCK_IGU,
	BLOCK_CAU,
	BLOCK_UMAC,
	BLOCK_XMAC,
	BLOCK_DBG,
	BLOCK_NIG,
	BLOCK_WOL,
	BLOCK_BMBN,
	BLOCK_IPC,
	BLOCK_NWM,
	BLOCK_NWS,
	BLOCK_MS,
	BLOCK_PHY_PCIE,
	BLOCK_MISC_AEU,
	BLOCK_BAR0_MAP,
	MAX_BLOCK_ID
};

/*
 * Igu cleanup bit values to distinguish between clean or producer consumer
 */
enum command_type_bit {
	IGU_COMMAND_TYPE_NOP = 0,
	IGU_COMMAND_TYPE_SET = 1,
	MAX_COMMAND_TYPE_BIT
};

/*
 * DMAE command
 */
struct dmae_cmd {
	__le32 opcode;
#define DMAE_CMD_SRC_MASK              0x1
#define DMAE_CMD_SRC_SHIFT             0
#define DMAE_CMD_DST_MASK              0x3
#define DMAE_CMD_DST_SHIFT             1
#define DMAE_CMD_C_DST_MASK            0x1
#define DMAE_CMD_C_DST_SHIFT           3
#define DMAE_CMD_CRC_RESET_MASK        0x1
#define DMAE_CMD_CRC_RESET_SHIFT       4
#define DMAE_CMD_SRC_ADDR_RESET_MASK   0x1
#define DMAE_CMD_SRC_ADDR_RESET_SHIFT  5
#define DMAE_CMD_DST_ADDR_RESET_MASK   0x1
#define DMAE_CMD_DST_ADDR_RESET_SHIFT  6
#define DMAE_CMD_COMP_FUNC_MASK        0x1
#define DMAE_CMD_COMP_FUNC_SHIFT       7
#define DMAE_CMD_COMP_WORD_EN_MASK     0x1
#define DMAE_CMD_COMP_WORD_EN_SHIFT    8
#define DMAE_CMD_COMP_CRC_EN_MASK      0x1
#define DMAE_CMD_COMP_CRC_EN_SHIFT     9
#define DMAE_CMD_COMP_CRC_OFFSET_MASK  0x7
#define DMAE_CMD_COMP_CRC_OFFSET_SHIFT 10
#define DMAE_CMD_RESERVED1_MASK        0x1
#define DMAE_CMD_RESERVED1_SHIFT       13
#define DMAE_CMD_ENDIANITY_MODE_MASK   0x3
#define DMAE_CMD_ENDIANITY_MODE_SHIFT  14
#define DMAE_CMD_ERR_HANDLING_MASK     0x3
#define DMAE_CMD_ERR_HANDLING_SHIFT    16
#define DMAE_CMD_PORT_ID_MASK          0x3
#define DMAE_CMD_PORT_ID_SHIFT         18
#define DMAE_CMD_SRC_PF_ID_MASK        0xF
#define DMAE_CMD_SRC_PF_ID_SHIFT       20
#define DMAE_CMD_DST_PF_ID_MASK        0xF
#define DMAE_CMD_DST_PF_ID_SHIFT       24
#define DMAE_CMD_SRC_VF_ID_VALID_MASK  0x1
#define DMAE_CMD_SRC_VF_ID_VALID_SHIFT 28
#define DMAE_CMD_DST_VF_ID_VALID_MASK  0x1
#define DMAE_CMD_DST_VF_ID_VALID_SHIFT 29
#define DMAE_CMD_RESERVED2_MASK        0x3
#define DMAE_CMD_RESERVED2_SHIFT       30
	__le32 src_addr_lo
	    /* PCIe source address low in bytes or GRC source address in DW */;
	__le32 src_addr_hi;
	__le32 dst_addr_lo;
	__le32 dst_addr_hi;
	__le16 length /* Length in DW */;
	__le16 opcode_b;
#define DMAE_CMD_SRC_VF_ID_MASK        0xFF
#define DMAE_CMD_SRC_VF_ID_SHIFT       0
#define DMAE_CMD_DST_VF_ID_MASK        0xFF
#define DMAE_CMD_DST_VF_ID_SHIFT       8
	__le32 comp_addr_lo /* PCIe completion address low or grc address */;
	__le32 comp_addr_hi;
	__le32 comp_val /* Value to write to completion address */;
	__le32 crc32 /* crc16 result */;
	__le32 crc_32_c /* crc32_c result */;
	__le16 crc16 /* crc16 result */;
	__le16 crc16_c /* crc16_c result */;
	__le16 crc10 /* crc_t10 result */;
	__le16 reserved;
	__le16 xsum16 /* checksum16 result  */;
	__le16 xsum8 /* checksum8 result  */;
};

struct fw_ver_num {
	u8 major /* Firmware major version number */;
	u8 minor /* Firmware minor version number */;
	u8 rev /* Firmware revision version number */;
	u8 eng /* Firmware engineering version number (for bootleg versions) */
	  ;
};

struct fw_ver_info {
	__le16 tools_ver /* Tools version number */;
	u8 image_id /* FW image ID (e.g. main, l2b, kuku) */;
	u8 reserved1;
	struct fw_ver_num num /* FW version number */;
	__le32 timestamp /* FW Timestamp in unix time  (sec. since 1970) */;
	__le32 reserved2;
};

struct storm_ram_section {
	__le16 offset
	    /* The offset of the section in the RAM (in 64 bit units) */;
	__le16 size /* The size of the section (in 64 bit units) */;
};

struct fw_info {
	struct fw_ver_info ver /* FW version information */;
	struct storm_ram_section fw_asserts_section
	    /* The FW Asserts offset/size in Storm RAM */;
	__le32 reserved;
};

struct fw_info_location {
	__le32 grc_addr /* GRC address where the fw_info struct is located. */;
	__le32 size
	    /* Size of the fw_info structure (thats located at the grc_addr). */
	  ;
};

/*
 * IGU cleanup command
 */
struct igu_cleanup {
	__le32 sb_id_and_flags;
#define IGU_CLEANUP_RESERVED0_MASK     0x7FFFFFF
#define IGU_CLEANUP_RESERVED0_SHIFT    0
#define IGU_CLEANUP_CLEANUP_SET_MASK   0x1
#define IGU_CLEANUP_CLEANUP_SET_SHIFT  27
#define IGU_CLEANUP_CLEANUP_TYPE_MASK  0x7
#define IGU_CLEANUP_CLEANUP_TYPE_SHIFT 28
#define IGU_CLEANUP_COMMAND_TYPE_MASK  0x1
#define IGU_CLEANUP_COMMAND_TYPE_SHIFT 31
	__le32 reserved1;
};

/*
 * IGU firmware driver command
 */
union igu_command {
	struct igu_prod_cons_update prod_cons_update;
	struct igu_cleanup cleanup;
};

/*
 * IGU firmware driver command
 */
struct igu_command_reg_ctrl {
	__le16 opaque_fid;
	__le16 igu_command_reg_ctrl_fields;
#define IGU_COMMAND_REG_CTRL_PXP_BAR_ADDR_MASK  0xFFF
#define IGU_COMMAND_REG_CTRL_PXP_BAR_ADDR_SHIFT 0
#define IGU_COMMAND_REG_CTRL_RESERVED_MASK      0x7
#define IGU_COMMAND_REG_CTRL_RESERVED_SHIFT     12
#define IGU_COMMAND_REG_CTRL_COMMAND_TYPE_MASK  0x1
#define IGU_COMMAND_REG_CTRL_COMMAND_TYPE_SHIFT 15
};

/*
 * IGU mapping line structure
 */
struct igu_mapping_line {
	__le32 igu_mapping_line_fields;
#define IGU_MAPPING_LINE_VALID_MASK            0x1
#define IGU_MAPPING_LINE_VALID_SHIFT           0
#define IGU_MAPPING_LINE_VECTOR_NUMBER_MASK    0xFF
#define IGU_MAPPING_LINE_VECTOR_NUMBER_SHIFT   1
#define IGU_MAPPING_LINE_FUNCTION_NUMBER_MASK  0xFF
#define IGU_MAPPING_LINE_FUNCTION_NUMBER_SHIFT 9
#define IGU_MAPPING_LINE_PF_VALID_MASK         0x1
#define IGU_MAPPING_LINE_PF_VALID_SHIFT        17
#define IGU_MAPPING_LINE_IPS_GROUP_MASK        0x3F
#define IGU_MAPPING_LINE_IPS_GROUP_SHIFT       18
#define IGU_MAPPING_LINE_RESERVED_MASK         0xFF
#define IGU_MAPPING_LINE_RESERVED_SHIFT        24
};

/*
 * IGU MSIX line structure
 */
struct igu_msix_vector {
	struct regpair address;
	__le32 data;
	__le32 msix_vector_fields;
#define IGU_MSIX_VECTOR_MASK_BIT_MASK      0x1
#define IGU_MSIX_VECTOR_MASK_BIT_SHIFT     0
#define IGU_MSIX_VECTOR_RESERVED0_MASK     0x7FFF
#define IGU_MSIX_VECTOR_RESERVED0_SHIFT    1
#define IGU_MSIX_VECTOR_STEERING_TAG_MASK  0xFF
#define IGU_MSIX_VECTOR_STEERING_TAG_SHIFT 16
#define IGU_MSIX_VECTOR_RESERVED1_MASK     0xFF
#define IGU_MSIX_VECTOR_RESERVED1_SHIFT    24
};

enum init_modes {
	MODE_BB_A0,
	MODE_BB_B0,
	MODE_K2,
	MODE_ASIC,
	MODE_EMUL_REDUCED,
	MODE_EMUL_FULL,
	MODE_FPGA,
	MODE_CHIPSIM,
	MODE_SF,
	MODE_MF_SD,
	MODE_MF_SI,
	MODE_PORTS_PER_ENG_1,
	MODE_PORTS_PER_ENG_2,
	MODE_PORTS_PER_ENG_4,
	MODE_100G,
	MODE_EAGLE_ENG1_WORKAROUND,
	MAX_INIT_MODES
};

enum init_phases {
	PHASE_ENGINE,
	PHASE_PORT,
	PHASE_PF,
	PHASE_VF,
	PHASE_QM_PF,
	MAX_INIT_PHASES
};

struct mstorm_core_conn_ag_ctx {
	u8 byte0 /* cdu_validation */;
	u8 byte1 /* state */;
	u8 flags0;
#define MSTORM_CORE_CONN_AG_CTX_BIT0_MASK     0x1
#define MSTORM_CORE_CONN_AG_CTX_BIT0_SHIFT    0
#define MSTORM_CORE_CONN_AG_CTX_BIT1_MASK     0x1
#define MSTORM_CORE_CONN_AG_CTX_BIT1_SHIFT    1
#define MSTORM_CORE_CONN_AG_CTX_CF0_MASK      0x3
#define MSTORM_CORE_CONN_AG_CTX_CF0_SHIFT     2
#define MSTORM_CORE_CONN_AG_CTX_CF1_MASK      0x3
#define MSTORM_CORE_CONN_AG_CTX_CF1_SHIFT     4
#define MSTORM_CORE_CONN_AG_CTX_CF2_MASK      0x3
#define MSTORM_CORE_CONN_AG_CTX_CF2_SHIFT     6
	u8 flags1;
#define MSTORM_CORE_CONN_AG_CTX_CF0EN_MASK    0x1
#define MSTORM_CORE_CONN_AG_CTX_CF0EN_SHIFT   0
#define MSTORM_CORE_CONN_AG_CTX_CF1EN_MASK    0x1
#define MSTORM_CORE_CONN_AG_CTX_CF1EN_SHIFT   1
#define MSTORM_CORE_CONN_AG_CTX_CF2EN_MASK    0x1
#define MSTORM_CORE_CONN_AG_CTX_CF2EN_SHIFT   2
#define MSTORM_CORE_CONN_AG_CTX_RULE0EN_MASK  0x1
#define MSTORM_CORE_CONN_AG_CTX_RULE0EN_SHIFT 3
#define MSTORM_CORE_CONN_AG_CTX_RULE1EN_MASK  0x1
#define MSTORM_CORE_CONN_AG_CTX_RULE1EN_SHIFT 4
#define MSTORM_CORE_CONN_AG_CTX_RULE2EN_MASK  0x1
#define MSTORM_CORE_CONN_AG_CTX_RULE2EN_SHIFT 5
#define MSTORM_CORE_CONN_AG_CTX_RULE3EN_MASK  0x1
#define MSTORM_CORE_CONN_AG_CTX_RULE3EN_SHIFT 6
#define MSTORM_CORE_CONN_AG_CTX_RULE4EN_MASK  0x1
#define MSTORM_CORE_CONN_AG_CTX_RULE4EN_SHIFT 7
	__le16 word0 /* word0 */;
	__le16 word1 /* word1 */;
	__le32 reg0 /* reg0 */;
	__le32 reg1 /* reg1 */;
};

/*
 * per encapsulation type enabling flags
 */
struct prs_reg_encapsulation_type_en {
	u8 flags;
#define PRS_REG_ENCAPSULATION_TYPE_EN_ETH_OVER_GRE_ENABLE_MASK     0x1
#define PRS_REG_ENCAPSULATION_TYPE_EN_ETH_OVER_GRE_ENABLE_SHIFT    0
#define PRS_REG_ENCAPSULATION_TYPE_EN_IP_OVER_GRE_ENABLE_MASK      0x1
#define PRS_REG_ENCAPSULATION_TYPE_EN_IP_OVER_GRE_ENABLE_SHIFT     1
#define PRS_REG_ENCAPSULATION_TYPE_EN_VXLAN_ENABLE_MASK            0x1
#define PRS_REG_ENCAPSULATION_TYPE_EN_VXLAN_ENABLE_SHIFT           2
#define PRS_REG_ENCAPSULATION_TYPE_EN_T_TAG_ENABLE_MASK            0x1
#define PRS_REG_ENCAPSULATION_TYPE_EN_T_TAG_ENABLE_SHIFT           3
#define PRS_REG_ENCAPSULATION_TYPE_EN_ETH_OVER_GENEVE_ENABLE_MASK  0x1
#define PRS_REG_ENCAPSULATION_TYPE_EN_ETH_OVER_GENEVE_ENABLE_SHIFT 4
#define PRS_REG_ENCAPSULATION_TYPE_EN_IP_OVER_GENEVE_ENABLE_MASK   0x1
#define PRS_REG_ENCAPSULATION_TYPE_EN_IP_OVER_GENEVE_ENABLE_SHIFT  5
#define PRS_REG_ENCAPSULATION_TYPE_EN_RESERVED_MASK                0x3
#define PRS_REG_ENCAPSULATION_TYPE_EN_RESERVED_SHIFT               6
};

enum pxp_tph_st_hint {
	TPH_ST_HINT_BIDIR /* Read/Write access by Host and Device */,
	TPH_ST_HINT_REQUESTER /* Read/Write access by Device */,
	TPH_ST_HINT_TARGET
	    /* Device Write and Host Read, or Host Write and Device Read */,
	TPH_ST_HINT_TARGET_PRIO,
	MAX_PXP_TPH_ST_HINT
};

/*
 * QM hardware structure of enable bypass credit mask
 */
struct qm_rf_bypass_mask {
	u8 flags;
#define QM_RF_BYPASS_MASK_LINEVOQ_MASK    0x1
#define QM_RF_BYPASS_MASK_LINEVOQ_SHIFT   0
#define QM_RF_BYPASS_MASK_RESERVED0_MASK  0x1
#define QM_RF_BYPASS_MASK_RESERVED0_SHIFT 1
#define QM_RF_BYPASS_MASK_PFWFQ_MASK      0x1
#define QM_RF_BYPASS_MASK_PFWFQ_SHIFT     2
#define QM_RF_BYPASS_MASK_VPWFQ_MASK      0x1
#define QM_RF_BYPASS_MASK_VPWFQ_SHIFT     3
#define QM_RF_BYPASS_MASK_PFRL_MASK       0x1
#define QM_RF_BYPASS_MASK_PFRL_SHIFT      4
#define QM_RF_BYPASS_MASK_VPQCNRL_MASK    0x1
#define QM_RF_BYPASS_MASK_VPQCNRL_SHIFT   5
#define QM_RF_BYPASS_MASK_FWPAUSE_MASK    0x1
#define QM_RF_BYPASS_MASK_FWPAUSE_SHIFT   6
#define QM_RF_BYPASS_MASK_RESERVED1_MASK  0x1
#define QM_RF_BYPASS_MASK_RESERVED1_SHIFT 7
};

/*
 * QM hardware structure of opportunistic credit mask
 */
struct qm_rf_opportunistic_mask {
	__le16 flags;
#define QM_RF_OPPORTUNISTIC_MASK_LINEVOQ_MASK     0x1
#define QM_RF_OPPORTUNISTIC_MASK_LINEVOQ_SHIFT    0
#define QM_RF_OPPORTUNISTIC_MASK_BYTEVOQ_MASK     0x1
#define QM_RF_OPPORTUNISTIC_MASK_BYTEVOQ_SHIFT    1
#define QM_RF_OPPORTUNISTIC_MASK_PFWFQ_MASK       0x1
#define QM_RF_OPPORTUNISTIC_MASK_PFWFQ_SHIFT      2
#define QM_RF_OPPORTUNISTIC_MASK_VPWFQ_MASK       0x1
#define QM_RF_OPPORTUNISTIC_MASK_VPWFQ_SHIFT      3
#define QM_RF_OPPORTUNISTIC_MASK_PFRL_MASK        0x1
#define QM_RF_OPPORTUNISTIC_MASK_PFRL_SHIFT       4
#define QM_RF_OPPORTUNISTIC_MASK_VPQCNRL_MASK     0x1
#define QM_RF_OPPORTUNISTIC_MASK_VPQCNRL_SHIFT    5
#define QM_RF_OPPORTUNISTIC_MASK_FWPAUSE_MASK     0x1
#define QM_RF_OPPORTUNISTIC_MASK_FWPAUSE_SHIFT    6
#define QM_RF_OPPORTUNISTIC_MASK_RESERVED0_MASK   0x1
#define QM_RF_OPPORTUNISTIC_MASK_RESERVED0_SHIFT  7
#define QM_RF_OPPORTUNISTIC_MASK_QUEUEEMPTY_MASK  0x1
#define QM_RF_OPPORTUNISTIC_MASK_QUEUEEMPTY_SHIFT 8
#define QM_RF_OPPORTUNISTIC_MASK_RESERVED1_MASK   0x7F
#define QM_RF_OPPORTUNISTIC_MASK_RESERVED1_SHIFT  9
};

/*
 * QM hardware structure of QM map memory
 */
struct qm_rf_pq_map {
	__le32 reg;
#define QM_RF_PQ_MAP_PQ_VALID_MASK          0x1
#define QM_RF_PQ_MAP_PQ_VALID_SHIFT         0
#define QM_RF_PQ_MAP_RL_ID_MASK             0xFF
#define QM_RF_PQ_MAP_RL_ID_SHIFT            1
#define QM_RF_PQ_MAP_VP_PQ_ID_MASK          0x1FF
#define QM_RF_PQ_MAP_VP_PQ_ID_SHIFT         9
#define QM_RF_PQ_MAP_VOQ_MASK               0x1F
#define QM_RF_PQ_MAP_VOQ_SHIFT              18
#define QM_RF_PQ_MAP_WRR_WEIGHT_GROUP_MASK  0x3
#define QM_RF_PQ_MAP_WRR_WEIGHT_GROUP_SHIFT 23
#define QM_RF_PQ_MAP_RL_VALID_MASK          0x1
#define QM_RF_PQ_MAP_RL_VALID_SHIFT         25
#define QM_RF_PQ_MAP_RESERVED_MASK          0x3F
#define QM_RF_PQ_MAP_RESERVED_SHIFT         26
};

/*
 * Completion params for aggregated interrupt completion
 */
struct sdm_agg_int_comp_params {
	__le16 params;
#define SDM_AGG_INT_COMP_PARAMS_AGG_INT_INDEX_MASK      0x3F
#define SDM_AGG_INT_COMP_PARAMS_AGG_INT_INDEX_SHIFT     0
#define SDM_AGG_INT_COMP_PARAMS_AGG_VECTOR_ENABLE_MASK  0x1
#define SDM_AGG_INT_COMP_PARAMS_AGG_VECTOR_ENABLE_SHIFT 6
#define SDM_AGG_INT_COMP_PARAMS_AGG_VECTOR_BIT_MASK     0x1FF
#define SDM_AGG_INT_COMP_PARAMS_AGG_VECTOR_BIT_SHIFT    7
};

/*
 * SDM operation gen command (generate aggregative interrupt)
 */
struct sdm_op_gen {
	__le32 command;
#define SDM_OP_GEN_COMP_PARAM_MASK  0xFFFF
#define SDM_OP_GEN_COMP_PARAM_SHIFT 0
#define SDM_OP_GEN_COMP_TYPE_MASK   0xF
#define SDM_OP_GEN_COMP_TYPE_SHIFT  16
#define SDM_OP_GEN_RESERVED_MASK    0xFFF
#define SDM_OP_GEN_RESERVED_SHIFT   20
};

struct ystorm_core_conn_ag_ctx {
	u8 byte0 /* cdu_validation */;
	u8 byte1 /* state */;
	u8 flags0;
#define YSTORM_CORE_CONN_AG_CTX_BIT0_MASK     0x1
#define YSTORM_CORE_CONN_AG_CTX_BIT0_SHIFT    0
#define YSTORM_CORE_CONN_AG_CTX_BIT1_MASK     0x1
#define YSTORM_CORE_CONN_AG_CTX_BIT1_SHIFT    1
#define YSTORM_CORE_CONN_AG_CTX_CF0_MASK      0x3
#define YSTORM_CORE_CONN_AG_CTX_CF0_SHIFT     2
#define YSTORM_CORE_CONN_AG_CTX_CF1_MASK      0x3
#define YSTORM_CORE_CONN_AG_CTX_CF1_SHIFT     4
#define YSTORM_CORE_CONN_AG_CTX_CF2_MASK      0x3
#define YSTORM_CORE_CONN_AG_CTX_CF2_SHIFT     6
	u8 flags1;
#define YSTORM_CORE_CONN_AG_CTX_CF0EN_MASK    0x1
#define YSTORM_CORE_CONN_AG_CTX_CF0EN_SHIFT   0
#define YSTORM_CORE_CONN_AG_CTX_CF1EN_MASK    0x1
#define YSTORM_CORE_CONN_AG_CTX_CF1EN_SHIFT   1
#define YSTORM_CORE_CONN_AG_CTX_CF2EN_MASK    0x1
#define YSTORM_CORE_CONN_AG_CTX_CF2EN_SHIFT   2
#define YSTORM_CORE_CONN_AG_CTX_RULE0EN_MASK  0x1
#define YSTORM_CORE_CONN_AG_CTX_RULE0EN_SHIFT 3
#define YSTORM_CORE_CONN_AG_CTX_RULE1EN_MASK  0x1
#define YSTORM_CORE_CONN_AG_CTX_RULE1EN_SHIFT 4
#define YSTORM_CORE_CONN_AG_CTX_RULE2EN_MASK  0x1
#define YSTORM_CORE_CONN_AG_CTX_RULE2EN_SHIFT 5
#define YSTORM_CORE_CONN_AG_CTX_RULE3EN_MASK  0x1
#define YSTORM_CORE_CONN_AG_CTX_RULE3EN_SHIFT 6
#define YSTORM_CORE_CONN_AG_CTX_RULE4EN_MASK  0x1
#define YSTORM_CORE_CONN_AG_CTX_RULE4EN_SHIFT 7
	u8 byte2 /* byte2 */;
	u8 byte3 /* byte3 */;
	__le16 word0 /* word0 */;
	__le32 reg0 /* reg0 */;
	__le32 reg1 /* reg1 */;
	__le16 word1 /* word1 */;
	__le16 word2 /* word2 */;
	__le16 word3 /* word3 */;
	__le16 word4 /* word4 */;
	__le32 reg2 /* reg2 */;
	__le32 reg3 /* reg3 */;
};

#endif /* __ECORE_HSI_COMMON__ */
