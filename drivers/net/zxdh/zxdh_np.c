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

static uint64_t g_np_bar_offset;
static ZXDH_DEV_MGR_T g_dev_mgr;
static ZXDH_SDT_MGR_T g_sdt_mgr;
static uint32_t g_dpp_dtb_int_enable;
static uint32_t g_table_type[ZXDH_DEV_CHANNEL_MAX][ZXDH_DEV_SDT_ID_MAX];
static ZXDH_PPU_CLS_BITMAP_T g_ppu_cls_bit_map[ZXDH_DEV_CHANNEL_MAX];
static ZXDH_DTB_MGR_T *p_dpp_dtb_mgr[ZXDH_DEV_CHANNEL_MAX];
static ZXDH_RISCV_DTB_MGR *p_riscv_dtb_queue_mgr[ZXDH_DEV_CHANNEL_MAX];
static ZXDH_TLB_MGR_T *g_p_dpp_tlb_mgr[ZXDH_DEV_CHANNEL_MAX];
static ZXDH_REG_T g_dpp_reg_info[4];
static ZXDH_DTB_TABLE_T g_dpp_dtb_table_info[4];
static ZXDH_SDT_TBL_DATA_T g_sdt_info[ZXDH_DEV_CHANNEL_MAX][ZXDH_DEV_SDT_ID_MAX];
static ZXDH_PPU_STAT_CFG_T g_ppu_stat_cfg;

#define ZXDH_SDT_MGR_PTR_GET()    (&g_sdt_mgr)
#define ZXDH_SDT_SOFT_TBL_GET(id) (g_sdt_mgr.sdt_tbl_array[id])

#define ZXDH_COMM_MASK_BIT(_bitnum_)\
	(0x1U << (_bitnum_))

#define ZXDH_COMM_GET_BIT_MASK(_inttype_, _bitqnt_)\
	((_inttype_)(((_bitqnt_) < 32)))

#define ZXDH_COMM_UINT32_GET_BITS(_uidst_, _uisrc_, _uistartpos_, _uilen_)\
	((_uidst_) = (((_uisrc_) >> (_uistartpos_)) & \
	(ZXDH_COMM_GET_BIT_MASK(uint32_t, (_uilen_)))))

#define ZXDH_REG_DATA_MAX      (128)

#define ZXDH_COMM_CHECK_DEV_POINT(dev_id, point)\
do {\
	if (NULL == (point)) {\
		PMD_DRV_LOG(ERR, "dev: %d ZXIC %s:%d[Error:POINT NULL] !"\
			"FUNCTION : %s!", (dev_id), __FILE__, __LINE__, __func__);\
		RTE_ASSERT(0);\
	} \
} while (0)

#define ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, becall)\
do {\
	if ((rc) != 0) {\
		PMD_DRV_LOG(ERR, "dev: %d ZXIC  %s:%d !"\
		"-- %s Call %s Fail!", (dev_id), __FILE__, __LINE__, __func__, becall);\
		RTE_ASSERT(0);\
	} \
} while (0)

#define ZXDH_COMM_CHECK_POINT_NO_ASSERT(point)\
do {\
	if ((point) == NULL) {\
		PMD_DRV_LOG(ERR, "ZXIC %s:%d[Error:POINT NULL] ! FUNCTION : %s!",\
		__FILE__, __LINE__, __func__);\
	} \
} while (0)

#define ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, becall)\
do {\
	if ((rc) != 0) {\
		PMD_DRV_LOG(ERR, "ZXIC  %s:%d !-- %s Call %s"\
		" Fail!", __FILE__, __LINE__, __func__, becall);\
	} \
} while (0)

#define ZXDH_COMM_CHECK_RC(rc, becall)\
do {\
	if ((rc) != 0) {\
		PMD_DRV_LOG(ERR, "ZXIC  %s:%d!-- %s Call %s "\
		"Fail!", __FILE__, __LINE__, __func__, becall);\
		RTE_ASSERT(0);\
	} \
} while (0)

#define ZXDH_COMM_CHECK_POINT(point)\
do {\
	if ((point) == NULL) {\
		PMD_DRV_LOG(ERR, "ZXIC %s:%d[Error:POINT NULL] ! FUNCTION : %s!",\
		__FILE__, __LINE__, __func__);\
		RTE_ASSERT(0);\
	} \
} while (0)


#define ZXDH_COMM_CHECK_POINT_MEMORY_FREE(point, ptr)\
do {\
	if ((point) == NULL) {\
		PMD_DRV_LOG(ERR, "ZXIC %s:%d[Error:POINT NULL] !"\
		"FUNCTION : %s!", __FILE__, __LINE__, __func__);\
		rte_free(ptr);\
		RTE_ASSERT(0);\
	} \
} while (0)

#define ZXDH_COMM_CHECK_RC_MEMORY_FREE_NO_ASSERT(rc, becall, ptr)\
do {\
	if ((rc) != 0) {\
		PMD_DRV_LOG(ERR, "ZXICP  %s:%d, %s Call"\
		" %s Fail!", __FILE__, __LINE__, __func__, becall);\
		rte_free(ptr);\
	} \
} while (0)

#define ZXDH_COMM_CONVERT16(w_data) \
			(((w_data) & 0xff) << 8)

#define ZXDH_DTB_TAB_UP_WR_INDEX_GET(DEV_ID, QUEUE_ID)       \
		(p_dpp_dtb_mgr[(DEV_ID)]->queue_info[(QUEUE_ID)].tab_up.wr_index)

#define ZXDH_DTB_TAB_UP_USER_PHY_ADDR_FLAG_GET(DEV_ID, QUEUE_ID, INDEX)     \
	(p_dpp_dtb_mgr[(DEV_ID)]->queue_info[(QUEUE_ID)].tab_up.user_addr[(INDEX)].user_flag)

#define ZXDH_DTB_TAB_UP_USER_PHY_ADDR_GET(DEV_ID, QUEUE_ID, INDEX)     \
		(p_dpp_dtb_mgr[(DEV_ID)]->queue_info[(QUEUE_ID)].tab_up.user_addr[(INDEX)].phy_addr)

#define ZXDH_DTB_TAB_UP_DATA_LEN_GET(DEV_ID, QUEUE_ID, INDEX)       \
		(p_dpp_dtb_mgr[(DEV_ID)]->queue_info[(QUEUE_ID)].tab_up.data_len[(INDEX)])

#define ZXDH_DTB_TAB_UP_VIR_ADDR_GET(DEV_ID, QUEUE_ID, INDEX)     \
		((INDEX) * p_dpp_dtb_mgr[(DEV_ID)]->queue_info[(QUEUE_ID)].tab_up.item_size)

#define ZXDH_DTB_TAB_DOWN_VIR_ADDR_GET(DEV_ID, QUEUE_ID, INDEX)   \
		((INDEX) * p_dpp_dtb_mgr[(DEV_ID)]->queue_info[(QUEUE_ID)].tab_down.item_size)

#define ZXDH_DTB_TAB_DOWN_WR_INDEX_GET(DEV_ID, QUEUE_ID)       \
		(p_dpp_dtb_mgr[(DEV_ID)]->queue_info[(QUEUE_ID)].tab_down.wr_index)

#define ZXDH_DTB_QUEUE_INIT_FLAG_GET(DEV_ID, QUEUE_ID)       \
		(p_dpp_dtb_mgr[(DEV_ID)]->queue_info[(QUEUE_ID)].init_flag)

