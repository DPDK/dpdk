/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 ZTE Corporation
 */

#ifndef ZXDH_NP_H
#define ZXDH_NP_H

#include <stdint.h>
#include <rte_spinlock.h>

#define ZXDH_OK                               (0)
#define ZXDH_ERR                              (1)
#define ZXDH_TRUE	                          (1)
#define ZXDH_FALSE	                          (0)
#define ZXDH_DISABLE                          (0)
#define ZXDH_ENABLE                           (1)
#define ZXDH_PORT_NAME_MAX                    (32)
#define ZXDH_DEV_CHANNEL_MAX                  (16)
#define ZXDH_DEV_SDT_ID_MAX                   (256U)
#define ZXDH_DEV_PF_NUM_MAX                   (8)
#define ZXDH_DEV_SLOT_ID(DEVICE_ID)           ((DEVICE_ID) & (ZXDH_DEV_CHANNEL_MAX - 1))
#define ZXDH_DEV_PCIE_ID(DEVICE_ID)           (((DEVICE_ID) >> 16) & 0xFFFF)
#define ZXDH_DEV_VF_INDEX(DEVICE_ID)          (ZXDH_DEV_PCIE_ID(DEVICE_ID) & 0xFF)
#define ZXDH_DEV_PF_INDEX(DEVICE_ID)          ((ZXDH_DEV_PCIE_ID(DEVICE_ID) >> 8) & 0x7)

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
#define ZXDH_DTB_ZCAM_LEN_SIZE                   (5)
#define ZXDH_DTB_DUMP_ZCAM_TYPE                  (0)
#define ZXDH_DTB_DUMP_DDR_TYPE                   (1)
#define ZXDH_DTB_DMUP_DATA_MAX                   (64 * 1024 * 1024)
#define ZXDH_DTB_TABLE_DUMP_INFO_BUFF_SIZE       (1024 * 4)
#define ZXDH_DTB_ETCAM_LEN_SIZE                  (6)

#define ZXDH_ETCAM_LEN_SIZE            (6)
#define ZXDH_ETCAM_BLOCK_NUM           (8)
#define ZXDH_ETCAM_TBLID_NUM           (8)
#define ZXDH_ETCAM_RAM_NUM             (8)
#define ZXDH_ETCAM_RAM_WIDTH           (80U)
#define ZXDH_ETCAM_WR_MASK_MAX         (((uint32_t)1 << ZXDH_ETCAM_RAM_NUM) - 1)
#define ZXDH_ETCAM_WIDTH_MIN           (ZXDH_ETCAM_RAM_WIDTH)
#define ZXDH_ETCAM_WIDTH_MAX           (ZXDH_ETCAM_RAM_NUM * ZXDH_ETCAM_RAM_WIDTH)
#define ZXDH_ETCAM_RAM_DEPTH           (512)
#define ZXDH_ACL_FLAG_ETCAM0_EN        (1 << 0)
#define ZXDH_BLOCK_IDXBASE_BIT_OFF     (9)
#define ZXDH_BLOCK_IDXBASE_BIT_MASK    (0x7f)

#define ZXDH_DTB_TABLE_DATA_BUFF_SIZE           (16384)
#define ZXDH_DTB_TABLE_CMD_SIZE_BIT             (128)

#define ZXDH_SE_SMMU0_ERAM_BLOCK_NUM            (32)
#define ZXDH_SE_SMMU0_ERAM_ADDR_NUM_PER_BLOCK   (0x4000)
#define ZXDH_SE_SMMU0_ERAM_ADDR_NUM_TOTAL  \
		(ZXDH_SE_SMMU0_ERAM_BLOCK_NUM * ZXDH_SE_SMMU0_ERAM_ADDR_NUM_PER_BLOCK)

#define ZXDH_CHANNEL_REPS_LEN                   (4)

#define ZXDH_NPSDK_COMPAT_ITEM_ID               (10)
#define ZXDH_DPU_NO_DEBUG_PF_COMPAT_REG_OFFSET  (0x5400)

#define ZXDH_VF_ACTIVE(VPORT)                   (((VPORT) & 0x0800) >> 11)
#define ZXDH_EPID_BY(VPORT)                     (((VPORT) & 0x7000) >> 12)
#define ZXDH_FUNC_NUM(VPORT)                    (((VPORT) & 0x0700) >> 8)
#define ZXDH_VFUNC_NUM(VPORT)                   (((VPORT) & 0x00FF))
#define ZXDH_IS_PF(VPORT)                       (!ZXDH_VF_ACTIVE(VPORT))

#define ZXDH_RBT_RED                            (0x1)
#define ZXDH_RBT_BLACK                          (0x2)
#define ZXDH_RBT_MAX_DEPTH                      (128)

#define ZXDH_LISTSTACK_INVALID_INDEX	        (0)
#define ZXDH_LISTSTACK_MAX_ELEMENT		        (0x0ffffffe)

#define ZXDH_HASH_FUNC_MAX_NUM                  (4)
#define ZXDH_HASH_BULK_MAX_NUM                  (32)
#define ZXDH_HASH_TABLE_MAX_NUM                 (38)
#define ZXDH_ERAM_MAX_NUM                       (60)
#define ZXDH_ETCAM_MAX_NUM                      (8)

#define ZXDH_SE_RAM_DEPTH                       (512)
#define ZXDH_SE_ZCELL_NUM                       (4)
#define ZXDH_SE_ZREG_NUM                        (4)
#define ZXDH_SE_ALG_BANK_NUM                    (29)
#define ZXDH_SE_ZBLK_NUM                        (32)
#define ZXDH_MAX_FUN_NUM                        (8)
#define ZXDH_HASH_KEY_MAX                       (49)
#define ZXDH_HASH_RST_MAX                       (32)
#define ZXDH_HASH_TBL_ID_NUM                    (32)
#define ZXDH_HASH_BULK_NUM                      (8)
#define ZXDH_HASH_CMP_ZCELL                     (1)
#define ZXDH_HASH_CMP_ZBLK                      (2)
#define ZXDH_HASH_FUNC_ID_NUM                   (4)
#define ZXDH_ZCELL_FLAG_IS_MONO                 (1)
#define ZXDH_ZREG_FLAG_IS_MONO                  (1)
#define ZXDH_HASH_DDR_CRC_NUM                   (4)
#define ZXDH_HASH_TBL_FLAG_AGE                  (1 << 0)
#define ZXDH_HASH_TBL_FLAG_LEARN                (1 << 1)
#define ZXDH_HASH_TBL_FLAG_MC_WRT               (1 << 2)
#define ZXDH_HASH_ACTU_KEY_STEP                 (1)
#define ZXDH_HASH_KEY_CTR_SIZE                  (1)
#define ZXDH_ZCELL_IDX_BT_WIDTH                 (2)
#define ZXDH_ZBLK_IDX_BT_WIDTH                  (3)
#define ZXDH_ZGRP_IDX_BT_WIDTH                  (2)
#define ZXDH_HASH_ENTRY_POS_STEP                (16)
#define ZXDH_HASH_TBL_ID_NUM                    (32)
#define ZXDH_SE_ITEM_WIDTH_MAX                  (64)
#define ZXDH_SE_ENTRY_WIDTH_MAX                 (64)
#define ZXDH_REG_SRAM_FLAG_BT_START             (16)
#define ZXDH_ZBLK_NUM_PER_ZGRP                  (8)
#define ZXDH_ZBLK_IDX_BT_START                  (11)
#define ZXDH_ZBLK_WRT_MASK_BT_START             (17)
#define ZXDH_ZCELL_ADDR_BT_WIDTH                (9)

