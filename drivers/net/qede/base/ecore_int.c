/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#include "bcm_osal.h"
#include "ecore.h"
#include "ecore_spq.h"
#include "reg_addr.h"
#include "ecore_gtt_reg_addr.h"
#include "ecore_init_ops.h"
#include "ecore_rt_defs.h"
#include "ecore_int.h"
#include "reg_addr.h"
#include "ecore_hw.h"
#include "ecore_sriov.h"
#include "ecore_vf.h"
#include "ecore_hw_defs.h"
#include "ecore_hsi_common.h"
#include "ecore_mcp.h"

struct ecore_pi_info {
	ecore_int_comp_cb_t comp_cb;
	void *cookie;		/* Will be sent to the compl cb function */
};

struct ecore_sb_sp_info {
	struct ecore_sb_info sb_info;
	/* per protocol index data */
	struct ecore_pi_info pi_info_arr[PIS_PER_SB];
};

enum ecore_attention_type {
	ECORE_ATTN_TYPE_ATTN,
	ECORE_ATTN_TYPE_PARITY,
};

#define SB_ATTN_ALIGNED_SIZE(p_hwfn) \
	ALIGNED_TYPE_SIZE(struct atten_status_block, p_hwfn)

struct aeu_invert_reg_bit {
	char bit_name[30];

#define ATTENTION_PARITY		(1 << 0)

#define ATTENTION_LENGTH_MASK		(0x00000ff0)
#define ATTENTION_LENGTH_SHIFT		(4)
#define ATTENTION_LENGTH(flags)		(((flags) & ATTENTION_LENGTH_MASK) >> \
					 ATTENTION_LENGTH_SHIFT)
#define ATTENTION_SINGLE		(1 << ATTENTION_LENGTH_SHIFT)
#define ATTENTION_PAR			(ATTENTION_SINGLE | ATTENTION_PARITY)
#define ATTENTION_PAR_INT		((2 << ATTENTION_LENGTH_SHIFT) | \
					 ATTENTION_PARITY)

/* Multiple bits start with this offset */
#define ATTENTION_OFFSET_MASK		(0x000ff000)
#define ATTENTION_OFFSET_SHIFT		(12)

#define	ATTENTION_CLEAR_ENABLE		(1 << 28)
#define	ATTENTION_FW_DUMP		(1 << 29)
#define	ATTENTION_PANIC_DUMP		(1 << 30)
	unsigned int flags;

	/* Callback to call if attention will be triggered */
	enum _ecore_status_t (*cb)(struct ecore_hwfn *p_hwfn);

	enum block_id block_index;
};

struct aeu_invert_reg {
	struct aeu_invert_reg_bit bits[32];
};

#define NUM_ATTN_REGS		(9)

#define ATTN_STATE_BITS		(0xfff)
#define ATTN_BITS_MASKABLE	(0x3ff)
struct ecore_sb_attn_info {
	/* Virtual & Physical address of the SB */
	struct atten_status_block *sb_attn;
	dma_addr_t sb_phys;

	/* Last seen running index */
	u16 index;

	/* A mask of the AEU bits resulting in a parity error */
	u32 parity_mask[NUM_ATTN_REGS];

	/* A pointer to the attention description structure */
	struct aeu_invert_reg *p_aeu_desc;

	/* Previously asserted attentions, which are still unasserted */
	u16 known_attn;

	/* Cleanup address for the link's general hw attention */
	u32 mfw_attn_addr;
};

static u16 ecore_attn_update_idx(struct ecore_hwfn *p_hwfn,
				 struct ecore_sb_attn_info *p_sb_desc)
{
	u16 rc = 0, index;

	OSAL_MMIOWB(p_hwfn->p_dev);

	index = OSAL_LE16_TO_CPU(p_sb_desc->sb_attn->sb_index);
	if (p_sb_desc->index != index) {
		p_sb_desc->index = index;
		rc = ECORE_SB_ATT_IDX;
	}

	OSAL_MMIOWB(p_hwfn->p_dev);

	return rc;
}

static void ecore_sb_ack_attn(struct ecore_hwfn *p_hwfn,
			      void OSAL_IOMEM *igu_addr, u32 ack_cons)
{
	struct igu_prod_cons_update igu_ack = { 0 };

	igu_ack.sb_id_and_flags =
	    ((ack_cons << IGU_PROD_CONS_UPDATE_SB_INDEX_SHIFT) |
	     (1 << IGU_PROD_CONS_UPDATE_UPDATE_FLAG_SHIFT) |
	     (IGU_INT_NOP << IGU_PROD_CONS_UPDATE_ENABLE_INT_SHIFT) |
	     (IGU_SEG_ACCESS_ATTN <<
	      IGU_PROD_CONS_UPDATE_SEGMENT_ACCESS_SHIFT));

	DIRECT_REG_WR(p_hwfn, igu_addr, igu_ack.sb_id_and_flags);

	/* Both segments (interrupts & acks) are written to same place address;
	 * Need to guarantee all commands will be received (in-order) by HW.
	 */
	OSAL_MMIOWB(p_hwfn->p_dev);
	OSAL_BARRIER(p_hwfn->p_dev);
}