ZXDH_FIELD_T g_stat_car0_cara_queue_ram0_159_0_reg[] = {
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

ZXDH_FIELD_T g_stat_car0_carb_queue_ram0_159_0_reg[] = {
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

ZXDH_FIELD_T g_stat_car0_carc_queue_ram0_159_0_reg[] = {
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

ZXDH_FIELD_T g_nppu_pktrx_cfg_pktrx_glbal_cfg_0_reg[] = {
	{"pktrx_glbal_cfg_0", ZXDH_FIELD_FLAG_RW, 31, 32, 0x0, 0x0},
};

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
		(*p_dw_tmp) = ZXDH_COMM_CONVERT16(*p_dw_tmp);
		p_dw_tmp++;
	}

	if (uc_byte_mode > 1) {
		p_w_tmp = (uint16_t *)(p_dw_tmp);
		(*p_w_tmp) = ZXDH_COMM_CONVERT16(*p_w_tmp);
	}
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

static uint32_t
zxdh_np_dev_add(uint32_t  dev_id, ZXDH_DEV_TYPE_E dev_type,
		ZXDH_DEV_ACCESS_TYPE_E  access_type, uint64_t  pcie_addr,
		uint64_t  riscv_addr, uint64_t  dma_vir_addr,
		uint64_t  dma_phy_addr)
{
	ZXDH_DEV_CFG_T *p_dev_info = NULL;
	ZXDH_DEV_MGR_T *p_dev_mgr  = NULL;

	p_dev_mgr = &g_dev_mgr;
	if (!p_dev_mgr->is_init) {
		PMD_DRV_LOG(ERR, "ErrorCode[ 0x%x]: Device Manager is not init",
								 ZXDH_RC_DEV_MGR_NOT_INIT);
		return ZXDH_RC_DEV_MGR_NOT_INIT;
	}

	if (p_dev_mgr->p_dev_array[dev_id] != NULL) {
		/* device is already exist. */
		PMD_DRV_LOG(ERR, "Device is added again");
		p_dev_info = p_dev_mgr->p_dev_array[dev_id];
	} else {
		/* device is new. */
		p_dev_info = rte_malloc(NULL, sizeof(ZXDH_DEV_CFG_T), 0);
		ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_dev_info);
		p_dev_mgr->p_dev_array[dev_id] = p_dev_info;
		p_dev_mgr->device_num++;
	}

	p_dev_info->device_id   = dev_id;
	p_dev_info->dev_type    = dev_type;
	p_dev_info->access_type = access_type;
	p_dev_info->pcie_addr   = pcie_addr;
	p_dev_info->riscv_addr   = riscv_addr;
	p_dev_info->dma_vir_addr = dma_vir_addr;
	p_dev_info->dma_phy_addr = dma_phy_addr;

	return 0;
}

static uint32_t
zxdh_np_dev_agent_status_set(uint32_t dev_id, uint32_t agent_flag)
{
	ZXDH_DEV_MGR_T *p_dev_mgr = NULL;
	ZXDH_DEV_CFG_T *p_dev_info = NULL;

	p_dev_mgr = &g_dev_mgr;
	p_dev_info = p_dev_mgr->p_dev_array[dev_id];

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
		PMD_DRV_LOG(ERR, "Error: %s for dev[%d]"
			"is called repeatedly!", __func__, dev_id);
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
		g_ppu_cls_bit_map[dev_id].cls_use[cls_id] = cls_use;
	}

	for (mem_id = 0; mem_id < ZXDH_PPU_INSTR_MEM_NUM; mem_id++) {
		instr_mem = (bitmap >> (mem_id * 2)) & 0x3;
		g_ppu_cls_bit_map[dev_id].instr_mem[mem_id] = ((instr_mem > 0) ? 1 : 0);
	}
}

static ZXDH_DTB_MGR_T *
zxdh_np_dtb_mgr_get(uint32_t dev_id)
{
	if (dev_id >= ZXDH_DEV_CHANNEL_MAX)
		return NULL;
	else
		return p_dpp_dtb_mgr[dev_id];
}

static uint32_t
zxdh_np_dtb_soft_init(uint32_t dev_id)
{
	ZXDH_DTB_MGR_T *p_dtb_mgr = NULL;

	if (dev_id >= ZXDH_DEV_CHANNEL_MAX)
		return 1;

	p_dtb_mgr = zxdh_np_dtb_mgr_get(dev_id);
	if (p_dtb_mgr == NULL) {
		p_dpp_dtb_mgr[dev_id] = rte_zmalloc(NULL, sizeof(ZXDH_DTB_MGR_T), 0);
		p_dtb_mgr = zxdh_np_dtb_mgr_get(dev_id);
		if (p_dtb_mgr == NULL)
			return 1;
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
	p_dev_info = p_dev_mgr->p_dev_array[dev_id];
	p_dev_info->vport = vport;
}

static void
zxdh_np_dev_agent_addr_set(uint32_t dev_id, uint64_t agent_addr)
{
	ZXDH_DEV_MGR_T *p_dev_mgr = NULL;
	ZXDH_DEV_CFG_T *p_dev_info = NULL;

	p_dev_mgr =  &g_dev_mgr;
	p_dev_info = p_dev_mgr->p_dev_array[dev_id];
	p_dev_info->agent_addr = agent_addr;
}

static uint64_t
zxdh_np_addr_calc(uint64_t pcie_vir_baddr, uint32_t bar_offset)
{
	uint64_t np_addr;

	np_addr = ((pcie_vir_baddr + bar_offset) > ZXDH_PCIE_NP_MEM_SIZE)
				? (pcie_vir_baddr + bar_offset - ZXDH_PCIE_NP_MEM_SIZE) : 0;
	g_np_bar_offset = bar_offset;

	return np_addr;
}

int
zxdh_np_host_init(uint32_t dev_id,
		ZXDH_DEV_INIT_CTRL_T *p_dev_init_ctrl)
{
	ZXDH_SYS_INIT_CTRL_T sys_init_ctrl = {0};
	uint32_t rc;
	uint64_t agent_addr;

	ZXDH_COMM_CHECK_POINT_NO_ASSERT(p_dev_init_ctrl);

	sys_init_ctrl.flags = (ZXDH_DEV_ACCESS_TYPE_PCIE << 0) | (ZXDH_DEV_AGENT_ENABLE << 10);
	sys_init_ctrl.pcie_vir_baddr = zxdh_np_addr_calc(p_dev_init_ctrl->pcie_vir_addr,
		p_dev_init_ctrl->np_bar_offset);
	sys_init_ctrl.device_type = ZXDH_DEV_TYPE_CHIP;
	rc = zxdh_np_base_soft_init(dev_id, &sys_init_ctrl);
	ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "zxdh_base_soft_init");

	zxdh_np_dev_vport_set(dev_id, p_dev_init_ctrl->vport);

	agent_addr = ZXDH_PCIE_AGENT_ADDR_OFFSET + p_dev_init_ctrl->pcie_vir_addr;
	zxdh_np_dev_agent_addr_set(dev_id, agent_addr);

	return 0;
}

static ZXDH_RISCV_DTB_MGR *
zxdh_np_riscv_dtb_queue_mgr_get(uint32_t dev_id)
{
	if (dev_id >= ZXDH_DEV_CHANNEL_MAX)
		return NULL;
	else
		return p_riscv_dtb_queue_mgr[dev_id];
}

static uint32_t
zxdh_np_riscv_dtb_mgr_queue_info_delete(uint32_t dev_id, uint32_t queue_id)
{
	ZXDH_RISCV_DTB_MGR *p_riscv_dtb_mgr = NULL;

	p_riscv_dtb_mgr = zxdh_np_riscv_dtb_queue_mgr_get(dev_id);
	if (p_riscv_dtb_mgr == NULL)
		return 1;

	p_riscv_dtb_mgr->queue_alloc_count--;
	p_riscv_dtb_mgr->queue_user_info[queue_id].alloc_flag = 0;
	p_riscv_dtb_mgr->queue_user_info[queue_id].queue_id = 0xFF;
	p_riscv_dtb_mgr->queue_user_info[queue_id].vport = 0;
	memset(p_riscv_dtb_mgr->queue_user_info[queue_id].user_name, 0, ZXDH_PORT_NAME_MAX);

	return 0;
}

