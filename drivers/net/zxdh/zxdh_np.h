/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 ZTE Corporation
 */

#ifndef ZXDH_NP_H
#define ZXDH_NP_H

#include <stdint.h>
#include <rte_spinlock.h>

#define ZXDH_OK                               (0)
#define ZXDH_ERR                              (1)
#define ZXDH_DISABLE                          (0)
#define ZXDH_ENABLE                           (1)
#define ZXDH_PORT_NAME_MAX                    (32)
#define ZXDH_DEV_CHANNEL_MAX                  (2)
#define ZXDH_DEV_SDT_ID_MAX                   (256U)


/*DTB*/
#define ZXDH_DTB_QUEUE_ITEM_NUM_MAX           (32)
#define ZXDH_DTB_QUEUE_NUM_MAX                (128)

#define ZXDH_PPU_CLS_ALL_START                (0x3F)
#define ZXDH_PPU_CLUSTER_NUM                  (6)
#define ZXDH_PPU_INSTR_MEM_NUM                (3)
#define ZXDH_SDT_CFG_LEN                      (2)
#define ZXDH_SDT_H_TBL_TYPE_BT_POS            (29)
#define ZXDH_SDT_H_TBL_TYPE_BT_LEN            (3)

#define ZXDH_RC_DEV_BASE                      (0x600)
#define ZXDH_RC_DEV_PARA_INVALID              (ZXDH_RC_DEV_BASE | 0x0)
#define ZXDH_RC_DEV_RANGE_INVALID             (ZXDH_RC_DEV_BASE | 0x1)
#define ZXDH_RC_DEV_CALL_FUNC_FAIL            (ZXDH_RC_DEV_BASE | 0x2)
#define ZXDH_RC_DEV_TYPE_INVALID              (ZXDH_RC_DEV_BASE | 0x3)
#define ZXDH_RC_DEV_CONNECT_FAIL              (ZXDH_RC_DEV_BASE | 0x4)
#define ZXDH_RC_DEV_MSG_INVALID               (ZXDH_RC_DEV_BASE | 0x5)
#define ZXDH_RC_DEV_NOT_EXIST                 (ZXDH_RC_DEV_BASE | 0x6)
#define ZXDH_RC_DEV_MGR_NOT_INIT              (ZXDH_RC_DEV_BASE | 0x7)
#define ZXDH_RC_DEV_CFG_NOT_INIT              (ZXDH_RC_DEV_BASE | 0x8)

#define ZXDH_SYS_VF_NP_BASE_OFFSET      0
#define ZXDH_PCIE_DTB4K_ADDR_OFFSET     (0x6000)
#define ZXDH_PCIE_NP_MEM_SIZE           (0x2000000)
#define ZXDH_PCIE_AGENT_ADDR_OFFSET     (0x2000)

#define ZXDH_INIT_FLAG_ACCESS_TYPE      (1 << 0)
#define ZXDH_INIT_FLAG_SERDES_DOWN_TP   (1 << 1)
#define ZXDH_INIT_FLAG_DDR_BACKDOOR     (1 << 2)
#define ZXDH_INIT_FLAG_SA_MODE          (1 << 3)
#define ZXDH_INIT_FLAG_SA_MESH          (1 << 4)
#define ZXDH_INIT_FLAG_SA_SERDES_MODE   (1 << 5)
#define ZXDH_INIT_FLAG_INT_DEST_MODE    (1 << 6)
#define ZXDH_INIT_FLAG_LIF0_MODE        (1 << 7)
#define ZXDH_INIT_FLAG_DMA_ENABLE       (1 << 8)
#define ZXDH_INIT_FLAG_TM_IMEM_FLAG     (1 << 9)
#define ZXDH_INIT_FLAG_AGENT_FLAG       (1 << 10)

#define ZXDH_REG_NUL_ARRAY              (0 << 0)
#define ZXDH_REG_UNI_ARRAY              (1 << 0)
#define ZXDH_REG_BIN_ARRAY              (1 << 1)
#define ZXDH_REG_FLAG_INDIRECT          (1 << 0)
#define ZXDH_REG_FLAG_DIRECT            (0 << 0)
#define ZXDH_FIELD_FLAG_RO              (1 << 0)
#define ZXDH_FIELD_FLAG_RW              (1 << 1)

#define ZXDH_SYS_NP_BASE_ADDR0          (0x00000000)
#define ZXDH_SYS_NP_BASE_ADDR1          (0x02000000)

#define ZXDH_ACL_TBL_ID_MIN             (0)
#define ZXDH_ACL_TBL_ID_MAX             (7)
#define ZXDH_ACL_TBL_ID_NUM             (8U)
#define ZXDH_ACL_BLOCK_NUM              (8U)

#define ZXDH_RD_CNT_MAX                          (100)
#define ZXDH_SMMU0_READ_REG_MAX_NUM              (4)

#define ZXDH_DTB_ITEM_ACK_SIZE                   (16)
#define ZXDH_DTB_ITEM_BUFF_SIZE                  (16 * 1024)
#define ZXDH_DTB_ITEM_SIZE                       (16 + 16 * 1024)
#define ZXDH_DTB_TAB_UP_SIZE                     ((16 + 16 * 1024) * 32)
#define ZXDH_DTB_TAB_DOWN_SIZE                   ((16 + 16 * 1024) * 32)

#define ZXDH_DTB_TAB_UP_ACK_VLD_MASK             (0x555555)
#define ZXDH_DTB_TAB_DOWN_ACK_VLD_MASK           (0x5a5a5a)
#define ZXDH_DTB_TAB_ACK_IS_USING_MASK           (0x11111100)
#define ZXDH_DTB_TAB_ACK_UNUSED_MASK             (0x0)
#define ZXDH_DTB_TAB_ACK_SUCCESS_MASK            (0xff)
#define ZXDH_DTB_TAB_ACK_FAILED_MASK             (0x1)
#define ZXDH_DTB_TAB_ACK_CHECK_VALUE             (0x12345678)

#define ZXDH_DTB_TAB_ACK_VLD_SHIFT               (104)
#define ZXDH_DTB_TAB_ACK_STATUS_SHIFT            (96)
#define ZXDH_DTB_LEN_POS_SETP                    (16)
#define ZXDH_DTB_ITEM_ADD_OR_UPDATE              (0)
#define ZXDH_DTB_ITEM_DELETE                     (1)