#define ZXDH_COMM_PTR_TO_VAL(p)                 ((uint64_t)(p))
#define ZXDH_COMM_VAL_TO_PTR(v)                 ((void *)((uint64_t)(v)))

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

#define ZXDH_SPIN_LOCK_BASE                     (ZXDH_RC_BASE            | 0x300)
#define ZXDH_SPIN_LOCK_INIT_FAIL                (ZXDH_SPIN_LOCK_BASE     | 0x001)
#define ZXDH_SPIN_LOCK_LOCK_FAIL                (ZXDH_SPIN_LOCK_BASE     | 0x002)
#define ZXDH_SPIN_LOCK_ULOCK_FAIL               (ZXDH_SPIN_LOCK_BASE     | 0X003)
#define ZXDH_SPIN_LOCK_DESTROY_FAIL             (ZXDH_SPIN_LOCK_BASE     | 0X004)

#define ZXDH_DOUBLE_LINK_BASE                   (ZXDH_RC_BASE            | 0x500)
#define ZXDH_DOUBLE_LINK_ELEMENT_NUM_ERR        (ZXDH_DOUBLE_LINK_BASE   | 0x001)
#define ZXDH_DOUBLE_LINK_MALLOC_FAIL            (ZXDH_DOUBLE_LINK_BASE   | 0x002)
#define ZXDH_DOUBLE_LINK_POINT_NULL             (ZXDH_DOUBLE_LINK_BASE   | 0x003)
#define ZXDH_DOUBLE_LINK_CHK_SUM_ERR            (ZXDH_DOUBLE_LINK_BASE   | 0x004)
#define ZXDH_DOUBLE_LINK_NO_EXIST_FREENODE      (ZXDH_DOUBLE_LINK_BASE   | 0x005)
#define ZXDH_DOUBLE_LINK_FREE_INDX_INVALID      (ZXDH_DOUBLE_LINK_BASE   | 0x006)
#define ZXDH_DOUBLE_LINK_NO_EXIST_PRENODE       (ZXDH_DOUBLE_LINK_BASE   | 0x007)
#define ZXDH_DOUBLE_LINK_INPUT_INDX_INVALID     (ZXDH_DOUBLE_LINK_BASE   | 0x008)
#define ZXDH_DOUBLE_LINK_INIT_ELEMENT_NUM_ERR   (ZXDH_DOUBLE_LINK_BASE   | 0x009)

#define ZXDH_LIST_STACK_BASE                    (ZXDH_RC_BASE            | 0x800)
#define ZXDH_LIST_STACK_ELEMENT_NUM_ERR         (ZXDH_LIST_STACK_BASE    | 0x001)
#define ZXDH_LIST_STACK_POINT_NULL              (ZXDH_LIST_STACK_BASE    | 0x002)
#define ZXDH_LIST_STACK_ALLOC_MEMORY_FAIL       (ZXDH_LIST_STACK_BASE    | 0x003)
#define ZXDH_LIST_STACK_ISEMPTY_ERR             (ZXDH_LIST_STACK_BASE    | 0x004)
#define ZXDH_LIST_STACK_FREE_INDEX_INVALID      (ZXDH_LIST_STACK_BASE    | 0x005)
#define ZXDH_LIST_STACK_ALLOC_INDEX_INVALID     (ZXDH_LIST_STACK_BASE    | 0x006)
#define ZXDH_LIST_STACK_ALLOC_INDEX_USED        (ZXDH_LIST_STACK_BASE    | 0x007)

#define ZXDH_ERAM128_BADDR_MASK                 (0x3FFFF80)

#define ZXDH_DTB_TABLE_MODE_ERAM                (0)
#define ZXDH_DTB_TABLE_MODE_DDR                 (1)
#define ZXDH_DTB_TABLE_MODE_ZCAM                (2)
#define ZXDH_DTB_TABLE_MODE_ETCAM               (3)
#define ZXDH_DTB_TABLE_MODE_MC_HASH             (4)
#define ZXDH_DTB_TABLE_VALID                    (1)
#define ZXDH_DTB_DUMP_MODE_ERAM                 (0)
#define ZXDH_DTB_DUMP_MODE_ZCAM                 (2)
#define ZXDH_DTB_DUMP_MODE_ETCAM                (3)

#define ZXDH_DTB_DELAY_TIME                     (50)
#define ZXDH_DTB_DOWN_OVER_TIME                 (2000)
#define ZXDH_DTB_DUMP_OVER_TIME                 (200000)

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

#define ZXDH_RC_CTRLCH_BASE                     (0xf00)
#define ZXDH_RC_CTRLCH_MSG_LEN_ZERO             (ZXDH_RC_CTRLCH_BASE | 0x0)
#define ZXDH_RC_CTRLCH_MSG_PRO_ERR              (ZXDH_RC_CTRLCH_BASE | 0x1)
#define ZXDH_RC_CTRLCH_MSG_TYPE_NOT_SUPPORT     (ZXDH_RC_CTRLCH_BASE | 0x2)
#define ZXDH_RC_CTRLCH_MSG_OPER_NOT_SUPPORT     (ZXDH_RC_CTRLCH_BASE | 0x3)
#define ZXDH_RC_CTRLCH_MSG_DROP                 (ZXDH_RC_CTRLCH_BASE | 0x4)

#define ZXDH_RBT_RC_BASE                        (0x1000)
#define ZXDH_RBT_RC_UPDATE                      (ZXDH_RBT_RC_BASE | 0x1)
#define ZXDH_RBT_RC_SRHFAIL                     (ZXDH_RBT_RC_BASE | 0x2)
#define ZXDH_RBT_RC_FULL                        (ZXDH_RBT_RC_BASE | 0x3)
#define ZXDH_RBT_ISEMPTY_ERR                    (ZXDH_RBT_RC_BASE | 0x4)
#define ZXDH_RBT_PARA_INVALID                   (ZXDH_RBT_RC_BASE | 0x5)

#define ZXDH_SE_RC_BASE                         (0x50000)
#define ZXDH_SE_RC_HASH_BASE                    (ZXDH_SE_RC_BASE | 0x4000)
#define ZXDH_HASH_RC_INVALID_FUNCINFO           (ZXDH_SE_RC_HASH_BASE | 0x1)
#define ZXDH_HASH_RC_INVALID_ZBLCK              (ZXDH_SE_RC_HASH_BASE | 0x2)
#define ZXDH_HASH_RC_INVALID_ZCELL              (ZXDH_SE_RC_HASH_BASE | 0x3)
#define ZXDH_HASH_RC_INVALID_KEY                (ZXDH_SE_RC_HASH_BASE | 0x4)
#define ZXDH_HASH_RC_INVALID_TBL_ID_INFO        (ZXDH_SE_RC_HASH_BASE | 0x5)
#define ZXDH_HASH_RC_RB_TREE_FULL               (ZXDH_SE_RC_HASH_BASE | 0x6)
#define ZXDH_HASH_RC_INVALID_KEY_TYPE           (ZXDH_SE_RC_HASH_BASE | 0x7)
#define ZXDH_HASH_RC_ADD_UPDATE                 (ZXDH_SE_RC_HASH_BASE | 0x8)
#define ZXDH_HASH_RC_DEL_SRHFAIL                (ZXDH_SE_RC_HASH_BASE | 0x9)
#define ZXDH_HASH_RC_ITEM_FULL                  (ZXDH_SE_RC_HASH_BASE | 0xa)
#define ZXDH_HASH_RC_INVALID_DDR_WIDTH_MODE     (ZXDH_SE_RC_HASH_BASE | 0xb)
#define ZXDH_HASH_RC_INVALID_PARA               (ZXDH_SE_RC_HASH_BASE | 0xc)
#define ZXDH_HASH_RC_TBL_FULL                   (ZXDH_SE_RC_HASH_BASE | 0xd)
#define ZXDH_HASH_RC_SRH_FAIL                   (ZXDH_SE_RC_HASH_BASE | 0xe)
#define ZXDH_HASH_RC_MATCH_ITEM_FAIL            (ZXDH_SE_RC_HASH_BASE | 0xf)
#define ZXDH_HASH_RC_DDR_WIDTH_MODE_ERR         (ZXDH_SE_RC_HASH_BASE | 0x10)
#define ZXDH_HASH_RC_INVALID_ITEM_TYPE          (ZXDH_SE_RC_HASH_BASE | 0x11)
#define ZXDH_HASH_RC_REPEAT_INIT                (ZXDH_SE_RC_HASH_BASE | 0x12)