static uint32_t
zxdh_np_dev_get_dev_type(uint32_t dev_id)
{
	ZXDH_DEV_MGR_T *p_dev_mgr = NULL;
	ZXDH_DEV_CFG_T *p_dev_info = NULL;

	p_dev_mgr = &g_dev_mgr;
	p_dev_info = p_dev_mgr->p_dev_array[dev_id];

	if (p_dev_info == NULL)
		return 0xffff;

	return p_dev_info->dev_type;
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
	ZXDH_REG_T *p_reg_info = NULL;
	ZXDH_FIELD_T *p_field_info = NULL;
	uint32_t rc = 0;
	uint32_t i;

	if (reg_no < 4) {
		p_reg_info = &g_dpp_reg_info[reg_no];
		p_field_info = p_reg_info->p_fields;
		for (i = 0; i < p_reg_info->field_num; i++) {
			rc = zxdh_np_comm_read_bits_ex((uint8_t *)p_buff,
									p_reg_info->width * 8,
									(uint32_t *)p_data + i,
									p_field_info[i].msb_pos,
									p_field_info[i].len);
			ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "zxic_comm_read_bits_ex");
			PMD_DRV_LOG(ERR, "dev_id %d(%d)(%d)is ok!", dev_id, m_offset, n_offset);
		}
	}
	return rc;
}

static uint32_t
zxdh_np_dtb_queue_vm_info_get(uint32_t dev_id,
		uint32_t queue_id,
		ZXDH_DTB_QUEUE_VM_INFO_T *p_vm_info)
{
	ZXDH_DTB4K_DTB_ENQ_CFG_EPID_V_FUNC_NUM_0_127_T vm_info = {0};
	uint32_t rc;

	rc = zxdh_np_reg_read(dev_id, ZXDH_DTB_CFG_EPID_V_FUNC_NUM,
						0, queue_id, &vm_info);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_reg_read");

	p_vm_info->dbi_en = vm_info.dbi_en;
	p_vm_info->queue_en = vm_info.queue_en;
	p_vm_info->epid = vm_info.cfg_epid;
	p_vm_info->vector = vm_info.cfg_vector;
	p_vm_info->vfunc_num = vm_info.cfg_vfunc_num;
	p_vm_info->func_num = vm_info.cfg_func_num;
	p_vm_info->vfunc_active = vm_info.cfg_vfunc_active;

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
	ZXDH_REG_T *p_reg_info = NULL;
	ZXDH_FIELD_T *p_field_info = NULL;
	uint32_t temp_data;
	uint32_t rc;
	uint32_t i;

	if (reg_no < 4) {
		p_reg_info = &g_dpp_reg_info[reg_no];
		p_field_info = p_reg_info->p_fields;

		for (i = 0; i < p_reg_info->field_num; i++) {
			if (p_field_info[i].len <= 32) {
				temp_data = *((uint32_t *)p_data + i);
				rc = zxdh_np_comm_write_bits_ex((uint8_t *)p_buff,
									p_reg_info->width * 8,
									temp_data,
									p_field_info[i].msb_pos,
									p_field_info[i].len);
				ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "zxdh_comm_write_bits_ex");
				PMD_DRV_LOG(ERR, "dev_id %d(%d)(%d)is ok!",
						dev_id, m_offset, n_offset);
			}
		}
	}

	return 0;
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
zxdh_np_dtb_queue_enable_set(uint32_t dev_id,
		uint32_t queue_id,
		uint32_t enable)
{
	ZXDH_DTB_QUEUE_VM_INFO_T vm_info = {0};
	uint32_t rc;

	rc = zxdh_np_dtb_queue_vm_info_get(dev_id, queue_id, &vm_info);
	ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "zxdh_dtb_queue_vm_info_get");

	vm_info.queue_en = enable;
	rc = zxdh_np_dtb_queue_vm_info_set(dev_id, queue_id, &vm_info);
	ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "zxdh_dtb_queue_vm_info_set");

	return rc;
}

static uint32_t
zxdh_np_riscv_dpp_dtb_queue_id_release(uint32_t dev_id,
			char name[ZXDH_PORT_NAME_MAX], uint32_t queue_id)
{
	ZXDH_RISCV_DTB_MGR *p_riscv_dtb_mgr = NULL;

	p_riscv_dtb_mgr = zxdh_np_riscv_dtb_queue_mgr_get(dev_id);
	if (p_riscv_dtb_mgr == NULL)
		return 1;

	if (zxdh_np_dev_get_dev_type(dev_id) == ZXDH_DEV_TYPE_SIM)
		return 0;

	if (p_riscv_dtb_mgr->queue_user_info[queue_id].alloc_flag != 1) {
		PMD_DRV_LOG(ERR, "queue %d not alloc!", queue_id);
		return 2;
	}

	if (strcmp(p_riscv_dtb_mgr->queue_user_info[queue_id].user_name, name) != 0) {
		PMD_DRV_LOG(ERR, "queue %d name %s error!", queue_id, name);
		return 3;
	}
	zxdh_np_dtb_queue_enable_set(dev_id, queue_id, 0);
	zxdh_np_riscv_dtb_mgr_queue_info_delete(dev_id, queue_id);

	return 0;
}

static uint32_t
zxdh_np_dtb_queue_unused_item_num_get(uint32_t dev_id,
						uint32_t queue_id,
						uint32_t *p_item_num)
{
	uint32_t rc;

	if (zxdh_np_dev_get_dev_type(dev_id) == ZXDH_DEV_TYPE_SIM) {
		*p_item_num = 32;
		return 0;
	}

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

	p_dtb_mgr = p_dpp_dtb_mgr[dev_id];
	if (p_dtb_mgr == NULL)
		return 1;

	rc = zxdh_np_dtb_queue_unused_item_num_get(dev_id, queue_id, &item_num);

	p_dtb_mgr->queue_info[queue_id].init_flag = 0;
	p_dtb_mgr->queue_info[queue_id].vport = 0;
	p_dtb_mgr->queue_info[queue_id].vector = 0;

	return rc;
}

static uint32_t
zxdh_np_dtb_queue_release(uint32_t devid,
		char pname[32],
		uint32_t queueid)
{
	uint32_t rc;

	ZXDH_COMM_CHECK_DEV_POINT(devid, pname);

	rc = zxdh_np_riscv_dpp_dtb_queue_id_release(devid, pname, queueid);
	ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "zxdh_riscv_dpp_dtb_queue_id_release");

	rc = zxdh_np_dtb_queue_id_free(devid, queueid);
	ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "zxdh_dtb_queue_id_free");

	return rc;
}

static void
zxdh_np_dtb_mgr_destroy(uint32_t dev_id)
{
	if (p_dpp_dtb_mgr[dev_id] != NULL) {
		free(p_dpp_dtb_mgr[dev_id]);
		p_dpp_dtb_mgr[dev_id] = NULL;
	}
}

static void
zxdh_np_tlb_mgr_destroy(uint32_t dev_id)
{
	if (g_p_dpp_tlb_mgr[dev_id] != NULL) {
		free(g_p_dpp_tlb_mgr[dev_id]);
		g_p_dpp_tlb_mgr[dev_id] = NULL;
	}
}

static void
zxdh_np_sdt_mgr_destroy(uint32_t dev_id)
{
	ZXDH_SDT_SOFT_TABLE_T *p_sdt_tbl_temp = NULL;
	ZXDH_SDT_MGR_T *p_sdt_mgr = NULL;

	p_sdt_tbl_temp = ZXDH_SDT_SOFT_TBL_GET(dev_id);
	p_sdt_mgr = ZXDH_SDT_MGR_PTR_GET();

	free(p_sdt_tbl_temp);

	ZXDH_SDT_SOFT_TBL_GET(dev_id) = NULL;

	p_sdt_mgr->channel_num--;
}

