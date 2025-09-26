/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include <stdlib.h>
#include <string.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>

#include "zxdh_np.h"
#include "zxdh_logs.h"
#include "zxdh_msg.h"

static ZXDH_DEV_MGR_T g_dev_mgr;
static ZXDH_SDT_MGR_T g_sdt_mgr;
static uint32_t g_table_type[ZXDH_DEV_CHANNEL_MAX][ZXDH_DEV_SDT_ID_MAX];
static ZXDH_PPU_CLS_BITMAP_T g_ppu_cls_bit_map[ZXDH_DEV_CHANNEL_MAX];
static ZXDH_DTB_MGR_T *p_dpp_dtb_mgr[ZXDH_DEV_CHANNEL_MAX];
static ZXDH_SDT_TBL_DATA_T g_sdt_info[ZXDH_DEV_CHANNEL_MAX][ZXDH_DEV_SDT_ID_MAX];
static ZXDH_PPU_STAT_CFG_T g_ppu_stat_cfg[ZXDH_DEV_CHANNEL_MAX];
static SE_APT_CALLBACK_T g_apt_se_callback[ZXDH_DEV_CHANNEL_MAX][ZXDH_DEV_SDT_ID_MAX];
static ZXDH_ACL_CFG_EX_T g_apt_acl_cfg[ZXDH_DEV_CHANNEL_MAX] = {0};
static ZXDH_ACL_CFG_EX_T *g_p_acl_ex_cfg[ZXDH_DEV_CHANNEL_MAX] = {NULL};
static uint64_t g_np_fw_compat_addr[ZXDH_DEV_CHANNEL_MAX];
static const ZXDH_VERSION_COMPATIBLE_REG_T g_np_sdk_version = {
	ZXDH_NPSDK_COMPAT_ITEM_ID, 1, 0, 0, 0, {0} };
static const uint32_t hardware_ep_id[5] = {5, 6, 7, 8, 9};
static ZXDH_RB_CFG *g_dtb_dump_addr_rb[ZXDH_DEV_CHANNEL_MAX][ZXDH_DTB_QUEUE_NUM_MAX];
static const char * const g_dpp_dtb_name[] = {
	"DOWN TAB",
	"UP TAB",
};
static ZXDH_SE_CFG *g_apt_se_cfg[ZXDH_DEV_CHANNEL_MAX];
static ZXDH_SE_CFG *dpp_se_cfg[ZXDH_DEV_CHANNEL_MAX];
static const uint16_t g_lpm_crc[ZXDH_SE_ZBLK_NUM] = {
	0x1021, 0x8005, 0x3D65, 0xab47, 0x3453, 0x0357, 0x0589, 0xa02b,
	0x1021, 0x8005, 0x3D65, 0xab47, 0x3453, 0x0357, 0x0589, 0xa02b,
	0x1021, 0x8005, 0x3D65, 0xab47, 0x3453, 0x0357, 0x0589, 0xa02b,
	0x1021, 0x8005, 0x3D65, 0xab47, 0x3453, 0x0357, 0x0589, 0xa02b
};
static uint32_t g_hash_zblk_idx[ZXDH_DEV_CHANNEL_MAX]
				[ZXDH_HASH_FUNC_ID_NUM]
				[ZXDH_HASH_TBL_ID_NUM] = {0};
static const uint32_t g_ddr_hash_arg[ZXDH_HASH_DDR_CRC_NUM] = {
	0x04C11DB7,
	0xF4ACFB13,
	0x20044009,
	0x00210801
};

static ZXDH_HASH_TBL_ID_INFO g_tbl_id_info[ZXDH_DEV_CHANNEL_MAX]
							 [ZXDH_HASH_FUNC_ID_NUM]
							 [ZXDH_HASH_TBL_ID_NUM];

static const ZXDH_FIELD_T g_smmu0_smmu0_cpu_ind_cmd_reg[] = {
	{"cpu_ind_rw", ZXDH_FIELD_FLAG_RW, 31, 1, 0x0, 0x0},
	{"cpu_ind_rd_mode", ZXDH_FIELD_FLAG_RW, 30, 1, 0x0, 0x0},
	{"cpu_req_mode", ZXDH_FIELD_FLAG_RW, 27, 2, 0x0, 0x0},
	{"cpu_ind_addr", ZXDH_FIELD_FLAG_RW, 25, 26, 0x0, 0x0},
};

static const ZXDH_FIELD_T g_smmu0_smmu0_cpu_ind_rd_done_reg[] = {
	{"cpu_ind_rd_done", ZXDH_FIELD_FLAG_RO, 0, 1, 0x0, 0x0},
};

static const ZXDH_FIELD_T g_smmu0_smmu0_cpu_ind_rdat0_reg[] = {
	{"cpu_ind_rdat0", ZXDH_FIELD_FLAG_RO, 31, 32, 0x0, 0x0},
};

static const ZXDH_FIELD_T g_smmu0_smmu0_cpu_ind_rdat1_reg[] = {
	{"cpu_ind_rdat1", ZXDH_FIELD_FLAG_RO, 31, 32, 0x0, 0x0},
};

static const ZXDH_FIELD_T g_smmu0_smmu0_cpu_ind_rdat2_reg[] = {
	{"cpu_ind_rdat2", ZXDH_FIELD_FLAG_RO, 31, 32, 0x0, 0x0},
};

static const ZXDH_FIELD_T g_smmu0_smmu0_cpu_ind_rdat3_reg[] = {
	{"cpu_ind_rdat3", ZXDH_FIELD_FLAG_RO, 31, 32, 0x0, 0x0},
};

static const ZXDH_FIELD_T g_smmu0_smmu0_wr_arb_cpu_rdy_reg[] = {
	{"wr_arb_cpu_rdy", ZXDH_FIELD_FLAG_RO, 0, 1, 0x1, 0x0},
};

static const ZXDH_FIELD_T g_dtb4k_dtb_enq_info_queue_buf_space_left_0_127_reg[] = {
	{"info_queue_buf_space_left", ZXDH_FIELD_FLAG_RO, 5, 6, 0x20, 0x0},
};

static const ZXDH_FIELD_T g_dtb4k_dtb_enq_cfg_epid_v_func_num_0_127_reg[] = {
	{"dbi_en", ZXDH_FIELD_FLAG_RW, 31, 1, 0x0, 0x0},
	{"queue_en", ZXDH_FIELD_FLAG_RW, 30, 1, 0x0, 0x0},
	{"cfg_epid", ZXDH_FIELD_FLAG_RW, 27, 4, 0x0, 0x0},
	{"cfg_vfunc_num", ZXDH_FIELD_FLAG_RW, 23, 8, 0x0, 0x0},
	{"cfg_vector", ZXDH_FIELD_FLAG_RW, 14, 7, 0x0, 0x0},
	{"cfg_func_num", ZXDH_FIELD_FLAG_RW, 7, 3, 0x0, 0x0},
	{"cfg_vfunc_active", ZXDH_FIELD_FLAG_RW, 0, 1, 0x0, 0x0},
};

static const ZXDH_DTB_FIELD_T g_dtb_ddr_table_cmd_info[] = {
	{"valid", 127, 1},
	{"type_mode", 126, 3},
	{"rw_len", 123, 2},
	{"v46_flag", 121, 1},
	{"lpm_wr_vld", 120, 1},
	{"baddr", 119, 20},
	{"ecc_en", 99, 1},
	{"rw_addr", 29, 30},
};

static const ZXDH_DTB_FIELD_T g_dtb_eram_table_cmd_1_info[] = {
	{"valid", 127, 1},
	{"type_mode", 126, 3},
	{"data_mode", 123, 2},
	{"cpu_wr", 121, 1},
	{"cpu_rd", 120, 1},
	{"cpu_rd_mode", 119, 1},
	{"addr", 113, 26},
	{"data_h", 0, 1},
};

static const ZXDH_DTB_FIELD_T g_dtb_eram_table_cmd_64_info[] = {
	{"valid", 127, 1},
	{"type_mode", 126, 3},
	{"data_mode", 123, 2},
	{"cpu_wr", 121, 1},
	{"cpu_rd", 120, 1},
	{"cpu_rd_mode", 119, 1},
	{"addr", 113, 26},
	{"data_h", 63, 32},
	{"data_l", 31, 32},
};

static const ZXDH_DTB_FIELD_T g_dtb_eram_table_cmd_128_info[] = {
	{"valid", 127, 1},
	{"type_mode", 126, 3},
	{"data_mode", 123, 2},
	{"cpu_wr", 121, 1},
	{"cpu_rd", 120, 1},
	{"cpu_rd_mode", 119, 1},
	{"addr", 113, 26},
};

static const ZXDH_DTB_FIELD_T g_dtb_zcam_table_cmd_info[] = {
	{"valid", 127, 1},
	{"type_mode", 126, 3},
	{"ram_reg_flag", 123, 1},
	{"zgroup_id", 122, 2},
	{"zblock_id", 120, 3},
	{"zcell_id", 117, 2},
	{"mask", 115, 4},
	{"sram_addr", 111, 9},
};

static const ZXDH_DTB_FIELD_T g_dtb_etcam_table_cmd_info[] = {
	{"valid", 127, 1},
	{"type_mode", 126, 3},
	{"block_sel", 123, 3},
	{"init_en", 120, 1},
	{"row_or_col_msk", 119, 1},
	{"vben", 118, 1},
	{"reg_tcam_flag", 117, 1},
	{"uload", 116, 8},
	{"rd_wr", 108, 1},
	{"wr_mode", 107, 8},
	{"data_or_mask", 99, 1},
	{"addr", 98, 9},
	{"vbit", 89, 8},
};

static const ZXDH_DTB_FIELD_T g_dtb_mc_hash_table_cmd_info[] = {
	{"valid", 127, 1},
	{"type_mode", 126, 3},
	{"std_h", 63, 32},
	{"std_l", 31, 32},
};

static const ZXDH_DTB_TABLE_T g_dpp_dtb_table_info[] = {
	{
		"ddr",
		ZXDH_DTB_TABLE_DDR,
		8,
		g_dtb_ddr_table_cmd_info,
	},
	{
		"eram 1 bit",
		ZXDH_DTB_TABLE_ERAM_1,
		8,
		g_dtb_eram_table_cmd_1_info,
	},
	{
		"eram 64 bit",
		ZXDH_DTB_TABLE_ERAM_64,
		9,
		g_dtb_eram_table_cmd_64_info,
	},
	{
		"eram 128 bit",
		ZXDH_DTB_TABLE_ERAM_128,
		7,
		g_dtb_eram_table_cmd_128_info,
	},
	{
		"zcam",
		ZXDH_DTB_TABLE_ZCAM,
		8,
		g_dtb_zcam_table_cmd_info,
	},
	{
		"etcam",
		ZXDH_DTB_TABLE_ETCAM,
		13,
		g_dtb_etcam_table_cmd_info,
	},
	{
		"mc_hash",
		ZXDH_DTB_TABLE_MC_HASH,
		4,
		g_dtb_mc_hash_table_cmd_info
	},
};

static const ZXDH_DTB_FIELD_T g_dtb_eram_dump_cmd_info[] = {
	{"valid", 127, 1},
	{"up_type", 126, 2},
	{"base_addr", 106, 19},
	{"tb_depth", 83, 20},
	{"tb_dst_addr_h", 63, 32},
	{"tb_dst_addr_l", 31, 32},
};

static const ZXDH_DTB_FIELD_T g_dtb_ddr_dump_cmd_info[] = {
	{"valid", 127, 1},
	{"up_type", 126, 2},
	{"base_addr", 117, 30},
	{"tb_depth", 83, 20},
	{"tb_dst_addr_h", 63, 32},
	{"tb_dst_addr_l", 31, 32},

};

static const ZXDH_DTB_FIELD_T g_dtb_zcam_dump_cmd_info[] = {
	{"valid", 127, 1},
	{"up_type", 126, 2},
	{"zgroup_id", 124, 2},
	{"zblock_id", 122, 3},
	{"ram_reg_flag", 119, 1},
	{"z_reg_cell_id", 118, 2},
	{"sram_addr", 116, 9},
	{"tb_depth", 97, 10},
	{"tb_width", 65, 2},
	{"tb_dst_addr_h", 63, 32},
	{"tb_dst_addr_l", 31, 32},

};

static const ZXDH_DTB_FIELD_T g_dtb_etcam_dump_cmd_info[] = {
	{"valid", 127, 1},
	{"up_type", 126, 2},
	{"block_sel", 124, 3},
	{"addr", 121, 9},
	{"rd_mode", 112, 8},
	{"data_or_mask", 104, 1},
	{"tb_depth", 91, 10},
	{"tb_width", 81, 2},
	{"tb_dst_addr_h", 63, 32},
	{"tb_dst_addr_l", 31, 32},

};

static const ZXDH_DTB_TABLE_T g_dpp_dtb_dump_info[] = {
	{
		"eram",
		ZXDH_DTB_DUMP_ERAM,
		6,
		g_dtb_eram_dump_cmd_info,
	},
	{
		"ddr",
		ZXDH_DTB_DUMP_DDR,
		6,
		g_dtb_ddr_dump_cmd_info,
	},
	{
		"zcam",
		ZXDH_DTB_DUMP_ZCAM,
		11,
		g_dtb_zcam_dump_cmd_info,
	},
	{
		"etcam",
		ZXDH_DTB_DUMP_ETCAM,
		10,
		g_dtb_etcam_dump_cmd_info,
	},
};

#define ZXDH_SDT_MGR_PTR_GET()    (&g_sdt_mgr)
#define ZXDH_SDT_SOFT_TBL_GET(id) (g_sdt_mgr.sdt_tbl_array[ZXDH_DEV_SLOT_ID(id)])
#define ZXDH_DEV_INFO_GET(id) (g_dev_mgr.p_dev_array[ZXDH_DEV_SLOT_ID(id)])

#define ZXDH_DTB_LEN(cmd_type, int_en, data_len) \
	(((data_len) & 0x3ff) | \
	((int_en) << 29) | \
	((cmd_type) << 30))

static inline uint32_t
zxdh_np_comm_mask_bit(uint32_t bitnum) {
	return (uint32_t)(0x1U << bitnum);
}

static inline uint32_t
zxdh_np_comm_get_bit_mask(uint32_t bit_quantity) {
	if (bit_quantity < 32)
		return zxdh_np_comm_mask_bit(bit_quantity & 0x1F) - 1;
	else
		return 0xFFFFFFFF;
}

#define ZXDH_COMM_UINT32_GET_BITS(_uidst_, _uisrc_, _uistartpos_, _uilen_)\
	((_uidst_) = (((_uisrc_) >> (_uistartpos_)) & \
	(zxdh_np_comm_get_bit_mask((_uilen_)))))

#define ZXDH_REG_DATA_MAX      (128)

#define ZXDH_COMM_CHECK_DEV_POINT(dev_id, point)\
do {\
	if (NULL == (point)) {\
		PMD_DRV_LOG(ERR, "dev: %u [POINT NULL]", (dev_id));\
		RTE_ASSERT(0);\
	} \
} while (0)

#define ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, becall)\
do {\
	if ((rc) != 0) {\
		PMD_DRV_LOG(ERR, "dev: %u, %s failed!", (dev_id), becall);\
		RTE_ASSERT(0);\
	} \
} while (0)

#define ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, becall)\
do {\
	if ((rc) != 0) {\
		PMD_DRV_LOG(ERR, "%s failed!", becall);\
	} \
} while (0)

#define ZXDH_COMM_CHECK_RC(rc, becall)\
do {\
	if ((rc) != 0) {\
		PMD_DRV_LOG(ERR, "%s failed!", becall);\
		RTE_ASSERT(0);\
	} \
} while (0)

#define ZXDH_COMM_CHECK_POINT(point)\
do {\
	if ((point) == NULL) {\
		PMD_DRV_LOG(ERR, "[POINT NULL]");\
		RTE_ASSERT(0);\
	} \
} while (0)

static inline uint16_t zxdh_np_comm_convert16(uint16_t w_data)
{
	return ((w_data) & 0xff) << 8 | ((w_data) & 0xff00) >> 8;
}

static inline uint32_t
zxdh_np_comm_convert32(uint32_t dw_data)
{
	return ((dw_data) & 0xff) << 24 | ((dw_data) & 0xff00) << 8 |
		((dw_data) & 0xff0000) >> 8 | ((dw_data) & 0xff000000) >> 24;
}

#define ZXDH_COMM_CONVERT16(w_data) \
			zxdh_np_comm_convert16(w_data)

#define ZXDH_COMM_CONVERT32(w_data) \
			zxdh_np_comm_convert32(w_data)

#define ZXDH_DTB_TAB_UP_WR_INDEX_GET(DEV_ID, QUEUE_ID) \
		(p_dpp_dtb_mgr[(ZXDH_DEV_SLOT_ID(DEV_ID))]->queue_info[(QUEUE_ID)].tab_up.wr_index)

#define ZXDH_DTB_TAB_UP_USER_PHY_ADDR_FLAG_GET(DEV_ID, QUEUE_ID, INDEX)    \
		(p_dpp_dtb_mgr[(ZXDH_DEV_SLOT_ID(DEV_ID))]->                       \
		queue_info[(QUEUE_ID)].tab_up.user_addr[(INDEX)].user_flag)

#define ZXDH_DTB_TAB_UP_USER_PHY_ADDR_GET(DEV_ID, QUEUE_ID, INDEX)         \
		(p_dpp_dtb_mgr[(ZXDH_DEV_SLOT_ID(DEV_ID))]->                       \
		queue_info[(QUEUE_ID)].tab_up.user_addr[(INDEX)].phy_addr)

#define ZXDH_DTB_TAB_UP_DATA_LEN_GET(DEV_ID, QUEUE_ID, INDEX)              \
		(p_dpp_dtb_mgr[(ZXDH_DEV_SLOT_ID(DEV_ID))]->                       \
		queue_info[(QUEUE_ID)].tab_up.data_len[(INDEX)])

#define ZXDH_DTB_TAB_UP_VIR_ADDR_GET(DEV_ID, QUEUE_ID, INDEX)   \
		(p_dpp_dtb_mgr[(ZXDH_DEV_SLOT_ID(DEV_ID))]->            \
		queue_info[(QUEUE_ID)].tab_up.start_vir_addr +          \
		(INDEX) * p_dpp_dtb_mgr[(ZXDH_DEV_SLOT_ID(DEV_ID))]->   \
		queue_info[(QUEUE_ID)].tab_up.item_size)

#define ZXDH_DTB_TAB_DOWN_VIR_ADDR_GET(DEV_ID, QUEUE_ID, INDEX) \
		(p_dpp_dtb_mgr[ZXDH_DEV_SLOT_ID(DEV_ID)]->              \
		queue_info[QUEUE_ID].tab_down.start_vir_addr +          \
		(INDEX) * p_dpp_dtb_mgr[ZXDH_DEV_SLOT_ID(DEV_ID)]->     \
		queue_info[QUEUE_ID].tab_down.item_size)

#define ZXDH_DTB_TAB_DOWN_WR_INDEX_GET(DEV_ID, QUEUE_ID)        \
		(p_dpp_dtb_mgr[(ZXDH_DEV_SLOT_ID(DEV_ID))]->            \
		queue_info[(QUEUE_ID)].tab_down.wr_index)

#define ZXDH_DTB_QUEUE_INIT_FLAG_GET(DEV_ID, QUEUE_ID)          \
		(p_dpp_dtb_mgr[(ZXDH_DEV_SLOT_ID(DEV_ID))]->            \
		queue_info[(QUEUE_ID)].init_flag)

#define ZXDH_DTB_TAB_UP_USER_VIR_ADDR_GET(DEV_ID, QUEUE_ID, INDEX)          \
		(p_dpp_dtb_mgr[ZXDH_DEV_SLOT_ID(DEV_ID)]->                          \
		queue_info[QUEUE_ID].tab_up.user_addr[INDEX].vir_addr)

#define ZXDH_DTB_TAB_UP_USER_ADDR_FLAG_SET(DEV_ID, QUEUE_ID, INDEX, VAL)    \
		(p_dpp_dtb_mgr[ZXDH_DEV_SLOT_ID(DEV_ID)]->                          \
		queue_info[QUEUE_ID].tab_up.user_addr[INDEX].user_flag = VAL)

static inline uint64_t
zxdh_np_dtb_tab_down_phy_addr_get(uint32_t DEV_ID, uint32_t QUEUE_ID,
	uint32_t INDEX)
{
	return p_dpp_dtb_mgr[ZXDH_DEV_SLOT_ID(DEV_ID)]->queue_info[QUEUE_ID].tab_down.start_phy_addr
	+ INDEX * p_dpp_dtb_mgr[ZXDH_DEV_SLOT_ID(DEV_ID)]->queue_info[QUEUE_ID].tab_down.item_size;
}

#define ZXDH_DTB_TAB_DOWN_PHY_ADDR_GET(DEV_ID, QUEUE_ID, INDEX)   \
	zxdh_np_dtb_tab_down_phy_addr_get(DEV_ID, QUEUE_ID, INDEX)

static inline uint64_t
zxdh_np_dtb_tab_up_phy_addr_get(uint32_t DEV_ID, uint32_t QUEUE_ID,
	uint32_t INDEX)
{
	return p_dpp_dtb_mgr[ZXDH_DEV_SLOT_ID(DEV_ID)]->queue_info[QUEUE_ID].tab_up.start_phy_addr
	+ INDEX * p_dpp_dtb_mgr[ZXDH_DEV_SLOT_ID(DEV_ID)]->queue_info[QUEUE_ID].tab_up.item_size;
}

#define ZXDH_DTB_TAB_UP_PHY_ADDR_GET(DEV_ID, QUEUE_ID, INDEX)	 \
	zxdh_np_dtb_tab_up_phy_addr_get(DEV_ID, QUEUE_ID, INDEX)

static inline void
zxdh_np_init_d_node(ZXDH_D_NODE *ptr, void *pdata)
{
	ptr->data = pdata;
	ptr->prev = NULL;
	ptr->next = NULL;
}

static inline void
zxdh_np_init_rbt_tn(ZXDH_RB_TN *p_tn, void *p_newkey)
{
	p_tn->p_key = p_newkey;
	p_tn->color_lsv = 0;
	p_tn->p_left = NULL;
	p_tn->p_right = NULL;
	p_tn->p_parent = NULL;
	zxdh_np_init_d_node(&p_tn->tn_ln, p_tn);
}

static inline uint32_t
zxdh_np_get_tn_lsv(ZXDH_RB_TN *p_tn)
{
	return p_tn->color_lsv >> 2;
}

static inline void
zxdh_np_set_tn_lsv(ZXDH_RB_TN *p_tn, uint32_t list_val)
{
	p_tn->color_lsv &= 0x3;
	p_tn->color_lsv |= (list_val << 2);
}

static inline void
zxdh_np_set_tn_color(ZXDH_RB_TN *p_tn, uint32_t color)
{
	p_tn->color_lsv &= 0xfffffffc;
	p_tn->color_lsv |= (color & 0x3);
}

static inline uint32_t
zxdh_np_get_tn_color(ZXDH_RB_TN *p_tn)
{
	return ((p_tn == NULL) ? ZXDH_RBT_BLACK : (p_tn)->color_lsv & 0x3);
}

#define ZXDH_SE_GET_ZBLK_CFG(p_se, zblk_idx) \
	(&(((ZXDH_SE_CFG *)(p_se))->zblk_info[zblk_idx]))

#define GET_ZBLK_IDX(zcell_idx) \
	(((zcell_idx) & 0x7F) >> 2)

#define GET_ZCELL_IDX(zcell_idx) \
	((zcell_idx) & 0x3)

#define ZXDH_GET_FUN_INFO(p_se, fun_id) \
	(&(((ZXDH_SE_CFG *)(p_se))->fun_info[fun_id]))

#define GET_DDR_HASH_ARG(ddr_crc_sel) (g_ddr_hash_arg[ddr_crc_sel])

#define ZXDH_SDT_GET_LOW_DATA(source_value, low_width) \
	((source_value) & ((1 << (low_width)) - 1))

#define ZXDH_ACL_AS_RSLT_SIZE_GET_EX(mode) (2U << (mode))

#define ZXDH_GET_ACTU_KEY_BY_SIZE(actu_key_size) \
	((actu_key_size) * ZXDH_HASH_ACTU_KEY_STEP)

#define ZXDH_GET_KEY_SIZE(actu_key_size) \
	(ZXDH_GET_ACTU_KEY_BY_SIZE(actu_key_size) + ZXDH_HASH_KEY_CTR_SIZE)

#define ZXDH_ACL_ENTRY_MAX_GET(key_mode, block_num) \
	((block_num) * ZXDH_ETCAM_RAM_DEPTH * (1U << (key_mode)))

#define ZXDH_ETCAM_ENTRY_SIZE_GET(entry_mode) \
	((ZXDH_ETCAM_RAM_WIDTH << (3 - (entry_mode))) / 8)

#define ZXDH_ACL_KEYSIZE_GET(key_mode) (2 * ZXDH_ETCAM_ENTRY_SIZE_GET(key_mode))

#define GET_HASH_TBL_ID_INFO(dev_id, fun_id, tbl_id) \
	(&g_tbl_id_info[ZXDH_DEV_SLOT_ID(dev_id)][fun_id][tbl_id])

#define ZXDH_GET_HASH_TBL_ID(p_key)    ((p_key)[0] & 0x1F)

static inline uint32_t
zxdh_np_get_hash_entry_size(uint32_t key_type)
{
	return ((key_type == ZXDH_HASH_KEY_128b) ? 16U : ((key_type == ZXDH_HASH_KEY_256b) ? 32U :
		 ((key_type == ZXDH_HASH_KEY_512b) ? 64U : 0)));
}

#define ZXDH_GET_HASH_ENTRY_SIZE(key_type) \
	zxdh_np_get_hash_entry_size(key_type)

static inline void
zxdh_np_comm_uint32_write_bits(uint32_t *dst, uint32_t src,
	uint32_t start_pos, uint32_t len)
{
	uint32_t mask = zxdh_np_comm_get_bit_mask(len);
	*dst = (*dst & ~(mask << start_pos)) | ((src & mask) << start_pos);
}

#define ZXDH_COMM_UINT32_WRITE_BITS(dst, src, start_pos, len)\
	zxdh_np_comm_uint32_write_bits(&(dst), src, start_pos, len)

static inline uint32_t
zxdh_np_zblk_addr_conv(uint32_t zblk_idx)
{
	uint32_t group_size = 1 << ZXDH_ZBLK_IDX_BT_WIDTH;
	uint32_t group_number = zblk_idx / ZXDH_ZBLK_NUM_PER_ZGRP;
	uint32_t offset_in_group = zblk_idx % ZXDH_ZBLK_NUM_PER_ZGRP;

	return group_number * group_size + offset_in_group;
}

static inline uint32_t
zxdh_np_zcell_addr_conv(uint32_t zcell_idx)
{
	uint32_t blk_grp_idx = (zcell_idx >> ZXDH_ZCELL_IDX_BT_WIDTH) &
			((1 << (ZXDH_ZBLK_IDX_BT_WIDTH + ZXDH_ZGRP_IDX_BT_WIDTH)) - 1);

	uint32_t cell_idx = zcell_idx & ((1 << ZXDH_ZCELL_IDX_BT_WIDTH) - 1);

	return (zxdh_np_zblk_addr_conv(blk_grp_idx) << ZXDH_ZCELL_IDX_BT_WIDTH) | cell_idx;
}

#define ZXDH_ZBLK_REG_ADDR_CALC(zblk_idx, offset) \
	((0xF << ZXDH_ZBLK_WRT_MASK_BT_START) | \
	(0x1 << ZXDH_REG_SRAM_FLAG_BT_START) | \
	((zxdh_np_zblk_addr_conv(zblk_idx) & 0x1F) << ZXDH_ZBLK_IDX_BT_START) | \
	((offset) & 0x1FF))

#define ZXDH_ZBLK_HASH_LIST_REG_ADDR_CALC(zblk_idx, reg_idx) \
	(ZXDH_ZBLK_REG_ADDR_CALC((zblk_idx), (0xD + (reg_idx))))

#define ZXDH_ZCELL_BASE_ADDR_CALC(zcell_idx) \
	((0xF << ZXDH_ZBLK_WRT_MASK_BT_START) | (((zxdh_np_zcell_addr_conv(zcell_idx)) & \
	((1 << (ZXDH_ZCELL_IDX_BT_WIDTH + ZXDH_ZBLK_IDX_BT_WIDTH + ZXDH_ZGRP_IDX_BT_WIDTH)) - 1)) \
	<< ZXDH_ZCELL_ADDR_BT_WIDTH))
#define ZXDH_ZBLK_ITEM_ADDR_CALC(zcell_idx, item_idx) \
	((ZXDH_ZCELL_BASE_ADDR_CALC(zcell_idx)) | ((item_idx) & (ZXDH_SE_RAM_DEPTH - 1)))

static inline uint32_t
zxdh_np_get_rst_size(uint32_t key_type, uint32_t actu_key_size)
{
	return ((ZXDH_GET_HASH_ENTRY_SIZE(key_type) != 0) ?
		(ZXDH_GET_HASH_ENTRY_SIZE(key_type) - ZXDH_GET_ACTU_KEY_BY_SIZE(actu_key_size) -
		ZXDH_HASH_KEY_CTR_SIZE) : 0xFF);
}

#define ZXDH_GET_RST_SIZE(key_type, actu_key_size) \
	zxdh_np_get_rst_size(key_type, actu_key_size)

#define ZXDH_GET_HASH_KEY_TYPE(p_key)  (((p_key)[0] >> 5) & 0x3)

#define ZXDH_GET_HASH_KEY_VALID(p_key) (((p_key)[0] >> 7) & 0x1)

#define ZXDH_GET_HASH_KEY_CTRL(valid, type, tbl_id) \
	((((valid) & 0x1) << 7) | (((type) & 0x3) << 5) | ((tbl_id) & 0x1f))

static inline uint32_t
zxdh_np_get_hash_entry_mask(uint32_t entry_size, uint32_t entry_pos)
{
	return (((1U << (entry_size / 16U)) - 1U) << (4U - entry_size / 16U - entry_pos)) & 0xF;
}

#define ZXDH_GET_HASH_ENTRY_MASK(entry_size, entry_pos) \
	zxdh_np_get_hash_entry_mask(entry_size, entry_pos)

#define GET_HASH_DDR_HW_ADDR(base_addr, item_idx) \
	((base_addr) + (item_idx))

#define GET_ZCELL_CRC_VAL(zcell_id, crc16_val) \
	(((crc16_val) >> (zcell_id)) & (ZXDH_SE_RAM_DEPTH - 1))

#define ZXDH_COMM_DM_TO_X(d, m)        ((d) & ~(m))
#define ZXDH_COMM_DM_TO_Y(d, m)        (~(d) & ~(m))
#define ZXDH_COMM_XY_TO_MASK(x, y)     (~(x) & ~(y))
#define ZXDH_COMM_XY_TO_DATA(x, y)     (x)

static ZXDH_FIELD_T g_stat_car0_cara_queue_ram0_159_0_reg[] = {
	{"cara_drop", ZXDH_FIELD_FLAG_RW, 147, 1, 0x0, 0x0},
	{"cara_plcr_en", ZXDH_FIELD_FLAG_RW, 146, 1, 0x0, 0x0},
	{"cara_profile_id", ZXDH_FIELD_FLAG_RW, 145, 9, 0x0, 0x0},
	{"cara_tq_h", ZXDH_FIELD_FLAG_RO, 136, 13, 0x0, 0x0},
	{"cara_tq_l", ZXDH_FIELD_FLAG_RO, 123, 32, 0x0, 0x0},
	{"cara_ted", ZXDH_FIELD_FLAG_RO, 91, 19, 0x0, 0x0},
	{"cara_tcd", ZXDH_FIELD_FLAG_RO, 72, 19, 0x0, 0x0},
	{"cara_tei", ZXDH_FIELD_FLAG_RO, 53, 27, 0x0, 0x0},
	{"cara_tci", ZXDH_FIELD_FLAG_RO, 26, 27, 0x0, 0x0},
};

static ZXDH_FIELD_T g_stat_car0_carb_queue_ram0_159_0_reg[] = {
	{"carb_drop", ZXDH_FIELD_FLAG_RW, 147, 1, 0x0, 0x0},
	{"carb_plcr_en", ZXDH_FIELD_FLAG_RW, 146, 1, 0x0, 0x0},
	{"carb_profile_id", ZXDH_FIELD_FLAG_RW, 145, 9, 0x0, 0x0},
	{"carb_tq_h", ZXDH_FIELD_FLAG_RO, 136, 13, 0x0, 0x0},
	{"carb_tq_l", ZXDH_FIELD_FLAG_RO, 123, 32, 0x0, 0x0},
	{"carb_ted", ZXDH_FIELD_FLAG_RO, 91, 19, 0x0, 0x0},
	{"carb_tcd", ZXDH_FIELD_FLAG_RO, 72, 19, 0x0, 0x0},
	{"carb_tei", ZXDH_FIELD_FLAG_RO, 53, 27, 0x0, 0x0},
	{"carb_tci", ZXDH_FIELD_FLAG_RO, 26, 27, 0x0, 0x0},
};

static ZXDH_FIELD_T g_stat_car0_carc_queue_ram0_159_0_reg[] = {
	{"carc_drop", ZXDH_FIELD_FLAG_RW, 147, 1, 0x0, 0x0},
	{"carc_plcr_en", ZXDH_FIELD_FLAG_RW, 146, 1, 0x0, 0x0},
	{"carc_profile_id", ZXDH_FIELD_FLAG_RW, 145, 9, 0x0, 0x0},
	{"carc_tq_h", ZXDH_FIELD_FLAG_RO, 136, 13, 0x0, 0x0},
	{"carc_tq_l", ZXDH_FIELD_FLAG_RO, 123, 32, 0x0, 0x0},
	{"carc_ted", ZXDH_FIELD_FLAG_RO, 91, 19, 0x0, 0x0},
	{"carc_tcd", ZXDH_FIELD_FLAG_RO, 72, 19, 0x0, 0x0},
	{"carc_tei", ZXDH_FIELD_FLAG_RO, 53, 27, 0x0, 0x0},
	{"carc_tci", ZXDH_FIELD_FLAG_RO, 26, 27, 0x0, 0x0},
};

static ZXDH_FIELD_T g_nppu_pktrx_cfg_pktrx_glbal_cfg_0_reg[] = {
	{"pktrx_glbal_cfg_0", ZXDH_FIELD_FLAG_RW, 31, 32, 0x0, 0x0},
};

static uint32_t zxdh_dtb_info_print(uint32_t dev_id,
						uint32_t queue_id,
						uint32_t item_index,
						ZXDH_DTB_QUEUE_ITEM_INFO_T *item_info)
{
	uint64_t element_start_addr = 0;
	uint64_t ack_start_addr = 0;
	uint64_t data_addr = 0;
	uint32_t data = 0;
	uint32_t i = 0;
	uint32_t j = 0;

	PMD_DRV_LOG(DEBUG, "queue: %u, element:%u,  %s table info is:",
				queue_id, item_index, (item_info->cmd_type) ? "up" : "down");
	PMD_DRV_LOG(DEBUG, "cmd_vld    : %u", item_info->cmd_vld);
	PMD_DRV_LOG(DEBUG, "cmd_type   : %s", (item_info->cmd_type) ? "up" : "down");
	PMD_DRV_LOG(DEBUG, "int_en     : %u", item_info->int_en);
	PMD_DRV_LOG(DEBUG, "data_len   : %u", item_info->data_len);
	PMD_DRV_LOG(DEBUG, "data_hddr  : 0x%x", item_info->data_hddr);
	PMD_DRV_LOG(DEBUG, "data_laddr : 0x%x", item_info->data_laddr);

	if (item_info->cmd_type == ZXDH_DTB_DIR_UP_TYPE) {
		if (ZXDH_DTB_TAB_UP_USER_PHY_ADDR_FLAG_GET(dev_id, queue_id, item_index) ==
		ZXDH_DTB_TAB_UP_USER_ADDR_TYPE) {
			ack_start_addr =
			ZXDH_DTB_TAB_UP_USER_VIR_ADDR_GET(dev_id, queue_id, item_index);
		}
		ack_start_addr = ZXDH_DTB_TAB_UP_VIR_ADDR_GET(dev_id, queue_id, item_index);
		element_start_addr =
		ZXDH_DTB_TAB_UP_VIR_ADDR_GET(dev_id, queue_id, item_index) + ZXDH_DTB_ITEM_ACK_SIZE;
	} else {
		ack_start_addr = ZXDH_DTB_TAB_DOWN_VIR_ADDR_GET(dev_id, queue_id, item_index);
		element_start_addr =
		ZXDH_DTB_TAB_DOWN_VIR_ADDR_GET(dev_id, queue_id, item_index)
		+ ZXDH_DTB_ITEM_ACK_SIZE;
	}
	PMD_DRV_LOG(DEBUG, "dtb data:");

	PMD_DRV_LOG(DEBUG, "ack info:");
	for (j = 0; j < 4; j++) {
		data  = ZXDH_COMM_CONVERT32(*((uint32_t *)(ack_start_addr + 4 * j)));
		PMD_DRV_LOG(DEBUG, "0x%08x  ", data);
	}

	for (i = 0; i < item_info->data_len; i++) {
		data_addr = element_start_addr + 16 * i;

		PMD_DRV_LOG(DEBUG, "row_%u:", i);

		for (j = 0; j < 4; j++) {
			data  = ZXDH_COMM_CONVERT32(*((uint32_t *)(data_addr + 4 * j)));
			PMD_DRV_LOG(DEBUG, "0x%08x  ", data);
		}
	}

	PMD_DRV_LOG(DEBUG, "zxdh dtb info print end.");
	return ZXDH_OK;
}

static uint32_t
zxdh_np_comm_is_big_endian(void)
{
	ZXDH_ENDIAN_U c_data;

	c_data.a = 1;

	if (c_data.b == 1)
		return 0;
	else
		return 1;
}

static void
zxdh_np_comm_swap(uint8_t *p_uc_data, uint32_t dw_byte_len)
{
	uint16_t *p_w_tmp = NULL;
	uint32_t *p_dw_tmp = NULL;
	uint32_t dw_byte_num;
	uint8_t uc_byte_mode;
	uint32_t uc_is_big_flag;
	uint32_t i;

	p_dw_tmp = (uint32_t *)(p_uc_data);
	uc_is_big_flag = zxdh_np_comm_is_big_endian();
	if (uc_is_big_flag)
		return;

	dw_byte_num  = dw_byte_len >> 2;
	uc_byte_mode = dw_byte_len % 4 & 0xff;

	for (i = 0; i < dw_byte_num; i++) {
		(*p_dw_tmp) = ZXDH_COMM_CONVERT32(*p_dw_tmp);
		p_dw_tmp++;
	}

	if (uc_byte_mode > 1) {
		p_w_tmp = (uint16_t *)(p_dw_tmp);
		(*p_w_tmp) = ZXDH_COMM_CONVERT16(*p_w_tmp);
	}
}

static uint32_t
zxdh_comm_double_link_init(uint32_t elmemtnum, ZXDH_D_HEAD *p_head)
{
	uint32_t err_code = 0;

	if (elmemtnum == 0) {
		err_code = ZXDH_DOUBLE_LINK_INIT_ELEMENT_NUM_ERR;
		PMD_DRV_LOG(ERR, "Error:[0x%x] doule_link_init Element Num Err !",
			err_code);
		return err_code;
	}

	p_head->maxnum   = elmemtnum;
	p_head->used	 = 0;
	p_head->p_next   = NULL;
	p_head->p_prev   = NULL;

	return ZXDH_OK;
}

static uint32_t
zxdh_np_comm_liststack_create(uint32_t element_num, ZXDH_LISTSTACK_MANAGER **p_list)
{
	ZXDH_LISTSTACK_MANAGER *p_local_list = NULL;
	uint32_t dw_list_size = 0;
	uint32_t dw_manage_size = 0;
	uint32_t dw_actual_element_num = 0;
	uint32_t i = 0;

	if (p_list == NULL) {
		PMD_DRV_LOG(ERR, " p_list is NULL!");
		return ZXDH_LIST_STACK_POINT_NULL;
	}
	if (element_num <= 0) {
		*p_list = NULL;
		PMD_DRV_LOG(ERR, " FtmComm_ListStackCreat_dwElementNum <=0");
		return ZXDH_LIST_STACK_ELEMENT_NUM_ERR;
	}

	if (element_num > ZXDH_LISTSTACK_MAX_ELEMENT - 1)
		dw_actual_element_num = ZXDH_LISTSTACK_MAX_ELEMENT;
	else
		dw_actual_element_num = element_num + 1;

	dw_list_size = (dw_actual_element_num * sizeof(ZXDH_COMM_FREELINK)) & 0xffffffff;
	dw_manage_size = ((sizeof(ZXDH_LISTSTACK_MANAGER) & 0xFFFFFFFFU) + dw_list_size) &
		0xffffffff;

	p_local_list = rte_zmalloc(NULL, dw_manage_size, 0);
	if (p_local_list == NULL) {
		*p_list = NULL;
		PMD_DRV_LOG(ERR, "malloc memory failed");
		return ZXDH_LIST_STACK_ALLOC_MEMORY_FAIL;
	}

	p_local_list->p_array = (ZXDH_COMM_FREELINK *)((uint8_t *)p_local_list +
		sizeof(ZXDH_LISTSTACK_MANAGER));

	p_local_list->capacity = dw_actual_element_num;
	p_local_list->free_num = dw_actual_element_num - 1;
	p_local_list->used_num = 0;

	for (i = 1; i < (dw_actual_element_num - 1); i++) {
		p_local_list->p_array[i].index = i;
		p_local_list->p_array[i].next = i + 1;
	}

	p_local_list->p_array[0].index = 0;
	p_local_list->p_array[0].next =  0;

	p_local_list->p_array[dw_actual_element_num - 1].index = dw_actual_element_num - 1;
	p_local_list->p_array[dw_actual_element_num - 1].next = 0xffffffff;

	p_local_list->p_head = p_local_list->p_array[1].index;

	*p_list = p_local_list;

	return ZXDH_OK;
}

static uint32_t
zxdh_np_comm_liststack_alloc(ZXDH_LISTSTACK_MANAGER *p_list, uint32_t *p_index)
{
	uint32_t dw_alloc_index = 0;
	uint32_t dw_next_free = 0;

	if (p_list == NULL) {
		*p_index = ZXDH_LISTSTACK_INVALID_INDEX;
		return ZXDH_LIST_STACK_POINT_NULL;
	}

	if (p_list->p_head == ZXDH_LISTSTACK_INVALID_INDEX) {
		*p_index = ZXDH_LISTSTACK_INVALID_INDEX;
		return ZXDH_LIST_STACK_ISEMPTY_ERR;
	}

	dw_alloc_index = p_list->p_head;

	dw_next_free = p_list->p_array[dw_alloc_index].next;
	p_list->p_array[dw_alloc_index].next = ZXDH_LISTSTACK_INVALID_INDEX;

	if (dw_next_free != 0xffffffff)
		p_list->p_head = p_list->p_array[dw_next_free].index;
	else
		p_list->p_head = ZXDH_LISTSTACK_INVALID_INDEX;

	*p_index = dw_alloc_index - 1;

	p_list->free_num--;
	p_list->used_num++;

	if (p_list->free_num == 0 || (p_list->used_num == (p_list->capacity - 1)))
		p_list->p_head = ZXDH_LISTSTACK_INVALID_INDEX;

	return ZXDH_OK;
}

static uint32_t
zxdh_comm_liststack_free(ZXDH_LISTSTACK_MANAGER *p_list, uint32_t index)
{
	uint32_t	 dw_free_index = 0;
	uint32_t	 dw_prev_free  = 0;
	uint32_t	 dw_index	  = 0;

	dw_index	  = index + 1;

	if (p_list == NULL) {
		PMD_DRV_LOG(ERR, " p_list is null");
		return ZXDH_LIST_STACK_POINT_NULL;
	}

	if (dw_index >= p_list->capacity) {
		PMD_DRV_LOG(ERR, "dw_index is invalid");
		return ZXDH_LIST_STACK_FREE_INDEX_INVALID;
	}

	if (p_list->p_array[dw_index].next != ZXDH_LISTSTACK_INVALID_INDEX)
		return ZXDH_OK;

	dw_free_index = dw_index;
	dw_prev_free = p_list->p_head;

	if (dw_prev_free != 0)
		p_list->p_array[dw_free_index].next =  p_list->p_array[dw_prev_free].index;
	else
		p_list->p_array[dw_free_index].next = 0xffffffff;

	p_list->p_head = p_list->p_array[dw_free_index].index;

	p_list->free_num++;
	p_list->used_num--;

	return ZXDH_OK;
}

static uint32_t
zxdh_comm_double_link_insert_1st(ZXDH_D_NODE *p_newnode, ZXDH_D_HEAD *p_head)
{
	RTE_ASSERT(!(!p_head->p_next && p_head->p_prev));
	RTE_ASSERT(!(p_head->p_next && !p_head->p_prev));

	p_newnode->next = p_head->p_next;
	p_newnode->prev = NULL;

	if (p_head->p_next)
		p_head->p_next->prev = p_newnode;
	else
		p_head->p_prev = p_newnode;

	p_head->p_next = p_newnode;
	p_head->used++;

	return ZXDH_OK;
}

static uint32_t
zxdh_comm_double_link_insert_aft(ZXDH_D_NODE *p_newnode,
								ZXDH_D_NODE *p_oldnode,
								ZXDH_D_HEAD *p_head)
{
	RTE_ASSERT(!(!p_head->p_next && p_head->p_prev));
	RTE_ASSERT(!(p_head->p_next && !p_head->p_prev));

	p_newnode->next = p_oldnode->next;
	p_newnode->prev = p_oldnode;

	if (p_oldnode->next)
		p_oldnode->next->prev = p_newnode;
	else
		p_head->p_prev = p_newnode;

	p_oldnode->next = p_newnode;
	p_head->used++;

	return ZXDH_OK;
}

static uint32_t
zxdh_comm_double_link_insert_pre(ZXDH_D_NODE *p_newnode,
	ZXDH_D_NODE *p_oldnode, ZXDH_D_HEAD *p_head)
{
	RTE_ASSERT(!(!p_head->p_next && p_head->p_prev));
	RTE_ASSERT(!(p_head->p_next && !p_head->p_prev));

	p_newnode->next = p_oldnode;
	p_newnode->prev = p_oldnode->prev;

	if (p_oldnode->prev)
		p_oldnode->prev->next = p_newnode;
	else
		p_head->p_next = p_newnode;

	p_oldnode->prev = p_newnode;
	p_head->used++;

	return ZXDH_OK;
}

static uint32_t
zxdh_comm_double_link_insert_last(ZXDH_D_NODE *p_newnode, ZXDH_D_HEAD *p_head)
{
	ZXDH_D_NODE *p_dnode = NULL;

	RTE_ASSERT(!(!p_head->p_next && p_head->p_prev));
	RTE_ASSERT(!(p_head->p_next && !p_head->p_prev));

	p_dnode = p_head->p_prev;

	if (!p_dnode) {
		p_head->p_next  = p_newnode;
		p_head->p_prev  = p_newnode;
		p_newnode->next = NULL;
		p_newnode->prev = NULL;
	} else {
		p_newnode->prev = p_dnode;
		p_newnode->next = NULL;
		p_head->p_prev  = p_newnode;
		p_dnode->next   = p_newnode;
	}

	p_head->used++;

	return ZXDH_OK;
}

static uint32_t
zxdh_comm_double_link_del(ZXDH_D_NODE *delnode, ZXDH_D_HEAD *p_head)
{
	ZXDH_D_NODE *next = NULL;
	ZXDH_D_NODE *pre  = NULL;

	next = delnode->next;
	pre  = delnode->prev;

	if (next)
		next->prev = delnode->prev;
	else
		p_head->p_prev = delnode->prev;

	if (pre)
		pre->next = delnode->next;
	else
		p_head->p_next = delnode->next;

	p_head->used--;
	delnode->next = NULL;
	delnode->prev = NULL;
	return ZXDH_OK;
}

static int32_t
zxdh_comm_double_link_default_cmp_fuc(ZXDH_D_NODE *p_data1,
	ZXDH_D_NODE *p_data2, __rte_unused void *p_data)
{
	uint32_t data1 = *(uint32_t *)p_data1->data;
	uint32_t data2 = *(uint32_t *)p_data2->data;

	if (data1 > data2)
		return 1;
	else if (data1 == data2)
		return 0;
	else
		return -1;
}

static uint32_t
zxdh_comm_double_link_insert_sort(ZXDH_D_NODE *p_newnode,
			ZXDH_D_HEAD *p_head, ZXDH_CMP_FUNC cmp_fuc, void *cmp_data)
{
	ZXDH_D_NODE *pre_node = NULL;

	if (cmp_fuc == NULL)
		cmp_fuc = zxdh_comm_double_link_default_cmp_fuc;

	if (p_head->used == 0)
		return zxdh_comm_double_link_insert_1st(p_newnode, p_head);

	pre_node = p_head->p_next;

	while (pre_node != NULL) {
		if (cmp_fuc(p_newnode, pre_node, cmp_data) <= 0)
			return zxdh_comm_double_link_insert_pre(p_newnode,
				pre_node, p_head);
		else
			pre_node = pre_node->next;
	}

	return zxdh_comm_double_link_insert_last(p_newnode, p_head);
}

static int32_t
zxdh_comm_rb_def_cmp(void *p_new, void *p_old, uint32_t key_size)
{
	return memcmp(p_new, p_old, key_size);
}

static void
zxdh_comm_rb_switch_color(ZXDH_RB_TN  *p_tn1, ZXDH_RB_TN *p_tn2)
{
	uint32_t color1, color2;

	color1 = zxdh_np_get_tn_color(p_tn1);
	color2 = zxdh_np_get_tn_color(p_tn2);

	zxdh_np_set_tn_color(p_tn1, color2);
	zxdh_np_set_tn_color(p_tn2, color1);
}

static ZXDH_RB_TN *
zxdh_comm_rb_get_brotn(ZXDH_RB_TN *p_cur_tn)
{
	return (p_cur_tn->p_parent->p_left == p_cur_tn) ? p_cur_tn->p_parent->p_right :
		p_cur_tn->p_parent->p_left;
}

static uint32_t
zxdh_comm_rb_handle_ins(__rte_unused ZXDH_RB_CFG *p_rb_cfg,
						ZXDH_RB_TN  ***stack_tn,
						uint32_t	  stack_top)
{
	ZXDH_RB_TN  **pp_cur_tn		= NULL;
	ZXDH_RB_TN  *p_cur_tn		  = NULL;
	ZXDH_RB_TN  **pp_tmp_tn		= NULL;
	ZXDH_RB_TN  *p_tmp_tn		  = NULL;

	while (stack_top > 0) {
		pp_cur_tn = stack_tn[stack_top];
		p_cur_tn  = *pp_cur_tn;

		if (!p_cur_tn->p_parent) {
			zxdh_np_set_tn_color(p_cur_tn, ZXDH_RBT_BLACK);
			break;
		} else if (zxdh_np_get_tn_color(p_cur_tn->p_parent) == ZXDH_RBT_RED) {
			ZXDH_RB_TN *p_unc_tn = zxdh_comm_rb_get_brotn(p_cur_tn->p_parent);

			RTE_ASSERT(p_cur_tn->p_parent == *stack_tn[stack_top - 1]);

			if (zxdh_np_get_tn_color(p_unc_tn) == ZXDH_RBT_RED) {
				RTE_ASSERT(p_unc_tn);
				zxdh_np_set_tn_color(p_cur_tn->p_parent, ZXDH_RBT_BLACK);
				zxdh_np_set_tn_color(p_unc_tn, ZXDH_RBT_BLACK);

				RTE_ASSERT(p_cur_tn->p_parent->p_parent ==
					*stack_tn[stack_top - 2]);

				zxdh_np_set_tn_color(p_cur_tn->p_parent->p_parent, ZXDH_RBT_RED);
				stack_top -= 2;
			} else {
				ZXDH_RB_TN *p_bro_tn = NULL;

				pp_tmp_tn = stack_tn[stack_top - 2];
				p_tmp_tn  = *pp_tmp_tn;

				if (p_cur_tn->p_parent == p_tmp_tn->p_left && p_cur_tn ==
				p_cur_tn->p_parent->p_left) {
					*pp_tmp_tn = p_cur_tn->p_parent;

					p_bro_tn  = zxdh_comm_rb_get_brotn(p_cur_tn);
					p_cur_tn->p_parent->p_parent = p_tmp_tn->p_parent;

					p_tmp_tn->p_left   = p_bro_tn;
					p_tmp_tn->p_parent = p_cur_tn->p_parent;
					p_cur_tn->p_parent->p_right = p_tmp_tn;

					if (p_bro_tn)
						p_bro_tn->p_parent  = p_tmp_tn;

					zxdh_comm_rb_switch_color(*pp_tmp_tn, p_tmp_tn);
				} else if (p_cur_tn->p_parent == p_tmp_tn->p_left && p_cur_tn ==
				p_cur_tn->p_parent->p_right) {
					*pp_tmp_tn = p_cur_tn;

					p_cur_tn->p_parent->p_right = p_cur_tn->p_left;

					if (p_cur_tn->p_left)
						p_cur_tn->p_left->p_parent = p_cur_tn->p_parent;

					p_cur_tn->p_parent->p_parent = p_cur_tn;
					p_tmp_tn->p_left = p_cur_tn->p_right;

					if (p_cur_tn->p_right)
						p_cur_tn->p_right->p_parent = p_tmp_tn;

					p_cur_tn->p_left = p_cur_tn->p_parent;
					p_cur_tn->p_right = p_tmp_tn;

					p_cur_tn->p_parent = p_tmp_tn->p_parent;
					p_tmp_tn->p_parent = p_cur_tn;

					zxdh_comm_rb_switch_color(*pp_tmp_tn, p_tmp_tn);
				} else if (p_cur_tn->p_parent == p_tmp_tn->p_right && p_cur_tn ==
				p_cur_tn->p_parent->p_right) {
					*pp_tmp_tn = p_cur_tn->p_parent;
					p_bro_tn  = zxdh_comm_rb_get_brotn(p_cur_tn);

					p_cur_tn->p_parent->p_parent = p_tmp_tn->p_parent;

					p_tmp_tn->p_right = p_cur_tn->p_parent->p_left;
					p_tmp_tn->p_parent = p_cur_tn->p_parent;
					p_cur_tn->p_parent->p_left = p_tmp_tn;

					if (p_bro_tn)
						p_bro_tn->p_parent  = p_tmp_tn;

					zxdh_comm_rb_switch_color(*pp_tmp_tn, p_tmp_tn);
				} else {
					*pp_tmp_tn = p_cur_tn;
					p_cur_tn->p_parent->p_left = p_cur_tn->p_right;

					if (p_cur_tn->p_right)
						p_cur_tn->p_right->p_parent = p_cur_tn->p_parent;

					p_cur_tn->p_parent->p_parent = p_cur_tn;
					p_tmp_tn->p_right = p_cur_tn->p_left;

					if (p_cur_tn->p_left)
						p_cur_tn->p_left->p_parent = p_tmp_tn;

					p_cur_tn->p_right = p_cur_tn->p_parent;
					p_cur_tn->p_left = p_tmp_tn;

					p_cur_tn->p_parent = p_tmp_tn->p_parent;
					p_tmp_tn->p_parent = p_cur_tn;

					zxdh_comm_rb_switch_color(*pp_tmp_tn, p_tmp_tn);
				}
				break;
			}
		} else {
			break;
		}
	}

	return ZXDH_OK;
}

static uint32_t
zxdh_comm_rb_init(ZXDH_RB_CFG *p_rb_cfg,
				uint32_t	  total_num,
				uint32_t	  key_size,
				ZXDH_RB_CMPFUN cmpfun)
{
	uint32_t	  rtn  = ZXDH_OK;
	uint32_t	  malloc_size = 0;

	if (p_rb_cfg->is_init) {
		PMD_DRV_LOG(ERR, " p_rb_cfg already init!");
		return ZXDH_OK;
	}

	p_rb_cfg->key_size =  key_size;
	p_rb_cfg->p_root   =  NULL;

	if (cmpfun)
		p_rb_cfg->p_cmpfun =  cmpfun;
	else
		p_rb_cfg->p_cmpfun = zxdh_comm_rb_def_cmp;

	if (total_num) {
		p_rb_cfg->is_dynamic = 0;

		rtn = zxdh_comm_double_link_init(total_num, &p_rb_cfg->tn_list);
		ZXDH_COMM_CHECK_RC(rtn, "zxdh_comm_double_link_init");

		rtn = zxdh_np_comm_liststack_create(total_num, &p_rb_cfg->p_lsm);
		ZXDH_COMM_CHECK_RC(rtn, "zxdh_np_comm_liststack_create");

		p_rb_cfg->p_keybase = rte_zmalloc(NULL,
			total_num * p_rb_cfg->key_size, 0);
		if (p_rb_cfg->p_keybase == NULL) {
			PMD_DRV_LOG(ERR, "malloc memory failed");
			return ZXDH_PAR_CHK_POINT_NULL;
		}

		malloc_size = ((sizeof(ZXDH_RB_TN) & 0xFFFFFFFFU) * total_num) & UINT32_MAX;

		p_rb_cfg->p_tnbase  = rte_zmalloc(NULL, malloc_size, 0);
		if (p_rb_cfg->p_tnbase == NULL) {
			PMD_DRV_LOG(ERR, "malloc memory failed");
			return ZXDH_PAR_CHK_POINT_NULL;
		}
	} else {
		p_rb_cfg->is_dynamic = 1;

		rtn = zxdh_comm_double_link_init(0xFFFFFFFF, &p_rb_cfg->tn_list);
		ZXDH_COMM_CHECK_RC(rtn, "zxdh_comm_double_link_init");
	}
	p_rb_cfg->is_init = 1;

	return ZXDH_OK;
}

static uint32_t
zxdh_comm_rb_insert(ZXDH_RB_CFG *p_rb_cfg,
						 void	   *p_key,
						 void	   *out_val)
{
	uint32_t	 rtn			= 0;
	uint32_t	 stack_top	  = 1;
	int32_t		 cmprtn		 = 0;
	uint32_t	 lsm_out		= 0;

	ZXDH_RB_TN  **stack_tn[ZXDH_RBT_MAX_DEPTH] = {0};
	ZXDH_RB_TN  *p_cur_tn	  = NULL;
	ZXDH_RB_TN  *p_pre_tn	  = NULL;
	ZXDH_RB_TN **pp_cur_tn	 = NULL;
	void	   *p_cur_key	 = NULL;
	ZXDH_RB_TN  *p_ins_tn	  = p_key;

	p_cur_key = p_rb_cfg->is_dynamic ? ((ZXDH_RB_TN *)p_key)->p_key : p_key;

	pp_cur_tn = &p_rb_cfg->p_root;

	for (;;) {
		p_cur_tn = *pp_cur_tn;

		if (!p_cur_tn) {
			if (p_rb_cfg->is_dynamic == 0) {
				rtn = zxdh_np_comm_liststack_alloc(p_rb_cfg->p_lsm, &lsm_out);

				if (rtn == ZXDH_LIST_STACK_ISEMPTY_ERR)
					return ZXDH_RBT_RC_FULL;

				ZXDH_COMM_CHECK_RC(rtn, "zxdh_np_comm_liststack_alloc");

				p_ins_tn = p_rb_cfg->p_tnbase + lsm_out;

				zxdh_np_init_rbt_tn(p_ins_tn, p_rb_cfg->key_size * lsm_out +
					p_rb_cfg->p_keybase);

				memcpy(p_ins_tn->p_key, p_key, p_rb_cfg->key_size);

				zxdh_np_set_tn_lsv(p_ins_tn, lsm_out);

				if (out_val)
					*((uint32_t *)out_val) = lsm_out;
			} else {
				zxdh_np_init_d_node(&p_ins_tn->tn_ln, p_ins_tn);
			}

			zxdh_np_set_tn_color(p_ins_tn, ZXDH_RBT_RED);

			if (cmprtn < 0) {
				rtn = zxdh_comm_double_link_insert_pre(&p_ins_tn->tn_ln,
					&p_pre_tn->tn_ln, &p_rb_cfg->tn_list);
				ZXDH_COMM_CHECK_RC(rtn, "zxdh_comm_double_link_insert_pre");
			} else if (cmprtn > 0) {
				rtn = zxdh_comm_double_link_insert_aft(&p_ins_tn->tn_ln,
					&p_pre_tn->tn_ln, &p_rb_cfg->tn_list);
				ZXDH_COMM_CHECK_RC(rtn, "zxdh_comm_double_link_insert_aft");
			} else {
				RTE_ASSERT(!p_pre_tn);

				rtn = zxdh_comm_double_link_insert_1st(&p_ins_tn->tn_ln,
					&p_rb_cfg->tn_list);
				ZXDH_COMM_CHECK_RC(rtn, "zxdh_comm_double_link_insert_1st");
			}

			break;
		}

		stack_tn[stack_top++] =  pp_cur_tn;
		p_pre_tn = p_cur_tn;
		cmprtn = p_rb_cfg->p_cmpfun(p_cur_key, p_cur_tn->p_key, p_rb_cfg->key_size);

		if (cmprtn > 0) {
			pp_cur_tn = &p_cur_tn->p_right;
		} else if (cmprtn < 0) {
			pp_cur_tn = &p_cur_tn->p_left;
		} else {
			PMD_DRV_LOG(ERR, "rb_key is same");

			if (p_rb_cfg->is_dynamic) {
				if (out_val)
					*((ZXDH_RB_TN **)out_val) = p_cur_tn;
			} else {
				if (out_val)
					*((uint32_t *)out_val) = zxdh_np_get_tn_lsv(p_cur_tn);
			}

			return ZXDH_RBT_RC_UPDATE;
		}
	}

	p_ins_tn->p_parent = (stack_top > 1) ? *stack_tn[stack_top - 1] : NULL;
	stack_tn[stack_top] = pp_cur_tn;

	*pp_cur_tn = p_ins_tn;

	rtn = zxdh_comm_rb_handle_ins(p_rb_cfg, stack_tn, stack_top);
	ZXDH_COMM_CHECK_RC(rtn, "zxdh_comm_rb_handle_ins");

	if (p_rb_cfg->is_dynamic) {
		if (out_val)
			*((ZXDH_RB_TN **)out_val) = p_ins_tn;
	}

	return ZXDH_OK;
}

static uint32_t
zxdh_comm_rb_search(ZXDH_RB_CFG *p_rb_cfg,
						 void	   *p_key,
						 void	   *out_val)
{
	int32_t	cmprtn   = 0;
	ZXDH_RB_TN *p_cur_tn = NULL;

	p_cur_tn = p_rb_cfg->p_root;

	while (p_cur_tn) {
		cmprtn = p_rb_cfg->p_cmpfun(p_key, p_cur_tn->p_key, p_rb_cfg->key_size);

		if (cmprtn > 0)
			p_cur_tn = p_cur_tn->p_right;
		else if (cmprtn < 0)
			p_cur_tn = p_cur_tn->p_left;
		else
			break;
	}

	if (!p_cur_tn) {
		PMD_DRV_LOG(DEBUG, "rb srh fail");
		return ZXDH_RBT_RC_SRHFAIL;
	}

	if (p_rb_cfg->is_dynamic)
		*(ZXDH_RB_TN **)out_val = p_cur_tn;
	else
		*(uint32_t *)out_val = zxdh_np_get_tn_lsv(p_cur_tn);

	return ZXDH_OK;
}

static uint32_t
zxdh_comm_rb_handle_del(__rte_unused ZXDH_RB_CFG *p_rb_cfg,
							ZXDH_RB_TN ***stack_tn,
							uint32_t   stack_top)
{
	ZXDH_RB_TN  **pp_cur_tn		= NULL;
	ZXDH_RB_TN  *p_cur_tn		  = NULL;
	ZXDH_RB_TN  *p_tmp_tn		  = NULL;
	ZXDH_RB_TN  *p_unc_tn		  = NULL;
	ZXDH_RB_TN  *p_par_tn		  = NULL;

	while (stack_top > 1) {
		pp_cur_tn =  stack_tn[stack_top];
		p_cur_tn  = *pp_cur_tn;

		p_par_tn  = *stack_tn[stack_top - 1];

		if (p_cur_tn && p_cur_tn->p_parent) {
			p_unc_tn  = zxdh_comm_rb_get_brotn(p_cur_tn);
		} else if (p_cur_tn && !p_cur_tn->p_parent) {
			RTE_ASSERT(p_par_tn == p_cur_tn->p_parent);

			zxdh_np_set_tn_color(p_cur_tn, ZXDH_RBT_BLACK);

			break;
		}
		if (!p_cur_tn) {
			RTE_ASSERT(!p_cur_tn);

			if (p_par_tn)
				p_unc_tn = p_par_tn->p_left ? p_par_tn->p_left : p_par_tn->p_right;
			else
				break;
		}

		if (p_unc_tn)
			RTE_ASSERT(p_unc_tn->p_parent == p_par_tn);

		if (!p_unc_tn) {
			RTE_ASSERT(0);
			RTE_ASSERT(zxdh_np_get_tn_color(p_par_tn) ==  ZXDH_RBT_RED);

			zxdh_np_set_tn_color(p_par_tn, ZXDH_RBT_BLACK);

			break;
		}
		if (zxdh_np_get_tn_color(p_unc_tn) == ZXDH_RBT_RED) {
			if (p_unc_tn == p_par_tn->p_left) {
				*stack_tn[stack_top - 1] = p_unc_tn;
				p_unc_tn->p_parent = p_par_tn->p_parent;
				p_par_tn->p_left = p_unc_tn->p_right;

				if (p_unc_tn->p_right)
					p_unc_tn->p_right->p_parent = p_par_tn;

				p_par_tn->p_parent = p_unc_tn;
				p_unc_tn->p_right = p_par_tn;

				stack_tn[stack_top++] = &p_unc_tn->p_right;
				stack_tn[stack_top]   = &p_par_tn->p_right;
			} else {
				RTE_ASSERT(p_unc_tn == p_par_tn->p_right);
				*stack_tn[stack_top - 1] = p_unc_tn;
				p_unc_tn->p_parent = p_par_tn->p_parent;
				p_par_tn->p_right = p_unc_tn->p_left;

				if (p_unc_tn->p_left)
					p_unc_tn->p_left->p_parent = p_par_tn;

				p_par_tn->p_parent = p_unc_tn;
				p_unc_tn->p_left  = p_par_tn;

				stack_tn[stack_top++] = &p_unc_tn->p_left;
				stack_tn[stack_top]   = &p_par_tn->p_left;
			}

			zxdh_comm_rb_switch_color(p_unc_tn, p_par_tn);
		} else {
			if (zxdh_np_get_tn_color(p_unc_tn->p_left) == ZXDH_RBT_BLACK &&
			zxdh_np_get_tn_color(p_unc_tn->p_right) == ZXDH_RBT_BLACK) {
				if (zxdh_np_get_tn_color(p_unc_tn->p_parent) == ZXDH_RBT_BLACK) {
					zxdh_np_set_tn_color(p_unc_tn, ZXDH_RBT_RED);
					stack_top--;
				} else {
					RTE_ASSERT(zxdh_np_get_tn_color(p_unc_tn->p_parent)
						== ZXDH_RBT_RED);

					zxdh_comm_rb_switch_color(p_unc_tn->p_parent, p_unc_tn);

					break;
				}
			} else if (p_unc_tn == p_par_tn->p_right) {
				if (zxdh_np_get_tn_color(p_unc_tn->p_right) == ZXDH_RBT_RED) {
					*stack_tn[stack_top - 1] = p_unc_tn;
					p_unc_tn->p_parent = p_par_tn->p_parent;
					p_par_tn->p_right = p_unc_tn->p_left;

					if (p_unc_tn->p_left)
						p_unc_tn->p_left->p_parent = p_par_tn;

					p_par_tn->p_parent = p_unc_tn;
					p_unc_tn->p_left  = p_par_tn;

					zxdh_comm_rb_switch_color(p_unc_tn, p_par_tn);

					zxdh_np_set_tn_color(p_unc_tn->p_right, ZXDH_RBT_BLACK);

					break;
				}
				RTE_ASSERT(zxdh_np_get_tn_color(p_unc_tn->p_left)
					== ZXDH_RBT_RED);

				p_tmp_tn = p_unc_tn->p_left;

				p_par_tn->p_right  = p_tmp_tn;
				p_tmp_tn->p_parent = p_par_tn;
				p_unc_tn->p_left  = p_tmp_tn->p_right;

				if (p_tmp_tn->p_right)
					p_tmp_tn->p_right->p_parent = p_unc_tn;

				p_tmp_tn->p_right = p_unc_tn;
				p_unc_tn->p_parent = p_tmp_tn;

				zxdh_comm_rb_switch_color(p_tmp_tn, p_unc_tn);
			} else {
				RTE_ASSERT(p_unc_tn == p_par_tn->p_left);

				if (zxdh_np_get_tn_color(p_unc_tn->p_left) == ZXDH_RBT_RED) {
					*stack_tn[stack_top - 1] = p_unc_tn;
					p_unc_tn->p_parent = p_par_tn->p_parent;
					p_par_tn->p_left  = p_unc_tn->p_right;

					if (p_unc_tn->p_right)
						p_unc_tn->p_right->p_parent = p_par_tn;

					p_par_tn->p_parent = p_unc_tn;
					p_unc_tn->p_right = p_par_tn;

					zxdh_comm_rb_switch_color(p_unc_tn, p_par_tn);

					zxdh_np_set_tn_color(p_unc_tn->p_left, ZXDH_RBT_BLACK);
					break;
				}
				RTE_ASSERT(zxdh_np_get_tn_color(p_unc_tn->p_right)
					== ZXDH_RBT_RED);

				p_tmp_tn = p_unc_tn->p_right;

				p_par_tn->p_left  = p_tmp_tn;
				p_tmp_tn->p_parent = p_par_tn;
				p_unc_tn->p_right  = p_tmp_tn->p_left;

				if (p_tmp_tn->p_left)
					p_tmp_tn->p_left->p_parent = p_unc_tn;

				p_tmp_tn->p_left = p_unc_tn;
				p_unc_tn->p_parent = p_tmp_tn;

				zxdh_comm_rb_switch_color(p_tmp_tn, p_unc_tn);
			}
		}
	}

	return ZXDH_OK;
}

static uint32_t
zxdh_comm_rb_delete(ZXDH_RB_CFG *p_rb_cfg,
						 void	   *p_key,
						 void	   *out_val)
{
	uint32_t	 rtn			= 0;
	uint32_t	 stack_top	  = 1;
	int32_t	cmprtn		 = 0;
	uint32_t	 rsv_stack	  = 0;
	uint32_t	 del_is_red	 = 0;
	ZXDH_RB_TN  **stack_tn[ZXDH_RBT_MAX_DEPTH] = {0};
	ZXDH_RB_TN  *p_cur_tn	  = NULL;
	ZXDH_RB_TN **pp_cur_tn	 = NULL;
	void	   *p_cur_key	 = NULL;
	ZXDH_RB_TN  *p_rsv_tn	  = NULL;
	ZXDH_RB_TN  *p_del_tn	  = NULL;

	p_cur_key = p_key;

	pp_cur_tn = &p_rb_cfg->p_root;

	for (;;) {
		p_cur_tn = *pp_cur_tn;

		if (!p_cur_tn)
			return ZXDH_RBT_RC_SRHFAIL;

		stack_tn[stack_top++] = pp_cur_tn;

		cmprtn = p_rb_cfg->p_cmpfun(p_cur_key, p_cur_tn->p_key, p_rb_cfg->key_size);

		if (cmprtn > 0) {
			pp_cur_tn = &p_cur_tn->p_right;
		} else if (cmprtn < 0) {
			pp_cur_tn = &p_cur_tn->p_left;
		} else {
			PMD_DRV_LOG(DEBUG, " find the key!");

			break;
		}
	}

	rsv_stack =  stack_top - 1;
	p_rsv_tn  =  p_cur_tn;

	pp_cur_tn = &p_cur_tn->p_right;
	p_cur_tn  = *pp_cur_tn;

	if (p_cur_tn) {
		stack_tn[stack_top++] = pp_cur_tn;

		pp_cur_tn = &p_cur_tn->p_left;
		p_cur_tn  = *pp_cur_tn;

		while (p_cur_tn) {
			stack_tn[stack_top++] = pp_cur_tn;
			pp_cur_tn = &p_cur_tn->p_left;
			p_cur_tn  = *pp_cur_tn;
		}

		p_del_tn = *stack_tn[stack_top - 1];

		*stack_tn[stack_top - 1] = p_del_tn->p_right;

		if (p_del_tn->p_right)
			p_del_tn->p_right->p_parent =  p_del_tn->p_parent;

		if (zxdh_np_get_tn_color(p_del_tn) == ZXDH_RBT_RED)
			del_is_red = 1;

		*stack_tn[rsv_stack]   = p_del_tn;

		stack_tn[rsv_stack + 1]  = &p_del_tn->p_right;

		zxdh_np_set_tn_color(p_del_tn, zxdh_np_get_tn_color(p_rsv_tn));
		p_del_tn->p_parent = p_rsv_tn->p_parent;

		p_del_tn->p_left   = p_rsv_tn->p_left;

		if (p_rsv_tn->p_left)
			p_rsv_tn->p_left->p_parent = p_del_tn;

		p_del_tn->p_right  = p_rsv_tn->p_right;

		if (p_rsv_tn->p_right)
			p_rsv_tn->p_right->p_parent = p_del_tn;
	} else {
		if (zxdh_np_get_tn_color(p_rsv_tn) == ZXDH_RBT_RED)
			del_is_red = 1;

		*stack_tn[stack_top - 1] = p_rsv_tn->p_left;

		if (p_rsv_tn->p_left)
			p_rsv_tn->p_left->p_parent = p_rsv_tn->p_parent;
	}

	stack_top--;
	if (zxdh_np_get_tn_color(*stack_tn[stack_top]) == ZXDH_RBT_RED) {
		zxdh_np_set_tn_color(*stack_tn[stack_top], ZXDH_RBT_BLACK);
	} else if (!del_is_red) {
		rtn = zxdh_comm_rb_handle_del(p_rb_cfg, stack_tn, stack_top);
		ZXDH_COMM_CHECK_RC(rtn, "zxdh_comm_rb_handle_del");
	}

	rtn = zxdh_comm_double_link_del(&p_rsv_tn->tn_ln, &p_rb_cfg->tn_list);
	ZXDH_COMM_CHECK_RC(rtn, "zxdh_comm_double_link_del");

	if (p_rb_cfg->is_dynamic) {
		*(ZXDH_RB_TN **)out_val = p_rsv_tn;
	} else {
		rtn = zxdh_comm_liststack_free(p_rb_cfg->p_lsm, zxdh_np_get_tn_lsv(p_rsv_tn));
		ZXDH_COMM_CHECK_RC(rtn, "zxdh_comm_liststack_free");

		*(uint32_t *)out_val = zxdh_np_get_tn_lsv(p_rsv_tn);

		memset(p_rsv_tn->p_key, 0, p_rb_cfg->key_size);
		memset(p_rsv_tn, 0, sizeof(ZXDH_RB_TN));
	}

	return ZXDH_OK;
}

static uint32_t
zxdh_comm_liststack_destroy(ZXDH_LISTSTACK_MANAGER *p_list)
{
	if (p_list == NULL) {
		PMD_DRV_LOG(ERR, "p_list point null");
		return ZXDH_LIST_STACK_POINT_NULL;
	}
	rte_free(p_list);

	return ZXDH_OK;
}

static uint32_t
zxdh_comm_rb_destroy(ZXDH_RB_CFG *p_rb_cfg)
{
	uint32_t rtn = 0;

	if (p_rb_cfg->is_dynamic == 0)
		zxdh_comm_liststack_destroy(p_rb_cfg->p_lsm);

	if (p_rb_cfg->p_keybase != NULL) {
		rte_free(p_rb_cfg->p_keybase);
		p_rb_cfg->p_keybase = NULL;
	}

	if (p_rb_cfg->p_tnbase != NULL) {
		rte_free(p_rb_cfg->p_tnbase);
		p_rb_cfg->p_tnbase = NULL;
	}

	memset(p_rb_cfg, 0, sizeof(ZXDH_RB_CFG));

	return rtn;
}

static int
zxdh_np_se_apt_key_default_cmp(void *p_new_key,
	void *p_old_key, __rte_unused uint32_t key_len)
{
	return memcmp((uint32_t *)p_new_key, (uint32_t *)p_old_key, sizeof(uint32_t));
}

static uint32_t
zxdh_np_se_apt_rb_insert(ZXDH_RB_CFG *rb_cfg, void *p_data, uint32_t len)
{
	uint8_t *p_rb_key		 = NULL;
	ZXDH_RB_TN *p_rb_new	 = NULL;
	ZXDH_RB_TN *p_rb_rtn	 = NULL;
	uint32_t rc				 = ZXDH_OK;

	p_rb_key = rte_zmalloc(NULL, len, 0);
	if (p_rb_key == NULL) {
		PMD_DRV_LOG(ERR, "malloc memory failed");
		return ZXDH_PAR_CHK_POINT_NULL;
	}
	memcpy(p_rb_key, p_data, len);

	p_rb_new = rte_zmalloc(NULL, sizeof(ZXDH_RB_TN), 0);
	if (NULL == (p_rb_new)) {
		rte_free(p_rb_key);
		PMD_DRV_LOG(ERR, "malloc memory failed");
		return ZXDH_PAR_CHK_POINT_NULL;
	}
	zxdh_np_init_rbt_tn(p_rb_new, p_rb_key);

	rc = zxdh_comm_rb_insert(rb_cfg, p_rb_new, &p_rb_rtn);
	if (rc == ZXDH_RBT_RC_UPDATE) {
		if (p_rb_rtn == NULL) {
			PMD_DRV_LOG(ERR, "p_rb_rtn point null!");
			return ZXDH_PAR_CHK_POINT_NULL;
		}

		memcpy(p_rb_rtn->p_key, p_data, len);
		rte_free(p_rb_new);
		rte_free(p_rb_key);
		PMD_DRV_LOG(DEBUG, "update exist entry!");
		return ZXDH_OK;
	}

	return rc;
}

static uint32_t
zxdh_np_se_apt_rb_delete(ZXDH_RB_CFG *rb_cfg, void *p_data, __rte_unused uint32_t len)
{
	uint32_t rc = ZXDH_OK;
	ZXDH_RB_TN *p_rb_rtn	 = NULL;

	rc = zxdh_comm_rb_delete(rb_cfg, p_data, &p_rb_rtn);
	if (rc != ZXDH_OK)
		return rc;
	rte_free(p_rb_rtn->p_key);
	rte_free(p_rb_rtn);

	return rc;
}

static uint32_t
zxdh_np_se_apt_rb_search(ZXDH_RB_CFG *rb_cfg, void *p_data, uint32_t len)
{
	uint32_t rc = ZXDH_OK;
	ZXDH_RB_TN *p_rb_rtn	 = NULL;

	rc = zxdh_comm_rb_search(rb_cfg, p_data, &p_rb_rtn);
	if (rc != ZXDH_OK)
		return rc;

	memcpy(p_data, p_rb_rtn->p_key, len);
	return rc;
}

static uint32_t
zxdh_np_dev_init(void)
{
	if (g_dev_mgr.is_init) {
		PMD_DRV_LOG(ERR, "Dev is already initialized");
		return 0;
	}

	g_dev_mgr.device_num = 0;
	g_dev_mgr.is_init    = 1;

	return 0;
}

static void
zxdh_np_dev_vport_get(uint32_t dev_id, uint32_t *vport)
{
	ZXDH_DEV_MGR_T *p_dev_mgr = &g_dev_mgr;
	ZXDH_DEV_CFG_T *p_dev_info = p_dev_mgr->p_dev_array[ZXDH_DEV_SLOT_ID(dev_id)];

	*vport = p_dev_info->vport[ZXDH_DEV_PF_INDEX(dev_id)];
}

static void
zxdh_np_dev_agent_addr_get(uint32_t dev_id, uint64_t *agent_addr)
{
	ZXDH_DEV_MGR_T *p_dev_mgr = &g_dev_mgr;
	ZXDH_DEV_CFG_T *p_dev_info = p_dev_mgr->p_dev_array[ZXDH_DEV_SLOT_ID(dev_id)];

	*agent_addr = p_dev_info->agent_addr[ZXDH_DEV_PF_INDEX(dev_id)];
}

static void
zxdh_np_dev_bar_pcie_id_get(uint32_t dev_id, uint16_t *p_pcie_id)
{
	ZXDH_DEV_MGR_T *p_dev_mgr = &g_dev_mgr;
	ZXDH_DEV_CFG_T *p_dev_info = p_dev_mgr->p_dev_array[ZXDH_DEV_SLOT_ID(dev_id)];

	*p_pcie_id = p_dev_info->pcie_id[ZXDH_DEV_PF_INDEX(dev_id)];
}

static void
zxdh_np_dev_fw_bar_msg_num_set(uint32_t dev_id, uint32_t bar_msg_num)
{
	ZXDH_DEV_MGR_T *p_dev_mgr = &g_dev_mgr;
	ZXDH_DEV_CFG_T *p_dev_info = p_dev_mgr->p_dev_array[ZXDH_DEV_SLOT_ID(dev_id)];

	p_dev_info->fw_bar_msg_num = bar_msg_num;

	PMD_DRV_LOG(INFO, "fw_bar_msg_num_set:fw support agent msg num = %u!", bar_msg_num);
}

static void
zxdh_np_dev_fw_bar_msg_num_get(uint32_t dev_id, uint32_t *bar_msg_num)
{
	ZXDH_DEV_MGR_T *p_dev_mgr = &g_dev_mgr;
	ZXDH_DEV_CFG_T *p_dev_info = p_dev_mgr->p_dev_array[ZXDH_DEV_SLOT_ID(dev_id)];

	*bar_msg_num = p_dev_info->fw_bar_msg_num;
}

static uint32_t
zxdh_np_dev_opr_spinlock_get(uint32_t dev_id, uint32_t type, ZXDH_SPINLOCK_T **p_spinlock_out)
{
	ZXDH_DEV_MGR_T *p_dev_mgr = &g_dev_mgr;
	ZXDH_DEV_CFG_T *p_dev_info = p_dev_mgr->p_dev_array[ZXDH_DEV_SLOT_ID(dev_id)];

	if (p_dev_info == NULL) {
		PMD_DRV_LOG(ERR, "Get dev_info[ %u ] fail!", dev_id);
		return ZXDH_DEV_TYPE_INVALID;
	}

	switch (type) {
	case ZXDH_DEV_SPINLOCK_T_DTB:
		*p_spinlock_out = &p_dev_info->dtb_spinlock;
		break;
	case ZXDH_DEV_SPINLOCK_T_SMMU0:
		*p_spinlock_out = &p_dev_info->smmu0_spinlock;
		break;
	default:
		PMD_DRV_LOG(ERR, "spinlock type is invalid!");
		return ZXDH_ERR;
	}

	return ZXDH_OK;
}

static uint32_t
zxdh_np_dev_dtb_opr_spinlock_get(uint32_t dev_id, uint32_t type,
			uint32_t index, ZXDH_SPINLOCK_T **p_spinlock_out)
{
	ZXDH_DEV_MGR_T *p_dev_mgr = &g_dev_mgr;
	ZXDH_DEV_CFG_T *p_dev_info = p_dev_mgr->p_dev_array[ZXDH_DEV_SLOT_ID(dev_id)];

	switch (type) {
	case ZXDH_DEV_SPINLOCK_T_DTB:
		*p_spinlock_out = &p_dev_info->dtb_queue_spinlock[index];
		break;
	default:
		PMD_DRV_LOG(ERR, "spinlock type is invalid!");
		return ZXDH_ERR;
	}

	return ZXDH_OK;
}

static void
zxdh_np_dev_hash_opr_spinlock_get(uint32_t dev_id,
	uint32_t fun_id, ZXDH_SPINLOCK_T **p_spinlock_out)
{
	ZXDH_DEV_MGR_T *p_dev_mgr = &g_dev_mgr;
	ZXDH_DEV_CFG_T *p_dev_info = p_dev_mgr->p_dev_array[ZXDH_DEV_SLOT_ID(dev_id)];

	*p_spinlock_out = &p_dev_info->hash_spinlock[fun_id];
}

static uint32_t
zxdh_np_dev_read_channel(uint32_t dev_id, uint32_t addr, uint32_t size, uint32_t *p_data)
{
	ZXDH_DEV_CFG_T *p_dev_info = NULL;

	p_dev_info = ZXDH_DEV_INFO_GET(dev_id);

	if (p_dev_info == NULL) {
		PMD_DRV_LOG(ERR, "Error: Channel[%u] dev is not exist",
			dev_id);
		return ZXDH_ERR;
	}
	if (p_dev_info->access_type == ZXDH_DEV_ACCESS_TYPE_PCIE) {
		p_dev_info->p_pcie_read_fun(dev_id, addr, size, p_data);
	} else {
		PMD_DRV_LOG(ERR, "Dev access type[ %u ] is invalid",
			p_dev_info->access_type);
		return ZXDH_ERR;
	}

	return ZXDH_OK;
}

static uint32_t
zxdh_np_dev_write_channel(uint32_t dev_id, uint32_t addr, uint32_t size, uint32_t *p_data)
{
	ZXDH_DEV_CFG_T *p_dev_info = NULL;

	p_dev_info = ZXDH_DEV_INFO_GET(dev_id);

	if (p_dev_info == NULL) {
		PMD_DRV_LOG(ERR, "Error: Channel[%u] dev is not exist", dev_id);
		return ZXDH_ERR;
	}
	if (p_dev_info->access_type == ZXDH_DEV_ACCESS_TYPE_PCIE) {
		p_dev_info->p_pcie_write_fun(dev_id, addr, size, p_data);
	} else {
		PMD_DRV_LOG(ERR, "Dev access type[ %u ] is invalid", p_dev_info->access_type);
		return ZXDH_ERR;
	}

	return ZXDH_OK;
}

static void
zxdh_np_pci_write32(uint64_t abs_addr, uint32_t *p_data)
{
	uint32_t data = 0;
	uint64_t addr = 0;

	data = *p_data;

	if (zxdh_np_comm_is_big_endian())
		data = ZXDH_COMM_CONVERT32(data);

	addr = abs_addr + ZXDH_SYS_VF_NP_BASE_OFFSET;
	*((volatile uint32_t *)addr) = data;
}

static void
zxdh_np_pci_read32(uint64_t abs_addr, uint32_t *p_data)
{
	uint32_t data = 0;
	uint64_t addr = 0;

	addr = abs_addr + ZXDH_SYS_VF_NP_BASE_OFFSET;
	data = *((volatile uint32_t *)addr);

	if (zxdh_np_comm_is_big_endian())
		data = ZXDH_COMM_CONVERT32(data);

	*p_data = data;
}

static uint64_t
zxdh_np_dev_get_pcie_addr(uint32_t dev_id)
{
	ZXDH_DEV_MGR_T *p_dev_mgr = NULL;
	ZXDH_DEV_CFG_T *p_dev_info = NULL;

	p_dev_mgr = &g_dev_mgr;
	p_dev_info = p_dev_mgr->p_dev_array[ZXDH_DEV_SLOT_ID(dev_id)];

	if (p_dev_info == NULL)
		return ZXDH_DEV_TYPE_INVALID;

	return p_dev_info->pcie_addr[ZXDH_DEV_PF_INDEX(dev_id)];
}

static void
zxdh_np_dev_pcie_default_write(uint32_t dev_id, uint32_t addr, uint32_t size, uint32_t *p_data)
{
	uint32_t i;
	uint64_t abs_addr = 0;

	abs_addr = zxdh_np_dev_get_pcie_addr(dev_id) + addr;

	for (i = 0; i < size; i++)
		zxdh_np_pci_write32(abs_addr + 4 * i, p_data + i);
}

static void
zxdh_np_dev_pcie_default_read(uint32_t dev_id, uint32_t addr, uint32_t size, uint32_t *p_data)
{
	uint32_t i;
	uint64_t abs_addr = 0;

	abs_addr = zxdh_np_dev_get_pcie_addr(dev_id) + addr;

	for (i = 0; i < size; i++)
		zxdh_np_pci_read32(abs_addr + 4 * i, p_data + i);
}

static uint32_t
zxdh_np_read(uint32_t dev_id, uint32_t addr, uint32_t *p_data)
{
	return zxdh_np_dev_read_channel(dev_id, addr, 1, p_data);
}

static uint32_t
zxdh_np_write(uint32_t dev_id, uint32_t addr, uint32_t *p_data)
{
	return zxdh_np_dev_write_channel(dev_id, addr, 1, p_data);
}

static uint32_t
zxdh_np_se_smmu0_write(uint32_t dev_id, uint32_t addr, uint32_t *p_data)
{
	return zxdh_np_write(dev_id, addr, p_data);
}

static uint32_t
zxdh_np_se_smmu0_read(uint32_t dev_id, uint32_t addr, uint32_t *p_data)
{
	return zxdh_np_read(dev_id, addr, p_data);
}

static ZXDH_REG_T g_dpp_reg_info[] = {
	{
		.reg_name = "cpu_ind_cmd",
		.reg_no = 669,
		.module_no = SMMU0,
		.flags = ZXDH_REG_FLAG_DIRECT,
		.array_type = ZXDH_REG_NUL_ARRAY,
		.addr = ZXDH_SYS_SE_SMMU0_BASE_ADDR + ZXDH_MODULE_SE_SMMU0_BASE_ADDR + 0x14,
		.width = (32 / 8),
		.m_size = 0,
		.n_size = 0,
		.m_step = 0,
		.n_step = 0,
		.field_num = 4,
		.p_fields = g_smmu0_smmu0_cpu_ind_cmd_reg,
		.p_write_fun = zxdh_np_se_smmu0_write,
		.p_read_fun = zxdh_np_se_smmu0_read,
	},
	{
		.reg_name = "cpu_ind_rd_done",
		.reg_no = 670,
		.module_no = SMMU0,
		.flags = ZXDH_REG_FLAG_DIRECT,
		.array_type = ZXDH_REG_NUL_ARRAY,
		.addr = ZXDH_SYS_SE_SMMU0_BASE_ADDR + ZXDH_MODULE_SE_SMMU0_BASE_ADDR + 0x40,
		.width = (32 / 8),
		.m_size = 0,
		.n_size = 0,
		.m_step = 0,
		.n_step = 0,
		.field_num = 1,
		.p_fields = g_smmu0_smmu0_cpu_ind_rd_done_reg,
		.p_write_fun = zxdh_np_se_smmu0_write,
		.p_read_fun = zxdh_np_se_smmu0_read,
	},
	{
		.reg_name = "cpu_ind_rdat0",
		.reg_no = 671,
		.module_no = SMMU0,
		.flags = ZXDH_REG_FLAG_DIRECT,
		.array_type = ZXDH_REG_NUL_ARRAY,
		.addr = ZXDH_SYS_SE_SMMU0_BASE_ADDR + ZXDH_MODULE_SE_SMMU0_BASE_ADDR + 0x44,
		.width = (32 / 8),
		.m_size = 0,
		.n_size = 0,
		.m_step = 0,
		.n_step = 0,
		.field_num = 1,
		.p_fields = g_smmu0_smmu0_cpu_ind_rdat0_reg,
		.p_write_fun = zxdh_np_se_smmu0_write,
		.p_read_fun = zxdh_np_se_smmu0_read,
	},
	{
		.reg_name = "cpu_ind_rdat1",
		.reg_no = 672,
		.module_no = SMMU0,
		.flags = ZXDH_REG_FLAG_DIRECT,
		.array_type = ZXDH_REG_NUL_ARRAY,
		.addr = ZXDH_SYS_SE_SMMU0_BASE_ADDR + ZXDH_MODULE_SE_SMMU0_BASE_ADDR + 0x48,
		.width = (32 / 8),
		.m_size = 0,
		.n_size = 0,
		.m_step = 0,
		.n_step = 0,
		.field_num = 1,
		.p_fields = g_smmu0_smmu0_cpu_ind_rdat1_reg,
		.p_write_fun = zxdh_np_se_smmu0_write,
		.p_read_fun = zxdh_np_se_smmu0_read,
	},
	{
		.reg_name = "cpu_ind_rdat2",
		.reg_no = 673,
		.module_no = SMMU0,
		.flags = ZXDH_REG_FLAG_DIRECT,
		.array_type = ZXDH_REG_NUL_ARRAY,
		.addr = ZXDH_SYS_SE_SMMU0_BASE_ADDR + ZXDH_MODULE_SE_SMMU0_BASE_ADDR + 0x4c,
		.width = (32 / 8),
		.m_size = 0,
		.n_size = 0,
		.m_step = 0,
		.n_step = 0,
		.field_num = 1,
		.p_fields = g_smmu0_smmu0_cpu_ind_rdat2_reg,
		.p_write_fun = zxdh_np_se_smmu0_write,
		.p_read_fun = zxdh_np_se_smmu0_read,
	},
	{
		.reg_name = "cpu_ind_rdat3",
		.reg_no = 674,
		.module_no = SMMU0,
		.flags = ZXDH_REG_FLAG_DIRECT,
		.array_type = ZXDH_REG_NUL_ARRAY,
		.addr = ZXDH_SYS_SE_SMMU0_BASE_ADDR + ZXDH_MODULE_SE_SMMU0_BASE_ADDR + 0x50,
		.width = (32 / 8),
		.m_size = 0,
		.n_size = 0,
		.m_step = 0,
		.n_step = 0,
		.field_num = 1,
		.p_fields = g_smmu0_smmu0_cpu_ind_rdat3_reg,
		.p_write_fun = zxdh_np_se_smmu0_write,
		.p_read_fun = zxdh_np_se_smmu0_read,
	},
	{
		.reg_name = "wr_arb_cpu_rdy",
		.reg_no = 676,
		.module_no = SMMU0,
		.flags = ZXDH_REG_FLAG_DIRECT,
		.array_type = ZXDH_REG_NUL_ARRAY,
		.addr = ZXDH_SYS_SE_SMMU0_BASE_ADDR + ZXDH_MODULE_SE_SMMU0_BASE_ADDR + 0x10c,
		.width = (32 / 8),
		.m_size = 0,
		.n_size = 0,
		.m_step = 0,
		.n_step = 0,
		.field_num = 1,
		.p_fields = g_smmu0_smmu0_wr_arb_cpu_rdy_reg,
		.p_write_fun = zxdh_np_se_smmu0_write,
		.p_read_fun = zxdh_np_se_smmu0_read,
	},
	{
		.reg_name = "info_queue_buf_space_left_0_127",
		.reg_no = 820,
		.module_no = DTB4K,
		.flags = ZXDH_REG_FLAG_DIRECT,
		.array_type = ZXDH_REG_UNI_ARRAY,
		.addr = ZXDH_SYS_DTB_BASE_ADDR + ZXDH_MODULE_DTB_ENQ_BASE_ADDR + 0xc,
		.width = (32 / 8),
		.m_size = 0,
		.n_size = 127 + 1,
		.m_step = 0,
		.n_step = 32,
		.field_num = 1,
		.p_fields = g_dtb4k_dtb_enq_info_queue_buf_space_left_0_127_reg,
		.p_write_fun = zxdh_np_write,
		.p_read_fun = zxdh_np_read,
	},
	{
		.reg_name = "cfg_epid_v_func_num_0_127",
		.reg_no = 821,
		.module_no = DTB4K,
		.flags = ZXDH_REG_FLAG_DIRECT,
		.array_type = ZXDH_REG_UNI_ARRAY,
		.addr = ZXDH_SYS_DTB_BASE_ADDR + ZXDH_MODULE_DTB_ENQ_BASE_ADDR + 0x10,
		.width = (32 / 8),
		.m_size = 0,
		.n_size = 127 + 1,
		.m_step = 0,
		.n_step = 32,
		.field_num = 7,
		.p_fields = g_dtb4k_dtb_enq_cfg_epid_v_func_num_0_127_reg,
		.p_write_fun = zxdh_np_write,
		.p_read_fun = zxdh_np_read,
	},
	{
		.reg_name = "cara_queue_ram0_159_0",
		.reg_no = 721,
		.module_no = STAT,
		.flags = ZXDH_REG_FLAG_INDIRECT,
		.array_type = ZXDH_REG_UNI_ARRAY,
		.addr = 0x000000 + 0x14000000,
		.width = (160 / 8),
		.m_size = 0,
		.n_size = 0x7FFF + 1,
		.m_step = 0,
		.n_step = 8,
		.field_num = 9,
		.p_fields = g_stat_car0_cara_queue_ram0_159_0_reg,
		.p_write_fun = NULL,
		.p_read_fun = NULL,
	},
	{
		.reg_name = "carb_queue_ram0_159_0",
		.reg_no = 738,
		.module_no = STAT,
		.flags = ZXDH_REG_FLAG_INDIRECT,
		.array_type = ZXDH_REG_UNI_ARRAY,
		.addr = 0x100000 + 0x14000000,
		.width = (160 / 8),
		.m_size = 0,
		.n_size = 0xFFF + 1,
		.m_step = 0,
		.n_step = 8,
		.field_num = 9,
		.p_fields = g_stat_car0_carb_queue_ram0_159_0_reg,
		.p_write_fun = NULL,
		.p_read_fun = NULL,
	},
	{
		.reg_name = "carc_queue_ram0_159_0",
		.reg_no = 755,
		.module_no = STAT,
		.flags = ZXDH_REG_FLAG_INDIRECT,
		.array_type = ZXDH_REG_UNI_ARRAY,
		.addr = 0x200000 + 0x14000000,
		.width = (160 / 8),
		.m_size = 0,
		.n_size = 0x3FF + 1,
		.m_step = 0,
		.n_step = 8,
		.field_num = 9,
		.p_fields = g_stat_car0_carc_queue_ram0_159_0_reg,
		.p_write_fun = NULL,
		.p_read_fun = NULL,
	},
	{
		.reg_name = "pktrx_glbal_cfg_0",
		.reg_no = 448,
		.module_no = NPPU,
		.flags = ZXDH_REG_FLAG_DIRECT,
		.array_type = ZXDH_REG_NUL_ARRAY,
		.addr = ZXDH_SYS_NPPU_BASE_ADDR + ZXDH_MODULE_NPPU_PKTRX_CFG_BASE_ADDR + 0x01f8,
		.width = (32 / 8),
		.m_size = 0,
		.n_size = 0,
		.m_step = 0,
		.n_step = 0,
		.field_num = 1,
		.p_fields = g_nppu_pktrx_cfg_pktrx_glbal_cfg_0_reg,
		.p_write_fun = NULL,
		.p_read_fun = NULL,
	},
};

static uint32_t
zxdh_np_reg_get_reg_addr(uint32_t reg_no, uint32_t m_offset, uint32_t n_offset)
{
	uint32_t	 addr		= 0;
	ZXDH_REG_T  *p_reg_info = NULL;

	p_reg_info = &g_dpp_reg_info[reg_no];

	addr = p_reg_info->addr;

	if (p_reg_info->array_type & ZXDH_REG_UNI_ARRAY) {
		if (n_offset > (p_reg_info->n_size - 1))
			PMD_DRV_LOG(ERR, "reg n_offset is out of range, reg_no:%u", reg_no);

		addr += n_offset * p_reg_info->n_step;
	} else if (p_reg_info->array_type & ZXDH_REG_BIN_ARRAY) {
		if ((n_offset > (p_reg_info->n_size - 1)) || (m_offset > (p_reg_info->m_size - 1)))
			PMD_DRV_LOG(ERR, "reg n_offset/m_offset out of range, reg_no:%u", reg_no);

		addr += m_offset * p_reg_info->m_step + n_offset * p_reg_info->n_step;
	}

	return addr;
}

static uint32_t
zxdh_np_dev_add(uint32_t  dev_id, ZXDH_DEV_TYPE_E dev_type,
		ZXDH_DEV_ACCESS_TYPE_E  access_type, uint64_t  pcie_addr,
		uint64_t  riscv_addr, uint64_t  dma_vir_addr,
		uint64_t  dma_phy_addr)
{
	ZXDH_DEV_CFG_T *p_dev_info = NULL;
	ZXDH_DEV_MGR_T *p_dev_mgr  = NULL;
	uint32_t i = 0;

	p_dev_mgr = &g_dev_mgr;
	if (!p_dev_mgr->is_init) {
		PMD_DRV_LOG(ERR, "ErrorCode[ 0x%x]: Device Manager is not init",
								 ZXDH_RC_DEV_MGR_NOT_INIT);
		return ZXDH_RC_DEV_MGR_NOT_INIT;
	}

	if (p_dev_mgr->p_dev_array[ZXDH_DEV_SLOT_ID(dev_id)] != NULL) {
		/* device is already exist. */
		PMD_DRV_LOG(ERR, "Device is added again");
		p_dev_info = p_dev_mgr->p_dev_array[ZXDH_DEV_SLOT_ID(dev_id)];
	} else {
		/* device is new. */
		p_dev_info = rte_malloc(NULL, sizeof(ZXDH_DEV_CFG_T), 0);
		if (p_dev_info == NULL) {
			PMD_DRV_LOG(ERR, "malloc memory failed");
			return ZXDH_PAR_CHK_POINT_NULL;
		}
		p_dev_mgr->p_dev_array[ZXDH_DEV_SLOT_ID(dev_id)] = p_dev_info;
		p_dev_mgr->device_num++;
	}

	p_dev_info->slot_id   = ZXDH_DEV_SLOT_ID(dev_id);
	p_dev_info->dev_type    = dev_type;
	p_dev_info->access_type = access_type;
	p_dev_info->pcie_addr[ZXDH_DEV_PF_INDEX(dev_id)] = pcie_addr;
	p_dev_info->pcie_id[ZXDH_DEV_PF_INDEX(dev_id)] = ZXDH_DEV_PCIE_ID(dev_id);
	p_dev_info->riscv_addr   = riscv_addr;
	p_dev_info->dma_vir_addr = dma_vir_addr;
	p_dev_info->dma_phy_addr = dma_phy_addr;

	p_dev_info->p_pcie_write_fun = zxdh_np_dev_pcie_default_write;
	p_dev_info->p_pcie_read_fun  = zxdh_np_dev_pcie_default_read;

	rte_spinlock_init(&p_dev_info->dtb_spinlock.spinlock);

	rte_spinlock_init(&p_dev_info->smmu0_spinlock.spinlock);

	for (i = 0; i < ZXDH_DTB_QUEUE_NUM_MAX; i++)
		rte_spinlock_init(&p_dev_info->dtb_queue_spinlock[i].spinlock);

	for (i = 0; i < ZXDH_HASH_FUNC_ID_NUM; i++)
		rte_spinlock_init(&p_dev_info->hash_spinlock[i].spinlock);

	return ZXDH_OK;
}

static uint32_t
zxdh_np_dev_agent_status_set(uint32_t dev_id, uint32_t agent_flag)
{
	ZXDH_DEV_MGR_T *p_dev_mgr = NULL;
	ZXDH_DEV_CFG_T *p_dev_info = NULL;

	p_dev_mgr = &g_dev_mgr;
	p_dev_info = p_dev_mgr->p_dev_array[ZXDH_DEV_SLOT_ID(dev_id)];

	if (p_dev_info == NULL)
		return ZXDH_DEV_TYPE_INVALID;
	p_dev_info->agent_flag = agent_flag;

	return 0;
}

static void
zxdh_np_sdt_mgr_init(void)
{
	if (!g_sdt_mgr.is_init) {
		g_sdt_mgr.channel_num = 0;
		g_sdt_mgr.is_init = 1;
		memset(g_sdt_mgr.sdt_tbl_array, 0, ZXDH_DEV_CHANNEL_MAX *
			sizeof(ZXDH_SDT_SOFT_TABLE_T *));
	}
}

static uint32_t
zxdh_np_sdt_mgr_create(uint32_t dev_id)
{
	ZXDH_SDT_SOFT_TABLE_T *p_sdt_tbl_temp = NULL;
	ZXDH_SDT_MGR_T *p_sdt_mgr = NULL;

	p_sdt_mgr = ZXDH_SDT_MGR_PTR_GET();

	if (ZXDH_SDT_SOFT_TBL_GET(dev_id) == NULL) {
		p_sdt_tbl_temp = rte_malloc(NULL, sizeof(ZXDH_SDT_SOFT_TABLE_T), 0);

		p_sdt_tbl_temp->device_id = dev_id;
		memset(p_sdt_tbl_temp->sdt_array, 0, ZXDH_DEV_SDT_ID_MAX * sizeof(ZXDH_SDT_ITEM_T));

		ZXDH_SDT_SOFT_TBL_GET(dev_id) = p_sdt_tbl_temp;

		p_sdt_mgr->channel_num++;
	} else {
		PMD_DRV_LOG(ERR, "called repeatedly!");
		return 1;
	}

	return 0;
}

static uint32_t
zxdh_np_sdt_init(uint32_t dev_num, uint32_t *dev_id_array)
{
	uint32_t rc;
	uint32_t i;

	zxdh_np_sdt_mgr_init();

	for (i = 0; i < dev_num; i++) {
		rc = zxdh_np_sdt_mgr_create(dev_id_array[i]);
		ZXDH_COMM_CHECK_RC(rc, "zxdh_sdt_mgr_create");
	}

	return rc;
}

static uint32_t
zxdh_np_sdt_mgr_sdt_item_add(uint32_t dev_id, uint32_t sdt_no,
		uint32_t sdt_hig32, uint32_t sdt_low32)
{
	ZXDH_SDT_SOFT_TABLE_T *p_sdt_soft_tbl = NULL;
	ZXDH_SDT_ITEM_T *p_sdt_item = NULL;

	p_sdt_soft_tbl = ZXDH_SDT_SOFT_TBL_GET(dev_id);

	if (p_sdt_soft_tbl == NULL) {
		PMD_DRV_LOG(ERR, "soft sdt table not init!");
		RTE_ASSERT(0);
		return ZXDH_RC_TABLE_SDT_MGR_INVALID;
	}

	if (dev_id != p_sdt_soft_tbl->device_id) {
		PMD_DRV_LOG(ERR, "soft sdt table item invalid!");
		RTE_ASSERT(0);
		return ZXDH_RC_TABLE_PARA_INVALID;
	}

	p_sdt_item = &p_sdt_soft_tbl->sdt_array[sdt_no];
	p_sdt_item->valid = ZXDH_SDT_VALID;
	p_sdt_item->table_cfg[0] = sdt_hig32;
	p_sdt_item->table_cfg[1] = sdt_low32;

	PMD_DRV_LOG(DEBUG, "0x%08x 0x%08x", p_sdt_item->table_cfg[0], p_sdt_item->table_cfg[1]);

	return ZXDH_OK;
}

static uint32_t
zxdh_np_sdt_mgr_sdt_item_del(uint32_t dev_id, uint32_t sdt_no)
{
	ZXDH_SDT_SOFT_TABLE_T *p_sdt_soft_tbl = NULL;
	ZXDH_SDT_ITEM_T *p_sdt_item = NULL;

	p_sdt_soft_tbl = ZXDH_SDT_SOFT_TBL_GET(dev_id);

	if (p_sdt_soft_tbl != NULL) {
		if (dev_id != p_sdt_soft_tbl->device_id) {
			PMD_DRV_LOG(ERR, "soft table item invalid !");
			RTE_ASSERT(0);
			return ZXDH_RC_TABLE_PARA_INVALID;
		}

		p_sdt_item = &p_sdt_soft_tbl->sdt_array[sdt_no];
		p_sdt_item->valid = ZXDH_SDT_INVALID;
		p_sdt_item->table_cfg[0] = 0;
		p_sdt_item->table_cfg[1] = 0;
	}
	PMD_DRV_LOG(DEBUG, "sdt_no: 0x%08x", sdt_no);
	return ZXDH_OK;
}

static void
zxdh_np_soft_sdt_tbl_set(uint32_t dev_id,
						uint32_t sdt_no,
						uint32_t table_type,
						ZXDH_SDT_TBL_DATA_T *p_sdt_info)
{
	g_table_type[ZXDH_DEV_SLOT_ID(dev_id)][sdt_no] = table_type;
	g_sdt_info[ZXDH_DEV_SLOT_ID(dev_id)][sdt_no].data_high32 = p_sdt_info->data_high32;
	g_sdt_info[ZXDH_DEV_SLOT_ID(dev_id)][sdt_no].data_low32  = p_sdt_info->data_low32;
}

static uint32_t
zxdh_np_sdt_tbl_write(uint32_t dev_id,
					uint32_t sdt_no,
					uint32_t table_type,
					void *p_sdt_info,
					uint32_t opr_type)
{
	uint32_t rtn = 0;

	ZXDH_SDT_TBL_DATA_T sdt_tbl = {0};
	ZXDH_SDT_TBL_ERAM_T *p_sdt_eram = NULL;
	ZXDH_SDT_TBL_HASH_T *p_sdt_hash = NULL;
	ZXDH_SDT_TBL_ETCAM_T *p_sdt_etcam = NULL;
	ZXDH_SDT_TBL_PORTTBL_T *p_sdt_porttbl = NULL;

	PMD_DRV_LOG(DEBUG, "sdt: %u", sdt_no);

	if (opr_type) {
		zxdh_np_soft_sdt_tbl_set(dev_id, sdt_no, 0, &sdt_tbl);

		rtn = zxdh_np_sdt_mgr_sdt_item_del(dev_id, sdt_no);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rtn, "zxdh_np_sdt_mgr_sdt_item_del");
	} else {
		switch (table_type) {
		case ZXDH_SDT_TBLT_ERAM:
			p_sdt_eram = (ZXDH_SDT_TBL_ERAM_T *)p_sdt_info;
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_high32,
				p_sdt_eram->eram_mode,
				ZXDH_SDT_H_ERAM_MODE_BT_POS,
				ZXDH_SDT_H_ERAM_MODE_BT_LEN);
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_high32,
				p_sdt_eram->eram_base_addr,
				ZXDH_SDT_H_ERAM_BASE_ADDR_BT_POS,
				ZXDH_SDT_H_ERAM_BASE_ADDR_BT_LEN);
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_low32,
				p_sdt_eram->eram_table_depth,
				ZXDH_SDT_L_ERAM_TABLE_DEPTH_BT_POS,
				ZXDH_SDT_L_ERAM_TABLE_DEPTH_BT_LEN);
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_low32,
				p_sdt_eram->eram_clutch_en,
				ZXDH_SDT_L_CLUTCH_EN_BT_POS,
				ZXDH_SDT_L_CLUTCH_EN_BT_LEN);
			break;

		case ZXDH_SDT_TBLT_HASH:
			p_sdt_hash = (ZXDH_SDT_TBL_HASH_T *)p_sdt_info;
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_high32,
				p_sdt_hash->hash_id,
				ZXDH_SDT_H_HASH_ID_BT_POS,
				ZXDH_SDT_H_HASH_ID_BT_LEN);
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_high32,
				p_sdt_hash->hash_table_width,
				ZXDH_SDT_H_HASH_TABLE_WIDTH_BT_POS,
				ZXDH_SDT_H_HASH_TABLE_WIDTH_BT_LEN);
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_high32,
				p_sdt_hash->key_size,
				ZXDH_SDT_H_HASH_KEY_SIZE_BT_POS,
				ZXDH_SDT_H_HASH_KEY_SIZE_BT_LEN);
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_high32,
				p_sdt_hash->hash_table_id,
				ZXDH_SDT_H_HASH_TABLE_ID_BT_POS,
				ZXDH_SDT_H_HASH_TABLE_ID_BT_LEN);
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_high32,
				p_sdt_hash->learn_en,
				ZXDH_SDT_H_LEARN_EN_BT_POS,
				ZXDH_SDT_H_LEARN_EN_BT_LEN);
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_high32,
				p_sdt_hash->keep_alive,
				ZXDH_SDT_H_KEEP_ALIVE_BT_POS,
				ZXDH_SDT_H_KEEP_ALIVE_BT_LEN);
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_high32,
				((p_sdt_hash->keep_alive_baddr) >>
				ZXDH_SDT_L_KEEP_ALIVE_BADDR_BT_LEN),
				ZXDH_SDT_H_KEEP_ALIVE_BADDR_BT_POS,
				ZXDH_SDT_H_KEEP_ALIVE_BADDR_BT_LEN);
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_low32,
				ZXDH_SDT_GET_LOW_DATA((p_sdt_hash->keep_alive_baddr),
				ZXDH_SDT_L_KEEP_ALIVE_BADDR_BT_LEN),
				ZXDH_SDT_L_KEEP_ALIVE_BADDR_BT_POS,
				ZXDH_SDT_L_KEEP_ALIVE_BADDR_BT_LEN);
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_low32,
				p_sdt_hash->rsp_mode,
				ZXDH_SDT_L_RSP_MODE_BT_POS,
				ZXDH_SDT_L_RSP_MODE_BT_LEN);
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_low32,
				p_sdt_hash->hash_clutch_en,
				ZXDH_SDT_L_CLUTCH_EN_BT_POS,
				ZXDH_SDT_L_CLUTCH_EN_BT_LEN);
			break;

		case ZXDH_SDT_TBLT_ETCAM:
			p_sdt_etcam = (ZXDH_SDT_TBL_ETCAM_T *)p_sdt_info;
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_high32,
				p_sdt_etcam->etcam_id,
				ZXDH_SDT_H_ETCAM_ID_BT_POS,
				ZXDH_SDT_H_ETCAM_ID_BT_LEN);
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_high32,
				p_sdt_etcam->etcam_key_mode,
				ZXDH_SDT_H_ETCAM_KEY_MODE_BT_POS,
				ZXDH_SDT_H_ETCAM_KEY_MODE_BT_LEN);
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_high32,
				p_sdt_etcam->etcam_table_id,
				ZXDH_SDT_H_ETCAM_TABLE_ID_BT_POS,
				ZXDH_SDT_H_ETCAM_TABLE_ID_BT_LEN);
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_high32,
				p_sdt_etcam->no_as_rsp_mode,
				ZXDH_SDT_H_ETCAM_NOAS_RSP_MODE_BT_POS,
				ZXDH_SDT_H_ETCAM_NOAS_RSP_MODE_BT_LEN);
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_high32,
				p_sdt_etcam->as_en,
				ZXDH_SDT_H_ETCAM_AS_EN_BT_POS,
				ZXDH_SDT_H_ETCAM_AS_EN_BT_LEN);

			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_high32,
				((p_sdt_etcam->as_eram_baddr) >>
				ZXDH_SDT_L_ETCAM_AS_ERAM_BADDR_BT_LEN),
				ZXDH_SDT_H_ETCAM_AS_ERAM_BADDR_BT_POS,
				ZXDH_SDT_H_ETCAM_AS_ERAM_BADDR_BT_LEN);
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_low32,
				ZXDH_SDT_GET_LOW_DATA((p_sdt_etcam->as_eram_baddr),
				ZXDH_SDT_L_ETCAM_AS_ERAM_BADDR_BT_LEN),
				ZXDH_SDT_L_ETCAM_AS_ERAM_BADDR_BT_POS,
				ZXDH_SDT_L_ETCAM_AS_ERAM_BADDR_BT_LEN);

			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_low32, p_sdt_etcam->as_rsp_mode,
				ZXDH_SDT_L_ETCAM_AS_RSP_MODE_BT_POS,
				ZXDH_SDT_L_ETCAM_AS_RSP_MODE_BT_LEN);
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_low32,
				p_sdt_etcam->etcam_table_depth, ZXDH_SDT_L_ETCAM_TABLE_DEPTH_BT_POS,
				ZXDH_SDT_L_ETCAM_TABLE_DEPTH_BT_LEN);
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_low32,
				p_sdt_etcam->etcam_clutch_en, ZXDH_SDT_L_CLUTCH_EN_BT_POS,
				ZXDH_SDT_L_CLUTCH_EN_BT_LEN);
			break;

		case ZXDH_SDT_TBLT_PORTTBL:
			p_sdt_porttbl = (ZXDH_SDT_TBL_PORTTBL_T *)p_sdt_info;
			ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_low32,
				p_sdt_porttbl->porttbl_clutch_en, ZXDH_SDT_L_CLUTCH_EN_BT_POS,
				ZXDH_SDT_L_CLUTCH_EN_BT_LEN);
			break;

		default:
			PMD_DRV_LOG(ERR, "SDT table_type[ %u ] is invalid!",
				table_type);
			return ZXDH_ERR;
		}

		ZXDH_COMM_UINT32_WRITE_BITS(sdt_tbl.data_high32, table_type,
			ZXDH_SDT_H_TBL_TYPE_BT_POS, ZXDH_SDT_H_TBL_TYPE_BT_LEN);
		zxdh_np_soft_sdt_tbl_set(dev_id, sdt_no, table_type, &sdt_tbl);

		rtn = zxdh_np_sdt_mgr_sdt_item_add(dev_id, sdt_no, sdt_tbl.data_high32,
			sdt_tbl.data_low32);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rtn, "zxdh_np_sdt_mgr_sdt_item_add");
	}

	return ZXDH_OK;
}

static void
zxdh_np_ppu_parse_cls_bitmap(uint32_t dev_id,
								uint32_t bitmap)
{
	uint32_t cls_id;
	uint32_t mem_id;
	uint32_t cls_use;
	uint32_t instr_mem;

	for (cls_id = 0; cls_id < ZXDH_PPU_CLUSTER_NUM; cls_id++) {
		cls_use = (bitmap >> cls_id) & 0x1;
		g_ppu_cls_bit_map[ZXDH_DEV_SLOT_ID(dev_id)].cls_use[cls_id] = cls_use;
	}

	for (mem_id = 0; mem_id < ZXDH_PPU_INSTR_MEM_NUM; mem_id++) {
		instr_mem = (bitmap >> (mem_id * 2)) & 0x3;
		g_ppu_cls_bit_map[ZXDH_DEV_SLOT_ID(dev_id)].instr_mem[mem_id] =
		((instr_mem > 0) ? 1 : 0);
	}
}

static void
zxdh_np_agent_msg_prt(uint8_t type, uint32_t rtn)
{
	switch (rtn) {
	case ZXDH_RC_CTRLCH_MSG_LEN_ZERO:
		PMD_DRV_LOG(ERR, "type[%u]:msg len is zero!", type);
		break;
	case ZXDH_RC_CTRLCH_MSG_PRO_ERR:
		PMD_DRV_LOG(ERR, "type[%u]:msg process error!", type);
		break;
	case ZXDH_RC_CTRLCH_MSG_TYPE_NOT_SUPPORT:
		PMD_DRV_LOG(ERR, "type[%u]:fw not support the msg!", type);
		break;
	case ZXDH_RC_CTRLCH_MSG_OPER_NOT_SUPPORT:
		PMD_DRV_LOG(ERR, "type[%u]:fw not support opr of the msg!", type);
		break;
	case ZXDH_RC_CTRLCH_MSG_DROP:
		PMD_DRV_LOG(ERR, "type[%u]:fw not support,drop msg!", type);
		break;
	default:
		break;
	}
}

static uint32_t
zxdh_np_agent_bar_msg_check(uint32_t dev_id, ZXDH_AGENT_CHANNEL_MSG_T *p_msg)
{
	uint8_t type = 0;
	uint32_t bar_msg_num = 0;

	type = *((uint8_t *)(p_msg->msg) + 1);
	if (type != ZXDH_PCIE_BAR_MSG) {
		zxdh_np_dev_fw_bar_msg_num_get(dev_id, &bar_msg_num);
		if (type >= bar_msg_num) {
			PMD_DRV_LOG(ERR, "type[%u] > fw_bar_msg_num[%u]!", type, bar_msg_num);
			return ZXDH_RC_CTRLCH_MSG_TYPE_NOT_SUPPORT;
		}
	}

	return ZXDH_OK;
}

static uint32_t
zxdh_np_agent_channel_sync_send(uint32_t dev_id,
				ZXDH_AGENT_CHANNEL_MSG_T *p_msg,
				uint32_t *p_data,
				uint32_t rep_len)
{
	uint32_t ret = ZXDH_OK;
	uint32_t vport = 0;
	struct zxdh_pci_bar_msg in = {0};
	struct zxdh_msg_recviver_mem result = {0};
	uint32_t *recv_buffer = NULL;
	uint8_t *reply_ptr = NULL;
	uint16_t reply_msg_len = 0;
	uint64_t agent_addr = 0;
	uint16_t bar_pcie_id = 0;

	ret = zxdh_np_agent_bar_msg_check(dev_id, p_msg);
	if (ret != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "zxdh_np_agent_bar_msg_check failed!");
		return ret;
	}

	zxdh_np_dev_vport_get(dev_id, &vport);
	zxdh_np_dev_agent_addr_get(dev_id, &agent_addr);
	zxdh_np_dev_bar_pcie_id_get(dev_id, &bar_pcie_id);

	if (ZXDH_IS_PF(vport))
		in.src = ZXDH_MSG_CHAN_END_PF;
	else
		in.src = ZXDH_MSG_CHAN_END_VF;

	in.virt_addr = agent_addr;
	in.payload_addr = p_msg->msg;
	in.payload_len = p_msg->msg_len;
	in.dst = ZXDH_MSG_CHAN_END_RISC;
	in.module_id = ZXDH_BAR_MDOULE_NPSDK;
	in.src_pcieid = bar_pcie_id;

	recv_buffer = rte_zmalloc(NULL, rep_len + ZXDH_CHANNEL_REPS_LEN, 0);
	if (recv_buffer == NULL) {
		PMD_DRV_LOG(ERR, "malloc memory failed");
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	result.buffer_len = rep_len + ZXDH_CHANNEL_REPS_LEN;
	result.recv_buffer = recv_buffer;

	ret = zxdh_bar_chan_sync_msg_send(&in, &result);
	if (ret == ZXDH_BAR_MSG_OK) {
		reply_ptr = (uint8_t *)(result.recv_buffer);
		if (*reply_ptr == 0XFF) {
			reply_msg_len = *(uint16_t *)(reply_ptr + 1);
			memcpy(p_data, reply_ptr + 4,
				((reply_msg_len > rep_len) ? rep_len : reply_msg_len));
		} else {
			PMD_DRV_LOG(ERR, "Message not replied");
		}
	} else {
		PMD_DRV_LOG(ERR, "Error[0x%x], bar msg send failed!", ret);
	}

	rte_free(recv_buffer);
	return ret;
}

static uint32_t
zxdh_np_agent_channel_reg_sync_send(uint32_t dev_id,
	ZXDH_AGENT_CHANNEL_REG_MSG_T *p_msg, uint32_t *p_data, uint32_t rep_len)
{
	uint32_t ret = ZXDH_OK;
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_msg);
	ZXDH_AGENT_CHANNEL_MSG_T agent_msg = {
		.msg = (void *)p_msg,
		.msg_len = sizeof(ZXDH_AGENT_CHANNEL_REG_MSG_T),
	};

	ret = zxdh_np_agent_channel_sync_send(dev_id, &agent_msg, p_data, rep_len);
	if (ret != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "zxdh_np_agent_channel_sync_send failed");
		return ZXDH_ERR;
	}

	ret = *p_data;
	if (ret != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "zxdh_np_agent_channel_sync_send failed in buffer");
		return ZXDH_ERR;
	}

	return ret;
}

static uint32_t
zxdh_np_agent_channel_pcie_bar_request(uint32_t dev_id,
									uint32_t *p_bar_msg_num)
{
	uint32_t rc = ZXDH_OK;
	uint32_t rsp_buff[2] = {0};
	uint32_t msg_result = 0;
	uint32_t bar_msg_num = 0;
	ZXDH_AGENT_PCIE_BAR_MSG_T msgcfg = {
		.dev_id = 0,
		.type   = ZXDH_PCIE_BAR_MSG,
		.oper   = ZXDH_BAR_MSG_NUM_REQ,
	};
	ZXDH_AGENT_CHANNEL_MSG_T agent_msg = {
		.msg = (void *)&msgcfg,
		.msg_len = sizeof(ZXDH_AGENT_PCIE_BAR_MSG_T),
	};

	rc = zxdh_np_agent_channel_sync_send(dev_id, &agent_msg, rsp_buff, sizeof(rsp_buff));
	if (rc != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "zxdh_np_agent_channel_sync_send failed!");
		return rc;
	}

	msg_result = rsp_buff[0];
	bar_msg_num = rsp_buff[1];

	zxdh_np_agent_msg_prt(msgcfg.type, msg_result);

	*p_bar_msg_num = bar_msg_num;

	return msg_result;
}

static uint32_t
zxdh_np_agent_channel_reg_read(uint32_t dev_id,
							uint32_t reg_type,
							uint32_t reg_no,
							uint32_t reg_width,
							uint32_t addr,
							uint32_t *p_data)
{
	uint32_t ret = 0;
	ZXDH_AGENT_CHANNEL_REG_MSG_T msgcfg = {
		.dev_id  = 0,
		.type    = ZXDH_REG_MSG,
		.subtype = reg_type,
		.oper    = ZXDH_RD,
		.reg_no  = reg_no,
		.addr	 = addr,
		.val_len = reg_width / 4,
	};

	uint32_t resp_len = reg_width + 4;
	uint8_t *resp_buffer = rte_zmalloc(NULL, resp_len, 0);
	if (resp_buffer == NULL) {
		PMD_DRV_LOG(ERR, "malloc memory failed");
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	ret = zxdh_np_agent_channel_reg_sync_send(dev_id,
		&msgcfg, (uint32_t *)resp_buffer, resp_len);
	if (ret != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "dev id %u reg_no %u send agent read failed.", dev_id, reg_no);
		rte_free(resp_buffer);
		return ZXDH_ERR;
	}

	if (*((uint32_t *)resp_buffer) != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "dev id %u reg_no %u agent read resp err %u .",
			dev_id, reg_no, *((uint32_t *)resp_buffer));
		rte_free(resp_buffer);
		return ZXDH_ERR;
	}

	memcpy(p_data, resp_buffer + 4, reg_width);

	rte_free(resp_buffer);

	return ret;
}

static uint32_t
zxdh_np_agent_channel_reg_write(uint32_t dev_id,
							uint32_t reg_type,
							uint32_t reg_no,
							uint32_t reg_width,
							uint32_t addr,
							uint32_t *p_data)
{
	uint32_t ret = ZXDH_OK;
	ZXDH_AGENT_CHANNEL_REG_MSG_T msgcfg = {
		.dev_id  = 0,
		.type    = ZXDH_REG_MSG,
		.subtype = reg_type,
		.oper    = ZXDH_WR,
		.reg_no  = reg_no,
		.addr	 = addr,
		.val_len = reg_width / 4,
	};

	memcpy(msgcfg.val, p_data, reg_width);

	uint32_t resp_len = reg_width + 4;
	uint8_t *resp_buffer = rte_zmalloc(NULL, resp_len, 0);
	if (resp_buffer == NULL) {
		PMD_DRV_LOG(ERR, "malloc memory failed");
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	ret = zxdh_np_agent_channel_reg_sync_send(dev_id,
		&msgcfg, (uint32_t *)resp_buffer, resp_len);

	if (ret != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "dev id %u reg_no %u send agent write failed.", dev_id, reg_no);
		rte_free(resp_buffer);
		return ZXDH_ERR;
	}

	if (*((uint32_t *)resp_buffer) != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "dev id %u reg_no %u agent write resp err %u .",
			dev_id, reg_no, *((uint32_t *)resp_buffer));
		rte_free(resp_buffer);
		return ZXDH_ERR;
	}

	memcpy(p_data, resp_buffer + 4, reg_width);

	rte_free(resp_buffer);

	return ret;
}

static uint32_t
zxdh_np_agent_channel_dtb_sync_send(uint32_t dev_id,
							ZXDH_AGENT_CHANNEL_DTB_MSG_T *p_msg,
							uint32_t *p_data,
							uint32_t rep_len)
{
	uint32_t ret = ZXDH_OK;

	ZXDH_AGENT_CHANNEL_MSG_T agent_msg = {0};
	agent_msg.msg = (void *)p_msg;
	agent_msg.msg_len = sizeof(ZXDH_AGENT_CHANNEL_DTB_MSG_T);

	ret = zxdh_np_agent_channel_sync_send(dev_id, &agent_msg, p_data, rep_len);
	if (ret != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "zxdh_np_agent_channel_sync_send failed");
		return ZXDH_ERR;
	}

	return ZXDH_OK;
}

static uint32_t
zxdh_np_agent_channel_dtb_queue_request(uint32_t dev_id,
									char p_name[32],
									uint32_t vport_info,
									uint32_t *p_queue_id)
{
	uint32_t rc = ZXDH_OK;

	uint32_t rsp_buff[2] = {0};
	uint32_t msg_result = 0;
	uint32_t queue_id = 0;
	ZXDH_AGENT_CHANNEL_DTB_MSG_T msgcfg = {
		.dev_id  = 0,
		.type    = ZXDH_DTB_MSG,
		.oper    = ZXDH_QUEUE_REQUEST,
		.vport   = vport_info,
	};
	memcpy(msgcfg.name, p_name, strnlen(p_name, ZXDH_PORT_NAME_MAX));

	PMD_DRV_LOG(DEBUG, "msgcfg.name=%s", msgcfg.name);

	rc = zxdh_np_agent_channel_dtb_sync_send(dev_id, &msgcfg, rsp_buff, sizeof(rsp_buff));
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_agent_channel_dtb_sync_send");

	msg_result = rsp_buff[0];
	queue_id = rsp_buff[1];

	PMD_DRV_LOG(DEBUG, "dev_id: %u, msg_result: %u", dev_id, msg_result);
	PMD_DRV_LOG(DEBUG, "dev_id: %u, queue_id: %u", dev_id, queue_id);

	*p_queue_id = queue_id;

	return msg_result;
}

static uint32_t
zxdh_np_agent_channel_dtb_queue_release(uint32_t dev_id,
								char p_name[32],
								__rte_unused uint32_t queue_id)
{
	uint32_t rc = ZXDH_OK;

	uint32_t msg_result = 0;
	uint32_t rsp_buff[2] = {0};
	ZXDH_AGENT_CHANNEL_DTB_MSG_T msgcfg = {
		.dev_id  = 0,
		.type    = ZXDH_DTB_MSG,
		.oper    = ZXDH_QUEUE_RELEASE,
		.queue_id = queue_id,
	};

	memcpy(msgcfg.name, p_name, strnlen(p_name, ZXDH_PORT_NAME_MAX));

	rc = zxdh_np_agent_channel_dtb_sync_send(dev_id, &msgcfg, rsp_buff, sizeof(rsp_buff));
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_agent_channel_dtb_sync_send");

	msg_result = rsp_buff[0];
	PMD_DRV_LOG(DEBUG, "msg_result: %u", msg_result);

	return msg_result;
}

static uint32_t
zxdh_np_agent_channel_se_res_get(uint32_t dev_id,
								uint32_t sub_type,
								uint32_t opr,
								uint32_t *p_rsp_buff,
								uint32_t buff_size)
{
	uint32_t rc = ZXDH_OK;

	uint32_t msg_result = 0;
	ZXDH_AGENT_SE_RES_MSG_T msgcfg = {
		.dev_id   = 0,
		.type     = ZXDH_RES_MSG,
		.sub_type = sub_type,
		.oper     = opr,
	};
	ZXDH_AGENT_CHANNEL_MSG_T agent_msg = {
		.msg = (void *)&msgcfg,
		.msg_len = sizeof(ZXDH_AGENT_SE_RES_MSG_T),
	};

	rc = zxdh_np_agent_channel_sync_send(dev_id, &agent_msg, p_rsp_buff, buff_size);
	if (rc != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "agent send msg failed");
		return ZXDH_ERR;
	}

	msg_result = p_rsp_buff[0];
	PMD_DRV_LOG(DEBUG, "msg_result: 0x%x", msg_result);
	zxdh_np_agent_msg_prt(msgcfg.type, msg_result);

	return msg_result;
}

static uint32_t
zxdh_np_agent_channel_acl_index_request(uint32_t dev_id, uint32_t sdt_no,
			uint32_t vport, uint32_t *p_index)
{
	uint32_t rsp_buff[2] = {0};
	ZXDH_AGENT_CHANNEL_ACL_MSG_T msgcfg = {
		.dev_id = 0,
		.type = ZXDH_ACL_MSG,
		.oper = ZXDH_ACL_INDEX_REQUEST,
		.vport = vport,
		.sdt_no = sdt_no,
	};
	ZXDH_AGENT_CHANNEL_MSG_T agent_msg = {
		.msg = (void *)&msgcfg,
		.msg_len = sizeof(ZXDH_AGENT_CHANNEL_ACL_MSG_T),
	};

	uint32_t rc = zxdh_np_agent_channel_sync_send(dev_id,
					&agent_msg, rsp_buff, sizeof(rsp_buff));
	if (rc != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "agent send msg failed");
		return ZXDH_ERR;
	}

	uint32_t msg_result = rsp_buff[0];
	uint32_t acl_index = rsp_buff[1];

	PMD_DRV_LOG(DEBUG, "dev_id: %u, msg_result: %u", dev_id, msg_result);
	PMD_DRV_LOG(DEBUG, "dev_id: %u, acl_index: %u", dev_id, acl_index);

	*p_index = acl_index;

	return msg_result;
}

static uint32_t
zxdh_np_agent_channel_acl_index_release(uint32_t dev_id, uint32_t rel_type,
			uint32_t sdt_no, uint32_t vport, uint32_t index)
{
	uint32_t rsp_buff[2] = {0};
	ZXDH_AGENT_CHANNEL_ACL_MSG_T msgcfg = {
		.dev_id = 0,
		.type = ZXDH_ACL_MSG,
		.oper = rel_type,
		.index = index,
		.sdt_no = sdt_no,
		.vport = vport,
	};
	ZXDH_AGENT_CHANNEL_MSG_T agent_msg = {
		.msg = (void *)&msgcfg,
		.msg_len = sizeof(ZXDH_AGENT_CHANNEL_ACL_MSG_T),
	};

	uint32_t rc = zxdh_np_agent_channel_sync_send(dev_id,
					&agent_msg, rsp_buff, sizeof(rsp_buff));
	if (rc != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "agent send msg failed");
		return ZXDH_ERR;
	}

	uint32_t msg_result = rsp_buff[0];
	PMD_DRV_LOG(DEBUG, "msg_result: %u", msg_result);

	return msg_result;
}

static ZXDH_DTB_MGR_T *
zxdh_np_dtb_mgr_get(uint32_t dev_id)
{
	if (ZXDH_DEV_SLOT_ID(dev_id) >= ZXDH_DEV_CHANNEL_MAX)
		return NULL;
	else
		return p_dpp_dtb_mgr[ZXDH_DEV_SLOT_ID(dev_id)];
}

static uint32_t
zxdh_np_dtb_mgr_create(uint32_t dev_id)
{
	if (p_dpp_dtb_mgr[ZXDH_DEV_SLOT_ID(dev_id)] != NULL) {
		PMD_DRV_LOG(ERR, "ErrorCode[0x%x]: Dma Manager"
			" is exist!!!", ZXDH_RC_DTB_MGR_EXIST);
		return ZXDH_RC_DTB_MGR_EXIST;
	}

	p_dpp_dtb_mgr[ZXDH_DEV_SLOT_ID(dev_id)] = rte_zmalloc(NULL, sizeof(ZXDH_DTB_MGR_T), 0);
	if (p_dpp_dtb_mgr[ZXDH_DEV_SLOT_ID(dev_id)] == NULL) {
		PMD_DRV_LOG(ERR, "malloc memory failed");
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	return ZXDH_OK;
}

static uint32_t
zxdh_np_dtb_soft_init(uint32_t dev_id)
{
	ZXDH_DTB_MGR_T *p_dtb_mgr = NULL;

	if (ZXDH_DEV_SLOT_ID(dev_id) >= ZXDH_DEV_CHANNEL_MAX)
		return 1;

	p_dtb_mgr = zxdh_np_dtb_mgr_get(dev_id);
	if (p_dtb_mgr == NULL) {
		zxdh_np_dtb_mgr_create(dev_id);

		p_dtb_mgr = zxdh_np_dtb_mgr_get(dev_id);
		if (p_dtb_mgr == NULL)
			return ZXDH_RC_DTB_MGR_NOT_EXIST;
	}

	return 0;
}

static uint32_t
zxdh_np_base_soft_init(uint32_t dev_id, ZXDH_SYS_INIT_CTRL_T *p_init_ctrl)
{
	uint32_t dev_id_array[ZXDH_DEV_CHANNEL_MAX] = {0};
	uint32_t rt;
	uint32_t access_type;
	uint32_t agent_flag;

	rt = zxdh_np_dev_init();
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rt, "zxdh_dev_init");

	if (p_init_ctrl->flags & ZXDH_INIT_FLAG_ACCESS_TYPE)
		access_type = ZXDH_DEV_ACCESS_TYPE_RISCV;
	else
		access_type = ZXDH_DEV_ACCESS_TYPE_PCIE;

	if (p_init_ctrl->flags & ZXDH_INIT_FLAG_AGENT_FLAG)
		agent_flag = ZXDH_DEV_AGENT_ENABLE;
	else
		agent_flag = ZXDH_DEV_AGENT_DISABLE;

	rt = zxdh_np_dev_add(dev_id,
					 p_init_ctrl->device_type,
					 access_type,
					 p_init_ctrl->pcie_vir_baddr,
					 p_init_ctrl->riscv_vir_baddr,
					 p_init_ctrl->dma_vir_baddr,
					 p_init_ctrl->dma_phy_baddr);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rt, "zxdh_dev_add");

	rt = zxdh_np_dev_agent_status_set(dev_id, agent_flag);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rt, "zxdh_dev_agent_status_set");

	dev_id_array[0] = dev_id;
	rt = zxdh_np_sdt_init(1, dev_id_array);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rt, "zxdh_sdt_init");

	zxdh_np_ppu_parse_cls_bitmap(dev_id, ZXDH_PPU_CLS_ALL_START);

	rt = zxdh_np_dtb_soft_init(dev_id);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rt, "zxdh_dtb_soft_init");

	return rt;
}

static void
zxdh_np_dev_vport_set(uint32_t dev_id, uint32_t vport)
{
	ZXDH_DEV_MGR_T *p_dev_mgr = NULL;
	ZXDH_DEV_CFG_T *p_dev_info = NULL;

	p_dev_mgr =  &g_dev_mgr;
	p_dev_info = p_dev_mgr->p_dev_array[ZXDH_DEV_SLOT_ID(dev_id)];
	p_dev_info->vport[ZXDH_DEV_PF_INDEX(dev_id)] = vport;
}

static void
zxdh_np_dev_agent_addr_set(uint32_t dev_id, uint64_t agent_addr)
{
	ZXDH_DEV_MGR_T *p_dev_mgr = NULL;
	ZXDH_DEV_CFG_T *p_dev_info = NULL;

	p_dev_mgr =  &g_dev_mgr;
	p_dev_info = p_dev_mgr->p_dev_array[ZXDH_DEV_SLOT_ID(dev_id)];
	p_dev_info->agent_addr[ZXDH_DEV_PF_INDEX(dev_id)] = agent_addr;
}

static uint64_t
zxdh_np_addr_calc(uint64_t pcie_vir_baddr, uint32_t bar_offset)
{
	uint64_t np_addr;

	np_addr = ((pcie_vir_baddr + bar_offset) > ZXDH_PCIE_NP_MEM_SIZE)
				? (pcie_vir_baddr + bar_offset - ZXDH_PCIE_NP_MEM_SIZE) : 0;
	return np_addr;
}

static uint64_t
zxdh_np_fw_compatible_addr_calc(uint64_t pcie_vir_baddr, uint64_t compatible_offset)
{
	return (pcie_vir_baddr + compatible_offset);
}

static void
zxdh_np_pf_fw_compatible_addr_set(uint32_t dev_id, uint64_t pcie_vir_baddr)
{
	uint64_t compatible_offset = ZXDH_DPU_NO_DEBUG_PF_COMPAT_REG_OFFSET;
	uint64_t compatible_addr = 0;

	compatible_addr = zxdh_np_fw_compatible_addr_calc(pcie_vir_baddr, compatible_offset);

	g_np_fw_compat_addr[ZXDH_DEV_SLOT_ID(dev_id)] = compatible_addr;
}

static void
zxdh_np_fw_compatible_addr_get(uint32_t dev_id, uint64_t *p_compatible_addr)
{
	*p_compatible_addr = g_np_fw_compat_addr[ZXDH_DEV_SLOT_ID(dev_id)];
}

static void
zxdh_np_fw_version_data_read(uint64_t compatible_base_addr,
			ZXDH_VERSION_COMPATIBLE_REG_T *p_fw_version_data, uint32_t module_id)
{
	void *fw_addr = NULL;
	uint64_t module_compatible_addr = 0;

	module_compatible_addr = compatible_base_addr +
		sizeof(ZXDH_VERSION_COMPATIBLE_REG_T) * (module_id - 1);

	fw_addr = (void *)module_compatible_addr;

	memcpy(p_fw_version_data, fw_addr, sizeof(ZXDH_VERSION_COMPATIBLE_REG_T));
}

static void
zxdh_np_fw_version_compatible_data_get(uint32_t dev_id,
			ZXDH_VERSION_COMPATIBLE_REG_T *p_version_compatible_value,
			uint32_t module_id)
{
	uint64_t compatible_addr = 0;

	zxdh_np_fw_compatible_addr_get(dev_id, &compatible_addr);

	zxdh_np_fw_version_data_read(compatible_addr, p_version_compatible_value, module_id);
}

static uint32_t
zxdh_np_np_sdk_version_compatible_check(uint32_t dev_id)
{
	ZXDH_VERSION_COMPATIBLE_REG_T fw_version = {0};

	zxdh_np_fw_version_compatible_data_get(dev_id, &fw_version, ZXDH_NPSDK_COMPAT_ITEM_ID);

	if (fw_version.version_compatible_item != ZXDH_NPSDK_COMPAT_ITEM_ID) {
		PMD_DRV_LOG(ERR, "version_compatible_item is not DH_NPSDK.");
		return ZXDH_ERR;
	}

	if (g_np_sdk_version.major != fw_version.major) {
		PMD_DRV_LOG(ERR, "dh_npsdk major:%hhu: is not match fw:%hhu!",
			g_np_sdk_version.major, fw_version.major);
		return ZXDH_ERR;
	}

	if (g_np_sdk_version.fw_minor > fw_version.fw_minor) {
		PMD_DRV_LOG(ERR, "dh_npsdk fw_minor:%hhu is higher than fw:%hhu!",
			g_np_sdk_version.fw_minor, fw_version.fw_minor);
		return ZXDH_ERR;
	}

	if (g_np_sdk_version.drv_minor < fw_version.drv_minor) {
		PMD_DRV_LOG(ERR, "dh_npsdk drv_minor:%hhu is lower than fw:%hhu!",
			g_np_sdk_version.drv_minor, fw_version.drv_minor);
		return ZXDH_ERR;
	}

	PMD_DRV_LOG(INFO, "dh_npsdk compatible check success!");

	return ZXDH_OK;
}

static uint32_t
zxdh_np_pcie_bar_msg_num_get(uint32_t dev_id, uint32_t *p_bar_msg_num)
{
	uint32_t rc = ZXDH_OK;
	ZXDH_SPINLOCK_T *p_dtb_spinlock = NULL;
	ZXDH_DEV_SPINLOCK_TYPE_E spinlock = ZXDH_DEV_SPINLOCK_T_DTB;

	rc = zxdh_np_dev_opr_spinlock_get(dev_id, (uint32_t)spinlock, &p_dtb_spinlock);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dev_opr_spinlock_get");

	rte_spinlock_lock(&p_dtb_spinlock->spinlock);
	rc = zxdh_np_agent_channel_pcie_bar_request(dev_id, p_bar_msg_num);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_agent_channel_pcie_bar_request");
	rte_spinlock_unlock(&p_dtb_spinlock->spinlock);

	return rc;
}

static uint32_t
zxdh_np_comm_read_bits(uint8_t *p_base, uint32_t base_size_bit,
		uint32_t *p_data, uint32_t start_bit, uint32_t end_bit)
{
	uint32_t start_byte_index;
	uint32_t end_byte_index;
	uint32_t byte_num;
	uint32_t buffer_size;
	uint32_t len;

	if (0 != (base_size_bit % 8))
		return 1;

	if (start_bit > end_bit)
		return 1;

	if (base_size_bit < end_bit)
		return 1;

	len = end_bit - start_bit + 1;
	buffer_size = base_size_bit / 8;
	while (0 != (buffer_size & (buffer_size - 1)))
		buffer_size += 1;

	*p_data = 0;
	end_byte_index     = (end_bit    >> 3);
	start_byte_index   = (start_bit  >> 3);

	if (start_byte_index == end_byte_index) {
		*p_data = (uint32_t)(((p_base[start_byte_index] >> (7U - (end_bit & 7)))
			& (0xff >> (8U - len))) & 0xff);
		return 0;
	}

	if (start_bit & 7) {
		*p_data = (p_base[start_byte_index] & (0xff >> (start_bit & 7))) & UINT8_MAX;
		start_byte_index++;
	}

	for (byte_num = start_byte_index; byte_num < end_byte_index; byte_num++) {
		*p_data <<= 8;
		*p_data  += p_base[byte_num];
	}

	*p_data <<= 1 + (end_bit & 7);
	*p_data  += ((p_base[byte_num & (buffer_size - 1)] & (0xff << (7 - (end_bit  & 7)))) >>
		(7 - (end_bit  & 7))) & 0xff;

	return 0;
}

static uint32_t
zxdh_np_comm_read_bits_ex(uint8_t *p_base, uint32_t base_size_bit,
		uint32_t *p_data, uint32_t msb_start_pos, uint32_t len)
{
	uint32_t rtn;

	rtn = zxdh_np_comm_read_bits(p_base,
				base_size_bit,
				p_data,
				(base_size_bit - 1 - msb_start_pos),
				(base_size_bit - 1 - msb_start_pos + len - 1));
	return rtn;
}

static uint32_t
zxdh_np_reg_read(uint32_t dev_id, uint32_t reg_no,
		uint32_t m_offset, uint32_t n_offset, void *p_data)
{
	uint32_t p_buff[ZXDH_REG_DATA_MAX] = {0};
	ZXDH_REG_T *p_reg_info = &g_dpp_reg_info[reg_no];
	const ZXDH_FIELD_T *p_field_info = p_reg_info->p_fields;
	uint32_t rc = 0;
	uint32_t i;
	uint32_t addr = 0;
	uint32_t reg_module = p_reg_info->module_no;
	uint32_t reg_width = p_reg_info->width;
	uint32_t reg_real_no = p_reg_info->reg_no;
	uint32_t reg_type = p_reg_info->flags;

	addr = zxdh_np_reg_get_reg_addr(reg_no, m_offset, n_offset);

	if (reg_module == DTB4K) {
		rc = p_reg_info->p_read_fun(dev_id, addr, p_buff);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "p_reg_info->p_read_fun");
	} else {
		rc = zxdh_np_agent_channel_reg_read(dev_id,
			reg_type, reg_real_no, reg_width, addr, p_buff);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_agent_channel_reg_read");
	}

	if (!zxdh_np_comm_is_big_endian()) {
		for (i = 0; i < p_reg_info->width / 4; i++) {
			PMD_DRV_LOG(DEBUG, "data = 0x%08x.", p_buff[i]);
			p_buff[i] = ZXDH_COMM_CONVERT32(p_buff[i]);
		}
	}

	for (i = 0; i < p_reg_info->field_num; i++) {
		rc = zxdh_np_comm_read_bits_ex((uint8_t *)p_buff,
								p_reg_info->width * 8,
								(uint32_t *)p_data + i,
								p_field_info[i].msb_pos,
								p_field_info[i].len);
		ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "zxdh_np_comm_read_bits_ex");
		PMD_DRV_LOG(DEBUG, "dev_id %u(%u)(%u)is ok!", dev_id, m_offset, n_offset);
	}

	return rc;
}

static uint32_t
zxdh_np_reg_read32(uint32_t dev_id, uint32_t reg_no,
	uint32_t m_offset, uint32_t n_offset, uint32_t *p_data)
{
	uint32_t rc = 0;
	uint32_t addr = 0;
	ZXDH_REG_T *p_reg_info = &g_dpp_reg_info[reg_no];
	uint32_t p_buff[ZXDH_REG_DATA_MAX] = {0};
	uint32_t reg_real_no = p_reg_info->reg_no;
	uint32_t reg_type = p_reg_info->flags;
	uint32_t reg_module = p_reg_info->module_no;

	addr = zxdh_np_reg_get_reg_addr(reg_no, m_offset, n_offset);

	if (reg_module == DTB4K) {
		rc = p_reg_info->p_read_fun(dev_id, addr, p_data);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "p_reg_info->p_read_fun");
	} else {
		rc = zxdh_np_agent_channel_reg_read(dev_id, reg_type, reg_real_no, 4, addr, p_buff);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_agent_channel_reg_read");
		*p_data = p_buff[0];
	}

	return rc;
}

static uint32_t
zxdh_np_dtb_queue_vm_info_get(uint32_t dev_id,
		uint32_t queue_id,
		ZXDH_DTB_QUEUE_VM_INFO_T *p_vm_info)
{
	uint32_t rc = 0;
	uint32_t dtb_epid_v_func_reg = ZXDH_SYS_DTB_BASE_ADDR +
		ZXDH_MODULE_DTB_ENQ_BASE_ADDR + 0x0010;
	uint32_t epid_v_func = 0;

	rc = zxdh_np_dev_read_channel(dev_id, dtb_epid_v_func_reg + queue_id * 32, 1, &epid_v_func);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dev_read_channel");

	p_vm_info->dbi_en = (epid_v_func >> 31 & 0x1);
	p_vm_info->queue_en = (epid_v_func >> 30 & 0x1);
	p_vm_info->epid = (epid_v_func >> 24 & 0xF);
	p_vm_info->vfunc_num = (epid_v_func >> 16 & 0xFF);
	p_vm_info->vector = (epid_v_func >> 8 & 0x7);
	p_vm_info->func_num = (epid_v_func >> 5 & 0x7);
	p_vm_info->vfunc_active = (epid_v_func & 0x1);

	return rc;
}

static uint32_t
zxdh_np_comm_write_bits(uint8_t *p_base, uint32_t base_size_bit,
			uint32_t data, uint32_t start_bit, uint32_t end_bit)
{
	uint32_t start_byte_index;
	uint32_t end_byte_index;
	uint8_t mask_value;
	uint32_t byte_num;
	uint32_t buffer_size;

	if (0 != (base_size_bit % 8))
		return 1;

	if (start_bit > end_bit)
		return 1;

	if (base_size_bit < end_bit)
		return 1;

	buffer_size = base_size_bit / 8;

	while (0 != (buffer_size & (buffer_size - 1)))
		buffer_size += 1;

	end_byte_index     = (end_bit    >> 3);
	start_byte_index   = (start_bit  >> 3);

	if (start_byte_index == end_byte_index) {
		mask_value  = ((0xFE << (7 - (start_bit & 7))) & 0xff);
		mask_value |= (((1 << (7 - (end_bit  & 7))) - 1) & 0xff);
		p_base[end_byte_index] &= mask_value;
		p_base[end_byte_index] |= (((data << (7 - (end_bit & 7)))) & 0xff);
		return 0;
	}

	if (7 != (end_bit & 7)) {
		mask_value = ((0x7f >> (end_bit  & 7)) & 0xff);
		p_base[end_byte_index] &= mask_value;
		p_base[end_byte_index] |= ((data << (7 - (end_bit & 7))) & 0xff);
		end_byte_index--;
		data >>= 1 + (end_bit  & 7);
	}

	for (byte_num = end_byte_index; byte_num > start_byte_index; byte_num--) {
		p_base[byte_num & (buffer_size - 1)] = data & 0xff;
		data >>= 8;
	}

	mask_value        = ((0xFE << (7 - (start_bit  & 7))) & 0xff);
	p_base[byte_num] &= mask_value;
	p_base[byte_num] |= data;

	return 0;
}

static uint32_t
zxdh_np_comm_write_bits_ex(uint8_t *p_base,
		uint32_t base_size_bit,
		uint32_t data,
		uint32_t msb_start_pos,
		uint32_t len)
{
	uint32_t rtn;

	rtn = zxdh_np_comm_write_bits(p_base,
				base_size_bit,
				data,
				(base_size_bit - 1 - msb_start_pos),
				(base_size_bit - 1 - msb_start_pos + len - 1));

	return rtn;
}

static uint32_t
zxdh_np_reg_write(uint32_t dev_id, uint32_t reg_no,
			uint32_t m_offset, uint32_t n_offset, void *p_data)
{
	uint32_t p_buff[ZXDH_REG_DATA_MAX] = {0};
	ZXDH_REG_T *p_reg_info = &g_dpp_reg_info[reg_no];
	const ZXDH_FIELD_T *p_field_info = p_reg_info->p_fields;
	uint32_t temp_data;
	uint32_t rc = ZXDH_OK;
	uint32_t i;
	uint32_t addr = 0;
	uint32_t reg_module = p_reg_info->module_no;
	uint32_t reg_width = p_reg_info->width;
	uint32_t reg_type = p_reg_info->flags;
	uint32_t reg_real_no = p_reg_info->reg_no;

	for (i = 0; i < p_reg_info->field_num; i++) {
		if (p_field_info[i].len <= 32) {
			temp_data = *((uint32_t *)p_data + i) &
				zxdh_np_comm_get_bit_mask(p_field_info[i].len);
			rc = zxdh_np_comm_write_bits_ex((uint8_t *)p_buff,
								p_reg_info->width * 8,
								temp_data,
								p_field_info[i].msb_pos,
								p_field_info[i].len);
			ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "zxdh_comm_write_bits_ex");
		}
	}

	PMD_DRV_LOG(DEBUG, "zxdh_np_comm_write_bits_ex data = 0x%08x.", p_buff[0]);

	if (!zxdh_np_comm_is_big_endian()) {
		for (i = 0; i < p_reg_info->width / 4; i++) {
			p_buff[i] = ZXDH_COMM_CONVERT32(p_buff[i]);

			PMD_DRV_LOG(DEBUG, "ZXDH_COMM_CONVERT32 data = 0x%08x.",
				p_buff[i]);
		}
	}

	addr = zxdh_np_reg_get_reg_addr(reg_no, m_offset, n_offset);

	PMD_DRV_LOG(DEBUG, "reg_no = %u. m_offset = %u n_offset = %u",
		reg_no, m_offset, n_offset);
	PMD_DRV_LOG(DEBUG, "baseaddr = 0x%08x.", addr);

	if (reg_module == DTB4K) {
		rc = p_reg_info->p_write_fun(dev_id, addr, p_buff);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "p_reg_info->p_write_fun");
	} else {
		rc = zxdh_np_agent_channel_reg_write(dev_id,
			reg_type, reg_real_no, reg_width, addr, p_buff);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_agent_channel_reg_write");
	}

	return rc;
}

static uint32_t
zxdh_np_dtb_queue_vm_info_set(uint32_t dev_id,
		uint32_t queue_id,
		ZXDH_DTB_QUEUE_VM_INFO_T *p_vm_info)
{
	uint32_t rc = 0;
	ZXDH_DTB4K_DTB_ENQ_CFG_EPID_V_FUNC_NUM_0_127_T vm_info = {0};

	vm_info.dbi_en = p_vm_info->dbi_en;
	vm_info.queue_en = p_vm_info->queue_en;
	vm_info.cfg_epid = p_vm_info->epid;
	vm_info.cfg_vector = p_vm_info->vector;
	vm_info.cfg_vfunc_num = p_vm_info->vfunc_num;
	vm_info.cfg_func_num = p_vm_info->func_num;
	vm_info.cfg_vfunc_active = p_vm_info->vfunc_active;

	rc = zxdh_np_reg_write(dev_id, ZXDH_DTB_CFG_EPID_V_FUNC_NUM,
						0, queue_id, &vm_info);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_reg_write");

	return rc;
}

static uint32_t
zxdh_np_dtb_queue_unused_item_num_get(uint32_t dev_id,
						uint32_t queue_id,
						uint32_t *p_item_num)
{
	uint32_t rc;

	rc = zxdh_np_reg_read(dev_id, ZXDH_DTB_INFO_QUEUE_BUF_SPACE,
		0, queue_id, p_item_num);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "dpp_reg_read");
	return rc;
}

static uint32_t
zxdh_np_dtb_queue_id_free(uint32_t dev_id,
					uint32_t queue_id)
{
	uint32_t item_num = 0;
	ZXDH_DTB_MGR_T *p_dtb_mgr = NULL;
	uint32_t rc;

	p_dtb_mgr = p_dpp_dtb_mgr[ZXDH_DEV_SLOT_ID(dev_id)];
	if (p_dtb_mgr == NULL)
		return 1;

	rc = zxdh_np_dtb_queue_unused_item_num_get(dev_id, queue_id, &item_num);

	if (item_num != ZXDH_DTB_QUEUE_ITEM_NUM_MAX)
		return ZXDH_RC_DTB_QUEUE_IS_WORKING;

	p_dtb_mgr->queue_info[queue_id].init_flag = 0;
	p_dtb_mgr->queue_info[queue_id].vport = 0;
	p_dtb_mgr->queue_info[queue_id].vector = 0;

	memset(&p_dtb_mgr->queue_info[queue_id].tab_up, 0, sizeof(ZXDH_DTB_TAB_UP_INFO_T));
	memset(&p_dtb_mgr->queue_info[queue_id].tab_down, 0, sizeof(ZXDH_DTB_TAB_DOWN_INFO_T));

	return rc;
}

static ZXDH_RB_CFG *
zxdh_np_dtb_dump_addr_rb_get(uint32_t dev_id, uint32_t queue_id)
{
	return g_dtb_dump_addr_rb[ZXDH_DEV_SLOT_ID(dev_id)][queue_id];
}

static uint32_t
zxdh_np_dtb_dump_addr_rb_set(uint32_t dev_id, uint32_t queue_id, ZXDH_RB_CFG *p_dump_addr_rb)
{
	g_dtb_dump_addr_rb[ZXDH_DEV_SLOT_ID(dev_id)][queue_id] = p_dump_addr_rb;
	return ZXDH_OK;
}

static uint32_t
zxdh_np_dtb_dump_sdt_addr_clear(uint32_t dev_id,
								uint32_t queue_id,
								uint32_t sdt_no)
{
	uint32_t rc = ZXDH_OK;

	ZXDH_DTB_ADDR_INFO_T dtb_dump_addr_info = {0};
	ZXDH_RB_CFG *p_dtb_dump_addr_rb = NULL;

	dtb_dump_addr_info.sdt_no = sdt_no;

	p_dtb_dump_addr_rb = zxdh_np_dtb_dump_addr_rb_get(dev_id, queue_id);
	rc = zxdh_np_se_apt_rb_delete(p_dtb_dump_addr_rb, &dtb_dump_addr_info,
		sizeof(ZXDH_DTB_ADDR_INFO_T));
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_se_apt_rb_delete");

	return rc;
}

static uint32_t
zxdh_np_dtb_dump_addr_rb_destroy(uint32_t dev_id, uint32_t queue_id)
{
	uint32_t rc = ZXDH_OK;
	ZXDH_D_NODE *p_node = NULL;
	ZXDH_RB_TN *p_rb_tn = NULL;
	ZXDH_DTB_ADDR_INFO_T *p_rbkey = NULL;
	ZXDH_D_HEAD *p_head_dtb_rb = NULL;
	ZXDH_RB_CFG *p_dtb_dump_addr_rb = NULL;
	uint32_t sdt_no = 0;

	p_dtb_dump_addr_rb = zxdh_np_dtb_dump_addr_rb_get(dev_id, queue_id);
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_dtb_dump_addr_rb);

	p_head_dtb_rb = &p_dtb_dump_addr_rb->tn_list;

	while (p_head_dtb_rb->used) {
		p_node = p_head_dtb_rb->p_next;
		p_rb_tn = (ZXDH_RB_TN *)p_node->data;
		p_rbkey = (ZXDH_DTB_ADDR_INFO_T *)p_rb_tn->p_key;

		sdt_no = p_rbkey->sdt_no;
		rc = zxdh_np_dtb_dump_sdt_addr_clear(dev_id, queue_id, sdt_no);

		if (rc == ZXDH_HASH_RC_DEL_SRHFAIL)
			PMD_DRV_LOG(ERR, "dtb dump delete key is not exist,"
				"std:%u", sdt_no);
		else
			ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_dump_sdt_addr_clear");
	}

	rc  =  zxdh_comm_rb_destroy(p_dtb_dump_addr_rb);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_comm_rb_init");

	return rc;
}

static uint32_t
zxdh_np_dtb_dump_addr_rb_init(uint32_t dev_id, uint32_t queue_id)
{
	uint32_t rc = ZXDH_OK;

	ZXDH_RB_CFG *p_dtb_dump_addr_rb = NULL;
	p_dtb_dump_addr_rb = zxdh_np_dtb_dump_addr_rb_get(dev_id, queue_id);

	if (p_dtb_dump_addr_rb == NULL) {
		p_dtb_dump_addr_rb = rte_zmalloc(NULL, sizeof(ZXDH_RB_CFG), 0);
		if (p_dtb_dump_addr_rb == NULL) {
			PMD_DRV_LOG(ERR, "malloc memory failed");
			return ZXDH_PAR_CHK_POINT_NULL;
		}

		rc = zxdh_np_dtb_dump_addr_rb_set(dev_id, queue_id, p_dtb_dump_addr_rb);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_dump_addr_rb_set");
	}

	rc = zxdh_comm_rb_init(p_dtb_dump_addr_rb, 0,
		sizeof(ZXDH_DTB_ADDR_INFO_T), zxdh_np_se_apt_key_default_cmp);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_comm_rb_init");

	return rc;
}

static uint32_t
zxdh_np_dtb_queue_request(uint32_t dev_id, char p_name[32],
					uint16_t vport, uint32_t *p_queue_id)
{
	uint32_t rc = ZXDH_OK;
	uint32_t queue_id = 0xFF;
	ZXDH_SPINLOCK_T *p_dtb_spinlock = NULL;
	ZXDH_DEV_SPINLOCK_TYPE_E spinlock = ZXDH_DEV_SPINLOCK_T_DTB;
	uint32_t vport_info = (uint32_t)vport;

	rc = zxdh_np_dev_opr_spinlock_get(dev_id, (uint32_t)spinlock, &p_dtb_spinlock);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dev_opr_spinlock_get");

	rte_spinlock_lock(&p_dtb_spinlock->spinlock);

	rc = zxdh_np_agent_channel_dtb_queue_request(dev_id, p_name, vport_info, &queue_id);
	if (rc == ZXDH_RC_DTB_QUEUE_RES_EMPTY) {
		PMD_DRV_LOG(ERR, "dtb queue is locked full.");
		rte_spinlock_unlock(&p_dtb_spinlock->spinlock);
		return ZXDH_RC_DTB_QUEUE_RES_EMPTY;
	}

	rte_spinlock_unlock(&p_dtb_spinlock->spinlock);

	PMD_DRV_LOG(DEBUG, "dtb request queue is %u.", queue_id);

	rc = zxdh_np_dtb_dump_addr_rb_init(dev_id, queue_id);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_dump_addr_rb_init");

	*p_queue_id = queue_id;

	PMD_DRV_LOG(INFO, "dev_id %u vport 0x%x name %s queue_id %u done.",
		dev_id, vport_info, p_name, queue_id);

	return rc;
}

static uint32_t
zxdh_np_dtb_queue_release(uint32_t devid,
		char pname[32],
		uint32_t queueid)
{
	uint32_t rc = ZXDH_OK;
	ZXDH_SPINLOCK_T *p_dtb_spinlock = NULL;
	ZXDH_DEV_SPINLOCK_TYPE_E spinlock = ZXDH_DEV_SPINLOCK_T_DTB;

	rc = zxdh_np_dev_opr_spinlock_get(devid, (uint32_t)spinlock, &p_dtb_spinlock);
	ZXDH_COMM_CHECK_DEV_RC(devid, rc, "zxdh_np_dev_opr_spinlock_get");

	rte_spinlock_lock(&p_dtb_spinlock->spinlock);

	rc = zxdh_np_agent_channel_dtb_queue_release(devid, pname, queueid);

	if (rc == ZXDH_RC_DTB_QUEUE_NOT_ALLOC) {
		PMD_DRV_LOG(ERR, "dtb queue id %u not request.", queueid);
		rte_spinlock_unlock(&p_dtb_spinlock->spinlock);
		return ZXDH_RC_DTB_QUEUE_NOT_ALLOC;
	}

	if (rc == ZXDH_RC_DTB_QUEUE_NAME_ERROR) {
		PMD_DRV_LOG(ERR, "dtb queue %u name error.", queueid);
		rte_spinlock_unlock(&p_dtb_spinlock->spinlock);
		return ZXDH_RC_DTB_QUEUE_NAME_ERROR;
	}

	rte_spinlock_unlock(&p_dtb_spinlock->spinlock);

	rc = zxdh_np_dtb_dump_addr_rb_destroy(devid, queueid);
	ZXDH_COMM_CHECK_DEV_RC(devid, rc, "zxdh_np_dtb_dump_addr_rb_destroy");

	rc = zxdh_np_dtb_queue_id_free(devid, queueid);
	ZXDH_COMM_CHECK_DEV_RC(devid, rc, "zxdh_np_dtb_queue_id_free");

	PMD_DRV_LOG(INFO, "release queueid %u", queueid);

	return rc;
}

static void
zxdh_np_dtb_mgr_destroy(uint32_t dev_id)
{
	if (p_dpp_dtb_mgr[ZXDH_DEV_SLOT_ID(dev_id)] != NULL) {
		rte_free(p_dpp_dtb_mgr[ZXDH_DEV_SLOT_ID(dev_id)]);
		p_dpp_dtb_mgr[ZXDH_DEV_SLOT_ID(dev_id)] = NULL;
	}
}

static void
zxdh_np_sdt_mgr_destroy(uint32_t dev_id)
{
	ZXDH_SDT_SOFT_TABLE_T *p_sdt_tbl_temp = NULL;
	ZXDH_SDT_MGR_T *p_sdt_mgr = NULL;

	p_sdt_tbl_temp = ZXDH_SDT_SOFT_TBL_GET(dev_id);
	p_sdt_mgr = ZXDH_SDT_MGR_PTR_GET();

	rte_free(p_sdt_tbl_temp);

	ZXDH_SDT_SOFT_TBL_GET(dev_id) = NULL;

	p_sdt_mgr->channel_num--;
}

static void
zxdh_np_dev_del(uint32_t dev_id)
{
	ZXDH_DEV_CFG_T *p_dev_info = NULL;
	ZXDH_DEV_MGR_T *p_dev_mgr  = NULL;

	p_dev_mgr = &g_dev_mgr;
	p_dev_info = p_dev_mgr->p_dev_array[ZXDH_DEV_SLOT_ID(dev_id)];

	if (p_dev_info != NULL) {
		rte_free(p_dev_info);
		p_dev_mgr->p_dev_array[ZXDH_DEV_SLOT_ID(dev_id)] = NULL;
		p_dev_mgr->device_num--;
	}
}

static uint32_t
zxdh_np_hash_soft_all_entry_delete(ZXDH_SE_CFG *p_se_cfg, uint32_t hash_id)
{
	uint32_t rc = 0;
	uint32_t dev_id = p_se_cfg->dev_id;
	uint8_t table_id = 0;
	uint32_t bulk_id = 0;

	ZXDH_D_NODE *p_node = NULL;
	ZXDH_RB_TN *p_rb_tn = NULL;
	ZXDH_RB_TN *p_rb_tn_rtn = NULL;
	ZXDH_HASH_RBKEY_INFO *p_rbkey = NULL;
	ZXDH_HASH_RBKEY_INFO *p_rbkey_rtn = NULL;
	ZXDH_SE_ITEM_CFG *p_item = NULL;
	ZXDH_FUNC_ID_INFO *p_func_info = ZXDH_GET_FUN_INFO(p_se_cfg, hash_id);
	ZXDH_HASH_CFG *p_hash_cfg = (ZXDH_HASH_CFG *)p_func_info->fun_ptr;
	ZXDH_D_HEAD *p_head_hash_rb = &p_hash_cfg->hash_rb.tn_list;

	while (p_head_hash_rb->used) {
		p_node = p_head_hash_rb->p_next;
		p_rb_tn = (ZXDH_RB_TN *)p_node->data;
		p_rbkey = (ZXDH_HASH_RBKEY_INFO *)p_rb_tn->p_key;
		table_id = ZXDH_GET_HASH_TBL_ID(p_rbkey->key);
		bulk_id = ((table_id >> 2) & 0x7);

		rc = zxdh_comm_rb_delete(&p_hash_cfg->hash_rb, p_rbkey, &p_rb_tn_rtn);
		if (rc == ZXDH_RBT_RC_SRHFAIL) {
			p_hash_cfg->hash_stat.delete_fail++;
			PMD_DRV_LOG(DEBUG, "Error!there is not item in hash!");
			return ZXDH_HASH_RC_DEL_SRHFAIL;
		}

		ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_rb_tn_rtn);
		p_rbkey_rtn = (ZXDH_HASH_RBKEY_INFO *)(p_rb_tn_rtn->p_key);
		p_item = p_rbkey_rtn->p_item_info;

		rc = zxdh_comm_double_link_del(&p_rbkey_rtn->entry_dn, &p_item->item_list);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_comm_double_link_del");
		p_item->wrt_mask &= ~(ZXDH_GET_HASH_ENTRY_MASK(p_rbkey_rtn->entry_size,
			p_rbkey_rtn->entry_pos)) & 0xF;

		if (p_item->item_list.used == 0) {
			if (p_item->item_type == ZXDH_ITEM_DDR_256 ||
				p_item->item_type == ZXDH_ITEM_DDR_512) {
				p_hash_cfg->p_bulk_ddr_info[bulk_id]->p_item_array
					[p_item->item_index] = NULL;
				rte_free(p_item);
			} else {
				p_item->valid = 0;
			}
		}

		rte_free(p_rbkey_rtn);
		rte_free(p_rb_tn_rtn);
		p_hash_cfg->hash_stat.delete_ok++;
	}

	return rc;
}

static uint32_t
zxdh_np_hash_zcam_resource_deinit(ZXDH_HASH_CFG *p_hash_cfg)
{
	uint32_t rc = 0;
	uint32_t dev_id = p_hash_cfg->p_se_info->dev_id;
	uint32_t i = 0;

	ZXDH_D_NODE *p_node = NULL;
	ZXDH_D_HEAD *p_head = &p_hash_cfg->hash_shareram.zcell_free_list;
	ZXDH_SE_ZBLK_CFG *p_zblk_cfg = NULL;
	ZXDH_SE_ZCELL_CFG *p_zcell_cfg = NULL;

	while (p_head->used) {
		p_node = p_head->p_next;

		rc = zxdh_comm_double_link_del(p_node, p_head);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_comm_double_link_del");
		p_zcell_cfg = (ZXDH_SE_ZCELL_CFG *)p_node->data;
		p_zcell_cfg->is_used = 0;
		p_zcell_cfg->flag = 0;
	}

	p_head = &p_hash_cfg->hash_shareram.zblk_list;

	while (p_head->used) {
		p_node = p_head->p_next;

		rc = zxdh_comm_double_link_del(p_node, p_head);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_comm_double_link_del");

		p_zblk_cfg = ZXDH_SE_GET_ZBLK_CFG(p_hash_cfg->p_se_info,
			((ZXDH_SE_ZBLK_CFG *)p_node->data)->zblk_idx);
		p_zblk_cfg->is_used = 0;
		for (i = 0; i < ZXDH_SE_ZREG_NUM; i++)
			p_zblk_cfg->zreg_info[i].flag = 0;
	}

	return rc;
}

static uint32_t
zxdh_np_se_fun_deinit(ZXDH_SE_CFG *p_se_cfg,
							 uint8_t	   id,
							 uint32_t	 fun_type)
{
	ZXDH_FUNC_ID_INFO  *p_fun_info = ZXDH_GET_FUN_INFO(p_se_cfg, id);

	if (p_fun_info->is_used == 0) {
		PMD_DRV_LOG(ERR, " Error[0x%x], fun_id [%u] is already deinit!",
			ZXDH_SE_RC_FUN_INVALID, id);
		return ZXDH_OK;
	}

	switch (fun_type) {
	case (ZXDH_FUN_HASH):
		if (p_fun_info->fun_ptr) {
			rte_free(p_fun_info->fun_ptr);
			p_fun_info->fun_ptr = NULL;
		}
		break;
	default:
		PMD_DRV_LOG(ERR, " Error, unrecgnized fun_type[ %u] ", fun_type);
		RTE_ASSERT(0);
		return ZXDH_SE_RC_BASE;
	}

	p_fun_info->fun_id = id;
	p_fun_info->is_used = 0;

	return ZXDH_OK;
}

static uint32_t
zxdh_np_one_hash_soft_uninstall(uint32_t dev_id, uint32_t hash_id)
{
	uint32_t rc = 0;
	uint32_t i = 0;

	ZXDH_D_NODE *p_node = NULL;
	ZXDH_SE_CFG *p_se_cfg = dpp_se_cfg[ZXDH_DEV_SLOT_ID(dev_id)];
	ZXDH_RB_TN *p_rb_tn = NULL;
	ZXDH_RB_TN *p_rb_tn_rtn = NULL;
	HASH_DDR_CFG *p_rbkey = NULL;
	ZXDH_HASH_CFG *p_hash_cfg = NULL;
	ZXDH_FUNC_ID_INFO *p_func_info = ZXDH_GET_FUN_INFO(p_se_cfg, hash_id);
	ZXDH_D_HEAD *p_head_ddr_cfg_rb = NULL;
	HASH_DDR_CFG *p_temp_rbkey = NULL;

	if (p_func_info->is_used == 0) {
		PMD_DRV_LOG(ERR, "Error[0x%x], fun_id [%u] is not init!",
			ZXDH_SE_RC_FUN_INVALID, hash_id);
		return ZXDH_OK;
	}

	rc = zxdh_np_hash_soft_all_entry_delete(p_se_cfg, hash_id);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_hash_soft_all_entry_delete");

	p_hash_cfg = (ZXDH_HASH_CFG *)p_func_info->fun_ptr;
	for (i = 0; i < ZXDH_HASH_BULK_NUM; i++) {
		if (p_hash_cfg->hash_stat.p_bulk_zcam_mono[i] != NULL) {
			rte_free((&p_hash_cfg->hash_stat)->p_bulk_zcam_mono[i]);
			(&p_hash_cfg->hash_stat)->p_bulk_zcam_mono[i] = NULL;
		}
	}

	p_head_ddr_cfg_rb = &p_hash_cfg->ddr_cfg_rb.tn_list;
	while (p_head_ddr_cfg_rb->used) {
		p_node = p_head_ddr_cfg_rb->p_next;

		p_rb_tn = (ZXDH_RB_TN *)p_node->data;
		p_rbkey = p_rb_tn->p_key;

		rc = zxdh_comm_rb_delete(&p_hash_cfg->ddr_cfg_rb, p_rbkey, &p_rb_tn_rtn);

		if (rc == ZXDH_RBT_RC_SRHFAIL)
			PMD_DRV_LOG(ERR, "ddr_cfg_rb delete key is not exist, key: 0x");
		else
			ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_comm_rb_delete");

		ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_rb_tn_rtn);
		p_temp_rbkey = (HASH_DDR_CFG *)(p_rb_tn_rtn->p_key);
		rte_free(p_temp_rbkey->p_item_array);
		p_temp_rbkey->p_item_array = NULL;
		rte_free(p_temp_rbkey);
		rte_free(p_rb_tn_rtn);
	}

	rc = zxdh_np_hash_zcam_resource_deinit(p_hash_cfg);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_hash_zcam_resource_deinit");

	rc = zxdh_np_se_fun_deinit(p_se_cfg, (hash_id & 0xff), ZXDH_FUN_HASH);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_se_fun_deinit");

	memset(g_tbl_id_info[ZXDH_DEV_SLOT_ID(dev_id)][hash_id], 0,
		ZXDH_HASH_TBL_ID_NUM * sizeof(ZXDH_HASH_TBL_ID_INFO));

	return rc;
}

static uint32_t
zxdh_np_hash_soft_uninstall(uint32_t dev_id)
{
	uint32_t  rc	   = ZXDH_OK;
	uint32_t hash_id  = 0;

	for (hash_id = 0; hash_id < ZXDH_HASH_FUNC_ID_NUM; hash_id++) {
		rc = zxdh_np_one_hash_soft_uninstall(dev_id, hash_id);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_one_hash_soft_uninstall");
	}

	return rc;
}

static uint32_t
zxdh_np_acl_cfg_get(uint32_t dev_id, ZXDH_ACL_CFG_EX_T **p_acl_cfg)
{
	if (g_p_acl_ex_cfg[ZXDH_DEV_SLOT_ID(dev_id)] == NULL) {
		PMD_DRV_LOG(ERR, "etcam_is not init!");
		RTE_ASSERT(0);
		return ZXDH_ACL_RC_ETCAMID_NOT_INIT;
	}

	*p_acl_cfg = g_p_acl_ex_cfg[ZXDH_DEV_SLOT_ID(dev_id)];

	return ZXDH_OK;
}

static uint32_t
zxdh_np_acl_res_destroy(uint32_t dev_id)
{
	uint32_t table_id = 0;
	uint32_t as_enable = 0;
	ZXDH_ACL_CFG_EX_T *p_acl_cfg = NULL;
	ZXDH_ACL_TBL_CFG_T *p_tbl_cfg = NULL;

	zxdh_np_acl_cfg_get(dev_id, &p_acl_cfg);

	if (!p_acl_cfg->acl_etcamids.is_valid) {
		PMD_DRV_LOG(DEBUG, "etcam is not init!");
		return ZXDH_OK;
	}

	for (table_id = ZXDH_ACL_TBL_ID_MIN; table_id <= ZXDH_ACL_TBL_ID_MAX; table_id++) {
		p_tbl_cfg = p_acl_cfg->acl_tbls + table_id;
		if (!p_tbl_cfg->is_used) {
			PMD_DRV_LOG(DEBUG, "table_id[ %u ] is not used!", table_id);
			continue;
		}

		zxdh_comm_rb_destroy(&p_tbl_cfg->acl_rb);

		as_enable = p_tbl_cfg->as_enable;
		if (as_enable) {
			if (p_tbl_cfg->as_rslt_buff) {
				rte_free(p_tbl_cfg->as_rslt_buff);
				p_tbl_cfg->as_rslt_buff = NULL;
			}
		}

		if (p_tbl_cfg->block_array) {
			rte_free(p_tbl_cfg->block_array);
			p_tbl_cfg->block_array = NULL;
		}

		p_tbl_cfg->is_used = 0;
	}

	p_acl_cfg->acl_etcamids.is_valid = 0;

	return ZXDH_OK;
}

static void
zxdh_np_apt_hash_global_res_uninit(uint32_t dev_id)
{
	if (g_apt_se_cfg[ZXDH_DEV_SLOT_ID(dev_id)] != NULL) {
		rte_free(g_apt_se_cfg[ZXDH_DEV_SLOT_ID(dev_id)]);
		g_apt_se_cfg[ZXDH_DEV_SLOT_ID(dev_id)] = NULL;
		dpp_se_cfg[ZXDH_DEV_SLOT_ID(dev_id)] = NULL;
	}
}

int
zxdh_np_online_uninit(uint32_t dev_id,
			char *port_name,
			uint32_t queue_id)
{
	uint32_t rc;

	rc = zxdh_np_dtb_queue_release(dev_id, port_name, queue_id);
	if (rc != 0)
		PMD_DRV_LOG(ERR, "dtb release port name %s queue id %u", port_name, queue_id);

	rc = zxdh_np_soft_res_uninstall(dev_id);
	if (rc != 0)
		PMD_DRV_LOG(ERR, "zxdh_np_soft_res_uninstall failed");

	return 0;
}

uint32_t
zxdh_np_soft_res_uninstall(uint32_t dev_id)
{
	uint32_t rc;

	rc = zxdh_np_hash_soft_uninstall(dev_id);
	if (rc != ZXDH_OK)
		PMD_DRV_LOG(ERR, "zxdh_np_hash_soft_uninstall error! ");

	zxdh_np_apt_hash_global_res_uninit(dev_id);
	zxdh_np_acl_res_destroy(dev_id);
	zxdh_np_dtb_mgr_destroy(dev_id);
	zxdh_np_sdt_mgr_destroy(dev_id);
	zxdh_np_dev_del(dev_id);

	return rc;
}

static uint32_t
zxdh_np_sdt_tbl_type_get(uint32_t dev_id, uint32_t sdt_no)
{
	return g_table_type[ZXDH_DEV_SLOT_ID(dev_id)][sdt_no];
}


static const ZXDH_DTB_TABLE_T *
zxdh_np_dtb_table_info_get(uint32_t table_type)
{
	return &g_dpp_dtb_table_info[table_type];
}

static const ZXDH_DTB_TABLE_T *
zxdh_np_dtb_dump_info_get(uint32_t up_type)
{
	return &g_dpp_dtb_dump_info[up_type];
}

static uint32_t
zxdh_np_dtb_write_table_cmd(uint32_t dev_id,
			ZXDH_DTB_TABLE_INFO_E table_type,
			void *p_cmd_data,
			void *p_cmd_buff)
{
	uint32_t         field_cnt;
	const ZXDH_DTB_TABLE_T     *p_table_info = NULL;
	const ZXDH_DTB_FIELD_T     *p_field_info = NULL;
	uint32_t         temp_data;
	uint32_t         rc = 0;

	ZXDH_COMM_CHECK_POINT(p_cmd_data);
	ZXDH_COMM_CHECK_POINT(p_cmd_buff);
	p_table_info = zxdh_np_dtb_table_info_get(table_type);
	p_field_info = p_table_info->p_fields;
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_table_info);

	for (field_cnt = 0; field_cnt < p_table_info->field_num; field_cnt++) {
		temp_data = *((uint32_t *)p_cmd_data + field_cnt) &
					zxdh_np_comm_get_bit_mask(p_field_info[field_cnt].len);

		rc = zxdh_np_comm_write_bits_ex((uint8_t *)p_cmd_buff,
					ZXDH_DTB_TABLE_CMD_SIZE_BIT,
					temp_data,
					p_field_info[field_cnt].lsb_pos,
					p_field_info[field_cnt].len);

		ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "zxdh_np_comm_write_bits_ex");
	}

	return rc;
}

static uint32_t
zxdh_np_dtb_write_dump_cmd(uint32_t dev_id,
							ZXDH_DTB_DUMP_INFO_E dump_type,
							void *p_cmd_data,
							void *p_cmd_buff)
{
	uint32_t rc = ZXDH_OK;
	uint32_t field_cnt = 0;
	const ZXDH_DTB_TABLE_T *p_table_info = NULL;
	const ZXDH_DTB_FIELD_T *p_field_info = NULL;
	uint32_t temp_data = 0;

	p_table_info = zxdh_np_dtb_dump_info_get(dump_type);
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_table_info);
	p_field_info = p_table_info->p_fields;
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_field_info);

	for (field_cnt = 0; field_cnt < p_table_info->field_num; field_cnt++) {
		temp_data = *((uint32_t *)p_cmd_data + field_cnt) &
			zxdh_np_comm_get_bit_mask(p_field_info[field_cnt].len);

		rc = zxdh_np_comm_write_bits_ex((uint8_t *)p_cmd_buff,
						ZXDH_DTB_TABLE_CMD_SIZE_BIT,
						temp_data,
						p_field_info[field_cnt].lsb_pos,
						p_field_info[field_cnt].len);

		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_comm_write_bits");
	}

	return rc;
}

static uint32_t
zxdh_np_dtb_smmu0_write_entry_data(uint32_t dev_id,
		uint32_t mode,
		uint32_t addr,
		uint32_t *p_data,
		ZXDH_DTB_ENTRY_T *p_entry)
{
	ZXDH_DTB_ERAM_TABLE_FORM_T dtb_eram_form_info = {0};
	uint32_t  rc = 0;

	dtb_eram_form_info.valid = ZXDH_DTB_TABLE_VALID;
	dtb_eram_form_info.type_mode = ZXDH_DTB_TABLE_MODE_ERAM;
	dtb_eram_form_info.data_mode = mode;
	dtb_eram_form_info.cpu_wr = 1;
	dtb_eram_form_info.addr = addr;
	dtb_eram_form_info.cpu_rd = 0;
	dtb_eram_form_info.cpu_rd_mode = 0;

	if (ZXDH_ERAM128_OPR_128b == mode) {
		p_entry->data_in_cmd_flag = 0;
		p_entry->data_size = 128 / 8;

		rc = zxdh_np_dtb_write_table_cmd(dev_id, ZXDH_DTB_TABLE_ERAM_128,
			&dtb_eram_form_info, p_entry->cmd);
		ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "dpp_dtb_write_table_cmd");

		memcpy(p_entry->data, p_data, 128 / 8);
	} else if (ZXDH_ERAM128_OPR_64b == mode) {
		p_entry->data_in_cmd_flag = 1;
		p_entry->data_size  = 64 / 8;
		dtb_eram_form_info.data_l = *(p_data + 1);
		dtb_eram_form_info.data_h = *(p_data);

		rc = zxdh_np_dtb_write_table_cmd(dev_id, ZXDH_DTB_TABLE_ERAM_64,
			&dtb_eram_form_info, p_entry->cmd);
		ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "dpp_dtb_write_table_cmd");

	} else if (ZXDH_ERAM128_OPR_1b == mode) {
		p_entry->data_in_cmd_flag = 1;
		p_entry->data_size  = 1;
		dtb_eram_form_info.data_h = *(p_data);

		rc = zxdh_np_dtb_write_table_cmd(dev_id, ZXDH_DTB_TABLE_ERAM_1,
			&dtb_eram_form_info, p_entry->cmd);
		ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "dpp_dtb_write_table_cmd");
	}

	return rc;
}

static uint32_t
zxdh_np_dtb_zcam_write_entry_data(uint32_t dev_id,
								uint32_t reg_sram_flag,
								uint32_t zgroup_id,
								uint32_t zblock_id,
								uint32_t zcell_id,
								uint32_t sram_addr,
								uint32_t mask,
								uint8_t *p_data,
								ZXDH_DTB_ENTRY_T *p_entry)
{
	uint32_t rc = ZXDH_OK;

	ZXDH_DTB_ZCAM_TABLE_FORM_T dtb_zcam_form_info = {
		.valid = 1,
		.type_mode = ZXDH_DTB_TABLE_MODE_ZCAM,
		.ram_reg_flag = reg_sram_flag,
		.zgroup_id = zgroup_id,
		.zblock_id = zblock_id,
		.zcell_id = zcell_id,
		.mask = mask,
		.sram_addr = sram_addr & 0x1FF,
	};

	p_entry->data_in_cmd_flag = 0;
	p_entry->data_size = ZXDH_DTB_LEN_POS_SETP * (ZXDH_DTB_ZCAM_LEN_SIZE - 1);

	rc = zxdh_np_dtb_write_table_cmd(dev_id, ZXDH_DTB_TABLE_ZCAM,
		&dtb_zcam_form_info, p_entry->cmd);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_write_table_cmd");

	memcpy(p_entry->data, p_data,
		ZXDH_DTB_LEN_POS_SETP * (ZXDH_DTB_ZCAM_LEN_SIZE - 1));

	return rc;
}

static uint32_t
zxdh_np_dtb_se_alg_zcam_data_write(uint32_t dev_id,
									uint32_t addr,
									uint8_t *p_data,
									ZXDH_DTB_ENTRY_T *p_entry)
{
	uint32_t rc = ZXDH_OK;
	uint32_t reg_sram_flag = (addr >> 16) & 0x1;
	uint32_t zgroup_id = (addr >> 14) & 0x3;
	uint32_t zblock_id = (addr >> 11) & 0x7;
	uint32_t zcell_id = (addr >> 9) & 0x3;
	uint32_t mask = (addr >> 17) & 0xF;
	uint32_t sram_addr = addr & 0x1FF;

	rc = zxdh_np_dtb_zcam_write_entry_data(dev_id, reg_sram_flag, zgroup_id, zblock_id,
			zcell_id, sram_addr, mask, p_data, p_entry);

	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_zcam_write_entry_data");

	return rc;
}

static uint32_t
zxdh_np_dtb_etcam_write_entry_data(uint32_t dev_id,
									uint32_t block_idx,
									uint32_t row_or_col_msk,
									uint32_t vben,
									uint32_t reg_tcam_flag,
									uint32_t flush,
									uint32_t rd_wr,
									uint32_t wr_mode,
									uint32_t data_or_mask,
									uint32_t ram_addr,
									uint32_t vbit,
									uint8_t *p_data,
									ZXDH_DTB_ENTRY_T *p_entry)
{
	uint32_t rc = ZXDH_OK;
	uint32_t i = 0;
	uint32_t offset = 0;
	uint8_t *p_temp = NULL;

	uint8_t  buff[ZXDH_ETCAM_WIDTH_MAX / 8] = {0};

	ZXDH_DTB_ETCAM_TABLE_FORM_T dtb_etcam_form_info = {
		.valid = 1,
		.type_mode = ZXDH_DTB_TABLE_MODE_ETCAM,
		.block_sel = block_idx,
		.init_en  = 0,
		.row_or_col_msk = row_or_col_msk,
		.vben = vben,
		.reg_tcam_flag = reg_tcam_flag,
		.uload = flush,
		.rd_wr = rd_wr,
		.wr_mode = wr_mode,
		.data_or_mask = data_or_mask,
		.addr = ram_addr,
		.vbit = vbit,
	};

	p_entry->data_in_cmd_flag = 0;
	p_entry->data_size = ZXDH_DTB_LEN_POS_SETP * (ZXDH_DTB_ETCAM_LEN_SIZE - 1);

	rc = zxdh_np_dtb_write_table_cmd(dev_id, ZXDH_DTB_TABLE_ETCAM,
		&dtb_etcam_form_info, p_entry->cmd);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_write_table_cmd");

	p_temp = p_data;

	for (i = 0; i < ZXDH_ETCAM_RAM_NUM; i++) {
		offset = i * ((uint32_t)ZXDH_ETCAM_WIDTH_MIN / 8);

		if ((wr_mode >> (ZXDH_ETCAM_RAM_NUM - 1 - i)) & 0x1) {
			memcpy(buff + offset, p_temp, ZXDH_ETCAM_WIDTH_MIN / 8);
			p_temp += ZXDH_ETCAM_WIDTH_MIN / 8;
		}
	}

	zxdh_np_comm_swap((uint8_t *)buff, ZXDH_DTB_LEN_POS_SETP * (ZXDH_DTB_ETCAM_LEN_SIZE - 1));

	memcpy(p_entry->data, buff,
		ZXDH_DTB_LEN_POS_SETP * (ZXDH_DTB_ETCAM_LEN_SIZE - 1));

	return rc;
}

static uint32_t
zxdh_np_dtb_smmu0_dump_info_write(uint32_t dev_id,
							uint32_t base_addr,
							uint32_t depth,
							uint32_t addr_high32,
							uint32_t addr_low32,
							uint32_t *p_dump_info)
{
	uint32_t rc = ZXDH_OK;

	ZXDH_DTB_ERAM_DUMP_FORM_T dtb_eram_dump_form_info = {
		.valid = 1,
		.up_type = ZXDH_DTB_DUMP_MODE_ERAM,
		.base_addr = base_addr,
		.tb_depth = depth,
		.tb_dst_addr_h = addr_high32,
		.tb_dst_addr_l = addr_low32,
	};

	PMD_DRV_LOG(DEBUG, "valid: %u", dtb_eram_dump_form_info.valid);
	PMD_DRV_LOG(DEBUG, "up_type: %u", dtb_eram_dump_form_info.up_type);
	PMD_DRV_LOG(DEBUG, "base_addr: 0x%x", dtb_eram_dump_form_info.base_addr);
	PMD_DRV_LOG(DEBUG, "tb_depth: %u", dtb_eram_dump_form_info.tb_depth);
	PMD_DRV_LOG(DEBUG, "tb_dst_addr_h: 0x%x", dtb_eram_dump_form_info.tb_dst_addr_h);
	PMD_DRV_LOG(DEBUG, "tb_dst_addr_l: 0x%x", dtb_eram_dump_form_info.tb_dst_addr_l);

	rc = zxdh_np_dtb_write_dump_cmd(dev_id, ZXDH_DTB_DUMP_ERAM,
		&dtb_eram_dump_form_info, p_dump_info);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_write_dump_cmd");

	return rc;
}

static uint32_t
zxdh_np_dtb_zcam_dump_info_write(uint32_t dev_id,
							uint32_t addr,
							uint32_t tb_width,
							uint32_t depth,
							uint32_t addr_high32,
							uint32_t addr_low32,
							uint32_t *p_dump_info)
{
	uint32_t rc = ZXDH_OK;

	ZXDH_DTB_ZCAM_DUMP_FORM_T dtb_zcam_dump_form_info = {
		.valid = 1,
		.up_type = ZXDH_DTB_DUMP_MODE_ZCAM,
		.tb_width = tb_width,
		.sram_addr = addr & 0x1FF,
		.ram_reg_flag = (addr >> 16) & 0x1,
		.z_reg_cell_id = (addr >> 9) & 0x3,
		.zblock_id = (addr >> 11) & 0x7,
		.zgroup_id = (addr >> 14) & 0x3,
		.tb_depth = depth,
		.tb_dst_addr_h = addr_high32,
		.tb_dst_addr_l = addr_low32,
	};

	rc = zxdh_np_dtb_write_dump_cmd(dev_id, ZXDH_DTB_DUMP_ZCAM,
		&dtb_zcam_dump_form_info, p_dump_info);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_write_dump_cmd");

	return rc;
}

static uint32_t
zxdh_np_dtb_etcam_dump_info_write(uint32_t dev_id,
						ZXDH_ETCAM_DUMP_INFO_T *p_etcam_dump_info,
						uint32_t addr_high32,
						uint32_t addr_low32,
						uint32_t *p_dump_info)
{
	uint32_t rc = ZXDH_OK;

	ZXDH_DTB_ETCAM_DUMP_FORM_T dtb_etcam_dump_form_info = {
		.valid = 1,
		.up_type = ZXDH_DTB_DUMP_MODE_ETCAM,
		.block_sel = p_etcam_dump_info->block_sel,
		.addr = p_etcam_dump_info->addr,
		.rd_mode = p_etcam_dump_info->rd_mode,
		.data_or_mask = p_etcam_dump_info->data_or_mask,
		.tb_depth = p_etcam_dump_info->tb_depth,
		.tb_width = p_etcam_dump_info->tb_width,
		.tb_dst_addr_h = addr_high32,
		.tb_dst_addr_l = addr_low32,
	};

	rc = zxdh_np_dtb_write_dump_cmd(dev_id, ZXDH_DTB_DUMP_ETCAM,
		&dtb_etcam_dump_form_info, p_dump_info);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_write_dump_cmd");

	return rc;
}

static void
zxdh_np_dtb_zcam_dump_entry(uint32_t dev_id,
					uint32_t addr,
					uint32_t tb_width,
					uint32_t depth,
					uint32_t addr_high32,
					uint32_t addr_low32,
					ZXDH_DTB_ENTRY_T *p_entry)
{
	zxdh_np_dtb_zcam_dump_info_write(dev_id,
						addr,
						tb_width,
						depth,
						addr_high32,
						addr_low32,
						(uint32_t *)p_entry->cmd);
	p_entry->data_in_cmd_flag = 1;
}

static void
zxdh_np_dtb_smmu0_dump_entry(uint32_t dev_id,
								uint32_t base_addr,
								uint32_t depth,
								uint32_t addr_high32,
								uint32_t addr_low32,
								ZXDH_DTB_ENTRY_T *p_entry)
{
	zxdh_np_dtb_smmu0_dump_info_write(dev_id,
						base_addr,
						depth,
						addr_high32,
						addr_low32,
						(uint32_t *)p_entry->cmd);
	p_entry->data_in_cmd_flag = 1;
}

static void
zxdh_np_dtb_etcam_dump_entry(uint32_t dev_id,
						ZXDH_ETCAM_DUMP_INFO_T *p_etcam_dump_info,
						uint32_t addr_high32,
						uint32_t addr_low32,
						ZXDH_DTB_ENTRY_T *p_entry)
{
	zxdh_np_dtb_etcam_dump_info_write(dev_id,
					p_etcam_dump_info,
					addr_high32,
					addr_low32,
					(uint32_t *)p_entry->cmd);
	p_entry->data_in_cmd_flag = 1;
}

static uint32_t
zxdh_np_dtb_se_smmu0_ind_write(uint32_t dev_id,
		uint32_t base_addr,
		uint32_t index,
		uint32_t wrt_mode,
		uint32_t *p_data,
		ZXDH_DTB_ENTRY_T *p_entry)
{
	uint32_t temp_idx = 0;
	uint32_t dtb_ind_addr;
	uint32_t rc = 0;

	switch (wrt_mode) {
	case ZXDH_ERAM128_OPR_128b:
		if ((0xFFFFFFFF - (base_addr)) < (index)) {
			PMD_DRV_LOG(ERR, "base addr:0x%x, index:0x%x invalid", base_addr, index);
			return ZXDH_PAR_CHK_INVALID_INDEX;
		}
		if (base_addr + index > ZXDH_SE_SMMU0_ERAM_ADDR_NUM_TOTAL - 1) {
			PMD_DRV_LOG(ERR, "dpp_se_smmu0_ind_write : index out of range");
			return 1;
		}
		temp_idx = index << 7;
		break;
	case ZXDH_ERAM128_OPR_64b:
		if ((base_addr + (index >> 1)) > ZXDH_SE_SMMU0_ERAM_ADDR_NUM_TOTAL - 1) {
			PMD_DRV_LOG(ERR, "dpp_se_smmu0_ind_write : index out of range");
			return 1;
		}
		temp_idx = index << 6;
		break;
	case ZXDH_ERAM128_OPR_1b:
		if ((base_addr + (index >> 7)) > ZXDH_SE_SMMU0_ERAM_ADDR_NUM_TOTAL - 1) {
			PMD_DRV_LOG(ERR, "dpp_se_smmu0_ind_write : index out of range");
			return 1;
		}

		temp_idx = index;
		break;
	default:
		break;
	}

	dtb_ind_addr = ((base_addr << 7) & ZXDH_ERAM128_BADDR_MASK) + temp_idx;

	PMD_DRV_LOG(DEBUG, "dtb eram item 1bit addr: 0x%x", dtb_ind_addr);

	rc = zxdh_np_dtb_smmu0_write_entry_data(dev_id,
						  wrt_mode,
						  dtb_ind_addr,
						  p_data,
						  p_entry);
	ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "dpp_dtb_smmu0_write_entry_data");

	return rc;
}

static uint32_t
zxdh_np_eram_dtb_len_get(uint32_t mode)
{
	uint32_t dtb_len = 0;

	switch (mode) {
	case ZXDH_ERAM128_OPR_128b:
		dtb_len += 2;
		break;
	case ZXDH_ERAM128_OPR_64b:
	case ZXDH_ERAM128_OPR_1b:
		dtb_len += 1;
		break;
	default:
		break;
	}

	return dtb_len;
}

static void
zxdh_np_sdt_tbl_data_get(uint32_t dev_id, uint32_t sdt_no, ZXDH_SDT_TBL_DATA_T *p_sdt_data)
{
	p_sdt_data->data_high32 = g_sdt_info[ZXDH_DEV_SLOT_ID(dev_id)][sdt_no].data_high32;
	p_sdt_data->data_low32  = g_sdt_info[ZXDH_DEV_SLOT_ID(dev_id)][sdt_no].data_low32;
}

static uint32_t
zxdh_np_sdt_tbl_data_parser(uint32_t sdt_hig32, uint32_t sdt_low32, void *p_sdt_info)
{
	uint32_t tbl_type = 0;
	uint32_t clutch_en = 0;
	uint32_t tmp = 0;

	ZXDH_SDT_TBL_ERAM_T *p_sdt_eram = NULL;
	ZXDH_SDT_TBL_HASH_T *p_sdt_hash = NULL;
	ZXDH_SDT_TBL_ETCAM_T *p_sdt_etcam = NULL;
	ZXDH_SDT_TBL_PORTTBL_T *p_sdt_porttbl = NULL;

	ZXDH_COMM_UINT32_GET_BITS(tbl_type, sdt_hig32,
		ZXDH_SDT_H_TBL_TYPE_BT_POS, ZXDH_SDT_H_TBL_TYPE_BT_LEN);
	ZXDH_COMM_UINT32_GET_BITS(clutch_en, sdt_low32, 0, 1);

	switch (tbl_type) {
	case ZXDH_SDT_TBLT_ERAM:
		p_sdt_eram = (ZXDH_SDT_TBL_ERAM_T *)p_sdt_info;
		p_sdt_eram->table_type = tbl_type;
		ZXDH_COMM_UINT32_GET_BITS(p_sdt_eram->eram_mode, sdt_hig32,
			ZXDH_SDT_H_ERAM_MODE_BT_POS, ZXDH_SDT_H_ERAM_MODE_BT_LEN);
		ZXDH_COMM_UINT32_GET_BITS(p_sdt_eram->eram_base_addr, sdt_hig32,
			ZXDH_SDT_H_ERAM_BASE_ADDR_BT_POS, ZXDH_SDT_H_ERAM_BASE_ADDR_BT_LEN);
		ZXDH_COMM_UINT32_GET_BITS(p_sdt_eram->eram_table_depth, sdt_low32,
			ZXDH_SDT_L_ERAM_TABLE_DEPTH_BT_POS, ZXDH_SDT_L_ERAM_TABLE_DEPTH_BT_LEN);
		p_sdt_eram->eram_clutch_en = clutch_en;
		break;
	case ZXDH_SDT_TBLT_HASH:
		p_sdt_hash = (ZXDH_SDT_TBL_HASH_T *)p_sdt_info;
		p_sdt_hash->table_type = tbl_type;
		ZXDH_COMM_UINT32_GET_BITS(p_sdt_hash->hash_id, sdt_hig32,
			ZXDH_SDT_H_HASH_ID_BT_POS, ZXDH_SDT_H_HASH_ID_BT_LEN);
		ZXDH_COMM_UINT32_GET_BITS(p_sdt_hash->hash_table_width, sdt_hig32,
			ZXDH_SDT_H_HASH_TABLE_WIDTH_BT_POS, ZXDH_SDT_H_HASH_TABLE_WIDTH_BT_LEN);
		ZXDH_COMM_UINT32_GET_BITS(p_sdt_hash->key_size, sdt_hig32,
			ZXDH_SDT_H_HASH_KEY_SIZE_BT_POS, ZXDH_SDT_H_HASH_KEY_SIZE_BT_LEN);
		ZXDH_COMM_UINT32_GET_BITS(p_sdt_hash->hash_table_id, sdt_hig32,
			ZXDH_SDT_H_HASH_TABLE_ID_BT_POS, ZXDH_SDT_H_HASH_TABLE_ID_BT_LEN);
		ZXDH_COMM_UINT32_GET_BITS(p_sdt_hash->learn_en, sdt_hig32,
			ZXDH_SDT_H_LEARN_EN_BT_POS, ZXDH_SDT_H_LEARN_EN_BT_LEN);
		ZXDH_COMM_UINT32_GET_BITS(p_sdt_hash->keep_alive, sdt_hig32,
			ZXDH_SDT_H_KEEP_ALIVE_BT_POS, ZXDH_SDT_H_KEEP_ALIVE_BT_LEN);
		ZXDH_COMM_UINT32_GET_BITS(tmp, sdt_hig32,
			ZXDH_SDT_H_KEEP_ALIVE_BADDR_BT_POS, ZXDH_SDT_H_KEEP_ALIVE_BADDR_BT_LEN);
		ZXDH_COMM_UINT32_GET_BITS(p_sdt_hash->keep_alive_baddr, sdt_low32,
			ZXDH_SDT_L_KEEP_ALIVE_BADDR_BT_POS, ZXDH_SDT_L_KEEP_ALIVE_BADDR_BT_LEN);
		p_sdt_hash->keep_alive_baddr += (tmp << ZXDH_SDT_L_KEEP_ALIVE_BADDR_BT_LEN);
		ZXDH_COMM_UINT32_GET_BITS(p_sdt_hash->rsp_mode, sdt_low32,
			ZXDH_SDT_L_RSP_MODE_BT_POS, ZXDH_SDT_L_RSP_MODE_BT_LEN);
		p_sdt_hash->hash_clutch_en = clutch_en;
		break;

	case ZXDH_SDT_TBLT_ETCAM:
		p_sdt_etcam = (ZXDH_SDT_TBL_ETCAM_T *)p_sdt_info;
		p_sdt_etcam->table_type = tbl_type;
		ZXDH_COMM_UINT32_GET_BITS(p_sdt_etcam->etcam_id, sdt_hig32,
			ZXDH_SDT_H_ETCAM_ID_BT_POS, ZXDH_SDT_H_ETCAM_ID_BT_LEN);
		ZXDH_COMM_UINT32_GET_BITS(p_sdt_etcam->etcam_key_mode, sdt_hig32,
			ZXDH_SDT_H_ETCAM_KEY_MODE_BT_POS, ZXDH_SDT_H_ETCAM_KEY_MODE_BT_LEN);
		ZXDH_COMM_UINT32_GET_BITS(p_sdt_etcam->etcam_table_id, sdt_hig32,
			ZXDH_SDT_H_ETCAM_TABLE_ID_BT_POS, ZXDH_SDT_H_ETCAM_TABLE_ID_BT_LEN);
		ZXDH_COMM_UINT32_GET_BITS(p_sdt_etcam->no_as_rsp_mode, sdt_hig32,
			ZXDH_SDT_H_ETCAM_NOAS_RSP_MODE_BT_POS,
			ZXDH_SDT_H_ETCAM_NOAS_RSP_MODE_BT_LEN);
		ZXDH_COMM_UINT32_GET_BITS(p_sdt_etcam->as_en, sdt_hig32,
			ZXDH_SDT_H_ETCAM_AS_EN_BT_POS, ZXDH_SDT_H_ETCAM_AS_EN_BT_LEN);
		ZXDH_COMM_UINT32_GET_BITS(tmp, sdt_hig32, ZXDH_SDT_H_ETCAM_AS_ERAM_BADDR_BT_POS,
			ZXDH_SDT_H_ETCAM_AS_ERAM_BADDR_BT_LEN);
		ZXDH_COMM_UINT32_GET_BITS(p_sdt_etcam->as_eram_baddr, sdt_low32,
			ZXDH_SDT_L_ETCAM_AS_ERAM_BADDR_BT_POS,
			ZXDH_SDT_L_ETCAM_AS_ERAM_BADDR_BT_LEN);
		p_sdt_etcam->as_eram_baddr += (tmp << ZXDH_SDT_L_ETCAM_AS_ERAM_BADDR_BT_LEN);
		ZXDH_COMM_UINT32_GET_BITS(p_sdt_etcam->as_rsp_mode, sdt_low32,
			ZXDH_SDT_L_ETCAM_AS_RSP_MODE_BT_POS, ZXDH_SDT_L_ETCAM_AS_RSP_MODE_BT_LEN);
		ZXDH_COMM_UINT32_GET_BITS(p_sdt_etcam->etcam_table_depth, sdt_low32,
			ZXDH_SDT_L_ETCAM_TABLE_DEPTH_BT_POS, ZXDH_SDT_L_ETCAM_TABLE_DEPTH_BT_LEN);
		p_sdt_etcam->etcam_clutch_en = clutch_en;
		break;

	case ZXDH_SDT_TBLT_PORTTBL:
		p_sdt_porttbl = (ZXDH_SDT_TBL_PORTTBL_T *)p_sdt_info;
		p_sdt_porttbl->table_type = tbl_type;
		p_sdt_porttbl->porttbl_clutch_en = clutch_en;
		break;
	default:
		PMD_DRV_LOG(ERR, "SDT table_type[ %u ] is invalid!", tbl_type);
		return 1;
	}

	return 0;
}

static uint32_t
zxdh_np_soft_sdt_tbl_get(uint32_t dev_id, uint32_t sdt_no, void *p_sdt_info)
{
	ZXDH_SDT_TBL_DATA_T sdt_tbl = {0};
	uint32_t rc;

	if (sdt_no > ZXDH_DEV_SDT_ID_MAX - 1) {
		PMD_DRV_LOG(ERR, "SDT NO [ %u ] is invalid!", sdt_no);
		return ZXDH_PAR_CHK_INVALID_PARA;
	}

	zxdh_np_sdt_tbl_data_get(dev_id, sdt_no, &sdt_tbl);

	rc = zxdh_np_sdt_tbl_data_parser(sdt_tbl.data_high32, sdt_tbl.data_low32, p_sdt_info);
	if (rc != 0)
		PMD_DRV_LOG(ERR, "dpp sdt [%u] tbl_data_parser error.", sdt_no);

	return rc;
}

static uint32_t
zxdh_np_dtb_eram_one_entry(uint32_t dev_id,
		uint32_t sdt_no,
		uint32_t del_en,
		void *pdata,
		uint32_t *p_dtb_len,
		ZXDH_DTB_ENTRY_T *p_dtb_one_entry)
{
	uint32_t buff[ZXDH_SMMU0_READ_REG_MAX_NUM]      = {0};
	ZXDH_SDT_TBL_ERAM_T sdt_eram           = {0};
	ZXDH_DTB_ERAM_ENTRY_INFO_T *peramdata = NULL;
	uint32_t base_addr;
	uint32_t index;
	uint32_t opr_mode;
	uint32_t rc = ZXDH_OK;

	ZXDH_COMM_CHECK_POINT(pdata);
	ZXDH_COMM_CHECK_POINT(p_dtb_one_entry);
	ZXDH_COMM_CHECK_POINT(p_dtb_len);

	PMD_DRV_LOG(DEBUG, "sdt_no:%u", sdt_no);
	rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_eram);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "dpp_soft_sdt_tbl_get");

	peramdata = (ZXDH_DTB_ERAM_ENTRY_INFO_T *)pdata;
	index = peramdata->index;
	base_addr = sdt_eram.eram_base_addr;
	opr_mode = sdt_eram.eram_mode;

	switch (opr_mode) {
	case ZXDH_ERAM128_TBL_128b:
		opr_mode = ZXDH_ERAM128_OPR_128b;
		break;
	case ZXDH_ERAM128_TBL_64b:
		opr_mode = ZXDH_ERAM128_OPR_64b;
		break;
	case ZXDH_ERAM128_TBL_1b:
		opr_mode = ZXDH_ERAM128_OPR_1b;
		break;
	default:
		break;
	}

	PMD_DRV_LOG(DEBUG, "std no:0x%x, index:0x%x, base addr:0x%x", sdt_no, index, base_addr);
	if (opr_mode == ZXDH_ERAM128_OPR_128b)
		PMD_DRV_LOG(DEBUG, "value[0x%08x 0x%08x 0x%08x 0x%08x]",
		peramdata->p_data[0], peramdata->p_data[1],
		peramdata->p_data[2], peramdata->p_data[3]);
	else if (opr_mode == ZXDH_ERAM128_OPR_64b)
		PMD_DRV_LOG(DEBUG, "value[0x%08x 0x%08x]",
		peramdata->p_data[0], peramdata->p_data[1]);

	if (del_en) {
		memset((uint8_t *)buff, 0, sizeof(buff));
		rc = zxdh_np_dtb_se_smmu0_ind_write(dev_id,
						base_addr,
						index,
						opr_mode,
						buff,
						p_dtb_one_entry);
		ZXDH_COMM_CHECK_DEV_RC(sdt_no, rc, "zxdh_dtb_se_smmu0_ind_write");
	} else {
		rc = zxdh_np_dtb_se_smmu0_ind_write(dev_id,
								   base_addr,
								   index,
								   opr_mode,
								   peramdata->p_data,
								   p_dtb_one_entry);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_dtb_se_smmu0_ind_write");
	}
	*p_dtb_len = zxdh_np_eram_dtb_len_get(opr_mode);

	return rc;
}

static uint32_t
zxdh_np_dtb_data_write(uint8_t *p_data_buff,
			uint32_t addr_offset,
			ZXDH_DTB_ENTRY_T *entry)
{
	ZXDH_COMM_CHECK_POINT(p_data_buff);
	ZXDH_COMM_CHECK_POINT(entry);

	uint8_t *p_cmd = p_data_buff + addr_offset;
	uint32_t cmd_size = ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8;

	uint8_t *p_data = p_cmd + cmd_size;
	uint32_t data_size = entry->data_size;

	uint8_t *cmd = (uint8_t *)entry->cmd;
	uint8_t *data = (uint8_t *)entry->data;

	memcpy(p_cmd, cmd, cmd_size);

	if (!entry->data_in_cmd_flag) {
		zxdh_np_comm_swap(data, data_size);
		memcpy(p_data, data, data_size);
	}

	return 0;
}

static uint32_t
zxdh_np_dtb_queue_enable_get(uint32_t dev_id,
		uint32_t queue_id,
		uint32_t *enable)
{
	uint32_t rc = 0;
	ZXDH_DTB_QUEUE_VM_INFO_T vm_info = {0};

	rc = zxdh_np_dtb_queue_vm_info_get(dev_id, queue_id, &vm_info);
	ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "zxdh_dtb_queue_vm_info_get");

	*enable = vm_info.queue_en;
	return rc;
}

static uint32_t
zxdh_np_dtb_item_buff_wr(uint32_t dev_id,
		uint32_t queue_id,
		uint32_t dir_flag,
		uint32_t index,
		uint32_t pos,
		uint32_t len,
		uint32_t *p_data)
{
	uint64_t addr;

	if (dir_flag == 1)
		addr = ZXDH_DTB_TAB_UP_VIR_ADDR_GET(dev_id, queue_id, index) +
			ZXDH_DTB_ITEM_ACK_SIZE + pos * 4;
	else
		addr = ZXDH_DTB_TAB_DOWN_VIR_ADDR_GET(dev_id, queue_id, index) +
			ZXDH_DTB_ITEM_ACK_SIZE + pos * 4;

	memcpy((uint8_t *)(addr), p_data, len * 4);

	return 0;
}

static uint32_t
zxdh_np_dtb_item_ack_rd(uint32_t dev_id,
		uint32_t queue_id,
		uint32_t dir_flag,
		uint32_t index,
		uint32_t pos,
		uint32_t *p_data)
{
	uint64_t addr;
	uint32_t val;

	if (dir_flag == 1)
		addr = ZXDH_DTB_TAB_UP_VIR_ADDR_GET(dev_id, queue_id, index) + pos * 4;
	else
		addr = ZXDH_DTB_TAB_DOWN_VIR_ADDR_GET(dev_id, queue_id, index) + pos * 4;

	val = *((volatile uint32_t *)(addr));

	if (!zxdh_np_comm_is_big_endian())
		val = ZXDH_COMM_CONVERT32(val);

	*p_data = val;

	return 0;
}

static uint32_t
zxdh_np_dtb_item_ack_wr(uint32_t dev_id,
		uint32_t queue_id,
		uint32_t dir_flag,
		uint32_t index,
		uint32_t pos,
		uint32_t data)
{
	uint64_t addr;

	if (dir_flag == 1)
		addr = ZXDH_DTB_TAB_UP_VIR_ADDR_GET(dev_id, queue_id, index) + pos * 4;
	else
		addr = ZXDH_DTB_TAB_DOWN_VIR_ADDR_GET(dev_id, queue_id, index) + pos * 4;

	if (!zxdh_np_comm_is_big_endian())
		data = ZXDH_COMM_CONVERT32(data);

	*((volatile uint32_t *)addr) = data;

	return 0;
}

static uint32_t
zxdh_np_dtb_item_ack_prt(uint32_t dev_id,
					uint32_t queue_id,
					uint32_t dir_flag,
					uint32_t index)
{
	uint32_t rc = 0;
	uint32_t i = 0;
	uint32_t ack_data[4] = {0};

	for (i = 0; i < ZXDH_DTB_ITEM_ACK_SIZE / 4; i++) {
		rc = zxdh_np_dtb_item_ack_rd(dev_id, queue_id, dir_flag, index, i, ack_data + i);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_item_ack_rd");
	}

	PMD_DRV_LOG(DEBUG, "[%s] BD INFO:", g_dpp_dtb_name[dir_flag]);
	PMD_DRV_LOG(DEBUG, "[index : %u] : 0x%08x 0x%08x 0x%08x 0x%08x", index,
				ack_data[0], ack_data[1], ack_data[2], ack_data[3]);

	return rc;
}

static uint32_t
zxdh_np_dtb_item_buff_rd(uint32_t dev_id,
					uint32_t queue_id,
					uint32_t dir_flag,
					uint32_t index,
					uint32_t pos,
					uint32_t len,
					uint32_t *p_data)
{
	uint64_t addr = 0;

	if (ZXDH_DTB_QUEUE_INIT_FLAG_GET(dev_id, queue_id) == 0) {
		PMD_DRV_LOG(ERR, "dtb queue %u is not init.", queue_id);
		return ZXDH_RC_DTB_QUEUE_IS_NOT_INIT;
	}

	if (dir_flag == ZXDH_DTB_DIR_UP_TYPE) {
		if (ZXDH_DTB_TAB_UP_USER_PHY_ADDR_FLAG_GET(dev_id, queue_id, index) ==
		ZXDH_DTB_TAB_UP_USER_ADDR_TYPE) {
			addr = ZXDH_DTB_TAB_UP_USER_VIR_ADDR_GET(dev_id, queue_id, index) + pos * 4;
			ZXDH_DTB_TAB_UP_USER_ADDR_FLAG_SET(dev_id, queue_id, index, 0);
		} else {
			addr = ZXDH_DTB_TAB_UP_VIR_ADDR_GET(dev_id, queue_id, index) +
				ZXDH_DTB_ITEM_ACK_SIZE + pos * 4;
		}
	} else {
		addr = ZXDH_DTB_TAB_DOWN_VIR_ADDR_GET(dev_id, queue_id, index) +
			ZXDH_DTB_ITEM_ACK_SIZE + pos * 4;
	}

	memcpy(p_data, (uint8_t *)(addr), len * 4);

	zxdh_np_comm_swap((uint8_t *)p_data, len * 4);

	return ZXDH_OK;
}

static uint32_t
zxdh_np_dtb_item_buff_prt(uint32_t dev_id,
					uint32_t queue_id,
					uint32_t dir_flag,
					uint32_t index,
					uint32_t len)
{
	uint32_t rc = 0;
	uint32_t i = 0;
	uint32_t j = 0;
	uint32_t *p_item_buff = NULL;

	p_item_buff = rte_zmalloc(NULL, len * sizeof(uint32_t), 0);
	if (p_item_buff == NULL) {
		PMD_DRV_LOG(ERR, "Alloc dtb item buffer failed!!!");
		return ZXDH_RC_DTB_MEMORY_ALLOC_ERR;
	}

	zxdh_np_dtb_item_buff_rd(dev_id, queue_id, dir_flag, index, 0, len, p_item_buff);

	PMD_DRV_LOG(DEBUG, "[%s] BUFF INFO:", g_dpp_dtb_name[dir_flag]);
	for (i = 0, j = 0; i < len; i++, j++) {
		if (j % 4 == 0)
			PMD_DRV_LOG(DEBUG, "0x%08x ", (*(p_item_buff + i)));
		else
			PMD_DRV_LOG(DEBUG, "0x%08x ", (*(p_item_buff + i)));
	}

	rte_free(p_item_buff);

	return rc;
}

static uint32_t
zxdh_np_dtb_queue_item_info_set(uint32_t dev_id,
		uint32_t queue_id,
		ZXDH_DTB_QUEUE_ITEM_INFO_T *p_item_info)
{
	uint32_t rc;

	uint32_t dtb_addr_h_reg = ZXDH_SYS_DTB_BASE_ADDR +
		ZXDH_MODULE_DTB_ENQ_BASE_ADDR + 0x0000;
	uint32_t dtb_addr_l_reg = ZXDH_SYS_DTB_BASE_ADDR +
		ZXDH_MODULE_DTB_ENQ_BASE_ADDR + 0x0004;
	uint32_t dtb_len_reg = ZXDH_SYS_DTB_BASE_ADDR +
		ZXDH_MODULE_DTB_ENQ_BASE_ADDR + 0x0008;
	uint32_t dtb_len = 0;

	rc = zxdh_np_dev_write_channel(dev_id, dtb_addr_h_reg + queue_id * 32,
		1, &p_item_info->data_hddr);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dev_write_channel Fail");
	rc = zxdh_np_dev_write_channel(dev_id, dtb_addr_l_reg + queue_id * 32,
		1, &p_item_info->data_laddr);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dev_write_channel Fail");
	dtb_len = ZXDH_DTB_LEN(p_item_info->cmd_type, p_item_info->int_en, p_item_info->data_len);
	rc = zxdh_np_dev_write_channel(dev_id, dtb_len_reg + queue_id * 32, 1, &dtb_len);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dev_write_channel Fail");

	return rc;
}

static uint32_t
zxdh_np_dtb_tab_down_info_set(uint32_t dev_id,
		uint32_t queue_id,
		uint32_t int_flag,
		uint32_t data_len,
		uint32_t *p_data,
		uint32_t *p_item_index)
{
	ZXDH_DTB_QUEUE_ITEM_INFO_T item_info = {0};
	uint32_t unused_item_num = 0;
	uint32_t queue_en = 0;
	uint32_t ack_vale = 0;
	uint64_t phy_addr;
	uint32_t item_index;
	uint32_t i;
	uint32_t rc;
	ZXDH_SPINLOCK_T *p_spinlock = NULL;

	zxdh_np_dev_dtb_opr_spinlock_get(dev_id, ZXDH_DEV_SPINLOCK_T_DTB, queue_id, &p_spinlock);
	rte_spinlock_lock(&p_spinlock->spinlock);

	if (ZXDH_DTB_QUEUE_INIT_FLAG_GET(dev_id, queue_id) == 0) {
		PMD_DRV_LOG(ERR, "dtb queue %u is not init.", queue_id);
		rte_spinlock_unlock(&p_spinlock->spinlock);
		return ZXDH_RC_DTB_QUEUE_IS_NOT_INIT;
	}

	if (data_len % 4 != 0) {
		rte_spinlock_unlock(&p_spinlock->spinlock);
		return ZXDH_RC_DTB_PARA_INVALID;
	}

	rc = zxdh_np_dtb_queue_enable_get(dev_id, queue_id, &queue_en);
	if (!queue_en) {
		PMD_DRV_LOG(ERR, "the queue %u is not enable!,rc=%u", queue_id, rc);
		rte_spinlock_unlock(&p_spinlock->spinlock);
		return ZXDH_RC_DTB_QUEUE_NOT_ENABLE;
	}

	rc = zxdh_np_dtb_queue_unused_item_num_get(dev_id, queue_id, &unused_item_num);
	if (unused_item_num == 0) {
		rte_spinlock_unlock(&p_spinlock->spinlock);
		return ZXDH_RC_DTB_QUEUE_ITEM_HW_EMPTY;
	}

	for (i = 0; i < ZXDH_DTB_QUEUE_ITEM_NUM_MAX; i++) {
		item_index = ZXDH_DTB_TAB_DOWN_WR_INDEX_GET(dev_id, queue_id) %
			ZXDH_DTB_QUEUE_ITEM_NUM_MAX;

		rc = zxdh_np_dtb_item_ack_rd(dev_id, queue_id, 0,
			item_index, 0, &ack_vale);

		ZXDH_DTB_TAB_DOWN_WR_INDEX_GET(dev_id, queue_id)++;

		if ((ack_vale >> 8) == ZXDH_DTB_TAB_ACK_UNUSED_MASK)
			break;
	}

	if (i == ZXDH_DTB_QUEUE_ITEM_NUM_MAX) {
		rte_spinlock_unlock(&p_spinlock->spinlock);
		return ZXDH_RC_DTB_QUEUE_ITEM_SW_EMPTY;
	}

	rc = zxdh_np_dtb_item_buff_wr(dev_id, queue_id, 0,
		item_index, 0, data_len, p_data);

	rc = zxdh_np_dtb_item_ack_wr(dev_id, queue_id, 0,
		item_index, 0, ZXDH_DTB_TAB_ACK_IS_USING_MASK);

	item_info.cmd_vld = 1;
	item_info.cmd_type = 0;
	item_info.int_en = int_flag;
	item_info.data_len = data_len / 4;
	phy_addr = p_dpp_dtb_mgr[ZXDH_DEV_SLOT_ID(dev_id)]->queue_info[queue_id].
		tab_down.start_phy_addr +
		item_index * p_dpp_dtb_mgr[ZXDH_DEV_SLOT_ID(dev_id)]->queue_info[queue_id].
		tab_down.item_size;
	item_info.data_hddr = ((phy_addr >> 4) >> 32) & 0xffffffff;
	item_info.data_laddr = (phy_addr >> 4) & 0xffffffff;

	zxdh_dtb_info_print(dev_id, queue_id, item_index, &item_info);

	rc = zxdh_np_dtb_queue_item_info_set(dev_id, queue_id, &item_info);
	*p_item_index = item_index;

	rte_spinlock_unlock(&p_spinlock->spinlock);

	return rc;
}

static uint32_t
zxdh_np_dtb_write_down_table_data(uint32_t dev_id,
		uint32_t queue_id,
		uint32_t down_table_len,
		uint8_t *p_down_table_buff,
		uint32_t *p_element_id)
{
	uint32_t  rc = 0;
	uint32_t dtb_interrupt_status = 0;

	rc = zxdh_np_dtb_tab_down_info_set(dev_id,
					queue_id,
					dtb_interrupt_status,
					down_table_len / 4,
					(uint32_t *)p_down_table_buff,
					p_element_id);
	return rc;
}

static void
zxdh_np_dtb_down_table_element_addr_get(uint32_t dev_id,
						uint32_t queue_id,
						uint32_t element_id,
						uint32_t *p_element_start_addr_h,
						uint32_t *p_element_start_addr_l,
						uint32_t *p_element_table_addr_h,
						uint32_t *p_element_table_addr_l)
{
	uint32_t addr_h = 0;
	uint32_t addr_l = 0;

	addr_h = (ZXDH_DTB_TAB_DOWN_PHY_ADDR_GET(dev_id, queue_id, element_id) >> 32) & 0xffffffff;
	addr_l = ZXDH_DTB_TAB_DOWN_PHY_ADDR_GET(dev_id, queue_id, element_id) & 0xffffffff;

	*p_element_start_addr_h = addr_h;
	*p_element_start_addr_l = addr_l;

	addr_h = ((ZXDH_DTB_TAB_DOWN_PHY_ADDR_GET(dev_id, queue_id, element_id) +
		ZXDH_DTB_ITEM_ACK_SIZE) >> 32) & 0xffffffff;
	addr_l = (ZXDH_DTB_TAB_DOWN_PHY_ADDR_GET(dev_id, queue_id, element_id) +
		ZXDH_DTB_ITEM_ACK_SIZE) & 0xffffffff;

	*p_element_table_addr_h = addr_h;
	*p_element_table_addr_l = addr_l;
}

static uint32_t
zxdh_np_dtb_down_table_element_info_prt(uint32_t dev_id,
					uint32_t queue_id,
					uint32_t element_id)
{
	uint32_t rc = 0;
	uint32_t element_start_addr_h = 0;
	uint32_t element_start_addr_l = 0;
	uint32_t element_table_addr_h = 0;
	uint32_t element_table_addr_l = 0;

	zxdh_np_dtb_down_table_element_addr_get(dev_id,
								queue_id,
								element_id,
								&element_start_addr_h,
								&element_start_addr_l,
								&element_table_addr_h,
								&element_table_addr_l);

	PMD_DRV_LOG(DEBUG, "queue_id %u.", queue_id);
	PMD_DRV_LOG(DEBUG, "element_id %u.", element_id);
	PMD_DRV_LOG(DEBUG, "element_start_addr_h 0x%x.", element_start_addr_h);
	PMD_DRV_LOG(DEBUG, "element_start_addr_l 0x%x.", element_start_addr_l);
	PMD_DRV_LOG(DEBUG, "element_table_addr_h 0x%x..", element_table_addr_h);
	PMD_DRV_LOG(DEBUG, "element_table_addr_l 0x%x.", element_table_addr_l);

	rc = zxdh_np_dtb_item_ack_prt(dev_id, queue_id, ZXDH_DTB_DIR_DOWN_TYPE, element_id);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_item_ack_prt");

	rc = zxdh_np_dtb_item_buff_prt(dev_id, queue_id, ZXDH_DTB_DIR_DOWN_TYPE, element_id, 24);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_item_buff_prt");

	return rc;
}

static uint32_t
zxdh_np_dtb_tab_down_success_status_check(uint32_t dev_id,
					uint32_t queue_id,
					uint32_t element_id)
{
	uint32_t rc = 0;
	uint32_t rd_cnt = 0;
	uint32_t ack_value = 0;
	uint32_t success_flag = 0;

	while (!success_flag) {
		rc = zxdh_np_dtb_item_ack_rd(dev_id, queue_id, ZXDH_DTB_DIR_DOWN_TYPE,
			element_id, 0, &ack_value);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_item_ack_rd");

		PMD_DRV_LOG(DEBUG, "zxdh_np_dtb_item_ack_rd ack_value:0x%08x", ack_value);

		if (((ack_value >> 8) & 0xffffff) == ZXDH_DTB_TAB_DOWN_ACK_VLD_MASK) {
			success_flag = 1;
			break;
		}

		if (rd_cnt > ZXDH_DTB_DOWN_OVER_TIME) {
			PMD_DRV_LOG(ERR, "down queue %u item %u overtime!", queue_id, element_id);

			rc = zxdh_np_dtb_down_table_element_info_prt(dev_id, queue_id, element_id);
			ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_down_table_element_info_prt");

			rc = zxdh_np_dtb_item_ack_wr(dev_id, queue_id, ZXDH_DTB_DIR_DOWN_TYPE,
				element_id, 0, ZXDH_DTB_TAB_ACK_UNUSED_MASK);
			ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_item_ack_wr");

			return ZXDH_ERR;
		}

		rd_cnt++;
		rte_delay_us(ZXDH_DTB_DELAY_TIME);
	}

	if ((ack_value & 0xff) != ZXDH_DTB_TAB_ACK_SUCCESS_MASK) {
		rc = zxdh_np_dtb_item_ack_wr(dev_id, queue_id, ZXDH_DTB_DIR_DOWN_TYPE,
			element_id, 0, ZXDH_DTB_TAB_ACK_UNUSED_MASK);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_item_ack_wr");
		return ack_value & 0xff;
	}

	rc = zxdh_np_dtb_item_ack_wr(dev_id, queue_id, ZXDH_DTB_DIR_DOWN_TYPE,
		element_id, 0, ZXDH_DTB_TAB_ACK_UNUSED_MASK);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_item_ack_wr");

	return rc;
}

static uint32_t
zxdh_np_hash_get_hash_info_from_sdt(uint32_t dev_id,
		uint32_t sdt_no, HASH_ENTRY_CFG *p_hash_entry_cfg)
{
	uint32_t rc = ZXDH_OK;
	ZXDH_FUNC_ID_INFO *p_func_info = NULL;
	ZXDH_SE_CFG *p_se_cfg = NULL;
	ZXDH_SDT_TBL_HASH_T sdt_hash_info = {0};

	PMD_DRV_LOG(DEBUG, "sdt_no:%u", sdt_no);
	rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_hash_info);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_read");

	p_hash_entry_cfg->fun_id   = sdt_hash_info.hash_id;
	p_hash_entry_cfg->table_id = sdt_hash_info.hash_table_id;
	p_hash_entry_cfg->bulk_id = ((p_hash_entry_cfg->table_id >> 2) & 0x7);
	p_hash_entry_cfg->key_type = sdt_hash_info.hash_table_width;
	p_hash_entry_cfg->rsp_mode = sdt_hash_info.rsp_mode;
	p_hash_entry_cfg->actu_key_size = sdt_hash_info.key_size;
	p_hash_entry_cfg->key_by_size = ZXDH_GET_KEY_SIZE(p_hash_entry_cfg->actu_key_size);
	p_hash_entry_cfg->rst_by_size = ZXDH_GET_RST_SIZE(p_hash_entry_cfg->key_type,
		p_hash_entry_cfg->actu_key_size);

	p_se_cfg = dpp_se_cfg[ZXDH_DEV_SLOT_ID(dev_id)];
	p_hash_entry_cfg->p_se_cfg = p_se_cfg;

	p_func_info = ZXDH_GET_FUN_INFO(p_se_cfg, p_hash_entry_cfg->fun_id);

	p_hash_entry_cfg->p_hash_cfg = (ZXDH_HASH_CFG *)p_func_info->fun_ptr;
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_hash_entry_cfg->p_hash_cfg);

	return ZXDH_OK;
}

static void
zxdh_np_hash_set_crc_key(HASH_ENTRY_CFG *p_hash_entry_cfg,
					 ZXDH_HASH_ENTRY *p_entry, uint8_t *p_temp_key)
{
	uint32_t key_by_size = 0;
	uint8_t temp_tbl_id  = 0;

	key_by_size = p_hash_entry_cfg->key_by_size;
	memcpy(p_temp_key, p_entry->p_key, key_by_size);

	temp_tbl_id = (*p_temp_key) & 0x1F;
	memmove(p_temp_key, p_temp_key + 1, key_by_size - ZXDH_HASH_KEY_CTR_SIZE);
	p_temp_key[key_by_size - ZXDH_HASH_KEY_CTR_SIZE] = temp_tbl_id;
}

static uint8_t
zxdh_np_hash_sdt_partner_valid(uint32_t sdt_no, uint32_t sdt_partner, uint8_t *p_key)
{
	uint32_t  rc		= ZXDH_OK;
	uint32_t dev_id	= 0;
	uint32_t key_valid = 1;

	ZXDH_SDT_TBL_HASH_T sdt_hash1 = {0};
	ZXDH_SDT_TBL_HASH_T sdt_hash2 = {0};

	if (sdt_no == sdt_partner ||
	sdt_partner <= ZXDH_DEV_SDT_ID_MAX ||
	p_key == NULL) {
		return ZXDH_FALSE;
	}

	PMD_DRV_LOG(DEBUG, "sdt_no:%u", sdt_no);
	PMD_DRV_LOG(DEBUG, "sdt_partner:%u", sdt_partner);
	rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_hash1);
	rc |= zxdh_np_soft_sdt_tbl_get(dev_id, sdt_partner, &sdt_hash2);
	if (rc != ZXDH_OK)
		return ZXDH_FALSE;

	if (sdt_hash1.table_type != (ZXDH_SDT_TBLT_HASH) ||
	sdt_hash2.table_type != ZXDH_SDT_TBLT_HASH ||
	sdt_hash1.hash_table_width != sdt_hash2.hash_table_width ||
	sdt_hash1.key_size != sdt_hash2.key_size ||
	sdt_hash1.rsp_mode != sdt_hash2.rsp_mode) {
		return ZXDH_FALSE;
	}

	*p_key = ZXDH_GET_HASH_KEY_CTRL(key_valid,
									sdt_hash2.hash_table_width,
									sdt_hash2.hash_table_id);

	return ZXDH_TRUE;
}

static uint32_t
zxdh_np_hash_red_black_node_alloc(ZXDH_RB_TN **p_rb_tn_new,
					ZXDH_HASH_RBKEY_INFO **p_rbkey_new)
{
	ZXDH_RB_TN *p_rb_tn_new_temp = NULL;
	ZXDH_HASH_RBKEY_INFO *p_rbkey_new_temp = NULL;

	p_rbkey_new_temp = rte_zmalloc(NULL, sizeof(ZXDH_HASH_RBKEY_INFO), 0);
	if (p_rbkey_new_temp == NULL) {
		PMD_DRV_LOG(ERR, "malloc memory failed");
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	p_rb_tn_new_temp = rte_zmalloc(NULL, sizeof(ZXDH_RB_TN), 0);
	if (p_rb_tn_new_temp == NULL) {
		rte_free(p_rbkey_new_temp);
		PMD_DRV_LOG(ERR, "malloc memory failed");
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	zxdh_np_init_rbt_tn(p_rb_tn_new_temp, p_rbkey_new_temp);

	*p_rb_tn_new = p_rb_tn_new_temp;
	*p_rbkey_new = p_rbkey_new_temp;

	return ZXDH_OK;
}

static uint32_t
zxdh_np_hash_rb_insert(uint32_t dev_id, HASH_ENTRY_CFG *p_hash_entry_cfg,
					ZXDH_HASH_ENTRY *p_entry)
{
	uint32_t		 rc			  = ZXDH_OK;
	ZXDH_HASH_RBKEY_INFO *p_rbkey_rtn	= NULL;
	ZXDH_RB_TN		    *p_rb_tn_rtn	= NULL;
	ZXDH_HASH_RBKEY_INFO *p_rbkey_new	= p_hash_entry_cfg->p_rbkey_new;
	ZXDH_RB_TN		     *p_rb_tn_new	= p_hash_entry_cfg->p_rb_tn_new;
	ZXDH_HASH_CFG	     *p_hash_cfg	= p_hash_entry_cfg->p_hash_cfg;
	uint32_t rst_actual_size = ((p_hash_entry_cfg->rst_by_size) > ZXDH_HASH_RST_MAX) ?
			ZXDH_HASH_RST_MAX : p_hash_entry_cfg->rst_by_size;

	rc = zxdh_comm_rb_insert(&p_hash_cfg->hash_rb, (void *)p_rb_tn_new, (void *)(&p_rb_tn_rtn));
	if (rc == ZXDH_RBT_RC_FULL) {
		PMD_DRV_LOG(ERR, "The red black tree is full!");
		rte_free(p_rbkey_new);
		rte_free(p_rb_tn_new);
		RTE_ASSERT(0);
		return ZXDH_HASH_RC_RB_TREE_FULL;
	} else if (rc == ZXDH_RBT_RC_UPDATE) {
		p_hash_cfg->hash_stat.insert_same++;
		PMD_DRV_LOG(DEBUG, "Hash update exist entry!");

		ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_rb_tn_rtn);
		p_rbkey_rtn = (ZXDH_HASH_RBKEY_INFO *)(p_rb_tn_rtn->p_key);

		memcpy(p_rbkey_rtn->rst, p_entry->p_rst, rst_actual_size);

		rte_free(p_rbkey_new);
		rte_free(p_rb_tn_new);
		p_hash_entry_cfg->p_rbkey_new = p_rbkey_rtn;
		p_hash_entry_cfg->p_rb_tn_new = p_rb_tn_rtn;

		return ZXDH_HASH_RC_ADD_UPDATE;
	}
	PMD_DRV_LOG(DEBUG, "Hash insert new entry!");

	memcpy(p_rbkey_new->rst, p_entry->p_rst, rst_actual_size);
	p_rbkey_new->entry_size = ZXDH_GET_HASH_ENTRY_SIZE(p_hash_entry_cfg->key_type);
	zxdh_np_init_d_node(&p_rbkey_new->entry_dn, p_rbkey_new);

	return rc;
}

static uint32_t
zxdh_np_hash_get_item_free_pos(uint32_t item_entry_max,
				uint32_t wrt_mask, uint32_t entry_size)
{
	uint32_t i = 0;
	uint32_t pos = 0xFFFFFFFF;
	uint32_t mask = 0;

	for (i = 0; i < item_entry_max; i += entry_size / ZXDH_HASH_ENTRY_POS_STEP) {
		mask = ZXDH_GET_HASH_ENTRY_MASK(entry_size, i);

		if (0 == (mask & wrt_mask)) {
			pos = i;
			break;
		}
	}

	return pos;
}

static uint32_t
zxdh_np_hash_insrt_to_item(ZXDH_HASH_CFG *p_hash_cfg,
							ZXDH_HASH_RBKEY_INFO *p_rbkey,
							ZXDH_SE_ITEM_CFG *p_item,
							uint32_t item_idx,
							uint32_t item_type,
							__rte_unused uint32_t insrt_key_type)
{
	uint32_t rc = ZXDH_OK;

	uint32_t free_pos = 0;
	uint32_t dev_id = p_hash_cfg->p_se_info->dev_id;
	uint32_t item_entry_max = 4;

	if (item_type == ZXDH_ITEM_DDR_256)
		item_entry_max = 2;

	if (!p_item->valid) {
		rc = zxdh_comm_double_link_init(item_entry_max, &p_item->item_list);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_comm_double_link_init");

		p_rbkey->entry_pos = 0;
		p_item->wrt_mask = ZXDH_GET_HASH_ENTRY_MASK(p_rbkey->entry_size,
			p_rbkey->entry_pos);
		p_item->item_index = item_idx;
		p_item->item_type = item_type;
		p_item->valid = 1;
	} else {
		free_pos = zxdh_np_hash_get_item_free_pos(item_entry_max,
					p_item->wrt_mask, p_rbkey->entry_size);

		if (free_pos == 0xFFFFFFFF)
			return ZXDH_HASH_RC_ITEM_FULL;
		p_rbkey->entry_pos = free_pos;
		p_item->wrt_mask   |=
			ZXDH_GET_HASH_ENTRY_MASK(p_rbkey->entry_size, p_rbkey->entry_pos);
	}

	PMD_DRV_LOG(DEBUG, "pos is[%u], size is[%u]", free_pos, p_rbkey->entry_size);

	rc = zxdh_comm_double_link_insert_last(&p_rbkey->entry_dn, &p_item->item_list);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_comm_double_link_insert_last");

	p_rbkey->p_item_info = p_item;

	return rc;
}

static uint32_t
zxdh_np_hash_insert_ddr(uint32_t dev_id, HASH_ENTRY_CFG *p_hash_entry_cfg,
					uint8_t *p_temp_key, uint8_t *p_end_flag)
{
	ZXDH_HASH_CFG		*p_hash_cfg = p_hash_entry_cfg->p_hash_cfg;
	uint8_t				bulk_id	    = p_hash_entry_cfg->bulk_id;
	HASH_DDR_CFG		*p_ddr_cfg  = p_hash_cfg->p_bulk_ddr_info[bulk_id];
	uint8_t				key_type	= 0;
	uint8_t				table_id	= p_hash_entry_cfg->table_id;
	uint32_t			key_by_size = 0;
	uint32_t			crc_value	= 0;
	uint32_t			item_idx	= 0xFFFFFFFF;
	uint32_t			item_type	= 0;
	ZXDH_SE_ITEM_CFG    *p_item	    = NULL;
	uint32_t rc = ZXDH_OK;

	key_type = p_hash_entry_cfg->key_type;
	if (ZXDH_HASH_KEY_512b == key_type && ZXDH_DDR_WIDTH_256b == p_ddr_cfg->width_mode) {
		PMD_DRV_LOG(ERR, "hash ddr width mode is not match to the key type.");
		return ZXDH_HASH_RC_DDR_WIDTH_MODE_ERR;
	}

	key_by_size = p_hash_entry_cfg->key_by_size;
	crc_value = p_hash_cfg->p_hash32_fun(p_temp_key, key_by_size, p_ddr_cfg->hash_ddr_arg);
	PMD_DRV_LOG(DEBUG, "hash ddr arg is: 0x%x.crc_value is 0x%x",
		p_ddr_cfg->hash_ddr_arg, crc_value);

	item_idx = crc_value % p_ddr_cfg->item_num;
	item_type = ZXDH_ITEM_DDR_256;
	if (ZXDH_DDR_WIDTH_512b == p_ddr_cfg->width_mode) {
		item_idx = crc_value % p_ddr_cfg->item_num;
		item_type = ZXDH_ITEM_DDR_512;
	}

	PMD_DRV_LOG(DEBUG, "Hash insert in ITEM_DDR_%s, item_idx is: 0x%x.",
		((item_type == ZXDH_ITEM_DDR_256) ? "256" : "512"), item_idx);

	p_item = p_ddr_cfg->p_item_array[item_idx];
	if (p_item == NULL) {
		p_item = rte_zmalloc(NULL, sizeof(ZXDH_SE_ITEM_CFG), 0);
		if (p_item == NULL) {
			PMD_DRV_LOG(ERR, "malloc memory failed");
			return ZXDH_PAR_CHK_POINT_NULL;
		}
		p_ddr_cfg->p_item_array[item_idx] = p_item;
	}

	rc = zxdh_np_hash_insrt_to_item(p_hash_cfg,
						p_hash_entry_cfg->p_rbkey_new,
						p_item,
						item_idx,
						item_type,
						key_type);

	if (rc != ZXDH_HASH_RC_ITEM_FULL) {
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_hash_insrt_to_item");
		*p_end_flag = 1;

		p_hash_cfg->hash_stat.insert_ddr++;
		p_hash_cfg->hash_stat.insert_table[table_id].ddr++;

		p_item->hw_addr = GET_HASH_DDR_HW_ADDR(p_ddr_cfg->hw_baddr, item_idx);
		p_item->bulk_id = p_hash_entry_cfg->bulk_id;
	}

	return rc;
}

static uint32_t
zxdh_np_hash_insert_zcell(uint32_t dev_id, HASH_ENTRY_CFG *p_hash_entry_cfg,
				uint8_t *p_temp_key, uint8_t *p_end_flag)
{
	uint8_t   bulk_id	  = p_hash_entry_cfg->bulk_id;
	ZXDH_D_NODE	   *p_zcell_dn  = NULL;
	ZXDH_SE_ZCELL_CFG *p_zcell	 = NULL;
	uint32_t  zblk_idx	 = 0;
	uint32_t  zcell_id	 = 0;
	uint32_t  pre_zblk_idx = 0xFFFFFFFF;
	ZXDH_SE_ITEM_CFG *p_item	   = NULL;
	uint32_t  item_idx	 = 0xFFFFFFFF;
	uint32_t  item_type	= 0;
	uint32_t  rc		   = ZXDH_OK;
	uint32_t  crc_value	= 0;
	uint8_t   table_id	 = p_hash_entry_cfg->table_id;
	ZXDH_SE_CFG   *p_se_cfg	= NULL;
	ZXDH_HASH_CFG *p_hash_cfg  = NULL;
	ZXDH_SE_ZBLK_CFG  *p_zblk	  = NULL;

	PMD_DRV_LOG(DEBUG, "insert zcell start");
	p_se_cfg = p_hash_entry_cfg->p_se_cfg;
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_se_cfg);
	p_hash_cfg = p_hash_entry_cfg->p_hash_cfg;
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_hash_cfg);
	p_zcell_dn = p_hash_cfg->hash_shareram.zcell_free_list.p_next;

	while (p_zcell_dn) {
		p_zcell = (ZXDH_SE_ZCELL_CFG *)p_zcell_dn->data;
		ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_zcell);

		if (((p_zcell->flag & ZXDH_ZCELL_FLAG_IS_MONO) && p_zcell->bulk_id != bulk_id) ||
		((!(p_zcell->flag & ZXDH_ZCELL_FLAG_IS_MONO)) &&
		p_hash_cfg->bulk_ram_mono[bulk_id])) {
			p_zcell_dn = p_zcell_dn->next;
			continue;
		}

		zblk_idx = GET_ZBLK_IDX(p_zcell->zcell_idx);
		p_zblk = &p_se_cfg->zblk_info[zblk_idx];
		if (zblk_idx != pre_zblk_idx) {
			pre_zblk_idx = zblk_idx;
			crc_value = p_hash_cfg->p_hash16_fun(p_temp_key,
				p_hash_entry_cfg->key_by_size, p_zblk->hash_arg);
		}

		PMD_DRV_LOG(DEBUG, "zblk_idx:[0x%x] hash_arg:[0x%x] crc_value:[0x%x]",
			zblk_idx, p_zblk->hash_arg, crc_value);

		zcell_id = GET_ZCELL_IDX(p_zcell->zcell_idx);
		item_idx = GET_ZCELL_CRC_VAL(zcell_id, crc_value);
		p_item = &p_zcell->item_info[item_idx];
		item_type = ZXDH_ITEM_RAM;

		PMD_DRV_LOG(DEBUG, "zcell_id:[0x%x] item_idx:[0x%x]", zcell_id, item_idx);

		rc = zxdh_np_hash_insrt_to_item(p_hash_cfg,
						p_hash_entry_cfg->p_rbkey_new,
						p_item,
						item_idx,
						item_type,
						p_hash_entry_cfg->key_type);

		if (rc == ZXDH_HASH_RC_ITEM_FULL) {
			PMD_DRV_LOG(DEBUG, "The item is full, check next.");
		} else {
			ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_hash_insrt_to_item");
			*p_end_flag = 1;

			p_hash_cfg->hash_stat.insert_zcell++;
			p_hash_cfg->hash_stat.insert_table[table_id].zcell++;

			p_item->hw_addr = ZXDH_ZBLK_ITEM_ADDR_CALC(p_zcell->zcell_idx, item_idx);
			break;
		}

		p_zcell_dn = p_zcell_dn->next;
	}

	return rc;
}

static uint32_t
zxdh_np_hash_insert_zreg(uint32_t dev_id, HASH_ENTRY_CFG *p_hash_entry_cfg,
				__rte_unused uint8_t *p_temp_key, uint8_t *p_end_flag)
{
	ZXDH_HASH_CFG *p_hash_cfg  = p_hash_entry_cfg->p_hash_cfg;
	ZXDH_D_NODE *p_zblk_dn   = p_hash_cfg->hash_shareram.zblk_list.p_next;
	ZXDH_SE_ZBLK_CFG *p_zblk	  = NULL;
	ZXDH_SE_ZREG_CFG *p_zreg	  = NULL;
	ZXDH_SE_ITEM_CFG *p_item	  = NULL;
	uint8_t reg_index	= 0;
	uint32_t zblk_idx	 = 0;
	uint32_t rc		   = ZXDH_OK;
	uint8_t bulk_id	  = p_hash_entry_cfg->bulk_id;
	uint32_t item_idx	 = 0xFFFFFFFF;
	uint32_t item_type	= 0;
	uint32_t table_id	 = p_hash_entry_cfg->table_id;

	while (p_zblk_dn) {
		p_zblk = (ZXDH_SE_ZBLK_CFG *)p_zblk_dn->data;
		zblk_idx = p_zblk->zblk_idx;

		for (reg_index = 0; reg_index < ZXDH_SE_ZREG_NUM; reg_index++) {
			p_zreg = &p_zblk->zreg_info[reg_index];

			if (((p_zreg->flag & ZXDH_ZREG_FLAG_IS_MONO) &&
				p_zreg->bulk_id != bulk_id) ||
				((!(p_zreg->flag & ZXDH_ZREG_FLAG_IS_MONO)) &&
				p_hash_cfg->bulk_ram_mono[bulk_id])) {
				continue;
			}

			p_item = &p_zblk->zreg_info[reg_index].item_info;
			item_type = ZXDH_ITEM_REG;
			item_idx = reg_index;
			rc = zxdh_np_hash_insrt_to_item(p_hash_cfg,
							p_hash_entry_cfg->p_rbkey_new,
							p_item,
							item_idx,
							item_type,
							p_hash_entry_cfg->key_type);

			if (rc == ZXDH_HASH_RC_ITEM_FULL) {
				PMD_DRV_LOG(DEBUG, "The item is full, check next.");
			} else {
				ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_hash_insrt_to_item");
				*p_end_flag = 1;

				p_hash_cfg->hash_stat.insert_zreg++;
				p_hash_cfg->hash_stat.insert_table[table_id].zreg++;

				p_item->hw_addr = ZXDH_ZBLK_HASH_LIST_REG_ADDR_CALC(zblk_idx,
					reg_index);
				break;
			}
		}

		if (*p_end_flag)
			break;

		p_zblk_dn = p_zblk_dn->next;
	}

	return rc;
}

static uint32_t
zxdh_np_dtb_hash_form_write(ZXDH_HASH_CFG *p_hash_cfg,
							ZXDH_HASH_RBKEY_INFO *p_rbkey_new,
							uint32_t actu_key_size,
							ZXDH_DTB_ENTRY_T *p_entry,
							__rte_unused uint32_t opr_mode)
{
	uint32_t key_type = 0;
	uint32_t key_by_size = 0;
	uint32_t rst_by_size = 0;
	uint32_t byte_offset = 0;
	uint32_t dev_id = p_hash_cfg->p_se_info->dev_id;
	uint32_t addr;

	ZXDH_D_NODE *p_entry_dn = NULL;
	ZXDH_HASH_RBKEY_INFO *p_rbkey = NULL;
	uint8_t entry_data[ZXDH_SE_ENTRY_WIDTH_MAX] = {0};
	ZXDH_SE_ITEM_CFG *p_item_info = p_rbkey_new->p_item_info;

	if (p_item_info == NULL) {
		PMD_DRV_LOG(ERR, "p_item_info point null!");
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	PMD_DRV_LOG(DEBUG, "zcam p_item_info->hw_addr is 0x%x",
		p_item_info->hw_addr);
	addr = p_item_info->hw_addr;

	p_entry_dn = p_item_info->item_list.p_next;

	while (p_entry_dn) {
		p_rbkey = (ZXDH_HASH_RBKEY_INFO *)(p_entry_dn->data);
		key_type = ZXDH_GET_HASH_KEY_TYPE(p_rbkey->key);
		key_by_size = ZXDH_GET_KEY_SIZE(actu_key_size);
		rst_by_size = ZXDH_GET_RST_SIZE(key_type, actu_key_size);

		byte_offset = p_rbkey->entry_pos * ZXDH_HASH_ENTRY_POS_STEP;
		memcpy(entry_data + byte_offset, p_rbkey->key, key_by_size);

		byte_offset += key_by_size;
		memcpy(entry_data + byte_offset, p_rbkey->rst,
			((rst_by_size > ZXDH_HASH_RST_MAX) ? ZXDH_HASH_RST_MAX : rst_by_size));

		p_entry_dn = p_entry_dn->next;
	}

	zxdh_np_comm_swap(entry_data, ZXDH_SE_ENTRY_WIDTH_MAX);

	zxdh_np_dtb_se_alg_zcam_data_write(dev_id,
									addr,
									entry_data,
									p_entry);

	return ZXDH_OK;
}

static uint32_t
zxdh_np_dtb_hash_delete(uint32_t dev_id,
						uint32_t sdt_no,
						ZXDH_HASH_ENTRY *p_hash_entry,
						ZXDH_DTB_ENTRY_T *p_dtb_one_entry)
{
	uint32_t rc = ZXDH_OK;
	ZXDH_HASH_CFG *p_hash_cfg = NULL;
	ZXDH_SE_ITEM_CFG *p_item = NULL;
	ZXDH_RB_TN *p_rb_tn_rtn = NULL;
	ZXDH_HASH_RBKEY_INFO *p_rbkey_rtn = NULL;
	ZXDH_SPINLOCK_T *p_hash_spinlock = NULL;
	HASH_ENTRY_CFG  hash_entry_cfg = {0};
	ZXDH_HASH_RBKEY_INFO temp_rbkey = {0};
	HASH_DDR_CFG       *bulk_ddr_info = NULL;

	rc = zxdh_np_hash_get_hash_info_from_sdt(dev_id, sdt_no, &hash_entry_cfg);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_hash_get_hash_info_from_sdt");

	p_hash_cfg = hash_entry_cfg.p_hash_cfg;
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_hash_cfg);

	zxdh_np_dev_hash_opr_spinlock_get(dev_id, p_hash_cfg->fun_id, &p_hash_spinlock);
	rte_spinlock_lock(&p_hash_spinlock->spinlock);

	memcpy(temp_rbkey.key, p_hash_entry->p_key, hash_entry_cfg.key_by_size);
	rc = zxdh_comm_rb_delete(&p_hash_cfg->hash_rb, &temp_rbkey, &p_rb_tn_rtn);
	if (rc == ZXDH_RBT_RC_SRHFAIL) {
		p_hash_cfg->hash_stat.delete_fail++;
		PMD_DRV_LOG(ERR, "Error!there is not item in hash!");
		rte_spinlock_unlock(&p_hash_spinlock->spinlock);
		return ZXDH_HASH_RC_DEL_SRHFAIL;
	}

	p_rbkey_rtn = (ZXDH_HASH_RBKEY_INFO *)(p_rb_tn_rtn->p_key);
	memset(p_rbkey_rtn->rst, 0, sizeof(p_rbkey_rtn->rst));
	hash_entry_cfg.p_rbkey_new = p_rbkey_rtn;
	hash_entry_cfg.p_rb_tn_new = p_rb_tn_rtn;

	p_item = p_rbkey_rtn->p_item_info;
	rc = zxdh_comm_double_link_del(&p_rbkey_rtn->entry_dn, &p_item->item_list);
	if (rc != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "zxdh_comm_double_link_del failed, rc=0x%x.", rc);
		rte_spinlock_unlock(&p_hash_spinlock->spinlock);
		return ZXDH_ERR;
	}
	p_item->wrt_mask &= ~(ZXDH_GET_HASH_ENTRY_MASK(p_rbkey_rtn->entry_size,
		p_rbkey_rtn->entry_pos)) & 0xF;

	rc = zxdh_np_dtb_hash_form_write(hash_entry_cfg.p_hash_cfg,
								 hash_entry_cfg.p_rbkey_new,
								 hash_entry_cfg.actu_key_size,
								 p_dtb_one_entry,
								 ZXDH_DTB_ITEM_DELETE);
	if (rc != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "zxdh_np_dtb_hash_form_write failed, rc=0x%x.", rc);
		rte_spinlock_unlock(&p_hash_spinlock->spinlock);
		return ZXDH_ERR;
	}

	if (p_item->item_list.used == 0) {
		if (p_item->item_type == ZXDH_ITEM_DDR_256 ||
			p_item->item_type == ZXDH_ITEM_DDR_512) {
			bulk_ddr_info = p_hash_cfg->p_bulk_ddr_info[hash_entry_cfg.bulk_id];
			if (p_item->item_index > bulk_ddr_info->item_num) {
				rte_spinlock_unlock(&p_hash_spinlock->spinlock);
				return ZXDH_PAR_CHK_INVALID_INDEX;
			}
			bulk_ddr_info->p_item_array[p_item->item_index] = NULL;
			rte_free(p_item);
		} else {
			p_item->valid = 0;
		}
	}

	rte_free(p_rbkey_rtn);
	rte_free(p_rb_tn_rtn);
	p_hash_cfg->hash_stat.delete_ok++;
	rte_spinlock_unlock(&p_hash_spinlock->spinlock);
	return rc;
}

static uint32_t
zxdh_np_dtb_hash_insert(uint32_t dev_id,
						uint32_t sdt_no,
						ZXDH_HASH_ENTRY *p_hash_entry,
						ZXDH_DTB_ENTRY_T *p_dtb_one_entry)
{
	uint32_t rc = ZXDH_OK;
	uint8_t end_flag = 0;
	uint8_t temp_key[ZXDH_HASH_KEY_MAX] = {0};
	ZXDH_HASH_CFG *p_hash_cfg = NULL;
	ZXDH_RB_TN *p_rb_tn_rtn = NULL;
	ZXDH_RB_TN *p_rb_tn_new = NULL;
	ZXDH_HASH_RBKEY_INFO *p_rbkey_new = NULL;
	ZXDH_SPINLOCK_T *p_hash_spinlock = NULL;
	HASH_ENTRY_CFG  hash_entry_cfg = {0};

	rc = zxdh_np_hash_get_hash_info_from_sdt(dev_id, sdt_no, &hash_entry_cfg);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_hash_get_hash_info_from_sdt");

	p_hash_cfg = hash_entry_cfg.p_hash_cfg;
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_hash_cfg);

	zxdh_np_dev_hash_opr_spinlock_get(dev_id, p_hash_cfg->fun_id, &p_hash_spinlock);
	rte_spinlock_lock(&p_hash_spinlock->spinlock);

	rc = zxdh_np_hash_red_black_node_alloc(&p_rb_tn_new, &p_rbkey_new);
	if (rc != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "zxdh_np_hash_red_black_node_alloc failed, rc=0x%x.", rc);
		rte_spinlock_unlock(&p_hash_spinlock->spinlock);
		return ZXDH_ERR;
	}
	memcpy(p_rbkey_new->key, p_hash_entry->p_key,
		hash_entry_cfg.key_by_size);
	hash_entry_cfg.p_rbkey_new = p_rbkey_new;
	hash_entry_cfg.p_rb_tn_new = p_rb_tn_new;

	rc = zxdh_np_hash_rb_insert(dev_id, &hash_entry_cfg, p_hash_entry);
	if (rc != ZXDH_OK) {
		if (rc == ZXDH_HASH_RC_ADD_UPDATE) {
			rc = zxdh_np_dtb_hash_form_write(p_hash_cfg,
								 hash_entry_cfg.p_rbkey_new,
								 hash_entry_cfg.actu_key_size,
								 p_dtb_one_entry,
								 ZXDH_DTB_ITEM_ADD_OR_UPDATE);
			if (rc != ZXDH_OK) {
				PMD_DRV_LOG(ERR, "dtb_hash_form_write failed, rc=0x%x.", rc);
				rte_spinlock_unlock(&p_hash_spinlock->spinlock);
				return ZXDH_ERR;
			}
		}

		rte_spinlock_unlock(&p_hash_spinlock->spinlock);
		return rc;
	}

	zxdh_np_hash_set_crc_key(&hash_entry_cfg, p_hash_entry, temp_key);

	if (p_hash_cfg->ddr_valid) {
		rc = zxdh_np_hash_insert_ddr(dev_id, &hash_entry_cfg, temp_key, &end_flag);
		if (rc != ZXDH_OK) {
			PMD_DRV_LOG(ERR, "hash_insert_ddr failed, rc=0x%x.", rc);
			rte_spinlock_unlock(&p_hash_spinlock->spinlock);
			return ZXDH_ERR;
		}
	}

	if (!end_flag) {
		rc = zxdh_np_hash_insert_zcell(dev_id, &hash_entry_cfg, temp_key, &end_flag);
		if (rc != ZXDH_OK) {
			PMD_DRV_LOG(ERR, "hash_insert_zcell failed, rc=0x%x.", rc);
			rte_spinlock_unlock(&p_hash_spinlock->spinlock);
			return ZXDH_ERR;
		}
	}

	if (!end_flag) {
		rc = zxdh_np_hash_insert_zreg(dev_id, &hash_entry_cfg, temp_key, &end_flag);
		if (rc != ZXDH_OK) {
			PMD_DRV_LOG(ERR, "hash_insert_zreg failed, rc=0x%x.", rc);
			rte_spinlock_unlock(&p_hash_spinlock->spinlock);
			return ZXDH_ERR;
		}
	}

	if (!end_flag) {
		p_hash_cfg->hash_stat.insert_fail++;
		memcpy(temp_key, p_hash_entry->p_key, hash_entry_cfg.key_by_size);
		rc = zxdh_comm_rb_delete(&p_hash_cfg->hash_rb, p_rbkey_new, &p_rb_tn_rtn);
		if (rc != ZXDH_OK) {
			PMD_DRV_LOG(ERR, "hash_insert_zreg failed, rc=0x%x.", rc);
			rte_spinlock_unlock(&p_hash_spinlock->spinlock);
			return ZXDH_ERR;
		}
		RTE_ASSERT(p_rb_tn_new == p_rb_tn_rtn);
		rte_free(p_rbkey_new);
		rte_free(p_rb_tn_rtn);
		PMD_DRV_LOG(ERR, "ZXDH_HASH_RC_TBL_FULL.sdt_no=%u", sdt_no);
		rte_spinlock_unlock(&p_hash_spinlock->spinlock);
		return ZXDH_RC_DTB_DOWN_HASH_CONFLICT;
	}

	rc = zxdh_np_dtb_hash_form_write(p_hash_cfg,
								 hash_entry_cfg.p_rbkey_new,
								 hash_entry_cfg.actu_key_size,
								 p_dtb_one_entry,
								 ZXDH_DTB_ITEM_ADD_OR_UPDATE);
	if (rc != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "hash form write failed, rc=0x%x.", rc);
		rte_spinlock_unlock(&p_hash_spinlock->spinlock);
		return ZXDH_ERR;
	}

	p_hash_cfg->hash_stat.insert_ok++;
	rte_spinlock_unlock(&p_hash_spinlock->spinlock);
	return rc;
}

static uint32_t
zxdh_np_apt_get_sdt_partner(uint32_t dev_id, uint32_t sdt_no)
{
	SE_APT_CALLBACK_T *p_apt_callback = NULL;

	p_apt_callback = &g_apt_se_callback[ZXDH_DEV_SLOT_ID(dev_id)][sdt_no];

	if (p_apt_callback->table_type == ZXDH_SDT_TBLT_HASH)
		return  p_apt_callback->se_func_info.hash_func.sdt_partner;

	if (p_apt_callback->table_type == ZXDH_SDT_TBLT_ETCAM)
		return  p_apt_callback->se_func_info.acl_func.sdt_partner;

	return UINT32_MAX;
}

static uint32_t
zxdh_np_dtb_hash_one_entry(uint32_t dev_id,
							uint32_t sdt_no,
							uint32_t del_en,
							void *p_data,
							uint32_t *p_dtb_len,
							ZXDH_DTB_ENTRY_T *p_dtb_one_entry)
{
	uint32_t rc = ZXDH_OK;
	uint8_t temp_key[ZXDH_HASH_KEY_MAX] = {0};
	uint8_t temp_rst[ZXDH_HASH_RST_MAX] = {0};
	uint8_t key_valid = 1;
	uint8_t key = 0;
	uint32_t sdt_partner = 0;
	uint8_t valid	 = 0;

	ZXDH_SDT_TBL_HASH_T sdt_hash = {0};
	ZXDH_DTB_HASH_ENTRY_INFO_T *p_entry = NULL;
	ZXDH_HASH_ENTRY entry = {0};

	PMD_DRV_LOG(DEBUG, "sdt_no:%u", sdt_no);
	rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_hash);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");

	entry.p_key = temp_key;
	entry.p_rst = temp_rst;
	entry.p_key[0] = ZXDH_GET_HASH_KEY_CTRL(key_valid,
							sdt_hash.hash_table_width,
							sdt_hash.hash_table_id);
	p_entry = (ZXDH_DTB_HASH_ENTRY_INFO_T *)p_data;
	memcpy(&entry.p_key[1], p_entry->p_actu_key,
		ZXDH_GET_ACTU_KEY_BY_SIZE(sdt_hash.key_size));
	memcpy(entry.p_rst, p_entry->p_rst, 4 * (0x1 << sdt_hash.rsp_mode));

	if (del_en) {
		do {
			rc = zxdh_np_dtb_hash_delete(dev_id, sdt_no, &entry, p_dtb_one_entry);
			sdt_partner = zxdh_np_apt_get_sdt_partner(dev_id, sdt_no);
			valid = zxdh_np_hash_sdt_partner_valid(sdt_no, sdt_partner, &key);
			entry.p_key[0] = key;
			sdt_no = sdt_partner;
		} while ((rc == ZXDH_HASH_RC_DEL_SRHFAIL) && (valid == ZXDH_TRUE));

		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_hash_delete");
	} else {
		do {
			rc = zxdh_np_dtb_hash_insert(dev_id, sdt_no, &entry, p_dtb_one_entry);
			sdt_partner = zxdh_np_apt_get_sdt_partner(dev_id, sdt_no);
			valid = zxdh_np_hash_sdt_partner_valid(sdt_no, sdt_partner, &key);
			entry.p_key[0] = key;
			sdt_no = sdt_partner;
		} while ((rc == ZXDH_RC_DTB_DOWN_HASH_CONFLICT) && (valid == ZXDH_TRUE));

		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_hash_insert");
	}

	*p_dtb_len = p_dtb_one_entry->data_size / ZXDH_DTB_LEN_POS_SETP + 1;

	return rc;
}

static void
zxdh_np_acl_hdw_addr_get(ZXDH_ACL_TBL_CFG_T *p_tbl_cfg, uint32_t handle,
		uint32_t *p_block_idx, uint32_t *p_addr, uint32_t *p_wr_mask)
{
	uint32_t block_entry_num = ZXDH_ACL_ENTRY_MAX_GET(p_tbl_cfg->key_mode, 1);
	uint32_t entry_pos = (handle % block_entry_num) % (1U << p_tbl_cfg->key_mode);

	*p_block_idx = p_tbl_cfg->block_array[handle / block_entry_num];
	*p_addr = (handle % block_entry_num) / (1U << p_tbl_cfg->key_mode);
	*p_wr_mask = (((1U << (8U >> (p_tbl_cfg->key_mode))) - 1) <<
		((8U >> (p_tbl_cfg->key_mode)) * (entry_pos))) & 0xFF;
}

static void
zxdh_np_etcam_dm_to_xy(ZXDH_ETCAM_ENTRY_T *p_dm,
				   ZXDH_ETCAM_ENTRY_T *p_xy,
				   uint32_t len)
{
	uint32_t i = 0;

	RTE_ASSERT(p_dm->p_data && p_dm->p_mask && p_xy->p_data && p_xy->p_mask);

	for (i = 0; i < len; i++) {
		p_xy->p_data[i] = ZXDH_COMM_DM_TO_X(p_dm->p_data[i], p_dm->p_mask[i]);
		p_xy->p_mask[i] = ZXDH_COMM_DM_TO_Y(p_dm->p_data[i], p_dm->p_mask[i]);
	}
}

static uint32_t
zxdh_np_eram_opr_mode_get(uint32_t as_mode)
{
	uint32_t opr_mode = 0;

	switch (as_mode) {
	case ZXDH_ERAM128_TBL_128b:
		opr_mode =  ZXDH_ERAM128_OPR_128b;
		break;
	case ZXDH_ERAM128_TBL_64b:
		opr_mode =  ZXDH_ERAM128_OPR_64b;
		break;
	case ZXDH_ERAM128_TBL_1b:
		opr_mode =  ZXDH_ERAM128_OPR_1b;
		break;
	default:
		break;
	}

	return opr_mode;
}

static uint32_t
zxdh_np_dtb_etcam_entry_add(uint32_t dev_id,
							uint32_t addr,
							uint32_t block_idx,
							uint32_t wr_mask,
							uint32_t opr_type,
							ZXDH_ETCAM_ENTRY_T *p_entry,
							ZXDH_DTB_ENTRY_T *p_entry_data,
							ZXDH_DTB_ENTRY_T *p_entry_mask)
{
	uint32_t rc = ZXDH_OK;
	uint8_t temp_data[ZXDH_ETCAM_WIDTH_MAX / 8] = {0};
	uint8_t temp_mask[ZXDH_ETCAM_WIDTH_MAX / 8] = {0};
	ZXDH_ETCAM_ENTRY_T entry_xy = {0};

	RTE_ASSERT(p_entry->p_data && p_entry->p_mask);

	entry_xy.p_data = temp_data;
	entry_xy.p_mask = temp_mask;

	if (opr_type == ZXDH_ETCAM_OPR_DM) {
		zxdh_np_etcam_dm_to_xy(p_entry, &entry_xy,
			ZXDH_ETCAM_ENTRY_SIZE_GET(p_entry->mode));
	} else {
		memcpy(entry_xy.p_data, p_entry->p_data,
			ZXDH_ETCAM_ENTRY_SIZE_GET(p_entry->mode));
		memcpy(entry_xy.p_mask, p_entry->p_mask,
			ZXDH_ETCAM_ENTRY_SIZE_GET(p_entry->mode));
	}

	rc = zxdh_np_dtb_etcam_write_entry_data(dev_id, block_idx, 0, 1, 0, 0, 0,
		wr_mask, ZXDH_ETCAM_DTYPE_DATA, addr, 0, entry_xy.p_data, p_entry_data);

	rc = zxdh_np_dtb_etcam_write_entry_data(dev_id, block_idx, 0, 1, 0, 0, 0,
		wr_mask, ZXDH_ETCAM_DTYPE_MASK, addr, 0xFF, entry_xy.p_mask, p_entry_mask);

	return rc;
}

static uint32_t
zxdh_np_dtb_acl_delete(uint32_t dev_id,
						uint32_t sdt_no,
						ZXDH_ACL_ENTRY_EX_T *p_acl_entry,
						ZXDH_DTB_ENTRY_T *p_dtb_data_entry,
						ZXDH_DTB_ENTRY_T *p_dtb_mask_entry,
						ZXDH_DTB_ENTRY_T *p_dtb_as_entry)
{
	uint32_t  rc = ZXDH_OK;
	uint32_t as_eram_baddr = 0;
	uint32_t as_enable = 0;
	uint32_t etcam_table_id = 0;
	uint32_t etcam_as_mode = 0;
	uint32_t opr_mode = 0;
	uint32_t block_idx = 0;
	uint32_t ram_addr = 0;
	uint32_t etcam_wr_mask = 0;
	uint8_t temp_data[ZXDH_ETCAM_WIDTH_MAX / 8];
	uint8_t temp_mask[ZXDH_ETCAM_WIDTH_MAX / 8];
	uint8_t temp_buf[16] = {0};

	ZXDH_SDT_TBL_ETCAM_T sdt_acl = {0};
	ZXDH_ACL_CFG_EX_T *p_acl_cfg = NULL;
	ZXDH_ACL_TBL_CFG_T *p_tbl_cfg = NULL;
	ZXDH_ETCAM_ENTRY_T etcam_entry = {0};

	PMD_DRV_LOG(DEBUG, "sdt_no:%u", sdt_no);
	rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_acl);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");

	etcam_as_mode = sdt_acl.as_rsp_mode;
	etcam_table_id = sdt_acl.etcam_table_id;
	as_enable = sdt_acl.as_en;
	as_eram_baddr = sdt_acl.as_eram_baddr;

	zxdh_np_acl_cfg_get(dev_id, &p_acl_cfg);

	p_tbl_cfg = p_acl_cfg->acl_tbls + etcam_table_id;
	if (!p_tbl_cfg->is_used) {
		PMD_DRV_LOG(ERR, "table[ %u ] is not init!", etcam_table_id);
		RTE_ASSERT(0);
		return ZXDH_ACL_RC_TBL_NOT_INIT;
	}

	zxdh_np_acl_hdw_addr_get(p_tbl_cfg, p_acl_entry->pri,
		&block_idx, &ram_addr, &etcam_wr_mask);

	memset(temp_data, 0xff, ZXDH_ETCAM_WIDTH_MAX / 8);
	memset(temp_mask, 0, ZXDH_ETCAM_WIDTH_MAX / 8);
	etcam_entry.mode = p_tbl_cfg->key_mode;
	etcam_entry.p_data = temp_data;
	etcam_entry.p_mask = temp_mask;
	rc = zxdh_np_dtb_etcam_entry_add(dev_id,
							   ram_addr,
							   block_idx,
							   etcam_wr_mask,
							   ZXDH_ETCAM_OPR_DM,
							   &etcam_entry,
							   p_dtb_data_entry,
							   p_dtb_mask_entry);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_etcam_entry_add");

	if (as_enable) {
		memset(temp_buf, 0, sizeof(temp_buf));
		opr_mode = zxdh_np_eram_opr_mode_get(etcam_as_mode);
		rc = zxdh_np_dtb_se_smmu0_ind_write(dev_id,
								   as_eram_baddr,
								   p_acl_entry->pri,
								   opr_mode,
								   (uint32_t *)temp_buf,
								   p_dtb_as_entry);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_se_smmu0_ind_write");
	}

	return rc;
}

static uint32_t
zxdh_np_dtb_acl_insert(uint32_t dev_id,
					uint32_t sdt_no,
					ZXDH_ACL_ENTRY_EX_T *p_acl_entry,
					ZXDH_DTB_ENTRY_T *p_dtb_data_entry,
					ZXDH_DTB_ENTRY_T *p_dtb_mask_entry,
					ZXDH_DTB_ENTRY_T *p_dtb_as_entry)
{
	uint32_t  rc = ZXDH_OK;
	uint32_t as_eram_baddr = 0;
	uint32_t as_enable = 0;
	uint32_t etcam_table_id = 0;
	uint32_t etcam_as_mode = 0;
	uint32_t opr_mode = 0;
	uint32_t block_idx = 0;
	uint32_t ram_addr = 0;
	uint32_t etcam_wr_mask = 0;

	ZXDH_SDT_TBL_ETCAM_T sdt_acl = {0};
	ZXDH_ACL_CFG_EX_T *p_acl_cfg = NULL;
	ZXDH_ACL_TBL_CFG_T *p_tbl_cfg = NULL;
	ZXDH_ETCAM_ENTRY_T etcam_entry = {0};

	PMD_DRV_LOG(DEBUG, "sdt_no:%u", sdt_no);
	rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_acl);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");

	etcam_as_mode = sdt_acl.as_rsp_mode;
	etcam_table_id = sdt_acl.etcam_table_id;
	as_enable = sdt_acl.as_en;
	as_eram_baddr = sdt_acl.as_eram_baddr;

	zxdh_np_acl_cfg_get(dev_id, &p_acl_cfg);

	p_tbl_cfg = p_acl_cfg->acl_tbls + etcam_table_id;
	if (!p_tbl_cfg->is_used) {
		PMD_DRV_LOG(ERR, "table[ %u ] is not init!", etcam_table_id);
		RTE_ASSERT(0);
		return ZXDH_ACL_RC_TBL_NOT_INIT;
	}

	zxdh_np_acl_hdw_addr_get(p_tbl_cfg, p_acl_entry->pri,
		&block_idx, &ram_addr, &etcam_wr_mask);

	etcam_entry.mode = p_tbl_cfg->key_mode;
	etcam_entry.p_data = p_acl_entry->key_data;
	etcam_entry.p_mask = p_acl_entry->key_mask;

	rc = zxdh_np_dtb_etcam_entry_add(dev_id,
						ram_addr,
						block_idx,
						etcam_wr_mask,
						ZXDH_ETCAM_OPR_DM,
						&etcam_entry,
						p_dtb_data_entry,
						p_dtb_mask_entry);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_etcam_entry_add");

	if (as_enable) {
		opr_mode = zxdh_np_eram_opr_mode_get(etcam_as_mode);
		rc = zxdh_np_dtb_se_smmu0_ind_write(dev_id,
						as_eram_baddr,
						p_acl_entry->pri,
						opr_mode,
						(uint32_t *)p_acl_entry->p_as_rslt,
						p_dtb_as_entry);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_se_smmu0_ind_write");
	}

	return rc;
}

static uint32_t
zxdh_np_dtb_acl_one_entry(uint32_t dev_id,
						uint32_t sdt_no,
						uint32_t del_en,
						void *p_data,
						uint32_t *p_dtb_len,
						uint8_t *p_data_buff)
{
	uint32_t  rc	   = ZXDH_OK;
	uint32_t addr_offset = 0;
	ZXDH_ACL_ENTRY_EX_T acl_entry = {0};
	ZXDH_DTB_ACL_ENTRY_INFO_T *p_entry = NULL;

	uint8_t data_buff[ZXDH_ETCAM_WIDTH_MAX / 8] = {0};
	uint8_t mask_buff[ZXDH_ETCAM_WIDTH_MAX / 8] = {0};
	uint8_t data_cmd_buff[ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8] = {0};
	uint8_t mask_cmd_buff[ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8] = {0};
	uint8_t as_cmd_buff[ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8] = {0};
	uint32_t as_data_buff[4] = {0};

	ZXDH_DTB_ENTRY_T dtb_data_entry = {0};
	ZXDH_DTB_ENTRY_T dtb_mask_entry = {0};
	ZXDH_DTB_ENTRY_T dtb_as_entry = {0};

	dtb_data_entry.cmd = data_cmd_buff;
	dtb_data_entry.data = data_buff;
	dtb_mask_entry.cmd = mask_cmd_buff;
	dtb_mask_entry.data = mask_buff;
	dtb_as_entry.cmd = as_cmd_buff;
	dtb_as_entry.data = (uint8_t *)as_data_buff;

	p_entry = (ZXDH_DTB_ACL_ENTRY_INFO_T *)p_data;
	acl_entry.pri = p_entry->handle;
	acl_entry.key_data = p_entry->key_data;
	acl_entry.key_mask = p_entry->key_mask;
	acl_entry.p_as_rslt = p_entry->p_as_rslt;
	if (del_en) {
		rc = zxdh_np_dtb_acl_delete(dev_id,
							sdt_no,
							&acl_entry,
							&dtb_data_entry,
							&dtb_mask_entry,
							&dtb_as_entry);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_acl_delete");
	} else {
		rc = zxdh_np_dtb_acl_insert(dev_id,
							sdt_no,
							&acl_entry,
							&dtb_data_entry,
							&dtb_mask_entry,
							&dtb_as_entry);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_acl_insert");
	}

	addr_offset = (*p_dtb_len) * ZXDH_DTB_LEN_POS_SETP;
	*p_dtb_len += ZXDH_DTB_ETCAM_LEN_SIZE;

	rc = zxdh_np_dtb_data_write(p_data_buff, addr_offset, &dtb_data_entry);

	addr_offset = (*p_dtb_len) * ZXDH_DTB_LEN_POS_SETP;
	*p_dtb_len += ZXDH_DTB_ETCAM_LEN_SIZE;

	rc = zxdh_np_dtb_data_write(p_data_buff, addr_offset, &dtb_mask_entry);

	addr_offset = (*p_dtb_len) * ZXDH_DTB_LEN_POS_SETP;
	if (dtb_as_entry.data_in_cmd_flag)
		*p_dtb_len += 1;
	else
		*p_dtb_len += 2;

	rc = zxdh_np_dtb_data_write(p_data_buff, addr_offset, &dtb_as_entry);

	return ZXDH_OK;
}

int
zxdh_np_dtb_table_entry_write(uint32_t dev_id,
			uint32_t queue_id,
			uint32_t entrynum,
			ZXDH_DTB_USER_ENTRY_T *down_entries)
{
	ZXDH_DTB_USER_ENTRY_T *pentry = NULL;
	ZXDH_DTB_ENTRY_T   dtb_one_entry = {0};
	uint8_t entry_cmd[ZXDH_DTB_TABLE_CMD_SIZE_BIT] = {0};
	uint8_t entry_data[ZXDH_ETCAM_WIDTH_MAX] = {0};
	uint8_t *p_data_buff;
	uint32_t element_id = 0xff;
	uint32_t one_dtb_len = 0;
	uint32_t dtb_len = 0;
	uint32_t entry_index;
	uint32_t sdt_no;
	uint32_t tbl_type;
	uint32_t addr_offset;
	uint32_t max_size;
	uint32_t rc = 0;

	p_data_buff = rte_zmalloc(NULL, ZXDH_DTB_TABLE_DATA_BUFF_SIZE, 0);
	ZXDH_COMM_CHECK_POINT(p_data_buff);

	dtb_one_entry.cmd = entry_cmd;
	dtb_one_entry.data = entry_data;

	max_size = (ZXDH_DTB_TABLE_DATA_BUFF_SIZE / 16) - 1;

	for (entry_index = 0; entry_index < entrynum; entry_index++) {
		pentry = down_entries + entry_index;
		sdt_no = pentry->sdt_no;
		PMD_DRV_LOG(DEBUG, "sdt_no:%u", sdt_no);
		tbl_type = zxdh_np_sdt_tbl_type_get(dev_id, sdt_no);
		switch (tbl_type) {
		case ZXDH_SDT_TBLT_ERAM:
			rc = zxdh_np_dtb_eram_one_entry(dev_id, sdt_no, ZXDH_DTB_ITEM_ADD_OR_UPDATE,
				pentry->p_entry_data, &one_dtb_len, &dtb_one_entry);
			break;
		case ZXDH_SDT_TBLT_HASH:
			rc = zxdh_np_dtb_hash_one_entry(dev_id, sdt_no, ZXDH_DTB_ITEM_ADD_OR_UPDATE,
				pentry->p_entry_data, &one_dtb_len, &dtb_one_entry);
			break;
		case ZXDH_SDT_TBLT_ETCAM:
			rc = zxdh_np_dtb_acl_one_entry(dev_id, sdt_no, ZXDH_DTB_ITEM_ADD_OR_UPDATE,
				pentry->p_entry_data, &dtb_len, p_data_buff);
			continue;
		default:
			PMD_DRV_LOG(ERR, "SDT table_type[ %u ] is invalid!", tbl_type);
			rte_free(p_data_buff);
			return 1;
		}

		addr_offset = dtb_len * ZXDH_DTB_LEN_POS_SETP;
		dtb_len += one_dtb_len;
		if (dtb_len > max_size) {
			rte_free(p_data_buff);
			PMD_DRV_LOG(ERR, "error dtb_len>%u!", max_size);
			return ZXDH_RC_DTB_DOWN_LEN_INVALID;
		}
		rc = zxdh_np_dtb_data_write(p_data_buff, addr_offset, &dtb_one_entry);
		memset(entry_cmd, 0x0, sizeof(entry_cmd));
		memset(entry_data, 0x0, sizeof(entry_data));
	}

	if (dtb_len == 0) {
		rte_free(p_data_buff);
		return ZXDH_RC_DTB_DOWN_LEN_INVALID;
	}

	rc = zxdh_np_dtb_write_down_table_data(dev_id,
					queue_id,
					dtb_len * 16,
					p_data_buff,
					&element_id);
	rte_free(p_data_buff);

	rc = zxdh_np_dtb_tab_down_success_status_check(dev_id, queue_id, element_id);

	return rc;
}

int
zxdh_np_dtb_table_entry_delete(uint32_t dev_id,
			 uint32_t queue_id,
			 uint32_t entrynum,
			 ZXDH_DTB_USER_ENTRY_T *delete_entries)
{
	ZXDH_SDT_TBL_DATA_T sdt_tbl = {0};
	ZXDH_DTB_USER_ENTRY_T *pentry = NULL;
	ZXDH_DTB_ENTRY_T   dtb_one_entry = {0};
	uint8_t entry_cmd[ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8] = {0};
	uint8_t entry_data[ZXDH_ETCAM_WIDTH_MAX / 8] = {0};
	uint8_t *p_data_buff = NULL;
	uint32_t tbl_type = 0;
	uint32_t element_id = 0xff;
	uint32_t one_dtb_len = 0;
	uint32_t dtb_len = 0;
	uint32_t entry_index;
	uint32_t sdt_no;
	uint32_t addr_offset;
	uint32_t max_size;
	uint32_t rc;

	ZXDH_COMM_CHECK_POINT(delete_entries);

	p_data_buff = rte_calloc(NULL, 1, ZXDH_DTB_TABLE_DATA_BUFF_SIZE, 0);
	ZXDH_COMM_CHECK_POINT(p_data_buff);

	dtb_one_entry.cmd = entry_cmd;
	dtb_one_entry.data = entry_data;

	max_size = (ZXDH_DTB_TABLE_DATA_BUFF_SIZE / 16) - 1;

	for (entry_index = 0; entry_index < entrynum; entry_index++) {
		pentry = delete_entries + entry_index;

		sdt_no = pentry->sdt_no;
		PMD_DRV_LOG(DEBUG, "sdt_no:%u", sdt_no);
		zxdh_np_sdt_tbl_data_get(dev_id, sdt_no, &sdt_tbl);
		tbl_type = zxdh_np_sdt_tbl_type_get(dev_id, sdt_no);
		switch (tbl_type) {
		case ZXDH_SDT_TBLT_ERAM:
			rc = zxdh_np_dtb_eram_one_entry(dev_id, sdt_no, ZXDH_DTB_ITEM_DELETE,
				pentry->p_entry_data, &one_dtb_len, &dtb_one_entry);
			break;
		case ZXDH_SDT_TBLT_HASH:
			rc = zxdh_np_dtb_hash_one_entry(dev_id, sdt_no,
				ZXDH_DTB_ITEM_DELETE, pentry->p_entry_data,
				&one_dtb_len, &dtb_one_entry);
			if (rc == ZXDH_HASH_RC_DEL_SRHFAIL)
				continue;
			break;
		case ZXDH_SDT_TBLT_ETCAM:
			rc = zxdh_np_dtb_acl_one_entry(dev_id, sdt_no,
				ZXDH_DTB_ITEM_DELETE, pentry->p_entry_data,
				&dtb_len, p_data_buff);
			continue;
		default:
			PMD_DRV_LOG(ERR, "SDT table_type[ %u ] is invalid!", tbl_type);
			rte_free(p_data_buff);
			return 1;
		}

		addr_offset = dtb_len * ZXDH_DTB_LEN_POS_SETP;
		dtb_len += one_dtb_len;
		if (dtb_len > max_size) {
			rte_free(p_data_buff);
			PMD_DRV_LOG(ERR, "error dtb_len>%u!", max_size);
			return ZXDH_RC_DTB_DOWN_LEN_INVALID;
		}

		rc = zxdh_np_dtb_data_write(p_data_buff, addr_offset, &dtb_one_entry);
		memset(entry_cmd, 0x0, sizeof(entry_cmd));
		memset(entry_data, 0x0, sizeof(entry_data));
	}

	if (dtb_len == 0) {
		rte_free(p_data_buff);
		return ZXDH_RC_DTB_DOWN_LEN_INVALID;
	}

	rc = zxdh_np_dtb_write_down_table_data(dev_id,
				queue_id,
				dtb_len * 16,
				p_data_buff,
				&element_id);
	rte_free(p_data_buff);

	rc = zxdh_np_dtb_tab_down_success_status_check(dev_id, queue_id, element_id);

	return rc;
}

static void
zxdh_np_eram_index_cal(uint32_t eram_mode, uint32_t index,
		uint32_t *p_row_index, uint32_t *p_col_index)
{
	uint32_t row_index = 0;
	uint32_t col_index = 0;

	switch (eram_mode) {
	case ZXDH_ERAM128_TBL_128b:
		row_index = index;
		break;
	case ZXDH_ERAM128_TBL_64b:
		row_index = (index >> 1);
		col_index = index & 0x1;
		break;
	case ZXDH_ERAM128_TBL_1b:
		row_index = (index >> 7);
		col_index = index & 0x7F;
		break;
	default:
		break;
	}
	*p_row_index = row_index;
	*p_col_index = col_index;
}

static void
zxdh_np_stat_cfg_soft_get(uint32_t dev_id,
				ZXDH_PPU_STAT_CFG_T *p_stat_cfg)
{
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_stat_cfg);

	p_stat_cfg->ddr_base_addr = g_ppu_stat_cfg[ZXDH_DEV_SLOT_ID(dev_id)].ddr_base_addr;
	p_stat_cfg->eram_baddr = g_ppu_stat_cfg[ZXDH_DEV_SLOT_ID(dev_id)].eram_baddr;
	p_stat_cfg->eram_depth = g_ppu_stat_cfg[ZXDH_DEV_SLOT_ID(dev_id)].eram_depth;
	p_stat_cfg->ppu_addr_offset = g_ppu_stat_cfg[ZXDH_DEV_SLOT_ID(dev_id)].ppu_addr_offset;
}

static uint32_t
zxdh_np_dtb_tab_up_info_set(uint32_t dev_id,
			uint32_t queue_id,
			uint32_t item_index,
			uint32_t int_flag,
			uint32_t data_len,
			uint32_t desc_len,
			uint32_t *p_desc_data)
{
	ZXDH_DTB_QUEUE_ITEM_INFO_T item_info = {0};
	uint32_t queue_en = 0;
	uint32_t rc;
	ZXDH_SPINLOCK_T *p_spinlock = NULL;

	zxdh_np_dev_dtb_opr_spinlock_get(dev_id, ZXDH_DEV_SPINLOCK_T_DTB, queue_id, &p_spinlock);
	rte_spinlock_lock(&p_spinlock->spinlock);

	zxdh_np_dtb_queue_enable_get(dev_id, queue_id, &queue_en);
	if (!queue_en) {
		PMD_DRV_LOG(ERR, "the queue %u is not enable!", queue_id);
		rte_spinlock_unlock(&p_spinlock->spinlock);
		return ZXDH_RC_DTB_QUEUE_NOT_ENABLE;
	}

	if (ZXDH_DTB_QUEUE_INIT_FLAG_GET(dev_id, queue_id) == 0) {
		PMD_DRV_LOG(ERR, "dtb queue %u is not init", queue_id);
		rte_spinlock_unlock(&p_spinlock->spinlock);
		return ZXDH_RC_DTB_QUEUE_IS_NOT_INIT;
	}

	if (desc_len % 4 != 0) {
		rte_spinlock_unlock(&p_spinlock->spinlock);
		return ZXDH_RC_DTB_PARA_INVALID;
	}

	zxdh_np_dtb_item_buff_wr(dev_id, queue_id, ZXDH_DTB_DIR_UP_TYPE,
		item_index, 0, desc_len, p_desc_data);

	ZXDH_DTB_TAB_UP_DATA_LEN_GET(dev_id, queue_id, item_index) = data_len;

	item_info.cmd_vld = 1;
	item_info.cmd_type = ZXDH_DTB_DIR_UP_TYPE;
	item_info.int_en = int_flag;
	item_info.data_len = desc_len / 4;
	item_info.data_hddr =
	((ZXDH_DTB_TAB_UP_PHY_ADDR_GET(dev_id, queue_id, item_index) >> 4) >> 32) & 0xffffffff;
	item_info.data_laddr =
		(ZXDH_DTB_TAB_UP_PHY_ADDR_GET(dev_id, queue_id, item_index) >> 4) & 0xffffffff;
	zxdh_dtb_info_print(dev_id, queue_id, item_index,  &item_info);

	rc = zxdh_np_dtb_queue_item_info_set(dev_id, queue_id, &item_info);

	rte_spinlock_unlock(&p_spinlock->spinlock);

	return rc;
}

static uint32_t
zxdh_np_dtb_tab_up_data_get(uint32_t dev_id,
					uint32_t queue_id,
					uint32_t item_index,
					uint32_t data_len,
					uint32_t *p_data)
{
	uint32_t rc = 0;

	if (ZXDH_DTB_QUEUE_INIT_FLAG_GET(dev_id, queue_id) == 0) {
		PMD_DRV_LOG(ERR, "dtb queue %u is not init.", queue_id);
		return ZXDH_RC_DTB_QUEUE_IS_NOT_INIT;
	}

	rc = zxdh_np_dtb_item_buff_rd(dev_id,
					queue_id,
					ZXDH_DTB_DIR_UP_TYPE,
					item_index,
					0,
					data_len,
					p_data);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_item_buff_rd");

	rc = zxdh_np_dtb_item_ack_wr(dev_id, queue_id, ZXDH_DTB_DIR_UP_TYPE,
		item_index, 0, ZXDH_DTB_TAB_ACK_UNUSED_MASK);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_item_ack_wr");

	return rc;
}

static uint32_t
zxdh_np_dtb_tab_up_item_addr_get(uint32_t dev_id,
					uint32_t queue_id,
					uint32_t item_index,
					uint32_t *p_phy_haddr,
					uint32_t *p_phy_laddr)
{
	uint32_t rc = 0;
	uint64_t addr;

	if (ZXDH_DTB_QUEUE_INIT_FLAG_GET(dev_id, queue_id) == 0) {
		PMD_DRV_LOG(ERR, "dtb queue %u is not init.", queue_id);
		return ZXDH_RC_DTB_QUEUE_IS_NOT_INIT;
	}

	if (ZXDH_DTB_TAB_UP_USER_PHY_ADDR_FLAG_GET(dev_id, queue_id, item_index) ==
		ZXDH_DTB_TAB_UP_USER_ADDR_TYPE)
		addr = ZXDH_DTB_TAB_UP_USER_PHY_ADDR_GET(dev_id, queue_id, item_index);
	else
		addr = ZXDH_DTB_TAB_UP_PHY_ADDR_GET(dev_id, queue_id, item_index)
				+ ZXDH_DTB_ITEM_ACK_SIZE;

	*p_phy_haddr = (addr >> 32) & 0xffffffff;
	*p_phy_laddr = addr & 0xffffffff;

	return rc;
}

static void
zxdh_np_dtb_tab_up_item_offset_addr_get(uint32_t dev_id,
			uint32_t queue_id,
			uint32_t item_index,
			uint32_t addr_offset,
			uint32_t *p_phy_haddr,
			uint32_t *p_phy_laddr)
{
	uint64_t addr = 0;

	if (ZXDH_DTB_TAB_UP_USER_PHY_ADDR_FLAG_GET(dev_id, queue_id, item_index) ==
	ZXDH_DTB_TAB_UP_USER_ADDR_TYPE)
		addr = ZXDH_DTB_TAB_UP_USER_PHY_ADDR_GET(dev_id, queue_id, item_index);
	else
		addr = ZXDH_DTB_TAB_UP_PHY_ADDR_GET(dev_id, queue_id, item_index) +
			ZXDH_DTB_ITEM_ACK_SIZE;

	addr = addr + addr_offset;

	*p_phy_haddr = (addr >> 32) & 0xffffffff;
	*p_phy_laddr = addr & 0xffffffff;
}

static uint32_t
zxdh_np_dtb_dump_table_element_addr_get(uint32_t dev_id,
						uint32_t queue_id,
						uint32_t element_id,
						uint32_t *p_element_start_addr_h,
						uint32_t *p_element_start_addr_l,
						uint32_t *p_element_dump_addr_h,
						uint32_t *p_element_dump_addr_l,
						uint32_t *p_element_table_info_addr_h,
						uint32_t *p_element_table_info_addr_l)
{
	uint32_t rc = ZXDH_OK;
	uint32_t addr_h = 0;
	uint32_t addr_l = 0;

	addr_h = ((ZXDH_DTB_TAB_UP_PHY_ADDR_GET(dev_id, queue_id, element_id)) >> 32) & 0xffffffff;
	addr_l = (ZXDH_DTB_TAB_UP_PHY_ADDR_GET(dev_id, queue_id, element_id)) & 0xffffffff;

	*p_element_start_addr_h = addr_h;
	*p_element_start_addr_l = addr_l;

	addr_h = ((ZXDH_DTB_TAB_UP_PHY_ADDR_GET(dev_id, queue_id, element_id) +
		ZXDH_DTB_ITEM_ACK_SIZE) >> 32) & 0xffffffff;
	addr_l = (ZXDH_DTB_TAB_UP_PHY_ADDR_GET(dev_id, queue_id, element_id) +
		ZXDH_DTB_ITEM_ACK_SIZE) & 0xffffffff;

	*p_element_dump_addr_h = addr_h;
	*p_element_dump_addr_l = addr_l;

	rc = zxdh_np_dtb_tab_up_item_addr_get(dev_id, queue_id, element_id,
		p_element_table_info_addr_h, p_element_table_info_addr_l);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_tab_up_item_addr_get");

	return rc;
}

static uint32_t
zxdh_np_dtb_dump_table_element_info_prt(uint32_t dev_id,
					uint32_t queue_id,
					uint32_t element_id)
{
	uint32_t rc = 0;

	uint32_t element_start_addr_h = 0;
	uint32_t element_start_addr_l = 0;
	uint32_t element_dump_addr_h = 0;
	uint32_t element_dump_addr_l = 0;
	uint32_t element_table_info_addr_h = 0;
	uint32_t element_table_info_addr_l = 0;

	zxdh_np_dtb_dump_table_element_addr_get(dev_id,
						 queue_id,
						 element_id,
						 &element_start_addr_h,
						 &element_start_addr_l,
						 &element_dump_addr_h,
						 &element_dump_addr_l,
						 &element_table_info_addr_h,
						 &element_table_info_addr_l);
	PMD_DRV_LOG(DEBUG, "queue_id %u.", queue_id);
	PMD_DRV_LOG(DEBUG, "element_id %u.", element_id);
	PMD_DRV_LOG(DEBUG, "element_start_addr_h 0x%x.", element_start_addr_h);
	PMD_DRV_LOG(DEBUG, "element_start_addr_l 0x%x.", element_start_addr_l);
	PMD_DRV_LOG(DEBUG, "element_dump_addr_h 0x%x.", element_dump_addr_h);
	PMD_DRV_LOG(DEBUG, "element_dump_addr_l 0x%x.", element_dump_addr_l);
	PMD_DRV_LOG(DEBUG, "element_table_info_addr_h 0x%x.", element_table_info_addr_h);
	PMD_DRV_LOG(DEBUG, "element_table_info_addr_l 0x%x.", element_table_info_addr_l);

	rc = zxdh_np_dtb_item_ack_prt(dev_id, queue_id, ZXDH_DTB_DIR_UP_TYPE, element_id);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_item_buff_prt");

	rc = zxdh_np_dtb_item_buff_prt(dev_id, queue_id, ZXDH_DTB_DIR_UP_TYPE, element_id, 32);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_item_buff_prt");

	return rc;
}

static uint32_t
zxdh_np_dtb_tab_up_success_status_check(uint32_t dev_id,
					uint32_t queue_id,
					uint32_t element_id)
{
	uint32_t rc = 0;
	uint32_t rd_cnt = 0;
	uint32_t ack_value = 0;
	uint32_t success_flag = 0;

	while (!success_flag) {
		rc = zxdh_np_dtb_item_ack_rd(dev_id, queue_id, ZXDH_DTB_DIR_UP_TYPE,
			element_id, 0, &ack_value);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_item_ack_rd");
		PMD_DRV_LOG(DEBUG, "zxdh_np_dtb_item_ack_rd ack_value:0x%08x", ack_value);

		if ((((ack_value >> 8) & 0xffffff) == ZXDH_DTB_TAB_UP_ACK_VLD_MASK) &&
			 ((ack_value & 0xff) == ZXDH_DTB_TAB_ACK_SUCCESS_MASK)) {
			success_flag = 1;
			break;
		}

		if (rd_cnt > ZXDH_DTB_DUMP_OVER_TIME) {
			PMD_DRV_LOG(ERR, "dump queue %u item %u overtime!", queue_id, element_id);

			rc = zxdh_np_dtb_dump_table_element_info_prt(dev_id, queue_id, element_id);
			ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_dump_table_element_info_prt");

			rc = zxdh_np_dtb_item_ack_wr(dev_id, queue_id, ZXDH_DTB_DIR_UP_TYPE,
				element_id, 0, ZXDH_DTB_TAB_ACK_UNUSED_MASK);
			ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_item_ack_wr");

			return ZXDH_ERR;
		}

		rd_cnt++;
		rte_delay_us(ZXDH_DTB_DELAY_TIME);
	}

	return rc;
}

static uint32_t
zxdh_np_dtb_write_dump_desc_info(uint32_t dev_id,
		uint32_t queue_id,
		uint32_t queue_element_id,
		uint32_t *p_dump_info,
		uint32_t data_len,
		uint32_t desc_len,
		uint32_t *p_dump_data)
{
	uint32_t dtb_interrupt_status = 0;
	uint32_t rc;

	ZXDH_COMM_CHECK_POINT(p_dump_data);
	rc = zxdh_np_dtb_tab_up_info_set(dev_id,
				queue_id,
				queue_element_id,
				dtb_interrupt_status,
				data_len,
				desc_len,
				p_dump_info);
	if (rc != 0) {
		PMD_DRV_LOG(ERR, "queue %u element %u dump info set failed!",
			queue_id, queue_element_id);
		zxdh_np_dtb_item_ack_wr(dev_id, queue_id, ZXDH_DTB_DIR_UP_TYPE,
			queue_element_id, 0, ZXDH_DTB_TAB_ACK_UNUSED_MASK);
	}

	rc = zxdh_np_dtb_tab_up_success_status_check(dev_id,
				queue_id, queue_element_id);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_tab_up_success_status_check");

	rc = zxdh_np_dtb_tab_up_data_get(dev_id, queue_id, queue_element_id,
			data_len, p_dump_data);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_tab_up_data_get");

	PMD_DRV_LOG(DEBUG, "queue %u element %u dump done.", queue_id, queue_element_id);

	return rc;
}

static uint32_t
zxdh_np_dtb_tab_up_free_item_get(uint32_t dev_id,
					uint32_t queue_id,
					uint32_t *p_item_index)
{
	uint32_t ack_vale = 0;
	uint32_t item_index = 0;
	uint32_t unused_item_num = 0;
	uint32_t i;
	ZXDH_SPINLOCK_T *p_spinlock = NULL;

	zxdh_np_dev_dtb_opr_spinlock_get(dev_id, ZXDH_DEV_SPINLOCK_T_DTB, queue_id, &p_spinlock);
	rte_spinlock_lock(&p_spinlock->spinlock);

	if (ZXDH_DTB_QUEUE_INIT_FLAG_GET(dev_id, queue_id) == 0) {
		PMD_DRV_LOG(ERR, "dtb queue %u is not init", queue_id);
		rte_spinlock_unlock(&p_spinlock->spinlock);
		return ZXDH_RC_DTB_QUEUE_IS_NOT_INIT;
	}

	zxdh_np_dtb_queue_unused_item_num_get(dev_id, queue_id, &unused_item_num);

	if (unused_item_num == 0) {
		rte_spinlock_unlock(&p_spinlock->spinlock);
		return ZXDH_RC_DTB_QUEUE_ITEM_HW_EMPTY;
	}

	for (i = 0; i < ZXDH_DTB_QUEUE_ITEM_NUM_MAX; i++) {
		item_index = ZXDH_DTB_TAB_UP_WR_INDEX_GET(dev_id, queue_id) %
			ZXDH_DTB_QUEUE_ITEM_NUM_MAX;

		zxdh_np_dtb_item_ack_rd(dev_id, queue_id, ZXDH_DTB_DIR_UP_TYPE, item_index,
			0, &ack_vale);

		ZXDH_DTB_TAB_UP_WR_INDEX_GET(dev_id, queue_id)++;

		if ((ack_vale >> 8) == ZXDH_DTB_TAB_ACK_UNUSED_MASK)
			break;
	}

	if (i == ZXDH_DTB_QUEUE_ITEM_NUM_MAX) {
		rte_spinlock_unlock(&p_spinlock->spinlock);
		return ZXDH_RC_DTB_QUEUE_ITEM_SW_EMPTY;
	}

	zxdh_np_dtb_item_ack_wr(dev_id, queue_id, ZXDH_DTB_DIR_UP_TYPE, item_index,
		0, ZXDH_DTB_TAB_ACK_IS_USING_MASK);

	*p_item_index = item_index;

	rte_spinlock_unlock(&p_spinlock->spinlock);

	return 0;
}

static uint32_t
zxdh_np_dtb_tab_up_item_user_addr_set(uint32_t dev_id,
					uint32_t queue_id,
					uint32_t item_index,
					uint64_t phy_addr,
					uint64_t vir_addr)
{
	ZXDH_DTB_MGR_T *p_dtb_mgr = NULL;

	p_dtb_mgr = zxdh_np_dtb_mgr_get(dev_id);
	if (p_dtb_mgr == NULL) {
		PMD_DRV_LOG(ERR, "DTB Manager is not exist!");
		return ZXDH_RC_DTB_MGR_NOT_EXIST;
	}

	if (ZXDH_DTB_QUEUE_INIT_FLAG_GET(dev_id, queue_id) == 0) {
		PMD_DRV_LOG(ERR, "dtb queue %u is not init.", queue_id);
		return ZXDH_RC_DTB_QUEUE_IS_NOT_INIT;
	}

	p_dtb_mgr->queue_info[queue_id].tab_up.user_addr[item_index].phy_addr = phy_addr;
	p_dtb_mgr->queue_info[queue_id].tab_up.user_addr[item_index].vir_addr = vir_addr;
	p_dtb_mgr->queue_info[queue_id].tab_up.user_addr[item_index].user_flag =
		ZXDH_DTB_TAB_UP_USER_ADDR_TYPE;

	return ZXDH_OK;
}

static uint32_t
zxdh_np_dtb_dump_sdt_addr_get(uint32_t dev_id,
							uint32_t queue_id,
							uint32_t sdt_no,
							uint64_t *phy_addr,
							uint64_t *vir_addr,
							uint32_t *size)
{
	uint32_t rc = ZXDH_OK;

	ZXDH_DTB_ADDR_INFO_T dtb_dump_addr_info = {0};
	ZXDH_RB_CFG *p_dtb_dump_addr_rb = NULL;

	dtb_dump_addr_info.sdt_no = sdt_no;
	p_dtb_dump_addr_rb = zxdh_np_dtb_dump_addr_rb_get(dev_id, queue_id);
	rc = zxdh_np_se_apt_rb_search(p_dtb_dump_addr_rb, &dtb_dump_addr_info,
		sizeof(ZXDH_DTB_ADDR_INFO_T));
	if (rc == ZXDH_OK) {
		PMD_DRV_LOG(INFO, "search sdt_no %u success.", sdt_no);
	} else {
		PMD_DRV_LOG(ERR, "search sdt_no %u fail.", sdt_no);
		return rc;
	}

	*phy_addr = dtb_dump_addr_info.phy_addr;
	*vir_addr = dtb_dump_addr_info.vir_addr;
	*size = dtb_dump_addr_info.size;

	return rc;
}

static uint32_t
zxdh_np_dtb_dump_addr_set(uint32_t dev_id,
							uint32_t queue_id,
							uint32_t sdt_no,
							uint32_t *element_id)
{
	uint32_t rc = ZXDH_OK;
	uint32_t dump_element_id = 0;
	uint64_t phy_addr = 0;
	uint64_t vir_addr = 0;
	uint32_t size	 = 0;

	rc = zxdh_np_dtb_dump_sdt_addr_get(dev_id,
									queue_id,
									sdt_no,
									&phy_addr,
									&vir_addr,
									&size);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_dump_sdt_addr_get");
	memset((uint8_t *)vir_addr, 0, size);

	rc = zxdh_np_dtb_tab_up_free_item_get(dev_id, queue_id, &dump_element_id);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_tab_up_free_item_get");

	rc = zxdh_np_dtb_tab_up_item_user_addr_set(dev_id,
										   queue_id,
										   dump_element_id,
										   phy_addr,
										   vir_addr);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_tab_up_item_addr_set");

	*element_id = dump_element_id;

	return rc;
}

static uint32_t
zxdh_np_dtb_se_smmu0_dma_dump(uint32_t dev_id,
		uint32_t queue_id,
		uint32_t base_addr,
		uint32_t depth,
		uint32_t *p_data,
		uint32_t *element_id)
{
	uint8_t form_buff[ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8] = {0};
	uint32_t dump_dst_phy_haddr = 0;
	uint32_t dump_dst_phy_laddr = 0;
	uint32_t queue_item_index = 0;
	uint32_t data_len;
	uint32_t desc_len;
	uint32_t rc;

	rc = zxdh_np_dtb_tab_up_free_item_get(dev_id, queue_id, &queue_item_index);
	if (rc != 0) {
		PMD_DRV_LOG(ERR, "dpp_dtb_tab_up_free_item_get failed = %u!", base_addr);
		return ZXDH_RC_DTB_QUEUE_ITEM_SW_EMPTY;
	}

	*element_id = queue_item_index;

	rc = zxdh_np_dtb_tab_up_item_addr_get(dev_id, queue_id, queue_item_index,
		&dump_dst_phy_haddr, &dump_dst_phy_laddr);
	ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "dpp_dtb_tab_up_item_addr_get");

	rc = zxdh_np_dtb_smmu0_dump_info_write(dev_id,
									   base_addr,
									   depth,
									   dump_dst_phy_haddr,
									   dump_dst_phy_laddr,
									   (uint32_t *)form_buff);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_smmu0_dump_info_write");

	data_len = depth * 128 / 32;
	desc_len = ZXDH_DTB_LEN_POS_SETP / 4;

	rc = zxdh_np_dtb_write_dump_desc_info(dev_id, queue_id, queue_item_index,
		(uint32_t *)form_buff, data_len, desc_len, p_data);
	ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "dpp_dtb_write_dump_desc_info");

	return rc;
}

static uint32_t
zxdh_np_dtb_eram_data_get(uint32_t dev_id, uint32_t queue_id, uint32_t sdt_no,
		ZXDH_DTB_ERAM_ENTRY_INFO_T *p_dump_eram_entry)
{
	uint32_t index = p_dump_eram_entry->index;
	uint32_t *p_data = p_dump_eram_entry->p_data;
	ZXDH_SDT_TBL_ERAM_T sdt_eram_info = {0};
	uint32_t temp_data[4] = {0};
	uint32_t row_index = 0;
	uint32_t col_index = 0;
	uint32_t rd_mode;
	uint32_t rc;
	uint32_t eram_dump_base_addr = 0;
	uint32_t eram_base_addr = 0;
	uint32_t element_id = 0;

	PMD_DRV_LOG(DEBUG, "sdt_no:%u", sdt_no);
	rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_eram_info);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "dpp_soft_sdt_tbl_get");
	eram_base_addr = sdt_eram_info.eram_base_addr;
	rd_mode = sdt_eram_info.eram_mode;

	zxdh_np_eram_index_cal(rd_mode, index, &row_index, &col_index);

	eram_dump_base_addr = eram_base_addr + row_index;

	rc = zxdh_np_dtb_se_smmu0_dma_dump(dev_id,
								queue_id,
								eram_dump_base_addr,
								1,
								temp_data,
								&element_id);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_se_smmu0_dma_dump");

	switch (rd_mode) {
	case ZXDH_ERAM128_TBL_128b:
		memcpy(p_data, temp_data, (128 / 8));
		break;
	case ZXDH_ERAM128_TBL_64b:
		memcpy(p_data, temp_data + ((1 - col_index) << 1), (64 / 8));
		break;
	case ZXDH_ERAM128_TBL_1b:
		ZXDH_COMM_UINT32_GET_BITS(p_data[0], *(temp_data +
			(3 - col_index / 32)), (col_index % 32), 1);
		break;
	default:
		break;
	}

	PMD_DRV_LOG(DEBUG, "[eram_dump]std no:0x%x, index:0x%x, base addr:0x%x",
				sdt_no, p_dump_eram_entry->index, eram_dump_base_addr);
	if (rd_mode == ZXDH_ERAM128_TBL_128b)
		PMD_DRV_LOG(DEBUG, "value[0x%08x 0x%08x 0x%08x 0x%08x]",
		p_dump_eram_entry->p_data[0], p_dump_eram_entry->p_data[1],
		p_dump_eram_entry->p_data[2], p_dump_eram_entry->p_data[3]);
	else if (rd_mode == ZXDH_ERAM128_TBL_64b)
		PMD_DRV_LOG(DEBUG, "value[0x%08x 0x%08x]",
		p_dump_eram_entry->p_data[0], p_dump_eram_entry->p_data[1]);

	return rc;
}

static uint32_t
zxdh_np_dtb_se_zcam_dma_dump(uint32_t dev_id,
							uint32_t queue_id,
							uint32_t addr,
							uint32_t tb_width,
							uint32_t depth,
							uint32_t *p_data,
							uint32_t *element_id)
{
	uint32_t rc = ZXDH_OK;
	uint32_t dump_dst_phy_haddr = 0;
	uint32_t dump_dst_phy_laddr = 0;
	uint32_t queue_item_index = 0;
	uint32_t data_len = 0;
	uint32_t desc_len = 0;
	uint32_t tb_width_len = 0;
	uint8_t form_buff[ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8] = {0};

	rc = zxdh_np_dtb_tab_up_free_item_get(dev_id, queue_id, &queue_item_index);
	if (rc != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "zxdh_np_dtb_tab_up_free_item_get failed!");
		return ZXDH_RC_DTB_QUEUE_ITEM_SW_EMPTY;
	}

	PMD_DRV_LOG(DEBUG, "table up item queue_element_id is: %u.",
		queue_item_index);

	*element_id = queue_item_index;

	rc = zxdh_np_dtb_tab_up_item_addr_get(dev_id, queue_id, queue_item_index,
		&dump_dst_phy_haddr, &dump_dst_phy_laddr);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_tab_up_item_addr_get");

	rc = zxdh_np_dtb_zcam_dump_info_write(dev_id,
									  addr,
									  tb_width,
									  depth,
									  dump_dst_phy_haddr,
									  dump_dst_phy_laddr,
									  (uint32_t *)form_buff);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_zcam_dump_info_write");

	tb_width_len = ZXDH_DTB_LEN_POS_SETP << tb_width;
	data_len = depth * tb_width_len / 4;
	desc_len = ZXDH_DTB_LEN_POS_SETP / 4;

	rc = zxdh_np_dtb_write_dump_desc_info(dev_id, queue_id, queue_item_index,
		(uint32_t *)form_buff, data_len, desc_len, p_data);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_write_dump_desc_info");

	return rc;
}

static uint32_t
zxdh_np_dtb_hash_data_parse(uint32_t item_type,
						uint32_t key_by_size,
						ZXDH_HASH_ENTRY *p_entry,
						uint8_t  *p_item_data,
						uint32_t  *p_data_offset)
{
	uint32_t data_offset = 0;
	uint8_t temp_key_valid = 0;
	uint8_t temp_key_type = 0;
	uint32_t temp_entry_size = 0;
	uint8_t srh_key_type = 0;
	uint32_t srh_entry_size = 0;
	uint32_t rst_by_size = 0;
	uint8_t srh_succ = 0;
	uint32_t item_width = ZXDH_SE_ITEM_WIDTH_MAX;
	uint8_t *p_srh_key = NULL;
	uint8_t *p_temp_key = NULL;

	if (item_type == ZXDH_ITEM_DDR_256)
		item_width = item_width / 2;

	p_temp_key = p_item_data;
	p_srh_key = p_entry->p_key;
	srh_key_type = ZXDH_GET_HASH_KEY_TYPE(p_srh_key);
	srh_entry_size = ZXDH_GET_HASH_ENTRY_SIZE(srh_key_type);

	while (data_offset < item_width) {
		temp_key_valid = ZXDH_GET_HASH_KEY_VALID(p_temp_key);
		temp_key_type = ZXDH_GET_HASH_KEY_TYPE(p_temp_key);

		if (temp_key_valid && srh_key_type == temp_key_type) {
			if (memcmp(p_srh_key, p_temp_key, key_by_size) == 0) {
				PMD_DRV_LOG(DEBUG, "Hash search hardware successfully.");
				srh_succ = 1;
				break;
			}

			data_offset += srh_entry_size;
		} else if (temp_key_valid && (srh_key_type != temp_key_type)) {
			temp_entry_size = ZXDH_GET_HASH_ENTRY_SIZE(temp_key_type);
			data_offset += temp_entry_size;
		} else {
			data_offset += ZXDH_HASH_ENTRY_POS_STEP;
		}

		p_temp_key = p_item_data;
		p_temp_key += data_offset;
	}

	if (!srh_succ) {
		PMD_DRV_LOG(DEBUG, "Hash search hardware fail.");
		return ZXDH_HASH_RC_MATCH_ITEM_FAIL;
	}

	rst_by_size = srh_entry_size - key_by_size;
	memcpy(p_entry->p_rst, p_temp_key + key_by_size,
		(rst_by_size > ZXDH_HASH_RST_MAX) ? ZXDH_HASH_RST_MAX : rst_by_size);
	*p_data_offset = data_offset;

	return ZXDH_OK;
}

static uint32_t
zxdh_np_dtb_hash_zcam_get_hardware(uint32_t dev_id,
						uint32_t queue_id,
						HASH_ENTRY_CFG  *p_hash_entry_cfg,
						ZXDH_HASH_ENTRY  *p_hash_entry,
						uint8_t *p_srh_succ)
{
	uint32_t rc = ZXDH_OK;
	ZXDH_HASH_CFG *p_hash_cfg = NULL;
	ZXDH_SE_ZCELL_CFG *p_zcell = NULL;
	ZXDH_SE_ZBLK_CFG *p_zblk = NULL;
	uint32_t zblk_idx = 0;
	uint32_t pre_zblk_idx = 0xFFFFFFFF;
	uint16_t crc16_value = 0;
	uint32_t zcell_id = 0;
	uint32_t item_idx = 0;
	uint32_t element_id = 0;
	uint32_t byte_offset = 0;
	uint32_t addr = 0;
	uint32_t i	= 0;
	uint8_t srh_succ	 = 0;
	uint8_t temp_key[ZXDH_HASH_KEY_MAX] = {0};
	uint8_t  rd_buff[ZXDH_SE_ITEM_WIDTH_MAX]   = {0};
	ZXDH_D_NODE *p_zblk_dn = NULL;
	ZXDH_D_NODE *p_zcell_dn = NULL;
	ZXDH_SE_CFG *p_se_cfg = NULL;

	p_hash_cfg = p_hash_entry_cfg->p_hash_cfg;
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_hash_cfg);

	p_se_cfg = p_hash_entry_cfg->p_se_cfg;
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_se_cfg);

	zxdh_np_hash_set_crc_key(p_hash_entry_cfg, p_hash_entry, temp_key);

	p_zcell_dn = p_hash_cfg->hash_shareram.zcell_free_list.p_next;
	while (p_zcell_dn) {
		p_zcell = (ZXDH_SE_ZCELL_CFG *)p_zcell_dn->data;
		zblk_idx = GET_ZBLK_IDX(p_zcell->zcell_idx);
		p_zblk = &p_se_cfg->zblk_info[zblk_idx];

		if (zblk_idx != pre_zblk_idx) {
			pre_zblk_idx = zblk_idx;
			crc16_value = p_hash_cfg->p_hash16_fun(temp_key,
				p_hash_entry_cfg->key_by_size, p_zblk->hash_arg);
		}

		zcell_id = GET_ZCELL_IDX(p_zcell->zcell_idx);
		item_idx = GET_ZCELL_CRC_VAL(zcell_id, crc16_value);
		addr = ZXDH_ZBLK_ITEM_ADDR_CALC(p_zcell->zcell_idx, item_idx);
		rc =  zxdh_np_dtb_se_zcam_dma_dump(dev_id,
									   queue_id,
									   addr,
									   ZXDH_DTB_DUMP_ZCAM_512b,
									   1,
									   (uint32_t *)rd_buff,
									   &element_id);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_se_zcam_dma_dump");
		zxdh_np_comm_swap(rd_buff, sizeof(rd_buff));

		rc = zxdh_np_dtb_hash_data_parse(ZXDH_ITEM_RAM, p_hash_entry_cfg->key_by_size,
			p_hash_entry, rd_buff, &byte_offset);
		if (rc == ZXDH_OK) {
			PMD_DRV_LOG(DEBUG, "Hash search hardware succ in zcell.");
			srh_succ = 1;
			p_hash_cfg->hash_stat.search_ok++;
			break;
		}

		p_zcell_dn = p_zcell_dn->next;
	}

	if (srh_succ == 0) {
		p_zblk_dn = p_hash_cfg->hash_shareram.zblk_list.p_next;
		while (p_zblk_dn) {
			p_zblk = (ZXDH_SE_ZBLK_CFG *)p_zblk_dn->data;
			zblk_idx = p_zblk->zblk_idx;

			for (i = 0; i < ZXDH_SE_ZREG_NUM; i++) {
				item_idx = i;
				addr = ZXDH_ZBLK_HASH_LIST_REG_ADDR_CALC(zblk_idx, item_idx);
				rc =  zxdh_np_dtb_se_zcam_dma_dump(dev_id,
									   queue_id,
									   addr,
									   ZXDH_DTB_DUMP_ZCAM_512b,
									   1,
									   (uint32_t *)rd_buff,
									   &element_id);
				ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_se_zcam_dma_dump");
				zxdh_np_comm_swap(rd_buff, sizeof(rd_buff));

				rc = zxdh_np_dtb_hash_data_parse(ZXDH_ITEM_RAM,
					p_hash_entry_cfg->key_by_size, p_hash_entry,
					rd_buff, &byte_offset);
				if (rc == ZXDH_OK) {
					PMD_DRV_LOG(DEBUG, "Hash search hardware succ in zreg.");
					srh_succ = 1;
					p_hash_cfg->hash_stat.search_ok++;
					break;
				}
			}
			p_zblk_dn = p_zblk_dn->next;
		}
	}

	*p_srh_succ = srh_succ;

	return rc;
}

static uint32_t
zxdh_np_dtb_hash_software_item_check(ZXDH_HASH_ENTRY *p_entry,
						uint32_t key_by_size,
						uint32_t rst_by_size,
						ZXDH_SE_ITEM_CFG *p_item_info)
{
	uint8_t srh_succ = 0;
	uint8_t temp_key_type = 0;
	uint8_t srh_key_type = 0;
	uint32_t dev_id = 0;
	ZXDH_D_NODE *p_entry_dn = NULL;
	ZXDH_HASH_RBKEY_INFO *p_rbkey = NULL;

	srh_key_type = ZXDH_GET_HASH_KEY_TYPE(p_entry->p_key);
	p_entry_dn = p_item_info->item_list.p_next;
	while (p_entry_dn) {
		p_rbkey = (ZXDH_HASH_RBKEY_INFO *)p_entry_dn->data;
		ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_rbkey);

		RTE_ASSERT(p_rbkey->p_item_info == p_item_info);

		temp_key_type = ZXDH_GET_HASH_KEY_TYPE(p_rbkey->key);

		if (ZXDH_GET_HASH_KEY_VALID(p_rbkey->key) && srh_key_type == temp_key_type) {
			if (memcmp(p_entry->p_key, p_rbkey->key, key_by_size) == 0) {
				srh_succ = 1;
				break;
			}
		}

		p_entry_dn = p_entry_dn->next;
	}

	if (p_rbkey == NULL)
		return ZXDH_PAR_CHK_POINT_NULL;

	if (!srh_succ) {
		PMD_DRV_LOG(DEBUG, "hash search failed!");
		return ZXDH_HASH_RC_MATCH_ITEM_FAIL;
	}

	memcpy(p_entry->p_rst, p_rbkey->rst,
		(rst_by_size > ZXDH_HASH_RST_MAX) ? ZXDH_HASH_RST_MAX : rst_by_size);

	return ZXDH_OK;
}

static uint32_t
zxdh_np_dtb_hash_get_software(uint32_t dev_id,
							HASH_ENTRY_CFG  *p_hash_entry_cfg,
							ZXDH_HASH_ENTRY  *p_hash_entry,
							uint8_t *p_srh_succ)
{
	uint32_t rc = ZXDH_OK;
	ZXDH_HASH_RBKEY_INFO srh_rbkey = {0};
	ZXDH_HASH_RBKEY_INFO *p_rbkey = NULL;
	ZXDH_RB_TN *p_rb_tn_rtn = NULL;
	ZXDH_SE_ITEM_CFG *p_item = NULL;
	ZXDH_HASH_CFG *p_hash_cfg = NULL;
	ZXDH_SPINLOCK_T *p_hash_spinlock = NULL;

	memcpy(srh_rbkey.key, p_hash_entry->p_key, p_hash_entry_cfg->key_by_size);

	p_hash_cfg = p_hash_entry_cfg->p_hash_cfg;
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_hash_cfg);

	zxdh_np_dev_hash_opr_spinlock_get(dev_id, p_hash_cfg->fun_id, &p_hash_spinlock);
	rte_spinlock_lock(&p_hash_spinlock->spinlock);

	rc = zxdh_comm_rb_search(&p_hash_cfg->hash_rb, (void *)&srh_rbkey, (void *)(&p_rb_tn_rtn));
	if (rc == ZXDH_RBT_RC_SRHFAIL) {
		PMD_DRV_LOG(DEBUG, "zxdh_comm_rb_search fail.");
		rte_spinlock_unlock(&p_hash_spinlock->spinlock);
		return ZXDH_OK;
	}

	p_rbkey = p_rb_tn_rtn->p_key;
	p_item = p_rbkey->p_item_info;

	rc = zxdh_np_dtb_hash_software_item_check(p_hash_entry,
			p_hash_entry_cfg->key_by_size,
			p_hash_entry_cfg->rst_by_size,
			p_item);
	if (rc == ZXDH_OK) {
		PMD_DRV_LOG(DEBUG, "Hash search software succ.");
		*p_srh_succ = 1;
		p_hash_cfg->hash_stat.search_ok++;
	}

	rte_spinlock_unlock(&p_hash_spinlock->spinlock);
	return rc;
}

static uint32_t
zxdh_np_dtb_hash_zcam_get(uint32_t dev_id,
						uint32_t queue_id,
						HASH_ENTRY_CFG  *p_hash_entry_cfg,
						ZXDH_HASH_ENTRY  *p_hash_entry,
						uint32_t srh_mode,
						uint8_t *p_srh_succ)
{
	uint32_t  rc = ZXDH_OK;

	if (srh_mode == ZXDH_HASH_SRH_MODE_HDW) {
		rc = zxdh_np_dtb_hash_zcam_get_hardware(dev_id, queue_id,
			p_hash_entry_cfg, p_hash_entry, p_srh_succ);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_hash_zcam_get_hardware");
	} else {
		rc = zxdh_np_dtb_hash_get_software(dev_id, p_hash_entry_cfg,
			p_hash_entry, p_srh_succ);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_hash_get_software");
	}

	return rc;
}

static uint32_t
zxdh_np_dtb_hash_data_get(uint32_t dev_id,
						uint32_t queue_id,
						uint32_t sdt_no,
						ZXDH_DTB_HASH_ENTRY_INFO_T *p_dtb_hash_entry,
						uint32_t srh_mode)
{
	uint32_t rc = ZXDH_OK;
	uint8_t srh_succ = 0;
	uint8_t key_valid = 1;
	ZXDH_HASH_CFG *p_hash_cfg = NULL;
	HASH_ENTRY_CFG  hash_entry_cfg = {0};
	ZXDH_HASH_ENTRY hash_entry = {0};
	uint8_t temp_key[ZXDH_HASH_KEY_MAX] = {0};
	uint8_t temp_rst[ZXDH_HASH_RST_MAX] = {0};

	PMD_DRV_LOG(DEBUG, "hash get sdt_no:%u", sdt_no);

	rc = zxdh_np_hash_get_hash_info_from_sdt(dev_id, sdt_no, &hash_entry_cfg);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_hash_get_hash_info_from_sdt");

	p_hash_cfg = hash_entry_cfg.p_hash_cfg;
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_hash_cfg);

	hash_entry.p_key = temp_key;
	hash_entry.p_rst = temp_rst;
	hash_entry.p_key[0] = ZXDH_GET_HASH_KEY_CTRL(key_valid,
							hash_entry_cfg.key_type,
							hash_entry_cfg.table_id);

	memcpy(&hash_entry.p_key[1], p_dtb_hash_entry->p_actu_key,
		hash_entry_cfg.actu_key_size);

	if (!srh_succ) {
		rc  = zxdh_np_dtb_hash_zcam_get(dev_id, queue_id, &hash_entry_cfg,
			&hash_entry, srh_mode, &srh_succ);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_hash_zcam_get");
	}

	if (!srh_succ) {
		p_hash_cfg->hash_stat.search_fail++;
		PMD_DRV_LOG(DEBUG, "Hash search key fail!");
		return ZXDH_HASH_RC_SRH_FAIL;
	}

	memcpy(p_dtb_hash_entry->p_rst, hash_entry.p_rst,
		1 << (hash_entry_cfg.rsp_mode + 2));

	return rc;
}

static void
dtb_etcam_dump_data_len(uint32_t etcam_key_mode,
						uint32_t *p_etcam_dump_len,
						uint32_t *p_etcam_dump_inerval)
{
	uint32_t dump_data_len = 0;
	uint8_t etcam_dump_inerval = 0;

	if (ZXDH_ETCAM_KEY_640b == etcam_key_mode) {
		dump_data_len = 5 * ZXDH_DTB_LEN_POS_SETP;
		etcam_dump_inerval = 0;
	} else if (ZXDH_ETCAM_KEY_320b == etcam_key_mode) {
		dump_data_len = 3 * ZXDH_DTB_LEN_POS_SETP;
		etcam_dump_inerval = 8;
	} else if (ZXDH_ETCAM_KEY_160b == etcam_key_mode) {
		dump_data_len = 2 * ZXDH_DTB_LEN_POS_SETP;
		etcam_dump_inerval = 12;
	} else if (ZXDH_ETCAM_KEY_80b == etcam_key_mode) {
		dump_data_len = 1 * ZXDH_DTB_LEN_POS_SETP;
		etcam_dump_inerval = 6;
	}

	*p_etcam_dump_len = dump_data_len;
	*p_etcam_dump_inerval = etcam_dump_inerval;
}

static void
zxdh_np_dtb_get_etcam_xy_from_dump_data(uint8_t *p_data,
						uint8_t *p_mask,
						uint32_t etcam_dump_len,
						uint32_t etcam_dump_inerval,
						ZXDH_ETCAM_ENTRY_T *p_entry_xy)
{
	uint8_t *p_entry_data = NULL;
	uint8_t *p_entry_mask = NULL;

	zxdh_np_comm_swap(p_data, etcam_dump_len);
	zxdh_np_comm_swap(p_mask, etcam_dump_len);

	p_entry_data = p_data + etcam_dump_inerval;
	p_entry_mask = p_mask + etcam_dump_inerval;

	memcpy(p_entry_xy->p_data, p_entry_data,
		ZXDH_ETCAM_ENTRY_SIZE_GET(p_entry_xy->mode));
	memcpy(p_entry_xy->p_mask, p_entry_mask,
		ZXDH_ETCAM_ENTRY_SIZE_GET(p_entry_xy->mode));
}


static void
zxdh_np_etcam_xy_to_dm(ZXDH_ETCAM_ENTRY_T *p_dm,
						ZXDH_ETCAM_ENTRY_T *p_xy,
						uint32_t len)
{
	uint32_t i = 0;

	RTE_ASSERT(p_dm->p_data && p_dm->p_mask && p_xy->p_data && p_xy->p_mask);

	for (i = 0; i < len; i++) {
		p_dm->p_data[i] = ZXDH_COMM_XY_TO_DATA(p_xy->p_data[i], p_xy->p_mask[i]);
		p_dm->p_mask[i] = ZXDH_COMM_XY_TO_MASK(p_xy->p_data[i], p_xy->p_mask[i]);
	}
}

static uint32_t
zxdh_np_dtb_etcam_entry_get(uint32_t dev_id,
							uint32_t queue_id,
							uint32_t block_idx,
							uint32_t addr,
							uint32_t rd_mode,
							uint32_t opr_type,
							uint32_t as_en,
							uint32_t as_eram_baddr,
							uint32_t as_eram_index,
							uint32_t as_rsp_mode,
							ZXDH_ETCAM_ENTRY_T *p_entry,
							uint8_t	*p_as_rslt)
{
	uint32_t rc = ZXDH_OK;

	uint32_t etcam_key_mode = 0;

	uint8_t temp_data[ZXDH_ETCAM_WIDTH_MAX / 8] = {0};
	uint8_t temp_mask[ZXDH_ETCAM_WIDTH_MAX / 8] = {0};
	ZXDH_ETCAM_ENTRY_T entry_xy = {0};

	uint32_t etcam_data_dst_phy_haddr = 0;
	uint32_t etcam_data_dst_phy_laddr = 0;
	uint32_t etcam_mask_dst_phy_haddr = 0;
	uint32_t etcam_mask_dst_phy_laddr = 0;
	uint32_t as_rst_dst_phy_haddr = 0;
	uint32_t as_rst_dst_phy_laddr = 0;

	uint32_t dump_element_id = 0;
	uint32_t etcam_dump_one_data_len = 0;
	uint32_t etcam_dump_inerval = 0;
	uint32_t dtb_desc_addr_offset = 0;
	uint32_t dump_data_len = 0;
	uint32_t dtb_desc_len = 0;

	uint32_t eram_dump_base_addr = 0;
	uint32_t row_index = 0;
	uint32_t col_index = 0;

	uint8_t *p_data = NULL;
	uint8_t *p_mask = NULL;
	uint8_t *p_rst = NULL;
	uint8_t  *temp_dump_out_data = NULL;
	uint8_t *dump_info_buff = NULL;
	ZXDH_ETCAM_DUMP_INFO_T etcam_dump_info = {0};
	ZXDH_DTB_ENTRY_T   dtb_dump_entry = {0};
	uint8_t cmd_buff[ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8] = {0};

	dtb_dump_entry.cmd = cmd_buff;

	entry_xy.p_data = temp_data;
	entry_xy.p_mask = temp_mask;

	etcam_key_mode = p_entry->mode;

	etcam_dump_info.block_sel = block_idx;
	etcam_dump_info.addr = addr;
	etcam_dump_info.tb_width = 3 - etcam_key_mode;
	etcam_dump_info.rd_mode = rd_mode;
	etcam_dump_info.tb_depth = 1;

	rc = zxdh_np_dtb_tab_up_free_item_get(dev_id, queue_id, &dump_element_id);
	if (rc != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "zxdh_np_dtb_tab_up_free_item_get failed!");
		return ZXDH_RC_DTB_QUEUE_ITEM_SW_EMPTY;
	}

	dtb_etcam_dump_data_len(etcam_key_mode, &etcam_dump_one_data_len, &etcam_dump_inerval);

	etcam_dump_info.data_or_mask = ZXDH_ETCAM_DTYPE_DATA;
	zxdh_np_dtb_tab_up_item_offset_addr_get(dev_id,
							queue_id,
							dump_element_id,
							dump_data_len,
							&etcam_data_dst_phy_haddr,
							&etcam_data_dst_phy_laddr);

	zxdh_np_dtb_etcam_dump_entry(dev_id,
							&etcam_dump_info,
							etcam_data_dst_phy_haddr,
							etcam_data_dst_phy_laddr,
							&dtb_dump_entry);

	dump_info_buff = rte_zmalloc(NULL, ZXDH_DTB_TABLE_DUMP_INFO_BUFF_SIZE, 0);
	if (dump_info_buff == NULL) {
		PMD_DRV_LOG(ERR, "malloc memory failed");
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	zxdh_np_dtb_data_write(dump_info_buff, dtb_desc_addr_offset, &dtb_dump_entry);
	memset(cmd_buff, 0, ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8);
	dtb_desc_len += 1;
	dtb_desc_addr_offset += ZXDH_DTB_LEN_POS_SETP;
	dump_data_len += etcam_dump_one_data_len;

	etcam_dump_info.data_or_mask = ZXDH_ETCAM_DTYPE_MASK;
	zxdh_np_dtb_tab_up_item_offset_addr_get(dev_id,
								queue_id,
								dump_element_id,
								dump_data_len,
								&etcam_mask_dst_phy_haddr,
								&etcam_mask_dst_phy_laddr);

	zxdh_np_dtb_etcam_dump_entry(dev_id,
								&etcam_dump_info,
								etcam_mask_dst_phy_haddr,
								etcam_mask_dst_phy_laddr,
								&dtb_dump_entry);
	zxdh_np_dtb_data_write(dump_info_buff, dtb_desc_addr_offset, &dtb_dump_entry);
	memset(cmd_buff, 0, ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8);
	dtb_desc_len += 1;
	dtb_desc_addr_offset += ZXDH_DTB_LEN_POS_SETP;
	dump_data_len += etcam_dump_one_data_len;

	if (as_en) {
		zxdh_np_eram_index_cal(as_rsp_mode, as_eram_index, &row_index, &col_index);

		eram_dump_base_addr = as_eram_baddr + row_index;
		zxdh_np_dtb_tab_up_item_offset_addr_get(dev_id,
								queue_id,
								dump_element_id,
								dump_data_len,
								&as_rst_dst_phy_haddr,
								&as_rst_dst_phy_laddr);

		zxdh_np_dtb_smmu0_dump_entry(dev_id,
								eram_dump_base_addr,
								1,
								as_rst_dst_phy_haddr,
								as_rst_dst_phy_laddr,
								&dtb_dump_entry);
		zxdh_np_dtb_data_write(dump_info_buff, dtb_desc_addr_offset, &dtb_dump_entry);
		memset(cmd_buff, 0, ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8);
		dtb_desc_len += 1;
		dtb_desc_addr_offset += ZXDH_DTB_LEN_POS_SETP;
		dump_data_len += ZXDH_DTB_LEN_POS_SETP;
	}

	temp_dump_out_data = rte_zmalloc(NULL, dump_data_len, 0);
	if (temp_dump_out_data == NULL) {
		PMD_DRV_LOG(ERR, "malloc memory failed");
		rte_free(dump_info_buff);
		return ZXDH_PAR_CHK_POINT_NULL;
	}
	p_data = temp_dump_out_data;

	rc = zxdh_np_dtb_write_dump_desc_info(dev_id,
								queue_id,
								dump_element_id,
								(uint32_t *)dump_info_buff,
								dump_data_len / 4,
								dtb_desc_len * 4,
								(uint32_t *)temp_dump_out_data);

	p_data = temp_dump_out_data;
	p_mask = p_data + etcam_dump_one_data_len;

	zxdh_np_dtb_get_etcam_xy_from_dump_data(p_data,
								p_mask,
								etcam_dump_one_data_len,
								etcam_dump_inerval,
								&entry_xy);

	if (opr_type == ZXDH_ETCAM_OPR_DM) {
		zxdh_np_etcam_xy_to_dm(p_entry, &entry_xy,
			ZXDH_ETCAM_ENTRY_SIZE_GET(p_entry->mode));
	} else {
		memcpy(p_entry->p_data, entry_xy.p_data,
			ZXDH_ETCAM_ENTRY_SIZE_GET(p_entry->mode));
		memcpy(p_entry->p_mask, entry_xy.p_mask,
			ZXDH_ETCAM_ENTRY_SIZE_GET(p_entry->mode));
	}

	if (as_en) {
		p_rst = p_mask + etcam_dump_one_data_len;
		memcpy(p_as_rslt, p_rst, (128 / 8));
	}

	rte_free(dump_info_buff);
	rte_free(temp_dump_out_data);

	return rc;
}

static uint32_t
zxdh_np_etcam_entry_cmp(ZXDH_ETCAM_ENTRY_T *p_entry_dm, ZXDH_ETCAM_ENTRY_T *p_entry_xy)
{
	uint32_t data_mask_len = 0;
	uint8_t temp_data[ZXDH_ETCAM_WIDTH_MAX / 8] = {0};
	uint8_t temp_mask[ZXDH_ETCAM_WIDTH_MAX / 8] = {0};
	ZXDH_ETCAM_ENTRY_T entry_xy_temp = {0};

	entry_xy_temp.mode = p_entry_dm->mode;
	entry_xy_temp.p_data = temp_data;
	entry_xy_temp.p_mask = temp_mask;

	data_mask_len = ZXDH_ETCAM_ENTRY_SIZE_GET(entry_xy_temp.mode);

	zxdh_np_etcam_dm_to_xy(p_entry_dm, &entry_xy_temp, data_mask_len);

	if ((memcmp(entry_xy_temp.p_data, p_entry_xy->p_data, data_mask_len) != 0) ||
		(memcmp(entry_xy_temp.p_mask, p_entry_xy->p_mask, data_mask_len) != 0)) {
		return ZXDH_ERR;
	}

	return ZXDH_OK;
}

static uint32_t
zxdh_np_dtb_acl_data_get(uint32_t dev_id,
						uint32_t queue_id,
						uint32_t sdt_no,
						ZXDH_DTB_ACL_ENTRY_INFO_T *p_dump_acl_entry)
{
	uint32_t  rc = ZXDH_OK;
	uint32_t block_idx = 0;
	uint32_t ram_addr = 0;
	uint32_t etcam_wr_mode = 0;
	uint32_t etcam_key_mode = 0;
	uint32_t etcam_table_id = 0;
	uint32_t as_enable = 0;
	uint32_t as_eram_baddr = 0;
	uint32_t etcam_as_mode = 0;
	uint32_t row_index = 0;
	uint32_t col_index = 0;

	ZXDH_ETCAM_ENTRY_T etcam_entry_dm = {0};
	ZXDH_ETCAM_ENTRY_T etcam_entry_xy = {0};
	uint32_t as_eram_data[4] = {0};
	uint8_t temp_data[ZXDH_ETCAM_WIDTH_MAX / 8] = {0};
	uint8_t temp_mask[ZXDH_ETCAM_WIDTH_MAX / 8] = {0};

	ZXDH_ACL_CFG_EX_T *p_acl_cfg = NULL;
	ZXDH_ACL_TBL_CFG_T *p_tbl_cfg = NULL;

	ZXDH_SDT_TBL_ETCAM_T sdt_etcam_info = {0};

	PMD_DRV_LOG(DEBUG, "sdt_no:%u", sdt_no);
	rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_etcam_info);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");
	etcam_key_mode = sdt_etcam_info.etcam_key_mode;
	etcam_as_mode = sdt_etcam_info.as_rsp_mode;
	etcam_table_id = sdt_etcam_info.etcam_table_id;
	as_enable = sdt_etcam_info.as_en;
	as_eram_baddr = sdt_etcam_info.as_eram_baddr;

	etcam_entry_xy.mode = etcam_key_mode;
	etcam_entry_xy.p_data = temp_data;
	etcam_entry_xy.p_mask = temp_mask;
	etcam_entry_dm.mode = etcam_key_mode;
	etcam_entry_dm.p_data = p_dump_acl_entry->key_data;
	etcam_entry_dm.p_mask = p_dump_acl_entry->key_mask;

	zxdh_np_acl_cfg_get(dev_id, &p_acl_cfg);

	p_tbl_cfg = p_acl_cfg->acl_tbls + etcam_table_id;

	if (!p_tbl_cfg->is_used) {
		PMD_DRV_LOG(ERR, "table[ %u ] is not init!", etcam_table_id);
		RTE_ASSERT(0);
		return ZXDH_ACL_RC_TBL_NOT_INIT;
	}

	zxdh_np_acl_hdw_addr_get(p_tbl_cfg, p_dump_acl_entry->handle,
		&block_idx, &ram_addr, &etcam_wr_mode);

	rc = zxdh_np_dtb_etcam_entry_get(dev_id,
								 queue_id,
								 block_idx,
								 ram_addr,
								 etcam_wr_mode,
								 ZXDH_ETCAM_OPR_XY,
								 as_enable,
								 as_eram_baddr,
								 p_dump_acl_entry->handle,
								 etcam_as_mode,
								 &etcam_entry_xy,
								 (uint8_t *)as_eram_data);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_etcam_entry_get");

	if (zxdh_np_etcam_entry_cmp(&etcam_entry_dm, &etcam_entry_xy) == 0) {
		PMD_DRV_LOG(DEBUG, "get done, handle:0x%x block:%u ram_addr:%u rd_mode:%x",
			p_dump_acl_entry->handle, block_idx, ram_addr, etcam_wr_mode);
	} else {
		PMD_DRV_LOG(DEBUG, "get fail, handle:0x%x block:%u ram_addr:%u rd_mode:%x",
			p_dump_acl_entry->handle, block_idx, ram_addr, etcam_wr_mode);

		return ZXDH_ERR;
	}

	if (as_enable) {
		zxdh_np_eram_index_cal(etcam_as_mode, p_dump_acl_entry->handle,
			&row_index, &col_index);
		switch (etcam_as_mode) {
		case ZXDH_ERAM128_TBL_128b:
			memcpy(p_dump_acl_entry->p_as_rslt, as_eram_data, (128 / 8));
			break;

		case ZXDH_ERAM128_TBL_64b:
			memcpy(p_dump_acl_entry->p_as_rslt, as_eram_data +
				((1 - col_index) << 1), (64 / 8));
			break;

		case ZXDH_ERAM128_TBL_1b:
			ZXDH_COMM_UINT32_GET_BITS(*(uint32_t *)p_dump_acl_entry->p_as_rslt,
				*(as_eram_data + (3 - col_index / 32)), (col_index % 32), 1);
			break;
		default:
			break;
		}
	}

	return rc;
}

int
zxdh_np_dtb_table_entry_get(uint32_t dev_id,
		 uint32_t queue_id,
		 ZXDH_DTB_USER_ENTRY_T *get_entry,
		 uint32_t srh_mode)
{
	ZXDH_SDT_TBL_DATA_T sdt_tbl = {0};
	uint32_t tbl_type = 0;
	uint32_t rc;
	uint32_t sdt_no;
	uint32_t sdt_partner = 0;
	uint32_t valid = 0;
	uint8_t key = 0;

	sdt_no = get_entry->sdt_no;
	PMD_DRV_LOG(DEBUG, "sdt_no:%u", sdt_no);

	zxdh_np_sdt_tbl_data_get(dev_id, sdt_no, &sdt_tbl);

	ZXDH_COMM_UINT32_GET_BITS(tbl_type, sdt_tbl.data_high32,
			ZXDH_SDT_H_TBL_TYPE_BT_POS, ZXDH_SDT_H_TBL_TYPE_BT_LEN);
	switch (tbl_type) {
	case ZXDH_SDT_TBLT_ERAM:
		rc = zxdh_np_dtb_eram_data_get(dev_id,
				queue_id,
				sdt_no,
				(ZXDH_DTB_ERAM_ENTRY_INFO_T *)get_entry->p_entry_data);
		ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "dpp_dtb_eram_data_get");
		break;
	case ZXDH_SDT_TBLT_HASH:
		do {
			rc = zxdh_np_dtb_hash_data_get(dev_id,
					queue_id,
					sdt_no,
					(ZXDH_DTB_HASH_ENTRY_INFO_T *)get_entry->p_entry_data,
					srh_mode);
			sdt_partner = zxdh_np_apt_get_sdt_partner(dev_id, sdt_no);
			valid = zxdh_np_hash_sdt_partner_valid(sdt_no, sdt_partner, &key);
			sdt_no = sdt_partner;
		} while ((rc == ZXDH_HASH_RC_SRH_FAIL) && (valid == ZXDH_TRUE));

		if (rc == ZXDH_HASH_RC_SRH_FAIL) {
			PMD_DRV_LOG(DEBUG, "hash search failed");
			return rc;
		}

		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_hash_data_get");
		break;
	case ZXDH_SDT_TBLT_ETCAM:
		rc = zxdh_np_dtb_acl_data_get(dev_id,
				queue_id,
				sdt_no,
				(ZXDH_DTB_ACL_ENTRY_INFO_T *)get_entry->p_entry_data);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_acl_data_get");
		break;
	default:
		PMD_DRV_LOG(ERR, "SDT table_type[ %u ] is invalid!", tbl_type);
		return 1;
	}

	return 0;
}

static uint32_t
zxdh_np_dtb_se_smmu0_ind_read(uint32_t dev_id,
		uint32_t queue_id,
		uint32_t base_addr,
		uint32_t index,
		uint32_t rd_mode,
		uint32_t *p_data)
{
	uint32_t temp_data[4] = {0};
	uint32_t element_id = 0;
	uint32_t row_index = 0;
	uint32_t col_index = 0;
	uint32_t eram_dump_base_addr;
	uint32_t rc;

	switch (rd_mode) {
	case ZXDH_ERAM128_OPR_128b:
		row_index = index;
		break;
	case ZXDH_ERAM128_OPR_64b:
		row_index = (index >> 1);
		col_index = index & 0x1;
		break;
	case ZXDH_ERAM128_OPR_1b:
		row_index = (index >> 7);
		col_index = index & 0x7F;
		break;
	default:
		break;
	}

	eram_dump_base_addr = base_addr + row_index;
	rc = zxdh_np_dtb_se_smmu0_dma_dump(dev_id,
			queue_id,
			eram_dump_base_addr,
			1,
			temp_data,
			&element_id);
	ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "zxdh_np_dtb_se_smmu0_dma_dump");

	switch (rd_mode) {
	case ZXDH_ERAM128_OPR_128b:
		memcpy(p_data, temp_data, (128 / 8));
		break;
	case ZXDH_ERAM128_OPR_64b:
		memcpy(p_data, temp_data + ((1 - col_index) << 1), (64 / 8));
		break;
	case ZXDH_ERAM128_OPR_1b:
		ZXDH_COMM_UINT32_GET_BITS(p_data[0], *(temp_data +
			(3 - col_index / 32)), (col_index % 32), 1);
		break;
	default:
		break;
	}

	return rc;
}

static uint32_t
zxdh_np_dtb_stat_smmu0_int_read(uint32_t dev_id,
		uint32_t queue_id,
		uint32_t smmu0_base_addr,
		ZXDH_STAT_CNT_MODE_E rd_mode,
		uint32_t index,
		uint32_t *p_data)
{
	uint32_t eram_rd_mode;
	uint32_t rc;

	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_data);

	if (rd_mode == ZXDH_STAT_128_MODE)
		eram_rd_mode = ZXDH_ERAM128_OPR_128b;
	else
		eram_rd_mode = ZXDH_ERAM128_OPR_64b;

	rc = zxdh_np_dtb_se_smmu0_ind_read(dev_id,
								   queue_id,
								   smmu0_base_addr,
								   index,
								   eram_rd_mode,
								   p_data);
	ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "zxdh_np_dtb_se_smmu0_ind_read");

	return rc;
}

int
zxdh_np_dtb_stats_get(uint32_t dev_id,
		uint32_t queue_id,
		ZXDH_STAT_CNT_MODE_E rd_mode,
		uint32_t index,
		uint32_t *p_data)
{
	ZXDH_PPU_STAT_CFG_T stat_cfg = {0};
	uint32_t ppu_eram_baddr;
	uint32_t ppu_eram_depth;
	uint32_t rc = 0;

	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_data);

	memset(&stat_cfg, 0x0, sizeof(stat_cfg));

	zxdh_np_stat_cfg_soft_get(dev_id, &stat_cfg);

	ppu_eram_depth = stat_cfg.eram_depth;
	ppu_eram_baddr = stat_cfg.eram_baddr;

	if ((index >> (ZXDH_STAT_128_MODE - rd_mode)) < ppu_eram_depth) {
		rc = zxdh_np_dtb_stat_smmu0_int_read(dev_id,
									queue_id,
									ppu_eram_baddr,
									rd_mode,
									index,
									p_data);
		ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "dpp_dtb_stat_smmu0_int_read");
	}

	return rc;
}

static uint32_t
zxdh_np_dtb_queue_down_init(uint32_t dev_id,
							uint32_t queue_id,
							ZXDH_DTB_QUEUE_CFG_T *p_queue_cfg)
{
	uint32_t rc = 0;
	uint32_t i = 0;
	uint32_t ack_vale = 0;
	uint32_t tab_down_item_size = 0;
	ZXDH_DTB_MGR_T *p_dtb_mgr = NULL;

	p_dtb_mgr = zxdh_np_dtb_mgr_get(dev_id);
	p_dtb_mgr->queue_info[queue_id].init_flag = 1;

	tab_down_item_size = (p_queue_cfg->down_item_size == 0) ?
		ZXDH_DTB_ITEM_SIZE : p_queue_cfg->down_item_size;

	p_dtb_mgr->queue_info[queue_id].tab_down.item_size = tab_down_item_size;
	p_dtb_mgr->queue_info[queue_id].tab_down.start_phy_addr = p_queue_cfg->down_start_phy_addr;
	p_dtb_mgr->queue_info[queue_id].tab_down.start_vir_addr = p_queue_cfg->down_start_vir_addr;
	p_dtb_mgr->queue_info[queue_id].tab_down.wr_index = 0;
	p_dtb_mgr->queue_info[queue_id].tab_down.rd_index = 0;

	for (i = 0; i < ZXDH_DTB_QUEUE_ITEM_NUM_MAX; i++) {
		rc = zxdh_np_dtb_item_ack_wr(dev_id, queue_id,
			ZXDH_DTB_DIR_DOWN_TYPE, i, 0, ZXDH_DTB_TAB_ACK_CHECK_VALUE);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_item_ack_wr");
	}

	for (i = 0; i < ZXDH_DTB_QUEUE_ITEM_NUM_MAX; i++) {
		rc = zxdh_np_dtb_item_ack_rd(dev_id, queue_id,
			ZXDH_DTB_DIR_DOWN_TYPE, i, 0, &ack_vale);
		if (ack_vale != ZXDH_DTB_TAB_ACK_CHECK_VALUE) {
			PMD_DRV_LOG(ERR, "dtb queue [%u] down init failed!", queue_id);
			return ZXDH_RC_DTB_MEMORY_ALLOC_ERR;
		}
	}

	memset((uint8_t *)(p_queue_cfg->down_start_vir_addr), 0,
		tab_down_item_size * ZXDH_DTB_QUEUE_ITEM_NUM_MAX);

	PMD_DRV_LOG(INFO, "dtb queue [%u] down init success!!!", queue_id);

	return ZXDH_OK;
}

static uint32_t
zxdh_np_dtb_queue_dump_init(uint32_t dev_id,
					uint32_t queue_id,
					ZXDH_DTB_QUEUE_CFG_T *p_queue_cfg)
{
	uint32_t i = 0;
	uint32_t ack_vale = 0;
	uint32_t tab_up_item_size = 0;
	ZXDH_DTB_MGR_T *p_dtb_mgr = NULL;

	p_dtb_mgr = zxdh_np_dtb_mgr_get(dev_id);
	p_dtb_mgr->queue_info[queue_id].init_flag = 1;

	tab_up_item_size = (p_queue_cfg->up_item_size == 0) ?
		ZXDH_DTB_ITEM_SIZE : p_queue_cfg->up_item_size;

	p_dtb_mgr->queue_info[queue_id].tab_up.item_size = tab_up_item_size;
	p_dtb_mgr->queue_info[queue_id].tab_up.start_phy_addr = p_queue_cfg->up_start_phy_addr;
	p_dtb_mgr->queue_info[queue_id].tab_up.start_vir_addr = p_queue_cfg->up_start_vir_addr;
	p_dtb_mgr->queue_info[queue_id].tab_up.wr_index = 0;
	p_dtb_mgr->queue_info[queue_id].tab_up.rd_index = 0;

	for (i = 0; i < ZXDH_DTB_QUEUE_ITEM_NUM_MAX; i++) {
		zxdh_np_dtb_item_ack_wr(dev_id, queue_id,
			ZXDH_DTB_DIR_UP_TYPE, i, 0, ZXDH_DTB_TAB_ACK_CHECK_VALUE);
	}

	for (i = 0; i < ZXDH_DTB_QUEUE_ITEM_NUM_MAX; i++) {
		zxdh_np_dtb_item_ack_rd(dev_id, queue_id,
			ZXDH_DTB_DIR_UP_TYPE, i, 0, &ack_vale);
		if (ack_vale != ZXDH_DTB_TAB_ACK_CHECK_VALUE) {
			PMD_DRV_LOG(ERR, "dtb queue [%u] dump init failed!!!", queue_id);
			return ZXDH_RC_DTB_MEMORY_ALLOC_ERR;
		}
	}

	memset((uint8_t *)(p_queue_cfg->up_start_vir_addr), 0,
		tab_up_item_size * ZXDH_DTB_QUEUE_ITEM_NUM_MAX);

	PMD_DRV_LOG(INFO, "dtb queue [%u] up init success!!!", queue_id);

	return ZXDH_OK;
}

static void
zxdh_np_dtb_down_channel_addr_set(uint32_t dev_id,
								uint32_t channel_id,
								uint64_t phy_addr,
								uint64_t vir_addr,
								uint32_t size)
{
	ZXDH_DTB_QUEUE_CFG_T down_queue_cfg = {
		.down_start_phy_addr = phy_addr,
		.down_start_vir_addr = vir_addr,
		.down_item_size = size,
	};

	zxdh_np_dtb_queue_down_init(dev_id, channel_id, &down_queue_cfg);
}

static void
zxdh_np_dtb_dump_channel_addr_set(uint32_t dev_id,
								uint32_t channel_id,
								uint64_t phy_addr,
								uint64_t vir_addr,
								uint32_t size)
{
	ZXDH_DTB_QUEUE_CFG_T dump_queue_cfg = {
		.up_start_phy_addr = phy_addr,
		.up_start_vir_addr = vir_addr,
		.up_item_size = size,
	};

	zxdh_np_dtb_queue_dump_init(dev_id, channel_id, &dump_queue_cfg);
}

static uint32_t
zxdh_np_dtb_user_info_set(uint32_t dev_id, uint32_t queue_id, uint16_t vport, uint32_t vector)
{
	uint32_t rc = ZXDH_OK;

	ZXDH_DTB_QUEUE_VM_INFO_T vm_info = {0};
	ZXDH_DTB_MGR_T *p_dtb_mgr = zxdh_np_dtb_mgr_get(dev_id);

	rc = zxdh_np_dtb_queue_vm_info_get(dev_id, queue_id, &vm_info);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_queue_vm_info_get");

	vm_info.dbi_en = 1;
	vm_info.epid = hardware_ep_id[ZXDH_EPID_BY(vport)];
	vm_info.vfunc_num = ZXDH_VFUNC_NUM(vport);
	vm_info.func_num = ZXDH_FUNC_NUM(vport);
	vm_info.vfunc_active = ZXDH_VF_ACTIVE(vport);
	vm_info.vector = vector;

	p_dtb_mgr->queue_info[queue_id].vport = vport;
	p_dtb_mgr->queue_info[queue_id].vector = vector;

	rc = zxdh_np_dtb_queue_vm_info_set(dev_id, queue_id, &vm_info);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_queue_vm_info_set");

	return rc;
}

static uint32_t
zxdh_np_dtb_dump_sdt_addr_set(uint32_t dev_id,
							uint32_t queue_id,
							uint32_t sdt_no,
							uint64_t phy_addr,
							uint64_t vir_addr,
							uint32_t size)
{
	uint32_t rc = ZXDH_OK;

	ZXDH_DTB_ADDR_INFO_T dtb_dump_addr_info = {
		.sdt_no = sdt_no,
		.phy_addr = phy_addr,
		.vir_addr = vir_addr,
		.size = size,
	};
	ZXDH_RB_CFG *p_dtb_dump_addr_rb = NULL;

	p_dtb_dump_addr_rb = zxdh_np_dtb_dump_addr_rb_get(dev_id, queue_id);
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_dtb_dump_addr_rb);

	rc = zxdh_np_se_apt_rb_insert(p_dtb_dump_addr_rb,
		&dtb_dump_addr_info, sizeof(ZXDH_DTB_ADDR_INFO_T));
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_se_apt_rb_insert");

	return rc;
}

static uint32_t
zxdh_np_apt_dtb_res_init(uint32_t dev_id, ZXDH_DEV_INIT_CTRL_T *p_dev_init_ctrl)
{
	uint32_t rc = ZXDH_OK;

	uint32_t queue_id = 0;
	uint32_t index = 0;
	uint32_t dump_sdt_num = 0;
	ZXDH_DTB_ADDR_INFO_T *p_dump_info = NULL;

	rc = zxdh_np_dtb_queue_request(dev_id, p_dev_init_ctrl->port_name,
		p_dev_init_ctrl->vport, &queue_id);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_queue_request");

	p_dev_init_ctrl->queue_id = queue_id;

	rc = zxdh_np_dtb_user_info_set(dev_id, queue_id,
		p_dev_init_ctrl->vport, p_dev_init_ctrl->vector);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_user_info_set");

	zxdh_np_dtb_down_channel_addr_set(dev_id, queue_id,
		p_dev_init_ctrl->down_phy_addr, p_dev_init_ctrl->down_vir_addr, 0);

	zxdh_np_dtb_dump_channel_addr_set(dev_id, queue_id,
		p_dev_init_ctrl->dump_phy_addr, p_dev_init_ctrl->dump_vir_addr, 0);

	dump_sdt_num = p_dev_init_ctrl->dump_sdt_num;
	for (index = 0; index < dump_sdt_num; index++) {
		p_dump_info = p_dev_init_ctrl->dump_addr_info + index;
		ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_dump_info);
		rc = zxdh_np_dtb_dump_sdt_addr_set(dev_id,
							queue_id,
							p_dump_info->sdt_no,
							p_dump_info->phy_addr,
							p_dump_info->vir_addr,
							p_dump_info->size);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_dump_sdt_addr_set");
	}

	return ZXDH_OK;
}

int
zxdh_np_host_init(uint32_t dev_id,
		ZXDH_DEV_INIT_CTRL_T *p_dev_init_ctrl)
{
	ZXDH_SYS_INIT_CTRL_T sys_init_ctrl = {0};
	uint32_t rc;
	uint64_t agent_addr;
	uint32_t bar_msg_num = 0;

	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_dev_init_ctrl);

	sys_init_ctrl.flags = (ZXDH_DEV_ACCESS_TYPE_PCIE << 0) | (ZXDH_DEV_AGENT_ENABLE << 10);
	sys_init_ctrl.pcie_vir_baddr = zxdh_np_addr_calc(p_dev_init_ctrl->pcie_vir_addr,
		p_dev_init_ctrl->np_bar_offset);
	sys_init_ctrl.device_type = ZXDH_DEV_TYPE_CHIP;

	rc = zxdh_np_base_soft_init(dev_id, &sys_init_ctrl);
	ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "zxdh_base_soft_init");

	zxdh_np_dev_vport_set(dev_id, p_dev_init_ctrl->vport);

	agent_addr = ZXDH_PCIE_AGENT_ADDR_OFFSET + p_dev_init_ctrl->pcie_vir_addr;
	zxdh_np_dev_agent_addr_set(dev_id, agent_addr);

	zxdh_np_pf_fw_compatible_addr_set(dev_id, p_dev_init_ctrl->pcie_vir_addr);

	rc = zxdh_np_np_sdk_version_compatible_check(dev_id);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_np_sdk_version_compatible_check");

	rc = zxdh_np_pcie_bar_msg_num_get(dev_id, &bar_msg_num);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_pcie_bar_msg_num_get");

	zxdh_np_dev_fw_bar_msg_num_set(dev_id, bar_msg_num);

	rc = zxdh_np_apt_dtb_res_init(dev_id, p_dev_init_ctrl);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_apt_dtb_res_init");
	PMD_DRV_LOG(INFO, "host init done, queue_id = %u", p_dev_init_ctrl->queue_id);

	return 0;
}

static uint32_t
zxdh_np_get_se_buff_size(uint32_t opr)
{
	uint32_t buff_size = 0;

	switch (opr) {
	case ZXDH_HASH_FUNC_BULK_REQ:
		buff_size = sizeof(ZXDH_NP_SE_HASH_FUNC_BULK_T);
		break;
	case ZXDH_HASH_TBL_REQ:
		buff_size = sizeof(ZXDH_NP_SE_HASH_TBL_T);
		break;
	case ZXDH_ERAM_TBL_REQ:
		buff_size = sizeof(ZXDH_NP_SE_ERAM_TBL_T);
		break;
	case ZXDH_ACL_TBL_REQ:
		buff_size = sizeof(ZXDH_NP_SE_ACL_TBL_T);
		break;
	case ZXDH_STAT_CFG_REQ:
		buff_size = sizeof(ZXDH_NP_SE_STAT_CFG_T);
		break;
	default:
		break;
	}

	return buff_size;
}

static void
zxdh_np_hash_func_bulk_set(ZXDH_APT_HASH_RES_INIT_T *p_hash_res_init,
						ZXDH_NP_SE_HASH_FUNC_BULK_T *p_func_bulk)
{
	uint32_t index  = 0;
	ZXDH_APT_HASH_FUNC_RES_T *p_func_res = NULL;
	ZXDH_APT_HASH_BULK_RES_T *p_bulk_res = NULL;

	p_hash_res_init->func_num = p_func_bulk->func_num;
	p_hash_res_init->bulk_num = p_func_bulk->bulk_num;
	for (index = 0; index < (p_hash_res_init->func_num); index++) {
		p_func_res = p_hash_res_init->func_res + index;

		p_func_res->func_id	 = p_func_bulk->fun[index].func_id;
		p_func_res->ddr_dis	 = p_func_bulk->fun[index].ddr_dis;
		p_func_res->zblk_num	= p_func_bulk->fun[index].zblk_num;
		p_func_res->zblk_bitmap = p_func_bulk->fun[index].zblk_bitmap;
	}

	for (index = 0; index < (p_hash_res_init->bulk_num); index++) {
		p_bulk_res = p_hash_res_init->bulk_res + index;

		p_bulk_res->func_id		= p_func_bulk->bulk[index].func_id;
		p_bulk_res->bulk_id		= p_func_bulk->bulk[index].bulk_id;
		p_bulk_res->zcell_num	  = p_func_bulk->bulk[index].zcell_num;
		p_bulk_res->zreg_num	   = p_func_bulk->bulk[index].zreg_num;
		p_bulk_res->ddr_baddr	  = p_func_bulk->bulk[index].ddr_baddr;
		p_bulk_res->ddr_item_num   = p_func_bulk->bulk[index].ddr_item_num;
		p_bulk_res->ddr_width_mode = p_func_bulk->bulk[index].ddr_width_mode;
		p_bulk_res->ddr_crc_sel	= p_func_bulk->bulk[index].ddr_crc_sel;
		p_bulk_res->ddr_ecc_en	 = p_func_bulk->bulk[index].ddr_ecc_en;
	}
}

static void
zxdh_np_hash_tbl_set(ZXDH_APT_HASH_RES_INIT_T *p_hash_res_init, ZXDH_NP_SE_HASH_TBL_T *p_hash_tbl)
{
	uint32_t index  = 0;
	ZXDH_APT_HASH_TABLE_T  *p_tbl_res = NULL;

	p_hash_res_init->tbl_num = p_hash_tbl->tbl_num;
	for (index = 0; index < (p_hash_res_init->tbl_num); index++) {
		p_tbl_res = p_hash_res_init->tbl_res + index;

		p_tbl_res->sdt_no = p_hash_tbl->table[index].sdt_no;
		p_tbl_res->sdt_partner = p_hash_tbl->table[index].sdt_partner;
		p_tbl_res->tbl_flag	= p_hash_tbl->table[index].tbl_flag;
		p_tbl_res->hash_sdt.table_type =
			p_hash_tbl->table[index].hash_sdt.table_type;
		p_tbl_res->hash_sdt.hash_id	= p_hash_tbl->table[index].hash_sdt.hash_id;
		p_tbl_res->hash_sdt.hash_table_width =
			p_hash_tbl->table[index].hash_sdt.hash_table_width;
		p_tbl_res->hash_sdt.key_size = p_hash_tbl->table[index].hash_sdt.key_size;
		p_tbl_res->hash_sdt.hash_table_id =
			p_hash_tbl->table[index].hash_sdt.hash_table_id;
		p_tbl_res->hash_sdt.learn_en = p_hash_tbl->table[index].hash_sdt.learn_en;
		p_tbl_res->hash_sdt.keep_alive =
			p_hash_tbl->table[index].hash_sdt.keep_alive;
		p_tbl_res->hash_sdt.keep_alive_baddr =
			p_hash_tbl->table[index].hash_sdt.keep_alive_baddr;
		p_tbl_res->hash_sdt.rsp_mode =
			p_hash_tbl->table[index].hash_sdt.rsp_mode;
		p_tbl_res->hash_sdt.hash_clutch_en =
			p_hash_tbl->table[index].hash_sdt.hash_clutch_en;
	}
}

static void
zxdh_np_eram_tbl_set(ZXDH_APT_ERAM_RES_INIT_T *p_eam_res_init, ZXDH_NP_SE_ERAM_TBL_T *p_eram_tbl)
{
	uint32_t index  = 0;
	ZXDH_APT_ERAM_TABLE_T *p_eram_res = NULL;

	p_eam_res_init->tbl_num = p_eram_tbl->tbl_num;
	for (index = 0; index < (p_eam_res_init->tbl_num); index++) {
		p_eram_res = p_eam_res_init->eram_res + index;

		p_eram_res->sdt_no	= p_eram_tbl->eram[index].sdt_no;
		p_eram_res->opr_mode = p_eram_tbl->eram[index].opr_mode;
		p_eram_res->rd_mode	= p_eram_tbl->eram[index].rd_mode;
		p_eram_res->eram_sdt.table_type	= p_eram_tbl->eram[index].eram_sdt.table_type;
		p_eram_res->eram_sdt.eram_mode = p_eram_tbl->eram[index].eram_sdt.eram_mode;
		p_eram_res->eram_sdt.eram_base_addr =
			p_eram_tbl->eram[index].eram_sdt.eram_base_addr;
		p_eram_res->eram_sdt.eram_table_depth =
			p_eram_tbl->eram[index].eram_sdt.eram_table_depth;
		p_eram_res->eram_sdt.eram_clutch_en =
			p_eram_tbl->eram[index].eram_sdt.eram_clutch_en;
	}
}

static void
zxdh_np_acl_tbl_set(ZXDH_APT_ACL_RES_INIT_T *p_acl_res_init, ZXDH_NP_SE_ACL_TBL_T *p_acl_tbl)
{
	uint32_t index  = 0;
	ZXDH_APT_ACL_TABLE_T *p_acl_res = NULL;

	p_acl_res_init->tbl_num = p_acl_tbl->tbl_num;
	for (index = 0; index < (p_acl_tbl->tbl_num); index++) {
		p_acl_res = p_acl_res_init->acl_res + index;

		p_acl_res->sdt_no = p_acl_tbl->acl[index].sdt_no;
		p_acl_res->sdt_partner = p_acl_tbl->acl[index].sdt_partner;
		p_acl_res->acl_res.block_num = p_acl_tbl->acl[index].acl_res.block_num;
		 p_acl_res->acl_res.entry_num = p_acl_tbl->acl[index].acl_res.entry_num;
		p_acl_res->acl_res.pri_mode	= p_acl_tbl->acl[index].acl_res.pri_mode;
		memcpy(p_acl_res->acl_res.block_index,
			p_acl_tbl->acl[index].acl_res.block_index,
			sizeof(uint32_t) * ZXDH_ETCAM_BLOCK_NUM);
		p_acl_res->acl_sdt.table_type = p_acl_tbl->acl[index].acl_sdt.table_type;
		p_acl_res->acl_sdt.etcam_id	= p_acl_tbl->acl[index].acl_sdt.etcam_id;
		p_acl_res->acl_sdt.etcam_key_mode = p_acl_tbl->acl[index].acl_sdt.etcam_key_mode;
		p_acl_res->acl_sdt.etcam_table_id = p_acl_tbl->acl[index].acl_sdt.etcam_table_id;
		p_acl_res->acl_sdt.no_as_rsp_mode = p_acl_tbl->acl[index].acl_sdt.no_as_rsp_mode;
		p_acl_res->acl_sdt.as_en = p_acl_tbl->acl[index].acl_sdt.as_en;
		p_acl_res->acl_sdt.as_eram_baddr = p_acl_tbl->acl[index].acl_sdt.as_eram_baddr;
		p_acl_res->acl_sdt.as_rsp_mode = p_acl_tbl->acl[index].acl_sdt.as_rsp_mode;
		p_acl_res->acl_sdt.etcam_table_depth =
			p_acl_tbl->acl[index].acl_sdt.etcam_table_depth;
		p_acl_res->acl_sdt.etcam_clutch_en = p_acl_tbl->acl[index].acl_sdt.etcam_clutch_en;
	}
}

static void
zxdh_np_stat_cfg_set(ZXDH_APT_STAT_RES_INIT_T *p_stat_res_init, ZXDH_NP_SE_STAT_CFG_T *p_stat_cfg)
{
	p_stat_res_init->eram_baddr	 = p_stat_cfg->eram_baddr;
	p_stat_res_init->eram_depth	 = p_stat_cfg->eram_depth;
	p_stat_res_init->ddr_baddr	  = p_stat_cfg->ddr_baddr;
	p_stat_res_init->ppu_ddr_offset = p_stat_cfg->ppu_ddr_offset;
}

static uint32_t
zxdh_np_agent_hash_func_bulk_get(uint32_t dev_id, uint32_t type,
						ZXDH_APT_HASH_RES_INIT_T *p_hash_res_init)
{
	uint32_t rc = ZXDH_OK;
	uint32_t opr = ZXDH_HASH_FUNC_BULK_REQ;
	uint32_t sub_type = ZXDH_RES_STD_NIC_MSG;
	uint32_t buff_size = 0;
	ZXDH_SPINLOCK_T *p_dtb_spinlock = NULL;
	uint32_t *p_rsp_buff = NULL;
	ZXDH_NP_SE_HASH_FUNC_BULK_T *p_func_bulk = NULL;
	ZXDH_DEV_SPINLOCK_TYPE_E spinlock = ZXDH_DEV_SPINLOCK_T_DTB;

	rc = zxdh_np_dev_opr_spinlock_get(dev_id, (uint32_t)spinlock, &p_dtb_spinlock);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dev_opr_spinlock_get");

	rte_spinlock_lock(&p_dtb_spinlock->spinlock);

	buff_size = zxdh_np_get_se_buff_size(opr) + sizeof(uint32_t);
	p_rsp_buff = rte_zmalloc(NULL, buff_size, 0);
	if (p_rsp_buff == NULL) {
		PMD_DRV_LOG(ERR, "malloc memory failed");
		rte_spinlock_unlock(&p_dtb_spinlock->spinlock);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	sub_type = (type == ZXDH_SE_STD_NIC_RES_TYPE) ? ZXDH_RES_STD_NIC_MSG : ZXDH_RES_OFFLOAD_MSG;

	rc = zxdh_np_agent_channel_se_res_get(dev_id, sub_type, opr, p_rsp_buff, buff_size);
	if (rc != ZXDH_OK) {
		rte_free(p_rsp_buff);
		PMD_DRV_LOG(ERR, "hash func&bulk res get fail rc=0x%x.", rc);
		rte_spinlock_unlock(&p_dtb_spinlock->spinlock);
		return ZXDH_ERR;
	}

	rte_spinlock_unlock(&p_dtb_spinlock->spinlock);

	p_func_bulk = (ZXDH_NP_SE_HASH_FUNC_BULK_T *)(p_rsp_buff + 1);
	zxdh_np_hash_func_bulk_set(p_hash_res_init, p_func_bulk);
	rte_free(p_rsp_buff);

	return rc;
}

static uint32_t
zxdh_np_agent_hash_tbl_get(uint32_t dev_id,
			uint32_t type,
			ZXDH_APT_HASH_RES_INIT_T *p_hash_res_init)
{
	uint32_t rc = ZXDH_OK;
	uint32_t opr = ZXDH_HASH_TBL_REQ;
	uint32_t sub_type = ZXDH_RES_STD_NIC_MSG;
	uint32_t buff_size = 0;
	ZXDH_SPINLOCK_T *p_dtb_spinlock = NULL;
	uint32_t *p_rsp_buff = NULL;
	ZXDH_NP_SE_HASH_TBL_T *p_hash_tbl = NULL;
	ZXDH_DEV_SPINLOCK_TYPE_E spinlock = ZXDH_DEV_SPINLOCK_T_DTB;

	rc = zxdh_np_dev_opr_spinlock_get(dev_id, (uint32_t)spinlock, &p_dtb_spinlock);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dev_opr_spinlock_get");

	rte_spinlock_lock(&p_dtb_spinlock->spinlock);

	buff_size = zxdh_np_get_se_buff_size(opr) + sizeof(uint32_t);
	p_rsp_buff = rte_zmalloc(NULL, buff_size, 0);
	if (p_rsp_buff == NULL) {
		PMD_DRV_LOG(ERR, "malloc memory failed");
		rte_spinlock_unlock(&p_dtb_spinlock->spinlock);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	sub_type = (type == ZXDH_SE_STD_NIC_RES_TYPE) ?
		ZXDH_RES_STD_NIC_MSG : ZXDH_RES_OFFLOAD_MSG;

	rc = zxdh_np_agent_channel_se_res_get(dev_id, sub_type, opr, p_rsp_buff, buff_size);
	if (rc != ZXDH_OK) {
		rte_free(p_rsp_buff);
		PMD_DRV_LOG(ERR, "hash table res get fail rc=0x%x.", rc);
		rte_spinlock_unlock(&p_dtb_spinlock->spinlock);
		return ZXDH_ERR;
	}

	rte_spinlock_unlock(&p_dtb_spinlock->spinlock);

	p_hash_tbl = (ZXDH_NP_SE_HASH_TBL_T *)(p_rsp_buff + 1);
	zxdh_np_hash_tbl_set(p_hash_res_init, p_hash_tbl);
	rte_free(p_rsp_buff);

	return rc;
}

static uint32_t
zxdh_np_agent_eram_tbl_get(uint32_t dev_id, uint32_t type, ZXDH_APT_ERAM_RES_INIT_T *p_eam_res_init)
{
	uint32_t rc = ZXDH_OK;
	uint32_t opr = ZXDH_ERAM_TBL_REQ;
	uint32_t sub_type = ZXDH_RES_STD_NIC_MSG;
	uint32_t buff_size = 0;
	ZXDH_SPINLOCK_T *p_dtb_spinlock = NULL;
	uint32_t *p_rsp_buff = NULL;
	ZXDH_NP_SE_ERAM_TBL_T *p_eram_tbl = NULL;
	ZXDH_DEV_SPINLOCK_TYPE_E spinlock = ZXDH_DEV_SPINLOCK_T_DTB;

	rc = zxdh_np_dev_opr_spinlock_get(dev_id, (uint32_t)spinlock, &p_dtb_spinlock);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dev_opr_spinlock_get");

	rte_spinlock_lock(&p_dtb_spinlock->spinlock);

	buff_size = zxdh_np_get_se_buff_size(opr) + sizeof(uint32_t);
	p_rsp_buff = rte_zmalloc(NULL, buff_size, 0);
	if (p_rsp_buff == NULL) {
		PMD_DRV_LOG(ERR, "malloc memory failed");
		rte_spinlock_unlock(&p_dtb_spinlock->spinlock);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	sub_type = (type == ZXDH_SE_STD_NIC_RES_TYPE) ?
		ZXDH_RES_STD_NIC_MSG : ZXDH_RES_OFFLOAD_MSG;

	rc = zxdh_np_agent_channel_se_res_get(dev_id, sub_type, opr, p_rsp_buff, buff_size);
	if (rc != ZXDH_OK) {
		rte_free(p_rsp_buff);
		PMD_DRV_LOG(ERR, "eram table res get fail rc=0x%x.", rc);
		rte_spinlock_unlock(&p_dtb_spinlock->spinlock);
		return ZXDH_ERR;
	}

	rte_spinlock_unlock(&p_dtb_spinlock->spinlock);

	p_eram_tbl = (ZXDH_NP_SE_ERAM_TBL_T *)(p_rsp_buff + 1);
	zxdh_np_eram_tbl_set(p_eam_res_init, p_eram_tbl);
	rte_free(p_rsp_buff);

	return rc;
}

static uint32_t
zxdh_np_agent_acl_tbl_get(uint32_t dev_id, uint32_t type, ZXDH_APT_ACL_RES_INIT_T *p_acl_res_init)
{
	uint32_t rc = ZXDH_OK;
	uint32_t opr = ZXDH_ACL_TBL_REQ;
	uint32_t sub_type = ZXDH_RES_STD_NIC_MSG;
	uint32_t buff_size = 0;
	ZXDH_SPINLOCK_T *p_dtb_spinlock = NULL;
	uint32_t *p_rsp_buff = NULL;
	ZXDH_NP_SE_ACL_TBL_T *p_acl_tbl = NULL;
	ZXDH_DEV_SPINLOCK_TYPE_E spinlock = ZXDH_DEV_SPINLOCK_T_DTB;

	rc = zxdh_np_dev_opr_spinlock_get(dev_id, (uint32_t)spinlock, &p_dtb_spinlock);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dev_opr_spinlock_get");

	rte_spinlock_lock(&p_dtb_spinlock->spinlock);

	buff_size = zxdh_np_get_se_buff_size(opr) + sizeof(uint32_t);
	p_rsp_buff = rte_zmalloc(NULL, buff_size, 0);
	if (p_rsp_buff == NULL) {
		PMD_DRV_LOG(ERR, "malloc memory failed");
		rte_spinlock_unlock(&p_dtb_spinlock->spinlock);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	sub_type = (type == ZXDH_SE_STD_NIC_RES_TYPE) ?
		ZXDH_RES_STD_NIC_MSG : ZXDH_RES_OFFLOAD_MSG;

	rc = zxdh_np_agent_channel_se_res_get(dev_id, sub_type, opr, p_rsp_buff, buff_size);
	if (rc != ZXDH_OK) {
		rte_free(p_rsp_buff);
		PMD_DRV_LOG(ERR, "acl table res get fail rc=0x%x.", rc);
		rte_spinlock_unlock(&p_dtb_spinlock->spinlock);
		return ZXDH_ERR;
	}

	rte_spinlock_unlock(&p_dtb_spinlock->spinlock);

	p_acl_tbl = (ZXDH_NP_SE_ACL_TBL_T *)(p_rsp_buff + 1);
	zxdh_np_acl_tbl_set(p_acl_res_init, p_acl_tbl);
	rte_free(p_rsp_buff);

	return rc;
}

static uint32_t
zxdh_np_agent_stat_cfg_get(uint32_t dev_id,
					uint32_t type,
					ZXDH_APT_STAT_RES_INIT_T *p_stat_cfg_init)
{
	uint32_t rc = ZXDH_OK;
	uint32_t opr = ZXDH_STAT_CFG_REQ;
	uint32_t sub_type = ZXDH_RES_STD_NIC_MSG;
	uint32_t buff_size = 0;
	ZXDH_SPINLOCK_T *p_dtb_spinlock = NULL;
	uint32_t *p_rsp_buff = NULL;
	ZXDH_NP_SE_STAT_CFG_T *p_stat_cfg = NULL;
	ZXDH_DEV_SPINLOCK_TYPE_E spinlock = ZXDH_DEV_SPINLOCK_T_DTB;

	rc = zxdh_np_dev_opr_spinlock_get(dev_id, (uint32_t)spinlock, &p_dtb_spinlock);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dev_opr_spinlock_get");

	rte_spinlock_lock(&p_dtb_spinlock->spinlock);

	buff_size = zxdh_np_get_se_buff_size(opr) + sizeof(uint32_t);
	p_rsp_buff = rte_zmalloc(NULL, buff_size, 0);
	if (p_rsp_buff == NULL) {
		PMD_DRV_LOG(ERR, "malloc memory failed");
		rte_spinlock_unlock(&p_dtb_spinlock->spinlock);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	sub_type = (type == ZXDH_SE_STD_NIC_RES_TYPE) ? ZXDH_RES_STD_NIC_MSG : ZXDH_RES_OFFLOAD_MSG;

	rc = zxdh_np_agent_channel_se_res_get(dev_id, sub_type, opr, p_rsp_buff, buff_size);
	if (rc != ZXDH_OK) {
		rte_free(p_rsp_buff);
		PMD_DRV_LOG(ERR, "ddr table res get fail rc = 0x%x.", rc);
		rte_spinlock_unlock(&p_dtb_spinlock->spinlock);
		return ZXDH_ERR;
	}

	rte_spinlock_unlock(&p_dtb_spinlock->spinlock);

	p_stat_cfg = (ZXDH_NP_SE_STAT_CFG_T *)(p_rsp_buff + 1);
	zxdh_np_stat_cfg_set(p_stat_cfg_init, p_stat_cfg);
	rte_free(p_rsp_buff);

	return rc;
}

static void *
zxdh_np_dev_get_se_res_ptr(uint32_t dev_id, uint32_t type)
{
	ZXDH_DEV_MGR_T *p_dev_mgr = &g_dev_mgr;
	ZXDH_DEV_CFG_T *p_dev_info = p_dev_mgr->p_dev_array[ZXDH_DEV_SLOT_ID(dev_id)];

	if (type == ZXDH_SE_STD_NIC_RES_TYPE)
		return (void *)&p_dev_info->dev_apt_se_tbl_res.std_nic_res;
	else
		return (void *)&p_dev_info->dev_apt_se_tbl_res.offload_res;
}

static uint32_t
zxdh_np_agent_se_res_get(uint32_t dev_id, uint32_t type)
{
	uint32_t rc = ZXDH_OK;
	ZXDH_APT_SE_RES_T *p_se_res = NULL;
	ZXDH_APT_HASH_RES_INIT_T hash_res = {0};
	ZXDH_APT_ERAM_RES_INIT_T eram_res = {0};
	ZXDH_APT_ACL_RES_INIT_T acl_res = {0};

	p_se_res = (ZXDH_APT_SE_RES_T *)zxdh_np_dev_get_se_res_ptr(dev_id, type);
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_se_res);

	if (p_se_res->valid) {
		PMD_DRV_LOG(INFO, "dev_id [0x%x] res_type [%u] status ready", dev_id, type);
		return ZXDH_OK;
	}

	hash_res.func_res = p_se_res->hash_func;
	hash_res.bulk_res = p_se_res->hash_bulk;
	hash_res.tbl_res = p_se_res->hash_tbl;
	rc = zxdh_np_agent_hash_func_bulk_get(dev_id, type, &hash_res);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_agent_hash_func_bulk_get");

	rc = zxdh_np_agent_hash_tbl_get(dev_id, type, &hash_res);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_agent_hash_tbl_get");
	p_se_res->hash_func_num = hash_res.func_num;
	p_se_res->hash_bulk_num = hash_res.bulk_num;
	p_se_res->hash_tbl_num = hash_res.tbl_num;

	eram_res.eram_res = p_se_res->eram_tbl;
	rc = zxdh_np_agent_eram_tbl_get(dev_id, type, &eram_res);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_agent_eram_tbl_get");
	p_se_res->eram_num = eram_res.tbl_num;

	acl_res.acl_res = p_se_res->acl_tbl;
	rc = zxdh_np_agent_acl_tbl_get(dev_id, type, &acl_res);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_agent_acl_tbl_get");
	p_se_res->acl_num = acl_res.tbl_num;

	rc = zxdh_np_agent_stat_cfg_get(dev_id, type, &p_se_res->stat_cfg);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_agent_stat_cfg_get");

	p_se_res->valid = 1;
	return rc;
}

static uint32_t
zxdh_np_se_init_ex(uint32_t dev_id, ZXDH_SE_CFG *p_se_cfg)
{
	uint32_t i = 0;
	uint32_t j = 0;

	ZXDH_SE_ZBLK_CFG *p_zblk_cfg = NULL;
	ZXDH_SE_ZCELL_CFG *p_zcell_cfg = NULL;

	if (dpp_se_cfg[ZXDH_DEV_SLOT_ID(dev_id)] != NULL) {
		PMD_DRV_LOG(DEBUG, "SE global config is already initialized.");
		return ZXDH_OK;
	}

	memset(p_se_cfg, 0, sizeof(ZXDH_SE_CFG));

	p_se_cfg->dev_id = dev_id;
	dpp_se_cfg[ZXDH_DEV_SLOT_ID(p_se_cfg->dev_id)] = p_se_cfg;

	p_se_cfg->p_as_rslt_wrt_fun = NULL;
	p_se_cfg->p_client = ZXDH_COMM_VAL_TO_PTR(dev_id);

	for (i = 0; i < ZXDH_SE_ZBLK_NUM; i++) {
		p_zblk_cfg = ZXDH_SE_GET_ZBLK_CFG(p_se_cfg, i);

		p_zblk_cfg->zblk_idx = i;
		p_zblk_cfg->is_used  = 0;
		p_zblk_cfg->hash_arg = g_lpm_crc[i];
		p_zblk_cfg->zcell_bm = 0;
		zxdh_np_init_d_node(&p_zblk_cfg->zblk_dn, p_zblk_cfg);

		for (j = 0; j < ZXDH_SE_ZCELL_NUM; j++) {
			p_zcell_cfg = &p_zblk_cfg->zcell_info[j];

			p_zcell_cfg->zcell_idx = (i << 2) + j;
			p_zcell_cfg->item_used = 0;
			p_zcell_cfg->mask_len  = 0;

			zxdh_np_init_d_node(&p_zcell_cfg->zcell_dn, p_zcell_cfg);

			p_zcell_cfg->zcell_avl.p_key = p_zcell_cfg;
		}
	}

	return ZXDH_OK;
}

static uint32_t
zxdh_np_apt_hash_global_res_init(uint32_t dev_id)
{
	if (g_apt_se_cfg[ZXDH_DEV_SLOT_ID(dev_id)] == NULL) {
		g_apt_se_cfg[ZXDH_DEV_SLOT_ID(dev_id)] = rte_zmalloc(NULL, sizeof(ZXDH_SE_CFG), 0);
		if (g_apt_se_cfg[ZXDH_DEV_SLOT_ID(dev_id)] == NULL) {
			PMD_DRV_LOG(ERR, "malloc memory failed");
			return ZXDH_PAR_CHK_POINT_NULL;
		}

		zxdh_np_se_init_ex(dev_id, g_apt_se_cfg[ZXDH_DEV_SLOT_ID(dev_id)]);

		g_apt_se_cfg[ZXDH_DEV_SLOT_ID(dev_id)]->p_client = ZXDH_COMM_VAL_TO_PTR(dev_id);
	}

	return ZXDH_OK;
}

static void
se_apt_get_zblock_index(uint32_t zblock_bitmap, uint32_t *zblk_idx)
{
	uint32_t index0 = 0;
	uint32_t index1 = 0;

	for (index0 = 0; index0 < 32; index0++) {
		if ((zblock_bitmap >> index0) & 0x1) {
			*(zblk_idx + index1) = index0;
			index1++;
		}
	}
}

static uint32_t
zxdh_np_crc32_calc(uint8_t *p_input_key, uint32_t dw_byte_num, uint32_t dw_crc_poly)
{
	uint32_t dw_result = 0;
	uint32_t dw_data_type = 0;

	uint32_t i = 0;
	uint32_t j = 0;

	while (i < dw_byte_num) {
		dw_data_type = (uint32_t)((dw_result & 0xff000000) ^ (p_input_key[i] << 24));
		for (j = 0; j < 8; j++) {
			if (dw_data_type & 0x80000000) {
				dw_data_type <<= 1;
				dw_data_type ^= dw_crc_poly;
			} else {
				dw_data_type <<= 1;
			}
		}
		dw_result <<= 8;
		dw_result ^= dw_data_type;

		i++;
	}

	return dw_result;
}

static uint16_t
zxdh_np_crc16_calc(uint8_t *p_input_key, uint32_t dw_byte_num, uint16_t dw_crc_poly)
{
	uint16_t dw_result = 0;
	uint16_t dw_data_type = 0;

	uint32_t i = 0;
	uint32_t j = 0;

	while (i < dw_byte_num) {
		dw_data_type = (uint16_t)(((dw_result & 0xff00) ^ (p_input_key[i] << 8)) & 0xFFFF);
		for (j = 0; j < 8; j++) {
			if (dw_data_type & 0x8000) {
				dw_data_type <<= 1;
				dw_data_type ^= dw_crc_poly;
			} else {
				dw_data_type <<= 1;
			}
		}
		dw_result <<= 8;
		dw_result ^= dw_data_type;

		i++;
	}

	return dw_result;
}

static uint32_t
zxdh_np_se_fun_init(ZXDH_SE_CFG *p_se_cfg,
						   uint8_t	   id,
						   uint32_t	 fun_type)
{
	ZXDH_FUNC_ID_INFO  *p_fun_info = NULL;

	p_fun_info = ZXDH_GET_FUN_INFO(p_se_cfg, id);

	if (p_fun_info->is_used) {
		PMD_DRV_LOG(ERR, "Error[0x%x], fun_id [%u] is already used!",
			ZXDH_SE_RC_FUN_INVALID, id);
		return ZXDH_SE_RC_FUN_INVALID;
	}

	p_fun_info->fun_id = id;
	p_fun_info->is_used = 1;

	switch (fun_type) {
	case (ZXDH_FUN_HASH):
		p_fun_info->fun_type = ZXDH_FUN_HASH;
		p_fun_info->fun_ptr = rte_zmalloc(NULL, sizeof(ZXDH_HASH_CFG), 0);
		if (p_fun_info->fun_ptr == NULL) {
			PMD_DRV_LOG(ERR, "malloc memory failed");
			return ZXDH_PAR_CHK_POINT_NULL;
		}
		((ZXDH_HASH_CFG *)(p_fun_info->fun_ptr))->p_se_info = p_se_cfg;
		break;
	default:
		PMD_DRV_LOG(ERR, "Error, unrecgnized fun_type[ %u]", fun_type);
		RTE_ASSERT(0);
		return ZXDH_SE_RC_BASE;
	}

	return ZXDH_OK;
}

static int32_t
zxdh_np_hash_list_cmp(ZXDH_D_NODE *data1, ZXDH_D_NODE *data2, void *data)
{
	uint32_t flag = 0;
	uint32_t data_new = 0;
	uint32_t data_pre = 0;

	flag = *(uint32_t *)data;

	if (flag == ZXDH_HASH_CMP_ZCELL) {
		data_new = ((ZXDH_SE_ZCELL_CFG *)data1->data)->zcell_idx;
		data_pre = ((ZXDH_SE_ZCELL_CFG *)data2->data)->zcell_idx;
	} else if (flag == ZXDH_HASH_CMP_ZBLK) {
		data_new = ((ZXDH_SE_ZBLK_CFG *)data1->data)->zblk_idx;
		data_pre = ((ZXDH_SE_ZBLK_CFG *)data2->data)->zblk_idx;
	}

	if (data_new > data_pre)
		return 1;
	else if (data_new == data_pre)
		return 0;
	else
		return -1;
}

static int32_t
zxdh_np_hash_rb_key_cmp(void *p_new, void *p_old, __rte_unused uint32_t key_size)
{
	ZXDH_HASH_RBKEY_INFO *p_rbkey_new = NULL;
	ZXDH_HASH_RBKEY_INFO *p_rbkey_old = NULL;

	p_rbkey_new = (ZXDH_HASH_RBKEY_INFO *)(p_new);
	p_rbkey_old = (ZXDH_HASH_RBKEY_INFO *)(p_old);

	return memcmp(p_rbkey_new->key, p_rbkey_old->key, ZXDH_HASH_KEY_MAX);
}

static int32_t
zxdh_np_hash_ddr_cfg_rb_key_cmp(void *p_new, void *p_old, __rte_unused uint32_t key_size)
{
	HASH_DDR_CFG *p_rbkey_new = NULL;
	HASH_DDR_CFG *p_rbkey_old = NULL;

	p_rbkey_new = (HASH_DDR_CFG *)(p_new);
	p_rbkey_old = (HASH_DDR_CFG *)(p_old);

	return memcmp(&p_rbkey_new->ddr_baddr, &p_rbkey_old->ddr_baddr, sizeof(uint32_t));
}

static uint32_t
zxdh_np_hash_zcam_resource_init(ZXDH_HASH_CFG *p_hash_cfg,
		uint32_t zblk_num, uint32_t *zblk_idx_array)
{
	uint32_t rc = ZXDH_OK;

	uint32_t i = 0;
	uint32_t j = 0;
	uint32_t cmp_type = 0;
	uint32_t zblk_idx = 0;
	uint32_t zcell_idx = 0;
	uint32_t dev_id = 0;

	ZXDH_D_HEAD *p_zblk_list = NULL;
	ZXDH_D_HEAD *p_zcell_free = NULL;
	ZXDH_SE_ZBLK_CFG *p_zblk_cfg = NULL;
	ZXDH_SE_ZCELL_CFG *p_zcell_cfg = NULL;

	dev_id = p_hash_cfg->p_se_info->dev_id;

	p_zblk_list = &p_hash_cfg->hash_shareram.zblk_list;
	rc = zxdh_comm_double_link_init(ZXDH_SE_ZBLK_NUM, p_zblk_list);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_comm_double_link_init");

	p_zcell_free = &p_hash_cfg->hash_shareram.zcell_free_list;
	rc = zxdh_comm_double_link_init(ZXDH_SE_ZBLK_NUM * ZXDH_SE_ZCELL_NUM, p_zcell_free);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_comm_double_link_init");

	for (i = 0; i < zblk_num; i++) {
		zblk_idx = zblk_idx_array[i];

		p_zblk_cfg = ZXDH_SE_GET_ZBLK_CFG(p_hash_cfg->p_se_info, zblk_idx);

		if (p_zblk_cfg->is_used) {
			PMD_DRV_LOG(ERR, "ZBlock[%u] is already used", zblk_idx);
			RTE_ASSERT(0);
			return ZXDH_HASH_RC_INVALID_ZBLCK;
		}

		for (j = 0; j < ZXDH_SE_ZCELL_NUM; j++) {
			zcell_idx = p_zblk_cfg->zcell_info[j].zcell_idx;
			p_zcell_cfg = &(((ZXDH_SE_CFG *)(p_hash_cfg->p_se_info))->
			zblk_info[GET_ZBLK_IDX(zcell_idx)].zcell_info[GET_ZCELL_IDX(zcell_idx)]);

			if (p_zcell_cfg->is_used) {
				PMD_DRV_LOG(ERR, "ZBlk[%u], ZCell[%u] is already used",
					zblk_idx, zcell_idx);
				RTE_ASSERT(0);
				return ZXDH_HASH_RC_INVALID_ZCELL;
			}

			p_zcell_cfg->is_used = 1;

			cmp_type = ZXDH_HASH_CMP_ZCELL;
			rc = zxdh_comm_double_link_insert_sort(&p_zcell_cfg->zcell_dn,
				p_zcell_free, zxdh_np_hash_list_cmp, &cmp_type);
			ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_comm_double_link_insert_sort");
		}

		p_zblk_cfg->is_used = 1;
		cmp_type = ZXDH_HASH_CMP_ZBLK;
		rc = zxdh_comm_double_link_insert_sort(&p_zblk_cfg->zblk_dn,
			p_zblk_list, zxdh_np_hash_list_cmp, &cmp_type);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_comm_double_link_insert_last");
	}

	return rc;
}

static uint32_t
zxdh_np_hash_init(ZXDH_SE_CFG *p_se_cfg,
				uint32_t fun_id,
				uint32_t zblk_num,
				uint32_t *zblk_idx,
				uint32_t ddr_dis)
{
	uint32_t rc = ZXDH_OK;
	uint32_t i = 0;
	uint32_t dev_id = p_se_cfg->dev_id;
	ZXDH_FUNC_ID_INFO *p_func_info = NULL;
	ZXDH_HASH_CFG *p_hash_cfg = NULL;

	rc = zxdh_np_se_fun_init(p_se_cfg, (fun_id & 0xff), ZXDH_FUN_HASH);
	if (rc == ZXDH_SE_RC_FUN_INVALID)
		return ZXDH_OK;

	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_se_fun_init");

	p_func_info = ZXDH_GET_FUN_INFO(p_se_cfg, fun_id);
	p_hash_cfg = (ZXDH_HASH_CFG *)p_func_info->fun_ptr;
	p_hash_cfg->fun_id = fun_id;
	p_hash_cfg->p_hash32_fun = zxdh_np_crc32_calc;
	p_hash_cfg->p_hash16_fun = zxdh_np_crc16_calc;
	p_hash_cfg->p_se_info = p_se_cfg;

	if (ddr_dis == 1)
		p_hash_cfg->ddr_valid = 0;
	else
		p_hash_cfg->ddr_valid = 1;

	p_hash_cfg->hash_stat.zblock_num = zblk_num;
	rc = zxdh_np_hash_zcam_resource_init(p_hash_cfg, zblk_num, zblk_idx);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_hash_zcam_resource_init");

	for (i = 0; i < zblk_num; i++)
		p_hash_cfg->hash_stat.zblock_array[i] = zblk_idx[i];

	rc = (uint32_t)zxdh_comm_rb_init(&p_hash_cfg->hash_rb,
								 0,
								 sizeof(ZXDH_HASH_RBKEY_INFO),
								 zxdh_np_hash_rb_key_cmp);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_comm_rb_init");

	rc = (uint32_t)zxdh_comm_rb_init(&p_hash_cfg->ddr_cfg_rb,
								 0,
								 sizeof(HASH_DDR_CFG),
								 zxdh_np_hash_ddr_cfg_rb_key_cmp);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_comm_rb_init");

	for (i = 0; i < zblk_num; i++)
		g_hash_zblk_idx[ZXDH_DEV_SLOT_ID(dev_id)][fun_id][i] = zblk_idx[i];

	return rc;
}

static uint32_t
zxdh_np_apt_hash_func_res_init(uint32_t dev_id, uint32_t func_num,
		ZXDH_APT_HASH_FUNC_RES_T *p_hash_func_res)
{
	uint32_t rc = ZXDH_OK;
	uint32_t index = 0;
	uint32_t zblk_idx[32] = {0};
	ZXDH_APT_HASH_FUNC_RES_T *p_hash_func_res_temp = NULL;
	ZXDH_SE_CFG *p_se_cfg = NULL;

	p_se_cfg = g_apt_se_cfg[ZXDH_DEV_SLOT_ID(dev_id)];

	for (index = 0; index < func_num; index++) {
		memset(zblk_idx, 0, sizeof(zblk_idx));
		p_hash_func_res_temp = p_hash_func_res + index;
		ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_hash_func_res_temp);

		se_apt_get_zblock_index(p_hash_func_res_temp->zblk_bitmap, zblk_idx);

		rc = zxdh_np_hash_init(p_se_cfg,
							p_hash_func_res_temp->func_id,
							p_hash_func_res_temp->zblk_num,
							zblk_idx,
							p_hash_func_res_temp->ddr_dis);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_hash_init");
	}

	return rc;
}

static uint32_t
zxdh_np_hash_bulk_init(ZXDH_SE_CFG *p_se_cfg,
						uint32_t fun_id,
						uint32_t bulk_id,
						ZXDH_HASH_DDR_RESC_CFG_T *p_ddr_resc_cfg,
						uint32_t zcell_num,
						uint32_t zreg_num)
{
	uint32_t rc = ZXDH_OK;

	uint32_t i = 0;
	uint32_t j = 0;
	uint32_t zblk_idx = 0;
	uint32_t dev_id = p_se_cfg->dev_id;
	uint32_t ddr_item_num = 0;

	ZXDH_D_NODE *p_zblk_dn = NULL;
	ZXDH_D_NODE *p_zcell_dn = NULL;
	ZXDH_RB_TN *p_rb_tn_new = NULL;
	ZXDH_RB_TN *p_rb_tn_rtn = NULL;
	ZXDH_SE_ZBLK_CFG *p_zblk_cfg = NULL;
	ZXDH_SE_ZREG_CFG *p_zreg_cfg = NULL;
	HASH_DDR_CFG *p_ddr_cfg = NULL;
	HASH_DDR_CFG *p_rbkey_new = NULL;
	HASH_DDR_CFG *p_rbkey_rtn = NULL;
	ZXDH_SE_ZCELL_CFG *p_zcell_cfg = NULL;
	ZXDH_HASH_CFG *p_hash_cfg = NULL;
	ZXDH_FUNC_ID_INFO *p_func_info = NULL;
	ZXDH_SE_ITEM_CFG **p_item_array = NULL;
	ZXDH_HASH_BULK_ZCAM_STAT *p_bulk_zcam_mono = NULL;

	p_func_info = ZXDH_GET_FUN_INFO(p_se_cfg, fun_id);
	p_hash_cfg = (ZXDH_HASH_CFG *)p_func_info->fun_ptr;
	if (p_hash_cfg == NULL) {
		PMD_DRV_LOG(ERR, "p_hash_cfg point null!");
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	if (p_hash_cfg->hash_stat.p_bulk_zcam_mono[bulk_id] != NULL) {
		PMD_DRV_LOG(ERR, "fun_id[%u] bulk_id[%u] is already init", fun_id, bulk_id);
		return ZXDH_OK;
	}

	PMD_DRV_LOG(DEBUG, "p_hash_cfg->ddr_valid = %u!", p_hash_cfg->ddr_valid);
	if (p_hash_cfg->ddr_valid == 1) {
		ddr_item_num = p_ddr_resc_cfg->ddr_item_num;
		if (ZXDH_DDR_WIDTH_512b == p_ddr_resc_cfg->ddr_width_mode)
			ddr_item_num = p_ddr_resc_cfg->ddr_item_num >> 1;

		p_rbkey_new = rte_zmalloc(NULL, sizeof(HASH_DDR_CFG), 0);
		if (p_rbkey_new == NULL) {
			PMD_DRV_LOG(ERR, "malloc memory failed");
			return ZXDH_PAR_CHK_POINT_NULL;
		}

		p_rbkey_new->ddr_baddr = p_ddr_resc_cfg->ddr_baddr;

		p_rb_tn_new = rte_zmalloc(NULL, sizeof(ZXDH_RB_TN), 0);
		if (p_rb_tn_new == NULL) {
			rte_free(p_rbkey_new);
			PMD_DRV_LOG(ERR, "malloc memory failed");
			return ZXDH_PAR_CHK_POINT_NULL;
		}
		zxdh_np_init_rbt_tn(p_rb_tn_new, p_rbkey_new);

		rc = zxdh_comm_rb_insert(&p_hash_cfg->ddr_cfg_rb,
			(void *)p_rb_tn_new, (void *)(&p_rb_tn_rtn));
		if (rc == ZXDH_RBT_RC_FULL) {
			PMD_DRV_LOG(ERR, "The red black tree is full!");
			rte_free(p_rbkey_new);
			rte_free(p_rb_tn_new);
			RTE_ASSERT(0);
			return ZXDH_HASH_RC_RB_TREE_FULL;
		} else if (rc == ZXDH_RBT_RC_UPDATE) {
			PMD_DRV_LOG(DEBUG, "some bulk_id share one bulk!");

			ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_rb_tn_rtn);
			p_rbkey_rtn = (HASH_DDR_CFG *)(p_rb_tn_rtn->p_key);

			p_ddr_cfg = p_hash_cfg->p_bulk_ddr_info[p_rbkey_rtn->bulk_id];

			if (p_ddr_cfg->hash_ddr_arg !=
			GET_DDR_HASH_ARG(p_ddr_resc_cfg->ddr_crc_sel) ||
				p_ddr_cfg->width_mode   != p_ddr_resc_cfg->ddr_width_mode ||
				p_ddr_cfg->ddr_ecc_en   != p_ddr_resc_cfg->ddr_ecc_en	 ||
				p_ddr_cfg->item_num	 != ddr_item_num) {
				PMD_DRV_LOG(ERR, "The base address is same");
				rte_free(p_rbkey_new);
				rte_free(p_rb_tn_new);
				RTE_ASSERT(0);
				return ZXDH_HASH_RC_INVALID_PARA;
			}

			p_hash_cfg->p_bulk_ddr_info[bulk_id] =
				p_hash_cfg->p_bulk_ddr_info[p_rbkey_rtn->bulk_id];

			rte_free(p_rbkey_new);
			rte_free(p_rb_tn_new);
		} else {
			p_item_array = rte_zmalloc(NULL, ddr_item_num *
				sizeof(ZXDH_SE_ITEM_CFG *), 0);
			if (NULL == (p_item_array)) {
				rte_free(p_rbkey_new);
				rte_free(p_rb_tn_new);
				PMD_DRV_LOG(ERR, "malloc memory failed");
				return ZXDH_PAR_CHK_POINT_NULL;
			}

			p_rbkey_new->p_item_array = p_item_array;
			p_rbkey_new->bulk_id = bulk_id;
			p_rbkey_new->hw_baddr = 0;
			p_rbkey_new->width_mode = p_ddr_resc_cfg->ddr_width_mode;
			p_rbkey_new->item_num = ddr_item_num;
			p_rbkey_new->ddr_ecc_en = p_ddr_resc_cfg->ddr_ecc_en;
			p_rbkey_new->hash_ddr_arg = GET_DDR_HASH_ARG(p_ddr_resc_cfg->ddr_crc_sel);
			p_rbkey_new->bulk_use = 1;
			p_rbkey_new->zcell_num = zcell_num;
			p_rbkey_new->zreg_num = zreg_num;
			p_hash_cfg->p_bulk_ddr_info[bulk_id] = p_rbkey_new;

			PMD_DRV_LOG(DEBUG, "one new ddr_bulk init!");
		}
	}

	p_bulk_zcam_mono = rte_zmalloc(NULL, sizeof(ZXDH_HASH_BULK_ZCAM_STAT), 0);
	if (NULL == (p_bulk_zcam_mono)) {
		rte_free(p_rbkey_new);
		rte_free(p_rb_tn_new);
		rte_free(p_item_array);
		PMD_DRV_LOG(ERR, "malloc memory failed");
		return ZXDH_PAR_CHK_POINT_NULL;
	}
	(&p_hash_cfg->hash_stat)->p_bulk_zcam_mono[bulk_id] = p_bulk_zcam_mono;

	for (i = 0; i < ZXDH_SE_ZBLK_NUM * ZXDH_SE_ZCELL_NUM; i++)
		p_bulk_zcam_mono->zcell_mono_idx[i] = 0xffffffff;

	for (i = 0; i < ZXDH_SE_ZBLK_NUM; i++) {
		for (j = 0; j < ZXDH_SE_ZREG_NUM; j++) {
			p_bulk_zcam_mono->zreg_mono_id[i][j].zblk_id = 0xffffffff;
			p_bulk_zcam_mono->zreg_mono_id[i][j].zreg_id = 0xffffffff;
		}
	}

	if (zcell_num > 0) {
		p_hash_cfg->bulk_ram_mono[bulk_id] = 1;

		p_zcell_dn = p_hash_cfg->hash_shareram.zcell_free_list.p_next;

		i = 0;

		while (p_zcell_dn) {
			p_zcell_cfg = (ZXDH_SE_ZCELL_CFG *)p_zcell_dn->data;

			if (p_zcell_cfg->is_used) {
				if (!(p_zcell_cfg->flag & ZXDH_ZCELL_FLAG_IS_MONO)) {
					p_zcell_cfg->flag	 |= ZXDH_ZCELL_FLAG_IS_MONO;
					p_zcell_cfg->bulk_id = bulk_id;

					p_bulk_zcam_mono->zcell_mono_idx[p_zcell_cfg->zcell_idx] =
						p_zcell_cfg->zcell_idx;

					if (++i >= zcell_num)
						break;
				}
			} else {
				PMD_DRV_LOG(ERR, "zcell[ %u ] is not init", p_zcell_cfg->zcell_idx);
				RTE_ASSERT(0);
				return ZXDH_HASH_RC_INVALID_PARA;
			}

			p_zcell_dn = p_zcell_dn->next;
		}

		if (i < zcell_num)
			PMD_DRV_LOG(ERR, "Input zcell_num is %u, bulk %u monopolize zcells is %u",
				zcell_num, bulk_id, i);
	}

	if (zreg_num > 0) {
		p_hash_cfg->bulk_ram_mono[bulk_id] = 1;

		p_zblk_dn = p_hash_cfg->hash_shareram.zblk_list.p_next;
		j = 0;

		while (p_zblk_dn) {
			p_zblk_cfg = (ZXDH_SE_ZBLK_CFG *)p_zblk_dn->data;
			zblk_idx = p_zblk_cfg->zblk_idx;

			if (p_zblk_cfg->is_used) {
				for (i = 0; i < ZXDH_SE_ZREG_NUM; i++) {
					p_zreg_cfg = &p_zblk_cfg->zreg_info[i];

					if (!(p_zreg_cfg->flag & ZXDH_ZREG_FLAG_IS_MONO)) {
						p_zreg_cfg->flag = ZXDH_ZREG_FLAG_IS_MONO;
						p_zreg_cfg->bulk_id = bulk_id;

					p_bulk_zcam_mono->zreg_mono_id[zblk_idx][i].zblk_id =
						zblk_idx;
					p_bulk_zcam_mono->zreg_mono_id[zblk_idx][i].zreg_id = i;

					if (++j >= zreg_num)
						break;
					}
				}

				if (j >= zreg_num)
					break;
			} else {
				PMD_DRV_LOG(ERR, "zblk [ %u ] is not init", p_zblk_cfg->zblk_idx);
				RTE_ASSERT(0);
				return ZXDH_HASH_RC_INVALID_PARA;
			}

			p_zblk_dn = p_zblk_dn->next;
		}

		if (j < zreg_num)
			PMD_DRV_LOG(ERR, "Input zreg_num' is %u, actually bulk %u monopolize zregs is %u",
				zreg_num, bulk_id, j);
	}

	return ZXDH_OK;
}

static uint32_t
zxdh_np_apt_hash_bulk_res_init(uint32_t dev_id, uint32_t bulk_num,
						   ZXDH_APT_HASH_BULK_RES_T *p_bulk_res)
{
	uint32_t rc = ZXDH_OK;
	uint32_t index = 0;
	ZXDH_APT_HASH_BULK_RES_T *p_hash_bulk_res_temp = NULL;
	ZXDH_HASH_DDR_RESC_CFG_T ddr_resc_cfg = {0};
	ZXDH_SE_CFG *p_se_cfg = NULL;

	p_se_cfg = g_apt_se_cfg[ZXDH_DEV_SLOT_ID(dev_id)];

	for (index = 0; index < bulk_num; index++) {
		memset(&ddr_resc_cfg, 0, sizeof(ZXDH_HASH_DDR_RESC_CFG_T));
		p_hash_bulk_res_temp = p_bulk_res + index;
		ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_hash_bulk_res_temp);

		ddr_resc_cfg.ddr_baddr = p_hash_bulk_res_temp->ddr_baddr;
		ddr_resc_cfg.ddr_item_num = p_hash_bulk_res_temp->ddr_item_num;
		ddr_resc_cfg.ddr_width_mode = p_hash_bulk_res_temp->ddr_width_mode;
		ddr_resc_cfg.ddr_crc_sel = p_hash_bulk_res_temp->ddr_crc_sel;
		ddr_resc_cfg.ddr_ecc_en = p_hash_bulk_res_temp->ddr_ecc_en;

		rc = zxdh_np_hash_bulk_init(p_se_cfg,
							p_hash_bulk_res_temp->func_id,
							p_hash_bulk_res_temp->bulk_id,
							&ddr_resc_cfg,
							p_hash_bulk_res_temp->zcell_num,
							p_hash_bulk_res_temp->zreg_num);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_hash_bulk_init");
	}

	return rc;
}

static uint32_t
zxdh_np_apt_set_callback(uint32_t dev_id, uint32_t sdt_no, uint32_t table_type, void *p_data)
{
	SE_APT_CALLBACK_T *apt_func = NULL;

	apt_func = &g_apt_se_callback[ZXDH_DEV_SLOT_ID(dev_id)][sdt_no];

	apt_func->sdt_no = sdt_no;
	apt_func->table_type = table_type;
	switch (table_type) {
	case ZXDH_SDT_TBLT_ERAM:
		apt_func->se_func_info.eram_func.opr_mode =
			((ZXDH_APT_ERAM_TABLE_T *)p_data)->opr_mode;
		apt_func->se_func_info.eram_func.rd_mode =
			((ZXDH_APT_ERAM_TABLE_T *)p_data)->rd_mode;
		apt_func->se_func_info.eram_func.eram_set_func =
			((ZXDH_APT_ERAM_TABLE_T *)p_data)->eram_set_func;
		apt_func->se_func_info.eram_func.eram_get_func =
			((ZXDH_APT_ERAM_TABLE_T *)p_data)->eram_get_func;
		break;
	case ZXDH_SDT_TBLT_HASH:
		apt_func->se_func_info.hash_func.sdt_partner   =
			((ZXDH_APT_HASH_TABLE_T *)p_data)->sdt_partner;
		apt_func->se_func_info.hash_func.hash_set_func =
			((ZXDH_APT_HASH_TABLE_T *)p_data)->hash_set_func;
		apt_func->se_func_info.hash_func.hash_get_func =
		((ZXDH_APT_HASH_TABLE_T *)p_data)->hash_get_func;
		break;
	case ZXDH_SDT_TBLT_ETCAM:
		apt_func->se_func_info.acl_func.sdt_partner =
			((ZXDH_APT_ACL_TABLE_T *)p_data)->sdt_partner;
		apt_func->se_func_info.acl_func.acl_set_func =
			((ZXDH_APT_ACL_TABLE_T *)p_data)->acl_set_func;
		apt_func->se_func_info.acl_func.acl_get_func =
			((ZXDH_APT_ACL_TABLE_T *)p_data)->acl_get_func;
		break;
	default:
		PMD_DRV_LOG(ERR, "table_type[ %u ] is invalid!", table_type);
		return ZXDH_ERR;
	}

	return ZXDH_OK;
}

static uint32_t
zxdh_np_hash_tbl_id_info_init(ZXDH_SE_CFG *p_se_cfg,
								uint32_t fun_id,
								uint32_t tbl_id,
								uint32_t tbl_flag,
								uint32_t key_type,
								uint32_t actu_key_size)
{
	uint32_t key_by_size = ZXDH_GET_KEY_SIZE(actu_key_size);
	uint32_t entry_size = ZXDH_GET_HASH_ENTRY_SIZE(key_type);
	uint32_t dev_id = p_se_cfg->dev_id;
	ZXDH_HASH_TBL_ID_INFO *p_tbl_id_info = NULL;

	if (key_by_size > entry_size) {
		PMD_DRV_LOG(ERR, "ErrorCode[%x]: actu_key_size[%u] not match to key_type[%u].",
								 ZXDH_HASH_RC_INVALID_PARA,
								 key_by_size,
								 entry_size);
		RTE_ASSERT(0);
		return ZXDH_HASH_RC_INVALID_PARA;
	}

	p_tbl_id_info = GET_HASH_TBL_ID_INFO(dev_id, fun_id, tbl_id);

	if (p_tbl_id_info->is_init) {
		PMD_DRV_LOG(ERR, "fun_id[%u], table_id[%u] is already init", fun_id, tbl_id);
		return ZXDH_OK;
	}

	p_tbl_id_info->fun_id = fun_id;
	p_tbl_id_info->actu_key_size = actu_key_size;
	p_tbl_id_info->key_type = key_type;
	p_tbl_id_info->is_init = 1;

	if (tbl_flag & ZXDH_HASH_TBL_FLAG_AGE)
		p_tbl_id_info->is_age = 1;

	if (tbl_flag & ZXDH_HASH_TBL_FLAG_LEARN)
		p_tbl_id_info->is_lrn = 1;

	if (tbl_flag & ZXDH_HASH_TBL_FLAG_MC_WRT)
		p_tbl_id_info->is_mc_wrt = 1;

	return ZXDH_OK;
}

static uint32_t
zxdh_np_apt_hash_tbl_res_init(uint32_t dev_id, uint32_t tbl_num,
						  ZXDH_APT_HASH_TABLE_T *p_hash_tbl)
{
	uint32_t rc = ZXDH_OK;
	uint32_t index = 0;
	ZXDH_APT_HASH_TABLE_T *p_hash_tbl_temp = NULL;
	ZXDH_SE_CFG *p_se_cfg = NULL;

	p_se_cfg = g_apt_se_cfg[ZXDH_DEV_SLOT_ID(dev_id)];

	for (index = 0; index < tbl_num; index++) {
		p_hash_tbl_temp = p_hash_tbl + index;
		ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_hash_tbl_temp);
		rc = zxdh_np_sdt_tbl_write(dev_id,
					p_hash_tbl_temp->sdt_no,
					p_hash_tbl_temp->hash_sdt.table_type,
					&p_hash_tbl_temp->hash_sdt,
					ZXDH_SDT_OPER_ADD);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_sdt_tbl_write");

		rc = zxdh_np_hash_tbl_id_info_init(p_se_cfg,
						p_hash_tbl_temp->hash_sdt.hash_id,
						p_hash_tbl_temp->hash_sdt.hash_table_id,
						p_hash_tbl_temp->tbl_flag,
						p_hash_tbl_temp->hash_sdt.hash_table_width,
						p_hash_tbl_temp->hash_sdt.key_size);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_hash_tbl_id_info_init_ex");

		rc = zxdh_np_apt_set_callback(dev_id,
						p_hash_tbl_temp->sdt_no,
						p_hash_tbl_temp->hash_sdt.table_type,
						(void *)p_hash_tbl_temp);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_apt_set_callback");
	}

	return rc;
}

static uint32_t
zxdh_np_apt_eram_res_init(uint32_t dev_id, uint32_t tbl_num, ZXDH_APT_ERAM_TABLE_T *p_eram_tbl)
{
	uint32_t rc = ZXDH_OK;
	uint32_t index = 0;
	ZXDH_APT_ERAM_TABLE_T *p_temp_eram_tbl = NULL;

	for (index = 0; index < tbl_num; index++) {
		p_temp_eram_tbl = p_eram_tbl + index;
		rc = zxdh_np_sdt_tbl_write(dev_id,
						p_temp_eram_tbl->sdt_no,
						p_temp_eram_tbl->eram_sdt.table_type,
						&p_temp_eram_tbl->eram_sdt,
						ZXDH_SDT_OPER_ADD);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_sdt_tbl_write");

		rc = zxdh_np_apt_set_callback(dev_id,
						p_temp_eram_tbl->sdt_no,
						p_temp_eram_tbl->eram_sdt.table_type,
						(void *)p_temp_eram_tbl);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_apt_set_callback");
	}

	return rc;
}

static uint32_t
zxdh_np_acl_entrynum_to_blocknum(uint32_t entry_num, uint32_t key_mode)
{
	uint32_t value = 0;

	value = entry_num % (ZXDH_ETCAM_RAM_DEPTH * ((uint32_t)1 << key_mode));

	if (value == 0)
		return (entry_num / (ZXDH_ETCAM_RAM_DEPTH * ((uint32_t)1 << key_mode)));
	else
		return (entry_num / (ZXDH_ETCAM_RAM_DEPTH * ((uint32_t)1 << key_mode)) + 1);
}

static int32_t
zxdh_np_acl_key_cmp(void *p_new_key, void *p_old_key, uint32_t key_len)
{
	return memcmp(&(((ZXDH_ACL_KEY_INFO_T *)p_new_key)->pri),
		&(((ZXDH_ACL_KEY_INFO_T *)p_old_key)->pri), key_len - sizeof(uint32_t));
}

static uint32_t
zxdh_np_acl_cfg_init_ex(ZXDH_ACL_CFG_EX_T *p_acl_cfg,
						void *p_client,
						uint32_t flags,
						ZXDH_ACL_AS_RSLT_WRT_FUNCTION p_as_wrt_fun)
{
	uint32_t rc = 0;

	memset(p_acl_cfg, 0, sizeof(ZXDH_ACL_CFG_EX_T));

	p_acl_cfg->p_client = p_client;
	p_acl_cfg->dev_id = (uint32_t)(ZXDH_COMM_PTR_TO_VAL(p_acl_cfg->p_client) & 0xFFFFFFFF);
	p_acl_cfg->flags = flags;

	g_p_acl_ex_cfg[ZXDH_DEV_SLOT_ID(p_acl_cfg->dev_id)] = p_acl_cfg;

	if (flags & ZXDH_ACL_FLAG_ETCAM0_EN) {
		p_acl_cfg->acl_etcamids.is_valid = 1;

		p_acl_cfg->acl_etcamids.as_eram_base = 0;

		rc = zxdh_comm_double_link_init(ZXDH_ACL_TBL_ID_NUM,
			&p_acl_cfg->acl_etcamids.tbl_list);
		ZXDH_COMM_CHECK_RC(rc, "zxdh_comm_double_link_init");
	}

	if (p_as_wrt_fun == NULL) {
		p_acl_cfg->p_as_rslt_write_fun = NULL;
		p_acl_cfg->p_as_rslt_read_fun = NULL;

	} else {
		p_acl_cfg->p_as_rslt_write_fun = p_as_wrt_fun;
	}

	return rc;
}

static uint32_t
zxdh_np_acl_tbl_init_ex(ZXDH_ACL_CFG_EX_T *p_acl_cfg,
						uint32_t table_id,
						uint32_t as_enable,
						uint32_t entry_num,
						ZXDH_ACL_PRI_MODE_E pri_mode,
						uint32_t key_mode,
						ZXDH_ACL_AS_MODE_E as_mode,
						uint32_t as_baddr,
						uint32_t block_num,
						uint32_t *p_block_idx)
{
	uint32_t rc = 0;
	uint32_t i = 0;

	g_p_acl_ex_cfg[ZXDH_DEV_SLOT_ID(p_acl_cfg->dev_id)] = p_acl_cfg;

	if (p_acl_cfg->acl_tbls[table_id].is_used) {
		PMD_DRV_LOG(ERR, "table_id[ %u ] is already used!", table_id);
		RTE_ASSERT(0);
		return ZXDH_ACL_RC_INVALID_TBLID;
	}

	if (!p_acl_cfg->acl_etcamids.is_valid) {
		PMD_DRV_LOG(ERR, "etcam is not init!");
		RTE_ASSERT(0);
		return ZXDH_ACL_RC_ETCAMID_NOT_INIT;
	}

	if (zxdh_np_acl_entrynum_to_blocknum(entry_num, key_mode) > block_num) {
		PMD_DRV_LOG(ERR, "key_mode %u, etcam block_num %u is not enough for entry_num 0x%x",
							 key_mode, block_num, entry_num);
		RTE_ASSERT(0);
		return ZXDH_ACL_RC_INVALID_BLOCKNUM;
	} else if (zxdh_np_acl_entrynum_to_blocknum(entry_num, key_mode) < block_num) {
		PMD_DRV_LOG(DEBUG, "key_mode %u, etcam block_num %u is more than entry_num 0x%x",
							 key_mode, block_num, entry_num);
	} else {
		PMD_DRV_LOG(DEBUG, "key_mode %u, etcam block_num %u is match with entry_num 0x%x",
							 key_mode, block_num, entry_num);
	}

	p_acl_cfg->acl_tbls[table_id].as_enable = as_enable;

	if (as_enable) {
		p_acl_cfg->acl_tbls[table_id].as_idx_base = as_baddr;
		p_acl_cfg->acl_tbls[table_id].as_rslt_buff =
			rte_zmalloc(NULL, entry_num * ZXDH_ACL_AS_RSLT_SIZE_GET_EX(as_mode), 0);
		if (p_acl_cfg->acl_tbls[table_id].as_rslt_buff == NULL) {
			PMD_DRV_LOG(ERR, "malloc memory failed");
			return ZXDH_PAR_CHK_POINT_NULL;
		}
	}

	rc = (uint32_t)zxdh_comm_rb_init(&p_acl_cfg->acl_tbls[table_id].acl_rb, 0,
		(sizeof(ZXDH_ACL_KEY_INFO_T) & 0xFFFFFFFFU) + ZXDH_ACL_KEYSIZE_GET(key_mode),
		zxdh_np_acl_key_cmp);
	ZXDH_COMM_CHECK_RC(rc, "zxdh_comm_rb_init");

	p_acl_cfg->acl_tbls[table_id].table_id = table_id;
	p_acl_cfg->acl_tbls[table_id].pri_mode = pri_mode;
	p_acl_cfg->acl_tbls[table_id].key_mode = key_mode;
	p_acl_cfg->acl_tbls[table_id].entry_num = entry_num;
	p_acl_cfg->acl_tbls[table_id].as_mode = as_mode;
	p_acl_cfg->acl_tbls[table_id].is_used = 1;

	zxdh_np_init_d_node(&p_acl_cfg->acl_tbls[table_id].entry_dn,
		&p_acl_cfg->acl_tbls[table_id]);
	rc = zxdh_comm_double_link_insert_last(&p_acl_cfg->acl_tbls[table_id].entry_dn,
		&p_acl_cfg->acl_etcamids.tbl_list);
	ZXDH_COMM_CHECK_RC(rc, "zxdh_comm_double_link_insert_last");

	p_acl_cfg->acl_tbls[table_id].block_num = block_num;
	p_acl_cfg->acl_tbls[table_id].block_array =
		rte_zmalloc(NULL, block_num * sizeof(uint32_t), 0);
	if (p_acl_cfg->acl_tbls[table_id].block_array == NULL) {
		PMD_DRV_LOG(ERR, "malloc memory failed");
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	for (i = 0; i < block_num; i++) {
		if (p_acl_cfg->acl_blocks[p_block_idx[i]].is_used) {
			PMD_DRV_LOG(ERR, "the block[ %u ] is already used by table[ %u ]!",
				p_block_idx[i], p_acl_cfg->acl_blocks[p_block_idx[i]].tbl_id);
			RTE_ASSERT(0);
			return ZXDH_ACL_RC_INVALID_BLOCKID;
		}

		p_acl_cfg->acl_tbls[table_id].block_array[i] = p_block_idx[i];
		p_acl_cfg->acl_blocks[p_block_idx[i]].is_used = 1;
		p_acl_cfg->acl_blocks[p_block_idx[i]].tbl_id = table_id;
		p_acl_cfg->acl_blocks[p_block_idx[i]].idx_base =
			((ZXDH_ACL_ENTRY_MAX_GET(key_mode, i)) >> ZXDH_BLOCK_IDXBASE_BIT_OFF) &
				ZXDH_BLOCK_IDXBASE_BIT_MASK;
	}

	return ZXDH_OK;
}

static uint32_t
zxdh_np_apt_acl_res_init(uint32_t dev_id, uint32_t tbl_num, ZXDH_APT_ACL_TABLE_T *p_acl_tbl_res)
{
	uint32_t rc = ZXDH_OK;
	uint8_t index = 0;
	ZXDH_APT_ACL_TABLE_T *p_temp_acl_tbl = NULL;

	rc = zxdh_np_acl_cfg_init_ex(&g_apt_acl_cfg[ZXDH_DEV_SLOT_ID(dev_id)],
						(void *)ZXDH_COMM_VAL_TO_PTR(dev_id),
						ZXDH_ACL_FLAG_ETCAM0_EN,
						NULL);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_acl_cfg_init_ex");

	for (index = 0; index < tbl_num; index++) {
		p_temp_acl_tbl = p_acl_tbl_res + index;
		rc = zxdh_np_sdt_tbl_write(dev_id,
						p_temp_acl_tbl->sdt_no,
						p_temp_acl_tbl->acl_sdt.table_type,
						&p_temp_acl_tbl->acl_sdt,
						ZXDH_SDT_OPER_ADD);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_sdt_tbl_write");

		 rc = zxdh_np_acl_tbl_init_ex(&g_apt_acl_cfg[ZXDH_DEV_SLOT_ID(dev_id)],
					p_temp_acl_tbl->acl_sdt.etcam_table_id,
					p_temp_acl_tbl->acl_sdt.as_en,
					p_temp_acl_tbl->acl_res.entry_num,
					p_temp_acl_tbl->acl_res.pri_mode,
					p_temp_acl_tbl->acl_sdt.etcam_key_mode,
					p_temp_acl_tbl->acl_sdt.as_rsp_mode,
					p_temp_acl_tbl->acl_sdt.as_eram_baddr,
					p_temp_acl_tbl->acl_res.block_num,
					p_temp_acl_tbl->acl_res.block_index);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_acl_tbl_init_ex");

		rc = zxdh_np_apt_set_callback(dev_id,
						p_temp_acl_tbl->sdt_no,
						p_temp_acl_tbl->acl_sdt.table_type,
						(void *)p_temp_acl_tbl);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_apt_set_callback");
	}

	return rc;
}

static void
zxdh_np_apt_stat_res_init(uint32_t dev_id, uint32_t type, ZXDH_APT_STAT_RES_INIT_T *stat_res_init)
{
	g_ppu_stat_cfg[ZXDH_DEV_SLOT_ID(dev_id)].eram_baddr = stat_res_init->eram_baddr;
	g_ppu_stat_cfg[ZXDH_DEV_SLOT_ID(dev_id)].eram_depth = stat_res_init->eram_depth;

	if (type == ZXDH_SE_NON_STD_NIC_RES_TYPE) {
		g_ppu_stat_cfg[ZXDH_DEV_SLOT_ID(dev_id)].ddr_base_addr = stat_res_init->ddr_baddr;
		g_ppu_stat_cfg[ZXDH_DEV_SLOT_ID(dev_id)].ppu_addr_offset =
			stat_res_init->ppu_ddr_offset;
	}
}

static uint32_t
zxdh_np_se_res_init(uint32_t dev_id, uint32_t type)
{
	uint32_t rc = ZXDH_OK;
	ZXDH_APT_SE_RES_T *p_se_res = NULL;
	ZXDH_APT_HASH_RES_INIT_T hash_res_init = {0};
	ZXDH_APT_ERAM_RES_INIT_T eram_res_init = {0};
	ZXDH_APT_ACL_RES_INIT_T acl_res_init = {0};

	p_se_res = (ZXDH_APT_SE_RES_T *)zxdh_np_dev_get_se_res_ptr(dev_id, type);
	if (!p_se_res->valid) {
		PMD_DRV_LOG(ERR, "dev_id[0x%x],se_type[0x%x] invalid!", dev_id, type);
		return ZXDH_ERR;
	}

	hash_res_init.func_num = p_se_res->hash_func_num;
	hash_res_init.bulk_num = p_se_res->hash_bulk_num;
	hash_res_init.tbl_num = p_se_res->hash_tbl_num;
	hash_res_init.func_res = p_se_res->hash_func;
	hash_res_init.bulk_res = p_se_res->hash_bulk;
	hash_res_init.tbl_res = p_se_res->hash_tbl;
	eram_res_init.tbl_num = p_se_res->eram_num;
	eram_res_init.eram_res = p_se_res->eram_tbl;
	acl_res_init.tbl_num = p_se_res->acl_num;
	acl_res_init.acl_res = p_se_res->acl_tbl;

	rc = zxdh_np_apt_hash_global_res_init(dev_id);
	ZXDH_COMM_CHECK_RC(rc, "zxdh_np_apt_hash_global_res_init");

	if (hash_res_init.func_num) {
		rc = zxdh_np_apt_hash_func_res_init(dev_id, hash_res_init.func_num,
			hash_res_init.func_res);
		ZXDH_COMM_CHECK_RC(rc, "zxdh_np_apt_hash_func_res_init");
	}

	if (hash_res_init.bulk_num) {
		rc = zxdh_np_apt_hash_bulk_res_init(dev_id, hash_res_init.bulk_num,
			hash_res_init.bulk_res);
		ZXDH_COMM_CHECK_RC(rc, "zxdh_np_apt_hash_bulk_res_init");
	}

	if (hash_res_init.tbl_num) {
		rc = zxdh_np_apt_hash_tbl_res_init(dev_id, hash_res_init.tbl_num,
			hash_res_init.tbl_res);
		ZXDH_COMM_CHECK_RC(rc, "zxdh_np_apt_hash_tbl_res_init");
	}

	if (eram_res_init.tbl_num) {
		rc = zxdh_np_apt_eram_res_init(dev_id, eram_res_init.tbl_num,
			eram_res_init.eram_res);
		ZXDH_COMM_CHECK_RC(rc, "zxdh_np_apt_eram_res_init");
	}

	if (acl_res_init.tbl_num) {
		rc = zxdh_np_apt_acl_res_init(dev_id, acl_res_init.tbl_num,
			acl_res_init.acl_res);
		ZXDH_COMM_CHECK_RC(rc, "zxdh_np_apt_acl_res_init");
	}

	zxdh_np_apt_stat_res_init(dev_id, type, &p_se_res->stat_cfg);

	return rc;
}

uint32_t
zxdh_np_se_res_get_and_init(uint32_t dev_id, uint32_t type)
{
	uint32_t rc = ZXDH_OK;

	rc = zxdh_np_agent_se_res_get(dev_id, type);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_agent_se_res_get");
	PMD_DRV_LOG(DEBUG, "se res get success.");

	rc = zxdh_np_se_res_init(dev_id, type);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_se_res_init");
	PMD_DRV_LOG(DEBUG, "se res init success.");

	return rc;
}

static uint32_t
zxdh_np_dtb_hash_zcam_del_one_hw(uint32_t dev_id,
								uint32_t queue_id,
								HASH_ENTRY_CFG  *p_hash_entry_cfg,
								ZXDH_HASH_ENTRY  *p_hash_entry,
								ZXDH_DTB_ENTRY_T *p_entry,
								uint8_t *p_srh_succ)
{
	uint32_t rc = ZXDH_OK;
	ZXDH_HASH_RBKEY_INFO srh_rbkey = {0};
	ZXDH_HASH_CFG *p_hash_cfg = NULL;
	ZXDH_SE_ZCELL_CFG *p_zcell = NULL;
	ZXDH_SE_ZBLK_CFG *p_zblk = NULL;

	uint32_t zblk_idx = 0;
	uint32_t pre_zblk_idx = 0xFFFFFFFF;
	uint16_t crc16_value = 0;
	uint32_t zcell_id = 0;
	uint32_t item_idx = 0;
	uint32_t element_id = 0;
	uint32_t byte_offset = 0;
	uint32_t addr = 0;
	uint32_t i	= 0;
	uint8_t srh_succ	 = 0;
	uint8_t temp_key[ZXDH_HASH_KEY_MAX] = {0};
	uint8_t  rd_buff[ZXDH_SE_ITEM_WIDTH_MAX]   = {0};

	ZXDH_D_NODE *p_zblk_dn = NULL;
	ZXDH_D_NODE *p_zcell_dn = NULL;
	ZXDH_SE_CFG *p_se_cfg = NULL;

	memcpy(srh_rbkey.key, p_hash_entry->p_key, p_hash_entry_cfg->key_by_size);

	p_hash_cfg = p_hash_entry_cfg->p_hash_cfg;
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_hash_cfg);

	p_se_cfg = p_hash_entry_cfg->p_se_cfg;
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_se_cfg);

	zxdh_np_hash_set_crc_key(p_hash_entry_cfg, p_hash_entry, temp_key);

	p_zcell_dn = p_hash_cfg->hash_shareram.zcell_free_list.p_next;

	while (p_zcell_dn) {
		p_zcell = (ZXDH_SE_ZCELL_CFG *)p_zcell_dn->data;
		zblk_idx = GET_ZBLK_IDX(p_zcell->zcell_idx);
		p_zblk = &p_se_cfg->zblk_info[zblk_idx];

		if (zblk_idx != pre_zblk_idx) {
			pre_zblk_idx = zblk_idx;
			crc16_value = p_hash_cfg->p_hash16_fun(temp_key,
				p_hash_entry_cfg->key_by_size, p_zblk->hash_arg);
		}

		zcell_id = GET_ZCELL_IDX(p_zcell->zcell_idx);
		item_idx = GET_ZCELL_CRC_VAL(zcell_id, crc16_value);
		addr = ZXDH_ZBLK_ITEM_ADDR_CALC(p_zcell->zcell_idx, item_idx);

		rc =  zxdh_np_dtb_se_zcam_dma_dump(dev_id,
									   queue_id,
									   addr,
									   ZXDH_DTB_DUMP_ZCAM_512b,
									   1,
									   (uint32_t *)rd_buff,
									   &element_id);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_se_zcam_dma_dump");

		zxdh_np_comm_swap(rd_buff, sizeof(rd_buff));

		rc = zxdh_np_dtb_hash_data_parse(ZXDH_ITEM_RAM, p_hash_entry_cfg->key_by_size,
			p_hash_entry, rd_buff, &byte_offset);
		if (rc == ZXDH_OK) {
			PMD_DRV_LOG(DEBUG, "Hash search hardware succ in zcell.");
			srh_succ = 1;
			p_hash_cfg->hash_stat.search_ok++;
			break;
		}

		p_zcell_dn = p_zcell_dn->next;
	}

	if (srh_succ == 0) {
		p_zblk_dn = p_hash_cfg->hash_shareram.zblk_list.p_next;
		while (p_zblk_dn) {
			p_zblk = (ZXDH_SE_ZBLK_CFG *)p_zblk_dn->data;
			zblk_idx = p_zblk->zblk_idx;

			for (i = 0; i < ZXDH_SE_ZREG_NUM; i++) {
				item_idx = i;
				addr = ZXDH_ZBLK_HASH_LIST_REG_ADDR_CALC(zblk_idx, item_idx);
				rc =  zxdh_np_dtb_se_zcam_dma_dump(dev_id,
									   queue_id,
									   addr,
									   ZXDH_DTB_DUMP_ZCAM_512b,
									   1,
									   (uint32_t *)rd_buff,
									   &element_id);
				ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_se_zcam_dma_dump");
				zxdh_np_comm_swap(rd_buff, sizeof(rd_buff));

				rc = zxdh_np_dtb_hash_data_parse(ZXDH_ITEM_RAM,
					p_hash_entry_cfg->key_by_size, p_hash_entry,
					rd_buff, &byte_offset);
				if (rc == ZXDH_OK) {
					PMD_DRV_LOG(DEBUG, "Hash search hardware succ in zreg.");
					srh_succ = 1;
					p_hash_cfg->hash_stat.search_ok++;
					break;
				}
			}
			p_zblk_dn = p_zblk_dn->next;
		}
	}

	if (srh_succ) {
		memset(rd_buff + byte_offset, 0,
			ZXDH_GET_HASH_ENTRY_SIZE(p_hash_entry_cfg->key_type));
		zxdh_np_comm_swap(rd_buff, sizeof(rd_buff));
		rc = zxdh_np_dtb_se_alg_zcam_data_write(dev_id,
									   addr,
									   rd_buff,
									   p_entry);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_se_alg_zcam_data_write");
		p_hash_cfg->hash_stat.delete_ok++;
	}

	*p_srh_succ = srh_succ;

	return rc;
}

static uint32_t
zxdh_np_dtb_hash_zcam_del_hw(uint32_t dev_id,
						uint32_t queue_id,
						uint32_t sdt_no,
						uint32_t entry_num,
						ZXDH_DTB_HASH_ENTRY_INFO_T *p_arr_hash_entry,
						uint32_t *element_id)
{
	uint32_t rc = ZXDH_OK;
	uint32_t item_cnt = 0;
	uint32_t key_valid = 1;
	uint32_t dtb_len = 0;
	uint8_t srh_succ = 0;
	uint8_t key[ZXDH_HASH_KEY_MAX] = {0};
	uint8_t rst[ZXDH_HASH_RST_MAX] = {0};
	uint8_t entry_cmd[ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8] = {0};

	uint8_t entry_data[ZXDH_ETCAM_WIDTH_MAX / 8] = {0};
	uint8_t *p_data_buff = NULL;

	ZXDH_HASH_ENTRY  entry = {0};
	ZXDH_DTB_ENTRY_T   dtb_one_entry = {0};
	HASH_ENTRY_CFG  hash_entry_cfg = {0};

	dtb_one_entry.cmd = entry_cmd;
	dtb_one_entry.data = entry_data;
	entry.p_key = key;
	entry.p_rst = rst;

	rc = zxdh_np_hash_get_hash_info_from_sdt(dev_id, sdt_no, &hash_entry_cfg);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_hash_get_hash_info_from_sdt");

	p_data_buff = rte_zmalloc(NULL, ZXDH_DTB_TABLE_DATA_BUFF_SIZE, 0);
	if (p_data_buff == NULL) {
		PMD_DRV_LOG(ERR, "malloc memory failed");
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	for (item_cnt = 0; item_cnt < entry_num; ++item_cnt) {
		srh_succ = 0;
		memset(key, 0, sizeof(key));
		memset(rst, 0, sizeof(rst));
		memset(entry_cmd, 0, sizeof(entry_cmd));
		memset(entry_data, 0, sizeof(entry_data));
		entry.p_key[0] = (uint8_t)(((key_valid & 0x1) << 7)
							| ((hash_entry_cfg.key_type & 0x3) << 5)
							| (hash_entry_cfg.table_id & 0x1f));
		memcpy(&entry.p_key[1], p_arr_hash_entry[item_cnt].p_actu_key,
			hash_entry_cfg.actu_key_size);

		rc = zxdh_np_dtb_hash_zcam_del_one_hw(dev_id,
										queue_id,
										&hash_entry_cfg,
										&entry,
										&dtb_one_entry,
										&srh_succ);

		if (srh_succ) {
			zxdh_np_dtb_data_write(p_data_buff, 0, &dtb_one_entry);
			dtb_len = dtb_one_entry.data_size / ZXDH_DTB_LEN_POS_SETP + 1;
			zxdh_np_dtb_write_down_table_data(dev_id, queue_id,
				dtb_len * ZXDH_DTB_LEN_POS_SETP, p_data_buff, element_id);
			rc = zxdh_np_dtb_tab_down_success_status_check(dev_id,
						queue_id, *element_id);
		}
	}

	rte_free(p_data_buff);

	return rc;
}

static uint32_t
zxdh_np_dtb_sdt_hash_zcam_mono_space_dump(uint32_t dev_id,
							uint32_t queue_id,
							uint32_t sdt_no,
							HASH_ENTRY_CFG *p_hash_entry_cfg,
							uint8_t  *p_data,
							uint32_t *p_dump_len)
{
	uint32_t rc = ZXDH_OK;

	uint32_t i = 0;
	uint32_t zblock_id = 0;
	uint32_t zcell_id   = 0;
	uint32_t start_addr  = 0;
	uint32_t dtb_desc_len = 0;
	uint32_t dump_pa_h   = 0;
	uint32_t dump_pa_l   = 0;
	uint32_t dma_addr_offset = 0;
	uint32_t desc_addr_offset = 0;
	uint32_t element_id  = 0;
	uint8_t  *p_dump_desc_buf = NULL;

	ZXDH_D_NODE *p_zblk_dn = NULL;
	ZXDH_SE_ZBLK_CFG *p_zblk = NULL;
	ZXDH_SE_ZREG_CFG *p_zreg = NULL;
	ZXDH_SE_ZCELL_CFG *p_zcell = NULL;
	ZXDH_HASH_CFG *p_hash_cfg = NULL;

	ZXDH_DTB_ENTRY_T   dtb_dump_entry = {0};
	uint8_t cmd_buff[ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8] = {0};

	rc = zxdh_np_dtb_dump_addr_set(dev_id, queue_id, sdt_no, &element_id);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_dump_addr_set");

	p_dump_desc_buf = rte_zmalloc(NULL, ZXDH_DTB_TABLE_DUMP_INFO_BUFF_SIZE, 0);
	if (p_dump_desc_buf == NULL) {
		PMD_DRV_LOG(ERR, "malloc memory failed");
		return ZXDH_PAR_CHK_POINT_NULL;
	}
	dtb_dump_entry.cmd = cmd_buff;

	p_hash_cfg = p_hash_entry_cfg->p_hash_cfg;
	p_zblk_dn = p_hash_cfg->hash_shareram.zblk_list.p_next;

	while (p_zblk_dn) {
		p_zblk = (ZXDH_SE_ZBLK_CFG *)p_zblk_dn->data;
		zblock_id = p_zblk->zblk_idx;

		for (i = 0; i < ZXDH_SE_ZCELL_NUM; i++) {
			p_zcell = &p_zblk->zcell_info[i];

			if ((p_zcell->flag & ZXDH_ZCELL_FLAG_IS_MONO) &&
			p_zcell->bulk_id == p_hash_entry_cfg->bulk_id) {
				zcell_id = p_zcell->zcell_idx;

				start_addr = ZXDH_ZBLK_ITEM_ADDR_CALC(zcell_id, 0);

				zxdh_np_dtb_tab_up_item_offset_addr_get(dev_id,
						queue_id,
						element_id,
						dma_addr_offset,
						&dump_pa_h,
						&dump_pa_l);

				zxdh_np_dtb_zcam_dump_entry(dev_id, start_addr,
				ZXDH_DTB_DUMP_ZCAM_512b, ZXDH_SE_RAM_DEPTH,
				dump_pa_h, dump_pa_l, &dtb_dump_entry);

				zxdh_np_dtb_data_write(p_dump_desc_buf, desc_addr_offset,
					&dtb_dump_entry);

				dtb_desc_len++;
				dma_addr_offset += ZXDH_SE_RAM_DEPTH * 512 / 8;
				desc_addr_offset += ZXDH_DTB_LEN_POS_SETP;

				PMD_DRV_LOG(DEBUG, "the Zblock[%u]'s bulk_id:%u Mono Zcell_id :%u",
					zblock_id, p_hash_entry_cfg->bulk_id, zcell_id);
			}
		}

		for (i = 0; i < ZXDH_SE_ZREG_NUM; i++) {
			p_zreg = &p_zblk->zreg_info[i];

			if ((p_zreg->flag & ZXDH_ZREG_FLAG_IS_MONO) &&
			p_zreg->bulk_id == p_hash_entry_cfg->bulk_id) {
				start_addr = ZXDH_ZBLK_HASH_LIST_REG_ADDR_CALC(zblock_id, i);

				zxdh_np_dtb_tab_up_item_offset_addr_get(dev_id,
						queue_id,
						element_id,
						dma_addr_offset,
						&dump_pa_h,
						&dump_pa_l);

				zxdh_np_dtb_zcam_dump_entry(dev_id, start_addr,
				ZXDH_DTB_DUMP_ZCAM_512b, 1, dump_pa_h, dump_pa_l, &dtb_dump_entry);

				zxdh_np_dtb_data_write(p_dump_desc_buf, desc_addr_offset,
					&dtb_dump_entry);

				dtb_desc_len++;
				dma_addr_offset += 512 / 8;
				desc_addr_offset += ZXDH_DTB_LEN_POS_SETP;

				PMD_DRV_LOG(DEBUG, "the Zblock[%u]'s bulk_id:%u  Mono Zreg_id :%u",
					zblock_id, p_zreg->bulk_id, i);
			}
		}

		p_zblk_dn = p_zblk_dn->next;
	}

	zxdh_np_dtb_write_dump_desc_info(dev_id,
					queue_id,
					element_id,
					(uint32_t *)p_dump_desc_buf,
					dma_addr_offset / 4,
					dtb_desc_len * 4,
					(uint32_t *)p_data);

	zxdh_np_comm_swap(p_data, dma_addr_offset);
	rte_free(p_dump_desc_buf);

	*p_dump_len = dma_addr_offset;

	return rc;
}

static uint32_t
zxdh_np_dtb_hash_table_zcam_dump(uint32_t dev_id,
	uint32_t queue_id,
	uint32_t sdt_no,
	uint32_t zblock_num,
	uint32_t zblock_array[ZXDH_SE_ZBLK_NUM],
	uint8_t  *p_data,
	uint32_t *p_dump_len)
{
	uint32_t  rc		  = ZXDH_OK;
	uint32_t zblk_idx	 = 0;
	uint32_t index		= 0;
	uint32_t zblock_id   = 0;
	uint32_t zcell_id   = 0;
	uint32_t start_addr  = 0;
	uint32_t dtb_desc_len = 0;
	uint32_t dump_pa_h   = 0;
	uint32_t dump_pa_l   = 0;
	uint32_t dma_addr_offset = 0;
	uint32_t desc_addr_offset = 0;
	uint32_t element_id  = 0;
	uint8_t  *p_dump_desc_buf = NULL;

	ZXDH_DTB_ENTRY_T   dtb_dump_entry = {0};
	uint8_t cmd_buff[ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8] = {0};

	rc = zxdh_np_dtb_dump_addr_set(dev_id, queue_id, sdt_no, &element_id);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_dump_addr_set");

	p_dump_desc_buf = rte_zmalloc(NULL, ZXDH_DTB_TABLE_DUMP_INFO_BUFF_SIZE, 0);
	if (p_dump_desc_buf == NULL) {
		PMD_DRV_LOG(ERR, "malloc memory failed");
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	dtb_dump_entry.cmd = cmd_buff;
	for (zblk_idx = 0; zblk_idx < zblock_num; zblk_idx++) {
		zblock_id = zblock_array[zblk_idx];
		for (index = 0; index < ZXDH_SE_ZCELL_NUM; index++) {
			zcell_id = zblock_id * ZXDH_SE_ZCELL_NUM + index;
			start_addr = ZXDH_ZBLK_ITEM_ADDR_CALC(zcell_id, 0);

			zxdh_np_dtb_tab_up_item_offset_addr_get(dev_id,
					queue_id,
					element_id,
					dma_addr_offset,
					&dump_pa_h,
					&dump_pa_l);

			zxdh_np_dtb_zcam_dump_entry(dev_id, start_addr,
			ZXDH_DTB_DUMP_ZCAM_512b, ZXDH_SE_RAM_DEPTH,
			dump_pa_h, dump_pa_l, &dtb_dump_entry);

			zxdh_np_dtb_data_write(p_dump_desc_buf,
				desc_addr_offset, &dtb_dump_entry);

			dtb_desc_len++;
			dma_addr_offset += ZXDH_SE_RAM_DEPTH * 512 / 8;
			desc_addr_offset += ZXDH_DTB_LEN_POS_SETP;
		}

		zxdh_np_dtb_tab_up_item_offset_addr_get(dev_id,
				queue_id,
				element_id,
				dma_addr_offset,
				&dump_pa_h,
				&dump_pa_l);

		start_addr = ZXDH_ZBLK_HASH_LIST_REG_ADDR_CALC(zblock_id, 0);
		zxdh_np_dtb_zcam_dump_entry(dev_id, start_addr, ZXDH_DTB_DUMP_ZCAM_512b,
			ZXDH_SE_ZREG_NUM, dump_pa_h, dump_pa_l, &dtb_dump_entry);

		zxdh_np_dtb_data_write(p_dump_desc_buf, desc_addr_offset, &dtb_dump_entry);

		dtb_desc_len++;
		dma_addr_offset += ZXDH_SE_ZREG_NUM * 512 / 8;
		desc_addr_offset += ZXDH_DTB_LEN_POS_SETP;
	}

	zxdh_np_dtb_write_dump_desc_info(dev_id,
					queue_id,
					element_id,
					(uint32_t *)p_dump_desc_buf,
					dma_addr_offset / 4,
					dtb_desc_len * 4,
					(uint32_t *)p_data);

	zxdh_np_comm_swap(p_data, dma_addr_offset);
	rte_free(p_dump_desc_buf);

	*p_dump_len = dma_addr_offset;

	return rc;
}

static void
zxdh_np_dtb_dump_hash_parse(HASH_ENTRY_CFG *p_hash_entry_cfg,
							uint32_t item_type,
							uint8_t *pdata,
							uint32_t dump_len,
							uint8_t *p_outdata,
							uint32_t *p_item_num)
{
	uint32_t item_num = 0;
	uint32_t data_offset = 0;
	uint32_t index = 0;
	uint8_t temp_key_valid = 0;
	uint8_t temp_key_type = 0;
	uint8_t temp_tbl_id   = 0;
	uint32_t srh_entry_size = 0;
	uint32_t item_width = ZXDH_SE_ITEM_WIDTH_MAX;
	uint8_t *p_temp_key = NULL;
	uint8_t *p_hash_item = NULL;
	ZXDH_DTB_HASH_ENTRY_INFO_T  *p_dtb_hash_entry = NULL;
	ZXDH_DTB_HASH_ENTRY_INFO_T  *p_temp_entry = NULL;

	if (item_type == ZXDH_ITEM_DDR_256)
		item_width = item_width / 2;

	p_dtb_hash_entry = (ZXDH_DTB_HASH_ENTRY_INFO_T *)p_outdata;
	srh_entry_size = ZXDH_GET_HASH_ENTRY_SIZE(p_hash_entry_cfg->key_type);

	for (index = 0; index < (dump_len / item_width); index++) {
		data_offset = 0;
		p_hash_item = pdata + index * item_width;
		while (data_offset < item_width) {
			p_temp_key = p_hash_item + data_offset;
			temp_key_valid = ZXDH_GET_HASH_KEY_VALID(p_temp_key);
			temp_key_type = ZXDH_GET_HASH_KEY_TYPE(p_temp_key);
			temp_tbl_id = ZXDH_GET_HASH_TBL_ID(p_temp_key);
			p_temp_entry = p_dtb_hash_entry + item_num;

			if (temp_key_valid && temp_key_type == p_hash_entry_cfg->key_type &&
			temp_tbl_id == p_hash_entry_cfg->table_id) {
				memcpy(p_temp_entry->p_actu_key, p_temp_key + 1,
					p_hash_entry_cfg->actu_key_size);
				memcpy(p_temp_entry->p_rst,
					p_temp_key + p_hash_entry_cfg->key_by_size,
					p_hash_entry_cfg->rst_by_size);
				item_num++;
			}

			data_offset += srh_entry_size;
		}
	}

	*p_item_num = item_num;
}

static uint32_t
zxdh_np_dtb_hash_table_dump(uint32_t dev_id,
						uint32_t queue_id,
						uint32_t sdt_no,
						ZXDH_DTB_DUMP_INDEX_T start_index,
						uint8_t *p_dump_data,
						uint32_t *p_entry_num,
						ZXDH_DTB_DUMP_INDEX_T *next_start_index,
						uint32_t *finish_flag)
{
	uint32_t  rc = ZXDH_OK;
	uint8_t *p_data = NULL;
	uint32_t data_len = 0;
	uint32_t entry_num = 0;
	ZXDH_HASH_CFG *p_hash_cfg = NULL;
	HASH_ENTRY_CFG  hash_entry_cfg = {0};

	rc = zxdh_np_hash_get_hash_info_from_sdt(dev_id, sdt_no, &hash_entry_cfg);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_hash_get_hash_info_from_sdt");

	p_hash_cfg = hash_entry_cfg.p_hash_cfg;
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_hash_cfg);

	p_data = rte_zmalloc(NULL, ZXDH_DTB_DMUP_DATA_MAX, 0);
	if (p_data == NULL) {
		PMD_DRV_LOG(ERR, "malloc memory failed");
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	if (start_index.index_type == ZXDH_DTB_DUMP_ZCAM_TYPE) {
		if (p_hash_cfg->bulk_ram_mono[hash_entry_cfg.bulk_id]) {
			rc = zxdh_np_dtb_sdt_hash_zcam_mono_space_dump(dev_id,
					queue_id,
					sdt_no,
					&hash_entry_cfg,
					p_data,
					&data_len);
		} else {
			rc = zxdh_np_dtb_hash_table_zcam_dump(dev_id,
						queue_id,
						sdt_no,
						p_hash_cfg->hash_stat.zblock_num,
						p_hash_cfg->hash_stat.zblock_array,
						p_data,
						&data_len);
		}

		zxdh_np_dtb_dump_hash_parse(&hash_entry_cfg,
									 ZXDH_ITEM_RAM,
									 p_data,
									 data_len,
									 p_dump_data,
									 &entry_num);

		if (p_hash_cfg->ddr_valid) {
			next_start_index->index = 0;
			next_start_index->index_type = ZXDH_DTB_DUMP_DDR_TYPE;
		} else {
			*finish_flag = 1;
		}
		*p_entry_num = entry_num;
	}

	rte_free(p_data);
	return rc;
}

static uint32_t
zxdh_np_dtb_hash_offline_zcam_delete(uint32_t dev_id,
								uint32_t queue_id,
								uint32_t sdt_no)
{
	uint32_t rc = ZXDH_OK;
	uint32_t entry_num = 0;
	uint32_t finish_flag = 0;
	uint32_t index = 0;
	uint32_t max_item_num = 1024 * 1024;
	uint8_t *p_dump_data = NULL;
	uint8_t *p_key = NULL;
	uint8_t *p_rst = NULL;
	uint32_t element_id = 0;

	ZXDH_DTB_HASH_ENTRY_INFO_T *p_dtb_hash_entry = NULL;
	ZXDH_DTB_HASH_ENTRY_INFO_T *p_temp_entry = NULL;
	ZXDH_DTB_DUMP_INDEX_T start_index = {0};
	ZXDH_DTB_DUMP_INDEX_T next_start_index = {0};

	ZXDH_SDT_TBL_HASH_T sdt_hash = {0};
	start_index.index = 0;
	start_index.index_type = ZXDH_DTB_DUMP_ZCAM_TYPE;

	PMD_DRV_LOG(DEBUG, "sdt_no:%u", sdt_no);
	rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_hash);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");

	p_dump_data = rte_zmalloc(NULL, max_item_num * sizeof(ZXDH_DTB_HASH_ENTRY_INFO_T), 0);
	if (p_dump_data == NULL) {
		PMD_DRV_LOG(ERR, "malloc memory failed");
		return ZXDH_PAR_CHK_POINT_NULL;
	}
	p_key = rte_zmalloc(NULL, max_item_num * ZXDH_HASH_KEY_MAX, 0);
	if (p_key == NULL) {
		PMD_DRV_LOG(ERR, "malloc memory failed");
		rte_free(p_dump_data);
		return ZXDH_PAR_CHK_POINT_NULL;
	}
	p_rst = rte_zmalloc(NULL, max_item_num * ZXDH_HASH_RST_MAX, 0);
	if (p_key == NULL) {
		PMD_DRV_LOG(ERR, "malloc memory failed");
		rte_free(p_dump_data);
		rte_free(p_key);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	p_dtb_hash_entry = (ZXDH_DTB_HASH_ENTRY_INFO_T *)p_dump_data;
	for (index = 0; index < max_item_num; index++) {
		p_temp_entry = p_dtb_hash_entry + index;
		p_temp_entry->p_actu_key = p_key + index * ZXDH_HASH_KEY_MAX;
		p_temp_entry->p_rst = p_rst + index * ZXDH_HASH_RST_MAX;
	}

	rc = zxdh_np_dtb_hash_table_dump(dev_id, queue_id, sdt_no, start_index,
		p_dump_data, &entry_num, &next_start_index, &finish_flag);

	if (entry_num) {
		rc = zxdh_np_dtb_hash_zcam_del_hw(dev_id, queue_id, sdt_no,
			entry_num, p_dtb_hash_entry, &element_id);
	}

	rte_free(p_dtb_hash_entry);
	rte_free(p_key);
	rte_free(p_rst);

	return rc;
}

uint32_t
zxdh_np_dtb_hash_online_delete(uint32_t dev_id, uint32_t queue_id, uint32_t sdt_no)
{
	uint32_t rc = 0;
	uint8_t key_valid = 0;
	uint32_t table_id = 0;
	uint32_t key_type = 0;

	ZXDH_D_NODE *p_node = NULL;
	ZXDH_RB_TN *p_rb_tn = NULL;
	ZXDH_D_HEAD *p_head_hash_rb = NULL;
	ZXDH_HASH_CFG *p_hash_cfg = NULL;
	ZXDH_HASH_RBKEY_INFO *p_rbkey = NULL;
	HASH_ENTRY_CFG  hash_entry_cfg = {0};
	ZXDH_DTB_USER_ENTRY_T del_entry = {0};
	ZXDH_DTB_HASH_ENTRY_INFO_T hash_entry = {0};

	del_entry.sdt_no = sdt_no;
	del_entry.p_entry_data = &hash_entry;

	rc = zxdh_np_hash_get_hash_info_from_sdt(dev_id, sdt_no, &hash_entry_cfg);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_hash_get_hash_info_from_sdt");

	p_hash_cfg = hash_entry_cfg.p_hash_cfg;
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_hash_cfg);

	p_head_hash_rb = &p_hash_cfg->hash_rb.tn_list;
	p_node = p_head_hash_rb->p_next;
	while (p_node) {
		p_rb_tn = (ZXDH_RB_TN *)p_node->data;
		ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_rb_tn);
		p_rbkey = (ZXDH_HASH_RBKEY_INFO *)p_rb_tn->p_key;
		hash_entry.p_actu_key = p_rbkey->key + 1;
		hash_entry.p_rst = p_rbkey->key;

		key_valid = ZXDH_GET_HASH_KEY_VALID(p_rbkey->key);
		table_id = ZXDH_GET_HASH_TBL_ID(p_rbkey->key);
		key_type = ZXDH_GET_HASH_KEY_TYPE(p_rbkey->key);
		if (!key_valid ||
		table_id != hash_entry_cfg.table_id ||
		key_type != hash_entry_cfg.key_type) {
			p_node = p_node->next;
			continue;
		}
		p_node = p_node->next;

		rc = zxdh_np_dtb_table_entry_delete(dev_id, queue_id, 1, &del_entry);
		if (rc == ZXDH_HASH_RC_DEL_SRHFAIL)
			continue;
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_table_entry_delete");
	}

	return rc;
}

uint32_t
zxdh_np_dtb_hash_offline_delete(uint32_t dev_id,
				uint32_t queue_id,
				uint32_t sdt_no,
				__rte_unused uint32_t flush_mode)
{
	uint32_t rc = ZXDH_OK;

	rc = zxdh_np_dtb_hash_offline_zcam_delete(dev_id, queue_id, sdt_no);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_hash_offline_zcam_delete");

	return rc;
}

static uint32_t
zxdh_np_se_done_status_check(uint32_t dev_id, uint32_t reg_no, uint32_t pos)
{
	uint32_t rc = 0;

	uint32_t data = 0;
	uint32_t rd_cnt = 0;
	uint32_t done_flag = 0;

	while (!done_flag) {
		rc = zxdh_np_reg_read32(dev_id, reg_no, 0, 0, &data);
		if (rc != 0) {
			PMD_DRV_LOG(ERR, "reg_read32 fail!");
			return rc;
		}

		done_flag = (data >> pos) & 0x1;

		if (done_flag)
			break;

		if (rd_cnt > ZXDH_RD_CNT_MAX * ZXDH_RD_CNT_MAX)
			return -1;

		rd_cnt++;
	}

	return rc;
}

static uint32_t
zxdh_np_se_smmu0_ind_read(uint32_t dev_id,
						uint32_t base_addr,
						uint32_t index,
						uint32_t rd_mode,
						uint32_t rd_clr_mode,
						uint32_t *p_data)
{
	uint32_t rc = ZXDH_OK;
	uint32_t i = 0;
	uint32_t row_index = 0;
	uint32_t col_index = 0;
	uint32_t temp_data[4] = {0};
	uint32_t *p_temp_data = NULL;
	ZXDH_SMMU0_SMMU0_CPU_IND_CMD_T cpu_ind_cmd = {0};
	ZXDH_SPINLOCK_T *p_ind_spinlock = NULL;

	rc = zxdh_np_dev_opr_spinlock_get(dev_id, ZXDH_DEV_SPINLOCK_T_SMMU0, &p_ind_spinlock);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dev_opr_spinlock_get");

	rte_spinlock_lock(&p_ind_spinlock->spinlock);

	rc = zxdh_np_se_done_status_check(dev_id, ZXDH_SMMU0_SMMU0_WR_ARB_CPU_RDYR, 0);
	if (rc != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "se done status check failed, rc=0x%x.", rc);
		rte_spinlock_unlock(&p_ind_spinlock->spinlock);
		return ZXDH_ERR;
	}

	if (rd_clr_mode == ZXDH_RD_MODE_HOLD) {
		cpu_ind_cmd.cpu_ind_rw = ZXDH_SE_OPR_RD;
		cpu_ind_cmd.cpu_ind_rd_mode = ZXDH_RD_MODE_HOLD;
		cpu_ind_cmd.cpu_req_mode = ZXDH_ERAM128_OPR_128b;

		switch (rd_mode) {
		case ZXDH_ERAM128_OPR_128b:
			if ((0xFFFFFFFF - (base_addr)) < (index)) {
				rte_spinlock_unlock(&p_ind_spinlock->spinlock);
				PMD_DRV_LOG(ERR, "index 0x%x is invalid!", index);
				return ZXDH_PAR_CHK_INVALID_INDEX;
			}
			if (base_addr + index > ZXDH_SE_SMMU0_ERAM_ADDR_NUM_TOTAL - 1) {
				PMD_DRV_LOG(ERR, "index out of range!");
				rte_spinlock_unlock(&p_ind_spinlock->spinlock);
				return ZXDH_ERR;
			}
			row_index = (index << 7) & ZXDH_ERAM128_BADDR_MASK;
			break;
		case ZXDH_ERAM128_OPR_64b:
			if ((base_addr + (index >> 1)) > ZXDH_SE_SMMU0_ERAM_ADDR_NUM_TOTAL - 1) {
				PMD_DRV_LOG(ERR, "index out of range!");
				rte_spinlock_unlock(&p_ind_spinlock->spinlock);
				return ZXDH_ERR;
			}
			row_index = (index << 6) & ZXDH_ERAM128_BADDR_MASK;
			col_index = index & 0x1;
			break;
		case ZXDH_ERAM128_OPR_32b:
			if ((base_addr + (index >> 2)) > ZXDH_SE_SMMU0_ERAM_ADDR_NUM_TOTAL - 1) {
				PMD_DRV_LOG(ERR, "index out of range!");
				rte_spinlock_unlock(&p_ind_spinlock->spinlock);
				return ZXDH_ERR;
			}
			row_index = (index << 5) & ZXDH_ERAM128_BADDR_MASK;
			col_index = index & 0x3;
			break;
		case ZXDH_ERAM128_OPR_1b:
			if ((base_addr + (index >> 7)) > ZXDH_SE_SMMU0_ERAM_ADDR_NUM_TOTAL - 1) {
				PMD_DRV_LOG(ERR, "index out of range!");
				rte_spinlock_unlock(&p_ind_spinlock->spinlock);
				return ZXDH_ERR;
			}
			row_index = index & ZXDH_ERAM128_BADDR_MASK;
			col_index = index & 0x7F;
			break;
		default:
			break;
		}

		cpu_ind_cmd.cpu_ind_addr = ((base_addr << 7) & ZXDH_ERAM128_BADDR_MASK) + row_index;
	} else {
		cpu_ind_cmd.cpu_ind_rw = ZXDH_SE_OPR_RD;
		cpu_ind_cmd.cpu_ind_rd_mode = ZXDH_RD_MODE_CLEAR;

		switch (rd_mode) {
		case ZXDH_ERAM128_OPR_128b:
			if ((0xFFFFFFFF - (base_addr)) < (index)) {
				PMD_DRV_LOG(ERR, "index 0x%x is invalid!", index);
				rte_spinlock_unlock(&p_ind_spinlock->spinlock);
				return ZXDH_PAR_CHK_INVALID_INDEX;
			}
			if (base_addr + index > ZXDH_SE_SMMU0_ERAM_ADDR_NUM_TOTAL - 1) {
				PMD_DRV_LOG(ERR, "index out of range!");
				rte_spinlock_unlock(&p_ind_spinlock->spinlock);
				return ZXDH_ERR;
			}
			row_index = (index << 7);
			cpu_ind_cmd.cpu_req_mode = ZXDH_ERAM128_OPR_128b;
			break;
		case ZXDH_ERAM128_OPR_64b:
			if ((base_addr + (index >> 1)) > ZXDH_SE_SMMU0_ERAM_ADDR_NUM_TOTAL - 1) {
				PMD_DRV_LOG(ERR, "index out of range!");
				rte_spinlock_unlock(&p_ind_spinlock->spinlock);
				return ZXDH_ERR;
			}
			row_index = (index << 6);
			cpu_ind_cmd.cpu_req_mode = 2;
			break;
		case ZXDH_ERAM128_OPR_32b:
			if ((base_addr + (index >> 2)) > ZXDH_SE_SMMU0_ERAM_ADDR_NUM_TOTAL - 1) {
				PMD_DRV_LOG(ERR, "index out of range!");
				rte_spinlock_unlock(&p_ind_spinlock->spinlock);
				return ZXDH_ERR;
			}
			row_index = (index << 5);
			cpu_ind_cmd.cpu_req_mode = 1;
			break;
		case ZXDH_ERAM128_OPR_1b:
			PMD_DRV_LOG(ERR, "rd_clr_mode[%u] or rd_mode[%u] error!",
				rd_clr_mode, rd_mode);
			rte_spinlock_unlock(&p_ind_spinlock->spinlock);
			return ZXDH_ERR;
		default:
			break;
		}
		cpu_ind_cmd.cpu_ind_addr = ((base_addr << 7) & ZXDH_ERAM128_BADDR_MASK) + row_index;
	}

	rc = zxdh_np_reg_write(dev_id,
			ZXDH_SMMU0_SMMU0_CPU_IND_CMDR,
			0,
			0,
			&cpu_ind_cmd);
	if (rc != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "zxdh_np_reg_write failed, rc=0x%x.", rc);
		rte_spinlock_unlock(&p_ind_spinlock->spinlock);
		return ZXDH_ERR;
	}

	rc = zxdh_np_se_done_status_check(dev_id, ZXDH_SMMU0_SMMU0_CPU_IND_RD_DONER, 0);
	if (rc != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "se done status check failed, rc=0x%x.", rc);
		rte_spinlock_unlock(&p_ind_spinlock->spinlock);
		return ZXDH_ERR;
	}

	p_temp_data = temp_data;
	for (i = 0; i < 4; i++) {
		rc = zxdh_np_reg_read(dev_id,
			ZXDH_SMMU0_SMMU0_CPU_IND_RDAT0R + i,
			0,
			0,
			p_temp_data + 3 - i);
		if (rc != ZXDH_OK) {
			PMD_DRV_LOG(ERR, "zxdh_np_reg_write failed, rc=0x%x.", rc);
			rte_spinlock_unlock(&p_ind_spinlock->spinlock);
			return ZXDH_ERR;
		}
	}

	if (rd_clr_mode == ZXDH_RD_MODE_HOLD) {
		switch (rd_mode) {
		case ZXDH_ERAM128_OPR_128b:
			memcpy(p_data, p_temp_data, (128 / 8));
			break;
		case ZXDH_ERAM128_OPR_64b:
			memcpy(p_data, p_temp_data + ((1 - col_index) << 1), (64 / 8));
			break;
		case ZXDH_ERAM128_OPR_32b:
			memcpy(p_data, p_temp_data + ((3 - col_index)), (32 / 8));
			break;
		case ZXDH_ERAM128_OPR_1b:
			ZXDH_COMM_UINT32_GET_BITS(p_data[0],
				*(p_temp_data + (3 - col_index / 32)), (col_index % 32), 1);
			break;
		default:
			break;
		}
	} else {
		switch (rd_mode) {
		case ZXDH_ERAM128_OPR_128b:
			memcpy(p_data, p_temp_data, (128 / 8));
			break;
		case ZXDH_ERAM128_OPR_64b:
			memcpy(p_data, p_temp_data, (64 / 8));
			break;
		case ZXDH_ERAM128_OPR_32b:
			memcpy(p_data, p_temp_data, (64 / 8));
			break;
		default:
			break;
		}
	}

	rte_spinlock_unlock(&p_ind_spinlock->spinlock);

	return rc;
}

uint32_t
zxdh_np_stat_ppu_cnt_get_ex(uint32_t dev_id,
				ZXDH_STAT_CNT_MODE_E rd_mode,
				uint32_t index,
				uint32_t clr_mode,
				uint32_t *p_data)
{
	uint32_t rc = 0;
	uint32_t ppu_eram_baddr = 0;
	uint32_t ppu_eram_depth = 0;
	uint32_t eram_rd_mode   = 0;
	uint32_t eram_clr_mode  = 0;
	ZXDH_PPU_STAT_CFG_T stat_cfg = {0};

	zxdh_np_stat_cfg_soft_get(dev_id, &stat_cfg);

	ppu_eram_depth = stat_cfg.eram_depth;
	ppu_eram_baddr = stat_cfg.eram_baddr;

	if ((index >> (ZXDH_STAT_128_MODE - rd_mode)) < ppu_eram_depth) {
		if (rd_mode == ZXDH_STAT_128_MODE)
			eram_rd_mode = ZXDH_ERAM128_OPR_128b;
		else
			eram_rd_mode = ZXDH_ERAM128_OPR_64b;

		if (clr_mode == ZXDH_STAT_RD_CLR_MODE_UNCLR)
			eram_clr_mode = ZXDH_RD_MODE_HOLD;
		else
			eram_clr_mode = ZXDH_RD_MODE_CLEAR;

		rc = zxdh_np_se_smmu0_ind_read(dev_id,
									ppu_eram_baddr,
									index,
									eram_rd_mode,
									eram_clr_mode,
									p_data);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_se_smmu0_ind_read");
	} else {
		PMD_DRV_LOG(ERR, "DPDK DON'T HAVE DDR STAT.");
	}

	return rc;
}

static uint32_t
zxdh_np_agent_channel_plcr_sync_send(uint32_t dev_id, ZXDH_AGENT_CHANNEL_PLCR_MSG_T *p_msg,
		uint32_t *p_data, uint32_t rep_len)
{
	uint32_t ret = 0;
	ZXDH_AGENT_CHANNEL_MSG_T agent_msg = {
		.msg = (void *)p_msg,
		.msg_len = sizeof(ZXDH_AGENT_CHANNEL_PLCR_MSG_T),
	};

	ret = zxdh_np_agent_channel_sync_send(dev_id, &agent_msg, p_data, rep_len);
	if (ret != 0)	{
		PMD_DRV_LOG(ERR, "agent_channel_sync_send failed.");
		return 1;
	}

	return 0;
}

static uint32_t
zxdh_np_agent_channel_plcr_profileid_request(uint32_t dev_id, uint32_t vport,
	uint32_t car_type, uint32_t *p_profileid)
{
	uint32_t ret = 0;
	uint32_t resp_buffer[2] = {0};

	ZXDH_AGENT_CHANNEL_PLCR_MSG_T msgcfg = {
		.dev_id = 0,
		.type = ZXDH_PLCR_MSG,
		.oper = ZXDH_PROFILEID_REQUEST,
		.vport = vport,
		.car_type = car_type,
		.profile_id = 0xFFFF,
	};

	ret = zxdh_np_agent_channel_plcr_sync_send(dev_id, &msgcfg,
		resp_buffer, sizeof(resp_buffer));
	if (ret != 0)	{
		PMD_DRV_LOG(ERR, "agent_channel_plcr_sync_send failed.");
		return 1;
	}

	memcpy(p_profileid, resp_buffer, sizeof(uint32_t) * ZXDH_SCHE_RSP_LEN);

	return ret;
}

static uint32_t
zxdh_np_agent_channel_plcr_car_rate(uint32_t dev_id,
		uint32_t car_type,
		uint32_t pkt_sign,
		uint32_t profile_id __rte_unused,
		void *p_car_profile_cfg)
{
	uint32_t ret = 0;
	uint32_t resp_buffer[2] = {0};
	uint32_t resp_len = 8;
	uint32_t i = 0;
	ZXDH_AGENT_CHANNEL_MSG_T agent_msg = {0};
	ZXDH_AGENT_CAR_PKT_PROFILE_MSG_T msgpktcfg = {0};
	ZXDH_AGENT_CAR_PROFILE_MSG_T msgcfg = {0};
	ZXDH_STAT_CAR_PROFILE_CFG_T *p_stat_car_profile_cfg = NULL;
	ZXDH_STAT_CAR_PKT_PROFILE_CFG_T *p_stat_pkt_car_profile_cfg = NULL;

	if (car_type == ZXDH_STAT_CAR_A_TYPE && pkt_sign == 1) {
		p_stat_pkt_car_profile_cfg = (ZXDH_STAT_CAR_PKT_PROFILE_CFG_T *)p_car_profile_cfg;
		msgpktcfg.dev_id = 0;
		msgpktcfg.type = ZXDH_PLCR_CAR_PKT_RATE;
		msgpktcfg.car_level = car_type;
		msgpktcfg.cir = p_stat_pkt_car_profile_cfg->cir;
		msgpktcfg.cbs = p_stat_pkt_car_profile_cfg->cbs;
		msgpktcfg.profile_id = p_stat_pkt_car_profile_cfg->profile_id;
		msgpktcfg.pkt_sign = p_stat_pkt_car_profile_cfg->pkt_sign;
		for (i = 0; i < ZXDH_CAR_PRI_MAX; i++)
			msgpktcfg.pri[i] = p_stat_pkt_car_profile_cfg->pri[i];

		agent_msg.msg = (void *)&msgpktcfg;
		agent_msg.msg_len = sizeof(ZXDH_AGENT_CAR_PKT_PROFILE_MSG_T);

		ret = zxdh_np_agent_channel_sync_send(dev_id, &agent_msg, resp_buffer, resp_len);
		if (ret != 0)	{
			PMD_DRV_LOG(ERR, "stat_car_a_type failed.");
			return 1;
		}

		ret = *(uint8_t *)resp_buffer;
	} else {
		p_stat_car_profile_cfg = (ZXDH_STAT_CAR_PROFILE_CFG_T *)p_car_profile_cfg;
		msgcfg.dev_id = 0;
		msgcfg.type = ZXDH_PLCR_CAR_RATE;
		msgcfg.car_level = car_type;
		msgcfg.cir = p_stat_car_profile_cfg->cir;
		msgcfg.cbs = p_stat_car_profile_cfg->cbs;
		msgcfg.profile_id = p_stat_car_profile_cfg->profile_id;
		msgcfg.pkt_sign = p_stat_car_profile_cfg->pkt_sign;
		msgcfg.cd = p_stat_car_profile_cfg->cd;
		msgcfg.cf = p_stat_car_profile_cfg->cf;
		msgcfg.cm = p_stat_car_profile_cfg->cm;
		msgcfg.cir = p_stat_car_profile_cfg->cir;
		msgcfg.cbs = p_stat_car_profile_cfg->cbs;
		msgcfg.eir = p_stat_car_profile_cfg->eir;
		msgcfg.ebs = p_stat_car_profile_cfg->ebs;
		msgcfg.random_disc_e = p_stat_car_profile_cfg->random_disc_e;
		msgcfg.random_disc_c = p_stat_car_profile_cfg->random_disc_c;
		for (i = 0; i < ZXDH_CAR_PRI_MAX; i++) {
			msgcfg.c_pri[i] = p_stat_car_profile_cfg->c_pri[i];
			msgcfg.e_green_pri[i] = p_stat_car_profile_cfg->e_green_pri[i];
			msgcfg.e_yellow_pri[i] = p_stat_car_profile_cfg->e_yellow_pri[i];
		}

		agent_msg.msg = (void *)&msgcfg;
		agent_msg.msg_len = sizeof(ZXDH_AGENT_CAR_PROFILE_MSG_T);

		ret = zxdh_np_agent_channel_sync_send(dev_id, &agent_msg, resp_buffer, resp_len);
		if (ret != 0)	{
			PMD_DRV_LOG(ERR, "stat_car_b_type failed.");
			return 1;
		}

		ret = *(uint8_t *)resp_buffer;
	}

	return ret;
}

static uint32_t
zxdh_np_agent_channel_plcr_profileid_release(uint32_t dev_id, uint32_t vport,
		uint32_t car_type __rte_unused,
		uint32_t profileid)
{
	uint32_t ret = 0;
	uint32_t resp_buffer[2] = {0};

	ZXDH_AGENT_CHANNEL_PLCR_MSG_T msgcfg = {
		.dev_id = 0,
		.type = ZXDH_PLCR_MSG,
		.oper = ZXDH_PROFILEID_RELEASE,
		.vport = vport,
		.profile_id = profileid,
	};

	ret = zxdh_np_agent_channel_plcr_sync_send(dev_id, &msgcfg,
		resp_buffer, sizeof(resp_buffer));
	if (ret != 0)	{
		PMD_DRV_LOG(ERR, "agent_channel_plcr_sync_send failed.");
		return 1;
	}

	ret = *(uint8_t *)resp_buffer;

	return ret;
}

static uint32_t
zxdh_np_stat_cara_queue_cfg_set(uint32_t dev_id,
		uint32_t flow_id,
		uint32_t drop_flag,
		uint32_t plcr_en,
		uint32_t profile_id)
{
	uint32_t rc = 0;

	ZXDH_STAT_CAR0_CARA_QUEUE_RAM0_159_0_T queue_cfg = {
		.cara_drop = drop_flag,
		.cara_plcr_en = plcr_en,
		.cara_profile_id = profile_id,
	};

	rc = zxdh_np_reg_write(dev_id,
				ZXDH_STAT_CAR0_CARA_QUEUE_RAM0,
				0,
				flow_id,
				&queue_cfg);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_reg_write");

	return rc;
}

static uint32_t
zxdh_np_stat_carb_queue_cfg_set(uint32_t dev_id,
		uint32_t flow_id,
		uint32_t drop_flag,
		uint32_t plcr_en,
		uint32_t profile_id)
{
	uint32_t rc = 0;

	ZXDH_STAT_CAR0_CARB_QUEUE_RAM0_159_0_T queue_cfg = {
		.carb_drop = drop_flag,
		.carb_plcr_en = plcr_en,
		.carb_profile_id = profile_id,
	};

	rc = zxdh_np_reg_write(dev_id,
				ZXDH_STAT_CAR0_CARB_QUEUE_RAM0,
				0,
				flow_id,
				&queue_cfg);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_reg_write");

	return rc;
}

static uint32_t
zxdh_np_stat_carc_queue_cfg_set(uint32_t dev_id,
		uint32_t flow_id,
		uint32_t drop_flag,
		uint32_t plcr_en,
		uint32_t profile_id)
{
	uint32_t rc = 0;

	ZXDH_STAT_CAR0_CARC_QUEUE_RAM0_159_0_T queue_cfg = {
		.carc_drop = drop_flag,
		.carc_plcr_en = plcr_en,
		.carc_profile_id = profile_id,
	};

	rc = zxdh_np_reg_write(dev_id,
						ZXDH_STAT_CAR0_CARC_QUEUE_RAM0,
						0,
						flow_id,
						&queue_cfg);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_reg_write");

	return rc;
}

uint32_t
zxdh_np_car_profile_id_add(uint32_t dev_id,
		uint32_t vport_id,
		uint32_t flags,
		uint64_t *p_profile_id)
{
	uint32_t ret = 0;
	uint32_t *profile_id;
	uint32_t profile_id_h = 0;
	uint32_t profile_id_l = 0;
	uint64_t temp_profile_id = 0;

	profile_id = rte_zmalloc(NULL, ZXDH_G_PROFILE_ID_LEN, 0);
	if (profile_id == NULL) {
		PMD_DRV_LOG(ERR, "profile_id point null!");
		return ZXDH_PAR_CHK_POINT_NULL;
	}
	ret = zxdh_np_agent_channel_plcr_profileid_request(dev_id, vport_id, flags, profile_id);

	profile_id_h = *(profile_id + 1);
	profile_id_l = *profile_id;
	rte_free(profile_id);

	temp_profile_id = (((uint64_t)profile_id_l) << 32) | ((uint64_t)profile_id_h);
	if (0 != (uint32_t)(temp_profile_id >> 56)) {
		PMD_DRV_LOG(ERR, "profile_id is overflow!");
		return 1;
	}

	*p_profile_id = temp_profile_id;

	return ret;
}

uint32_t
zxdh_np_car_profile_cfg_set(uint32_t dev_id,
		uint32_t vport_id __rte_unused,
		uint32_t car_type,
		uint32_t pkt_sign,
		uint32_t profile_id,
		void *p_car_profile_cfg)
{
	uint32_t ret = 0;

	ret = zxdh_np_agent_channel_plcr_car_rate(dev_id, car_type,
		pkt_sign, profile_id, p_car_profile_cfg);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "plcr_car_rate set failed!");
		return 1;
	}

	return ret;
}

uint32_t
zxdh_np_car_profile_id_delete(uint32_t dev_id, uint32_t vport_id,
	uint32_t flags, uint64_t profile_id)
{
	uint32_t ret = 0;
	uint32_t profileid = profile_id & 0xFFFF;

	ret = zxdh_np_agent_channel_plcr_profileid_release(dev_id, vport_id, flags, profileid);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "plcr profiled id release failed!");
		return 1;
	}

	return 0;
}

uint32_t
zxdh_np_stat_car_queue_cfg_set(uint32_t dev_id,
		uint32_t car_type,
		uint32_t flow_id,
		uint32_t drop_flag,
		uint32_t plcr_en,
		uint32_t profile_id)
{
	uint32_t rc = 0;

	if (car_type == ZXDH_STAT_CAR_A_TYPE) {
		if (flow_id > ZXDH_CAR_A_FLOW_ID_MAX) {
			PMD_DRV_LOG(ERR, "stat car a type flow_id invalid!");
			return ZXDH_PAR_CHK_INVALID_INDEX;
		}

		if (profile_id > ZXDH_CAR_A_PROFILE_ID_MAX) {
			PMD_DRV_LOG(ERR, "stat car a type profile_id invalid!");
			return ZXDH_PAR_CHK_INVALID_INDEX;
		}
	} else if (car_type == ZXDH_STAT_CAR_B_TYPE) {
		if (flow_id > ZXDH_CAR_B_FLOW_ID_MAX) {
			PMD_DRV_LOG(ERR, "stat car b type flow_id invalid!");
			return ZXDH_PAR_CHK_INVALID_INDEX;
		}

		if (profile_id > ZXDH_CAR_B_PROFILE_ID_MAX) {
			PMD_DRV_LOG(ERR, "stat car b type profile_id invalid!");
			return ZXDH_PAR_CHK_INVALID_INDEX;
		}
	} else {
		if (flow_id > ZXDH_CAR_C_FLOW_ID_MAX) {
			PMD_DRV_LOG(ERR, "stat car c type flow_id invalid!");
			return ZXDH_PAR_CHK_INVALID_INDEX;
		}

		if (profile_id > ZXDH_CAR_C_PROFILE_ID_MAX) {
			PMD_DRV_LOG(ERR, "stat car c type profile_id invalid!");
			return ZXDH_PAR_CHK_INVALID_INDEX;
		}
	}

	switch (car_type) {
	case ZXDH_STAT_CAR_A_TYPE:
		rc = zxdh_np_stat_cara_queue_cfg_set(dev_id,
					flow_id,
					drop_flag,
					plcr_en,
					profile_id);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "stat_cara_queue_cfg_set");
	break;

	case ZXDH_STAT_CAR_B_TYPE:
		rc = zxdh_np_stat_carb_queue_cfg_set(dev_id,
					flow_id,
					drop_flag,
					plcr_en,
					profile_id);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "stat_carb_queue_cfg_set");
	break;

	case ZXDH_STAT_CAR_C_TYPE:
		rc = zxdh_np_stat_carc_queue_cfg_set(dev_id,
					flow_id,
					drop_flag,
					plcr_en,
					profile_id);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "stat_carc_queue_cfg_set");
	break;
	}

	return rc;
}

uint32_t
zxdh_np_dtb_acl_index_request(uint32_t dev_id,
	uint32_t sdt_no, uint32_t vport, uint32_t *p_index)
{
	ZXDH_SDT_TBL_ETCAM_T sdt_acl = {0};
	ZXDH_SDT_TBL_ERAM_T sdt_eram = {0};

	uint32_t rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_acl);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");
	if (sdt_acl.table_type != ZXDH_SDT_TBLT_ETCAM) {
		PMD_DRV_LOG(ERR, "SDT[%u] table_type[ %u ] is not etcam table!",
			sdt_no, sdt_acl.table_type);
		return ZXDH_ERR;
	}

	uint32_t eram_sdt_no = zxdh_np_apt_get_sdt_partner(dev_id, sdt_no);

	rc = zxdh_np_soft_sdt_tbl_get(dev_id, eram_sdt_no, &sdt_eram);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");
	if (sdt_eram.table_type != ZXDH_SDT_TBLT_ERAM) {
		PMD_DRV_LOG(ERR, "SDT[%u] table_type[ %u ] is not eram table!",
			eram_sdt_no, sdt_eram.table_type);
		return ZXDH_ERR;
	}

	uint32_t index = 0;
	ZXDH_SPINLOCK_T *p_dtb_spinlock = NULL;
	ZXDH_DEV_SPINLOCK_TYPE_E spinlock = ZXDH_DEV_SPINLOCK_T_DTB;
	rc = zxdh_np_dev_opr_spinlock_get(dev_id, (uint32_t)spinlock, &p_dtb_spinlock);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dev_opr_spinlock_get");

	rte_spinlock_lock(&p_dtb_spinlock->spinlock);
	rc = zxdh_np_agent_channel_acl_index_request(dev_id, sdt_no, vport, &index);
	rte_spinlock_unlock(&p_dtb_spinlock->spinlock);

	*p_index = index;

	return rc;
}

uint32_t
zxdh_np_dtb_acl_index_release(uint32_t dev_id,
	uint32_t sdt_no, uint32_t vport, uint32_t index)
{
	ZXDH_SDT_TBL_ETCAM_T sdt_acl = {0};
	ZXDH_SDT_TBL_ERAM_T sdt_eram = {0};

	uint32_t rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_acl);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");
	if (sdt_acl.table_type != ZXDH_SDT_TBLT_ETCAM) {
		PMD_DRV_LOG(ERR, "SDT[%u] table_type[ %u ] is not etcam table!",
			sdt_no, sdt_acl.table_type);
		return ZXDH_ERR;
	}

	uint32_t eram_sdt_no = zxdh_np_apt_get_sdt_partner(dev_id, sdt_no);

	rc = zxdh_np_soft_sdt_tbl_get(dev_id, eram_sdt_no, &sdt_eram);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");
	if (sdt_eram.table_type != ZXDH_SDT_TBLT_ERAM) {
		PMD_DRV_LOG(ERR, "SDT[%u] table_type[ %u ] is not eram table!",
			eram_sdt_no, sdt_eram.table_type);
		return ZXDH_ERR;
	}

	ZXDH_SPINLOCK_T *p_dtb_spinlock = NULL;
	ZXDH_DEV_SPINLOCK_TYPE_E spinlock = ZXDH_DEV_SPINLOCK_T_DTB;
	rc = zxdh_np_dev_opr_spinlock_get(dev_id, (uint32_t)spinlock, &p_dtb_spinlock);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dev_opr_spinlock_get");

	rte_spinlock_lock(&p_dtb_spinlock->spinlock);

	rc = zxdh_np_agent_channel_acl_index_release(dev_id,
			ZXDH_ACL_INDEX_RELEASE, sdt_no, vport, index);

	rte_spinlock_unlock(&p_dtb_spinlock->spinlock);

	return rc;
}

static uint32_t
zxdh_np_dtb_sdt_eram_table_dump(uint32_t dev_id, uint32_t queue_id, uint32_t sdt_no,
	uint32_t start_index, uint32_t depth, uint32_t *p_data, uint32_t *element_id)
{
	uint32_t dump_item_index = 0;
	uint64_t dump_sdt_phy_addr = 0;
	uint64_t dump_sdt_vir_addr = 0;
	uint32_t dump_addr_size = 0;

	uint32_t rc = zxdh_np_dtb_dump_sdt_addr_get(dev_id,
					queue_id,
					sdt_no,
					&dump_sdt_phy_addr,
					&dump_sdt_vir_addr,
					&dump_addr_size);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_dump_sdt_addr_get");

	memset((uint8_t *)dump_sdt_vir_addr, 0, dump_addr_size);
	rc = zxdh_np_dtb_tab_up_free_item_get(dev_id, queue_id, &dump_item_index);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_tab_up_free_item_get");
	PMD_DRV_LOG(DEBUG, "dump queue id %u, element_id is: %u.",
		queue_id, dump_item_index);

	*element_id = dump_item_index;

	rc = zxdh_np_dtb_tab_up_item_user_addr_set(dev_id,
					queue_id,
					dump_item_index,
					dump_sdt_phy_addr,
					dump_sdt_vir_addr);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_tab_up_item_addr_set");

	uint32_t dump_dst_phy_haddr = 0;
	uint32_t dump_dst_phy_laddr = 0;
	rc = zxdh_np_dtb_tab_up_item_addr_get(dev_id, queue_id, dump_item_index,
		&dump_dst_phy_haddr, &dump_dst_phy_laddr);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_tab_up_item_addr_get");

	uint8_t form_buff[ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8] = {0};
	ZXDH_SDT_TBL_ERAM_T sdt_eram	= {0};

	rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_eram);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");

	uint32_t eram_base_addr = sdt_eram.eram_base_addr;
	uint32_t dump_addr_128bit = eram_base_addr + start_index;

	rc = zxdh_np_dtb_smmu0_dump_info_write(dev_id,
					dump_addr_128bit,
					depth,
					dump_dst_phy_haddr,
					dump_dst_phy_laddr,
					(uint32_t *)form_buff);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_smmu0_dump_info_write");

	uint32_t dump_data_len = depth * 128 / 32;
	uint32_t dump_desc_len = ZXDH_DTB_LEN_POS_SETP / 4;

	if (dump_data_len * 4 > dump_addr_size) {
		PMD_DRV_LOG(ERR, "eram dump size is too small!");
		return ZXDH_RC_DTB_DUMP_SIZE_SMALL;
	}

	rc = zxdh_np_dtb_write_dump_desc_info(dev_id,
					queue_id,
					dump_item_index,
					(uint32_t *)form_buff,
					dump_data_len,
					dump_desc_len,
					p_data);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_write_dump_desc_info");

	return rc;
}

static uint32_t
zxdh_np_dtb_eram_table_dump(uint32_t dev_id,
		uint32_t queue_id,
		uint32_t sdt_no,
		ZXDH_DTB_DUMP_INDEX_T start_index,
		ZXDH_DTB_ERAM_ENTRY_INFO_T *p_dump_data_arr,
		uint32_t *entry_num,
		__rte_unused ZXDH_DTB_DUMP_INDEX_T *next_start_index,
		uint32_t *finish_flag)
{
	uint32_t start_index_128bit = 0;
	uint32_t col_index = 0;
	uint32_t dump_depth_128bit = 0;
	uint32_t element_id = 0;
	ZXDH_SDT_TBL_ERAM_T sdt_eram = {0};

	uint32_t rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_eram);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");

	uint32_t dump_mode = sdt_eram.eram_mode;
	uint32_t eram_table_depth = sdt_eram.eram_table_depth;

	zxdh_np_eram_index_cal(dump_mode, eram_table_depth,
		&dump_depth_128bit, &col_index);

	zxdh_np_eram_index_cal(dump_mode, start_index.index,
		&start_index_128bit, &col_index);

	uint32_t dump_depth = dump_depth_128bit - start_index_128bit;

	uint8_t *dump_data_buff = malloc(dump_depth * ZXDH_DTB_LEN_POS_SETP);
	if (dump_data_buff == NULL) {
		PMD_DRV_LOG(ERR, "%s point null!", __func__);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	rc = zxdh_np_dtb_sdt_eram_table_dump(dev_id,
				queue_id,
				sdt_no,
				start_index_128bit,
				dump_depth,
				(uint32_t *)dump_data_buff,
				&element_id);

	if (dump_mode == ZXDH_ERAM128_TBL_128b) {
		for (uint32_t i = 0; i < dump_depth; i++) {
			ZXDH_DTB_ERAM_ENTRY_INFO_T *p_dump_user_data = p_dump_data_arr + i;
			uint8_t *temp_data = dump_data_buff + i * ZXDH_DTB_LEN_POS_SETP;
			if (p_dump_user_data == NULL || p_dump_user_data->p_data == NULL) {
				PMD_DRV_LOG(ERR, "data buff is NULL!");
				free(dump_data_buff);
				return ZXDH_ERR;
			}

			p_dump_user_data->index = start_index.index + i;
			memcpy(p_dump_user_data->p_data, temp_data, (128 / 8));
		}
	} else if (dump_mode == ZXDH_ERAM128_TBL_64b) {
		uint32_t row_index = 0;
		uint32_t remain =  start_index.index % 2;
		for (uint32_t i = 0; i < eram_table_depth - start_index.index; i++) {
			zxdh_np_eram_index_cal(dump_mode, remain, &row_index, &col_index);
			uint8_t *temp_data = dump_data_buff + row_index * ZXDH_DTB_LEN_POS_SETP;

			uint32_t *buff = (uint32_t *)temp_data;
			ZXDH_DTB_ERAM_ENTRY_INFO_T *p_dump_user_data = p_dump_data_arr + i;

			if (p_dump_user_data == NULL || p_dump_user_data->p_data == NULL) {
				PMD_DRV_LOG(ERR, "data buff is NULL!");
				free(dump_data_buff);
				return ZXDH_ERR;
			}

			p_dump_user_data->index = start_index.index + i;
			memcpy(p_dump_user_data->p_data,
				buff + ((1 - col_index) << 1), (64 / 8));

			remain++;
		}
	}

	*entry_num = eram_table_depth - start_index.index;
	*finish_flag = 1;
	PMD_DRV_LOG(DEBUG, "dump entry num %u, finish flag %u", *entry_num, *finish_flag);

	free(dump_data_buff);

	return ZXDH_OK;
}

static uint32_t
zxdh_np_dtb_acl_index_parse(uint32_t dev_id,
			uint32_t queue_id,
			uint32_t eram_sdt_no,
			uint32_t vport,
			uint32_t *index_num,
			uint32_t *p_index_array)
{
	ZXDH_SDT_TBL_ERAM_T sdt_eram = {0};

	uint32_t rc = zxdh_np_soft_sdt_tbl_get(dev_id, eram_sdt_no, &sdt_eram);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");

	uint32_t byte_num = (sdt_eram.eram_mode == ZXDH_ERAM128_TBL_64b) ? 8 : 16;
	uint32_t eram_table_depth = sdt_eram.eram_table_depth;
	ZXDH_DTB_ERAM_ENTRY_INFO_T *p_dump_data_arr = malloc(eram_table_depth *
		sizeof(ZXDH_DTB_ERAM_ENTRY_INFO_T));
	if (p_dump_data_arr == NULL) {
		PMD_DRV_LOG(ERR, "p_dump_data_arr point null!");
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	uint8_t *data_buff = malloc(byte_num * eram_table_depth);
	if (data_buff == NULL) {
		PMD_DRV_LOG(ERR, "data_buff point null!");
		free(p_dump_data_arr);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	for (uint32_t i = 0; i < eram_table_depth; i++) {
		p_dump_data_arr[i].index = i;
		p_dump_data_arr[i].p_data = (uint32_t *)(data_buff + i * byte_num);
	}

	uint32_t entry_num = 0;
	uint32_t valid_entry_num = 0;
	uint32_t finish_flag = 0;
	ZXDH_DTB_DUMP_INDEX_T start_index = {0};
	ZXDH_DTB_DUMP_INDEX_T next_start_index = {0};
	rc = zxdh_np_dtb_eram_table_dump(dev_id,
				queue_id,
				eram_sdt_no,
				start_index,
				p_dump_data_arr,
				&entry_num,
				&next_start_index,
				&finish_flag);

	for (uint32_t i = 0; i < entry_num; i++) {
		uint8_t valid = (p_dump_data_arr[i].p_data[0] >> 31) & 0x1;
		uint32_t temp_vport = p_dump_data_arr[i].p_data[0] & 0x7fffffff;
		if (valid && temp_vport == vport) {
			p_index_array[valid_entry_num] = i;
			valid_entry_num++;
		}
	}

	*index_num = valid_entry_num;
	free(data_buff);
	free(p_dump_data_arr);

	return rc;
}

static uint32_t
zxdh_np_dtb_etcam_ind_data_get(uint8_t *p_in_data, uint32_t rd_mode, uint8_t *p_out_data)
{
	uint8_t buff[ZXDH_ETCAM_WIDTH_MAX / 8] = {0};

	memcpy(buff, p_in_data, ZXDH_ETCAM_WIDTH_MAX / 8);
	zxdh_np_comm_swap(buff, ZXDH_ETCAM_WIDTH_MAX / 8);

	uint8_t *p_temp = p_out_data;
	for (uint32_t i = 0; i < ZXDH_ETCAM_RAM_NUM; i++) {
		uint32_t offset = i * (ZXDH_ETCAM_WIDTH_MIN / 8);

		if ((rd_mode >> (ZXDH_ETCAM_RAM_NUM - 1 - i)) & 0x1) {
			memcpy(p_temp, buff + offset, ZXDH_ETCAM_WIDTH_MIN / 8);
			p_temp += ZXDH_ETCAM_WIDTH_MIN / 8;
		}
	}

	return ZXDH_OK;
}

static uint32_t
zxdh_np_dtb_acl_table_dump(uint32_t dev_id,
		uint32_t queue_id,
		uint32_t sdt_no,
		__rte_unused ZXDH_DTB_DUMP_INDEX_T start_index,
		ZXDH_DTB_ACL_ENTRY_INFO_T *p_dump_data_arr,
		uint32_t *p_entry_num,
		__rte_unused ZXDH_DTB_DUMP_INDEX_T *next_start_index,
		uint32_t *p_finish_flag)
{
	ZXDH_SDT_TBL_ETCAM_T sdt_etcam_info = {0};

	uint32_t rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_etcam_info);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");
	uint32_t etcam_key_mode = sdt_etcam_info.etcam_key_mode;
	uint32_t etcam_as_mode = sdt_etcam_info.as_rsp_mode;
	uint32_t etcam_table_id = sdt_etcam_info.etcam_table_id;
	uint32_t as_enable = sdt_etcam_info.as_en;
	uint32_t as_eram_baddr = sdt_etcam_info.as_eram_baddr;
	uint32_t etcam_table_depth = sdt_etcam_info.etcam_table_depth;

	ZXDH_ACL_CFG_EX_T *p_acl_cfg = NULL;
	zxdh_np_acl_cfg_get(dev_id, &p_acl_cfg);
	ZXDH_ACL_TBL_CFG_T *p_tbl_cfg = p_acl_cfg->acl_tbls + etcam_table_id;

	if (!p_tbl_cfg->is_used) {
		PMD_DRV_LOG(ERR, "table[ %u ] is not init!", etcam_table_id);
		RTE_ASSERT(0);
		return ZXDH_ACL_RC_TBL_NOT_INIT;
	}

	uint32_t data_byte_size = ZXDH_ETCAM_ENTRY_SIZE_GET(etcam_key_mode);
	if (data_byte_size > ZXDH_ETCAM_RAM_WIDTH) {
		PMD_DRV_LOG(ERR, "etcam date size is over 80B!");
		return ZXDH_ACL_RC_INVALID_PARA;
	}

	uint32_t dump_element_id = 0;
	rc = zxdh_np_dtb_dump_addr_set(dev_id, queue_id, sdt_no, &dump_element_id);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_dump_addr_set");

	uint8_t *dump_info_buff = malloc(ZXDH_DTB_TABLE_DUMP_INFO_BUFF_SIZE);
	if (dump_info_buff == NULL) {
		PMD_DRV_LOG(ERR, "%s point null!", __func__);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	uint32_t dtb_desc_addr_offset = 0;
	uint32_t dump_data_len = 0;
	uint32_t dtb_desc_len = 0;
	uint32_t etcam_data_len_offset = 0;
	uint32_t etcam_mask_len_offset = 0;
	uint32_t block_num = p_tbl_cfg->block_num;
	uint8_t cmd_buff[ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8] = {0};
	ZXDH_DTB_ENTRY_T dtb_dump_entry = {0};
	dtb_dump_entry.cmd = cmd_buff;

	for (uint32_t i = 0; i < block_num; i++) {
		ZXDH_ETCAM_DUMP_INFO_T etcam_dump_info = {0};
		uint32_t etcam_data_dst_phy_haddr = 0;
		uint32_t etcam_data_dst_phy_laddr = 0;
		uint32_t block_idx = p_tbl_cfg->block_array[i];

		PMD_DRV_LOG(DEBUG, "block_idx: %u", block_idx);

		etcam_dump_info.block_sel = block_idx;
		etcam_dump_info.addr = 0;
		etcam_dump_info.tb_width = 3;
		etcam_dump_info.rd_mode = 0xFF;
		etcam_dump_info.tb_depth = ZXDH_ETCAM_RAM_DEPTH;
		etcam_dump_info.data_or_mask = ZXDH_ETCAM_DTYPE_DATA;

		zxdh_np_dtb_tab_up_item_offset_addr_get(dev_id,
					queue_id,
					dump_element_id,
					dump_data_len,
					&etcam_data_dst_phy_haddr,
					&etcam_data_dst_phy_laddr);

		zxdh_np_dtb_etcam_dump_entry(dev_id,
					&etcam_dump_info,
					etcam_data_dst_phy_haddr,
					etcam_data_dst_phy_laddr,
					&dtb_dump_entry);

		zxdh_np_dtb_data_write(dump_info_buff, dtb_desc_addr_offset, &dtb_dump_entry);

		memset(cmd_buff, 0, ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8);

		dtb_desc_len += 1;
		dtb_desc_addr_offset += ZXDH_DTB_LEN_POS_SETP;
		dump_data_len += ZXDH_ETCAM_RAM_DEPTH * 640 / 8;
	}

	etcam_data_len_offset = dump_data_len;

	for (uint32_t i = 0; i < block_num; i++) {
		ZXDH_ETCAM_DUMP_INFO_T etcam_dump_info = {0};
		uint32_t etcam_mask_dst_phy_haddr = 0;
		uint32_t etcam_mask_dst_phy_laddr = 0;
		uint32_t block_idx = p_tbl_cfg->block_array[i];

		PMD_DRV_LOG(DEBUG, "mask: block_idx: %u", block_idx);

		etcam_dump_info.block_sel = block_idx;
		etcam_dump_info.addr = 0;
		etcam_dump_info.tb_width = 3;
		etcam_dump_info.rd_mode = 0xFF;
		etcam_dump_info.tb_depth = ZXDH_ETCAM_RAM_DEPTH;
		etcam_dump_info.data_or_mask = ZXDH_ETCAM_DTYPE_MASK;

		zxdh_np_dtb_tab_up_item_offset_addr_get(dev_id,
					queue_id,
					dump_element_id,
					dump_data_len,
					&etcam_mask_dst_phy_haddr,
					&etcam_mask_dst_phy_laddr);

		zxdh_np_dtb_etcam_dump_entry(dev_id,
					&etcam_dump_info,
					etcam_mask_dst_phy_haddr,
					etcam_mask_dst_phy_laddr,
					&dtb_dump_entry);

		zxdh_np_dtb_data_write(dump_info_buff, dtb_desc_addr_offset, &dtb_dump_entry);

		memset(cmd_buff, 0, ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8);

		dtb_desc_len += 1;
		dtb_desc_addr_offset += ZXDH_DTB_LEN_POS_SETP;
		dump_data_len += ZXDH_ETCAM_RAM_DEPTH * 640 / 8;
	}
	etcam_mask_len_offset = dump_data_len;

	if (as_enable) {
		uint32_t as_rst_dst_phy_haddr = 0;
		uint32_t as_rst_dst_phy_laddr = 0;
		uint32_t dump_eram_depth_128bit = 0;
		uint32_t eram_col_index = 0;

		zxdh_np_eram_index_cal(etcam_as_mode,
			etcam_table_depth, &dump_eram_depth_128bit, &eram_col_index);

		zxdh_np_dtb_tab_up_item_offset_addr_get(dev_id,
					queue_id,
					dump_element_id,
					dump_data_len,
					&as_rst_dst_phy_haddr,
					&as_rst_dst_phy_laddr);

		zxdh_np_dtb_smmu0_dump_entry(dev_id,
					as_eram_baddr,
					dump_eram_depth_128bit,
					as_rst_dst_phy_haddr,
					as_rst_dst_phy_laddr,
					&dtb_dump_entry);

		zxdh_np_dtb_data_write(dump_info_buff, dtb_desc_addr_offset, &dtb_dump_entry);

		memset(cmd_buff, 0, ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8);
		dtb_desc_len += 1;
		dtb_desc_addr_offset += ZXDH_DTB_LEN_POS_SETP;
		dump_data_len += dump_eram_depth_128bit * 128 / 8;
	}

	uint8_t *temp_dump_out_data = malloc(dump_data_len * sizeof(uint8_t));
	if (temp_dump_out_data == NULL) {
		PMD_DRV_LOG(ERR, "temp_dump_out_data point null!");
		free(dump_info_buff);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	rc = zxdh_np_dtb_write_dump_desc_info(dev_id,
				queue_id,
				dump_element_id,
				(uint32_t *)dump_info_buff,
				dump_data_len / 4,
				dtb_desc_len * 4,
				(uint32_t *)temp_dump_out_data);
	free(dump_info_buff);

	uint8_t *p_data_start = temp_dump_out_data;
	uint8_t *p_mask_start = temp_dump_out_data + etcam_data_len_offset;
	uint8_t *p_rst_start = NULL;
	if (as_enable)
		p_rst_start = temp_dump_out_data + etcam_mask_len_offset;

	for (uint32_t handle = 0; handle < etcam_table_depth; handle++) {
		ZXDH_DTB_ACL_ENTRY_INFO_T *p_dump_user_data = p_dump_data_arr + handle;

		if (p_dump_user_data == NULL ||
			p_dump_user_data->key_data == NULL ||
			p_dump_user_data->key_mask == NULL) {
			PMD_DRV_LOG(ERR, "etcam handle 0x%x data user buff is NULL!", handle);
			free(temp_dump_out_data);
			return ZXDH_ERR;
		}

		if (as_enable) {
			if (p_dump_user_data->p_as_rslt == NULL) {
				PMD_DRV_LOG(ERR, "handle 0x%x data buff is NULL!", handle);
				free(temp_dump_out_data);
				return ZXDH_ERR;
			}
		}

		p_dump_user_data->handle = handle;

		uint32_t shift_amount = 8U >> etcam_key_mode;
		uint32_t mask_base = (1U << shift_amount) - 1;
		uint32_t offset = shift_amount * (handle % (1U << etcam_key_mode));
		uint32_t rd_mask = (mask_base << offset) & 0xFF;

		uint32_t addr_640bit = handle / (1U << etcam_key_mode);
		uint8_t *p_data_640bit = p_data_start + addr_640bit * 640 / 8;
		uint8_t *p_mask_640bit = p_mask_start + addr_640bit * 640 / 8;

		uint8_t xy_data[ZXDH_ETCAM_WIDTH_MAX / 8] = {0};
		uint8_t xy_mask[ZXDH_ETCAM_WIDTH_MAX / 8] = {0};
		uint8_t dm_data[ZXDH_ETCAM_WIDTH_MAX / 8] = {0};
		uint8_t dm_mask[ZXDH_ETCAM_WIDTH_MAX / 8] = {0};
		ZXDH_ETCAM_ENTRY_T entry_xy = {
			.p_data = xy_data,
			.p_mask = xy_mask,
			.mode = 0,
		};
		ZXDH_ETCAM_ENTRY_T entry_dm = {
			.p_data = dm_data,
			.p_mask = dm_mask,
			.mode = 0,
		};

		zxdh_np_dtb_etcam_ind_data_get(p_data_640bit, rd_mask, entry_xy.p_data);
		zxdh_np_dtb_etcam_ind_data_get(p_mask_640bit, rd_mask, entry_xy.p_mask);

		zxdh_np_etcam_xy_to_dm(&entry_dm, &entry_xy, data_byte_size);

		memcpy(p_dump_user_data->key_data, entry_dm.p_data, data_byte_size);
		memcpy(p_dump_user_data->key_mask, entry_dm.p_mask, data_byte_size);

		if (as_enable) {
			uint32_t eram_row_index = 0;
			uint32_t eram_col_index = 0;

			zxdh_np_eram_index_cal(etcam_as_mode,
				handle, &eram_row_index, &eram_col_index);

			uint8_t *p_rst_128bit = p_rst_start +
							eram_row_index * ZXDH_DTB_LEN_POS_SETP;
			uint32_t *eram_buff = (uint32_t *)p_rst_128bit;

			if (etcam_as_mode == ZXDH_ERAM128_TBL_128b)
				memcpy(p_dump_user_data->p_as_rslt, eram_buff, (128 / 8));
			else if (etcam_as_mode == ZXDH_ERAM128_TBL_64b)
				memcpy(p_dump_user_data->p_as_rslt,
					eram_buff + ((1 - eram_col_index) << 1), (64 / 8));
		}
	}

	*p_entry_num = etcam_table_depth;
	*p_finish_flag = 1;

	free(temp_dump_out_data);

	return ZXDH_OK;
}

static uint32_t
zxdh_np_smmu0_tbl_size_get(uint32_t eram_mode)
{
	uint32_t size = 0;
	if (eram_mode == ZXDH_ERAM128_TBL_128b)
		size = 16;
	else if (eram_mode == ZXDH_ERAM128_TBL_64b)
		size = 8;
	else if (eram_mode == ZXDH_ERAM128_TBL_32b)
		size = 4;
	else
		size = 1;

	return size;
}

static uint32_t
zxdh_np_dtb_acl_data_get_by_handle(uint32_t dev_id,
								uint32_t queue_id,
								uint32_t sdt_no,
								uint32_t index_num,
								uint32_t *p_index_array,
								uint8_t *p_dump_data)
{
	ZXDH_SDT_TBL_ETCAM_T sdt_etcam_info = {0};

	uint32_t rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_etcam_info);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");
	if (sdt_etcam_info.table_type != ZXDH_SDT_TBLT_ETCAM) {
		PMD_DRV_LOG(ERR, "SDT[%u] table_type[ %u ] is not etcam table!",
			sdt_no, sdt_etcam_info.table_type);
		return ZXDH_ERR;
	}

	uint32_t etcam_key_mode = sdt_etcam_info.etcam_key_mode;
	uint32_t etcam_table_depth = sdt_etcam_info.etcam_table_depth;
	uint32_t as_len = zxdh_np_smmu0_tbl_size_get(sdt_etcam_info.as_rsp_mode);
	uint32_t data_byte_size = ZXDH_ETCAM_ENTRY_SIZE_GET(etcam_key_mode);

	ZXDH_DTB_ACL_ENTRY_INFO_T *p_dtb_acl_entry = malloc(etcam_table_depth *
		sizeof(ZXDH_DTB_ACL_ENTRY_INFO_T));
	if (p_dtb_acl_entry == NULL) {
		PMD_DRV_LOG(ERR, "%s point null!", __func__);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	uint8_t *data_buff = malloc(etcam_table_depth * data_byte_size);
	if (data_buff == NULL) {
		PMD_DRV_LOG(ERR, "data_buff point null!");
		free(p_dtb_acl_entry);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	uint8_t *mask_buff = malloc(etcam_table_depth * data_byte_size);
	if (mask_buff == NULL) {
		PMD_DRV_LOG(ERR, "mask_buff point null!");
		free(data_buff);
		free(p_dtb_acl_entry);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	uint8_t *eram_buff = malloc(etcam_table_depth * as_len);
	if (eram_buff == NULL) {
		PMD_DRV_LOG(ERR, "eram_buff point null!");
		free(mask_buff);
		free(data_buff);
		free(p_dtb_acl_entry);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	for (uint32_t i = 0; i < etcam_table_depth; i++) {
		p_dtb_acl_entry[i].handle = i;
		p_dtb_acl_entry[i].key_data = data_buff + i * data_byte_size;
		p_dtb_acl_entry[i].key_mask = mask_buff + i * data_byte_size;
		p_dtb_acl_entry[i].p_as_rslt = eram_buff + i * as_len;
	}

	uint32_t entry_num = 0;
	uint32_t finish_flag = 0;
	ZXDH_DTB_DUMP_INDEX_T start_index = {0};
	ZXDH_DTB_DUMP_INDEX_T next_start_index = {0};
	rc = zxdh_np_dtb_acl_table_dump(dev_id,
				queue_id,
				sdt_no,
				start_index,
				p_dtb_acl_entry,
				&entry_num,
				&next_start_index,
				&finish_flag);
	if (rc != ZXDH_OK) {
		PMD_DRV_LOG(ERR, "acl sdt[%u] dump fail, rc:0x%x", sdt_no, rc);
		free(data_buff);
		free(mask_buff);
		free(eram_buff);
		free(p_dtb_acl_entry);
		return rc;
	}

	for (uint32_t i = 0; i < index_num; i++) {
		ZXDH_DTB_ACL_ENTRY_INFO_T *p_dump_entry =
					((ZXDH_DTB_ACL_ENTRY_INFO_T *)p_dump_data) + i;
		p_dump_entry->handle = p_index_array[i];
		ZXDH_DTB_ACL_ENTRY_INFO_T *p_temp_entry = p_dtb_acl_entry + p_index_array[i];
		memcpy(p_dump_entry->key_data, p_temp_entry->key_data, data_byte_size);
		memcpy(p_dump_entry->key_mask, p_temp_entry->key_mask, data_byte_size);
		memcpy(p_dump_entry->p_as_rslt, p_temp_entry->p_as_rslt, as_len);
	}

	free(data_buff);
	free(mask_buff);
	free(eram_buff);
	free(p_dtb_acl_entry);

	return rc;
}

uint32_t
zxdh_np_dtb_acl_table_dump_by_vport(uint32_t dev_id, uint32_t queue_id,
	uint32_t sdt_no, uint32_t vport, uint32_t *entry_num, uint8_t *p_dump_data)
{
	ZXDH_SDT_TBL_ETCAM_T sdt_etcam_info = {0};

	uint32_t rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_etcam_info);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");
	if (sdt_etcam_info.table_type != ZXDH_SDT_TBLT_ETCAM) {
		PMD_DRV_LOG(ERR, "SDT[%u] table_type[ %u ] is not etcam table!",
					sdt_no, sdt_etcam_info.table_type);
		return ZXDH_ERR;
	}

	uint32_t eram_sdt_no = zxdh_np_apt_get_sdt_partner(dev_id, sdt_no);
	ZXDH_SDT_TBL_ERAM_T sdt_eram = {0};

	rc = zxdh_np_soft_sdt_tbl_get(dev_id, eram_sdt_no, &sdt_eram);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");
	if (sdt_eram.table_type != ZXDH_SDT_TBLT_ERAM) {
		PMD_DRV_LOG(ERR, "SDT[%u] table_type[ %u ] is not eram table!",
					eram_sdt_no, sdt_eram.table_type);
		return ZXDH_ERR;
	}

	uint32_t *p_index_array = malloc(sizeof(uint32_t) * sdt_eram.eram_table_depth);
	if (p_index_array == NULL) {
		PMD_DRV_LOG(ERR, "%s point null!", __func__);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	uint32_t index_num = 0;
	rc = zxdh_np_dtb_acl_index_parse(dev_id, queue_id,
			eram_sdt_no, vport, &index_num, p_index_array);
	if (rc != ZXDH_OK) {
		free(p_index_array);
		PMD_DRV_LOG(ERR, "acl index parse failed");
		return ZXDH_ERR;
	}

	if (!index_num) {
		PMD_DRV_LOG(ERR, "SDT[%u] vport[0x%x] item num is zero!", sdt_no, vport);
		free(p_index_array);
		return ZXDH_OK;
	}

	rc = zxdh_np_dtb_acl_data_get_by_handle(dev_id, queue_id, sdt_no,
				index_num, p_index_array, p_dump_data);
	if (rc != ZXDH_OK) {
		free(p_index_array);
		PMD_DRV_LOG(ERR, "acl date by handle failed");
		return ZXDH_ERR;
	}

	*entry_num = index_num;
	free(p_index_array);

	return ZXDH_OK;
}

static uint32_t
zxdh_np_dtb_acl_dma_insert_cycle(uint32_t dev_id,
				uint32_t queue_id,
				uint32_t sdt_no,
				uint32_t entry_num,
				ZXDH_DTB_ACL_ENTRY_INFO_T *p_acl_entry_arr,
				uint32_t *element_id)
{
	uint32_t eram_wrt_mode = 0;
	uint32_t addr_offset_bk = 0;
	uint32_t dtb_len = 0;
	uint32_t as_addr_offset = 0;
	uint32_t as_dtb_len = 0;

	ZXDH_ACL_CFG_EX_T *p_acl_cfg = NULL;
	ZXDH_SDT_TBL_ETCAM_T sdt_etcam_info = {0};

	ZXDH_DTB_ENTRY_T entry_data = {0};
	ZXDH_DTB_ENTRY_T entry_mask = {0};

	uint8_t entry_data_buff[ZXDH_ETCAM_WIDTH_MAX / 8] = {0};
	uint8_t entry_mask_buff[ZXDH_ETCAM_WIDTH_MAX / 8] = {0};
	uint8_t entry_data_cmd_buff[ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8] = {0};
	uint8_t entry_mask_cmd_buff[ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8] = {0};

	entry_data.cmd = entry_data_cmd_buff;
	entry_data.data = entry_data_buff;
	entry_mask.cmd = entry_mask_cmd_buff;
	entry_mask.data = entry_mask_buff;

	uint32_t rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_etcam_info);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");
	uint32_t etcam_as_mode = sdt_etcam_info.as_rsp_mode;
	uint32_t etcam_table_id = sdt_etcam_info.etcam_table_id;
	uint32_t as_enable = sdt_etcam_info.as_en;
	uint32_t as_eram_baddr = sdt_etcam_info.as_eram_baddr;

	if (as_enable) {
		switch (etcam_as_mode) {
		case ZXDH_ERAM128_TBL_128b:
			eram_wrt_mode = ZXDH_ERAM128_OPR_128b;
			break;
		case ZXDH_ERAM128_TBL_64b:
			eram_wrt_mode = ZXDH_ERAM128_OPR_64b;
			break;
		case ZXDH_ERAM128_TBL_1b:
			eram_wrt_mode = ZXDH_ERAM128_OPR_1b;
			break;

		default:
			PMD_DRV_LOG(ERR, "etcam_as_mode is invalid!");
			return ZXDH_ERR;
		}
	}

	zxdh_np_acl_cfg_get(dev_id, &p_acl_cfg);
	ZXDH_ACL_TBL_CFG_T *p_tbl_cfg = p_acl_cfg->acl_tbls + etcam_table_id;

	if (!p_tbl_cfg->is_used) {
		PMD_DRV_LOG(ERR, "table[ %u ] is not init!", etcam_table_id);
		RTE_ASSERT(0);
		return ZXDH_ACL_RC_TBL_NOT_INIT;
	}

	uint8_t *table_data_buff = malloc(ZXDH_DTB_TABLE_DATA_BUFF_SIZE);
	if (table_data_buff == NULL) {
		PMD_DRV_LOG(ERR, "%s point null!", __func__);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	for (uint32_t item_cnt = 0; item_cnt < entry_num; ++item_cnt) {
		uint32_t block_idx = 0;
		uint32_t ram_addr = 0;
		uint32_t etcam_wr_mode = 0;
		ZXDH_ETCAM_ENTRY_T etcam_entry = {0};
		ZXDH_DTB_ACL_ENTRY_INFO_T *p_acl_entry = p_acl_entry_arr + item_cnt;

		etcam_entry.mode = p_tbl_cfg->key_mode;
		etcam_entry.p_data = p_acl_entry->key_data;
		etcam_entry.p_mask = p_acl_entry->key_mask;

		zxdh_np_acl_hdw_addr_get(p_tbl_cfg, p_acl_entry->handle,
			&block_idx, &ram_addr, &etcam_wr_mode);

		zxdh_np_dtb_etcam_entry_add(dev_id,
				ram_addr,
				block_idx,
				etcam_wr_mode,
				ZXDH_ETCAM_OPR_DM,
				&etcam_entry,
				&entry_data,
				&entry_mask);

		dtb_len += ZXDH_DTB_ETCAM_LEN_SIZE;
		zxdh_np_dtb_data_write(table_data_buff, addr_offset_bk, &entry_data);

		memset(entry_data_cmd_buff, 0, ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8);
		memset(entry_data_buff, 0, ZXDH_ETCAM_WIDTH_MAX / 8);
		addr_offset_bk = addr_offset_bk + ZXDH_DTB_ETCAM_LEN_SIZE * ZXDH_DTB_LEN_POS_SETP;

		dtb_len += ZXDH_DTB_ETCAM_LEN_SIZE;
		zxdh_np_dtb_data_write(table_data_buff, addr_offset_bk, &entry_mask);

		memset(entry_mask_cmd_buff, 0, ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8);
		memset(entry_mask_buff, 0, ZXDH_ETCAM_WIDTH_MAX / 8);
		addr_offset_bk = addr_offset_bk + ZXDH_DTB_ETCAM_LEN_SIZE * ZXDH_DTB_LEN_POS_SETP;

		if (as_enable) {
			uint32_t as_eram_data_buff[4] = {0};
			uint8_t as_eram_cmd_buff[ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8] = {0};
			ZXDH_DTB_ENTRY_T dtb_as_data_entry = {0};
			uint32_t *p_as_eram_data = (uint32_t *)(p_acl_entry->p_as_rslt);
			uint32_t eram_index = p_acl_entry->handle;

			dtb_as_data_entry.cmd = as_eram_cmd_buff;
			dtb_as_data_entry.data = (uint8_t *)as_eram_data_buff;

			zxdh_np_dtb_se_smmu0_ind_write(dev_id,
					as_eram_baddr,
					eram_index,
					eram_wrt_mode,
					p_as_eram_data,
					&dtb_as_data_entry);

			switch (eram_wrt_mode) {
			case ZXDH_ERAM128_OPR_128b:
				as_dtb_len = 2;
				as_addr_offset = ZXDH_DTB_LEN_POS_SETP * 2;
				break;
			case ZXDH_ERAM128_OPR_64b:
				as_dtb_len = 1;
				as_addr_offset = ZXDH_DTB_LEN_POS_SETP;
				break;
			case ZXDH_ERAM128_OPR_1b:
				as_dtb_len = 1;
				as_addr_offset = ZXDH_DTB_LEN_POS_SETP;
				break;
			}

			zxdh_np_dtb_data_write(table_data_buff,
				addr_offset_bk, &dtb_as_data_entry);
			addr_offset_bk = addr_offset_bk + as_addr_offset;
			dtb_len += as_dtb_len;

			memset(as_eram_cmd_buff, 0, ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8);
			memset(as_eram_data_buff, 0, 4 * sizeof(uint32_t));
		}
	}

	rc = zxdh_np_dtb_write_down_table_data(dev_id,
				queue_id,
				dtb_len * 16,
				table_data_buff,
				element_id);
	free(table_data_buff);

	rc = zxdh_np_dtb_tab_down_success_status_check(dev_id, queue_id, *element_id);

	return rc;
}

static uint32_t
zxdh_np_dtb_acl_dma_insert(uint32_t dev_id,
		uint32_t queue_id,
		uint32_t sdt_no,
		uint32_t entry_num,
		ZXDH_DTB_ACL_ENTRY_INFO_T *p_acl_entry_arr,
		uint32_t *element_id)
{
	uint32_t entry_num_max = 0;
	ZXDH_SDT_TBL_ETCAM_T sdt_etcam_info = {0};

	uint32_t rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_etcam_info);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_data_write");

	uint32_t as_enable = sdt_etcam_info.as_en;
	uint32_t etcam_as_mode = sdt_etcam_info.as_rsp_mode;

	if (!as_enable) {
		entry_num_max = 0x55;
	} else {
		if (etcam_as_mode == ZXDH_ERAM128_TBL_128b)
			entry_num_max = 0x49;
		else
			entry_num_max = 0x4e;
	}

	uint32_t entry_cycle = entry_num / entry_num_max;
	uint32_t entry_remains = entry_num % entry_num_max;

	for (uint32_t i = 0; i < entry_cycle; ++i) {
		ZXDH_DTB_ACL_ENTRY_INFO_T *p_entry = p_acl_entry_arr + entry_num_max * i;
		rc = zxdh_np_dtb_acl_dma_insert_cycle(dev_id,
					queue_id,
					sdt_no,
					entry_num_max,
					p_entry,
					element_id);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_acl_dma_insert_cycle");
	}

	if (entry_remains) {
		ZXDH_DTB_ACL_ENTRY_INFO_T *p_entry  = p_acl_entry_arr + entry_num_max * entry_cycle;
		rc = zxdh_np_dtb_acl_dma_insert_cycle(dev_id,
					queue_id,
					sdt_no,
					entry_remains,
					p_entry,
					element_id);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_acl_dma_insert_cycle");
	}

	return rc;
}

static uint32_t
zxdh_np_dtb_acl_data_clear(uint32_t dev_id, uint32_t queue_id,
	uint32_t sdt_no, uint32_t index_num, uint32_t *p_index_array)
{
	uint32_t element_id = 0;
	uint32_t *eram_buff = NULL;
	ZXDH_SDT_TBL_ETCAM_T sdt_etcam_info = {0};

	uint32_t rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_etcam_info);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");

	uint32_t etcam_key_mode = sdt_etcam_info.etcam_key_mode;
	uint32_t as_enable = sdt_etcam_info.as_en;
	uint32_t data_byte_size = ZXDH_ETCAM_ENTRY_SIZE_GET(etcam_key_mode);

	ZXDH_DTB_ACL_ENTRY_INFO_T *p_entry_arr = malloc(index_num *
		sizeof(ZXDH_DTB_ACL_ENTRY_INFO_T));
	if (p_entry_arr == NULL) {
		PMD_DRV_LOG(ERR, "%s point null!", __func__);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	uint8_t *data_buff = malloc(data_byte_size);
	if (data_buff == NULL) {
		PMD_DRV_LOG(ERR, "data_buff point null!");
		free(p_entry_arr);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	uint8_t *mask_buff = malloc(data_byte_size);
	if (mask_buff == NULL) {
		PMD_DRV_LOG(ERR, "mask_buff point null!");
		free(data_buff);
		free(p_entry_arr);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	if (as_enable) {
		eram_buff = malloc(4 * sizeof(uint32_t));
		if (eram_buff == NULL) {
			PMD_DRV_LOG(ERR, "eram_buff point null!");
			free(mask_buff);
			free(data_buff);
			free(p_entry_arr);
			return ZXDH_PAR_CHK_POINT_NULL;
		}
		memset(eram_buff, 0, 4 * sizeof(uint32_t));
	}

	for (uint32_t index = 0; index < index_num; index++) {
		p_entry_arr[index].handle = p_index_array[index];
		p_entry_arr[index].key_data = data_buff;
		p_entry_arr[index].key_mask = mask_buff;

		if (as_enable)
			p_entry_arr[index].p_as_rslt = (uint8_t *)eram_buff;
	}

	rc = zxdh_np_dtb_acl_dma_insert(dev_id,
				queue_id,
				sdt_no,
				index_num,
				p_entry_arr,
				&element_id);
	free(data_buff);
	free(mask_buff);
	if (eram_buff)
		free(eram_buff);

	free(p_entry_arr);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_acl_dma_insert");

	return rc;
}

static uint32_t
zxdh_np_dtb_acl_index_release_by_vport(uint32_t dev_id,
				uint32_t sdt_no, uint32_t vport)
{
	ZXDH_SDT_TBL_ETCAM_T sdt_acl = {0};
	ZXDH_SDT_TBL_ERAM_T sdt_eram = {0};

	uint32_t rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_acl);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");
	if (sdt_acl.table_type != ZXDH_SDT_TBLT_ETCAM) {
		PMD_DRV_LOG(ERR, "SDT[%u] table_type[ %u ] is not etcam table!",
			sdt_no, sdt_acl.table_type);
		return ZXDH_ERR;
	}

	uint32_t eram_sdt_no = zxdh_np_apt_get_sdt_partner(dev_id, sdt_no);

	rc = zxdh_np_soft_sdt_tbl_get(dev_id, eram_sdt_no, &sdt_eram);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");
	if (sdt_eram.table_type != ZXDH_SDT_TBLT_ERAM) {
		PMD_DRV_LOG(ERR, "SDT[%u] table_type[ %u ] is not eram table!",
			eram_sdt_no, sdt_eram.table_type);
		return ZXDH_ERR;
	}

	ZXDH_SPINLOCK_T *p_dtb_spinlock = NULL;
	ZXDH_DEV_SPINLOCK_TYPE_E spinlock = ZXDH_DEV_SPINLOCK_T_DTB;

	rc = zxdh_np_dev_opr_spinlock_get(dev_id, (uint32_t)spinlock, &p_dtb_spinlock);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dev_opr_spinlock_get");

	rte_spinlock_lock(&p_dtb_spinlock->spinlock);

	rc = zxdh_np_agent_channel_acl_index_release(dev_id,
		ZXDH_ACL_INDEX_VPORT_REL, sdt_no, vport, 0);
	if (rc == ZXDH_ACL_RC_SRH_FAIL)
		PMD_DRV_LOG(ERR, "ACL_INDEX_VPORT_REL[vport:0x%x] index is not exist.", vport);

	rte_spinlock_unlock(&p_dtb_spinlock->spinlock);

	return rc;
}

static uint32_t
zxdh_np_dtb_smmu0_data_write_cycle(uint32_t dev_id,
			uint32_t queue_id,
			uint32_t smmu0_base_addr,
			uint32_t smmu0_wr_mode,
			uint32_t entry_num,
			ZXDH_DTB_ERAM_ENTRY_INFO_T *p_entry_arr,
			uint32_t *element_id)
{
	uint32_t entry_data_buff[4] = {0};
	uint8_t cmd_buff[ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8] = {0};
	ZXDH_DTB_ENTRY_T dtb_one_entry = {0};

	uint8_t *table_data_buff = malloc(ZXDH_DTB_TABLE_DATA_BUFF_SIZE);
	if (table_data_buff == NULL) {
		PMD_DRV_LOG(ERR, "%s point null!", __func__);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	dtb_one_entry.cmd = cmd_buff;
	dtb_one_entry.data = (uint8_t *)entry_data_buff;

	uint32_t rc = ZXDH_OK;
	uint32_t addr_offset = 0;
	uint32_t dtb_len = 0;

	for (uint32_t item_cnt = 0; item_cnt < entry_num; ++item_cnt) {
		uint32_t *p_entry_data = (uint32_t *)p_entry_arr[item_cnt].p_data;
		uint32_t index = p_entry_arr[item_cnt].index;

		rc = zxdh_np_dtb_se_smmu0_ind_write(dev_id,
					smmu0_base_addr,
					index,
					smmu0_wr_mode,
					p_entry_data,
					&dtb_one_entry);

		switch (smmu0_wr_mode) {
		case ZXDH_ERAM128_OPR_128b:
			dtb_len += 2;
			addr_offset = item_cnt * ZXDH_DTB_LEN_POS_SETP * 2;
			break;
		case ZXDH_ERAM128_OPR_64b:
			dtb_len += 1;
			addr_offset = item_cnt * ZXDH_DTB_LEN_POS_SETP;
			break;
		case ZXDH_ERAM128_OPR_1b:
			dtb_len += 1;
			addr_offset = item_cnt * ZXDH_DTB_LEN_POS_SETP;
			break;
		}

		zxdh_np_dtb_data_write(table_data_buff, addr_offset, &dtb_one_entry);
		memset(cmd_buff, 0, ZXDH_DTB_TABLE_CMD_SIZE_BIT / 8);
		memset(entry_data_buff,	0, 4 * sizeof(uint32_t));
	}

	rc = zxdh_np_dtb_write_down_table_data(dev_id,
					queue_id,
					dtb_len * 16,
					table_data_buff,
					element_id);
	free(table_data_buff);

	rc = zxdh_np_dtb_tab_down_success_status_check(dev_id, queue_id, *element_id);

	return rc;
}

static uint32_t
zxdh_np_dtb_smmu0_data_write(uint32_t dev_id,
			uint32_t queue_id,
			uint32_t smmu0_base_addr,
			uint32_t smmu0_wr_mode,
			uint32_t entry_num,
			ZXDH_DTB_ERAM_ENTRY_INFO_T *p_entry_arr,
			uint32_t *element_id)
{
	uint32_t entry_num_max = 0;

	switch (smmu0_wr_mode) {
	case ZXDH_ERAM128_OPR_128b:
		entry_num_max = 0x1ff;
		break;
	case ZXDH_ERAM128_OPR_64b:
		entry_num_max = 0x3ff;
		break;
	case ZXDH_ERAM128_OPR_1b:
		entry_num_max = 0x3ff;
		break;
	}

	uint32_t entry_cycle = entry_num / entry_num_max;
	uint32_t entry_remains = entry_num % entry_num_max;
	uint32_t rc = ZXDH_OK;

	for (uint32_t i = 0; i < entry_cycle; ++i) {
		ZXDH_DTB_ERAM_ENTRY_INFO_T *p_entry = p_entry_arr + entry_num_max * i;
		rc = zxdh_np_dtb_smmu0_data_write_cycle(dev_id,
							queue_id,
							smmu0_base_addr,
							smmu0_wr_mode,
							entry_num_max,
							p_entry,
							element_id);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_smmu0_data_write_cycle");
	}

	if (entry_remains) {
		ZXDH_DTB_ERAM_ENTRY_INFO_T *p_entry = p_entry_arr + entry_num_max * entry_cycle;
		rc = zxdh_np_dtb_smmu0_data_write_cycle(dev_id,
							queue_id,
							smmu0_base_addr,
							smmu0_wr_mode,
							entry_remains,
							p_entry,
							element_id);
		ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_smmu0_data_write_cycle");
	}

	return rc;
}

static uint32_t
zxdh_np_dtb_eram_dma_write(uint32_t dev_id,
			uint32_t queue_id,
			uint32_t sdt_no,
			uint32_t entry_num,
			ZXDH_DTB_ERAM_ENTRY_INFO_T *p_entry_arr,
			uint32_t *element_id)
{
	ZXDH_SDT_TBL_ERAM_T sdt_eram_info = {0};
	uint32_t rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_eram_info);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");

	uint32_t base_addr = sdt_eram_info.eram_base_addr;
	uint32_t wrt_mode = sdt_eram_info.eram_mode;

	switch (wrt_mode) {
	case ZXDH_ERAM128_TBL_128b:
		wrt_mode = ZXDH_ERAM128_OPR_128b;
		break;
	case ZXDH_ERAM128_TBL_64b:
		wrt_mode = ZXDH_ERAM128_OPR_64b;
		break;
	case ZXDH_ERAM128_TBL_1b:
		wrt_mode = ZXDH_ERAM128_OPR_1b;
		break;
	}

	rc = zxdh_np_dtb_smmu0_data_write(dev_id,
					queue_id,
					base_addr,
					wrt_mode,
					entry_num,
					p_entry_arr,
					element_id);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_smmu0_data_write");

	return ZXDH_OK;
}

static uint32_t
zxdh_np_dtb_eram_data_clear(uint32_t dev_id,
			uint32_t queue_id,
			uint32_t sdt_no,
			uint32_t index_num,
			uint32_t *p_index_array)
{
	ZXDH_DTB_ERAM_ENTRY_INFO_T *p_eram_data_arr = malloc(index_num *
		sizeof(ZXDH_DTB_ERAM_ENTRY_INFO_T));
	if (p_eram_data_arr == NULL) {
		PMD_DRV_LOG(ERR, "%s point null!", __func__);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	uint8_t *data_buff = malloc(4 * sizeof(uint32_t));
	if (data_buff == NULL) {
		PMD_DRV_LOG(ERR, "data_buff point null!");
		free(p_eram_data_arr);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	for (uint32_t i = 0; i < index_num; i++) {
		p_eram_data_arr[i].index = p_index_array[i];
		p_eram_data_arr[i].p_data = (uint32_t *)data_buff;
	}

	uint32_t element_id = 0;
	uint32_t rc = zxdh_np_dtb_eram_dma_write(dev_id, queue_id,
		sdt_no, index_num, p_eram_data_arr, &element_id);
	free(data_buff);
	free(p_eram_data_arr);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_eram_dma_write");

	return rc;
}

static uint32_t
zxdh_np_dtb_eram_stat_data_clear(uint32_t dev_id,
		uint32_t queue_id,
		uint32_t counter_id,
		ZXDH_STAT_CNT_MODE_E rd_mode,
		uint32_t index_num,
		uint32_t *p_index_array)
{
	ZXDH_PPU_STAT_CFG_T stat_cfg = {0};
	zxdh_np_stat_cfg_soft_get(dev_id, &stat_cfg);

	ZXDH_DTB_ERAM_ENTRY_INFO_T *p_eram_data_arr = malloc(index_num *
		sizeof(ZXDH_DTB_ERAM_ENTRY_INFO_T));
	if (p_eram_data_arr == NULL) {
		PMD_DRV_LOG(ERR, "%s point null!", __func__);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	uint8_t *data_buff = malloc(4 * sizeof(uint32_t));
	if (data_buff == NULL) {
		PMD_DRV_LOG(ERR, "data_buff point null!");
		free(p_eram_data_arr);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	for (uint32_t i = 0; i < index_num; i++) {
		p_eram_data_arr[i].index = p_index_array[i];
		p_eram_data_arr[i].p_data = (uint32_t *)data_buff;
	}

	uint32_t wrt_mode = (rd_mode == ZXDH_STAT_128_MODE) ?
						ZXDH_ERAM128_OPR_128b : ZXDH_ERAM128_OPR_64b;
	uint32_t counter_id_128bit = (rd_mode == ZXDH_STAT_128_MODE) ?
								  counter_id : (counter_id >> 1);
	uint32_t start_addr = stat_cfg.eram_baddr + counter_id_128bit;
	uint32_t element_id = 0;
	uint32_t rc = zxdh_np_dtb_smmu0_data_write(dev_id,
					queue_id,
					start_addr,
					wrt_mode,
					index_num,
					p_eram_data_arr,
					&element_id);
	free(data_buff);
	free(p_eram_data_arr);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_dtb_smmu0_data_write");

	return rc;
}

uint32_t
zxdh_np_dtb_acl_offline_delete(uint32_t dev_id, uint32_t queue_id,
	uint32_t sdt_no, uint32_t vport, uint32_t counter_id, uint32_t rd_mode)
{
	ZXDH_SDT_TBL_ETCAM_T sdt_acl = {0};
	ZXDH_SDT_TBL_ERAM_T sdt_eram = {0};
	uint32_t index_num = 0;

	uint32_t rc = zxdh_np_soft_sdt_tbl_get(dev_id, sdt_no, &sdt_acl);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");
	if (sdt_acl.table_type != ZXDH_SDT_TBLT_ETCAM) {
		PMD_DRV_LOG(ERR, "SDT[%u] table_type[ %u ] is not etcam table!",
			sdt_no, sdt_acl.table_type);
		return ZXDH_ERR;
	}

	uint32_t eram_sdt_no = zxdh_np_apt_get_sdt_partner(dev_id, sdt_no);

	rc = zxdh_np_soft_sdt_tbl_get(dev_id, eram_sdt_no, &sdt_eram);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_soft_sdt_tbl_get");
	if (sdt_eram.table_type != ZXDH_SDT_TBLT_ERAM) {
		PMD_DRV_LOG(ERR, "SDT[%u] table_type[ %u ] is not eram table!",
			eram_sdt_no, sdt_eram.table_type);
		return ZXDH_ERR;
	}

	uint32_t *p_index_array = malloc(sizeof(uint32_t) * sdt_eram.eram_table_depth);
	if (p_index_array == NULL) {
		PMD_DRV_LOG(ERR, "%s point null!", __func__);
		return ZXDH_PAR_CHK_POINT_NULL;
	}

	rc = zxdh_np_dtb_acl_index_parse(dev_id, queue_id,
			eram_sdt_no, vport, &index_num, p_index_array);
	if (rc != ZXDH_OK) {
		free(p_index_array);
		PMD_DRV_LOG(ERR, "acl index parse failed");
		return ZXDH_ERR;
	}

	if (!index_num) {
		PMD_DRV_LOG(ERR, "SDT[%u] vport[0x%x] item num is zero!", sdt_no, vport);
		free(p_index_array);
		return ZXDH_OK;
	}

	rc = zxdh_np_dtb_acl_data_clear(dev_id, queue_id, sdt_no, index_num, p_index_array);
	rc = zxdh_np_dtb_eram_data_clear(dev_id, queue_id, eram_sdt_no, index_num, p_index_array);
	rc = zxdh_np_dtb_eram_stat_data_clear(dev_id, queue_id,
			counter_id, rd_mode, index_num, p_index_array);
	free(p_index_array);

	rc = zxdh_np_dtb_acl_index_release_by_vport(dev_id, sdt_no, vport);

	return rc;
}
