/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 ZTE Corporation
 */

#ifndef ZXDH_NP_H
#define ZXDH_NP_H

#include <stdint.h>

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

#define ZXDH_ACL_TBL_ID_MIN             (0)
#define ZXDH_ACL_TBL_ID_MAX             (7)
#define ZXDH_ACL_TBL_ID_NUM             (8U)
#define ZXDH_ACL_BLOCK_NUM              (8U)

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
	ZXDH_DTB_CFG_QUEUE_DTB_HADDR   = 0,
	ZXDH_DTB_CFG_QUEUE_DTB_LADDR   = 1,
	ZXDH_DTB_CFG_QUEUE_DTB_LEN    = 2,
	ZXDH_DTB_INFO_QUEUE_BUF_SPACE = 3,
	ZXDH_DTB_CFG_EPID_V_FUNC_NUM  = 4,
	ZXDH_REG_ENUM_MAX_VALUE
} ZXDH_REG_INFO_E;

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
} ZXDH_DEV_CFG_T;

typedef struct zxdh_dev_mngr_t {
	uint32_t         device_num;
	uint32_t         is_init;
	ZXDH_DEV_CFG_T       *p_dev_array[ZXDH_DEV_CHANNEL_MAX];
} ZXDH_DEV_MGR_T;

typedef struct zxdh_dtb_addr_info_t {
	uint32_t sdt_no;
	uint32_t size;
	uint32_t phy_addr;
	uint32_t vir_addr;
} ZXDH_DTB_ADDR_INFO_T;

typedef struct zxdh_dev_init_ctrl_t {
	uint32_t vport;
	char  port_name[ZXDH_PORT_NAME_MAX];
	uint32_t vector;
	uint32_t queue_id;
	uint32_t np_bar_offset;
	uint32_t np_bar_len;
	uint32_t pcie_vir_addr;
	uint32_t down_phy_addr;
	uint32_t down_vir_addr;
	uint32_t dump_phy_addr;
	uint32_t dump_vir_addr;
	uint32_t dump_sdt_num;
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
	ZXDH_FIELD_T *p_fields;

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

typedef struct zxdh_sdt_tbl_eram_t {
	uint32_t table_type;
	uint32_t eram_mode;
	uint32_t eram_base_addr;
	uint32_t eram_table_depth;
	uint32_t eram_clutch_en;
} ZXDH_SDTTBL_ERAM_T;

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
	ZXDH_DTB_FIELD_T *p_fields;
} ZXDH_DTB_TABLE_T;

typedef struct zxdh_dtb_queue_item_info_t {
	uint32_t cmd_vld;
	uint32_t cmd_type;
	uint32_t int_en;
	uint32_t data_len;
	uint32_t data_laddr;
	uint32_t data_hddr;
} ZXDH_DTB_QUEUE_ITEM_INFO_T;

typedef struct zxdh_dtb_queue_len_t {
	uint32_t cfg_dtb_cmd_type;
	uint32_t cfg_dtb_cmd_int_en;
	uint32_t cfg_queue_dtb_len;
} ZXDH_DTB_QUEUE_LEN_T;

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
} ZXDH_SDTTBL_ETCAM_T;

typedef struct zxdh_sdt_tbl_porttbl_t {
	uint32_t table_type;
	uint32_t porttbl_clutch_en;
} ZXDH_SDTTBL_PORTTBL_T;

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

#endif /* ZXDH_NP_H */