#define ZXDH_SE_RC_CFG_BASE                     (ZXDH_SE_RC_BASE | 0x1000)
#define ZXDH_SE_RC_ZBLK_FULL                    (ZXDH_SE_RC_CFG_BASE | 0x1)
#define ZXDH_SE_RC_FUN_INVALID                  (ZXDH_SE_RC_CFG_BASE | 0x2)
#define ZXDH_SE_RC_PARA_INVALID                 (ZXDH_SE_RC_CFG_BASE | 0x3)

#define ZXDH_RC_TABLE_BASE                      (0x800)
#define ZXDH_RC_TABLE_PARA_INVALID              (ZXDH_RC_TABLE_BASE | 0x0)
#define ZXDH_RC_TABLE_RANGE_INVALID             (ZXDH_RC_TABLE_BASE | 0x1)
#define ZXDH_RC_TABLE_CALL_FUNC_FAIL            (ZXDH_RC_TABLE_BASE | 0x2)
#define ZXDH_RC_TABLE_SDT_MSG_INVALID           (ZXDH_RC_TABLE_BASE | 0x3)
#define ZXDH_RC_TABLE_SDT_MGR_INVALID           (ZXDH_RC_TABLE_BASE | 0x4)
#define ZXDH_RC_TABLE_IF_VALUE_FAIL             (ZXDH_RC_TABLE_BASE | 0x5)

#define ZXDH_ACL_RC_BASE                        (0x60000)
#define ZXDH_ACL_RC_INVALID_TBLID               (ZXDH_ACL_RC_BASE | 0x0)
#define ZXDH_ACL_RC_INVALID_BLOCKNUM            (ZXDH_ACL_RC_BASE | 0x1)
#define ZXDH_ACL_RC_INVALID_BLOCKID             (ZXDH_ACL_RC_BASE | 0x2)
#define ZXDH_ACL_RC_TBL_NOT_INIT                (ZXDH_ACL_RC_BASE | 0x3)
#define ZXDH_ACL_RC_ETCAMID_NOT_INIT            (ZXDH_ACL_RC_BASE | 0x4)
#define ZXDH_ACL_RC_AS_ERAM_NOT_ENOUGH          (ZXDH_ACL_RC_BASE | 0x5)
#define ZXDH_ACL_RC_RB_TREE_FULL                (ZXDH_ACL_RC_BASE | 0x6)
#define ZXDH_ACL_RC_TABLE_FULL                  (ZXDH_ACL_RC_BASE | 0x7)
#define ZXDH_ACL_RC_INVALID_PARA                (ZXDH_ACL_RC_BASE | 0x8)
#define ZXDH_ACL_RC_DEL_SRHFAIL                 (ZXDH_ACL_RC_BASE | 0x9)
#define ZXDH_ACL_RC_TABLE_UPDATE                (ZXDH_ACL_RC_BASE | 0xa)
#define ZXDH_ACL_RC_SRH_FAIL                    (ZXDH_ACL_RC_BASE | 0xb)

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

typedef struct zxdh_s_freelink {
	uint32_t index;
	uint32_t next;
} ZXDH_COMM_FREELINK;

typedef struct zxdh_s_list_stack_manager {
	ZXDH_COMM_FREELINK  *p_array;
	uint32_t capacity;
	uint32_t p_head;
	uint32_t free_num;
	uint32_t used_num;
} ZXDH_LISTSTACK_MANAGER;

typedef struct zxdh_rb_tn {
	void        *p_key;
	uint32_t    color_lsv;
	struct    zxdh_rb_tn   *p_left;
	struct    zxdh_rb_tn   *p_right;
	struct    zxdh_rb_tn   *p_parent;
	ZXDH_D_NODE    tn_ln;
} ZXDH_RB_TN;

typedef int32_t  (*ZXDH_RB_CMPFUN)(void *p_new, void *p_old, uint32_t keysize);
typedef int32_t (*ZXDH_CMP_FUNC)(ZXDH_D_NODE *data1, ZXDH_D_NODE *data2, void*);
typedef uint32_t (*ZXDH_WRITE32_FUN)(uint32_t dev_id, uint32_t addr, uint32_t write_data);
typedef uint32_t (*ZXDH_READ32_FUN) (uint32_t dev_id, uint32_t addr, uint32_t *read_data);
typedef uint32_t (*ZXDH_LPM_AS_RSLT_WRT_FUNCTION)(uint32_t dev_id,
		uint32_t as_type, uint32_t tbl_id, uint32_t index, uint8_t *p_data);
typedef uint16_t (*ZXDH_HASH_FUNCTION)(uint8_t *pkey, uint32_t width, uint16_t arg);
typedef uint32_t (*ZXDH_HASH_FUNCTION32)(uint8_t *pkey, uint32_t width, uint32_t arg);

typedef struct _rb_cfg {
	uint32_t				key_size;
	uint32_t				is_dynamic;
	ZXDH_RB_TN				*p_root;
	ZXDH_D_HEAD				tn_list;
	ZXDH_RB_CMPFUN			p_cmpfun;
	ZXDH_LISTSTACK_MANAGER	*p_lsm;
	uint8_t					*p_keybase;
	ZXDH_RB_TN				*p_tnbase;
	uint32_t				is_init;
} ZXDH_RB_CFG;

typedef enum zxdh_se_fun_type_e {
	ZXDH_FUN_HASH = 1,
	ZXDH_FUN_LPM,
	ZXDH_FUN_ACL,
	ZXDH_FUN_MAX
} ZXDH_SE_FUN_TYPE;

typedef enum zxdh_acl_as_mode_e {
	ZXDH_ACL_AS_MODE_16b  = 0,
	ZXDH_ACL_AS_MODE_32b  = 1,
	ZXDH_ACL_AS_MODE_64b  = 2,
	ZXDH_ACL_AS_MODE_128b = 3,
	ZXDH_ACL_AS_MODE_INVALID,
} ZXDH_ACL_AS_MODE_E;

typedef enum zxdh_hash_key_type_e {
	ZXDH_HASH_KEY_INVALID = 0,
	ZXDH_HASH_KEY_128b,
	ZXDH_HASH_KEY_256b,
	ZXDH_HASH_KEY_512b,
} ZXDH_HASH_KEY_TYPE;

typedef enum zxdh_se_item_type_e {
	ZXDH_ITEM_INVALID = 0,
	ZXDH_ITEM_RAM,
	ZXDH_ITEM_DDR_256,
	ZXDH_ITEM_DDR_512,
	ZXDH_ITEM_REG,
} ZXDH_SE_ITEM_TYPE;