static void
zxdh_np_dev_del(uint32_t dev_id)
{
	ZXDH_DEV_CFG_T *p_dev_info = NULL;
	ZXDH_DEV_MGR_T *p_dev_mgr  = NULL;

	p_dev_mgr = &g_dev_mgr;
	p_dev_info = p_dev_mgr->p_dev_array[dev_id];

	if (p_dev_info != NULL) {
		free(p_dev_info);
		p_dev_mgr->p_dev_array[dev_id] = NULL;
		p_dev_mgr->device_num--;
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
		PMD_DRV_LOG(ERR, "%s:dtb release error,"
			"port name %s queue id %d", __func__, port_name, queue_id);

	zxdh_np_dtb_mgr_destroy(dev_id);
	zxdh_np_tlb_mgr_destroy(dev_id);
	zxdh_np_sdt_mgr_destroy(dev_id);
	zxdh_np_dev_del(dev_id);

	return 0;
}

static uint32_t
zxdh_np_sdt_tbl_type_get(uint32_t dev_id, uint32_t sdt_no)
{
	return g_table_type[dev_id][sdt_no];
}


static ZXDH_DTB_TABLE_T *
zxdh_np_table_info_get(uint32_t table_type)
{
	return &g_dpp_dtb_table_info[table_type];
}

static uint32_t
zxdh_np_dtb_write_table_cmd(uint32_t dev_id,
			ZXDH_DTB_TABLE_INFO_E table_type,
			void *p_cmd_data,
			void *p_cmd_buff)
{
	uint32_t         field_cnt;
	ZXDH_DTB_TABLE_T     *p_table_info = NULL;
	ZXDH_DTB_FIELD_T     *p_field_info = NULL;
	uint32_t         temp_data;
	uint32_t         rc = 0;

	ZXDH_COMM_CHECK_POINT(p_cmd_data);
	ZXDH_COMM_CHECK_POINT(p_cmd_buff);
	p_table_info = zxdh_np_table_info_get(table_type);
	p_field_info = p_table_info->p_fields;
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_table_info);

	for (field_cnt = 0; field_cnt < p_table_info->field_num; field_cnt++) {
		temp_data = *((uint32_t *)p_cmd_data + field_cnt) & ZXDH_COMM_GET_BIT_MASK(uint32_t,
			p_field_info[field_cnt].len);

		rc = zxdh_np_comm_write_bits_ex((uint8_t *)p_cmd_buff,
					ZXDH_DTB_TABLE_CMD_SIZE_BIT,
					temp_data,
					p_field_info[field_cnt].lsb_pos,
					p_field_info[field_cnt].len);

		ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "zxic_comm_write_bits");
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
zxdh_np_dtb_se_smmu0_ind_write(uint32_t dev_id,
		uint32_t base_addr,
		uint32_t index,
		uint32_t wrt_mode,
		uint32_t *p_data,
		ZXDH_DTB_ENTRY_T *p_entry)
{
	uint32_t temp_idx;
	uint32_t dtb_ind_addr;
	uint32_t rc = 0;

	switch (wrt_mode) {
	case ZXDH_ERAM128_OPR_128b:
		if ((0xFFFFFFFF - (base_addr)) < (index)) {
			PMD_DRV_LOG(ERR, "ICM %s:%d[Error:VALUE[val0=0x%x]"
				"INVALID] [val1=0x%x] FUNCTION :%s", __FILE__, __LINE__,
				base_addr, index, __func__);

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
	}

	dtb_ind_addr = ((base_addr << 7) & ZXDH_ERAM128_BADDR_MASK) + temp_idx;

	PMD_DRV_LOG(INFO, "dtb eram item 1bit addr: 0x%x", dtb_ind_addr);

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

static uint32_t
zxdh_np_dtb_eram_one_entry(uint32_t dev_id,
		uint32_t sdt_no,
		uint32_t del_en,
		void *pdata,
		uint32_t *p_dtb_len,
		ZXDH_DTB_ENTRY_T *p_dtb_one_entry)
{
	uint32_t buff[ZXDH_SMMU0_READ_REG_MAX_NUM]      = {0};
	ZXDH_SDTTBL_ERAM_T sdt_eram           = {0};
	ZXDH_DTB_ERAM_ENTRY_INFO_T *peramdata = NULL;
	uint32_t base_addr;
	uint32_t index;
	uint32_t opr_mode;
	uint32_t rc;

	ZXDH_COMM_CHECK_POINT(pdata);
	ZXDH_COMM_CHECK_POINT(p_dtb_one_entry);
	ZXDH_COMM_CHECK_POINT(p_dtb_len);

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
	}

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

	*((volatile uint32_t *)(addr)) = data;

	return 0;
}

static uint32_t
zxdh_np_dtb_queue_item_info_set(uint32_t dev_id,
		uint32_t queue_id,
		ZXDH_DTB_QUEUE_ITEM_INFO_T *p_item_info)
{
	ZXDH_DTB_QUEUE_LEN_T dtb_len = {0};
	uint32_t rc;

	dtb_len.cfg_dtb_cmd_type = p_item_info->cmd_type;
	dtb_len.cfg_dtb_cmd_int_en = p_item_info->int_en;
	dtb_len.cfg_queue_dtb_len = p_item_info->data_len;

	rc = zxdh_np_reg_write(dev_id, ZXDH_DTB_CFG_QUEUE_DTB_LEN,
						0, queue_id, (void *)&dtb_len);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "dpp_reg_write");
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

	if (ZXDH_DTB_QUEUE_INIT_FLAG_GET(dev_id, queue_id) == 0) {
		PMD_DRV_LOG(ERR, "dtb queue %d is not init", queue_id);
		return ZXDH_RC_DTB_QUEUE_IS_NOT_INIT;
	}

	if (data_len % 4 != 0)
		return ZXDH_RC_DTB_PARA_INVALID;

	rc = zxdh_np_dtb_queue_enable_get(dev_id, queue_id, &queue_en);
	if (!queue_en) {
		PMD_DRV_LOG(ERR, "the queue %d is not enable!,rc=%d", queue_id, rc);
		return ZXDH_RC_DTB_QUEUE_NOT_ENABLE;
	}

	rc = zxdh_np_dtb_queue_unused_item_num_get(dev_id, queue_id, &unused_item_num);
	if (unused_item_num == 0)
		return ZXDH_RC_DTB_QUEUE_ITEM_HW_EMPTY;

	for (i = 0; i < ZXDH_DTB_QUEUE_ITEM_NUM_MAX; i++) {
		item_index = ZXDH_DTB_TAB_DOWN_WR_INDEX_GET(dev_id, queue_id) %
			ZXDH_DTB_QUEUE_ITEM_NUM_MAX;

		rc = zxdh_np_dtb_item_ack_rd(dev_id, queue_id, 0,
			item_index, 0, &ack_vale);

		ZXDH_DTB_TAB_DOWN_WR_INDEX_GET(dev_id, queue_id)++;

		if ((ack_vale >> 8) == ZXDH_DTB_TAB_ACK_UNUSED_MASK)
			break;
	}

	if (i == ZXDH_DTB_QUEUE_ITEM_NUM_MAX)
		return ZXDH_RC_DTB_QUEUE_ITEM_SW_EMPTY;

	rc = zxdh_np_dtb_item_buff_wr(dev_id, queue_id, 0,
		item_index, 0, data_len, p_data);

	rc = zxdh_np_dtb_item_ack_wr(dev_id, queue_id, 0,
		item_index, 0, ZXDH_DTB_TAB_ACK_IS_USING_MASK);

	item_info.cmd_vld = 1;
	item_info.cmd_type = 0;
	item_info.int_en = int_flag;
	item_info.data_len = data_len / 4;
	phy_addr = p_dpp_dtb_mgr[dev_id]->queue_info[queue_id].tab_down.start_phy_addr +
		item_index * p_dpp_dtb_mgr[dev_id]->queue_info[queue_id].tab_down.item_size;
	item_info.data_hddr = ((phy_addr >> 4) >> 32) & 0xffffffff;
	item_info.data_laddr = (phy_addr >> 4) & 0xffffffff;

	rc = zxdh_np_dtb_queue_item_info_set(dev_id, queue_id, &item_info);
	*p_item_index = item_index;

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

	dtb_interrupt_status = g_dpp_dtb_int_enable;

	rc = zxdh_np_dtb_tab_down_info_set(dev_id,
					queue_id,
					dtb_interrupt_status,
					down_table_len / 4,
					(uint32_t *)p_down_table_buff,
					p_element_id);
	return rc;
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
	uint8_t *p_data_buff_ex;
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

	p_data_buff_ex = rte_zmalloc(NULL, ZXDH_DTB_TABLE_DATA_BUFF_SIZE, 0);
	ZXDH_COMM_CHECK_POINT_MEMORY_FREE(p_data_buff_ex, p_data_buff);

	dtb_one_entry.cmd = entry_cmd;
	dtb_one_entry.data = entry_data;

	max_size = (ZXDH_DTB_TABLE_DATA_BUFF_SIZE / 16) - 1;

	for (entry_index = 0; entry_index < entrynum; entry_index++) {
		pentry = down_entries + entry_index;
		sdt_no = pentry->sdt_no;
		tbl_type = zxdh_np_sdt_tbl_type_get(dev_id, sdt_no);
		switch (tbl_type) {
		case ZXDH_SDT_TBLT_ERAM:
			rc = zxdh_np_dtb_eram_one_entry(dev_id, sdt_no, ZXDH_DTB_ITEM_ADD_OR_UPDATE,
				pentry->p_entry_data, &one_dtb_len, &dtb_one_entry);
			break;
		default:
			PMD_DRV_LOG(ERR, "SDT table_type[ %d ] is invalid!", tbl_type);
			rte_free(p_data_buff);
			rte_free(p_data_buff_ex);
			return 1;
		}

		addr_offset = dtb_len * ZXDH_DTB_LEN_POS_SETP;
		dtb_len += one_dtb_len;
		if (dtb_len > max_size) {
			rte_free(p_data_buff);
			rte_free(p_data_buff_ex);
			PMD_DRV_LOG(ERR, "%s error dtb_len>%u!", __func__,
				max_size);
			return ZXDH_RC_DTB_DOWN_LEN_INVALID;
		}
		rc = zxdh_np_dtb_data_write(p_data_buff, addr_offset, &dtb_one_entry);
		memset(entry_cmd, 0x0, sizeof(entry_cmd));
		memset(entry_data, 0x0, sizeof(entry_data));
	}

	if (dtb_len == 0) {
		rte_free(p_data_buff);
		rte_free(p_data_buff_ex);
		return ZXDH_RC_DTB_DOWN_LEN_INVALID;
	}

	rc = zxdh_np_dtb_write_down_table_data(dev_id,
					queue_id,
					dtb_len * 16,
					p_data_buff,
					&element_id);
	rte_free(p_data_buff);
	rte_free(p_data_buff_ex);

	return rc;
}

static void
zxdh_np_sdt_tbl_data_get(uint32_t dev_id, uint32_t sdt_no, ZXDH_SDT_TBL_DATA_T *p_sdt_data)
{
	p_sdt_data->data_high32 = g_sdt_info[dev_id][sdt_no].data_high32;
	p_sdt_data->data_low32  = g_sdt_info[dev_id][sdt_no].data_low32;
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
	uint8_t *p_data_buff_ex = NULL;
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

	p_data_buff_ex = rte_calloc(NULL, 1, ZXDH_DTB_TABLE_DATA_BUFF_SIZE, 0);
	ZXDH_COMM_CHECK_POINT_MEMORY_FREE(p_data_buff_ex, p_data_buff);

	dtb_one_entry.cmd = entry_cmd;
	dtb_one_entry.data = entry_data;

	max_size = (ZXDH_DTB_TABLE_DATA_BUFF_SIZE / 16) - 1;

	for (entry_index = 0; entry_index < entrynum; entry_index++) {
		pentry = delete_entries + entry_index;

		sdt_no = pentry->sdt_no;
		zxdh_np_sdt_tbl_data_get(dev_id, sdt_no, &sdt_tbl);
		switch (tbl_type) {
		case ZXDH_SDT_TBLT_ERAM:
			rc = zxdh_np_dtb_eram_one_entry(dev_id, sdt_no, ZXDH_DTB_ITEM_DELETE,
				pentry->p_entry_data, &one_dtb_len, &dtb_one_entry);
			break;
		default:
			PMD_DRV_LOG(ERR, "SDT table_type[ %d ] is invalid!", tbl_type);
			rte_free(p_data_buff);
			rte_free(p_data_buff_ex);
			return 1;
		}

		addr_offset = dtb_len * ZXDH_DTB_LEN_POS_SETP;
		dtb_len += one_dtb_len;
		if (dtb_len > max_size) {
			rte_free(p_data_buff);
			rte_free(p_data_buff_ex);
			PMD_DRV_LOG(ERR, "%s error dtb_len>%u!", __func__,
				max_size);
			return ZXDH_RC_DTB_DOWN_LEN_INVALID;
		}

		rc = zxdh_np_dtb_data_write(p_data_buff, addr_offset, &dtb_one_entry);
		memset(entry_cmd, 0x0, sizeof(entry_cmd));
		memset(entry_data, 0x0, sizeof(entry_data));
	}

	if (dtb_len == 0) {
		rte_free(p_data_buff);
		rte_free(p_data_buff_ex);
		return ZXDH_RC_DTB_DOWN_LEN_INVALID;
	}

	rc = zxdh_np_dtb_write_down_table_data(dev_id,
				queue_id,
				dtb_len * 16,
				p_data_buff_ex,
				&element_id);
	rte_free(p_data_buff);
	ZXDH_COMM_CHECK_RC_MEMORY_FREE_NO_ASSERT(rc,
		"dpp_dtb_write_down_table_data", p_data_buff_ex);

	rte_free(p_data_buff_ex);
	return 0;
}

static uint32_t
zxdh_np_sdt_tbl_data_parser(uint32_t sdt_hig32, uint32_t sdt_low32, void *p_sdt_info)
{
	uint32_t tbl_type = 0;
	uint32_t clutch_en = 0;

	ZXDH_SDTTBL_ERAM_T *p_sdt_eram = NULL;
	ZXDH_SDTTBL_PORTTBL_T *p_sdt_porttbl = NULL;

	ZXDH_COMM_UINT32_GET_BITS(tbl_type, sdt_hig32,
		ZXDH_SDT_H_TBL_TYPE_BT_POS, ZXDH_SDT_H_TBL_TYPE_BT_LEN);
	ZXDH_COMM_UINT32_GET_BITS(clutch_en, sdt_low32, 0, 1);

	switch (tbl_type) {
	case ZXDH_SDT_TBLT_ERAM:
		p_sdt_eram = (ZXDH_SDTTBL_ERAM_T *)p_sdt_info;
		p_sdt_eram->table_type = tbl_type;
		p_sdt_eram->eram_clutch_en = clutch_en;
		break;
	case ZXDH_SDT_TBLT_PORTTBL:
		p_sdt_porttbl = (ZXDH_SDTTBL_PORTTBL_T *)p_sdt_info;
		p_sdt_porttbl->table_type = tbl_type;
		p_sdt_porttbl->porttbl_clutch_en = clutch_en;
		break;
	default:
		PMD_DRV_LOG(ERR, "SDT table_type[ %d ] is invalid!", tbl_type);
		return 1;
	}

	return 0;
}

static uint32_t
zxdh_np_soft_sdt_tbl_get(uint32_t dev_id, uint32_t sdt_no, void *p_sdt_info)
{
	ZXDH_SDT_TBL_DATA_T sdt_tbl = {0};
	uint32_t rc;

	zxdh_np_sdt_tbl_data_get(dev_id, sdt_no, &sdt_tbl);

	rc = zxdh_np_sdt_tbl_data_parser(sdt_tbl.data_high32, sdt_tbl.data_low32, p_sdt_info);
	if (rc != 0)
		PMD_DRV_LOG(ERR, "dpp sdt [%d] tbl_data_parser error", sdt_no);

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
	}
	*p_row_index = row_index;
	*p_col_index = col_index;
}

