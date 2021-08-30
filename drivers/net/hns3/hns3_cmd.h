/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#ifndef _HNS3_CMD_H_
#define _HNS3_CMD_H_

#include <stdint.h>

#define HNS3_CMDQ_TX_TIMEOUT		30000
#define HNS3_CMDQ_CLEAR_WAIT_TIME	200
#define HNS3_CMDQ_RX_INVLD_B		0
#define HNS3_CMDQ_RX_OUTVLD_B		1
#define HNS3_CMD_DESC_ALIGNMENT		4096
#define HNS3_CMD_FLAG_NEXT		BIT(2)

struct hns3_hw;

#define HNS3_CMD_DESC_DATA_NUM	6
struct hns3_cmd_desc {
	uint16_t opcode;
	uint16_t flag;
	uint16_t retval;
	uint16_t rsv;
	uint32_t data[HNS3_CMD_DESC_DATA_NUM];
};

struct hns3_cmq_ring {
	uint64_t desc_dma_addr;
	struct hns3_cmd_desc *desc;
	struct hns3_hw *hw;

	uint16_t buf_size;
	uint16_t desc_num;       /* max number of cmq descriptor */
	uint32_t next_to_use;
	uint32_t next_to_clean;
	uint8_t ring_type;       /* cmq ring type */
	rte_spinlock_t lock;     /* Command queue lock */

	const void *zone;        /* memory zone */
};

enum hns3_cmd_return_status {
	HNS3_CMD_EXEC_SUCCESS   = 0,
	HNS3_CMD_NO_AUTH        = 1,
	HNS3_CMD_NOT_SUPPORTED  = 2,
	HNS3_CMD_QUEUE_FULL     = 3,
	HNS3_CMD_NEXT_ERR       = 4,
	HNS3_CMD_UNEXE_ERR      = 5,
	HNS3_CMD_PARA_ERR       = 6,
	HNS3_CMD_RESULT_ERR     = 7,
	HNS3_CMD_TIMEOUT        = 8,
	HNS3_CMD_HILINK_ERR     = 9,
	HNS3_CMD_QUEUE_ILLEGAL  = 10,
	HNS3_CMD_INVALID        = 11,
	HNS3_CMD_ROH_CHECK_FAIL = 12
};

struct hns3_misc_vector {
	uint8_t *addr;
	int vector_irq;
};

struct hns3_cmq {
	struct hns3_cmq_ring csq;
	struct hns3_cmq_ring crq;
	uint16_t tx_timeout;
	enum hns3_cmd_return_status last_status;
};

enum hns3_opcode_type {
	/* Generic commands */
	HNS3_OPC_QUERY_FW_VER           = 0x0001,
	HNS3_OPC_CFG_RST_TRIGGER        = 0x0020,
	HNS3_OPC_GBL_RST_STATUS         = 0x0021,
	HNS3_OPC_QUERY_FUNC_STATUS      = 0x0022,
	HNS3_OPC_QUERY_PF_RSRC          = 0x0023,
	HNS3_OPC_QUERY_VF_RSRC          = 0x0024,
	HNS3_OPC_GET_CFG_PARAM          = 0x0025,
	HNS3_OPC_PF_RST_DONE            = 0x0026,

	HNS3_OPC_STATS_64_BIT           = 0x0030,
	HNS3_OPC_STATS_32_BIT           = 0x0031,
	HNS3_OPC_STATS_MAC              = 0x0032,
	HNS3_OPC_QUERY_MAC_REG_NUM      = 0x0033,
	HNS3_OPC_STATS_MAC_ALL          = 0x0034,

	HNS3_OPC_QUERY_REG_NUM          = 0x0040,
	HNS3_OPC_QUERY_32_BIT_REG       = 0x0041,
	HNS3_OPC_QUERY_64_BIT_REG       = 0x0042,
	HNS3_OPC_DFX_BD_NUM             = 0x0043,
	HNS3_OPC_DFX_BIOS_COMMON_REG    = 0x0044,
	HNS3_OPC_DFX_SSU_REG_0          = 0x0045,
	HNS3_OPC_DFX_SSU_REG_1          = 0x0046,
	HNS3_OPC_DFX_IGU_EGU_REG        = 0x0047,
	HNS3_OPC_DFX_RPU_REG_0          = 0x0048,
	HNS3_OPC_DFX_RPU_REG_1          = 0x0049,
	HNS3_OPC_DFX_NCSI_REG           = 0x004A,
	HNS3_OPC_DFX_RTC_REG            = 0x004B,
	HNS3_OPC_DFX_PPP_REG            = 0x004C,
	HNS3_OPC_DFX_RCB_REG            = 0x004D,
	HNS3_OPC_DFX_TQP_REG            = 0x004E,
	HNS3_OPC_DFX_SSU_REG_2          = 0x004F,

	HNS3_OPC_QUERY_DEV_SPECS        = 0x0050,

	HNS3_OPC_SSU_DROP_REG           = 0x0065,

	/* MAC command */
	HNS3_OPC_CONFIG_MAC_MODE        = 0x0301,
	HNS3_OPC_CONFIG_AN_MODE         = 0x0304,
	HNS3_OPC_QUERY_LINK_STATUS      = 0x0307,
	HNS3_OPC_CONFIG_MAX_FRM_SIZE    = 0x0308,
	HNS3_OPC_CONFIG_SPEED_DUP       = 0x0309,
	HNS3_OPC_QUERY_MAC_TNL_INT      = 0x0310,
	HNS3_OPC_MAC_TNL_INT_EN         = 0x0311,
	HNS3_OPC_CLEAR_MAC_TNL_INT      = 0x0312,
	HNS3_OPC_CONFIG_FEC_MODE        = 0x031A,

	/* PTP command */
	HNS3_OPC_PTP_INT_EN             = 0x0501,
	HNS3_OPC_CFG_PTP_MODE           = 0x0507,