#define ZXDH_ETCAM_LEN_SIZE            (6)
#define ZXDH_ETCAM_BLOCK_NUM           (8)
#define ZXDH_ETCAM_TBLID_NUM           (8)
#define ZXDH_ETCAM_RAM_NUM             (8)
#define ZXDH_ETCAM_RAM_WIDTH           (80U)
#define ZXDH_ETCAM_WR_MASK_MAX         (((uint32_t)1 << ZXDH_ETCAM_RAM_NUM) - 1)
#define ZXDH_ETCAM_WIDTH_MIN           (ZXDH_ETCAM_RAM_WIDTH)
#define ZXDH_ETCAM_WIDTH_MAX           (ZXDH_ETCAM_RAM_NUM * ZXDH_ETCAM_RAM_WIDTH)

#define ZXDH_DTB_TABLE_DATA_BUFF_SIZE           (16384)
#define ZXDH_DTB_TABLE_CMD_SIZE_BIT             (128)

#define ZXDH_SE_SMMU0_ERAM_BLOCK_NUM            (32)
#define ZXDH_SE_SMMU0_ERAM_ADDR_NUM_PER_BLOCK   (0x4000)
#define ZXDH_SE_SMMU0_ERAM_ADDR_NUM_TOTAL  \
		(ZXDH_SE_SMMU0_ERAM_BLOCK_NUM * ZXDH_SE_SMMU0_ERAM_ADDR_NUM_PER_BLOCK)

#define ZXDH_NPSDK_COMPAT_ITEM_ID               (10)
#define ZXDH_DPU_NO_DEBUG_PF_COMPAT_REG_OFFSET  (0x5400)

#define ZXDH_SDT_CFG_LEN                        (2)
#define ZXDH_SDT_VALID                          (1)
#define ZXDH_SDT_INVALID                        (0)
#define ZXDH_SDT_OPER_ADD                       (0)
#define ZXDH_SDT_H_TBL_TYPE_BT_POS              (29)
#define ZXDH_SDT_H_TBL_TYPE_BT_LEN              (3)
#define ZXDH_SDT_H_ERAM_MODE_BT_POS             (26)
#define ZXDH_SDT_H_ERAM_MODE_BT_LEN             (3)
#define ZXDH_SDT_H_ERAM_BASE_ADDR_BT_POS        (7)
#define ZXDH_SDT_H_ERAM_BASE_ADDR_BT_LEN        (19)
#define ZXDH_SDT_L_ERAM_TABLE_DEPTH_BT_POS      (1)
#define ZXDH_SDT_L_ERAM_TABLE_DEPTH_BT_LEN      (22)
#define ZXDH_SDT_H_HASH_ID_BT_POS               (27)
#define ZXDH_SDT_H_HASH_ID_BT_LEN               (2)
#define ZXDH_SDT_H_HASH_TABLE_WIDTH_BT_POS      (25)
#define ZXDH_SDT_H_HASH_TABLE_WIDTH_BT_LEN      (2)
#define ZXDH_SDT_H_HASH_KEY_SIZE_BT_POS         (19)
#define ZXDH_SDT_H_HASH_KEY_SIZE_BT_LEN         (6)
#define ZXDH_SDT_H_HASH_TABLE_ID_BT_POS         (14)
#define ZXDH_SDT_H_HASH_TABLE_ID_BT_LEN         (5)
#define ZXDH_SDT_H_LEARN_EN_BT_POS              (13)
#define ZXDH_SDT_H_LEARN_EN_BT_LEN              (1)
#define ZXDH_SDT_H_KEEP_ALIVE_BT_POS            (12)
#define ZXDH_SDT_H_KEEP_ALIVE_BT_LEN            (1)
#define ZXDH_SDT_H_KEEP_ALIVE_BADDR_BT_POS      (0)
#define ZXDH_SDT_H_KEEP_ALIVE_BADDR_BT_LEN      (12)
#define ZXDH_SDT_L_KEEP_ALIVE_BADDR_BT_POS      (25)
#define ZXDH_SDT_L_KEEP_ALIVE_BADDR_BT_LEN      (7)
#define ZXDH_SDT_L_RSP_MODE_BT_POS              (23)
#define ZXDH_SDT_L_RSP_MODE_BT_LEN              (2)
#define ZXDH_SDT_H_ETCAM_ID_BT_POS              (27)
#define ZXDH_SDT_H_ETCAM_ID_BT_LEN              (1)
#define ZXDH_SDT_H_ETCAM_KEY_MODE_BT_POS        (25)
#define ZXDH_SDT_H_ETCAM_KEY_MODE_BT_LEN        (2)
#define ZXDH_SDT_H_ETCAM_TABLE_ID_BT_POS        (21)
#define ZXDH_SDT_H_ETCAM_TABLE_ID_BT_LEN        (4)
#define ZXDH_SDT_H_ETCAM_NOAS_RSP_MODE_BT_POS   (19)
#define ZXDH_SDT_H_ETCAM_NOAS_RSP_MODE_BT_LEN   (2)
#define ZXDH_SDT_H_ETCAM_AS_EN_BT_POS           (18)
#define ZXDH_SDT_H_ETCAM_AS_EN_BT_LEN           (1)
#define ZXDH_SDT_H_ETCAM_AS_ERAM_BADDR_BT_POS   (0)
#define ZXDH_SDT_H_ETCAM_AS_ERAM_BADDR_BT_LEN   (18)
#define ZXDH_SDT_L_ETCAM_AS_ERAM_BADDR_BT_POS   (31)
#define ZXDH_SDT_L_ETCAM_AS_ERAM_BADDR_BT_LEN   (1)
#define ZXDH_SDT_L_ETCAM_AS_RSP_MODE_BT_POS     (28)
#define ZXDH_SDT_L_ETCAM_AS_RSP_MODE_BT_LEN     (3)
#define ZXDH_SDT_L_ETCAM_TABLE_DEPTH_BT_POS     (1)
#define ZXDH_SDT_L_ETCAM_TABLE_DEPTH_BT_LEN     (20)
#define ZXDH_SDT_L_CLUTCH_EN_BT_POS             (0)
#define ZXDH_SDT_L_CLUTCH_EN_BT_LEN             (1)