static uint32_t
zxdh_np_dtb_eram_data_get(uint32_t dev_id, uint32_t queue_id, uint32_t sdt_no,
		ZXDH_DTB_ERAM_ENTRY_INFO_T *p_dump_eram_entry)
{
	uint32_t index = p_dump_eram_entry->index;
	uint32_t *p_data = p_dump_eram_entry->p_data;
	ZXDH_SDTTBL_ERAM_T sdt_eram_info = {0};
	uint32_t temp_data[4] = {0};
	uint32_t row_index = 0;
	uint32_t col_index = 0;
	uint32_t rd_mode;
	uint32_t rc;

	rc = zxdh_np_soft_sdt_tbl_get(queue_id, sdt_no, &sdt_eram_info);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "dpp_soft_sdt_tbl_get");
	rd_mode = sdt_eram_info.eram_mode;

	zxdh_np_eram_index_cal(rd_mode, index, &row_index, &col_index);

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
	uint32_t rc = 0;
	uint32_t sdt_no;

	sdt_no = get_entry->sdt_no;
	zxdh_np_sdt_tbl_data_get(srh_mode, sdt_no, &sdt_tbl);

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
	default:
		PMD_DRV_LOG(ERR, "SDT table_type[ %d ] is invalid!", tbl_type);
		return 1;
	}

	return 0;
}