	/* PFC/Pause commands */
	HNS3_OPC_CFG_MAC_PAUSE_EN       = 0x0701,
	HNS3_OPC_CFG_PFC_PAUSE_EN       = 0x0702,
	HNS3_OPC_CFG_MAC_PARA           = 0x0703,
	HNS3_OPC_CFG_PFC_PARA           = 0x0704,
	HNS3_OPC_QUERY_MAC_TX_PKT_CNT   = 0x0705,
	HNS3_OPC_QUERY_MAC_RX_PKT_CNT   = 0x0706,
	HNS3_OPC_QUERY_PFC_TX_PKT_CNT   = 0x0707,
	HNS3_OPC_QUERY_PFC_RX_PKT_CNT   = 0x0708,
	HNS3_OPC_PRI_TO_TC_MAPPING      = 0x0709,
	HNS3_OPC_QOS_MAP                = 0x070A,

	/* ETS/scheduler commands */
	HNS3_OPC_TM_PG_TO_PRI_LINK      = 0x0804,
	HNS3_OPC_TM_QS_TO_PRI_LINK      = 0x0805,
	HNS3_OPC_TM_NQ_TO_QS_LINK       = 0x0806,
	HNS3_OPC_TM_RQ_TO_QS_LINK       = 0x0807,
	HNS3_OPC_TM_PORT_WEIGHT         = 0x0808,
	HNS3_OPC_TM_PG_WEIGHT           = 0x0809,
	HNS3_OPC_TM_QS_WEIGHT           = 0x080A,
	HNS3_OPC_TM_PRI_WEIGHT          = 0x080B,
	HNS3_OPC_TM_PRI_C_SHAPPING      = 0x080C,
	HNS3_OPC_TM_PRI_P_SHAPPING      = 0x080D,
	HNS3_OPC_TM_PG_C_SHAPPING       = 0x080E,
	HNS3_OPC_TM_PG_P_SHAPPING       = 0x080F,
	HNS3_OPC_TM_PORT_SHAPPING       = 0x0810,
	HNS3_OPC_TM_PG_SCH_MODE_CFG     = 0x0812,
	HNS3_OPC_TM_PRI_SCH_MODE_CFG    = 0x0813,
	HNS3_OPC_TM_QS_SCH_MODE_CFG     = 0x0814,
	HNS3_OPC_TM_BP_TO_QSET_MAPPING  = 0x0815,
	HNS3_OPC_ETS_TC_WEIGHT          = 0x0843,
	HNS3_OPC_QSET_DFX_STS           = 0x0844,
	HNS3_OPC_PRI_DFX_STS            = 0x0845,
	HNS3_OPC_PG_DFX_STS             = 0x0846,
	HNS3_OPC_PORT_DFX_STS           = 0x0847,
	HNS3_OPC_SCH_NQ_CNT             = 0x0848,
	HNS3_OPC_SCH_RQ_CNT             = 0x0849,
	HNS3_OPC_TM_INTERNAL_STS        = 0x0850,
	HNS3_OPC_TM_INTERNAL_CNT        = 0x0851,
	HNS3_OPC_TM_INTERNAL_STS_1      = 0x0852,

	HNS3_OPC_TM_PORT_LIMIT_RATE     = 0x0870,
	HNS3_OPC_TM_TC_LIMIT_RATE       = 0x0871,

	/* Mailbox cmd */
	HNS3_OPC_MBX_VF_TO_PF           = 0x2001,

	/* Packet buffer allocate commands */
	HNS3_OPC_TX_BUFF_ALLOC          = 0x0901,
	HNS3_OPC_RX_PRIV_BUFF_ALLOC     = 0x0902,
	HNS3_OPC_RX_PRIV_WL_ALLOC       = 0x0903,
	HNS3_OPC_RX_COM_THRD_ALLOC      = 0x0904,
	HNS3_OPC_RX_COM_WL_ALLOC        = 0x0905,

	/* TQP management command */
	HNS3_OPC_SET_TQP_MAP            = 0x0A01,

	/* TQP commands */
	HNS3_OPC_QUERY_TX_STATUS        = 0x0B03,
	HNS3_OPC_QUERY_RX_STATUS        = 0x0B13,
	HNS3_OPC_CFG_COM_TQP_QUEUE      = 0x0B20,
	HNS3_OPC_RESET_TQP_QUEUE        = 0x0B22,
	HNS3_OPC_RESET_TQP_QUEUE_INDEP  = 0x0B23,

	/* TSO command */
	HNS3_OPC_TSO_GENERIC_CONFIG     = 0x0C01,
	HNS3_OPC_GRO_GENERIC_CONFIG     = 0x0C10,

	/* RSS commands */
	HNS3_OPC_RSS_GENERIC_CONFIG     = 0x0D01,
	HNS3_OPC_RSS_INPUT_TUPLE        = 0x0D02,
	HNS3_OPC_RSS_INDIR_TABLE        = 0x0D07,
	HNS3_OPC_RSS_TC_MODE            = 0x0D08,

	/* Promisuous mode command */
	HNS3_OPC_CFG_PROMISC_MODE       = 0x0E01,

	/* Vlan offload commands */
	HNS3_OPC_VLAN_PORT_TX_CFG       = 0x0F01,
	HNS3_OPC_VLAN_PORT_RX_CFG       = 0x0F02,

	/* MAC commands */
	HNS3_OPC_MAC_VLAN_ADD           = 0x1000,
	HNS3_OPC_MAC_VLAN_REMOVE        = 0x1001,
	HNS3_OPC_MAC_VLAN_TYPE_ID       = 0x1002,
	HNS3_OPC_MAC_VLAN_INSERT        = 0x1003,
	HNS3_OPC_MAC_VLAN_ALLOCATE      = 0x1004,
	HNS3_OPC_MAC_ETHTYPE_ADD        = 0x1010,

	/* VLAN commands */
	HNS3_OPC_VLAN_FILTER_CTRL       = 0x1100,
	HNS3_OPC_VLAN_FILTER_PF_CFG     = 0x1101,
	HNS3_OPC_VLAN_FILTER_VF_CFG     = 0x1102,

	/* Flow Director command */
	HNS3_OPC_FD_MODE_CTRL           = 0x1200,
	HNS3_OPC_FD_GET_ALLOCATION      = 0x1201,
	HNS3_OPC_FD_KEY_CONFIG          = 0x1202,
	HNS3_OPC_FD_TCAM_OP             = 0x1203,
	HNS3_OPC_FD_AD_OP               = 0x1204,
	HNS3_OPC_FD_COUNTER_OP          = 0x1205,