/**errco code */
#define ZXDH_RC_BASE                            (0x1000U)
#define ZXDH_PARAMETER_CHK_BASE                 (ZXDH_RC_BASE            | 0x200)
#define ZXDH_PAR_CHK_POINT_NULL                 (ZXDH_PARAMETER_CHK_BASE | 0x001)
#define ZXDH_PAR_CHK_ARGIN_ZERO                 (ZXDH_PARAMETER_CHK_BASE | 0x002)
#define ZXDH_PAR_CHK_ARGIN_OVERFLOW             (ZXDH_PARAMETER_CHK_BASE | 0x003)
#define ZXDH_PAR_CHK_ARGIN_ERROR                (ZXDH_PARAMETER_CHK_BASE | 0x004)
#define ZXDH_PAR_CHK_INVALID_INDEX              (ZXDH_PARAMETER_CHK_BASE | 0x005)
#define ZXDH_PAR_CHK_INVALID_RANGE              (ZXDH_PARAMETER_CHK_BASE | 0x006)
#define ZXDH_PAR_CHK_INVALID_DEV_ID             (ZXDH_PARAMETER_CHK_BASE | 0x007)
#define ZXDH_PAR_CHK_INVALID_PARA               (ZXDH_PARAMETER_CHK_BASE | 0x008)

#define ZXDH_ERAM128_BADDR_MASK                 (0x3FFFF80)

#define ZXDH_DTB_TABLE_MODE_ERAM                (0)
#define ZXDH_DTB_TABLE_MODE_DDR                 (1)
#define ZXDH_DTB_TABLE_MODE_ZCAM                (2)
#define ZXDH_DTB_TABLE_MODE_ETCAM               (3)
#define ZXDH_DTB_TABLE_MODE_MC_HASH             (4)
#define ZXDH_DTB_TABLE_VALID                    (1)

/* DTB module error code */
#define ZXDH_RC_DTB_BASE                        (0xd00)
#define ZXDH_RC_DTB_MGR_EXIST                   (ZXDH_RC_DTB_BASE | 0x0)
#define ZXDH_RC_DTB_MGR_NOT_EXIST               (ZXDH_RC_DTB_BASE | 0x1)
#define ZXDH_RC_DTB_QUEUE_RES_EMPTY             (ZXDH_RC_DTB_BASE | 0x2)
#define ZXDH_RC_DTB_QUEUE_BUFF_SIZE_ERR         (ZXDH_RC_DTB_BASE | 0x3)
#define ZXDH_RC_DTB_QUEUE_ITEM_HW_EMPTY         (ZXDH_RC_DTB_BASE | 0x4)
#define ZXDH_RC_DTB_QUEUE_ITEM_SW_EMPTY         (ZXDH_RC_DTB_BASE | 0x5)
#define ZXDH_RC_DTB_TAB_UP_BUFF_EMPTY           (ZXDH_RC_DTB_BASE | 0x6)
#define ZXDH_RC_DTB_TAB_DOWN_BUFF_EMPTY         (ZXDH_RC_DTB_BASE | 0x7)
#define ZXDH_RC_DTB_TAB_UP_TRANS_ERR            (ZXDH_RC_DTB_BASE | 0x8)
#define ZXDH_RC_DTB_TAB_DOWN_TRANS_ERR          (ZXDH_RC_DTB_BASE | 0x9)
#define ZXDH_RC_DTB_QUEUE_IS_WORKING            (ZXDH_RC_DTB_BASE | 0xa)
#define ZXDH_RC_DTB_QUEUE_IS_NOT_INIT           (ZXDH_RC_DTB_BASE | 0xb)
#define ZXDH_RC_DTB_MEMORY_ALLOC_ERR            (ZXDH_RC_DTB_BASE | 0xc)
#define ZXDH_RC_DTB_PARA_INVALID                (ZXDH_RC_DTB_BASE | 0xd)
#define ZXDH_RC_DMA_RANGE_INVALID               (ZXDH_RC_DTB_BASE | 0xe)
#define ZXDH_RC_DMA_RCV_DATA_EMPTY              (ZXDH_RC_DTB_BASE | 0xf)
#define ZXDH_RC_DTB_LPM_INSERT_FAIL             (ZXDH_RC_DTB_BASE | 0x10)
#define ZXDH_RC_DTB_LPM_DELETE_FAIL             (ZXDH_RC_DTB_BASE | 0x11)
#define ZXDH_RC_DTB_DOWN_LEN_INVALID            (ZXDH_RC_DTB_BASE | 0x12)
#define ZXDH_RC_DTB_DOWN_HASH_CONFLICT          (ZXDH_RC_DTB_BASE | 0x13)
#define ZXDH_RC_DTB_QUEUE_NOT_ALLOC             (ZXDH_RC_DTB_BASE | 0x14)
#define ZXDH_RC_DTB_QUEUE_NAME_ERROR            (ZXDH_RC_DTB_BASE | 0x15)
#define ZXDH_RC_DTB_DUMP_SIZE_SMALL             (ZXDH_RC_DTB_BASE | 0x16)
#define ZXDH_RC_DTB_SEARCH_VPORT_QUEUE_ZERO     (ZXDH_RC_DTB_BASE | 0x17)
#define ZXDH_RC_DTB_QUEUE_NOT_ENABLE            (ZXDH_RC_DTB_BASE | 0x18)

#define ZXDH_SCHE_RSP_LEN                       (2)
#define ZXDH_G_PROFILE_ID_LEN                   (8)

#define ZXDH_CAR_A_FLOW_ID_MAX                  (0x7fff)
#define ZXDH_CAR_B_FLOW_ID_MAX                  (0xfff)
#define ZXDH_CAR_C_FLOW_ID_MAX                  (0x3ff)
#define ZXDH_CAR_A_PROFILE_ID_MAX               (0x1ff)
#define ZXDH_CAR_B_PROFILE_ID_MAX               (0x7f)
#define ZXDH_CAR_C_PROFILE_ID_MAX               (0x1f)

#define ZXDH_SYS_NP_BASE_ADDR0          (0x00000000)
#define ZXDH_SYS_NP_BASE_ADDR1          (0x02000000)

#define ZXDH_FIELD_FLAG_RO              (1 << 0)
#define ZXDH_FIELD_FLAG_RW              (1 << 1)