void ecore_int_sp_dpc(osal_int_ptr_t hwfn_cookie)
{
	struct ecore_hwfn *p_hwfn = (struct ecore_hwfn *)hwfn_cookie;
	struct ecore_pi_info *pi_info = OSAL_NULL;
	struct ecore_sb_attn_info *sb_attn;
	struct ecore_sb_info *sb_info;
	static int arr_size;
	u16 rc = 0;

	if (!p_hwfn) {
		DP_ERR(p_hwfn->p_dev, "DPC called - no hwfn!\n");
		return;
	}

	if (!p_hwfn->p_sp_sb) {
		DP_ERR(p_hwfn->p_dev, "DPC called - no p_sp_sb\n");
		return;
	}

	sb_info = &p_hwfn->p_sp_sb->sb_info;
	arr_size = OSAL_ARRAY_SIZE(p_hwfn->p_sp_sb->pi_info_arr);
	if (!sb_info) {
		DP_ERR(p_hwfn->p_dev,
		       "Status block is NULL - cannot ack interrupts\n");
		return;
	}

	if (!p_hwfn->p_sb_attn) {
		DP_ERR(p_hwfn->p_dev, "DPC called - no p_sb_attn");
		return;
	}
	sb_attn = p_hwfn->p_sb_attn;

	DP_VERBOSE(p_hwfn, ECORE_MSG_INTR, "DPC Called! (hwfn %p %d)\n",
		   p_hwfn, p_hwfn->my_id);

	/* Disable ack for def status block. Required both for msix +
	 * inta in non-mask mode, in inta does no harm.
	 */
	ecore_sb_ack(sb_info, IGU_INT_DISABLE, 0);

	/* Gather Interrupts/Attentions information */
	if (!sb_info->sb_virt) {
		DP_ERR(p_hwfn->p_dev,
		       "Interrupt Status block is NULL -"
		       " cannot check for new interrupts!\n");
	} else {
		u32 tmp_index = sb_info->sb_ack;
		rc = ecore_sb_update_sb_idx(sb_info);
		DP_VERBOSE(p_hwfn->p_dev, ECORE_MSG_INTR,
			   "Interrupt indices: 0x%08x --> 0x%08x\n",
			   tmp_index, sb_info->sb_ack);
	}

	if (!sb_attn || !sb_attn->sb_attn) {
		DP_ERR(p_hwfn->p_dev,
		       "Attentions Status block is NULL -"
		       " cannot check for new attentions!\n");
	} else {
		u16 tmp_index = sb_attn->index;

		rc |= ecore_attn_update_idx(p_hwfn, sb_attn);
		DP_VERBOSE(p_hwfn->p_dev, ECORE_MSG_INTR,
			   "Attention indices: 0x%08x --> 0x%08x\n",
			   tmp_index, sb_attn->index);
	}

	/* Check if we expect interrupts at this time. if not just ack them */
	if (!(rc & ECORE_SB_EVENT_MASK)) {
		ecore_sb_ack(sb_info, IGU_INT_ENABLE, 1);
		return;
	}

	/* Check the validity of the DPC ptt. If not ack interrupts and fail */
	if (!p_hwfn->p_dpc_ptt) {
		DP_NOTICE(p_hwfn->p_dev, true, "Failed to allocate PTT\n");
		ecore_sb_ack(sb_info, IGU_INT_ENABLE, 1);
		return;
	}

	if (rc & ECORE_SB_IDX) {
		int pi;

		/* Since we only looked at the SB index, it's possible more
		 * than a single protocol-index on the SB incremented.
		 * Iterate over all configured protocol indices and check
		 * whether something happened for each.
		 */
		for (pi = 0; pi < arr_size; pi++) {
			pi_info = &p_hwfn->p_sp_sb->pi_info_arr[pi];
			if (pi_info->comp_cb != OSAL_NULL)
				pi_info->comp_cb(p_hwfn, pi_info->cookie);
		}
	}

	if (sb_attn && (rc & ECORE_SB_ATT_IDX)) {
		/* This should be done before the interrupts are enabled,
		 * since otherwise a new attention will be generated.
		 */
		ecore_sb_ack_attn(p_hwfn, sb_info->igu_addr, sb_attn->index);
	}

	ecore_sb_ack(sb_info, IGU_INT_ENABLE, 1);
}

static void ecore_int_sb_attn_free(struct ecore_hwfn *p_hwfn)
{
	struct ecore_sb_attn_info *p_sb = p_hwfn->p_sb_attn;

	if (!p_sb)
		return;

	if (p_sb->sb_attn) {
		OSAL_DMA_FREE_COHERENT(p_hwfn->p_dev, p_sb->sb_attn,
				       p_sb->sb_phys,
				       SB_ATTN_ALIGNED_SIZE(p_hwfn));
	}
	OSAL_FREE(p_hwfn->p_dev, p_sb);
}

/* coalescing timeout = timeset << (timer_res + 1) */
#ifdef RTE_LIBRTE_QEDE_RX_COAL_US
#define ECORE_CAU_DEF_RX_USECS RTE_LIBRTE_QEDE_RX_COAL_US
#else
#define ECORE_CAU_DEF_RX_USECS 24
#endif

#ifdef RTE_LIBRTE_QEDE_TX_COAL_US
#define ECORE_CAU_DEF_TX_USECS RTE_LIBRTE_QEDE_TX_COAL_US
#else
#define ECORE_CAU_DEF_TX_USECS 48
#endif

void ecore_init_cau_sb_entry(struct ecore_hwfn *p_hwfn,
			     struct cau_sb_entry *p_sb_entry,
			     u8 pf_id, u16 vf_number, u8 vf_valid)
{
	struct ecore_dev *p_dev = p_hwfn->p_dev;
	u32 cau_state;

	OSAL_MEMSET(p_sb_entry, 0, sizeof(*p_sb_entry));

	SET_FIELD(p_sb_entry->params, CAU_SB_ENTRY_PF_NUMBER, pf_id);
	SET_FIELD(p_sb_entry->params, CAU_SB_ENTRY_VF_NUMBER, vf_number);
	SET_FIELD(p_sb_entry->params, CAU_SB_ENTRY_VF_VALID, vf_valid);
	SET_FIELD(p_sb_entry->params, CAU_SB_ENTRY_SB_TIMESET0, 0x7F);
	SET_FIELD(p_sb_entry->params, CAU_SB_ENTRY_SB_TIMESET1, 0x7F);

	/* setting the time resultion to a fixed value ( = 1) */
	SET_FIELD(p_sb_entry->params, CAU_SB_ENTRY_TIMER_RES0,
		  ECORE_CAU_DEF_RX_TIMER_RES);
	SET_FIELD(p_sb_entry->params, CAU_SB_ENTRY_TIMER_RES1,
		  ECORE_CAU_DEF_TX_TIMER_RES);

	cau_state = CAU_HC_DISABLE_STATE;

	if (p_dev->int_coalescing_mode == ECORE_COAL_MODE_ENABLE) {
		cau_state = CAU_HC_ENABLE_STATE;
		if (!p_dev->rx_coalesce_usecs) {
			p_dev->rx_coalesce_usecs = ECORE_CAU_DEF_RX_USECS;
			DP_INFO(p_dev, "Coalesce params rx-usecs=%u\n",
				p_dev->rx_coalesce_usecs);
		}
		if (!p_dev->tx_coalesce_usecs) {
			p_dev->tx_coalesce_usecs = ECORE_CAU_DEF_TX_USECS;
			DP_INFO(p_dev, "Coalesce params tx-usecs=%u\n",
				p_dev->tx_coalesce_usecs);
		}
	}

	SET_FIELD(p_sb_entry->data, CAU_SB_ENTRY_STATE0, cau_state);
	SET_FIELD(p_sb_entry->data, CAU_SB_ENTRY_STATE1, cau_state);
}