typedef struct zxdh_avl_node_t {
	void                    *p_key;
	uint32_t                result;
	int32_t                 avl_height;
	struct zxdh_avl_node_t  *p_avl_left;
	struct zxdh_avl_node_t  *p_avl_right;
	ZXDH_D_NODE             avl_node_list;
} ZXDH_AVL_NODE;

typedef struct zxdh_se_item_cfg_t {
	ZXDH_D_HEAD  item_list;
	uint32_t  item_index;
	uint32_t  hw_addr;
	uint32_t  bulk_id;
	uint32_t  item_type;
	uint8_t    wrt_mask;
	uint8_t    valid;
	uint8_t    pad[2];
} ZXDH_SE_ITEM_CFG;

typedef struct zxdh_se_zcell_cfg_t {
	uint8_t            flag;
	uint32_t           bulk_id;
	uint32_t           zcell_idx;
	uint16_t           mask_len;
	uint8_t            is_used;
	uint8_t            is_share;
	uint32_t           item_used;
	ZXDH_SE_ITEM_CFG   item_info[ZXDH_SE_RAM_DEPTH];
	ZXDH_D_NODE        zcell_dn;
	ZXDH_AVL_NODE      zcell_avl;
} ZXDH_SE_ZCELL_CFG;

typedef struct zxdh_se_zreg_cfg_t {
	uint8_t            flag;
	uint8_t            pad[3];
	uint32_t           bulk_id;
	ZXDH_SE_ITEM_CFG   item_info;
} ZXDH_SE_ZREG_CFG;

typedef struct zxdh_se_zblk_cfg_t {
	uint32_t          zblk_idx;
	uint16_t          is_used;
	uint16_t          zcell_bm;
	uint16_t          hash_arg;
	uint16_t          pad;
	ZXDH_SE_ZCELL_CFG    zcell_info[ZXDH_SE_ZCELL_NUM];
	ZXDH_SE_ZREG_CFG     zreg_info[ZXDH_SE_ZREG_NUM];
	ZXDH_D_NODE          zblk_dn;
} ZXDH_SE_ZBLK_CFG;

typedef struct zxdh_func_id_info_t {
	void *fun_ptr;
	uint8_t  fun_type;
	uint8_t  fun_id;
	uint8_t  is_used;
	uint8_t  pad;
} ZXDH_FUNC_ID_INFO;

typedef struct zxdh_ddr_mem_t {
	uint32_t     total_num;
	uint32_t     base_addr;
	uint32_t     base_addr_offset;
	uint32_t     ecc_en;
	uint32_t     bank_num;
	uint32_t     bank_info[ZXDH_SE_ALG_BANK_NUM];
	uint32_t     share_type;
	uint32_t     item_used;
	ZXDH_LISTSTACK_MANAGER *p_ddr_mng;
} ZXDH_DDR_MEM;

typedef struct zxdh_share_ram_t {
	uint32_t       zblk_array[ZXDH_SE_ZBLK_NUM];
	ZXDH_D_HEAD    zblk_list;
	ZXDH_D_HEAD    zcell_free_list;
	uint32_t       def_route_num;
	ZXDH_RB_CFG    def_rb;
	struct def_route_info  *p_dr_info;
	ZXDH_DDR_MEM   ddr4_info;
	ZXDH_DDR_MEM   ddr6_info;
} ZXDH_SHARE_RAM;

typedef struct zxdh_se_cfg_t {
	ZXDH_SE_ZBLK_CFG   zblk_info[ZXDH_SE_ZBLK_NUM];
	ZXDH_FUNC_ID_INFO  fun_info[ZXDH_MAX_FUN_NUM];
	ZXDH_SHARE_RAM     route_shareram;
	uint32_t           reg_base;
	ZXDH_WRITE32_FUN   p_write32_fun;
	ZXDH_READ32_FUN    p_read32_fun;
	uint32_t           lpm_flags;
	void               *p_client;
	uint32_t           dev_id;
	ZXDH_LPM_AS_RSLT_WRT_FUNCTION p_as_rslt_wrt_fun;
} ZXDH_SE_CFG;

typedef struct hash_ddr_cfg_t {
	uint32_t bulk_use;
	uint32_t ddr_baddr;
	uint32_t ddr_ecc_en;
	uint32_t item_num;
	uint32_t bulk_id;
	uint32_t hash_ddr_arg;
	uint32_t width_mode;
	uint32_t hw_baddr;
	uint32_t zcell_num;
	uint32_t zreg_num;
	ZXDH_SE_ITEM_CFG  **p_item_array;
} HASH_DDR_CFG;

typedef struct zxdh_hash_tbl_info_t {
	uint32_t fun_id;
	uint32_t actu_key_size;
	uint32_t key_type;
	uint8_t is_init;
	uint8_t mono_zcell;
	uint8_t zcell_num;
	uint8_t mono_zreg;
	uint8_t zreg_num;
	uint8_t is_age;
	uint8_t is_lrn;
	uint8_t is_mc_wrt;
} ZXDH_HASH_TBL_ID_INFO;

typedef struct zxdh_hash_rbkey_info_t {
	uint8_t          key[ZXDH_HASH_KEY_MAX];
	uint8_t          rst[ZXDH_HASH_RST_MAX];
	ZXDH_D_NODE      entry_dn;
	ZXDH_SE_ITEM_CFG *p_item_info;
	uint32_t         entry_size;
	uint32_t         entry_pos;
} ZXDH_HASH_RBKEY_INFO;

typedef struct zxdh_hash_table_stat_t {
	float ddr;
	float zcell;
	float zreg;
	float sum;
} ZXDH_HASH_TABLE_STAT;

typedef struct zxdh_hash_zreg_mono_stat_t {
	uint32_t zblk_id;
	uint32_t zreg_id;
} ZXDH_HASH_ZREG_MONO_STAT;

typedef struct zxdh_hash_bulk_zcam_stat_t {
	uint32_t zcell_mono_idx[ZXDH_SE_ZBLK_NUM * ZXDH_SE_ZCELL_NUM];
	ZXDH_HASH_ZREG_MONO_STAT zreg_mono_id[ZXDH_SE_ZBLK_NUM][ZXDH_SE_ZREG_NUM];
} ZXDH_HASH_BULK_ZCAM_STAT;

typedef struct zxdh_hash_stat_t {
	uint32_t insert_ok;
	uint32_t insert_fail;
	uint32_t insert_same;
	uint32_t insert_ddr;
	uint32_t insert_zcell;
	uint32_t insert_zreg;
	uint32_t delete_ok;
	uint32_t delete_fail;
	uint32_t search_ok;
	uint32_t search_fail;
	uint32_t zblock_num;
	uint32_t zblock_array[ZXDH_SE_ZBLK_NUM];
	ZXDH_HASH_TABLE_STAT insert_table[ZXDH_HASH_TBL_ID_NUM];
	ZXDH_HASH_BULK_ZCAM_STAT *p_bulk_zcam_mono[ZXDH_HASH_BULK_NUM];
} ZXDH_HASH_STAT;

typedef struct zxdh_hash_cfg_t {
	uint32_t              fun_id;
	uint8_t               ddr_valid;
	uint8_t               pad[3];
	ZXDH_HASH_FUNCTION32  p_hash32_fun;
	ZXDH_HASH_FUNCTION    p_hash16_fun;
	HASH_DDR_CFG          *p_bulk_ddr_info[ZXDH_HASH_BULK_NUM];
	uint8_t               bulk_ram_mono[ZXDH_HASH_BULK_NUM];
	ZXDH_SHARE_RAM        hash_shareram;
	ZXDH_SE_CFG           *p_se_info;
	ZXDH_RB_CFG           hash_rb;
	ZXDH_RB_CFG           ddr_cfg_rb;
	ZXDH_HASH_STAT        hash_stat;
} ZXDH_HASH_CFG;