#define ZXDH_VF_ACTIVE(VPORT)                   (((VPORT) & 0x0800) >> 11)
#define ZXDH_EPID_BY(VPORT)                     (((VPORT) & 0x7000) >> 12)
#define ZXDH_FUNC_NUM(VPORT)                    (((VPORT) & 0x0700) >> 8)
#define ZXDH_VFUNC_NUM(VPORT)                   (((VPORT) & 0x00FF))
#define ZXDH_IS_PF(VPORT)                       (!ZXDH_VF_ACTIVE(VPORT))

#define ZXDH_CHANNEL_REPS_LEN                   (4)

typedef enum zxdh_module_base_addr_e {
	ZXDH_MODULE_SE_SMMU0_BASE_ADDR = 0x00000000,
	ZXDH_MODULE_DTB_ENQ_BASE_ADDR  = 0x00000000,
	ZXDH_MODULE_NPPU_PKTRX_CFG_BASE_ADDR = 0x00000800,
} ZXDH_MODULE_BASE_ADDR_E;

typedef enum zxdh_sys_base_addr_e {
	ZXDH_SYS_NPPU_BASE_ADDR     = (ZXDH_SYS_NP_BASE_ADDR0 + 0x00000000),
	ZXDH_SYS_SE_SMMU0_BASE_ADDR = (ZXDH_SYS_NP_BASE_ADDR0 + 0x00300000),
	ZXDH_SYS_DTB_BASE_ADDR      = (ZXDH_SYS_NP_BASE_ADDR1 + 0x00000000),
	ZXDH_SYS_MAX_BASE_ADDR      = 0x20000000,
} ZXDH_SYS_BASE_ADDR_E;


typedef enum zxdh_module_init_e {
	ZXDH_MODULE_INIT_NPPU = 0,
	ZXDH_MODULE_INIT_PPU,
	ZXDH_MODULE_INIT_SE,
	ZXDH_MODULE_INIT_ETM,
	ZXDH_MODULE_INIT_DLB,
	ZXDH_MODULE_INIT_TRPG,
	ZXDH_MODULE_INIT_TSN,
	ZXDH_MODULE_INIT_MAX
} ZXDH_MODULE_INIT_E;

typedef enum zxdh_dev_type_e {
	ZXDH_DEV_TYPE_SIM  = 0,
	ZXDH_DEV_TYPE_VCS  = 1,
	ZXDH_DEV_TYPE_CHIP = 2,
	ZXDH_DEV_TYPE_FPGA = 3,
	ZXDH_DEV_TYPE_PCIE_ACC = 4,
	ZXDH_DEV_TYPE_INVALID,
} ZXDH_DEV_TYPE_E;

typedef enum zxdh_reg_info_e {
	ZXDH_SMMU0_SMMU0_CPU_IND_CMDR        = 0,
	ZXDH_SMMU0_SMMU0_CPU_IND_RD_DONER    = 1,
	ZXDH_SMMU0_SMMU0_CPU_IND_RDAT0R      = 2,
	ZXDH_SMMU0_SMMU0_CPU_IND_RDAT1R      = 3,
	ZXDH_SMMU0_SMMU0_CPU_IND_RDAT2R      = 4,
	ZXDH_SMMU0_SMMU0_CPU_IND_RDAT3R      = 5,
	ZXDH_SMMU0_SMMU0_WR_ARB_CPU_RDYR     = 6,
	ZXDH_DTB_INFO_QUEUE_BUF_SPACE        = 7,
	ZXDH_DTB_CFG_EPID_V_FUNC_NUM         = 8,
	ZXDH_STAT_CAR0_CARA_QUEUE_RAM0       = 9,
	ZXDH_STAT_CAR0_CARB_QUEUE_RAM0       = 10,
	ZXDH_STAT_CAR0_CARC_QUEUE_RAM0       = 11,
	ZXDH_NPPU_PKTRX_CFG_GLBAL_CFG_0R     = 12,
	ZXDH_REG_ENUM_MAX_VALUE
} ZXDH_REG_INFO_E;

typedef enum zxdh_dev_spinlock_type_e {
	ZXDH_DEV_SPINLOCK_T_SMMU0     = 0,
	ZXDH_DEV_SPINLOCK_T_DTB       = 1,
	ZXDH_DEV_SPINLOCK_T_MAX
} ZXDH_DEV_SPINLOCK_TYPE_E;

typedef enum zxdh_dev_access_type_e {
	ZXDH_DEV_ACCESS_TYPE_PCIE = 0,
	ZXDH_DEV_ACCESS_TYPE_RISCV = 1,
	ZXDH_DEV_ACCESS_TYPE_INVALID,
} ZXDH_DEV_ACCESS_TYPE_E;

typedef enum zxdh_dev_agent_flag_e {
	ZXDH_DEV_AGENT_DISABLE = 0,
	ZXDH_DEV_AGENT_ENABLE = 1,
	ZXDH_DEV_AGENT_INVALID,
} ZXDH_DEV_AGENT_FLAG_E;

typedef enum zxdh_acl_pri_mode_e {
	ZXDH_ACL_PRI_EXPLICIT = 1,
	ZXDH_ACL_PRI_IMPLICIT,
	ZXDH_ACL_PRI_SPECIFY,
	ZXDH_ACL_PRI_INVALID,
} ZXDH_ACL_PRI_MODE_E;

typedef enum zxdh_module_e {
	CFG = 1,
	NPPU,
	PPU,
	ETM,
	STAT,
	CAR,
	SE,
	SMMU0 = SE,
	SMMU1 = SE,
	DTB,
	TRPG,
	TSN,
	AXI,
	PTPTM,
	DTB4K,
	STAT4K,
	PPU4K,
	SE4K,
	SMMU14K,
	MODULE_MAX
} ZXDH_MODULE_E;

typedef struct zxdh_d_node {
	void *data;
	struct zxdh_d_node *prev;
	struct zxdh_d_node *next;
} ZXDH_D_NODE;

typedef struct zxdh_d_head {
	uint32_t  used;
	uint32_t  maxnum;
	ZXDH_D_NODE *p_next;
	ZXDH_D_NODE *p_prev;
} ZXDH_D_HEAD;

typedef struct zxdh_dtb_tab_up_user_addr_t {
	uint32_t user_flag;
	uint64_t phy_addr;
	uint64_t vir_addr;
} ZXDH_DTB_TAB_UP_USER_ADDR_T;