void ecore_int_cau_conf_sb(struct ecore_hwfn *p_hwfn,
			   struct ecore_ptt *p_ptt,
			   dma_addr_t sb_phys, u16 igu_sb_id,
			   u16 vf_number, u8 vf_valid)
{
	struct cau_sb_entry sb_entry;

	ecore_init_cau_sb_entry(p_hwfn, &sb_entry, p_hwfn->rel_pf_id,
				vf_number, vf_valid);

	if (p_hwfn->hw_init_done) {
		/* Wide-bus, initialize via DMAE */
		u64 phys_addr = (u64)sb_phys;

		ecore_dmae_host2grc(p_hwfn, p_ptt,
				    (u64)(osal_uintptr_t)&phys_addr,
				    CAU_REG_SB_ADDR_MEMORY +
				    igu_sb_id * sizeof(u64), 2, 0);
		ecore_dmae_host2grc(p_hwfn, p_ptt,
				    (u64)(osal_uintptr_t)&sb_entry,
				    CAU_REG_SB_VAR_MEMORY +
				    igu_sb_id * sizeof(u64), 2, 0);
	} else {
		/* Initialize Status Block Address */
		STORE_RT_REG_AGG(p_hwfn,
				 CAU_REG_SB_ADDR_MEMORY_RT_OFFSET +
				 igu_sb_id * 2, sb_phys);

		STORE_RT_REG_AGG(p_hwfn,
				 CAU_REG_SB_VAR_MEMORY_RT_OFFSET +
				 igu_sb_id * 2, sb_entry);
	}

	/* Configure pi coalescing if set */
	if (p_hwfn->p_dev->int_coalescing_mode == ECORE_COAL_MODE_ENABLE) {
		u8 num_tc = 1;	/* @@@TBD aelior ECORE_MULTI_COS */
		u8 timeset = p_hwfn->p_dev->rx_coalesce_usecs >>
		    (ECORE_CAU_DEF_RX_TIMER_RES + 1);
		u8 i;

		ecore_int_cau_conf_pi(p_hwfn, p_ptt, igu_sb_id, RX_PI,
				      ECORE_COAL_RX_STATE_MACHINE, timeset);

		timeset = p_hwfn->p_dev->tx_coalesce_usecs >>
		    (ECORE_CAU_DEF_TX_TIMER_RES + 1);

		for (i = 0; i < num_tc; i++) {
			ecore_int_cau_conf_pi(p_hwfn, p_ptt,
					      igu_sb_id, TX_PI(i),
					      ECORE_COAL_TX_STATE_MACHINE,
					      timeset);
		}
	}
}

void ecore_int_cau_conf_pi(struct ecore_hwfn *p_hwfn,
			   struct ecore_ptt *p_ptt,
			   u16 igu_sb_id, u32 pi_index,
			   enum ecore_coalescing_fsm coalescing_fsm, u8 timeset)
{
	struct cau_pi_entry pi_entry;
	u32 sb_offset, pi_offset;

	if (IS_VF(p_hwfn->p_dev))
		return;		/* @@@TBD MichalK- VF CAU... */

	sb_offset = igu_sb_id * PIS_PER_SB;
	OSAL_MEMSET(&pi_entry, 0, sizeof(struct cau_pi_entry));

	SET_FIELD(pi_entry.prod, CAU_PI_ENTRY_PI_TIMESET, timeset);
	if (coalescing_fsm == ECORE_COAL_RX_STATE_MACHINE)
		SET_FIELD(pi_entry.prod, CAU_PI_ENTRY_FSM_SEL, 0);
	else
		SET_FIELD(pi_entry.prod, CAU_PI_ENTRY_FSM_SEL, 1);

	pi_offset = sb_offset + pi_index;
	if (p_hwfn->hw_init_done) {
		ecore_wr(p_hwfn, p_ptt,
			 CAU_REG_PI_MEMORY + pi_offset * sizeof(u32),
			 *((u32 *)&(pi_entry)));
	} else {
		STORE_RT_REG(p_hwfn,
			     CAU_REG_PI_MEMORY_RT_OFFSET + pi_offset,
			     *((u32 *)&(pi_entry)));
	}
}

void ecore_int_sb_setup(struct ecore_hwfn *p_hwfn,
			struct ecore_ptt *p_ptt, struct ecore_sb_info *sb_info)
{
	/* zero status block and ack counter */
	sb_info->sb_ack = 0;
	OSAL_MEMSET(sb_info->sb_virt, 0, sizeof(*sb_info->sb_virt));

	if (IS_PF(p_hwfn->p_dev))
		ecore_int_cau_conf_sb(p_hwfn, p_ptt, sb_info->sb_phys,
				      sb_info->igu_sb_id, 0, 0);
}

/**
 * @brief ecore_get_igu_sb_id - given a sw sb_id return the
 *        igu_sb_id
 *
 * @param p_hwfn
 * @param sb_id
 *
 * @return u16
 */