typedef struct hash_entry_cfg_t {
	uint32_t fun_id;
	uint8_t bulk_id;
	uint8_t table_id;
	uint8_t key_type;
	uint8_t rsp_mode;
	uint32_t actu_key_size;
	uint32_t key_by_size;
	uint32_t rst_by_size;
	ZXDH_SE_CFG *p_se_cfg;
	ZXDH_HASH_CFG *p_hash_cfg;
	ZXDH_HASH_RBKEY_INFO *p_rbkey_new;
	ZXDH_RB_TN *p_rb_tn_new;
} HASH_ENTRY_CFG;

typedef struct zxdh_hash_ddr_resc_cfg_t {
	uint32_t ddr_width_mode;
	uint32_t ddr_crc_sel;
	uint32_t ddr_item_num;
	uint32_t ddr_baddr;
	uint32_t ddr_ecc_en;
} ZXDH_HASH_DDR_RESC_CFG_T;

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

typedef enum zxdh_hash_srh_mode {
	ZXDH_HASH_SRH_MODE_SOFT = 1,
	ZXDH_HASH_SRH_MODE_HDW  = 2,
} ZXDH_HASH_SRH_MODE;

typedef struct zxdh_hash_entry {
	uint8_t *p_key;
	uint8_t *p_rst;
} ZXDH_HASH_ENTRY;

typedef struct zxdh_acl_entry_ex_t {
	uint32_t  idx_val;
	ZXDH_D_HEAD  idx_list;
	uint32_t  pri;
	uint8_t    *key_data;
	uint8_t    *key_mask;
	uint8_t    *p_as_rslt;
} ZXDH_ACL_ENTRY_EX_T;

typedef uint32_t (*ZXDH_APT_ACL_ENTRY_SET_FUNC)(void *p_data, ZXDH_ACL_ENTRY_EX_T *acl_entry);
typedef uint32_t (*ZXDH_APT_ACL_ENTRY_GET_FUNC)(void *p_data, ZXDH_ACL_ENTRY_EX_T *acl_entry);
typedef uint32_t (*ZXDH_APT_ERAM_SET_FUNC)(void *p_data, uint32_t buf[4]);
typedef uint32_t (*ZXDH_APT_ERAM_GET_FUNC)(void *p_data, uint32_t buf[4]);
typedef uint32_t (*ZXDH_APT_HASH_ENTRY_SET_FUNC)(void *p_data, ZXDH_HASH_ENTRY *p_entry);
typedef uint32_t (*ZXDH_APT_HASH_ENTRY_GET_FUNC)(void *p_data, ZXDH_HASH_ENTRY *p_entry);
typedef int32_t (*ZXDH_KEY_CMP_FUNC)(void *p_new_key, void *p_old_key, uint32_t key_len);

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

typedef enum zxdh_hash_ddr_width_mode_e {
	ZXDH_DDR_WIDTH_INVALID = 0,
	ZXDH_DDR_WIDTH_256b,
	ZXDH_DDR_WIDTH_512b,
} ZXDH_HASH_DDR_WIDTH_MODE;

typedef struct zxdh_apt_acl_res_t {
	uint32_t pri_mode;
	uint32_t entry_num;
	uint32_t block_num;
	uint32_t block_index[ZXDH_ETCAM_BLOCK_NUM];
} ZXDH_APT_ACL_RES_T;

typedef struct zxdh_apt_hash_bulk_res_t {
	uint32_t func_id;
	uint32_t bulk_id;
	uint32_t zcell_num;
	uint32_t zreg_num;
	uint32_t ddr_baddr;
	uint32_t ddr_item_num;
	ZXDH_HASH_DDR_WIDTH_MODE ddr_width_mode;
	uint32_t ddr_crc_sel;
	uint32_t ddr_ecc_en;
} ZXDH_APT_HASH_BULK_RES_T;

typedef struct zxdh_apt_hash_func_res_t {
	uint32_t func_id;
	uint32_t zblk_num;
	uint32_t zblk_bitmap;
	uint32_t ddr_dis;
} ZXDH_APT_HASH_FUNC_RES_T;

typedef struct zxdh_apt_eram_table_t {
	uint32_t sdt_no;
	ZXDH_SDT_TBL_ERAM_T eram_sdt;
	uint32_t opr_mode;
	uint32_t rd_mode;
	ZXDH_APT_ERAM_SET_FUNC  eram_set_func;
	ZXDH_APT_ERAM_GET_FUNC  eram_get_func;
} ZXDH_APT_ERAM_TABLE_T;

typedef struct zxdh_apt_acl_table_t {
	uint32_t sdt_no;
	uint32_t sdt_partner;
	ZXDH_SDT_TBL_ETCAM_T acl_sdt;
	ZXDH_APT_ACL_RES_T acl_res;
	ZXDH_APT_ACL_ENTRY_SET_FUNC  acl_set_func;
	ZXDH_APT_ACL_ENTRY_GET_FUNC  acl_get_func;
} ZXDH_APT_ACL_TABLE_T;

typedef struct zxdh_apt_hash_table_t {
	uint32_t sdt_no;
	uint32_t sdt_partner;
	ZXDH_SDT_TBL_HASH_T hash_sdt;
	uint32_t tbl_flag;
	ZXDH_APT_HASH_ENTRY_SET_FUNC hash_set_func;
	ZXDH_APT_HASH_ENTRY_GET_FUNC hash_get_func;
} ZXDH_APT_HASH_TABLE_T;

typedef struct zxdh_apt_eram_res_init_t {
	uint32_t tbl_num;
	ZXDH_APT_ERAM_TABLE_T *eram_res;
} ZXDH_APT_ERAM_RES_INIT_T;

typedef struct zxdh_apt_hash_res_init_t {
	uint32_t func_num;
	uint32_t bulk_num;
	uint32_t tbl_num;
	ZXDH_APT_HASH_FUNC_RES_T *func_res;
	ZXDH_APT_HASH_BULK_RES_T *bulk_res;
	ZXDH_APT_HASH_TABLE_T  *tbl_res;
} ZXDH_APT_HASH_RES_INIT_T;

typedef struct zxdh_apt_acl_res_init_t {
	uint32_t tbl_num;
	ZXDH_APT_ACL_TABLE_T *acl_res;
} ZXDH_APT_ACL_RES_INIT_T;

typedef struct zxdh_apt_stat_res_init_t {
	uint32_t eram_baddr;
	uint32_t eram_depth;
	uint32_t ddr_baddr;
	uint32_t ppu_ddr_offset;
} ZXDH_APT_STAT_RES_INIT_T;

typedef struct zxdh_apt_se_res_t {
	uint32_t valid;
	uint32_t hash_func_num;
	uint32_t hash_bulk_num;
	uint32_t hash_tbl_num;
	uint32_t eram_num;
	uint32_t acl_num;
	uint32_t lpm_num;
	uint32_t ddr_num;
	ZXDH_APT_HASH_FUNC_RES_T  hash_func[ZXDH_HASH_FUNC_MAX_NUM];
	ZXDH_APT_HASH_BULK_RES_T  hash_bulk[ZXDH_HASH_BULK_MAX_NUM];
	ZXDH_APT_HASH_TABLE_T     hash_tbl[ZXDH_HASH_TABLE_MAX_NUM];
	ZXDH_APT_ERAM_TABLE_T     eram_tbl[ZXDH_ERAM_MAX_NUM];
	ZXDH_APT_ACL_TABLE_T      acl_tbl[ZXDH_ETCAM_MAX_NUM];
	ZXDH_APT_STAT_RES_INIT_T  stat_cfg;
} ZXDH_APT_SE_RES_T;