typedef struct zxdh_dtb_tab_up_info_t {
	uint64_t start_phy_addr;
	uint64_t start_vir_addr;
	uint32_t item_size;
	uint32_t wr_index;
	uint32_t rd_index;
	uint32_t data_len[ZXDH_DTB_QUEUE_ITEM_NUM_MAX];
	ZXDH_DTB_TAB_UP_USER_ADDR_T user_addr[ZXDH_DTB_QUEUE_ITEM_NUM_MAX];
} ZXDH_DTB_TAB_UP_INFO_T;

typedef struct zxdh_dtb_tab_down_info_t {
	uint64_t start_phy_addr;
	uint64_t start_vir_addr;
	uint32_t item_size;
	uint32_t wr_index;
	uint32_t rd_index;
} ZXDH_DTB_TAB_DOWN_INFO_T;

typedef struct zxdh_dtb_queue_info_t {
	uint32_t init_flag;
	uint32_t vport;
	uint32_t vector;
	ZXDH_DTB_TAB_UP_INFO_T tab_up;
	ZXDH_DTB_TAB_DOWN_INFO_T tab_down;
} ZXDH_DTB_QUEUE_INFO_T;

typedef struct zxdh_dtb_mgr_t {
	ZXDH_DTB_QUEUE_INFO_T queue_info[ZXDH_DTB_QUEUE_NUM_MAX];
} ZXDH_DTB_MGR_T;

typedef struct zxdh_ppu_cls_bitmap_t {
	uint32_t cls_use[ZXDH_PPU_CLUSTER_NUM];
	uint32_t instr_mem[ZXDH_PPU_INSTR_MEM_NUM];
} ZXDH_PPU_CLS_BITMAP_T;

typedef struct dpp_sdt_item_t {
	uint32_t     valid;
	uint32_t     table_cfg[ZXDH_SDT_CFG_LEN];
} ZXDH_SDT_ITEM_T;

typedef struct dpp_sdt_soft_table_t {
	uint32_t          device_id;
	ZXDH_SDT_ITEM_T  sdt_array[ZXDH_DEV_SDT_ID_MAX];
} ZXDH_SDT_SOFT_TABLE_T;

typedef struct zxdh_sdt_tbl_eram_t {
	uint32_t table_type;
	uint32_t eram_mode;
	uint32_t eram_base_addr;
	uint32_t eram_table_depth;
	uint32_t eram_clutch_en;
} ZXDH_SDT_TBL_ERAM_T;

typedef struct zxdh_sdt_tbl_etcam_t {
	uint32_t table_type;
	uint32_t etcam_id;
	uint32_t etcam_key_mode;
	uint32_t etcam_table_id;
	uint32_t no_as_rsp_mode;
	uint32_t as_en;
	uint32_t as_eram_baddr;
	uint32_t as_rsp_mode;
	uint32_t etcam_table_depth;
	uint32_t etcam_clutch_en;
} ZXDH_SDT_TBL_ETCAM_T;

typedef struct zxdh_sdt_tbl_hash_t {
	uint32_t table_type;
	uint32_t hash_id;
	uint32_t hash_table_width;
	uint32_t key_size;
	uint32_t hash_table_id;
	uint32_t learn_en;
	uint32_t keep_alive;
	uint32_t keep_alive_baddr;
	uint32_t rsp_mode;
	uint32_t hash_clutch_en;
} ZXDH_SDT_TBL_HASH_T;

typedef struct zxdh_spin_lock_t {
	rte_spinlock_t spinlock;
} ZXDH_SPINLOCK_T;

typedef void (*ZXDH_DEV_WRITE_FUNC)(uint32_t dev_id,
		uint32_t addr, uint32_t size, uint32_t *p_data);
typedef void (*ZXDH_DEV_READ_FUNC)(uint32_t dev_id,
		uint32_t addr, uint32_t size, uint32_t *p_data);

typedef struct zxdh_sys_init_ctrl_t {
	ZXDH_DEV_TYPE_E device_type;
	uint32_t flags;
	uint32_t sa_id;
	uint32_t case_num;
	uint32_t lif0_port_type;
	uint32_t lif1_port_type;
	uint64_t pcie_vir_baddr;
	uint64_t riscv_vir_baddr;
	uint64_t dma_vir_baddr;
	uint64_t dma_phy_baddr;
} ZXDH_SYS_INIT_CTRL_T;

typedef struct dpp_dev_cfg_t {
	uint32_t device_id;
	ZXDH_DEV_TYPE_E dev_type;
	uint32_t chip_ver;
	uint32_t access_type;
	uint32_t agent_flag;
	uint32_t vport;
	uint64_t pcie_addr;
	uint64_t riscv_addr;
	uint64_t dma_vir_addr;
	uint64_t dma_phy_addr;
	uint64_t agent_addr;
	uint32_t init_flags[ZXDH_MODULE_INIT_MAX];
	ZXDH_DEV_WRITE_FUNC p_pcie_write_fun;
	ZXDH_DEV_READ_FUNC  p_pcie_read_fun;
} ZXDH_DEV_CFG_T;

typedef struct zxdh_dev_mngr_t {
	uint32_t         device_num;
	uint32_t         is_init;
	ZXDH_DEV_CFG_T       *p_dev_array[ZXDH_DEV_CHANNEL_MAX];
} ZXDH_DEV_MGR_T;

typedef struct zxdh_dtb_addr_info_t {
	uint32_t sdt_no;
	uint32_t size;
	uint64_t phy_addr;
	uint64_t vir_addr;
} ZXDH_DTB_ADDR_INFO_T;

typedef struct zxdh_dev_init_ctrl_t {
	uint32_t vport;
	char  port_name[ZXDH_PORT_NAME_MAX];
	uint32_t vector;
	uint32_t queue_id;
	uint32_t np_bar_offset;
	uint32_t np_bar_len;
	uint64_t pcie_vir_addr;
	uint64_t down_phy_addr;
	uint64_t down_vir_addr;
	uint64_t dump_phy_addr;
	uint64_t dump_vir_addr;
	uint64_t dump_sdt_num;
	ZXDH_DTB_ADDR_INFO_T dump_addr_info[];
} ZXDH_DEV_INIT_CTRL_T;

typedef struct zxdh_sdt_mgr_t {
	uint32_t          channel_num;
	uint32_t          is_init;
	ZXDH_SDT_SOFT_TABLE_T *sdt_tbl_array[ZXDH_DEV_CHANNEL_MAX];
} ZXDH_SDT_MGR_T;