static u16 ecore_get_igu_sb_id(struct ecore_hwfn *p_hwfn, u16 sb_id)
{
	u16 igu_sb_id;

	/* Assuming continuous set of IGU SBs dedicated for given PF */
	if (sb_id == ECORE_SP_SB_ID)
		igu_sb_id = p_hwfn->hw_info.p_igu_info->igu_dsb_id;
	else if (IS_PF(p_hwfn->p_dev))
		igu_sb_id = sb_id + p_hwfn->hw_info.p_igu_info->igu_base_sb;
	else
		igu_sb_id = ecore_vf_get_igu_sb_id(p_hwfn, sb_id);

	if (sb_id == ECORE_SP_SB_ID)
		DP_VERBOSE(p_hwfn, ECORE_MSG_INTR,
			   "Slowpath SB index in IGU is 0x%04x\n", igu_sb_id);
	else
		DP_VERBOSE(p_hwfn, ECORE_MSG_INTR,
			   "SB [%04x] <--> IGU SB [%04x]\n", sb_id, igu_sb_id);

	return igu_sb_id;
}

enum _ecore_status_t ecore_int_sb_init(struct ecore_hwfn *p_hwfn,
				       struct ecore_ptt *p_ptt,
				       struct ecore_sb_info *sb_info,
				       void *sb_virt_addr,
				       dma_addr_t sb_phy_addr, u16 sb_id)
{
	sb_info->sb_virt = sb_virt_addr;
	sb_info->sb_phys = sb_phy_addr;

	sb_info->igu_sb_id = ecore_get_igu_sb_id(p_hwfn, sb_id);

	if (sb_id != ECORE_SP_SB_ID) {
		p_hwfn->sbs_info[sb_id] = sb_info;
		p_hwfn->num_sbs++;
	}
#ifdef ECORE_CONFIG_DIRECT_HWFN
	sb_info->p_hwfn = p_hwfn;
#endif
	sb_info->p_dev = p_hwfn->p_dev;

	/* The igu address will hold the absolute address that needs to be
	 * written to for a specific status block
	 */
	if (IS_PF(p_hwfn->p_dev)) {
		sb_info->igu_addr = (u8 OSAL_IOMEM *)p_hwfn->regview +
		    GTT_BAR0_MAP_REG_IGU_CMD + (sb_info->igu_sb_id << 3);

	} else {
		sb_info->igu_addr =
		    (u8 OSAL_IOMEM *)p_hwfn->regview +
		    PXP_VF_BAR0_START_IGU +
		    ((IGU_CMD_INT_ACK_BASE + sb_info->igu_sb_id) << 3);
	}

	sb_info->flags |= ECORE_SB_INFO_INIT;

	ecore_int_sb_setup(p_hwfn, p_ptt, sb_info);

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_int_sb_release(struct ecore_hwfn *p_hwfn,
					  struct ecore_sb_info *sb_info,
					  u16 sb_id)
{
	if (sb_id == ECORE_SP_SB_ID) {
		DP_ERR(p_hwfn, "Do Not free sp sb using this function");
		return ECORE_INVAL;
	}

	/* zero status block and ack counter */
	sb_info->sb_ack = 0;
	OSAL_MEMSET(sb_info->sb_virt, 0, sizeof(*sb_info->sb_virt));

	if (p_hwfn->sbs_info[sb_id] != OSAL_NULL) {
		p_hwfn->sbs_info[sb_id] = OSAL_NULL;
		p_hwfn->num_sbs--;
	}

	return ECORE_SUCCESS;
}

static void ecore_int_sp_sb_free(struct ecore_hwfn *p_hwfn)
{
	struct ecore_sb_sp_info *p_sb = p_hwfn->p_sp_sb;

	if (!p_sb)
		return;

	if (p_sb->sb_info.sb_virt) {
		OSAL_DMA_FREE_COHERENT(p_hwfn->p_dev,
				       p_sb->sb_info.sb_virt,
				       p_sb->sb_info.sb_phys,
				       SB_ALIGNED_SIZE(p_hwfn));
	}

	OSAL_FREE(p_hwfn->p_dev, p_sb);
}

static enum _ecore_status_t ecore_int_sp_sb_alloc(struct ecore_hwfn *p_hwfn,
						  struct ecore_ptt *p_ptt)
{
	struct ecore_sb_sp_info *p_sb;
	dma_addr_t p_phys = 0;
	void *p_virt;

	/* SB struct */
	p_sb =
	    OSAL_ALLOC(p_hwfn->p_dev, GFP_KERNEL,
		       sizeof(struct ecore_sb_sp_info));
	if (!p_sb) {
		DP_NOTICE(p_hwfn, true,
			  "Failed to allocate `struct ecore_sb_info'");
		return ECORE_NOMEM;
	}

	/* SB ring  */
	p_virt = OSAL_DMA_ALLOC_COHERENT(p_hwfn->p_dev,
					 &p_phys, SB_ALIGNED_SIZE(p_hwfn));
	if (!p_virt) {
		DP_NOTICE(p_hwfn, true, "Failed to allocate status block");
		OSAL_FREE(p_hwfn->p_dev, p_sb);
		return ECORE_NOMEM;
	}

	/* Status Block setup */
	p_hwfn->p_sp_sb = p_sb;
	ecore_int_sb_init(p_hwfn, p_ptt, &p_sb->sb_info,
			  p_virt, p_phys, ECORE_SP_SB_ID);

	OSAL_MEMSET(p_sb->pi_info_arr, 0, sizeof(p_sb->pi_info_arr));

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_int_register_cb(struct ecore_hwfn *p_hwfn,
					   ecore_int_comp_cb_t comp_cb,
					   void *cookie,
					   u8 *sb_idx, __le16 **p_fw_cons)
{
	struct ecore_sb_sp_info *p_sp_sb = p_hwfn->p_sp_sb;
	enum _ecore_status_t rc = ECORE_NOMEM;
	u8 pi;

	/* Look for a free index */
	for (pi = 0; pi < OSAL_ARRAY_SIZE(p_sp_sb->pi_info_arr); pi++) {
		if (p_sp_sb->pi_info_arr[pi].comp_cb != OSAL_NULL)
			continue;

		p_sp_sb->pi_info_arr[pi].comp_cb = comp_cb;
		p_sp_sb->pi_info_arr[pi].cookie = cookie;
		*sb_idx = pi;
		*p_fw_cons = &p_sp_sb->sb_info.sb_virt->pi_array[pi];
		rc = ECORE_SUCCESS;
		break;
	}

	return rc;
}

enum _ecore_status_t ecore_int_unregister_cb(struct ecore_hwfn *p_hwfn, u8 pi)
{
	struct ecore_sb_sp_info *p_sp_sb = p_hwfn->p_sp_sb;

	if (p_sp_sb->pi_info_arr[pi].comp_cb == OSAL_NULL)
		return ECORE_NOMEM;

	p_sp_sb->pi_info_arr[pi].comp_cb = OSAL_NULL;
	p_sp_sb->pi_info_arr[pi].cookie = OSAL_NULL;
	return ECORE_SUCCESS;
}

u16 ecore_int_get_sp_sb_id(struct ecore_hwfn *p_hwfn)
{
	return p_hwfn->p_sp_sb->sb_info.igu_sb_id;
}

void ecore_int_igu_enable_int(struct ecore_hwfn *p_hwfn,
			      struct ecore_ptt *p_ptt,
			      enum ecore_int_mode int_mode)
{
	u32 igu_pf_conf = IGU_PF_CONF_FUNC_EN;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_FPGA(p_hwfn->p_dev))
		DP_INFO(p_hwfn, "FPGA - don't enable ATTN generation in IGU\n");
	else
#endif
		igu_pf_conf |= IGU_PF_CONF_ATTN_BIT_EN;

	p_hwfn->p_dev->int_mode = int_mode;
	switch (p_hwfn->p_dev->int_mode) {
	case ECORE_INT_MODE_INTA:
		igu_pf_conf |= IGU_PF_CONF_INT_LINE_EN;
		igu_pf_conf |= IGU_PF_CONF_SINGLE_ISR_EN;
		break;

	case ECORE_INT_MODE_MSI:
		igu_pf_conf |= IGU_PF_CONF_MSI_MSIX_EN;
		igu_pf_conf |= IGU_PF_CONF_SINGLE_ISR_EN;
		break;

	case ECORE_INT_MODE_MSIX:
		igu_pf_conf |= IGU_PF_CONF_MSI_MSIX_EN;
		break;
	case ECORE_INT_MODE_POLL:
		break;
	}

	ecore_wr(p_hwfn, p_ptt, IGU_REG_PF_CONFIGURATION, igu_pf_conf);
}

static void ecore_int_igu_enable_attn(struct ecore_hwfn *p_hwfn,
				      struct ecore_ptt *p_ptt)
{
#ifndef ASIC_ONLY
	if (CHIP_REV_IS_FPGA(p_hwfn->p_dev)) {
		DP_INFO(p_hwfn,
			"FPGA - Don't enable Attentions in IGU and MISC\n");
		return;
	}
#endif

	/* Configure AEU signal change to produce attentions */
	ecore_wr(p_hwfn, p_ptt, IGU_REG_ATTENTION_ENABLE, 0);
	ecore_wr(p_hwfn, p_ptt, IGU_REG_LEADING_EDGE_LATCH, 0xfff);
	ecore_wr(p_hwfn, p_ptt, IGU_REG_TRAILING_EDGE_LATCH, 0xfff);
	ecore_wr(p_hwfn, p_ptt, IGU_REG_ATTENTION_ENABLE, 0xfff);

	OSAL_MMIOWB(p_hwfn->p_dev);

	/* Unmask AEU signals toward IGU */
	ecore_wr(p_hwfn, p_ptt, MISC_REG_AEU_MASK_ATTN_IGU, 0xff);
}

enum _ecore_status_t
ecore_int_igu_enable(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt,
		     enum ecore_int_mode int_mode)
{
	enum _ecore_status_t rc = ECORE_SUCCESS;
	u32 tmp, reg_addr;

	/* @@@tmp - Mask General HW attentions 0-31, Enable 32-36 */
	tmp = ecore_rd(p_hwfn, p_ptt, MISC_REG_AEU_ENABLE4_IGU_OUT_0);
	tmp |= 0xf;
	ecore_wr(p_hwfn, p_ptt, MISC_REG_AEU_ENABLE3_IGU_OUT_0, 0);
	ecore_wr(p_hwfn, p_ptt, MISC_REG_AEU_ENABLE4_IGU_OUT_0, tmp);

	/* @@@tmp - Starting with MFW 8.2.1.0 we've started hitting AVS stop
	 * attentions. Since we're waiting for BRCM answer regarding this
	 * attention, in the meanwhile we simply mask it.
	 */
	tmp = ecore_rd(p_hwfn, p_ptt, MISC_REG_AEU_ENABLE4_IGU_OUT_0);
	tmp &= ~0x800;
	ecore_wr(p_hwfn, p_ptt, MISC_REG_AEU_ENABLE4_IGU_OUT_0, tmp);

	ecore_int_igu_enable_attn(p_hwfn, p_ptt);

	if ((int_mode != ECORE_INT_MODE_INTA) || IS_LEAD_HWFN(p_hwfn)) {
		rc = OSAL_SLOWPATH_IRQ_REQ(p_hwfn);
		if (rc != ECORE_SUCCESS) {
			DP_NOTICE(p_hwfn, true,
				  "Slowpath IRQ request failed\n");
			return ECORE_NORESOURCES;
		}
		p_hwfn->b_int_requested = true;
	}

	/* Enable interrupt Generation */
	ecore_int_igu_enable_int(p_hwfn, p_ptt, int_mode);

	p_hwfn->b_int_enabled = 1;

	return rc;
}

void ecore_int_igu_disable_int(struct ecore_hwfn *p_hwfn,
			       struct ecore_ptt *p_ptt)
{
	p_hwfn->b_int_enabled = 0;

	if (IS_VF(p_hwfn->p_dev))
		return;

	ecore_wr(p_hwfn, p_ptt, IGU_REG_PF_CONFIGURATION, 0);
}

#define IGU_CLEANUP_SLEEP_LENGTH		(1000)
void ecore_int_igu_cleanup_sb(struct ecore_hwfn *p_hwfn,
			      struct ecore_ptt *p_ptt,
			      u32 sb_id, bool cleanup_set, u16 opaque_fid)
{
	u32 cmd_ctrl = 0, val = 0, sb_bit = 0, sb_bit_addr = 0, data = 0;
	u32 pxp_addr = IGU_CMD_INT_ACK_BASE + sb_id;
	u32 sleep_cnt = IGU_CLEANUP_SLEEP_LENGTH;
	u8 type = 0;		/* FIXME MichalS type??? */

	OSAL_BUILD_BUG_ON((IGU_REG_CLEANUP_STATUS_4 -
			   IGU_REG_CLEANUP_STATUS_0) != 0x200);

	/* USE Control Command Register to perform cleanup. There is an
	 * option to do this using IGU bar, but then it can't be used for VFs.
	 */

	/* Set the data field */
	SET_FIELD(data, IGU_CLEANUP_CLEANUP_SET, cleanup_set ? 1 : 0);
	SET_FIELD(data, IGU_CLEANUP_CLEANUP_TYPE, type);
	SET_FIELD(data, IGU_CLEANUP_COMMAND_TYPE, IGU_COMMAND_TYPE_SET);

	/* Set the control register */
	SET_FIELD(cmd_ctrl, IGU_CTRL_REG_PXP_ADDR, pxp_addr);
	SET_FIELD(cmd_ctrl, IGU_CTRL_REG_FID, opaque_fid);
	SET_FIELD(cmd_ctrl, IGU_CTRL_REG_TYPE, IGU_CTRL_CMD_TYPE_WR);

	ecore_wr(p_hwfn, p_ptt, IGU_REG_COMMAND_REG_32LSB_DATA, data);

	OSAL_BARRIER(p_hwfn->p_dev);

	ecore_wr(p_hwfn, p_ptt, IGU_REG_COMMAND_REG_CTRL, cmd_ctrl);

	OSAL_MMIOWB(p_hwfn->p_dev);

	/* calculate where to read the status bit from */
	sb_bit = 1 << (sb_id % 32);
	sb_bit_addr = sb_id / 32 * sizeof(u32);

	sb_bit_addr += IGU_REG_CLEANUP_STATUS_0 + (0x80 * type);

	/* Now wait for the command to complete */
	while (--sleep_cnt) {
		val = ecore_rd(p_hwfn, p_ptt, sb_bit_addr);
		if ((val & sb_bit) == (cleanup_set ? sb_bit : 0))
			break;
		OSAL_MSLEEP(5);
	}

	if (!sleep_cnt)
		DP_NOTICE(p_hwfn, true,
			  "Timeout waiting for clear status 0x%08x [for sb %d]\n",
			  val, sb_id);
}

void ecore_int_igu_init_pure_rt_single(struct ecore_hwfn *p_hwfn,
				       struct ecore_ptt *p_ptt,
				       u32 sb_id, u16 opaque, bool b_set)
{
	int pi;

	/* Set */
	if (b_set)
		ecore_int_igu_cleanup_sb(p_hwfn, p_ptt, sb_id, 1, opaque);

	/* Clear */
	ecore_int_igu_cleanup_sb(p_hwfn, p_ptt, sb_id, 0, opaque);

	/* Clear the CAU for the SB */
	for (pi = 0; pi < 12; pi++)
		ecore_wr(p_hwfn, p_ptt,
			 CAU_REG_PI_MEMORY + (sb_id * 12 + pi) * 4, 0);
}

void ecore_int_igu_init_pure_rt(struct ecore_hwfn *p_hwfn,
				struct ecore_ptt *p_ptt,
				bool b_set, bool b_slowpath)
{
	u32 igu_base_sb = p_hwfn->hw_info.p_igu_info->igu_base_sb;
	u32 igu_sb_cnt = p_hwfn->hw_info.p_igu_info->igu_sb_cnt;
	u32 sb_id = 0, val = 0;

	/* @@@TBD MichalK temporary... should be moved to init-tool... */
	val = ecore_rd(p_hwfn, p_ptt, IGU_REG_BLOCK_CONFIGURATION);
	val |= IGU_REG_BLOCK_CONFIGURATION_VF_CLEANUP_EN;
	val &= ~IGU_REG_BLOCK_CONFIGURATION_PXP_TPH_INTERFACE_EN;
	ecore_wr(p_hwfn, p_ptt, IGU_REG_BLOCK_CONFIGURATION, val);
	/* end temporary */

	DP_VERBOSE(p_hwfn, ECORE_MSG_INTR,
		   "IGU cleaning SBs [%d,...,%d]\n",
		   igu_base_sb, igu_base_sb + igu_sb_cnt - 1);

	for (sb_id = igu_base_sb; sb_id < igu_base_sb + igu_sb_cnt; sb_id++)
		ecore_int_igu_init_pure_rt_single(p_hwfn, p_ptt, sb_id,
						  p_hwfn->hw_info.opaque_fid,
						  b_set);

	if (!b_slowpath)
		return;

	sb_id = p_hwfn->hw_info.p_igu_info->igu_dsb_id;
	DP_VERBOSE(p_hwfn, ECORE_MSG_INTR,
		   "IGU cleaning slowpath SB [%d]\n", sb_id);
	ecore_int_igu_init_pure_rt_single(p_hwfn, p_ptt, sb_id,
					  p_hwfn->hw_info.opaque_fid, b_set);
}

static u32 ecore_int_igu_read_cam_block(struct ecore_hwfn *p_hwfn,
					struct ecore_ptt *p_ptt, u16 sb_id)
{
	u32 val = ecore_rd(p_hwfn, p_ptt,
			   IGU_REG_MAPPING_MEMORY + sizeof(u32) * sb_id);
	struct ecore_igu_block *p_block;

	p_block = &p_hwfn->hw_info.p_igu_info->igu_map.igu_blocks[sb_id];

	/* stop scanning when hit first invalid PF entry */
	if (!GET_FIELD(val, IGU_MAPPING_LINE_VALID) &&
	    GET_FIELD(val, IGU_MAPPING_LINE_PF_VALID))
		goto out;

	/* Fill the block information */
	p_block->status = ECORE_IGU_STATUS_VALID;
	p_block->function_id = GET_FIELD(val, IGU_MAPPING_LINE_FUNCTION_NUMBER);
	p_block->is_pf = GET_FIELD(val, IGU_MAPPING_LINE_PF_VALID);
	p_block->vector_number = GET_FIELD(val, IGU_MAPPING_LINE_VECTOR_NUMBER);

	DP_VERBOSE(p_hwfn, ECORE_MSG_INTR,
		   "IGU_BLOCK: [SB 0x%04x, Value in CAM 0x%08x] func_id = %d"
		   " is_pf = %d vector_num = 0x%x\n",
		   sb_id, val, p_block->function_id, p_block->is_pf,
		   p_block->vector_number);

out:
	return val;
}

enum _ecore_status_t ecore_int_igu_read_cam(struct ecore_hwfn *p_hwfn,
					    struct ecore_ptt *p_ptt)
{
	struct ecore_igu_info *p_igu_info;
	struct ecore_igu_block *p_block;
	u16 sb_id, last_iov_sb_id = 0;
	u32 min_vf, max_vf, val;
	u16 prev_sb_id = 0xFF;

	p_hwfn->hw_info.p_igu_info = OSAL_ALLOC(p_hwfn->p_dev,
						GFP_KERNEL,
						sizeof(*p_igu_info));
	if (!p_hwfn->hw_info.p_igu_info)
		return ECORE_NOMEM;

	OSAL_MEMSET(p_hwfn->hw_info.p_igu_info, 0, sizeof(*p_igu_info));

	p_igu_info = p_hwfn->hw_info.p_igu_info;

	/* Initialize base sb / sb cnt for PFs and VFs */
	p_igu_info->igu_base_sb = 0xffff;
	p_igu_info->igu_sb_cnt = 0;
	p_igu_info->igu_dsb_id = 0xffff;
	p_igu_info->igu_base_sb_iov = 0xffff;

#ifdef CONFIG_ECORE_SRIOV
	min_vf = p_hwfn->hw_info.first_vf_in_pf;
	max_vf = p_hwfn->hw_info.first_vf_in_pf +
	    p_hwfn->p_dev->sriov_info.total_vfs;
#else
	min_vf = 0;
	max_vf = 0;
#endif

	for (sb_id = 0; sb_id < ECORE_MAPPING_MEMORY_SIZE(p_hwfn->p_dev);
	     sb_id++) {
		p_block = &p_igu_info->igu_map.igu_blocks[sb_id];
		val = ecore_int_igu_read_cam_block(p_hwfn, p_ptt, sb_id);
		if (!GET_FIELD(val, IGU_MAPPING_LINE_VALID) &&
		    GET_FIELD(val, IGU_MAPPING_LINE_PF_VALID))
			break;

		if (p_block->is_pf) {
			if (p_block->function_id == p_hwfn->rel_pf_id) {
				p_block->status |= ECORE_IGU_STATUS_PF;

				if (p_block->vector_number == 0) {
					if (p_igu_info->igu_dsb_id == 0xffff)
						p_igu_info->igu_dsb_id = sb_id;
				} else {
					if (p_igu_info->igu_base_sb == 0xffff) {
						p_igu_info->igu_base_sb = sb_id;
					} else if (prev_sb_id != sb_id - 1) {
						DP_NOTICE(p_hwfn->p_dev, false,
							  "consecutive igu"
							  " vectors for HWFN"
							  " %x broken",
							  p_hwfn->rel_pf_id);
						break;
					}
					prev_sb_id = sb_id;
					/* we don't count the default */
					(p_igu_info->igu_sb_cnt)++;
				}
			}
		} else {
			if ((p_block->function_id >= min_vf) &&
			    (p_block->function_id < max_vf)) {
				/* Available for VFs of this PF */
				if (p_igu_info->igu_base_sb_iov == 0xffff) {
					p_igu_info->igu_base_sb_iov = sb_id;
				} else if (last_iov_sb_id != sb_id - 1) {
					if (!val)
						DP_VERBOSE(p_hwfn->p_dev,
							   ECORE_MSG_INTR,
							   "First uninited IGU"
							   " CAM entry at"
							   " index 0x%04x\n",
							   sb_id);
					else
						DP_NOTICE(p_hwfn->p_dev, false,
							  "Consecutive igu"
							  " vectors for HWFN"
							  " %x vfs is broken"
							  " [jumps from %04x"
							  " to %04x]\n",
							  p_hwfn->rel_pf_id,
							  last_iov_sb_id,
							  sb_id);
					break;
				}
				p_block->status |= ECORE_IGU_STATUS_FREE;
				p_hwfn->hw_info.p_igu_info->free_blks++;
				last_iov_sb_id = sb_id;
			}
		}
	}
	p_igu_info->igu_sb_cnt_iov = p_igu_info->free_blks;

	DP_VERBOSE(p_hwfn, ECORE_MSG_INTR,
		   "IGU igu_base_sb=0x%x [IOV 0x%x] igu_sb_cnt=%d [IOV 0x%x] "
		   "igu_dsb_id=0x%x\n",
		   p_igu_info->igu_base_sb, p_igu_info->igu_base_sb_iov,
		   p_igu_info->igu_sb_cnt, p_igu_info->igu_sb_cnt_iov,
		   p_igu_info->igu_dsb_id);

	if (p_igu_info->igu_base_sb == 0xffff ||
	    p_igu_info->igu_dsb_id == 0xffff || p_igu_info->igu_sb_cnt == 0) {
		DP_NOTICE(p_hwfn, true,
			  "IGU CAM returned invalid values igu_base_sb=0x%x "
			  "igu_sb_cnt=%d igu_dsb_id=0x%x\n",
			  p_igu_info->igu_base_sb, p_igu_info->igu_sb_cnt,
			  p_igu_info->igu_dsb_id);
		return ECORE_INVAL;
	}

	return ECORE_SUCCESS;
}

/**
 * @brief Initialize igu runtime registers
 *
 * @param p_hwfn
 */
void ecore_int_igu_init_rt(struct ecore_hwfn *p_hwfn)
{
	u32 igu_pf_conf = IGU_PF_CONF_FUNC_EN;

	STORE_RT_REG(p_hwfn, IGU_REG_PF_CONFIGURATION_RT_OFFSET, igu_pf_conf);
}

#define LSB_IGU_CMD_ADDR (IGU_REG_SISR_MDPC_WMASK_LSB_UPPER - \
			  IGU_CMD_INT_ACK_BASE)