	/* Clear hardware state command */
	HNS3_OPC_CLEAR_HW_STATE         = 0x700B,

	/* Firmware stats command */
	HNS3_OPC_FIRMWARE_COMPAT_CFG    = 0x701A,
	/* Firmware control phy command */
	HNS3_OPC_PHY_PARAM_CFG          = 0x7025,

	/* SFP command */
	HNS3_OPC_GET_SFP_EEPROM         = 0x7100,
	HNS3_OPC_GET_SFP_EXIST          = 0x7101,
	HNS3_OPC_GET_SFP_INFO           = 0x7104,

	/* Interrupts commands */
	HNS3_OPC_ADD_RING_TO_VECTOR     = 0x1503,
	HNS3_OPC_DEL_RING_TO_VECTOR     = 0x1504,

	/* Error INT commands */
	HNS3_OPC_MAC_COMMON_INT_EN              = 0x030E,
	HNS3_OPC_TM_SCH_ECC_INT_EN              = 0x0829,
	HNS3_OPC_SSU_ECC_INT_CMD                = 0x0989,
	HNS3_OPC_SSU_COMMON_INT_CMD             = 0x098C,
	HNS3_OPC_PPU_MPF_ECC_INT_CMD            = 0x0B40,
	HNS3_OPC_PPU_MPF_OTHER_INT_CMD          = 0x0B41,
	HNS3_OPC_PPU_PF_OTHER_INT_CMD           = 0x0B42,
	HNS3_OPC_COMMON_ECC_INT_CFG             = 0x1505,
	HNS3_OPC_QUERY_RAS_INT_STS_BD_NUM       = 0x1510,
	HNS3_OPC_QUERY_CLEAR_MPF_RAS_INT        = 0x1511,
	HNS3_OPC_QUERY_CLEAR_PF_RAS_INT         = 0x1512,
	HNS3_OPC_QUERY_MSIX_INT_STS_BD_NUM      = 0x1513,
	HNS3_OPC_QUERY_CLEAR_ALL_MPF_MSIX_INT   = 0x1514,
	HNS3_OPC_QUERY_CLEAR_ALL_PF_MSIX_INT    = 0x1515,
	HNS3_OPC_QUERY_ALL_ERR_BD_NUM           = 0x1516,
	HNS3_OPC_QUERY_ALL_ERR_INFO             = 0x1517,
	HNS3_OPC_IGU_EGU_TNL_INT_EN             = 0x1803,
	HNS3_OPC_IGU_COMMON_INT_EN              = 0x1806,
	HNS3_OPC_TM_QCN_MEM_INT_CFG             = 0x1A14,
	HNS3_OPC_PPP_CMD0_INT_CMD               = 0x2100,
	HNS3_OPC_PPP_CMD1_INT_CMD               = 0x2101,
	HNS3_OPC_NCSI_INT_EN                    = 0x2401,
};

#define HNS3_CMD_FLAG_IN	BIT(0)
#define HNS3_CMD_FLAG_OUT	BIT(1)
#define HNS3_CMD_FLAG_NEXT	BIT(2)
#define HNS3_CMD_FLAG_WR	BIT(3)
#define HNS3_CMD_FLAG_NO_INTR	BIT(4)
#define HNS3_CMD_FLAG_ERR_INTR	BIT(5)

#define HNS3_MPF_RAS_INT_MIN_BD_NUM	10
#define HNS3_PF_RAS_INT_MIN_BD_NUM	4
#define HNS3_MPF_MSIX_INT_MIN_BD_NUM	10
#define HNS3_PF_MSIX_INT_MIN_BD_NUM	4

#define HNS3_BUF_SIZE_UNIT	256
#define HNS3_BUF_MUL_BY		2
#define HNS3_BUF_DIV_BY		2
#define NEED_RESERVE_TC_NUM	2
#define BUF_MAX_PERCENT		100
#define BUF_RESERVE_PERCENT	90

#define HNS3_MAX_TC_NUM		8
#define HNS3_TC0_PRI_BUF_EN_B	15 /* Bit 15 indicate enable or not */
#define HNS3_BUF_UNIT_S		7  /* Buf size is united by 128 bytes */
#define HNS3_TX_BUFF_RSV_NUM	8
struct hns3_tx_buff_alloc_cmd {
	uint16_t tx_pkt_buff[HNS3_MAX_TC_NUM];
	uint8_t tx_buff_rsv[HNS3_TX_BUFF_RSV_NUM];
};

struct hns3_rx_priv_buff_cmd {
	uint16_t buf_num[HNS3_MAX_TC_NUM];
	uint16_t shared_buf;
	uint8_t rsv[6];
};

#define HNS3_FW_VERSION_BYTE3_S		24
#define HNS3_FW_VERSION_BYTE3_M		GENMASK(31, 24)
#define HNS3_FW_VERSION_BYTE2_S		16
#define HNS3_FW_VERSION_BYTE2_M		GENMASK(23, 16)
#define HNS3_FW_VERSION_BYTE1_S		8
#define HNS3_FW_VERSION_BYTE1_M		GENMASK(15, 8)
#define HNS3_FW_VERSION_BYTE0_S		0
#define HNS3_FW_VERSION_BYTE0_M		GENMASK(7, 0)

enum HNS3_CAPS_BITS {
	/*
	 * The following capability index definitions must be the same as those
	 * of the firmware.
	 */
	HNS3_CAPS_FD_QUEUE_REGION_B = 2,
	HNS3_CAPS_PTP_B,
	HNS3_CAPS_TX_PUSH_B = 6,
	HNS3_CAPS_PHY_IMP_B = 7,
	HNS3_CAPS_TQP_TXRX_INDEP_B,
	HNS3_CAPS_HW_PAD_B,
	HNS3_CAPS_STASH_B,
	HNS3_CAPS_UDP_TUNNEL_CSUM_B,
	HNS3_CAPS_RAS_IMP_B,
	HNS3_CAPS_RXD_ADV_LAYOUT_B = 15,
	HNS3_CAPS_TM_B = 17,
};