typedef struct zxdh_riscv_dtb_queue_USER_info_t {
	uint32_t alloc_flag;
	uint32_t queue_id;
	uint32_t vport;
	char  user_name[ZXDH_PORT_NAME_MAX];
} ZXDH_RISCV_DTB_QUEUE_USER_INFO_T;

typedef struct zxdh_riscv_dtb_mgr {
	uint32_t queue_alloc_count;
	uint32_t queue_index;
	ZXDH_RISCV_DTB_QUEUE_USER_INFO_T queue_user_info[ZXDH_DTB_QUEUE_NUM_MAX];
} ZXDH_RISCV_DTB_MGR;

typedef struct zxdh_dtb_queue_vm_info_t {
	uint32_t dbi_en;
	uint32_t queue_en;
	uint32_t epid;
	uint32_t vfunc_num;
	uint32_t vector;
	uint32_t func_num;
	uint32_t vfunc_active;
} ZXDH_DTB_QUEUE_VM_INFO_T;

typedef struct zxdh_dtb4k_dtb_enq_cfg_epid_v_func_num_0_127_t {
	uint32_t dbi_en;
	uint32_t queue_en;
	uint32_t cfg_epid;
	uint32_t cfg_vfunc_num;
	uint32_t cfg_vector;
	uint32_t cfg_func_num;
	uint32_t cfg_vfunc_active;
} ZXDH_DTB4K_DTB_ENQ_CFG_EPID_V_FUNC_NUM_0_127_T;


typedef uint32_t (*ZXDH_REG_WRITE)(uint32_t dev_id, uint32_t addr, uint32_t *p_data);
typedef uint32_t (*ZXDH_REG_READ)(uint32_t dev_id, uint32_t addr, uint32_t *p_data);

typedef struct zxdh_field_t {
	const char    *p_name;
	uint32_t  flags;
	uint16_t  msb_pos;

	uint16_t  len;
	uint32_t  default_value;
	uint32_t  default_step;
} ZXDH_FIELD_T;

typedef struct zxdh_reg_t {
	const char    *reg_name;
	uint32_t  reg_no;
	uint32_t  module_no;
	uint32_t  flags;
	uint32_t  array_type;
	uint32_t  addr;
	uint32_t  width;
	uint32_t  m_size;
	uint32_t  n_size;
	uint32_t  m_step;
	uint32_t  n_step;
	uint32_t  field_num;
	const ZXDH_FIELD_T *p_fields;

	ZXDH_REG_WRITE      p_write_fun;
	ZXDH_REG_READ       p_read_fun;
} ZXDH_REG_T;

typedef struct zxdh_tlb_mgr_t {
	uint32_t entry_num;
	uint32_t va_width;
	uint32_t pa_width;
} ZXDH_TLB_MGR_T;

typedef enum zxdh_eram128_tbl_mode_e {
	ZXDH_ERAM128_TBL_1b   = 0,
	ZXDH_ERAM128_TBL_32b  = 1,
	ZXDH_ERAM128_TBL_64b  = 2,
	ZXDH_ERAM128_TBL_128b = 3,
	ZXDH_ERAM128_TBL_2b   = 4,
	ZXDH_ERAM128_TBL_4b   = 5,
	ZXDH_ERAM128_TBL_8b   = 6,
	ZXDH_ERAM128_TBL_16b  = 7
} ZXDH_ERAM128_TBL_MODE_E;

typedef enum zxdh_eram128_opr_mode_e {
	ZXDH_ERAM128_OPR_128b = 0,
	ZXDH_ERAM128_OPR_64b  = 1,
	ZXDH_ERAM128_OPR_1b   = 2,
	ZXDH_ERAM128_OPR_32b  = 3

} ZXDH_ERAM128_OPR_MODE_E;

typedef enum zxdh_dtb_table_info_e {
	ZXDH_DTB_TABLE_DDR           = 0,
	ZXDH_DTB_TABLE_ERAM_1        = 1,
	ZXDH_DTB_TABLE_ERAM_64       = 2,
	ZXDH_DTB_TABLE_ERAM_128      = 3,
	ZXDH_DTB_TABLE_ZCAM          = 4,
	ZXDH_DTB_TABLE_ETCAM         = 5,
	ZXDH_DTB_TABLE_MC_HASH       = 6,
	ZXDH_DTB_TABLE_ENUM_MAX
} ZXDH_DTB_TABLE_INFO_E;

typedef enum zxdh_sdt_table_type_e {
	ZXDH_SDT_TBLT_INVALID = 0,
	ZXDH_SDT_TBLT_ERAM    = 1,
	ZXDH_SDT_TBLT_DDR3    = 2,
	ZXDH_SDT_TBLT_HASH    = 3,
	ZXDH_SDT_TBLT_LPM     = 4,
	ZXDH_SDT_TBLT_ETCAM   = 5,
	ZXDH_SDT_TBLT_PORTTBL = 6,
	ZXDH_SDT_TBLT_MAX     = 7,
} ZXDH_SDT_TABLE_TYPE_E;

typedef enum zxdh_dtb_dir_type_e {
	ZXDH_DTB_DIR_DOWN_TYPE    = 0,
	ZXDH_DTB_DIR_UP_TYPE    = 1,
	ZXDH_DTB_DIR_TYPE_MAX,
} ZXDH_DTB_DIR_TYPE_E;

typedef enum zxdh_dtb_tab_up_user_addr_type_e {
	ZXDH_DTB_TAB_UP_NOUSER_ADDR_TYPE     = 0,
	ZXDH_DTB_TAB_UP_USER_ADDR_TYPE       = 1,
	ZXDH_DTB_TAB_UP_USER_ADDR_TYPE_MAX,
} ZXDH_DTB_TAB_UP_USER_ADDR_TYPE_E;

typedef struct zxdh_dtb_lpm_entry_t {
	uint32_t dtb_len0;
	uint8_t *p_data_buff0;
	uint32_t dtb_len1;
	uint8_t *p_data_buff1;
} ZXDH_DTB_LPM_ENTRY_T;

typedef struct zxdh_dtb_entry_t {
	uint8_t *cmd;
	uint8_t *data;
	uint32_t data_in_cmd_flag;
	uint32_t data_size;
} ZXDH_DTB_ENTRY_T;