typedef struct zxdh_dev_apt_se_tbl_res_t {
	ZXDH_APT_SE_RES_T   std_nic_res;
	ZXDH_APT_SE_RES_T   offload_res;
} ZXDH_DEV_APT_SE_TBL_RES_T;

typedef struct se_apt_eram_func_t {
	uint32_t opr_mode;
	uint32_t rd_mode;
	ZXDH_APT_ERAM_SET_FUNC  eram_set_func;
	ZXDH_APT_ERAM_GET_FUNC  eram_get_func;
} ZXDH_SE_APT_ERAM_FUNC_T;

typedef struct se_apt_acl_func_t {
	uint32_t sdt_partner;
	ZXDH_APT_ACL_ENTRY_SET_FUNC  acl_set_func;
	ZXDH_APT_ACL_ENTRY_GET_FUNC  acl_get_func;
} ZXDH_SE_APT_ACL_FUNC_T;

typedef struct se_apt_hash_func_t {
	uint32_t sdt_partner;
	ZXDH_APT_HASH_ENTRY_SET_FUNC  hash_set_func;
	ZXDH_APT_HASH_ENTRY_GET_FUNC  hash_get_func;
} ZXDH_SE_APT_HASH_FUNC_T;

typedef struct se_apt_callback_t {
	uint32_t sdt_no;
	uint32_t table_type;
	union {
		ZXDH_SE_APT_ERAM_FUNC_T eram_func;
		ZXDH_SE_APT_ACL_FUNC_T  acl_func;
		ZXDH_SE_APT_HASH_FUNC_T hash_func;
	} se_func_info;
} SE_APT_CALLBACK_T;

typedef struct zxdh_acl_block_info_t {
	uint32_t is_used;
	uint32_t tbl_id;
	uint32_t idx_base;
} ZXDH_ACL_BLOCK_INFO_T;

typedef struct zxdh_acl_etcamid_cfg_t {
	uint32_t is_valid;
	uint32_t as_enable;
	uint32_t as_idx_offset;
	uint32_t as_eram_base;
	ZXDH_D_HEAD tbl_list;
} ZXDH_ACL_ETCAMID_CFG_T;

typedef struct zxdh_acl_key_info_t {
	uint32_t handle;
	uint32_t pri;
	uint8_t key[];
} ZXDH_ACL_KEY_INFO_T;

typedef uint32_t (*ZXDH_ACL_TBL_AS_DDR_WR_FUN)(uint32_t dev_id, uint32_t tbl_type,
		uint32_t tbl_id, uint32_t dir_tbl_share_type, uint32_t dir_tbl_base_addr,
		uint32_t ecc_en, uint32_t index, uint32_t as_mode, uint8_t *p_data);
typedef uint32_t (*ZXDH_ACL_TBL_AS_DDR_RD_FUN)(uint32_t dev_id, uint32_t base_addr,
		uint32_t index, uint32_t as_mode, uint8_t *p_data);
typedef uint32_t (*ZXDH_ACL_AS_RSLT_WRT_FUNCTION)(uint32_t dev_id,
		uint32_t base_addr, uint32_t index, uint32_t as_mode, uint8_t *p_data);

typedef struct zxdh_acl_tbl_cfg_t {
	uint32_t tbl_type;
	uint32_t table_id;
	uint8_t  is_as_ddr;
	uint8_t  ddr_bankcp_info;
	uint32_t dir_tbl_share_type;
	uint8_t  ddr_ecc_en;
	uint32_t pri_mode;
	uint32_t key_mode;
	uint32_t entry_num;
	uint32_t block_num;
	uint32_t *block_array;
	uint32_t is_used;
	uint32_t as_mode;
	uint32_t as_idx_base;
	uint32_t as_enable;
	uint32_t as_eram_base;
	uint32_t ddr_baddr;
	uint32_t idx_offset;
	ZXDH_ACL_TBL_AS_DDR_WR_FUN p_as_ddr_wr_fun;
	ZXDH_ACL_TBL_AS_DDR_RD_FUN p_as_ddr_rd_fun;
	ZXDH_D_NODE entry_dn;
	ZXDH_RB_CFG acl_rb;
	ZXDH_ACL_KEY_INFO_T **acl_key_buff;
	uint8_t *as_rslt_buff;
} ZXDH_ACL_TBL_CFG_T;

typedef struct zxdh_acl_cfg_ex_t {
	void *p_client;
	uint32_t dev_id;
	uint32_t flags;
	ZXDH_ACL_AS_RSLT_WRT_FUNCTION p_as_rslt_write_fun;
	ZXDH_ACL_AS_RSLT_WRT_FUNCTION p_as_rslt_read_fun;
	ZXDH_ACL_BLOCK_INFO_T acl_blocks[ZXDH_ACL_BLOCK_NUM];
	ZXDH_ACL_ETCAMID_CFG_T acl_etcamids;
	ZXDH_ACL_TBL_CFG_T acl_tbls[ZXDH_ACL_TBL_ID_NUM];
} ZXDH_ACL_CFG_EX_T;

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
	uint32_t slot_id;
	uint16_t pcie_id[ZXDH_DEV_PF_NUM_MAX];
	ZXDH_DEV_TYPE_E dev_type;
	uint32_t chip_ver;
	uint32_t access_type;
	uint32_t agent_flag;
	uint32_t vport[ZXDH_DEV_PF_NUM_MAX];
	uint32_t fw_bar_msg_num;
	uint64_t pcie_addr[ZXDH_DEV_PF_NUM_MAX];
	uint64_t riscv_addr;
	uint64_t dma_vir_addr;
	uint64_t dma_phy_addr;
	uint64_t agent_addr[ZXDH_DEV_PF_NUM_MAX];
	uint32_t init_flags[ZXDH_MODULE_INIT_MAX];
	ZXDH_DEV_WRITE_FUNC p_pcie_write_fun;
	ZXDH_DEV_READ_FUNC  p_pcie_read_fun;
	ZXDH_SPINLOCK_T dtb_spinlock;
	ZXDH_SPINLOCK_T smmu0_spinlock;
	ZXDH_SPINLOCK_T dtb_queue_spinlock[ZXDH_DTB_QUEUE_NUM_MAX];
	ZXDH_SPINLOCK_T hash_spinlock[ZXDH_HASH_FUNC_ID_NUM];
	ZXDH_DEV_APT_SE_TBL_RES_T dev_apt_se_tbl_res;
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

typedef enum zxdh_dtb_dump_info_e {
	ZXDH_DTB_DUMP_ERAM    = 0,
	ZXDH_DTB_DUMP_DDR     = 1,
	ZXDH_DTB_DUMP_ZCAM    = 2,
	ZXDH_DTB_DUMP_ETCAM   = 3,
	ZXDH_DTB_DUMP_ENUM_MAX
} ZXDH_DTB_DUMP_INFO_E;

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

typedef enum zxdh_dtb_dump_zcam_width_e {
	ZXDH_DTB_DUMP_ZCAM_128b = 0,
	ZXDH_DTB_DUMP_ZCAM_256b = 1,
	ZXDH_DTB_DUMP_ZCAM_512b = 2,
	ZXDH_DTB_DUMP_ZCAM_RSV  = 3,
} ZXDH_DTB_DUMP_ZCAM_WIDTH_E;

typedef enum zxdh_etcam_opr_type_e {
	ZXDH_ETCAM_OPR_DM = 0,
	ZXDH_ETCAM_OPR_XY = 1,
} ZXDH_ETCAM_OPR_TYPE_E;