/* Capabilities of VF dependent on the PF */
enum HNS3VF_CAPS_BITS {
	/*
	 * The following capability index definitions must be the same as those
	 * in kernel side PF.
	 */
	HNS3VF_CAPS_VLAN_FLT_MOD_B = 0,
};

enum HNS3_API_CAP_BITS {
	HNS3_API_CAP_FLEX_RSS_TBL_B,
};

#define HNS3_QUERY_CAP_LENGTH		3
struct hns3_query_version_cmd {
	uint32_t firmware;
	uint32_t hardware;
	uint32_t api_caps;
	uint32_t caps[HNS3_QUERY_CAP_LENGTH]; /* capabilities of device */
};

#define HNS3_RX_PRIV_EN_B	15
#define HNS3_TC_NUM_ONE_DESC	4
struct hns3_priv_wl {
	uint16_t high;
	uint16_t low;
};

struct hns3_rx_priv_wl_buf {
	struct hns3_priv_wl tc_wl[HNS3_TC_NUM_ONE_DESC];
};

struct hns3_rx_com_thrd {
	struct hns3_priv_wl com_thrd[HNS3_TC_NUM_ONE_DESC];
};

struct hns3_rx_com_wl {
	struct hns3_priv_wl com_wl;
};

struct hns3_waterline {
	uint32_t low;
	uint32_t high;
};

struct hns3_tc_thrd {
	uint32_t low;
	uint32_t high;
};

struct hns3_priv_buf {
	struct hns3_waterline wl; /* Waterline for low and high */
	uint32_t buf_size;        /* TC private buffer size */
	uint32_t tx_buf_size;
	uint32_t enable;          /* Enable TC private buffer or not */
};

struct hns3_shared_buf {
	struct hns3_waterline self;
	struct hns3_tc_thrd tc_thrd[HNS3_MAX_TC_NUM];
	uint32_t buf_size;
};

struct hns3_pkt_buf_alloc {
	struct hns3_priv_buf priv_buf[HNS3_MAX_TC_NUM];
	struct hns3_shared_buf s_buf;
};

#define HNS3_RX_COM_WL_EN_B	15
struct hns3_rx_com_wl_buf_cmd {
	uint16_t high_wl;
	uint16_t low_wl;
	uint8_t rsv[20];
};

#define HNS3_RX_PKT_EN_B	15
struct hns3_rx_pkt_buf_cmd {
	uint16_t high_pkt;
	uint16_t low_pkt;
	uint8_t rsv[20];
};

#define HNS3_PF_STATE_DONE_B	0
#define HNS3_PF_STATE_MAIN_B	1
#define HNS3_PF_STATE_BOND_B	2
#define HNS3_PF_STATE_MAC_N_B	6
#define HNS3_PF_MAC_NUM_MASK	0x3
#define HNS3_PF_STATE_MAIN	BIT(HNS3_PF_STATE_MAIN_B)
#define HNS3_PF_STATE_DONE	BIT(HNS3_PF_STATE_DONE_B)
#define HNS3_VF_RST_STATE_NUM	4
struct hns3_func_status_cmd {
	uint32_t vf_rst_state[HNS3_VF_RST_STATE_NUM];
	uint8_t pf_state;
	uint8_t mac_id;
	uint8_t rsv1;
	uint8_t pf_cnt_in_mac;
	uint8_t pf_num;
	uint8_t vf_num;
	uint8_t rsv[2];
};

#define HNS3_PF_VEC_NUM_S	0
#define HNS3_PF_VEC_NUM_M	GENMASK(15, 0)
#define HNS3_MIN_VECTOR_NUM	2 /* one for msi-x, another for IO */
struct hns3_pf_res_cmd {
	uint16_t tqp_num;
	uint16_t buf_size;
	uint16_t msixcap_localid_ba_nic;
	uint16_t nic_pf_intr_vector_number;
	uint16_t roce_pf_intr_vector_number;
	uint16_t pf_own_fun_number;
	uint16_t tx_buf_size;
	uint16_t dv_buf_size;
	/* number of queues that exceed 1024 */
	uint16_t ext_tqp_num;
	uint16_t roh_pf_intr_vector_number;
	uint32_t rsv[1];
};

#define HNS3_VF_VEC_NUM_S	0
#define HNS3_VF_VEC_NUM_M	GENMASK(7, 0)
struct hns3_vf_res_cmd {
	uint16_t tqp_num;
	uint16_t reserved;
	uint16_t msixcap_localid_ba_nic;
	uint16_t msixcap_localid_ba_rocee;
	uint16_t vf_intr_vector_number;
	uint16_t rsv[7];
};

#define HNS3_UMV_SPC_ALC_B	0
struct hns3_umv_spc_alc_cmd {
	uint8_t allocate;
	uint8_t rsv1[3];
	uint32_t space_size;
	uint8_t rsv2[16];
};

#define HNS3_CFG_OFFSET_S		0
#define HNS3_CFG_OFFSET_M		GENMASK(19, 0)
#define HNS3_CFG_RD_LEN_S		24
#define HNS3_CFG_RD_LEN_M		GENMASK(27, 24)
#define HNS3_CFG_RD_LEN_BYTES		16
#define HNS3_CFG_RD_LEN_UNIT		4

#define HNS3_CFG_TC_NUM_S		8
#define HNS3_CFG_TC_NUM_M		GENMASK(15, 8)
#define HNS3_CFG_TQP_DESC_N_S		16
#define HNS3_CFG_TQP_DESC_N_M		GENMASK(31, 16)
#define HNS3_CFG_PHY_ADDR_S		0
#define HNS3_CFG_PHY_ADDR_M		GENMASK(7, 0)
#define HNS3_CFG_MEDIA_TP_S		8
#define HNS3_CFG_MEDIA_TP_M		GENMASK(15, 8)
#define HNS3_CFG_RX_BUF_LEN_S		16
#define HNS3_CFG_RX_BUF_LEN_M		GENMASK(31, 16)
#define HNS3_CFG_MAC_ADDR_H_S		0
#define HNS3_CFG_MAC_ADDR_H_M		GENMASK(15, 0)
#define HNS3_CFG_DEFAULT_SPEED_S	16
#define HNS3_CFG_DEFAULT_SPEED_M	GENMASK(23, 16)
#define HNS3_CFG_RSS_SIZE_S		24
#define HNS3_CFG_RSS_SIZE_M		GENMASK(31, 24)
#define HNS3_CFG_SPEED_ABILITY_S	0
#define HNS3_CFG_SPEED_ABILITY_M	GENMASK(7, 0)
#define HNS3_CFG_UMV_TBL_SPACE_S	16
#define HNS3_CFG_UMV_TBL_SPACE_M	GENMASK(31, 16)
#define HNS3_CFG_EXT_RSS_SIZE_S		0
#define HNS3_CFG_EXT_RSS_SIZE_M		GENMASK(3, 0)