typedef struct zxdh_dtb_eram_table_form_t {
	uint32_t valid;
	uint32_t type_mode;
	uint32_t data_mode;
	uint32_t cpu_wr;
	uint32_t cpu_rd;
	uint32_t cpu_rd_mode;
	uint32_t addr;
	uint32_t data_h;
	uint32_t data_l;
} ZXDH_DTB_ERAM_TABLE_FORM_T;

typedef union zxdh_endian_u {
	unsigned int     a;
	unsigned char    b;
} ZXDH_ENDIAN_U;

typedef struct zxdh_dtb_field_t {
	const char    *p_name;
	uint16_t  lsb_pos;
	uint16_t  len;
} ZXDH_DTB_FIELD_T;

typedef struct zxdh_dtb_table_t {
	const char    *table_type;
	uint32_t  table_no;
	uint32_t  field_num;
	const ZXDH_DTB_FIELD_T *p_fields;
} ZXDH_DTB_TABLE_T;

typedef struct zxdh_dtb_queue_item_info_t {
	uint32_t cmd_vld;
	uint32_t cmd_type;
	uint32_t int_en;
	uint32_t data_len;
	uint32_t data_laddr;
	uint32_t data_hddr;
} ZXDH_DTB_QUEUE_ITEM_INFO_T;

typedef struct zxdh_dtb_eram_entry_info_t {
	uint32_t index;
	uint32_t *p_data;
} ZXDH_DTB_ERAM_ENTRY_INFO_T;

typedef struct zxdh_dtb_user_entry_t {
	uint32_t sdt_no;
	void *p_entry_data;
} ZXDH_DTB_USER_ENTRY_T;

typedef struct zxdh_sdt_tbl_data_t {
	uint32_t data_high32;
	uint32_t data_low32;
} ZXDH_SDT_TBL_DATA_T;

typedef struct zxdh_sdt_tbl_porttbl_t {
	uint32_t table_type;
	uint32_t porttbl_clutch_en;
} ZXDH_SDT_TBL_PORTTBL_T;

typedef struct zxdh_dtb_hash_entry_info_t {
	uint8_t *p_actu_key;
	uint8_t *p_rst;
} ZXDH_DTB_HASH_ENTRY_INFO_T;

typedef struct zxdh_ppu_stat_cfg_t {
	uint32_t eram_baddr;
	uint32_t eram_depth;
	uint32_t ddr_base_addr;
	uint32_t ppu_addr_offset;
} ZXDH_PPU_STAT_CFG_T;

typedef enum zxdh_stat_cnt_mode_e {
	ZXDH_STAT_64_MODE  = 0,
	ZXDH_STAT_128_MODE = 1,
	ZXDH_STAT_MAX_MODE,
} ZXDH_STAT_CNT_MODE_E;

typedef struct zxdh_smmu0_smmu0_cpu_ind_cmd_t {
	uint32_t cpu_ind_rw;
	uint32_t cpu_ind_rd_mode;
	uint32_t cpu_req_mode;
	uint32_t cpu_ind_addr;
} ZXDH_SMMU0_SMMU0_CPU_IND_CMD_T;

typedef enum zxdh_stat_rd_clr_mode_e {
	ZXDH_STAT_RD_CLR_MODE_UNCLR = 0,
	ZXDH_STAT_RD_CLR_MODE_CLR   = 1,
	ZXDH_STAT_RD_CLR_MODE_MAX,
} STAT_RD_CLR_MODE_E;

typedef enum zxdh_eram128_rd_clr_mode_e {
	ZXDH_RD_MODE_HOLD   = 0,
	ZXDH_RD_MODE_CLEAR  = 1,
} ZXDH_ERAM128_RD_CLR_MODE_E;

typedef enum zxdh_se_opr_mode_e {
	ZXDH_SE_OPR_WR      = 0,
	ZXDH_SE_OPR_RD      = 1,
} ZXDH_SE_OPR_MODE_E;

typedef enum zxdh_stat_car_type_e {
	ZXDH_STAT_CAR_A_TYPE  = 0,
	ZXDH_STAT_CAR_B_TYPE,
	ZXDH_STAT_CAR_C_TYPE,
	ZXDH_STAT_CAR_MAX_TYPE
} ZXDH_STAT_CAR_TYPE_E;

typedef enum zxdh_car_priority_e {
	ZXDH_CAR_PRI0 = 0,
	ZXDH_CAR_PRI1 = 1,
	ZXDH_CAR_PRI2 = 2,
	ZXDH_CAR_PRI3 = 3,
	ZXDH_CAR_PRI4 = 4,
	ZXDH_CAR_PRI5 = 5,
	ZXDH_CAR_PRI6 = 6,
	ZXDH_CAR_PRI7 = 7,
	ZXDH_CAR_PRI_MAX
} ZXDH_CAR_PRIORITY_E;

typedef struct zxdh_stat_car_pkt_profile_cfg_t {
	uint32_t profile_id;
	uint32_t pkt_sign;
	uint32_t cir;
	uint32_t cbs;
	uint32_t pri[ZXDH_CAR_PRI_MAX];
} ZXDH_STAT_CAR_PKT_PROFILE_CFG_T;

typedef struct zxdh_stat_car_profile_cfg_t {
	uint32_t profile_id;
	uint32_t pkt_sign;
	uint32_t cd;
	uint32_t cf;
	uint32_t cm;
	uint32_t cir;
	uint32_t cbs;
	uint32_t eir;
	uint32_t ebs;
	uint32_t random_disc_e;
	uint32_t random_disc_c;
	uint32_t c_pri[ZXDH_CAR_PRI_MAX];
	uint32_t e_green_pri[ZXDH_CAR_PRI_MAX];
	uint32_t e_yellow_pri[ZXDH_CAR_PRI_MAX];
} ZXDH_STAT_CAR_PROFILE_CFG_T;

typedef struct zxdh_stat_car0_cara_queue_ram0_159_0_t {
	uint32_t cara_drop;
	uint32_t cara_plcr_en;
	uint32_t cara_profile_id;
	uint32_t cara_tq_h;
	uint32_t cara_tq_l;
	uint32_t cara_ted;
	uint32_t cara_tcd;
	uint32_t cara_tei;
	uint32_t cara_tci;
} ZXDH_STAT_CAR0_CARA_QUEUE_RAM0_159_0_T;