#define MSB_IGU_CMD_ADDR (IGU_REG_SISR_MDPC_WMASK_MSB_UPPER - \
			  IGU_CMD_INT_ACK_BASE)
u64 ecore_int_igu_read_sisr_reg(struct ecore_hwfn *p_hwfn)
{
	u32 intr_status_hi = 0, intr_status_lo = 0;
	u64 intr_status = 0;

	intr_status_lo = REG_RD(p_hwfn,
				GTT_BAR0_MAP_REG_IGU_CMD +
				LSB_IGU_CMD_ADDR * 8);
	intr_status_hi = REG_RD(p_hwfn,
				GTT_BAR0_MAP_REG_IGU_CMD +
				MSB_IGU_CMD_ADDR * 8);
	intr_status = ((u64)intr_status_hi << 32) + (u64)intr_status_lo;

	return intr_status;
}

static void ecore_int_sp_dpc_setup(struct ecore_hwfn *p_hwfn)
{
	OSAL_DPC_INIT(p_hwfn->sp_dpc, p_hwfn);
	p_hwfn->b_sp_dpc_enabled = true;
}

static enum _ecore_status_t ecore_int_sp_dpc_alloc(struct ecore_hwfn *p_hwfn)
{
	p_hwfn->sp_dpc = OSAL_DPC_ALLOC(p_hwfn);
	if (!p_hwfn->sp_dpc)
		return ECORE_NOMEM;