#define HNS3_ACCEPT_TAG1_B		0
#define HNS3_ACCEPT_UNTAG1_B		1
#define HNS3_PORT_INS_TAG1_EN_B		2
#define HNS3_PORT_INS_TAG2_EN_B		3
#define HNS3_CFG_NIC_ROCE_SEL_B		4
#define HNS3_ACCEPT_TAG2_B		5
#define HNS3_ACCEPT_UNTAG2_B		6
#define HNS3_TAG_SHIFT_MODE_EN_B	7

#define HNS3_REM_TAG1_EN_B		0
#define HNS3_REM_TAG2_EN_B		1
#define HNS3_SHOW_TAG1_EN_B		2
#define HNS3_SHOW_TAG2_EN_B		3
#define HNS3_DISCARD_TAG1_EN_B		5
#define HNS3_DISCARD_TAG2_EN_B		6

/* Factor used to calculate offset and bitmap of VF num */
#define HNS3_VF_NUM_PER_CMD             64
#define HNS3_VF_NUM_PER_BYTE            8

struct hns3_cfg_param_cmd {
	uint32_t offset;
	uint32_t rsv;
	uint32_t param[4];
};

#define HNS3_VPORT_VTAG_RX_CFG_CMD_VF_BITMAP_NUM	8
struct hns3_vport_vtag_rx_cfg_cmd {
	uint8_t vport_vlan_cfg;
	uint8_t vf_offset;
	uint8_t rsv1[6];
	uint8_t vf_bitmap[HNS3_VPORT_VTAG_RX_CFG_CMD_VF_BITMAP_NUM];
	uint8_t rsv2[8];
};

struct hns3_vport_vtag_tx_cfg_cmd {
	uint8_t vport_vlan_cfg;
	uint8_t vf_offset;
	uint8_t rsv1[2];
	uint16_t def_vlan_tag1;
	uint16_t def_vlan_tag2;
	uint8_t vf_bitmap[8];
	uint8_t rsv2[8];
};


struct hns3_vlan_filter_ctrl_cmd {
	uint8_t vlan_type;
	uint8_t vlan_fe;
	uint8_t rsv1[2];
	uint8_t vf_id;
	uint8_t rsv2[19];
};

#define HNS3_VLAN_OFFSET_BITMAP_NUM	20
struct hns3_vlan_filter_pf_cfg_cmd {
	uint8_t vlan_offset;
	uint8_t vlan_cfg;
	uint8_t rsv[2];
	uint8_t vlan_offset_bitmap[HNS3_VLAN_OFFSET_BITMAP_NUM];
};

#define HNS3_VLAN_FILTER_VF_CFG_CMD_VF_BITMAP_NUM	16
struct hns3_vlan_filter_vf_cfg_cmd {
	uint16_t vlan_id;
	uint8_t  resp_code;
	uint8_t  rsv;
	uint8_t  vlan_cfg;
	uint8_t  rsv1[3];
	uint8_t  vf_bitmap[HNS3_VLAN_FILTER_VF_CFG_CMD_VF_BITMAP_NUM];
};

struct hns3_tx_vlan_type_cfg_cmd {
	uint16_t ot_vlan_type;
	uint16_t in_vlan_type;
	uint8_t rsv[20];
};

struct hns3_rx_vlan_type_cfg_cmd {
	uint16_t ot_fst_vlan_type;
	uint16_t ot_sec_vlan_type;
	uint16_t in_fst_vlan_type;
	uint16_t in_sec_vlan_type;
	uint8_t rsv[16];
};

#define HNS3_TSO_MSS_MIN_S	0
#define HNS3_TSO_MSS_MIN_M	GENMASK(13, 0)

#define HNS3_TSO_MSS_MAX_S	16
#define HNS3_TSO_MSS_MAX_M	GENMASK(29, 16)

struct hns3_cfg_tso_status_cmd {
	rte_le16_t tso_mss_min;
	rte_le16_t tso_mss_max;
	uint8_t rsv[20];
};

#define HNS3_GRO_EN_B		0
struct hns3_cfg_gro_status_cmd {
	rte_le16_t gro_en;
	uint8_t rsv[22];
};

#define HNS3_TSO_MSS_MIN	256
#define HNS3_TSO_MSS_MAX	9668

#define HNS3_RSS_HASH_KEY_OFFSET_B	4

#define HNS3_RSS_CFG_TBL_SIZE	16
#define HNS3_RSS_HASH_KEY_NUM	16
/* Configure the algorithm mode and Hash Key, opcode:0x0D01 */
struct hns3_rss_generic_config_cmd {
	/* Hash_algorithm(8.0~8.3), hash_key_offset(8.4~8.7) */
	uint8_t hash_config;
	uint8_t rsv[7];
	uint8_t hash_key[HNS3_RSS_HASH_KEY_NUM];
};

/* Configure the tuple selection for RSS hash input, opcode:0x0D02 */
struct hns3_rss_input_tuple_cmd {
	uint64_t tuple_field;
	uint8_t rsv[16];
};

#define HNS3_RSS_CFG_TBL_SIZE		16
#define HNS3_RSS_CFG_TBL_SIZE_H		4
#define HNS3_RSS_CFG_TBL_BW_H		2
#define HNS3_RSS_CFG_TBL_BW_L		8

/* Configure the indirection table, opcode:0x0D07 */
struct hns3_rss_indirection_table_cmd {
	uint16_t start_table_index;  /* Bit3~0 must be 0x0. */
	uint16_t rss_set_bitmap;
	uint8_t rss_result_h[HNS3_RSS_CFG_TBL_SIZE_H];
	uint8_t rss_result_l[HNS3_RSS_CFG_TBL_SIZE];
};