typedef struct dpp_agent_car_profile_msg {
	uint8_t dev_id;
	uint8_t type;
	uint8_t rsv;
	uint8_t rsv1;
	uint32_t car_level;
	uint32_t profile_id;
	uint32_t pkt_sign;
	uint32_t cd;
	uint32_t cf;
	uint32_t cm;
	uint32_t cir;
	uint32_t cbs;
	uint32_t eir;
	uint32_t ebs;
	uint32_t random_disc_e;
	uint32_t random_disc_c;
	uint32_t c_pri[ZXDH_CAR_PRI_MAX];
	uint32_t e_green_pri[ZXDH_CAR_PRI_MAX];
	uint32_t e_yellow_pri[ZXDH_CAR_PRI_MAX];
} ZXDH_AGENT_CAR_PROFILE_MSG_T;

typedef struct dpp_agent_car_pkt_profile_msg {
	uint8_t dev_id;
	uint8_t type;
	uint8_t rsv;
	uint8_t rsv1;
	uint32_t car_level;
	uint32_t profile_id;
	uint32_t pkt_sign;
	uint32_t cir;
	uint32_t cbs;
	uint32_t pri[ZXDH_CAR_PRI_MAX];
} ZXDH_AGENT_CAR_PKT_PROFILE_MSG_T;

typedef struct zxdh_agent_channel_plcr_msg {
	uint8_t dev_id;
	uint8_t type;
	uint8_t oper;
	uint8_t rsv;
	uint32_t vport;
	uint32_t car_type;
	uint32_t profile_id;
} ZXDH_AGENT_CHANNEL_PLCR_MSG_T;

typedef struct dpp_stat_car0_carc_queue_ram0_159_0_t {
	uint32_t carc_drop;
	uint32_t carc_plcr_en;
	uint32_t carc_profile_id;
	uint32_t carc_tq_h;
	uint32_t carc_tq_l;
	uint32_t carc_ted;
	uint32_t carc_tcd;
	uint32_t carc_tei;
	uint32_t carc_tci;
} ZXDH_STAT_CAR0_CARC_QUEUE_RAM0_159_0_T;

typedef struct zxdh_stat_car0_carb_queue_ram0_159_0_t {
	uint32_t carb_drop;
	uint32_t carb_plcr_en;
	uint32_t carb_profile_id;
	uint32_t carb_tq_h;
	uint32_t carb_tq_l;
	uint32_t carb_ted;
	uint32_t carb_tcd;
	uint32_t carb_tei;
	uint32_t carb_tci;
} ZXDH_STAT_CAR0_CARB_QUEUE_RAM0_159_0_T;

typedef enum zxdh_np_agent_msg_type_e {
	ZXDH_REG_MSG = 0,
	ZXDH_DTB_MSG,
	ZXDH_TM_MSG,
	ZXDH_PLCR_MSG,
	ZXDH_PKTRX_IND_REG_RW_MSG,
	ZXDH_PCIE_BAR_MSG,
	ZXDH_RESET_MSG,
	ZXDH_PXE_MSG,
	ZXDH_TM_FLOW_SHAPE,
	ZXDH_TM_TD,
	ZXDH_TM_SE_SHAPE,
	ZXDH_TM_PP_SHAPE,
	ZXDH_PLCR_CAR_RATE,
	ZXDH_PLCR_CAR_PKT_RATE,
	ZXDH_PPU_THASH_RSK,
	ZXDH_ACL_MSG,
	ZXDH_STAT_MSG,
	ZXDH_RES_MSG,
	ZXDH_MSG_MAX
} MSG_TYPE_E;

typedef enum zxdh_msg_plcr_oper {
	ZXDH_PROFILEID_REQUEST = 0,
	ZXDH_PROFILEID_RELEASE = 1,
} ZXDH_MSG_PLCR_OPER_E;

typedef enum zxdh_profile_type {
	CAR_A = 0,
	CAR_B = 1,
	CAR_C = 2,
	CAR_MAX
} ZXDH_PROFILE_TYPE;

typedef struct __rte_aligned(2) zxdh_version_compatible_reg_t {
	uint8_t version_compatible_item;
	uint8_t major;
	uint8_t fw_minor;
	uint8_t drv_minor;
	uint16_t patch;
	uint8_t rsv[2];
} ZXDH_VERSION_COMPATIBLE_REG_T;

typedef struct __rte_aligned(2) zxdh_agent_channel_msg_t {
	uint32_t msg_len;
	void *msg;
} ZXDH_AGENT_CHANNEL_MSG_T;

int zxdh_np_host_init(uint32_t dev_id, ZXDH_DEV_INIT_CTRL_T *p_dev_init_ctrl);
int zxdh_np_online_uninit(uint32_t dev_id, char *port_name, uint32_t queue_id);
int zxdh_np_dtb_table_entry_write(uint32_t dev_id, uint32_t queue_id,
			uint32_t entrynum, ZXDH_DTB_USER_ENTRY_T *down_entries);
int zxdh_np_dtb_table_entry_delete(uint32_t dev_id, uint32_t queue_id,
			 uint32_t entrynum, ZXDH_DTB_USER_ENTRY_T *delete_entries);
int zxdh_np_dtb_table_entry_get(uint32_t dev_id, uint32_t queue_id,
			ZXDH_DTB_USER_ENTRY_T *get_entry, uint32_t srh_mode);
int zxdh_np_dtb_stats_get(uint32_t dev_id,
			uint32_t queue_id,
			ZXDH_STAT_CNT_MODE_E rd_mode,
			uint32_t index,
			uint32_t *p_data);
uint32_t zxdh_np_stat_ppu_cnt_get_ex(uint32_t dev_id,
			ZXDH_STAT_CNT_MODE_E rd_mode,
			uint32_t index,
			uint32_t clr_mode,
			uint32_t *p_data);
uint32_t
zxdh_np_car_profile_id_add(uint32_t vport_id,
			uint32_t flags,
			uint64_t *p_profile_id);
uint32_t zxdh_np_car_profile_cfg_set(uint32_t vport_id,
			uint32_t car_type,
			uint32_t pkt_sign,
			uint32_t profile_id,
			void *p_car_profile_cfg);
uint32_t zxdh_np_car_profile_id_delete(uint32_t vport_id,
			uint32_t flags, uint64_t profile_id);
uint32_t zxdh_np_stat_car_queue_cfg_set(uint32_t dev_id,
			uint32_t car_type,
			uint32_t flow_id,
			uint32_t drop_flag,
			uint32_t plcr_en,
			uint32_t profile_id);

#endif /* ZXDH_NP_H */