	return ECORE_SUCCESS;
}

static void ecore_int_sp_dpc_free(struct ecore_hwfn *p_hwfn)
{
	OSAL_FREE(p_hwfn->p_dev, p_hwfn->sp_dpc);
}

enum _ecore_status_t ecore_int_alloc(struct ecore_hwfn *p_hwfn,
				     struct ecore_ptt *p_ptt)
{
	enum _ecore_status_t rc = ECORE_SUCCESS;

	rc = ecore_int_sp_dpc_alloc(p_hwfn);
	if (rc != ECORE_SUCCESS) {
		DP_ERR(p_hwfn->p_dev, "Failed to allocate sp dpc mem\n");
		return rc;
	}

	rc = ecore_int_sp_sb_alloc(p_hwfn, p_ptt);
	if (rc != ECORE_SUCCESS) {
		DP_ERR(p_hwfn->p_dev, "Failed to allocate sp sb mem\n");
		return rc;
	}

	return rc;
}

void ecore_int_free(struct ecore_hwfn *p_hwfn)
{
	ecore_int_sp_sb_free(p_hwfn);
	ecore_int_sb_attn_free(p_hwfn);
	ecore_int_sp_dpc_free(p_hwfn);
}

void ecore_int_setup(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt)
{
	if (!p_hwfn || !p_hwfn->p_sp_sb || !p_hwfn->p_sb_attn)
		return;

	ecore_int_sb_setup(p_hwfn, p_ptt, &p_hwfn->p_sp_sb->sb_info);
	ecore_int_sp_dpc_setup(p_hwfn);
}