#define HNS3_RSS_TC_OFFSET_S		0
#define HNS3_RSS_TC_OFFSET_M		GENMASK(10, 0)
#define HNS3_RSS_TC_SIZE_MSB_S		11
#define HNS3_RSS_TC_SIZE_MSB_OFFSET	3
#define HNS3_RSS_TC_SIZE_S		12
#define HNS3_RSS_TC_SIZE_M		GENMASK(14, 12)
#define HNS3_RSS_TC_VALID_B		15

/* Configure the tc_size and tc_offset, opcode:0x0D08 */
struct hns3_rss_tc_mode_cmd {
	uint16_t rss_tc_mode[HNS3_MAX_TC_NUM];
	uint8_t rsv[8];
};

#define HNS3_LINK_STATUS_UP_B	0
#define HNS3_LINK_STATUS_UP_M	BIT(HNS3_LINK_STATUS_UP_B)
struct hns3_link_status_cmd {
	uint8_t status;
	uint8_t rsv[23];
};

struct hns3_promisc_param {
	uint8_t vf_id;
	uint8_t enable;
};

#define HNS3_PROMISC_TX_EN_B	BIT(4)
#define HNS3_PROMISC_RX_EN_B	BIT(5)
#define HNS3_PROMISC_EN_B	1
#define HNS3_PROMISC_EN_ALL	0x7
#define HNS3_PROMISC_EN_UC	0x1
#define HNS3_PROMISC_EN_MC	0x2
#define HNS3_PROMISC_EN_BC	0x4
struct hns3_promisc_cfg_cmd {
	uint8_t flag;
	uint8_t vf_id;
	uint16_t rsv0;
	uint8_t rsv1[20];
};

enum hns3_promisc_type {
	HNS3_UNICAST	= 1,
	HNS3_MULTICAST	= 2,
	HNS3_BROADCAST	= 3,
};

#define HNS3_LINK_EVENT_REPORT_EN_B	0
#define HNS3_NCSI_ERROR_REPORT_EN_B	1
#define HNS3_FIRMWARE_PHY_DRIVER_EN_B	2
struct hns3_firmware_compat_cmd {
	uint32_t compat;
	uint8_t rsv[20];
};

/* Bitmap flags in supported, advertising and lp_advertising */
#define HNS3_PHY_LINK_SPEED_10M_HD_BIT		BIT(0)
#define HNS3_PHY_LINK_SPEED_10M_BIT		BIT(1)
#define HNS3_PHY_LINK_SPEED_100M_HD_BIT		BIT(2)
#define HNS3_PHY_LINK_SPEED_100M_BIT		BIT(3)
#define HNS3_PHY_LINK_SPEED_1000M_BIT		BIT(5)
#define HNS3_PHY_LINK_MODE_AUTONEG_BIT		BIT(6)
#define HNS3_PHY_LINK_MODE_PAUSE_BIT		BIT(13)
#define HNS3_PHY_LINK_MODE_ASYM_PAUSE_BIT	BIT(14)

#define HNS3_PHY_PARAM_CFG_BD_NUM	2
struct hns3_phy_params_bd0_cmd {
	uint32_t speed;
#define HNS3_PHY_DUPLEX_CFG_B		0
	uint8_t duplex;
#define HNS3_PHY_AUTONEG_CFG_B	0
	uint8_t autoneg;
	uint8_t eth_tp_mdix;
	uint8_t eth_tp_mdix_ctrl;
	uint8_t port;
	uint8_t transceiver;
	uint8_t phy_address;
	uint8_t rsv;
	uint32_t supported;
	uint32_t advertising;
	uint32_t lp_advertising;
};

struct hns3_phy_params_bd1_cmd {
	uint8_t master_slave_cfg;
	uint8_t master_slave_state;
	uint8_t rsv1[2];
	uint32_t rsv2[5];
};

#define HNS3_MAC_TX_EN_B		6
#define HNS3_MAC_RX_EN_B		7
#define HNS3_MAC_PAD_TX_B		11
#define HNS3_MAC_PAD_RX_B		12
#define HNS3_MAC_1588_TX_B		13
#define HNS3_MAC_1588_RX_B		14
#define HNS3_MAC_APP_LP_B		15
#define HNS3_MAC_LINE_LP_B		16
#define HNS3_MAC_FCS_TX_B		17
#define HNS3_MAC_RX_OVERSIZE_TRUNCATE_B	18
#define HNS3_MAC_RX_FCS_STRIP_B		19
#define HNS3_MAC_RX_FCS_B		20
#define HNS3_MAC_TX_UNDER_MIN_ERR_B	21
#define HNS3_MAC_TX_OVERSIZE_TRUNCATE_B	22

struct hns3_config_mac_mode_cmd {
	uint32_t txrx_pad_fcs_loop_en;
	uint8_t  rsv[20];
};

#define HNS3_CFG_SPEED_10M		6
#define HNS3_CFG_SPEED_100M		7
#define HNS3_CFG_SPEED_1G		0
#define HNS3_CFG_SPEED_10G		1
#define HNS3_CFG_SPEED_25G		2
#define HNS3_CFG_SPEED_40G		3
#define HNS3_CFG_SPEED_50G		4
#define HNS3_CFG_SPEED_100G		5
#define HNS3_CFG_SPEED_200G		8

#define HNS3_CFG_SPEED_S		0
#define HNS3_CFG_SPEED_M		GENMASK(5, 0)
#define HNS3_CFG_DUPLEX_B		7
#define HNS3_CFG_DUPLEX_M		BIT(HNS3_CFG_DUPLEX_B)

#define HNS3_CFG_MAC_SPEED_CHANGE_EN_B	0

struct hns3_config_mac_speed_dup_cmd {
	uint8_t speed_dup;
	uint8_t mac_change_fec_en;
	uint8_t rsv[22];
};

#define HNS3_TQP_ENABLE_B		0

#define HNS3_MAC_CFG_AN_EN_B		0
#define HNS3_MAC_CFG_AN_INT_EN_B	1
#define HNS3_MAC_CFG_AN_INT_MSK_B	2
#define HNS3_MAC_CFG_AN_INT_CLR_B	3
#define HNS3_MAC_CFG_AN_RST_B		4