static void
zxdh_np_stat_cfg_soft_get(uint32_t dev_id,
				ZXDH_PPU_STAT_CFG_T *p_stat_cfg)
{
	ZXDH_COMM_CHECK_DEV_POINT(dev_id, p_stat_cfg);

	p_stat_cfg->ddr_base_addr = g_ppu_stat_cfg.ddr_base_addr;
	p_stat_cfg->eram_baddr = g_ppu_stat_cfg.eram_baddr;
	p_stat_cfg->eram_depth = g_ppu_stat_cfg.eram_depth;
	p_stat_cfg->ppu_addr_offset = g_ppu_stat_cfg.ppu_addr_offset;
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

	zxdh_np_dtb_queue_enable_get(dev_id, queue_id, &queue_en);
	if (!queue_en) {
		PMD_DRV_LOG(ERR, "the queue %d is not enable!", queue_id);
		return ZXDH_RC_DTB_QUEUE_NOT_ENABLE;
	}

	if (ZXDH_DTB_QUEUE_INIT_FLAG_GET(dev_id, queue_id) == 0) {
		PMD_DRV_LOG(ERR, "dtb queue %d is not init", queue_id);
		return ZXDH_RC_DTB_QUEUE_IS_NOT_INIT;
	}

	if (desc_len % 4 != 0)
		return ZXDH_RC_DTB_PARA_INVALID;

	zxdh_np_dtb_item_buff_wr(dev_id, queue_id, ZXDH_DTB_DIR_UP_TYPE,
		item_index, 0, desc_len, p_desc_data);

	ZXDH_DTB_TAB_UP_DATA_LEN_GET(dev_id, queue_id, item_index) = data_len;

	item_info.cmd_vld = 1;
	item_info.cmd_type = ZXDH_DTB_DIR_UP_TYPE;
	item_info.int_en = int_flag;
	item_info.data_len = desc_len / 4;

	if (zxdh_np_dev_get_dev_type(dev_id) == ZXDH_DEV_TYPE_SIM)
		return 0;

	rc = zxdh_np_dtb_queue_item_info_set(dev_id, queue_id, &item_info);

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
		PMD_DRV_LOG(ERR, "the queue %d element id %d dump"
			" info set failed!", queue_id, queue_element_id);
		zxdh_np_dtb_item_ack_wr(dev_id, queue_id, ZXDH_DTB_DIR_UP_TYPE,
			queue_element_id, 0, ZXDH_DTB_TAB_ACK_UNUSED_MASK);
	}

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

	if (ZXDH_DTB_QUEUE_INIT_FLAG_GET(dev_id, queue_id) == 0) {
		PMD_DRV_LOG(ERR, "dtb queue %d is not init", queue_id);
		return ZXDH_RC_DTB_QUEUE_IS_NOT_INIT;
	}

	zxdh_np_dtb_queue_unused_item_num_get(dev_id, queue_id, &unused_item_num);

	if (unused_item_num == 0)
		return ZXDH_RC_DTB_QUEUE_ITEM_HW_EMPTY;

	for (i = 0; i < ZXDH_DTB_QUEUE_ITEM_NUM_MAX; i++) {
		item_index = ZXDH_DTB_TAB_UP_WR_INDEX_GET(dev_id, queue_id) %
			ZXDH_DTB_QUEUE_ITEM_NUM_MAX;

		zxdh_np_dtb_item_ack_rd(dev_id, queue_id, ZXDH_DTB_DIR_UP_TYPE, item_index,
			0, &ack_vale);

		ZXDH_DTB_TAB_UP_WR_INDEX_GET(dev_id, queue_id)++;

		if ((ack_vale >> 8) == ZXDH_DTB_TAB_ACK_UNUSED_MASK)
			break;
	}

	if (i == ZXDH_DTB_QUEUE_ITEM_NUM_MAX)
		return ZXDH_RC_DTB_QUEUE_ITEM_SW_EMPTY;

	zxdh_np_dtb_item_ack_wr(dev_id, queue_id, ZXDH_DTB_DIR_UP_TYPE, item_index,
		0, ZXDH_DTB_TAB_ACK_IS_USING_MASK);

	*p_item_index = item_index;


	return 0;
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
		PMD_DRV_LOG(ERR, "dtb queue %d is not init", queue_id);
		return ZXDH_RC_DTB_QUEUE_IS_NOT_INIT;
	}

	if (ZXDH_DTB_TAB_UP_USER_PHY_ADDR_FLAG_GET(dev_id, queue_id, item_index) ==
		ZXDH_DTB_TAB_UP_USER_ADDR_TYPE)
		addr = ZXDH_DTB_TAB_UP_USER_PHY_ADDR_GET(dev_id, queue_id, item_index);
	else
		addr = ZXDH_DTB_ITEM_ACK_SIZE;

	*p_phy_haddr = (addr >> 32) & 0xffffffff;
	*p_phy_laddr = addr & 0xffffffff;

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
		PMD_DRV_LOG(ERR, "dpp_dtb_tab_up_free_item_get failed = %d!", base_addr);
		return ZXDH_RC_DTB_QUEUE_ITEM_SW_EMPTY;
	}

	*element_id = queue_item_index;

	rc = zxdh_np_dtb_tab_up_item_addr_get(dev_id, queue_id, queue_item_index,
		&dump_dst_phy_haddr, &dump_dst_phy_laddr);
	ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "dpp_dtb_tab_up_item_addr_get");

	data_len = depth * 128 / 32;
	desc_len = ZXDH_DTB_LEN_POS_SETP / 4;

	rc = zxdh_np_dtb_write_dump_desc_info(dev_id, queue_id, queue_item_index,
		(uint32_t *)form_buff, data_len, desc_len, p_data);
	ZXDH_COMM_CHECK_RC_NO_ASSERT(rc, "dpp_dtb_write_dump_desc_info");

	return rc;
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
zxdh_np_se_done_status_check(uint32_t dev_id, uint32_t reg_no, uint32_t pos)
{
	uint32_t rc = 0;

	uint32_t data = 0;
	uint32_t rd_cnt = 0;
	uint32_t done_flag = 0;

	while (!done_flag) {
		rc = zxdh_np_reg_read(dev_id, reg_no, 0, 0, &data);
		if (rc != 0) {
			PMD_DRV_LOG(ERR, " [ErrorCode:0x%x] !-- zxdh_np_reg_read Fail!", rc);
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
	uint32_t rc = 0;
	uint32_t i = 0;
	uint32_t row_index = 0;
	uint32_t col_index = 0;
	uint32_t temp_data[4] = {0};
	uint32_t *p_temp_data = NULL;
	ZXDH_SMMU0_SMMU0_CPU_IND_CMD_T cpu_ind_cmd = {0};

	rc = zxdh_np_se_done_status_check(dev_id, ZXDH_SMMU0_SMMU0_WR_ARB_CPU_RDYR, 0);

	if (rd_clr_mode == ZXDH_RD_MODE_HOLD) {
		cpu_ind_cmd.cpu_ind_rw = ZXDH_SE_OPR_RD;
		cpu_ind_cmd.cpu_ind_rd_mode = ZXDH_RD_MODE_HOLD;
		cpu_ind_cmd.cpu_req_mode = ZXDH_ERAM128_OPR_128b;

		switch (rd_mode) {
		case ZXDH_ERAM128_OPR_128b:
			if ((0xFFFFFFFF - (base_addr)) < (index))
				return ZXDH_PAR_CHK_INVALID_INDEX;

			if (base_addr + index > ZXDH_SE_SMMU0_ERAM_ADDR_NUM_TOTAL - 1) {
				PMD_DRV_LOG(ERR, "%s : index out of range !", __func__);
				return -1;
			}

			row_index = (index << 7) & ZXDH_ERAM128_BADDR_MASK;
			break;
		case ZXDH_ERAM128_OPR_64b:
			if ((base_addr + (index >> 1)) > ZXDH_SE_SMMU0_ERAM_ADDR_NUM_TOTAL - 1) {
				PMD_DRV_LOG(ERR, "%s : index out of range !", __func__);
				return -1;
			}

			row_index = (index << 6) & ZXDH_ERAM128_BADDR_MASK;
			col_index = index & 0x1;
			break;
		case ZXDH_ERAM128_OPR_32b:
			if ((base_addr + (index >> 2)) > ZXDH_SE_SMMU0_ERAM_ADDR_NUM_TOTAL - 1) {
				PMD_DRV_LOG(ERR, "%s : index out of range !", __func__);
				return -1;
			}

			row_index = (index << 5) & ZXDH_ERAM128_BADDR_MASK;
			col_index = index & 0x3;
			break;
		case ZXDH_ERAM128_OPR_1b:
			if ((base_addr + (index >> 7)) > ZXDH_SE_SMMU0_ERAM_ADDR_NUM_TOTAL - 1) {
				PMD_DRV_LOG(ERR, "%s : index out of range !", __func__);
				return -1;
			}
			row_index = index & ZXDH_ERAM128_BADDR_MASK;
			col_index = index & 0x7F;
			break;
		}

		cpu_ind_cmd.cpu_ind_addr = ((base_addr << 7) & ZXDH_ERAM128_BADDR_MASK) + row_index;
	} else {
		cpu_ind_cmd.cpu_ind_rw = ZXDH_SE_OPR_RD;
		cpu_ind_cmd.cpu_ind_rd_mode = ZXDH_RD_MODE_CLEAR;

		switch (rd_mode) {
		case ZXDH_ERAM128_OPR_128b:
			if ((0xFFFFFFFF - (base_addr)) < (index)) {
				PMD_DRV_LOG(ERR, "%s : index 0x%x is invalid!", __func__, index);
				return ZXDH_PAR_CHK_INVALID_INDEX;
			}
			if (base_addr + index > ZXDH_SE_SMMU0_ERAM_ADDR_NUM_TOTAL - 1) {
				PMD_DRV_LOG(ERR, "%s : index out of range !", __func__);
				return -1;
			}
			row_index = (index << 7);
			cpu_ind_cmd.cpu_req_mode = ZXDH_ERAM128_OPR_128b;
			break;
		case ZXDH_ERAM128_OPR_64b:
			if ((base_addr + (index >> 1)) > ZXDH_SE_SMMU0_ERAM_ADDR_NUM_TOTAL - 1) {
				PMD_DRV_LOG(ERR, "%s : index out of range !", __func__);
				return -1;
			}

			row_index = (index << 6);
			cpu_ind_cmd.cpu_req_mode = 2;
			break;
		case ZXDH_ERAM128_OPR_32b:
			if ((base_addr + (index >> 2)) > ZXDH_SE_SMMU0_ERAM_ADDR_NUM_TOTAL - 1) {
				PMD_DRV_LOG(ERR, "%s : index out of range !", __func__);
				return -1;
			}
			row_index = (index << 5);
			cpu_ind_cmd.cpu_req_mode = 1;
			break;
		case ZXDH_ERAM128_OPR_1b:
			PMD_DRV_LOG(ERR, "rd_clr_mode[%d] or rd_mode[%d] error! ",
				rd_clr_mode, rd_mode);
			return -1;
		}
		cpu_ind_cmd.cpu_ind_addr = ((base_addr << 7) & ZXDH_ERAM128_BADDR_MASK) + row_index;
	}

	rc = zxdh_np_reg_write(dev_id,
						ZXDH_SMMU0_SMMU0_CPU_IND_CMDR,
						0,
						0,
						&cpu_ind_cmd);

	rc = zxdh_np_se_done_status_check(dev_id, ZXDH_SMMU0_SMMU0_RD_CPU_IND_DONER, 0);

	p_temp_data = temp_data;
	for (i = 0; i < 4; i++) {
		rc = zxdh_np_reg_read(dev_id,
							ZXDH_SMMU0_SMMU0_CPU_IND_RDAT0R + i,
							0,
							0,
							p_temp_data + 3 - i);
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
		}
	}

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
zxdh_np_agent_channel_sync_send(ZXDH_AGENT_CHANNEL_MSG_T *p_msg,
				uint32_t *p_data,
				uint32_t rep_len)
{
	uint32_t ret = 0;
	uint32_t vport = 0;
	struct zxdh_pci_bar_msg in = {0};
	struct zxdh_msg_recviver_mem result = {0};
	uint32_t *recv_buffer;
	uint8_t *reply_ptr = NULL;
	uint16_t reply_msg_len = 0;
	uint64_t agent_addr = 0;

	if (ZXDH_IS_PF(vport))
		in.src = ZXDH_MSG_CHAN_END_PF;
	else
		in.src = ZXDH_MSG_CHAN_END_VF;

	in.virt_addr = agent_addr;
	in.payload_addr = p_msg->msg;
	in.payload_len = p_msg->msg_len;
	in.dst = ZXDH_MSG_CHAN_END_RISC;
	in.module_id = ZXDH_BAR_MDOULE_NPSDK;

	recv_buffer = (uint32_t *)rte_zmalloc(NULL, rep_len + ZXDH_CHANNEL_REPS_LEN, 0);
	if (recv_buffer == NULL) {
		PMD_DRV_LOG(ERR, "%s point null!", __func__);
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
		PMD_DRV_LOG(ERR, "Error[0x%x], %s failed!", ret, __func__);
	}

	rte_free(recv_buffer);
	return ret;
}

static uint32_t
zxdh_np_agent_channel_plcr_sync_send(ZXDH_AGENT_CHANNEL_PLCR_MSG_T *p_msg,
		uint32_t *p_data, uint32_t rep_len)
{
	uint32_t ret = 0;
	ZXDH_AGENT_CHANNEL_MSG_T agent_msg = {0};

	agent_msg.msg = (void *)p_msg;
	agent_msg.msg_len = sizeof(ZXDH_AGENT_CHANNEL_PLCR_MSG_T);

	ret = zxdh_np_agent_channel_sync_send(&agent_msg, p_data, rep_len);
	if (ret != 0)	{
		PMD_DRV_LOG(ERR, "%s: agent_channel_sync_send failed.", __func__);
		return 1;
	}

	return 0;
}

static uint32_t
zxdh_np_agent_channel_plcr_profileid_request(uint32_t vport,
	uint32_t car_type, uint32_t *p_profileid)
{
	uint32_t ret = 0;
	uint32_t resp_buffer[2] = {0};

	ZXDH_AGENT_CHANNEL_PLCR_MSG_T msgcfg = {0};

	msgcfg.dev_id = 0;
	msgcfg.type = ZXDH_PLCR_MSG;
	msgcfg.oper = ZXDH_PROFILEID_REQUEST;
	msgcfg.vport = vport;
	msgcfg.car_type = car_type;
	msgcfg.profile_id = 0xFFFF;

	ret = zxdh_np_agent_channel_plcr_sync_send(&msgcfg,
		resp_buffer, sizeof(resp_buffer));
	if (ret != 0)	{
		PMD_DRV_LOG(ERR, "%s: agent_channel_plcr_sync_send failed.", __func__);
		return 1;
	}

	memcpy(p_profileid, resp_buffer, sizeof(uint32_t) * ZXDH_SCHE_RSP_LEN);

	return ret;
}

static uint32_t
zxdh_np_agent_channel_plcr_car_rate(uint32_t car_type,
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

		ret = zxdh_np_agent_channel_sync_send(&agent_msg, resp_buffer, resp_len);
		if (ret != 0)	{
			PMD_DRV_LOG(ERR, "%s: stat_car_a_type failed.", __func__);
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

		ret = zxdh_np_agent_channel_sync_send(&agent_msg, resp_buffer, resp_len);
		if (ret != 0)	{
			PMD_DRV_LOG(ERR, "%s: stat_car_b_type failed.", __func__);
			return 1;
		}

		ret = *(uint8_t *)resp_buffer;
	}

	return ret;
}

static uint32_t
zxdh_np_agent_channel_plcr_profileid_release(uint32_t vport,
		uint32_t car_type __rte_unused,
		uint32_t profileid)
{
	uint32_t ret = 0;
	uint32_t resp_buffer[2] = {0};

	ZXDH_AGENT_CHANNEL_PLCR_MSG_T msgcfg = {0};

	msgcfg.dev_id = 0;
	msgcfg.type = ZXDH_PLCR_MSG;
	msgcfg.oper = ZXDH_PROFILEID_RELEASE;
	msgcfg.vport = vport;
	msgcfg.profile_id = profileid;

	ret = zxdh_np_agent_channel_plcr_sync_send(&msgcfg,
		resp_buffer, sizeof(resp_buffer));
	if (ret != 0)	{
		PMD_DRV_LOG(ERR, "%s: agent_channel_plcr_sync_send failed.", __func__);
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

	ZXDH_STAT_CAR0_CARA_QUEUE_RAM0_159_0_T queue_cfg = {0};

	queue_cfg.cara_drop = drop_flag;
	queue_cfg.cara_plcr_en = plcr_en;
	queue_cfg.cara_profile_id = profile_id;
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

	ZXDH_STAT_CAR0_CARB_QUEUE_RAM0_159_0_T queue_cfg = {0};

	queue_cfg.carb_drop = drop_flag;
	queue_cfg.carb_plcr_en = plcr_en;
	queue_cfg.carb_profile_id = profile_id;

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

	ZXDH_STAT_CAR0_CARC_QUEUE_RAM0_159_0_T queue_cfg = {0};
	queue_cfg.carc_drop = drop_flag;
	queue_cfg.carc_plcr_en = plcr_en;
	queue_cfg.carc_profile_id = profile_id;

	rc = zxdh_np_reg_write(dev_id,
						ZXDH_STAT_CAR0_CARC_QUEUE_RAM0,
						0,
						flow_id,
						&queue_cfg);
	ZXDH_COMM_CHECK_DEV_RC(dev_id, rc, "zxdh_np_reg_write");

	return rc;
}

uint32_t
zxdh_np_car_profile_id_add(uint32_t vport_id,
		uint32_t flags,
		uint64_t *p_profile_id)
{
	uint32_t ret = 0;
	uint32_t *profile_id;
	uint32_t profile_id_h = 0;
	uint32_t profile_id_l = 0;
	uint64_t temp_profile_id = 0;

	profile_id = (uint32_t *)rte_zmalloc(NULL, ZXDH_G_PROFILE_ID_LEN, 0);
	if (profile_id == NULL) {
		PMD_DRV_LOG(ERR, "%s: profile_id point null!", __func__);
		return ZXDH_PAR_CHK_POINT_NULL;
	}
	ret = zxdh_np_agent_channel_plcr_profileid_request(vport_id, flags, profile_id);

	profile_id_h = *(profile_id + 1);
	profile_id_l = *profile_id;
	rte_free(profile_id);

	temp_profile_id = (((uint64_t)profile_id_l) << 32) | ((uint64_t)profile_id_h);
	if (0 != (uint32_t)(temp_profile_id >> 56)) {
		PMD_DRV_LOG(ERR, "%s: profile_id is overflow!", __func__);
		return 1;
	}

	*p_profile_id = temp_profile_id;

	return ret;
}

uint32_t
zxdh_np_car_profile_cfg_set(uint32_t vport_id __rte_unused,
		uint32_t car_type,
		uint32_t pkt_sign,
		uint32_t profile_id,
		void *p_car_profile_cfg)
{
	uint32_t ret = 0;

	ret = zxdh_np_agent_channel_plcr_car_rate(car_type,
		pkt_sign, profile_id, p_car_profile_cfg);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "%s: plcr_car_rate set failed!", __func__);
		return 1;
	}

	return ret;
}

uint32_t
zxdh_np_car_profile_id_delete(uint32_t vport_id,
	uint32_t flags, uint64_t profile_id)
{
	uint32_t ret = 0;
	uint32_t profileid = 0;

	profileid = profile_id & 0xFFFF;

	ret = zxdh_np_agent_channel_plcr_profileid_release(vport_id, flags, profileid);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "%s: plcr profiled id release failed!", __func__);
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
			PMD_DRV_LOG(ERR, "%s: stat car a type flow_id invalid!", __func__);
			return ZXDH_PAR_CHK_INVALID_INDEX;
		}

		if (profile_id > ZXDH_CAR_A_PROFILE_ID_MAX) {
			PMD_DRV_LOG(ERR, "%s: stat car a type profile_id invalid!", __func__);
			return ZXDH_PAR_CHK_INVALID_INDEX;
		}
	} else if (car_type == ZXDH_STAT_CAR_B_TYPE) {
		if (flow_id > ZXDH_CAR_B_FLOW_ID_MAX) {
			PMD_DRV_LOG(ERR, "%s: stat car b type flow_id invalid!", __func__);
			return ZXDH_PAR_CHK_INVALID_INDEX;
		}

		if (profile_id > ZXDH_CAR_B_PROFILE_ID_MAX) {
			PMD_DRV_LOG(ERR, "%s: stat car b type profile_id invalid!", __func__);
			return ZXDH_PAR_CHK_INVALID_INDEX;
		}
	} else {
		if (flow_id > ZXDH_CAR_C_FLOW_ID_MAX) {
			PMD_DRV_LOG(ERR, "%s: stat car c type flow_id invalid!", __func__);
			return ZXDH_PAR_CHK_INVALID_INDEX;
		}

		if (profile_id > ZXDH_CAR_C_PROFILE_ID_MAX) {
			PMD_DRV_LOG(ERR, "%s: stat car c type profile_id invalid!", __func__);
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