typedef enum zxdh_etcam_data_type_e {
	ZXDH_ETCAM_DTYPE_MASK = 0,
	ZXDH_ETCAM_DTYPE_DATA = 1,
} ZXDH_ETCAM_DATA_TYPE_E;

typedef enum zxdh_etcam_entry_mode_e {
	ZXDH_ETCAM_KEY_640b = 0,
	ZXDH_ETCAM_KEY_320b = 1,
	ZXDH_ETCAM_KEY_160b = 2,
	ZXDH_ETCAM_KEY_80b  = 3,
	ZXDH_ETCAM_KEY_INVALID,
} ZXDH_ETCAM_ENTRY_MODE_E;

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

typedef struct zxdh_etcam_entry_t {
	uint32_t mode;
	uint8_t *p_data;
	uint8_t *p_mask;
} ZXDH_ETCAM_ENTRY_T;

typedef struct zxdh_dtb_acl_entry_info_t {
	uint32_t handle;
	uint8_t *key_data;
	uint8_t *key_mask;
	uint8_t *p_as_rslt;
} ZXDH_DTB_ACL_ENTRY_INFO_T;

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

typedef struct zxdh_dtb_zcam_table_form_t {
	uint32_t valid;
	uint32_t type_mode;
	uint32_t ram_reg_flag;
	uint32_t zgroup_id;
	uint32_t zblock_id;
	uint32_t zcell_id;
	uint32_t mask;
	uint32_t sram_addr;
} ZXDH_DTB_ZCAM_TABLE_FORM_T;

typedef struct zxdh_dtb_etcam_table_form_t {
	uint32_t valid;
	uint32_t type_mode;
	uint32_t block_sel;
	uint32_t init_en;
	uint32_t row_or_col_msk;
	uint32_t vben;
	uint32_t reg_tcam_flag;
	uint32_t uload;
	uint32_t rd_wr;
	uint32_t wr_mode;
	uint32_t data_or_mask;
	uint32_t addr;
	uint32_t vbit;
} ZXDH_DTB_ETCAM_TABLE_FORM_T;

typedef struct zxdh_dtb_eram_dump_form_t {
	uint32_t valid;
	uint32_t up_type;
	uint32_t base_addr;
	uint32_t tb_depth;
	uint32_t tb_dst_addr_h;
	uint32_t tb_dst_addr_l;
} ZXDH_DTB_ERAM_DUMP_FORM_T;

typedef struct zxdh_dtb_zcam_dump_form_t {
	uint32_t valid;
	uint32_t up_type;
	uint32_t zgroup_id;
	uint32_t zblock_id;
	uint32_t ram_reg_flag;
	uint32_t z_reg_cell_id;
	uint32_t sram_addr;
	uint32_t tb_depth;
	uint32_t tb_width;
	uint32_t tb_dst_addr_h;
	uint32_t tb_dst_addr_l;
} ZXDH_DTB_ZCAM_DUMP_FORM_T;

typedef struct zxdh_dtb_etcam_dump_form_t {
	uint32_t valid;
	uint32_t up_type;
	uint32_t block_sel;
	uint32_t addr;
	uint32_t rd_mode;
	uint32_t data_or_mask;
	uint32_t tb_depth;
	uint32_t tb_width;
	uint32_t tb_dst_addr_h;
	uint32_t tb_dst_addr_l;
} ZXDH_DTB_ETCAM_DUMP_FORM_T;

typedef struct zxdh_etcam_dump_info_t {
	uint32_t block_sel;
	uint32_t addr;
	uint32_t rd_mode;
	uint32_t data_or_mask;
	uint32_t tb_depth;
	uint32_t tb_width;
} ZXDH_ETCAM_DUMP_INFO_T;

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

typedef struct zxdh_dtb_queue_cfg_t {
	uint64_t up_start_phy_addr;
	uint64_t up_start_vir_addr;
	uint64_t down_start_phy_addr;
	uint64_t down_start_vir_addr;
	uint32_t up_item_size;
	uint32_t down_item_size;
} ZXDH_DTB_QUEUE_CFG_T;

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

typedef enum  zxdh_agent_pcie_bar_e {
	ZXDH_BAR_MSG_NUM_REQ = 0,
	ZXDH_PCIE_BAR_MAX
} ZXDH_MSG_PCIE_BAR_E;

typedef enum zxdh_agent_msg_oper_e {
	ZXDH_WR = 0,
	ZXDH_RD,
	ZXDH_WR_RD_MAX
} ZXDH_MSG_OPER_E;

typedef enum zxdh_msg_dtb_oper_e {
	ZXDH_QUEUE_REQUEST = 0,
	ZXDH_QUEUE_RELEASE = 1,
} ZXDH_MSG_DTB_OPER_E;

typedef enum zxdh_se_res_oper_e {
	ZXDH_HASH_FUNC_BULK_REQ    = 0,
	ZXDH_HASH_TBL_REQ          = 1,
	ZXDH_ERAM_TBL_REQ          = 2,
	ZXDH_ACL_TBL_REQ           = 3,
	ZXDH_LPM_TBL_REQ           = 4,
	ZXDH_DDR_TBL_REQ           = 5,
	ZXDH_STAT_CFG_REQ          = 6,
	ZXDH_RES_REQ_MAX
} ZXDH_MSG_SE_RES_OPER_E;

typedef enum zxdh_agent_msg_res_e {
	ZXDH_RES_STD_NIC_MSG = 0,
	ZXDH_RES_OFFLOAD_MSG,
	ZXDH_RES_MAX_MSG
} MSG_RES_TYPE_E;

typedef enum zxdh_se_res_type_e {
	ZXDH_SE_STD_NIC_RES_TYPE      = 0,
	ZXDH_SE_NON_STD_NIC_RES_TYPE  = 1,
	ZXDH_SE_RES_TYPE_BUTT
} ZXDH_SE_RES_TYPE_E;

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

typedef enum zxdh_msg_acl_index_oper_e {
	ZXDH_ACL_INDEX_REQUEST    = 0,
	ZXDH_ACL_INDEX_RELEASE    = 1,
	ZXDH_ACL_INDEX_VPORT_REL  = 2,
	ZXDH_ACL_INDEX_ALL_REL    = 3,
	ZXDH_ACL_INDEX_STAT_CLR   = 4,
	ZXDH_ACL_INDEX_MAX
} ZXDH_MSG_ACL_INDEX_OPER_E;

typedef struct __rte_aligned(2) zxdh_version_compatible_reg_t {
	uint8_t version_compatible_item;
	uint8_t major;
	uint8_t fw_minor;
	uint8_t drv_minor;
	uint16_t patch;
	uint8_t rsv[2];
} ZXDH_VERSION_COMPATIBLE_REG_T;

typedef struct __rte_aligned(2) zxdh_agent_channel_pcie_bar_msg_t {
	uint8_t dev_id;
	uint8_t type;
	uint8_t oper;
	uint8_t rsv;
} ZXDH_AGENT_PCIE_BAR_MSG_T;

typedef struct __rte_aligned(2) zxdh_agent_channel_reg_msg {
	uint8_t dev_id;
	uint8_t type;
	uint8_t subtype;
	uint8_t oper;
	uint32_t reg_no;
	uint32_t addr;
	uint32_t val_len;
	uint32_t val[32];
} ZXDH_AGENT_CHANNEL_REG_MSG_T;

typedef struct __rte_aligned(2) zxdh_agent_channel_msg_t {
	uint32_t msg_len;
	void *msg;
} ZXDH_AGENT_CHANNEL_MSG_T;