#define HNS3_MAC_CFG_AN_EN	BIT(HNS3_MAC_CFG_AN_EN_B)

struct hns3_config_auto_neg_cmd {
	uint32_t  cfg_an_cmd_flag;
	uint8_t   rsv[20];
};

#define HNS3_SFP_INFO_BD0_LEN  20UL
#define HNS3_SFP_INFO_BDX_LEN  24UL

struct hns3_sfp_info_bd0_cmd {
	uint16_t offset;
	uint16_t read_len;
	uint8_t data[HNS3_SFP_INFO_BD0_LEN];
};

struct hns3_sfp_type {
	uint8_t type;
	uint8_t ext_type;
};

/* Bitmap flags in supported_speed */
#define HNS3_FIBER_LINK_SPEED_1G_BIT		BIT(0)
#define HNS3_FIBER_LINK_SPEED_10G_BIT		BIT(1)
#define HNS3_FIBER_LINK_SPEED_25G_BIT		BIT(2)
#define HNS3_FIBER_LINK_SPEED_50G_BIT		BIT(3)
#define HNS3_FIBER_LINK_SPEED_100G_BIT		BIT(4)
#define HNS3_FIBER_LINK_SPEED_40G_BIT		BIT(5)
#define HNS3_FIBER_LINK_SPEED_100M_BIT		BIT(6)
#define HNS3_FIBER_LINK_SPEED_10M_BIT		BIT(7)
#define HNS3_FIBER_LINK_SPEED_200G_BIT		BIT(8)

struct hns3_sfp_info_cmd {
	uint32_t sfp_speed;
	uint8_t query_type; /* 0: sfp speed, 1: active */
	uint8_t active_fec; /* current FEC mode */
	uint8_t autoneg; /* current autoneg state */
	/* 0: not support autoneg, 1: support autoneg */
	uint8_t autoneg_ability;
	uint32_t supported_speed; /* speed supported by current media */
	uint32_t module_type;
	uint8_t rsv1[8];
};

#define HNS3_MAC_CFG_FEC_AUTO_EN_B	0
#define HNS3_MAC_CFG_FEC_MODE_S		1
#define HNS3_MAC_CFG_FEC_MODE_M	GENMASK(3, 1)
#define HNS3_MAC_FEC_OFF		0
#define HNS3_MAC_FEC_BASER		1
#define HNS3_MAC_FEC_RS			2

/* Configure FEC mode, opcode:0x031A */
struct hns3_config_fec_cmd {
	uint8_t fec_mode;
	uint8_t rsv[23];
};

#define HNS3_MAC_MGR_MASK_VLAN_B		BIT(0)
#define HNS3_MAC_MGR_MASK_MAC_B			BIT(1)
#define HNS3_MAC_MGR_MASK_ETHERTYPE_B		BIT(2)
#define HNS3_MAC_ETHERTYPE_LLDP			0x88cc

struct hns3_mac_mgr_tbl_entry_cmd {
	uint8_t   flags;
	uint8_t   resp_code;
	uint16_t  vlan_tag;
	uint32_t  mac_addr_hi32;
	uint16_t  mac_addr_lo16;
	uint16_t  rsv1;
	uint16_t  ethter_type;
	uint16_t  egress_port;
	uint16_t  egress_queue;
	uint8_t   sw_port_id_aware;
	uint8_t   rsv2;
	uint8_t   i_port_bitmap;
	uint8_t   i_port_direction;
	uint8_t   rsv3[2];
};

struct hns3_cfg_com_tqp_queue_cmd {
	uint16_t tqp_id;
	uint16_t stream_id;
	uint8_t enable;
	uint8_t rsv[19];
};

#define HNS3_TQP_MAP_TYPE_PF		0
#define HNS3_TQP_MAP_TYPE_VF		1
#define HNS3_TQP_MAP_TYPE_B		0
#define HNS3_TQP_MAP_EN_B		1

struct hns3_tqp_map_cmd {
	uint16_t tqp_id;        /* Absolute tqp id for in this pf */
	uint8_t tqp_vf;         /* VF id */
	uint8_t tqp_flag;       /* Indicate it's pf or vf tqp */
	uint16_t tqp_vid;       /* Virtual id in this pf/vf */
	uint8_t rsv[18];
};

enum hns3_ring_type {
	HNS3_RING_TYPE_TX,
	HNS3_RING_TYPE_RX
};

enum hns3_int_gl_idx {
	HNS3_RING_GL_RX,
	HNS3_RING_GL_TX,
	HNS3_RING_GL_IMMEDIATE = 3
};

#define HNS3_RING_GL_IDX_S	0
#define HNS3_RING_GL_IDX_M	GENMASK(1, 0)

#define HNS3_VECTOR_ELEMENTS_PER_CMD	10

#define HNS3_INT_TYPE_S		0
#define HNS3_INT_TYPE_M		GENMASK(1, 0)
#define HNS3_TQP_ID_S		2
#define HNS3_TQP_ID_M		GENMASK(12, 2)
#define HNS3_INT_GL_IDX_S	13
#define HNS3_INT_GL_IDX_M	GENMASK(14, 13)
#define HNS3_TQP_INT_ID_L_S	0
#define HNS3_TQP_INT_ID_L_M	GENMASK(7, 0)
#define HNS3_TQP_INT_ID_H_S	8
#define HNS3_TQP_INT_ID_H_M	GENMASK(15, 8)
struct hns3_ctrl_vector_chain_cmd {
	uint8_t int_vector_id;    /* the low order of the interrupt id */
	uint8_t int_cause_num;
	uint16_t tqp_type_and_id[HNS3_VECTOR_ELEMENTS_PER_CMD];
	uint8_t vfid;
	uint8_t int_vector_id_h;  /* the high order of the interrupt id */
};

struct hns3_config_max_frm_size_cmd {
	uint16_t max_frm_size;
	uint8_t min_frm_size;
	uint8_t rsv[21];
};

