/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 ZTE Corporation
 */

#ifndef ZXDH_NP_H
#define ZXDH_NP_H

#include <stdint.h>

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

int zxdh_np_host_init(uint32_t dev_id, ZXDH_DEV_INIT_CTRL_T *p_dev_init_ctrl);

#endif /* ZXDH_NP_H */