typedef struct __rte_aligned(2) zxdh_agent_channel_dtb_msg_t {
	uint8_t dev_id;
	uint8_t type;
	uint8_t oper;
	uint8_t rsv;
	char name[32];
	uint32_t vport;
	uint32_t queue_id;
} ZXDH_AGENT_CHANNEL_DTB_MSG_T;

typedef struct __rte_aligned(2) zxdh_agent_se_res_msg_t {
	uint8_t dev_id;
	uint8_t type;
	uint8_t sub_type;
	uint8_t oper;
} ZXDH_AGENT_SE_RES_MSG_T;

typedef struct __rte_aligned(2) sdt_tbl_eram_t {
	uint32_t table_type;
	uint32_t eram_mode;
	uint32_t eram_base_addr;
	uint32_t eram_table_depth;
	uint32_t eram_clutch_en;
} ZXDH_NP_SDTTBL_ERAM_T;

typedef struct __rte_aligned(2) sdt_tbl_hash_t {
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
} ZXDH_NP_SDTTBL_HASH_T;

typedef struct __rte_aligned(2) sdt_tbl_etcam_t {
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
} ZXDH_NP_SDTTBL_ETCAM_T;

typedef struct __rte_aligned(2) hash_table_t {
	uint32_t sdt_no;
	uint32_t sdt_partner;
	ZXDH_NP_SDTTBL_HASH_T hash_sdt;
	uint32_t tbl_flag;
} ZXDH_NP_HASH_TABLE_T;

typedef struct __rte_aligned(2) zxdh_np_eram_table_t {
	uint32_t sdt_no;
	ZXDH_NP_SDTTBL_ERAM_T eram_sdt;
	uint32_t opr_mode;
	uint32_t rd_mode;
} ZXDH_NP_ERAM_TABLE_T;

typedef struct __rte_aligned(2) zxdh_np_acl_res_t {
	uint32_t pri_mode;
	uint32_t entry_num;
	uint32_t block_num;
	uint32_t block_index[ZXDH_ETCAM_BLOCK_NUM];
} ZXDH_NP_ACL_RES_T;

typedef struct __rte_aligned(2) zxdh_np_acl_table_t {
	uint32_t sdt_no;
	uint32_t sdt_partner;
	ZXDH_NP_SDTTBL_ETCAM_T acl_sdt;
	ZXDH_NP_ACL_RES_T acl_res;
} ZXDH_NP_ACL_TABLE_T;

typedef struct __rte_aligned(2) hash_func_res_t {
	uint32_t func_id;
	uint32_t zblk_num;
	uint32_t zblk_bitmap;
	uint32_t ddr_dis;
} ZXDH_NP_HASH_FUNC_RES_T;

typedef struct __rte_aligned(2) hash_bulk_res_t {
	uint32_t func_id;
	uint32_t bulk_id;
	uint32_t zcell_num;
	uint32_t zreg_num;
	uint32_t ddr_baddr;
	uint32_t ddr_item_num;
	uint32_t ddr_width_mode;
	uint32_t ddr_crc_sel;
	uint32_t ddr_ecc_en;
} ZXDH_NP_HASH_BULK_RES_T;

typedef struct __rte_aligned(2) zxdh_se_hash_func_bulk_t {
	uint32_t func_num;
	uint32_t bulk_num;
	ZXDH_NP_HASH_FUNC_RES_T fun[ZXDH_HASH_FUNC_MAX_NUM];
	ZXDH_NP_HASH_BULK_RES_T bulk[ZXDH_HASH_BULK_MAX_NUM];
} ZXDH_NP_SE_HASH_FUNC_BULK_T;

typedef struct __rte_aligned(2) zxdh_se_hash_tbl_t {
	uint32_t tbl_num;
	ZXDH_NP_HASH_TABLE_T table[ZXDH_HASH_TABLE_MAX_NUM];
} ZXDH_NP_SE_HASH_TBL_T;

typedef struct __rte_aligned(2) zxdh_se_eram_tbl_t {
	uint32_t tbl_num;
	ZXDH_NP_ERAM_TABLE_T eram[ZXDH_ERAM_MAX_NUM];
} ZXDH_NP_SE_ERAM_TBL_T;

typedef struct __rte_aligned(2) zxdh_se_acl_tbl_t {
	uint32_t tbl_num;
	ZXDH_NP_ACL_TABLE_T acl[ZXDH_ETCAM_MAX_NUM];
} ZXDH_NP_SE_ACL_TBL_T;

typedef struct __rte_aligned(2) zxdh_se_stat_cfg_t {
	uint32_t eram_baddr;
	uint32_t eram_depth;
	uint32_t ddr_baddr;
	uint32_t ppu_ddr_offset;
} ZXDH_NP_SE_STAT_CFG_T;

typedef struct zxdh_dtb_dump_index_t {
	uint32_t index;
	uint32_t index_type;
} ZXDH_DTB_DUMP_INDEX_T;

typedef struct __rte_aligned(2) zxdh_agent_channel_acl_msg_t {
	uint8_t dev_id;
	uint8_t type;
	uint8_t oper;
	uint8_t rsv;
	uint32_t sdt_no;
	uint32_t vport;
	uint32_t index;
	uint32_t counter_id;
	uint32_t rd_mode;
} ZXDH_AGENT_CHANNEL_ACL_MSG_T;

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
uint32_t zxdh_np_soft_res_uninstall(uint32_t dev_id);
uint32_t zxdh_np_stat_ppu_cnt_get_ex(uint32_t dev_id,
			ZXDH_STAT_CNT_MODE_E rd_mode,
			uint32_t index,
			uint32_t clr_mode,
			uint32_t *p_data);
uint32_t
zxdh_np_car_profile_id_add(uint32_t dev_id,
			uint32_t vport_id,
			uint32_t flags,
			uint64_t *p_profile_id);
uint32_t zxdh_np_car_profile_cfg_set(uint32_t dev_id,
			uint32_t vport_id,
			uint32_t car_type,
			uint32_t pkt_sign,
			uint32_t profile_id,
			void *p_car_profile_cfg);
uint32_t zxdh_np_car_profile_id_delete(uint32_t dev_id, uint32_t vport_id,
			uint32_t flags, uint64_t profile_id);
uint32_t zxdh_np_stat_car_queue_cfg_set(uint32_t dev_id,
			uint32_t car_type,
			uint32_t flow_id,
			uint32_t drop_flag,
			uint32_t plcr_en,
			uint32_t profile_id);
uint32_t zxdh_np_se_res_get_and_init(uint32_t dev_id, uint32_t type);
uint32_t zxdh_np_dtb_hash_online_delete(uint32_t dev_id, uint32_t queue_id, uint32_t sdt_no);
uint32_t zxdh_np_dtb_hash_offline_delete(uint32_t dev_id,
						uint32_t queue_id,
						uint32_t sdt_no,
						__rte_unused uint32_t flush_mode);
uint32_t zxdh_np_dtb_acl_index_request(uint32_t dev_id,
	uint32_t sdt_no, uint32_t vport, uint32_t *p_index);

uint32_t zxdh_np_dtb_acl_index_release(uint32_t dev_id,
	uint32_t sdt_no, uint32_t vport, uint32_t index);
uint32_t zxdh_np_dtb_acl_table_dump_by_vport(uint32_t dev_id, uint32_t queue_id,
	uint32_t sdt_no, uint32_t vport, uint32_t *entry_num, uint8_t *p_dump_data);
uint32_t zxdh_np_dtb_acl_offline_delete(uint32_t dev_id, uint32_t queue_id,
	uint32_t sdt_no, uint32_t vport, uint32_t counter_id, uint32_t rd_mode);
#endif /* ZXDH_NP_H */