enum hns3_mac_vlan_tbl_opcode {
	HNS3_MAC_VLAN_ADD,      /* Add new or modify mac_vlan */
	HNS3_MAC_VLAN_UPDATE,   /* Modify other fields of this table */
	HNS3_MAC_VLAN_REMOVE,   /* Remove a entry through mac_vlan key */
	HNS3_MAC_VLAN_LKUP,     /* Lookup a entry through mac_vlan key */
};

enum hns3_mac_vlan_add_resp_code {
	HNS3_ADD_UC_OVERFLOW = 2,  /* ADD failed for UC overflow */
	HNS3_ADD_MC_OVERFLOW,      /* ADD failed for MC overflow */
};

#define HNS3_MC_MAC_VLAN_ADD_DESC_NUM	3

#define HNS3_MAC_VLAN_BIT0_EN_B		0
#define HNS3_MAC_VLAN_BIT1_EN_B		1
#define HNS3_MAC_EPORT_SW_EN_B		12
#define HNS3_MAC_EPORT_TYPE_B		11
#define HNS3_MAC_EPORT_VFID_S		3
#define HNS3_MAC_EPORT_VFID_M		GENMASK(10, 3)
#define HNS3_MAC_EPORT_PFID_S		0
#define HNS3_MAC_EPORT_PFID_M		GENMASK(2, 0)
struct hns3_mac_vlan_tbl_entry_cmd {
	uint8_t	  flags;
	uint8_t   resp_code;
	uint16_t  vlan_tag;
	uint32_t  mac_addr_hi32;
	uint16_t  mac_addr_lo16;
	uint16_t  rsv1;
	uint8_t   entry_type;
	uint8_t   mc_mac_en;
	uint16_t  egress_port;
	uint16_t  egress_queue;
	uint8_t   rsv2[6];
};

#define HNS3_TQP_RESET_B	0
struct hns3_reset_tqp_queue_cmd {
	uint16_t tqp_id;
	uint8_t reset_req;
	uint8_t ready_to_reset;
	uint8_t queue_direction;
	uint8_t rsv[19];
};

#define HNS3_CFG_RESET_MAC_B		3
#define HNS3_CFG_RESET_FUNC_B		7
#define HNS3_CFG_RESET_RCB_B		1
struct hns3_reset_cmd {
	uint8_t mac_func_reset;
	uint8_t fun_reset_vfid;
	uint8_t fun_reset_rcb;
	uint8_t rsv1;
	uint16_t fun_reset_rcb_vqid_start;
	uint16_t fun_reset_rcb_vqid_num;
	uint8_t fun_reset_rcb_return_status;
	uint8_t rsv2[15];
};

#define HNS3_QUERY_DEV_SPECS_BD_NUM		4
struct hns3_dev_specs_0_cmd {
	uint32_t rsv0;
	uint32_t mac_entry_num;
	uint32_t mng_entry_num;
	uint16_t rss_ind_tbl_size;
	uint16_t rss_key_size;
	uint16_t intr_ql_max;
	uint8_t max_non_tso_bd_num;
	uint8_t rsv1;
	uint32_t max_tm_rate;
};

struct hns3_query_rpu_cmd {
	uint32_t tc_queue_num;
	uint32_t rsv1[2];
	uint32_t rpu_rx_pkt_drop_cnt;
	uint32_t rsv2[2];
};

#define HNS3_OPC_SSU_DROP_REG_NUM 2

struct hns3_query_ssu_cmd {
	uint8_t rxtx;
	uint8_t rsv[3];
	uint32_t full_drop_cnt;
	uint32_t part_drop_cnt;
	uint32_t oq_drop_cnt;
	uint32_t rev1[2];
};

#define HNS3_PTP_ENABLE_B               0
#define HNS3_PTP_TX_ENABLE_B            1
#define HNS3_PTP_RX_ENABLE_B            2

#define HNS3_PTP_TYPE_S                 0
#define HNS3_PTP_TYPE_M                (0x3 << HNS3_PTP_TYPE_S)

#define ALL_PTP_V2_TYPE                 0xF
#define HNS3_PTP_MESSAGE_TYPE_S         0
#define HNS3_PTP_MESSAGE_TYPE_M        (0xF << HNS3_PTP_MESSAGE_TYPE_S)

#define PTP_TYPE_L2_V2_TYPE             0

struct hns3_ptp_mode_cfg_cmd {
	uint8_t enable;
	uint8_t ptp_type;
	uint8_t v2_message_type_1;
	uint8_t v2_message_type_0;
	uint8_t rsv[20];
};

struct hns3_ptp_int_cmd {
	uint8_t int_en;
	uint8_t rsvd[23];
};

#define HNS3_MAX_TQP_NUM_HIP08_PF	64
#define HNS3_DEFAULT_TX_BUF		0x4000    /* 16k  bytes */
#define HNS3_TOTAL_PKT_BUF		0x108000  /* 1.03125M bytes */
#define HNS3_DEFAULT_DV			0xA000    /* 40k byte */
#define HNS3_DEFAULT_NON_DCB_DV		0x7800    /* 30K byte */
#define HNS3_NON_DCB_ADDITIONAL_BUF	0x1400    /* 5120 byte */

#define HNS3_TYPE_CRQ			0
#define HNS3_TYPE_CSQ			1

#define HNS3_NIC_SW_RST_RDY_B		16
#define HNS3_NIC_SW_RST_RDY			BIT(HNS3_NIC_SW_RST_RDY_B)
#define HNS3_NIC_CMQ_DESC_NUM		1024
#define HNS3_NIC_CMQ_DESC_NUM_S		3

#define HNS3_CMD_SEND_SYNC(flag) \
	((flag) & HNS3_CMD_FLAG_NO_INTR)

void hns3_cmd_reuse_desc(struct hns3_cmd_desc *desc, bool is_read);
void hns3_cmd_setup_basic_desc(struct hns3_cmd_desc *desc,
				enum hns3_opcode_type opcode, bool is_read);
int hns3_cmd_send(struct hns3_hw *hw, struct hns3_cmd_desc *desc, int num);
int hns3_cmd_init_queue(struct hns3_hw *hw);
int hns3_cmd_init(struct hns3_hw *hw);
void hns3_cmd_destroy_queue(struct hns3_hw *hw);
void hns3_cmd_uninit(struct hns3_hw *hw);

#endif /* _HNS3_CMD_H_ */