void ecore_int_get_num_sbs(struct ecore_hwfn *p_hwfn,
			   struct ecore_sb_cnt_info *p_sb_cnt_info)
{
	struct ecore_igu_info *info = p_hwfn->hw_info.p_igu_info;

	if (!info || !p_sb_cnt_info)
		return;

	p_sb_cnt_info->sb_cnt = info->igu_sb_cnt;
	p_sb_cnt_info->sb_iov_cnt = info->igu_sb_cnt_iov;
	p_sb_cnt_info->sb_free_blk = info->free_blks;
}

u16 ecore_int_queue_id_from_sb_id(struct ecore_hwfn *p_hwfn, u16 sb_id)
{
	struct ecore_igu_info *p_info = p_hwfn->hw_info.p_igu_info;

	/* Determine origin of SB id */
	if ((sb_id >= p_info->igu_base_sb) &&
	    (sb_id < p_info->igu_base_sb + p_info->igu_sb_cnt)) {
		return sb_id - p_info->igu_base_sb;
	} else if ((sb_id >= p_info->igu_base_sb_iov) &&
		   (sb_id < p_info->igu_base_sb_iov + p_info->igu_sb_cnt_iov)) {
		return sb_id - p_info->igu_base_sb_iov + p_info->igu_sb_cnt;
	}

	DP_NOTICE(p_hwfn, true, "SB %d not in range for function\n",
		  sb_id);
	return 0;
}

void ecore_int_disable_post_isr_release(struct ecore_dev *p_dev)
{
	int i;

	for_each_hwfn(p_dev, i)
		p_dev->hwfns[i].b_int_requested = false;
}
