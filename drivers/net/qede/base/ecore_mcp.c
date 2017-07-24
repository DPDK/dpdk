/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#include "bcm_osal.h"
#include "ecore.h"
#include "ecore_status.h"
#include "ecore_mcp.h"
#include "mcp_public.h"
#include "reg_addr.h"
#include "ecore_hw.h"
#include "ecore_init_fw_funcs.h"
#include "ecore_sriov.h"
#include "ecore_vf.h"
#include "ecore_iov_api.h"
#include "ecore_gtt_reg_addr.h"
#include "ecore_iro.h"
#include "ecore_dcbx.h"

#define CHIP_MCP_RESP_ITER_US 10
#define EMUL_MCP_RESP_ITER_US (1000 * 1000)

#define ECORE_DRV_MB_MAX_RETRIES (500 * 1000)	/* Account for 5 sec */
#define ECORE_MCP_RESET_RETRIES (50 * 1000)	/* Account for 500 msec */

#define DRV_INNER_WR(_p_hwfn, _p_ptt, _ptr, _offset, _val) \
	ecore_wr(_p_hwfn, _p_ptt, (_p_hwfn->mcp_info->_ptr + _offset), \
		 _val)

#define DRV_INNER_RD(_p_hwfn, _p_ptt, _ptr, _offset) \
	ecore_rd(_p_hwfn, _p_ptt, (_p_hwfn->mcp_info->_ptr + _offset))

#define DRV_MB_WR(_p_hwfn, _p_ptt, _field, _val) \
	DRV_INNER_WR(p_hwfn, _p_ptt, drv_mb_addr, \
		     OFFSETOF(struct public_drv_mb, _field), _val)

#define DRV_MB_RD(_p_hwfn, _p_ptt, _field) \
	DRV_INNER_RD(_p_hwfn, _p_ptt, drv_mb_addr, \
		     OFFSETOF(struct public_drv_mb, _field))

#define PDA_COMP (((FW_MAJOR_VERSION) + (FW_MINOR_VERSION << 8)) << \
	DRV_ID_PDA_COMP_VER_SHIFT)

#define MCP_BYTES_PER_MBIT_SHIFT 17

#ifndef ASIC_ONLY
static int loaded;
static int loaded_port[MAX_NUM_PORTS] = { 0 };
#endif

bool ecore_mcp_is_init(struct ecore_hwfn *p_hwfn)
{
	if (!p_hwfn->mcp_info || !p_hwfn->mcp_info->public_base)
		return false;
	return true;
}

void ecore_mcp_cmd_port_init(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt)
{
	u32 addr = SECTION_OFFSIZE_ADDR(p_hwfn->mcp_info->public_base,
					PUBLIC_PORT);
	u32 mfw_mb_offsize = ecore_rd(p_hwfn, p_ptt, addr);

	p_hwfn->mcp_info->port_addr = SECTION_ADDR(mfw_mb_offsize,
						   MFW_PORT(p_hwfn));
	DP_VERBOSE(p_hwfn, ECORE_MSG_SP,
		   "port_addr = 0x%x, port_id 0x%02x\n",
		   p_hwfn->mcp_info->port_addr, MFW_PORT(p_hwfn));
}

void ecore_mcp_read_mb(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt)
{
	u32 length = MFW_DRV_MSG_MAX_DWORDS(p_hwfn->mcp_info->mfw_mb_length);
	OSAL_BE32 tmp;
	u32 i;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_TEDIBEAR(p_hwfn->p_dev))
		return;
#endif

	if (!p_hwfn->mcp_info->public_base)
		return;

	for (i = 0; i < length; i++) {
		tmp = ecore_rd(p_hwfn, p_ptt,
			       p_hwfn->mcp_info->mfw_mb_addr +
			       (i << 2) + sizeof(u32));

		((u32 *)p_hwfn->mcp_info->mfw_mb_cur)[i] =
		    OSAL_BE32_TO_CPU(tmp);
	}
}

enum _ecore_status_t ecore_mcp_free(struct ecore_hwfn *p_hwfn)
{
	if (p_hwfn->mcp_info) {
		OSAL_FREE(p_hwfn->p_dev, p_hwfn->mcp_info->mfw_mb_cur);
		OSAL_FREE(p_hwfn->p_dev, p_hwfn->mcp_info->mfw_mb_shadow);
		OSAL_SPIN_LOCK_DEALLOC(&p_hwfn->mcp_info->lock);
	}
	OSAL_FREE(p_hwfn->p_dev, p_hwfn->mcp_info);

	return ECORE_SUCCESS;
}

static enum _ecore_status_t ecore_load_mcp_offsets(struct ecore_hwfn *p_hwfn,
						   struct ecore_ptt *p_ptt)
{
	struct ecore_mcp_info *p_info = p_hwfn->mcp_info;
	u32 drv_mb_offsize, mfw_mb_offsize;
	u32 mcp_pf_id = MCP_PF_ID(p_hwfn);

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->p_dev)) {
		DP_NOTICE(p_hwfn, false, "Emulation - assume no MFW\n");
		p_info->public_base = 0;
		return ECORE_INVAL;
	}
#endif

	p_info->public_base = ecore_rd(p_hwfn, p_ptt, MISC_REG_SHARED_MEM_ADDR);
	if (!p_info->public_base)
		return ECORE_INVAL;

	p_info->public_base |= GRCBASE_MCP;

	/* Calculate the driver and MFW mailbox address */
	drv_mb_offsize = ecore_rd(p_hwfn, p_ptt,
				  SECTION_OFFSIZE_ADDR(p_info->public_base,
						       PUBLIC_DRV_MB));
	p_info->drv_mb_addr = SECTION_ADDR(drv_mb_offsize, mcp_pf_id);
	DP_VERBOSE(p_hwfn, ECORE_MSG_SP,
		   "drv_mb_offsiz = 0x%x, drv_mb_addr = 0x%x"
		   " mcp_pf_id = 0x%x\n",
		   drv_mb_offsize, p_info->drv_mb_addr, mcp_pf_id);

	/* Set the MFW MB address */
	mfw_mb_offsize = ecore_rd(p_hwfn, p_ptt,
				  SECTION_OFFSIZE_ADDR(p_info->public_base,
						       PUBLIC_MFW_MB));
	p_info->mfw_mb_addr = SECTION_ADDR(mfw_mb_offsize, mcp_pf_id);
	p_info->mfw_mb_length = (u16)ecore_rd(p_hwfn, p_ptt,
					       p_info->mfw_mb_addr);

	/* Get the current driver mailbox sequence before sending
	 * the first command
	 */
	p_info->drv_mb_seq = DRV_MB_RD(p_hwfn, p_ptt, drv_mb_header) &
	    DRV_MSG_SEQ_NUMBER_MASK;

	/* Get current FW pulse sequence */
	p_info->drv_pulse_seq = DRV_MB_RD(p_hwfn, p_ptt, drv_pulse_mb) &
	    DRV_PULSE_SEQ_MASK;

	p_info->mcp_hist = (u16)ecore_rd(p_hwfn, p_ptt,
					  MISCS_REG_GENERIC_POR_0);

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_mcp_cmd_init(struct ecore_hwfn *p_hwfn,
					struct ecore_ptt *p_ptt)
{
	struct ecore_mcp_info *p_info;
	u32 size;

	/* Allocate mcp_info structure */
	p_hwfn->mcp_info = OSAL_ZALLOC(p_hwfn->p_dev, GFP_KERNEL,
				       sizeof(*p_hwfn->mcp_info));
	if (!p_hwfn->mcp_info)
		goto err;
	p_info = p_hwfn->mcp_info;

	if (ecore_load_mcp_offsets(p_hwfn, p_ptt) != ECORE_SUCCESS) {
		DP_NOTICE(p_hwfn, false, "MCP is not initialized\n");
		/* Do not free mcp_info here, since public_base indicate that
		 * the MCP is not initialized
		 */
		return ECORE_SUCCESS;
	}

	size = MFW_DRV_MSG_MAX_DWORDS(p_info->mfw_mb_length) * sizeof(u32);
	p_info->mfw_mb_cur = OSAL_ZALLOC(p_hwfn->p_dev, GFP_KERNEL, size);
	p_info->mfw_mb_shadow = OSAL_ZALLOC(p_hwfn->p_dev, GFP_KERNEL, size);
	if (!p_info->mfw_mb_shadow || !p_info->mfw_mb_addr)
		goto err;

	/* Initialize the MFW spinlock */
	OSAL_SPIN_LOCK_ALLOC(p_hwfn, &p_info->lock);
	OSAL_SPIN_LOCK_INIT(&p_info->lock);

	return ECORE_SUCCESS;

err:
	DP_NOTICE(p_hwfn, true, "Failed to allocate mcp memory\n");
	ecore_mcp_free(p_hwfn);
	return ECORE_NOMEM;
}

/* Locks the MFW mailbox of a PF to ensure a single access.
 * The lock is achieved in most cases by holding a spinlock, causing other
 * threads to wait till a previous access is done.
 * In some cases (currently when a [UN]LOAD_REQ commands are sent), the single
 * access is achieved by setting a blocking flag, which will fail other
 * competing contexts to send their mailboxes.
 */
static enum _ecore_status_t ecore_mcp_mb_lock(struct ecore_hwfn *p_hwfn,
					      u32 cmd)
{
	OSAL_SPIN_LOCK(&p_hwfn->mcp_info->lock);

	/* The spinlock shouldn't be acquired when the mailbox command is
	 * [UN]LOAD_REQ, since the engine is locked by the MFW, and a parallel
	 * pending [UN]LOAD_REQ command of another PF together with a spinlock
	 * (i.e. interrupts are disabled) - can lead to a deadlock.
	 * It is assumed that for a single PF, no other mailbox commands can be
	 * sent from another context while sending LOAD_REQ, and that any
	 * parallel commands to UNLOAD_REQ can be cancelled.
	 */
	if (cmd == DRV_MSG_CODE_LOAD_DONE || cmd == DRV_MSG_CODE_UNLOAD_DONE)
		p_hwfn->mcp_info->block_mb_sending = false;

	if (p_hwfn->mcp_info->block_mb_sending) {
		DP_NOTICE(p_hwfn, false,
			  "Trying to send a MFW mailbox command [0x%x]"
			  " in parallel to [UN]LOAD_REQ. Aborting.\n",
			  cmd);
		OSAL_SPIN_UNLOCK(&p_hwfn->mcp_info->lock);
		return ECORE_BUSY;
	}

	if (cmd == DRV_MSG_CODE_LOAD_REQ || cmd == DRV_MSG_CODE_UNLOAD_REQ) {
		p_hwfn->mcp_info->block_mb_sending = true;
		OSAL_SPIN_UNLOCK(&p_hwfn->mcp_info->lock);
	}

	return ECORE_SUCCESS;
}

static void ecore_mcp_mb_unlock(struct ecore_hwfn *p_hwfn, u32 cmd)
{
	if (cmd != DRV_MSG_CODE_LOAD_REQ && cmd != DRV_MSG_CODE_UNLOAD_REQ)
		OSAL_SPIN_UNLOCK(&p_hwfn->mcp_info->lock);
}

enum _ecore_status_t ecore_mcp_reset(struct ecore_hwfn *p_hwfn,
				     struct ecore_ptt *p_ptt)
{
	u32 seq = ++p_hwfn->mcp_info->drv_mb_seq;
	u32 delay = CHIP_MCP_RESP_ITER_US;
	u32 org_mcp_reset_seq, cnt = 0;
	enum _ecore_status_t rc = ECORE_SUCCESS;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->p_dev))
		delay = EMUL_MCP_RESP_ITER_US;
#endif

	/* Ensure that only a single thread is accessing the mailbox at a
	 * certain time.
	 */
	rc = ecore_mcp_mb_lock(p_hwfn, DRV_MSG_CODE_MCP_RESET);
	if (rc != ECORE_SUCCESS)
		return rc;

	/* Set drv command along with the updated sequence */
	org_mcp_reset_seq = ecore_rd(p_hwfn, p_ptt, MISCS_REG_GENERIC_POR_0);
	DRV_MB_WR(p_hwfn, p_ptt, drv_mb_header, (DRV_MSG_CODE_MCP_RESET | seq));

	do {
		/* Wait for MFW response */
		OSAL_UDELAY(delay);
		/* Give the FW up to 500 second (50*1000*10usec) */
	} while ((org_mcp_reset_seq == ecore_rd(p_hwfn, p_ptt,
						MISCS_REG_GENERIC_POR_0)) &&
		 (cnt++ < ECORE_MCP_RESET_RETRIES));

	if (org_mcp_reset_seq !=
	    ecore_rd(p_hwfn, p_ptt, MISCS_REG_GENERIC_POR_0)) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_SP,
			   "MCP was reset after %d usec\n", cnt * delay);
	} else {
		DP_ERR(p_hwfn, "Failed to reset MCP\n");
		rc = ECORE_AGAIN;
	}

	ecore_mcp_mb_unlock(p_hwfn, DRV_MSG_CODE_MCP_RESET);

	return rc;
}

static enum _ecore_status_t ecore_do_mcp_cmd(struct ecore_hwfn *p_hwfn,
					     struct ecore_ptt *p_ptt,
					     u32 cmd, u32 param,
					     u32 *o_mcp_resp,
					     u32 *o_mcp_param)
{
	u32 delay = CHIP_MCP_RESP_ITER_US;
	u32 max_retries = ECORE_DRV_MB_MAX_RETRIES;
	u32 seq, cnt = 1, actual_mb_seq;
	enum _ecore_status_t rc = ECORE_SUCCESS;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->p_dev))
		delay = EMUL_MCP_RESP_ITER_US;
	/* There is a built-in delay of 100usec in each MFW response read */
	if (CHIP_REV_IS_FPGA(p_hwfn->p_dev))
		max_retries /= 10;
#endif

	/* Get actual driver mailbox sequence */
	actual_mb_seq = DRV_MB_RD(p_hwfn, p_ptt, drv_mb_header) &
	    DRV_MSG_SEQ_NUMBER_MASK;

	/* Use MCP history register to check if MCP reset occurred between
	 * init time and now.
	 */
	if (p_hwfn->mcp_info->mcp_hist !=
	    ecore_rd(p_hwfn, p_ptt, MISCS_REG_GENERIC_POR_0)) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_SP, "Rereading MCP offsets\n");
		ecore_load_mcp_offsets(p_hwfn, p_ptt);
		ecore_mcp_cmd_port_init(p_hwfn, p_ptt);
	}
	seq = ++p_hwfn->mcp_info->drv_mb_seq;

	/* Set drv param */
	DRV_MB_WR(p_hwfn, p_ptt, drv_mb_param, param);

	/* Set drv command along with the updated sequence */
	DRV_MB_WR(p_hwfn, p_ptt, drv_mb_header, (cmd | seq));

	do {
		/* Wait for MFW response */
		OSAL_UDELAY(delay);
		*o_mcp_resp = DRV_MB_RD(p_hwfn, p_ptt, fw_mb_header);

		/* Give the FW up to 5 second (500*10ms) */
	} while ((seq != (*o_mcp_resp & FW_MSG_SEQ_NUMBER_MASK)) &&
		 (cnt++ < max_retries));

	/* Is this a reply to our command? */
	if (seq == (*o_mcp_resp & FW_MSG_SEQ_NUMBER_MASK)) {
		*o_mcp_resp &= FW_MSG_CODE_MASK;
		/* Get the MCP param */
		*o_mcp_param = DRV_MB_RD(p_hwfn, p_ptt, fw_mb_param);
	} else {
		/* FW BUG! */
		DP_ERR(p_hwfn, "MFW failed to respond [cmd 0x%x param 0x%x]\n",
		       cmd, param);
		*o_mcp_resp = 0;
		rc = ECORE_AGAIN;
		ecore_hw_err_notify(p_hwfn, ECORE_HW_ERR_MFW_RESP_FAIL);
	}
	return rc;
}

static enum _ecore_status_t
ecore_mcp_cmd_and_union(struct ecore_hwfn *p_hwfn,
			struct ecore_ptt *p_ptt,
			struct ecore_mcp_mb_params *p_mb_params)
{
	union drv_union_data union_data;
	u32 union_data_addr;
	enum _ecore_status_t rc;

	/* MCP not initialized */
	if (!ecore_mcp_is_init(p_hwfn)) {
		DP_NOTICE(p_hwfn, true, "MFW is not initialized !\n");
		return ECORE_BUSY;
	}

	if (p_mb_params->data_src_size > sizeof(union_data) ||
	    p_mb_params->data_dst_size > sizeof(union_data)) {
		DP_ERR(p_hwfn,
		       "The provided size is larger than the union data size [src_size %u, dst_size %u, union_data_size %zu]\n",
		       p_mb_params->data_src_size, p_mb_params->data_dst_size,
		       sizeof(union_data));
		return ECORE_INVAL;
	}

	union_data_addr = p_hwfn->mcp_info->drv_mb_addr +
			  OFFSETOF(struct public_drv_mb, union_data);

	/* Ensure that only a single thread is accessing the mailbox at a
	 * certain time.
	 */
	rc = ecore_mcp_mb_lock(p_hwfn, p_mb_params->cmd);
	if (rc != ECORE_SUCCESS)
		return rc;

	OSAL_MEM_ZERO(&union_data, sizeof(union_data));
	if (p_mb_params->p_data_src != OSAL_NULL && p_mb_params->data_src_size)
		OSAL_MEMCPY(&union_data, p_mb_params->p_data_src,
			    p_mb_params->data_src_size);
	ecore_memcpy_to(p_hwfn, p_ptt, union_data_addr, &union_data,
			sizeof(union_data));

	rc = ecore_do_mcp_cmd(p_hwfn, p_ptt, p_mb_params->cmd,
			      p_mb_params->param, &p_mb_params->mcp_resp,
			      &p_mb_params->mcp_param);

	if (p_mb_params->p_data_dst != OSAL_NULL &&
	    p_mb_params->data_dst_size)
		ecore_memcpy_from(p_hwfn, p_ptt, p_mb_params->p_data_dst,
				  union_data_addr, p_mb_params->data_dst_size);

	ecore_mcp_mb_unlock(p_hwfn, p_mb_params->cmd);

	return rc;
}

enum _ecore_status_t ecore_mcp_cmd(struct ecore_hwfn *p_hwfn,
				   struct ecore_ptt *p_ptt, u32 cmd, u32 param,
				   u32 *o_mcp_resp, u32 *o_mcp_param)
{
	struct ecore_mcp_mb_params mb_params;
	enum _ecore_status_t rc;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->p_dev)) {
		if (cmd == DRV_MSG_CODE_UNLOAD_REQ) {
			loaded--;
			loaded_port[p_hwfn->port_id]--;
			DP_VERBOSE(p_hwfn, ECORE_MSG_SP, "Unload cnt: 0x%x\n",
				   loaded);
		}
		return ECORE_SUCCESS;
	}
#endif

	OSAL_MEM_ZERO(&mb_params, sizeof(mb_params));
	mb_params.cmd = cmd;
	mb_params.param = param;
	rc = ecore_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc != ECORE_SUCCESS)
		return rc;

	*o_mcp_resp = mb_params.mcp_resp;
	*o_mcp_param = mb_params.mcp_param;

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_mcp_nvm_wr_cmd(struct ecore_hwfn *p_hwfn,
					  struct ecore_ptt *p_ptt,
					  u32 cmd,
					  u32 param,
					  u32 *o_mcp_resp,
					  u32 *o_mcp_param,
					  u32 i_txn_size, u32 *i_buf)
{
	struct ecore_mcp_mb_params mb_params;
	enum _ecore_status_t rc;

	OSAL_MEM_ZERO(&mb_params, sizeof(mb_params));
	mb_params.cmd = cmd;
	mb_params.param = param;
	mb_params.p_data_src = i_buf;
	mb_params.data_src_size = (u8)i_txn_size;
	rc = ecore_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc != ECORE_SUCCESS)
		return rc;

	*o_mcp_resp = mb_params.mcp_resp;
	*o_mcp_param = mb_params.mcp_param;

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_mcp_nvm_rd_cmd(struct ecore_hwfn *p_hwfn,
					  struct ecore_ptt *p_ptt,
					  u32 cmd,
					  u32 param,
					  u32 *o_mcp_resp,
					  u32 *o_mcp_param,
					  u32 *o_txn_size, u32 *o_buf)
{
	struct ecore_mcp_mb_params mb_params;
	u8 raw_data[MCP_DRV_NVM_BUF_LEN];
	enum _ecore_status_t rc;

	OSAL_MEM_ZERO(&mb_params, sizeof(mb_params));
	mb_params.cmd = cmd;
	mb_params.param = param;
	mb_params.p_data_dst = raw_data;

	/* Use the maximal value since the actual one is part of the response */
	mb_params.data_dst_size = MCP_DRV_NVM_BUF_LEN;

	rc = ecore_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc != ECORE_SUCCESS)
		return rc;

	*o_mcp_resp = mb_params.mcp_resp;
	*o_mcp_param = mb_params.mcp_param;

	*o_txn_size = *o_mcp_param;
	/* @DPDK */
	OSAL_MEMCPY(o_buf, raw_data, RTE_MIN(*o_txn_size, MCP_DRV_NVM_BUF_LEN));

	return ECORE_SUCCESS;
}

#ifndef ASIC_ONLY
static void ecore_mcp_mf_workaround(struct ecore_hwfn *p_hwfn,
				    u32 *p_load_code)
{
	static int load_phase = FW_MSG_CODE_DRV_LOAD_ENGINE;

	if (!loaded)
		load_phase = FW_MSG_CODE_DRV_LOAD_ENGINE;
	else if (!loaded_port[p_hwfn->port_id])
		load_phase = FW_MSG_CODE_DRV_LOAD_PORT;
	else
		load_phase = FW_MSG_CODE_DRV_LOAD_FUNCTION;

	/* On CMT, always tell that it's engine */
	if (p_hwfn->p_dev->num_hwfns > 1)
		load_phase = FW_MSG_CODE_DRV_LOAD_ENGINE;

	*p_load_code = load_phase;
	loaded++;
	loaded_port[p_hwfn->port_id]++;

	DP_VERBOSE(p_hwfn, ECORE_MSG_SP,
		   "Load phase: %x load cnt: 0x%x port id=%d port_load=%d\n",
		   *p_load_code, loaded, p_hwfn->port_id,
		   loaded_port[p_hwfn->port_id]);
}
#endif

static bool ecore_mcp_can_force_load(u8 drv_role, u8 exist_drv_role)
{
	return (drv_role == DRV_ROLE_OS &&
		exist_drv_role == DRV_ROLE_PREBOOT) ||
	       (drv_role == DRV_ROLE_KDUMP && exist_drv_role == DRV_ROLE_OS);
}

static enum _ecore_status_t ecore_mcp_cancel_load_req(struct ecore_hwfn *p_hwfn,
						      struct ecore_ptt *p_ptt)
{
	u32 resp = 0, param = 0;
	enum _ecore_status_t rc;

	rc = ecore_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_CANCEL_LOAD_REQ, 0,
			   &resp, &param);
	if (rc != ECORE_SUCCESS)
		DP_NOTICE(p_hwfn, false,
			  "Failed to send cancel load request, rc = %d\n", rc);

	return rc;
}

#define CONFIG_ECORE_L2_BITMAP_IDX	(0x1 << 0)
#define CONFIG_ECORE_SRIOV_BITMAP_IDX	(0x1 << 1)
#define CONFIG_ECORE_ROCE_BITMAP_IDX	(0x1 << 2)
#define CONFIG_ECORE_IWARP_BITMAP_IDX	(0x1 << 3)
#define CONFIG_ECORE_FCOE_BITMAP_IDX	(0x1 << 4)
#define CONFIG_ECORE_ISCSI_BITMAP_IDX	(0x1 << 5)
#define CONFIG_ECORE_LL2_BITMAP_IDX	(0x1 << 6)

static u32 ecore_get_config_bitmap(void)
{
	u32 config_bitmap = 0x0;

#ifdef CONFIG_ECORE_L2
	config_bitmap |= CONFIG_ECORE_L2_BITMAP_IDX;
#endif
#ifdef CONFIG_ECORE_SRIOV
	config_bitmap |= CONFIG_ECORE_SRIOV_BITMAP_IDX;
#endif
#ifdef CONFIG_ECORE_ROCE
	config_bitmap |= CONFIG_ECORE_ROCE_BITMAP_IDX;
#endif
#ifdef CONFIG_ECORE_IWARP
	config_bitmap |= CONFIG_ECORE_IWARP_BITMAP_IDX;
#endif
#ifdef CONFIG_ECORE_FCOE
	config_bitmap |= CONFIG_ECORE_FCOE_BITMAP_IDX;
#endif
#ifdef CONFIG_ECORE_ISCSI
	config_bitmap |= CONFIG_ECORE_ISCSI_BITMAP_IDX;
#endif
#ifdef CONFIG_ECORE_LL2
	config_bitmap |= CONFIG_ECORE_LL2_BITMAP_IDX;
#endif

	return config_bitmap;
}

struct ecore_load_req_in_params {
	u8 hsi_ver;
#define ECORE_LOAD_REQ_HSI_VER_DEFAULT	0
#define ECORE_LOAD_REQ_HSI_VER_1	1
	u32 drv_ver_0;
	u32 drv_ver_1;
	u32 fw_ver;
	u8 drv_role;
	u8 timeout_val;
	u8 force_cmd;
	bool avoid_eng_reset;
};

struct ecore_load_req_out_params {
	u32 load_code;
	u32 exist_drv_ver_0;
	u32 exist_drv_ver_1;
	u32 exist_fw_ver;
	u8 exist_drv_role;
	u8 mfw_hsi_ver;
	bool drv_exists;
};

static enum _ecore_status_t
__ecore_mcp_load_req(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt,
		     struct ecore_load_req_in_params *p_in_params,
		     struct ecore_load_req_out_params *p_out_params)
{
	struct ecore_mcp_mb_params mb_params;
	struct load_req_stc load_req;
	struct load_rsp_stc load_rsp;
	u32 hsi_ver;
	enum _ecore_status_t rc;

	OSAL_MEM_ZERO(&load_req, sizeof(load_req));
	load_req.drv_ver_0 = p_in_params->drv_ver_0;
	load_req.drv_ver_1 = p_in_params->drv_ver_1;
	load_req.fw_ver = p_in_params->fw_ver;
	ECORE_MFW_SET_FIELD(load_req.misc0, LOAD_REQ_ROLE,
			    p_in_params->drv_role);
	ECORE_MFW_SET_FIELD(load_req.misc0, LOAD_REQ_LOCK_TO,
			    p_in_params->timeout_val);
	ECORE_MFW_SET_FIELD(load_req.misc0, LOAD_REQ_FORCE,
			    p_in_params->force_cmd);
	ECORE_MFW_SET_FIELD(load_req.misc0, LOAD_REQ_FLAGS0,
			    p_in_params->avoid_eng_reset);

	hsi_ver = (p_in_params->hsi_ver == ECORE_LOAD_REQ_HSI_VER_DEFAULT) ?
		  DRV_ID_MCP_HSI_VER_CURRENT :
		  (p_in_params->hsi_ver << DRV_ID_MCP_HSI_VER_SHIFT);

	OSAL_MEM_ZERO(&mb_params, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_LOAD_REQ;
	mb_params.param = PDA_COMP | hsi_ver | p_hwfn->p_dev->drv_type;
	mb_params.p_data_src = &load_req;
	mb_params.data_src_size = sizeof(load_req);
	mb_params.p_data_dst = &load_rsp;
	mb_params.data_dst_size = sizeof(load_rsp);

	DP_VERBOSE(p_hwfn, ECORE_MSG_SP,
		   "Load Request: param 0x%08x [init_hw %d, drv_type %d, hsi_ver %d, pda 0x%04x]\n",
		   mb_params.param,
		   ECORE_MFW_GET_FIELD(mb_params.param, DRV_ID_DRV_INIT_HW),
		   ECORE_MFW_GET_FIELD(mb_params.param, DRV_ID_DRV_TYPE),
		   ECORE_MFW_GET_FIELD(mb_params.param, DRV_ID_MCP_HSI_VER),
		   ECORE_MFW_GET_FIELD(mb_params.param, DRV_ID_PDA_COMP_VER));

	if (p_in_params->hsi_ver != ECORE_LOAD_REQ_HSI_VER_1)
		DP_VERBOSE(p_hwfn, ECORE_MSG_SP,
			   "Load Request: drv_ver 0x%08x_0x%08x, fw_ver 0x%08x, misc0 0x%08x [role %d, timeout %d, force %d, flags0 0x%x]\n",
			   load_req.drv_ver_0, load_req.drv_ver_1,
			   load_req.fw_ver, load_req.misc0,
			   ECORE_MFW_GET_FIELD(load_req.misc0, LOAD_REQ_ROLE),
			   ECORE_MFW_GET_FIELD(load_req.misc0,
					       LOAD_REQ_LOCK_TO),
			   ECORE_MFW_GET_FIELD(load_req.misc0, LOAD_REQ_FORCE),
			   ECORE_MFW_GET_FIELD(load_req.misc0,
					       LOAD_REQ_FLAGS0));

	rc = ecore_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc != ECORE_SUCCESS) {
		DP_NOTICE(p_hwfn, false,
			  "Failed to send load request, rc = %d\n", rc);
		return rc;
	}

	DP_VERBOSE(p_hwfn, ECORE_MSG_SP,
		   "Load Response: resp 0x%08x\n", mb_params.mcp_resp);
	p_out_params->load_code = mb_params.mcp_resp;

	if (p_in_params->hsi_ver != ECORE_LOAD_REQ_HSI_VER_1 &&
	    p_out_params->load_code != FW_MSG_CODE_DRV_LOAD_REFUSED_HSI_1) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_SP,
			   "Load Response: exist_drv_ver 0x%08x_0x%08x, exist_fw_ver 0x%08x, misc0 0x%08x [exist_role %d, mfw_hsi %d, flags0 0x%x]\n",
			   load_rsp.drv_ver_0, load_rsp.drv_ver_1,
			   load_rsp.fw_ver, load_rsp.misc0,
			   ECORE_MFW_GET_FIELD(load_rsp.misc0, LOAD_RSP_ROLE),
			   ECORE_MFW_GET_FIELD(load_rsp.misc0, LOAD_RSP_HSI),
			   ECORE_MFW_GET_FIELD(load_rsp.misc0,
					       LOAD_RSP_FLAGS0));

		p_out_params->exist_drv_ver_0 = load_rsp.drv_ver_0;
		p_out_params->exist_drv_ver_1 = load_rsp.drv_ver_1;
		p_out_params->exist_fw_ver = load_rsp.fw_ver;
		p_out_params->exist_drv_role =
			ECORE_MFW_GET_FIELD(load_rsp.misc0, LOAD_RSP_ROLE);
		p_out_params->mfw_hsi_ver =
			ECORE_MFW_GET_FIELD(load_rsp.misc0, LOAD_RSP_HSI);
		p_out_params->drv_exists =
			ECORE_MFW_GET_FIELD(load_rsp.misc0, LOAD_RSP_FLAGS0) &
			LOAD_RSP_FLAGS0_DRV_EXISTS;
	}

	return ECORE_SUCCESS;
}

static enum _ecore_status_t eocre_get_mfw_drv_role(struct ecore_hwfn *p_hwfn,
						   enum ecore_drv_role drv_role,
						   u8 *p_mfw_drv_role)
{
	switch (drv_role) {
	case ECORE_DRV_ROLE_OS:
		*p_mfw_drv_role = DRV_ROLE_OS;
		break;
	case ECORE_DRV_ROLE_KDUMP:
		*p_mfw_drv_role = DRV_ROLE_KDUMP;
		break;
	default:
		DP_ERR(p_hwfn, "Unexpected driver role %d\n", drv_role);
		return ECORE_INVAL;
	}

	return ECORE_SUCCESS;
}

enum ecore_load_req_force {
	ECORE_LOAD_REQ_FORCE_NONE,
	ECORE_LOAD_REQ_FORCE_PF,
	ECORE_LOAD_REQ_FORCE_ALL,
};

static enum _ecore_status_t
ecore_get_mfw_force_cmd(struct ecore_hwfn *p_hwfn,
			enum ecore_load_req_force force_cmd,
			u8 *p_mfw_force_cmd)
{
	switch (force_cmd) {
	case ECORE_LOAD_REQ_FORCE_NONE:
		*p_mfw_force_cmd = LOAD_REQ_FORCE_NONE;
		break;
	case ECORE_LOAD_REQ_FORCE_PF:
		*p_mfw_force_cmd = LOAD_REQ_FORCE_PF;
		break;
	case ECORE_LOAD_REQ_FORCE_ALL:
		*p_mfw_force_cmd = LOAD_REQ_FORCE_ALL;
		break;
	default:
		DP_ERR(p_hwfn, "Unexpected force value %d\n", force_cmd);
		return ECORE_INVAL;
	}

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_mcp_load_req(struct ecore_hwfn *p_hwfn,
					struct ecore_ptt *p_ptt,
					struct ecore_load_req_params *p_params)
{
	struct ecore_load_req_out_params out_params;
	struct ecore_load_req_in_params in_params;
	u8 mfw_drv_role, mfw_force_cmd;
	enum _ecore_status_t rc;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->p_dev)) {
		ecore_mcp_mf_workaround(p_hwfn, &p_params->load_code);
		return ECORE_SUCCESS;
	}
#endif

	OSAL_MEM_ZERO(&in_params, sizeof(in_params));
	in_params.hsi_ver = ECORE_LOAD_REQ_HSI_VER_DEFAULT;
	in_params.drv_ver_0 = ECORE_VERSION;
	in_params.drv_ver_1 = ecore_get_config_bitmap();
	in_params.fw_ver = STORM_FW_VERSION;
	rc = eocre_get_mfw_drv_role(p_hwfn, p_params->drv_role, &mfw_drv_role);
	if (rc != ECORE_SUCCESS)
		return rc;

	in_params.drv_role = mfw_drv_role;
	in_params.timeout_val = p_params->timeout_val;
	rc = ecore_get_mfw_force_cmd(p_hwfn, ECORE_LOAD_REQ_FORCE_NONE,
				     &mfw_force_cmd);
	if (rc != ECORE_SUCCESS)
		return rc;

	in_params.force_cmd = mfw_force_cmd;
	in_params.avoid_eng_reset = p_params->avoid_eng_reset;

	OSAL_MEM_ZERO(&out_params, sizeof(out_params));
	rc = __ecore_mcp_load_req(p_hwfn, p_ptt, &in_params, &out_params);
	if (rc != ECORE_SUCCESS)
		return rc;

	/* First handle cases where another load request should/might be sent:
	 * - MFW expects the old interface [HSI version = 1]
	 * - MFW responds that a force load request is required
	 */
	if (out_params.load_code == FW_MSG_CODE_DRV_LOAD_REFUSED_HSI_1) {
		DP_INFO(p_hwfn,
			"MFW refused a load request due to HSI > 1. Resending with HSI = 1.\n");

		/* The previous load request set the mailbox blocking */
		p_hwfn->mcp_info->block_mb_sending = false;

		in_params.hsi_ver = ECORE_LOAD_REQ_HSI_VER_1;
		OSAL_MEM_ZERO(&out_params, sizeof(out_params));
		rc = __ecore_mcp_load_req(p_hwfn, p_ptt, &in_params,
					  &out_params);
		if (rc != ECORE_SUCCESS)
			return rc;
	} else if (out_params.load_code ==
		   FW_MSG_CODE_DRV_LOAD_REFUSED_REQUIRES_FORCE) {
		/* The previous load request set the mailbox blocking */
		p_hwfn->mcp_info->block_mb_sending = false;

		if (ecore_mcp_can_force_load(in_params.drv_role,
					     out_params.exist_drv_role)) {
			DP_INFO(p_hwfn,
				"A force load is required [existing: role %d, fw_ver 0x%08x, drv_ver 0x%08x_0x%08x]. Sending a force load request.\n",
				out_params.exist_drv_role,
				out_params.exist_fw_ver,
				out_params.exist_drv_ver_0,
				out_params.exist_drv_ver_1);

			rc = ecore_get_mfw_force_cmd(p_hwfn,
						     ECORE_LOAD_REQ_FORCE_ALL,
						     &mfw_force_cmd);
			if (rc != ECORE_SUCCESS)
				return rc;

			in_params.force_cmd = mfw_force_cmd;
			OSAL_MEM_ZERO(&out_params, sizeof(out_params));
			rc = __ecore_mcp_load_req(p_hwfn, p_ptt, &in_params,
						  &out_params);
			if (rc != ECORE_SUCCESS)
				return rc;
		} else {
			DP_NOTICE(p_hwfn, false,
				  "A force load is required [existing: role %d, fw_ver 0x%08x, drv_ver 0x%08x_0x%08x]. Avoiding to prevent disruption of active PFs.\n",
				  out_params.exist_drv_role,
				  out_params.exist_fw_ver,
				  out_params.exist_drv_ver_0,
				  out_params.exist_drv_ver_1);

			ecore_mcp_cancel_load_req(p_hwfn, p_ptt);
			return ECORE_BUSY;
		}
	}

	/* Now handle the other types of responses.
	 * The "REFUSED_HSI_1" and "REFUSED_REQUIRES_FORCE" responses are not
	 * expected here after the additional revised load requests were sent.
	 */
	switch (out_params.load_code) {
	case FW_MSG_CODE_DRV_LOAD_ENGINE:
	case FW_MSG_CODE_DRV_LOAD_PORT:
	case FW_MSG_CODE_DRV_LOAD_FUNCTION:
		if (out_params.mfw_hsi_ver != ECORE_LOAD_REQ_HSI_VER_1 &&
		    out_params.drv_exists) {
			/* The role and fw/driver version match, but the PF is
			 * already loaded and has not been unloaded gracefully.
			 * This is unexpected since a quasi-FLR request was
			 * previously sent as part of ecore_hw_prepare().
			 */
			DP_NOTICE(p_hwfn, false,
				  "PF is already loaded - shouldn't have got here since a quasi-FLR request was previously sent!\n");
			return ECORE_INVAL;
		}
		break;
	case FW_MSG_CODE_DRV_LOAD_REFUSED_PDA:
	case FW_MSG_CODE_DRV_LOAD_REFUSED_DIAG:
	case FW_MSG_CODE_DRV_LOAD_REFUSED_HSI:
	case FW_MSG_CODE_DRV_LOAD_REFUSED_REJECT:
		DP_NOTICE(p_hwfn, false,
			  "MFW refused a load request [resp 0x%08x]. Aborting.\n",
			  out_params.load_code);
		return ECORE_BUSY;
	default:
		DP_NOTICE(p_hwfn, false,
			  "Unexpected response to load request [resp 0x%08x]. Aborting.\n",
			  out_params.load_code);
		break;
	}

	p_params->load_code = out_params.load_code;

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_mcp_load_done(struct ecore_hwfn *p_hwfn,
					 struct ecore_ptt *p_ptt)
{
	u32 resp = 0, param = 0;
	enum _ecore_status_t rc;

	rc = ecore_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_LOAD_DONE, 0, &resp,
			   &param);
	if (rc != ECORE_SUCCESS) {
		DP_NOTICE(p_hwfn, false,
			  "Failed to send a LOAD_DONE command, rc = %d\n", rc);
		return rc;
	}

#define FW_MB_PARAM_LOAD_DONE_DID_EFUSE_ERROR     (1 << 0)

	/* Check if there is a DID mismatch between nvm-cfg/efuse */
	if (param & FW_MB_PARAM_LOAD_DONE_DID_EFUSE_ERROR)
		DP_NOTICE(p_hwfn, false,
			  "warning: device configuration is not supported on this board type. The device may not function as expected.\n");

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_mcp_unload_req(struct ecore_hwfn *p_hwfn,
					  struct ecore_ptt *p_ptt)
{
	u32 wol_param, mcp_resp, mcp_param;

	/* @DPDK */
	wol_param = DRV_MB_PARAM_UNLOAD_WOL_MCP;

	return ecore_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_UNLOAD_REQ, wol_param,
			     &mcp_resp, &mcp_param);
}

enum _ecore_status_t ecore_mcp_unload_done(struct ecore_hwfn *p_hwfn,
					   struct ecore_ptt *p_ptt)
{
	struct ecore_mcp_mb_params mb_params;
	struct mcp_mac wol_mac;

	OSAL_MEM_ZERO(&mb_params, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_UNLOAD_DONE;

	return ecore_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
}

static void ecore_mcp_handle_vf_flr(struct ecore_hwfn *p_hwfn,
				    struct ecore_ptt *p_ptt)
{
	u32 addr = SECTION_OFFSIZE_ADDR(p_hwfn->mcp_info->public_base,
					PUBLIC_PATH);
	u32 mfw_path_offsize = ecore_rd(p_hwfn, p_ptt, addr);
	u32 path_addr = SECTION_ADDR(mfw_path_offsize,
				     ECORE_PATH_ID(p_hwfn));
	u32 disabled_vfs[VF_MAX_STATIC / 32];
	int i;

	DP_VERBOSE(p_hwfn, ECORE_MSG_SP,
		   "Reading Disabled VF information from [offset %08x],"
		   " path_addr %08x\n",
		   mfw_path_offsize, path_addr);

	for (i = 0; i < (VF_MAX_STATIC / 32); i++) {
		disabled_vfs[i] = ecore_rd(p_hwfn, p_ptt,
					   path_addr +
					   OFFSETOF(struct public_path,
						    mcp_vf_disabled) +
					   sizeof(u32) * i);
		DP_VERBOSE(p_hwfn, (ECORE_MSG_SP | ECORE_MSG_IOV),
			   "FLR-ed VFs [%08x,...,%08x] - %08x\n",
			   i * 32, (i + 1) * 32 - 1, disabled_vfs[i]);
	}

	if (ecore_iov_mark_vf_flr(p_hwfn, disabled_vfs))
		OSAL_VF_FLR_UPDATE(p_hwfn);
}

enum _ecore_status_t ecore_mcp_ack_vf_flr(struct ecore_hwfn *p_hwfn,
					  struct ecore_ptt *p_ptt,
					  u32 *vfs_to_ack)
{
	u32 addr = SECTION_OFFSIZE_ADDR(p_hwfn->mcp_info->public_base,
					PUBLIC_FUNC);
	u32 mfw_func_offsize = ecore_rd(p_hwfn, p_ptt, addr);
	u32 func_addr = SECTION_ADDR(mfw_func_offsize,
				     MCP_PF_ID(p_hwfn));
	struct ecore_mcp_mb_params mb_params;
	enum _ecore_status_t rc;
	int i;

	for (i = 0; i < (VF_MAX_STATIC / 32); i++)
		DP_VERBOSE(p_hwfn, (ECORE_MSG_SP | ECORE_MSG_IOV),
			   "Acking VFs [%08x,...,%08x] - %08x\n",
			   i * 32, (i + 1) * 32 - 1, vfs_to_ack[i]);

	OSAL_MEM_ZERO(&mb_params, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_VF_DISABLED_DONE;
	mb_params.p_data_src = vfs_to_ack;
	mb_params.data_src_size = VF_MAX_STATIC / 8;
	rc = ecore_mcp_cmd_and_union(p_hwfn, p_ptt,
				     &mb_params);
	if (rc != ECORE_SUCCESS) {
		DP_NOTICE(p_hwfn, false,
			  "Failed to pass ACK for VF flr to MFW\n");
		return ECORE_TIMEOUT;
	}

	/* TMP - clear the ACK bits; should be done by MFW */
	for (i = 0; i < (VF_MAX_STATIC / 32); i++)
		ecore_wr(p_hwfn, p_ptt,
			 func_addr +
			 OFFSETOF(struct public_func, drv_ack_vf_disabled) +
			 i * sizeof(u32), 0);

	return rc;
}

static void ecore_mcp_handle_transceiver_change(struct ecore_hwfn *p_hwfn,
						struct ecore_ptt *p_ptt)
{
	u32 transceiver_state;

	transceiver_state = ecore_rd(p_hwfn, p_ptt,
				     p_hwfn->mcp_info->port_addr +
				     OFFSETOF(struct public_port,
					      transceiver_data));

	DP_VERBOSE(p_hwfn, (ECORE_MSG_HW | ECORE_MSG_SP),
		   "Received transceiver state update [0x%08x] from mfw"
		   " [Addr 0x%x]\n",
		   transceiver_state, (u32)(p_hwfn->mcp_info->port_addr +
					    OFFSETOF(struct public_port,
						     transceiver_data)));

	transceiver_state = GET_FIELD(transceiver_state, ETH_TRANSCEIVER_STATE);

	if (transceiver_state == ETH_TRANSCEIVER_STATE_PRESENT)
		DP_NOTICE(p_hwfn, false, "Transceiver is present.\n");
	else
		DP_NOTICE(p_hwfn, false, "Transceiver is unplugged.\n");
}

static void ecore_mcp_handle_link_change(struct ecore_hwfn *p_hwfn,
					 struct ecore_ptt *p_ptt,
					 bool b_reset)
{
	struct ecore_mcp_link_state *p_link;
	u8 max_bw, min_bw;
	u32 status = 0;

	p_link = &p_hwfn->mcp_info->link_output;
	OSAL_MEMSET(p_link, 0, sizeof(*p_link));
	if (!b_reset) {
		status = ecore_rd(p_hwfn, p_ptt,
				  p_hwfn->mcp_info->port_addr +
				  OFFSETOF(struct public_port, link_status));
		DP_VERBOSE(p_hwfn, (ECORE_MSG_LINK | ECORE_MSG_SP),
			   "Received link update [0x%08x] from mfw"
			   " [Addr 0x%x]\n",
			   status, (u32)(p_hwfn->mcp_info->port_addr +
					  OFFSETOF(struct public_port,
						   link_status)));
	} else {
		DP_VERBOSE(p_hwfn, ECORE_MSG_LINK,
			   "Resetting link indications\n");
		return;
	}

	if (p_hwfn->b_drv_link_init)
		p_link->link_up = !!(status & LINK_STATUS_LINK_UP);
	else
		p_link->link_up = false;

	p_link->full_duplex = true;
	switch ((status & LINK_STATUS_SPEED_AND_DUPLEX_MASK)) {
	case LINK_STATUS_SPEED_AND_DUPLEX_100G:
		p_link->speed = 100000;
		break;
	case LINK_STATUS_SPEED_AND_DUPLEX_50G:
		p_link->speed = 50000;
		break;
	case LINK_STATUS_SPEED_AND_DUPLEX_40G:
		p_link->speed = 40000;
		break;
	case LINK_STATUS_SPEED_AND_DUPLEX_25G:
		p_link->speed = 25000;
		break;
	case LINK_STATUS_SPEED_AND_DUPLEX_20G:
		p_link->speed = 20000;
		break;
	case LINK_STATUS_SPEED_AND_DUPLEX_10G:
		p_link->speed = 10000;
		break;
	case LINK_STATUS_SPEED_AND_DUPLEX_1000THD:
		p_link->full_duplex = false;
		/* Fall-through */
	case LINK_STATUS_SPEED_AND_DUPLEX_1000TFD:
		p_link->speed = 1000;
		break;
	default:
		p_link->speed = 0;
	}

	/* We never store total line speed as p_link->speed is
	 * again changes according to bandwidth allocation.
	 */
	if (p_link->link_up && p_link->speed)
		p_link->line_speed = p_link->speed;
	else
		p_link->line_speed = 0;

	max_bw = p_hwfn->mcp_info->func_info.bandwidth_max;
	min_bw = p_hwfn->mcp_info->func_info.bandwidth_min;

	/* Max bandwidth configuration */
	__ecore_configure_pf_max_bandwidth(p_hwfn, p_ptt,
					   p_link, max_bw);

	/* Mintz bandwidth configuration */
	__ecore_configure_pf_min_bandwidth(p_hwfn, p_ptt,
					   p_link, min_bw);
	ecore_configure_vp_wfq_on_link_change(p_hwfn->p_dev,
					      p_link->min_pf_rate);

	p_link->an = !!(status & LINK_STATUS_AUTO_NEGOTIATE_ENABLED);
	p_link->an_complete = !!(status & LINK_STATUS_AUTO_NEGOTIATE_COMPLETE);
	p_link->parallel_detection = !!(status &
					 LINK_STATUS_PARALLEL_DETECTION_USED);
	p_link->pfc_enabled = !!(status & LINK_STATUS_PFC_ENABLED);

	p_link->partner_adv_speed |=
	    (status & LINK_STATUS_LINK_PARTNER_1000TFD_CAPABLE) ?
	    ECORE_LINK_PARTNER_SPEED_1G_FD : 0;
	p_link->partner_adv_speed |=
	    (status & LINK_STATUS_LINK_PARTNER_1000THD_CAPABLE) ?
	    ECORE_LINK_PARTNER_SPEED_1G_HD : 0;
	p_link->partner_adv_speed |=
	    (status & LINK_STATUS_LINK_PARTNER_10G_CAPABLE) ?
	    ECORE_LINK_PARTNER_SPEED_10G : 0;
	p_link->partner_adv_speed |=
	    (status & LINK_STATUS_LINK_PARTNER_20G_CAPABLE) ?
	    ECORE_LINK_PARTNER_SPEED_20G : 0;
	p_link->partner_adv_speed |=
	    (status & LINK_STATUS_LINK_PARTNER_25G_CAPABLE) ?
	    ECORE_LINK_PARTNER_SPEED_25G : 0;
	p_link->partner_adv_speed |=
	    (status & LINK_STATUS_LINK_PARTNER_40G_CAPABLE) ?
	    ECORE_LINK_PARTNER_SPEED_40G : 0;
	p_link->partner_adv_speed |=
	    (status & LINK_STATUS_LINK_PARTNER_50G_CAPABLE) ?
	    ECORE_LINK_PARTNER_SPEED_50G : 0;
	p_link->partner_adv_speed |=
	    (status & LINK_STATUS_LINK_PARTNER_100G_CAPABLE) ?
	    ECORE_LINK_PARTNER_SPEED_100G : 0;

	p_link->partner_tx_flow_ctrl_en =
	    !!(status & LINK_STATUS_TX_FLOW_CONTROL_ENABLED);
	p_link->partner_rx_flow_ctrl_en =
	    !!(status & LINK_STATUS_RX_FLOW_CONTROL_ENABLED);

	switch (status & LINK_STATUS_LINK_PARTNER_FLOW_CONTROL_MASK) {
	case LINK_STATUS_LINK_PARTNER_SYMMETRIC_PAUSE:
		p_link->partner_adv_pause = ECORE_LINK_PARTNER_SYMMETRIC_PAUSE;
		break;
	case LINK_STATUS_LINK_PARTNER_ASYMMETRIC_PAUSE:
		p_link->partner_adv_pause = ECORE_LINK_PARTNER_ASYMMETRIC_PAUSE;
		break;
	case LINK_STATUS_LINK_PARTNER_BOTH_PAUSE:
		p_link->partner_adv_pause = ECORE_LINK_PARTNER_BOTH_PAUSE;
		break;
	default:
		p_link->partner_adv_pause = 0;
	}

	p_link->sfp_tx_fault = !!(status & LINK_STATUS_SFP_TX_FAULT);

	OSAL_LINK_UPDATE(p_hwfn);
}

enum _ecore_status_t ecore_mcp_set_link(struct ecore_hwfn *p_hwfn,
					struct ecore_ptt *p_ptt, bool b_up)
{
	struct ecore_mcp_link_params *params = &p_hwfn->mcp_info->link_input;
	struct ecore_mcp_mb_params mb_params;
	struct eth_phy_cfg phy_cfg;
	enum _ecore_status_t rc = ECORE_SUCCESS;
	u32 cmd;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->p_dev))
		return ECORE_SUCCESS;
#endif

	/* Set the shmem configuration according to params */
	OSAL_MEM_ZERO(&phy_cfg, sizeof(phy_cfg));
	cmd = b_up ? DRV_MSG_CODE_INIT_PHY : DRV_MSG_CODE_LINK_RESET;
	if (!params->speed.autoneg)
		phy_cfg.speed = params->speed.forced_speed;
	phy_cfg.pause |= (params->pause.autoneg) ? ETH_PAUSE_AUTONEG : 0;
	phy_cfg.pause |= (params->pause.forced_rx) ? ETH_PAUSE_RX : 0;
	phy_cfg.pause |= (params->pause.forced_tx) ? ETH_PAUSE_TX : 0;
	phy_cfg.adv_speed = params->speed.advertised_speeds;
	phy_cfg.loopback_mode = params->loopback_mode;
	p_hwfn->b_drv_link_init = b_up;

	if (b_up)
		DP_VERBOSE(p_hwfn, ECORE_MSG_LINK,
			   "Configuring Link: Speed 0x%08x, Pause 0x%08x,"
			   " adv_speed 0x%08x, loopback 0x%08x\n",
			   phy_cfg.speed, phy_cfg.pause, phy_cfg.adv_speed,
			   phy_cfg.loopback_mode);
	else
		DP_VERBOSE(p_hwfn, ECORE_MSG_LINK, "Resetting link\n");

	OSAL_MEM_ZERO(&mb_params, sizeof(mb_params));
	mb_params.cmd = cmd;
	mb_params.p_data_src = &phy_cfg;
	mb_params.data_src_size = sizeof(phy_cfg);
	rc = ecore_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);

	/* if mcp fails to respond we must abort */
	if (rc != ECORE_SUCCESS) {
		DP_ERR(p_hwfn, "MCP response failure, aborting\n");
		return rc;
	}

	/* Reset the link status if needed */
	if (!b_up)
		ecore_mcp_handle_link_change(p_hwfn, p_ptt, true);

	return rc;
}

u32 ecore_get_process_kill_counter(struct ecore_hwfn *p_hwfn,
				   struct ecore_ptt *p_ptt)
{
	u32 path_offsize_addr, path_offsize, path_addr, proc_kill_cnt;

	/* TODO - Add support for VFs */
	if (IS_VF(p_hwfn->p_dev))
		return ECORE_INVAL;

	path_offsize_addr = SECTION_OFFSIZE_ADDR(p_hwfn->mcp_info->public_base,
						 PUBLIC_PATH);
	path_offsize = ecore_rd(p_hwfn, p_ptt, path_offsize_addr);
	path_addr = SECTION_ADDR(path_offsize, ECORE_PATH_ID(p_hwfn));

	proc_kill_cnt = ecore_rd(p_hwfn, p_ptt,
				 path_addr +
				 OFFSETOF(struct public_path, process_kill)) &
	    PROCESS_KILL_COUNTER_MASK;

	return proc_kill_cnt;
}

static void ecore_mcp_handle_process_kill(struct ecore_hwfn *p_hwfn,
					  struct ecore_ptt *p_ptt)
{
	struct ecore_dev *p_dev = p_hwfn->p_dev;
	u32 proc_kill_cnt;

	/* Prevent possible attentions/interrupts during the recovery handling
	 * and till its load phase, during which they will be re-enabled.
	 */
	ecore_int_igu_disable_int(p_hwfn, p_ptt);

	DP_NOTICE(p_hwfn, false, "Received a process kill indication\n");

	/* The following operations should be done once, and thus in CMT mode
	 * are carried out by only the first HW function.
	 */
	if (p_hwfn != ECORE_LEADING_HWFN(p_dev))
		return;

	if (p_dev->recov_in_prog) {
		DP_NOTICE(p_hwfn, false,
			  "Ignoring the indication since a recovery"
			  " process is already in progress\n");
		return;
	}

	p_dev->recov_in_prog = true;

	proc_kill_cnt = ecore_get_process_kill_counter(p_hwfn, p_ptt);
	DP_NOTICE(p_hwfn, false, "Process kill counter: %d\n", proc_kill_cnt);

	OSAL_SCHEDULE_RECOVERY_HANDLER(p_hwfn);
}

static void ecore_mcp_send_protocol_stats(struct ecore_hwfn *p_hwfn,
					  struct ecore_ptt *p_ptt,
					  enum MFW_DRV_MSG_TYPE type)
{
	enum ecore_mcp_protocol_type stats_type;
	union ecore_mcp_protocol_stats stats;
	struct ecore_mcp_mb_params mb_params;
	u32 hsi_param;
	enum _ecore_status_t rc;

	switch (type) {
	case MFW_DRV_MSG_GET_LAN_STATS:
		stats_type = ECORE_MCP_LAN_STATS;
		hsi_param = DRV_MSG_CODE_STATS_TYPE_LAN;
		break;
	default:
		DP_INFO(p_hwfn, "Invalid protocol type %d\n", type);
		return;
	}

	OSAL_GET_PROTOCOL_STATS(p_hwfn->p_dev, stats_type, &stats);

	OSAL_MEM_ZERO(&mb_params, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_GET_STATS;
	mb_params.param = hsi_param;
	mb_params.p_data_src = &stats;
	mb_params.data_src_size = sizeof(stats);
	rc = ecore_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc != ECORE_SUCCESS)
		DP_ERR(p_hwfn, "Failed to send protocol stats, rc = %d\n", rc);
}

static void ecore_read_pf_bandwidth(struct ecore_hwfn *p_hwfn,
				    struct public_func *p_shmem_info)
{
	struct ecore_mcp_function_info *p_info;

	p_info = &p_hwfn->mcp_info->func_info;

	/* TODO - bandwidth min/max should have valid values of 1-100,
	 * as well as some indication that the feature is disabled.
	 * Until MFW/qlediag enforce those limitations, Assume THERE IS ALWAYS
	 * limit and correct value to min `1' and max `100' if limit isn't in
	 * range.
	 */
	p_info->bandwidth_min = (p_shmem_info->config &
				 FUNC_MF_CFG_MIN_BW_MASK) >>
	    FUNC_MF_CFG_MIN_BW_SHIFT;
	if (p_info->bandwidth_min < 1 || p_info->bandwidth_min > 100) {
		DP_INFO(p_hwfn,
			"bandwidth minimum out of bounds [%02x]. Set to 1\n",
			p_info->bandwidth_min);
		p_info->bandwidth_min = 1;
	}

	p_info->bandwidth_max = (p_shmem_info->config &
				 FUNC_MF_CFG_MAX_BW_MASK) >>
	    FUNC_MF_CFG_MAX_BW_SHIFT;
	if (p_info->bandwidth_max < 1 || p_info->bandwidth_max > 100) {
		DP_INFO(p_hwfn,
			"bandwidth maximum out of bounds [%02x]. Set to 100\n",
			p_info->bandwidth_max);
		p_info->bandwidth_max = 100;
	}
}

static u32 ecore_mcp_get_shmem_func(struct ecore_hwfn *p_hwfn,
				    struct ecore_ptt *p_ptt,
				    struct public_func *p_data,
				    int pfid)
{
	u32 addr = SECTION_OFFSIZE_ADDR(p_hwfn->mcp_info->public_base,
					PUBLIC_FUNC);
	u32 mfw_path_offsize = ecore_rd(p_hwfn, p_ptt, addr);
	u32 func_addr = SECTION_ADDR(mfw_path_offsize, pfid);
	u32 i, size;

	OSAL_MEM_ZERO(p_data, sizeof(*p_data));

	size = OSAL_MIN_T(u32, sizeof(*p_data),
			  SECTION_SIZE(mfw_path_offsize));
	for (i = 0; i < size / sizeof(u32); i++)
		((u32 *)p_data)[i] = ecore_rd(p_hwfn, p_ptt,
					      func_addr + (i << 2));

	return size;
}

static void
ecore_mcp_update_bw(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt)
{
	struct ecore_mcp_function_info *p_info;
	struct public_func shmem_info;
	u32 resp = 0, param = 0;

	ecore_mcp_get_shmem_func(p_hwfn, p_ptt, &shmem_info, MCP_PF_ID(p_hwfn));

	ecore_read_pf_bandwidth(p_hwfn, &shmem_info);

	p_info = &p_hwfn->mcp_info->func_info;

	ecore_configure_pf_min_bandwidth(p_hwfn->p_dev, p_info->bandwidth_min);

	ecore_configure_pf_max_bandwidth(p_hwfn->p_dev, p_info->bandwidth_max);

	/* Acknowledge the MFW */
	ecore_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_BW_UPDATE_ACK, 0, &resp,
		      &param);
}

static void ecore_mcp_handle_fan_failure(struct ecore_hwfn *p_hwfn,
					 struct ecore_ptt *p_ptt)
{
	/* A single notification should be sent to upper driver in CMT mode */
	if (p_hwfn != ECORE_LEADING_HWFN(p_hwfn->p_dev))
		return;

	DP_NOTICE(p_hwfn, false,
		  "Fan failure was detected on the network interface card"
		  " and it's going to be shut down.\n");

	ecore_hw_err_notify(p_hwfn, ECORE_HW_ERR_FAN_FAIL);
}

struct ecore_mdump_cmd_params {
	u32 cmd;
	void *p_data_src;
	u8 data_src_size;
	void *p_data_dst;
	u8 data_dst_size;
	u32 mcp_resp;
};

static enum _ecore_status_t
ecore_mcp_mdump_cmd(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt,
		    struct ecore_mdump_cmd_params *p_mdump_cmd_params)
{
	struct ecore_mcp_mb_params mb_params;
	enum _ecore_status_t rc;

	OSAL_MEM_ZERO(&mb_params, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_MDUMP_CMD;
	mb_params.param = p_mdump_cmd_params->cmd;
	mb_params.p_data_src = p_mdump_cmd_params->p_data_src;
	mb_params.data_src_size = p_mdump_cmd_params->data_src_size;
	mb_params.p_data_dst = p_mdump_cmd_params->p_data_dst;
	mb_params.data_dst_size = p_mdump_cmd_params->data_dst_size;
	rc = ecore_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc != ECORE_SUCCESS)
		return rc;

	p_mdump_cmd_params->mcp_resp = mb_params.mcp_resp;
	if (p_mdump_cmd_params->mcp_resp == FW_MSG_CODE_MDUMP_INVALID_CMD) {
		DP_NOTICE(p_hwfn, false,
			  "MFW claims that the mdump command is illegal [mdump_cmd 0x%x]\n",
			  p_mdump_cmd_params->cmd);
		rc = ECORE_INVAL;
	}

	return rc;
}

static enum _ecore_status_t ecore_mcp_mdump_ack(struct ecore_hwfn *p_hwfn,
						struct ecore_ptt *p_ptt)
{
	struct ecore_mdump_cmd_params mdump_cmd_params;

	OSAL_MEM_ZERO(&mdump_cmd_params, sizeof(mdump_cmd_params));
	mdump_cmd_params.cmd = DRV_MSG_CODE_MDUMP_ACK;

	return ecore_mcp_mdump_cmd(p_hwfn, p_ptt, &mdump_cmd_params);
}

enum _ecore_status_t ecore_mcp_mdump_set_values(struct ecore_hwfn *p_hwfn,
						struct ecore_ptt *p_ptt,
						u32 epoch)
{
	struct ecore_mdump_cmd_params mdump_cmd_params;

	OSAL_MEM_ZERO(&mdump_cmd_params, sizeof(mdump_cmd_params));
	mdump_cmd_params.cmd = DRV_MSG_CODE_MDUMP_SET_VALUES;
	mdump_cmd_params.p_data_src = &epoch;
	mdump_cmd_params.data_src_size = sizeof(epoch);

	return ecore_mcp_mdump_cmd(p_hwfn, p_ptt, &mdump_cmd_params);
}

enum _ecore_status_t ecore_mcp_mdump_trigger(struct ecore_hwfn *p_hwfn,
					     struct ecore_ptt *p_ptt)
{
	struct ecore_mdump_cmd_params mdump_cmd_params;

	OSAL_MEM_ZERO(&mdump_cmd_params, sizeof(mdump_cmd_params));
	mdump_cmd_params.cmd = DRV_MSG_CODE_MDUMP_TRIGGER;

	return ecore_mcp_mdump_cmd(p_hwfn, p_ptt, &mdump_cmd_params);
}

static enum _ecore_status_t
ecore_mcp_mdump_get_config(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt,
			   struct mdump_config_stc *p_mdump_config)
{
	struct ecore_mdump_cmd_params mdump_cmd_params;
	enum _ecore_status_t rc;

	OSAL_MEM_ZERO(&mdump_cmd_params, sizeof(mdump_cmd_params));
	mdump_cmd_params.cmd = DRV_MSG_CODE_MDUMP_GET_CONFIG;
	mdump_cmd_params.p_data_dst = p_mdump_config;
	mdump_cmd_params.data_dst_size = sizeof(*p_mdump_config);

	rc = ecore_mcp_mdump_cmd(p_hwfn, p_ptt, &mdump_cmd_params);
	if (rc != ECORE_SUCCESS)
		return rc;

	if (mdump_cmd_params.mcp_resp == FW_MSG_CODE_UNSUPPORTED) {
		DP_INFO(p_hwfn,
			"The mdump command is not supported by the MFW\n");
		return ECORE_NOTIMPL;
	}

	if (mdump_cmd_params.mcp_resp != FW_MSG_CODE_OK) {
		DP_NOTICE(p_hwfn, false,
			  "Failed to get the mdump configuration and logs info [mcp_resp 0x%x]\n",
			  mdump_cmd_params.mcp_resp);
		rc = ECORE_UNKNOWN_ERROR;
	}

	return rc;
}

enum _ecore_status_t
ecore_mcp_mdump_get_info(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt,
			 struct ecore_mdump_info *p_mdump_info)
{
	u32 addr, global_offsize, global_addr;
	struct mdump_config_stc mdump_config;
	enum _ecore_status_t rc;

	OSAL_MEMSET(p_mdump_info, 0, sizeof(*p_mdump_info));

	addr = SECTION_OFFSIZE_ADDR(p_hwfn->mcp_info->public_base,
				    PUBLIC_GLOBAL);
	global_offsize = ecore_rd(p_hwfn, p_ptt, addr);
	global_addr = SECTION_ADDR(global_offsize, 0);
	p_mdump_info->reason = ecore_rd(p_hwfn, p_ptt,
					global_addr +
					OFFSETOF(struct public_global,
						 mdump_reason));

	if (p_mdump_info->reason) {
		rc = ecore_mcp_mdump_get_config(p_hwfn, p_ptt, &mdump_config);
		if (rc != ECORE_SUCCESS)
			return rc;

		p_mdump_info->version = mdump_config.version;
		p_mdump_info->config = mdump_config.config;
		p_mdump_info->epoch = mdump_config.epoc;
		p_mdump_info->num_of_logs = mdump_config.num_of_logs;
		p_mdump_info->valid_logs = mdump_config.valid_logs;

		DP_VERBOSE(p_hwfn, ECORE_MSG_SP,
			   "MFW mdump info: reason %d, version 0x%x, config 0x%x, epoch 0x%x, num_of_logs 0x%x, valid_logs 0x%x\n",
			   p_mdump_info->reason, p_mdump_info->version,
			   p_mdump_info->config, p_mdump_info->epoch,
			   p_mdump_info->num_of_logs, p_mdump_info->valid_logs);
	} else {
		DP_VERBOSE(p_hwfn, ECORE_MSG_SP,
			   "MFW mdump info: reason %d\n", p_mdump_info->reason);
	}

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_mcp_mdump_clear_logs(struct ecore_hwfn *p_hwfn,
						struct ecore_ptt *p_ptt)
{
	struct ecore_mdump_cmd_params mdump_cmd_params;

	OSAL_MEM_ZERO(&mdump_cmd_params, sizeof(mdump_cmd_params));
	mdump_cmd_params.cmd = DRV_MSG_CODE_MDUMP_CLEAR_LOGS;

	return ecore_mcp_mdump_cmd(p_hwfn, p_ptt, &mdump_cmd_params);
}

static void ecore_mcp_handle_critical_error(struct ecore_hwfn *p_hwfn,
					    struct ecore_ptt *p_ptt)
{
	/* In CMT mode - no need for more than a single acknowledgment to the
	 * MFW, and no more than a single notification to the upper driver.
	 */
	if (p_hwfn != ECORE_LEADING_HWFN(p_hwfn->p_dev))
		return;

	DP_NOTICE(p_hwfn, false,
		  "Received a critical error notification from the MFW!\n");

	if (p_hwfn->p_dev->allow_mdump) {
		DP_NOTICE(p_hwfn, false,
			  "Not acknowledging the notification to allow the MFW crash dump\n");
		return;
	}

	ecore_mcp_mdump_ack(p_hwfn, p_ptt);
	ecore_hw_err_notify(p_hwfn, ECORE_HW_ERR_HW_ATTN);
}

enum _ecore_status_t ecore_mcp_handle_events(struct ecore_hwfn *p_hwfn,
					     struct ecore_ptt *p_ptt)
{
	struct ecore_mcp_info *info = p_hwfn->mcp_info;
	enum _ecore_status_t rc = ECORE_SUCCESS;
	bool found = false;
	u16 i;

	DP_VERBOSE(p_hwfn, ECORE_MSG_SP, "Received message from MFW\n");

	/* Read Messages from MFW */
	ecore_mcp_read_mb(p_hwfn, p_ptt);

	/* Compare current messages to old ones */
	for (i = 0; i < info->mfw_mb_length; i++) {
		if (info->mfw_mb_cur[i] == info->mfw_mb_shadow[i])
			continue;

		found = true;

		DP_VERBOSE(p_hwfn, ECORE_MSG_LINK,
			   "Msg [%d] - old CMD 0x%02x, new CMD 0x%02x\n",
			   i, info->mfw_mb_shadow[i], info->mfw_mb_cur[i]);

		switch (i) {
		case MFW_DRV_MSG_LINK_CHANGE:
			ecore_mcp_handle_link_change(p_hwfn, p_ptt, false);
			break;
		case MFW_DRV_MSG_VF_DISABLED:
			ecore_mcp_handle_vf_flr(p_hwfn, p_ptt);
			break;
		case MFW_DRV_MSG_LLDP_DATA_UPDATED:
			ecore_dcbx_mib_update_event(p_hwfn, p_ptt,
						    ECORE_DCBX_REMOTE_LLDP_MIB);
			break;
		case MFW_DRV_MSG_DCBX_REMOTE_MIB_UPDATED:
			ecore_dcbx_mib_update_event(p_hwfn, p_ptt,
						    ECORE_DCBX_REMOTE_MIB);
			break;
		case MFW_DRV_MSG_DCBX_OPERATIONAL_MIB_UPDATED:
			ecore_dcbx_mib_update_event(p_hwfn, p_ptt,
						    ECORE_DCBX_OPERATIONAL_MIB);
			break;
		case MFW_DRV_MSG_TRANSCEIVER_STATE_CHANGE:
			ecore_mcp_handle_transceiver_change(p_hwfn, p_ptt);
			break;
		case MFW_DRV_MSG_ERROR_RECOVERY:
			ecore_mcp_handle_process_kill(p_hwfn, p_ptt);
			break;
		case MFW_DRV_MSG_GET_LAN_STATS:
		case MFW_DRV_MSG_GET_FCOE_STATS:
		case MFW_DRV_MSG_GET_ISCSI_STATS:
		case MFW_DRV_MSG_GET_RDMA_STATS:
			ecore_mcp_send_protocol_stats(p_hwfn, p_ptt, i);
			break;
		case MFW_DRV_MSG_BW_UPDATE:
			ecore_mcp_update_bw(p_hwfn, p_ptt);
			break;
		case MFW_DRV_MSG_FAILURE_DETECTED:
			ecore_mcp_handle_fan_failure(p_hwfn, p_ptt);
			break;
		case MFW_DRV_MSG_CRITICAL_ERROR_OCCURRED:
			ecore_mcp_handle_critical_error(p_hwfn, p_ptt);
			break;
		default:
			DP_INFO(p_hwfn, "Unimplemented MFW message %d\n", i);
			rc = ECORE_INVAL;
		}
	}

	/* ACK everything */
	for (i = 0; i < MFW_DRV_MSG_MAX_DWORDS(info->mfw_mb_length); i++) {
		OSAL_BE32 val = OSAL_CPU_TO_BE32(((u32 *)info->mfw_mb_cur)[i]);

		/* MFW expect answer in BE, so we force write in that format */
		ecore_wr(p_hwfn, p_ptt,
			 info->mfw_mb_addr + sizeof(u32) +
			 MFW_DRV_MSG_MAX_DWORDS(info->mfw_mb_length) *
			 sizeof(u32) + i * sizeof(u32), val);
	}

	if (!found) {
		DP_NOTICE(p_hwfn, false,
			  "Received an MFW message indication but no"
			  " new message!\n");
		rc = ECORE_INVAL;
	}

	/* Copy the new mfw messages into the shadow */
	OSAL_MEMCPY(info->mfw_mb_shadow, info->mfw_mb_cur, info->mfw_mb_length);

	return rc;
}

enum _ecore_status_t ecore_mcp_get_mfw_ver(struct ecore_hwfn *p_hwfn,
					   struct ecore_ptt *p_ptt,
					   u32 *p_mfw_ver,
					   u32 *p_running_bundle_id)
{
	u32 global_offsize;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->p_dev)) {
		DP_NOTICE(p_hwfn, false, "Emulation - can't get MFW version\n");
		return ECORE_SUCCESS;
	}
#endif

	if (IS_VF(p_hwfn->p_dev)) {
		if (p_hwfn->vf_iov_info) {
			struct pfvf_acquire_resp_tlv *p_resp;

			p_resp = &p_hwfn->vf_iov_info->acquire_resp;
			*p_mfw_ver = p_resp->pfdev_info.mfw_ver;
			return ECORE_SUCCESS;
		} else {
			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "VF requested MFW version prior to ACQUIRE\n");
			return ECORE_INVAL;
		}
	}

	global_offsize = ecore_rd(p_hwfn, p_ptt,
				  SECTION_OFFSIZE_ADDR(p_hwfn->mcp_info->
						       public_base,
						       PUBLIC_GLOBAL));
	*p_mfw_ver =
	    ecore_rd(p_hwfn, p_ptt,
		     SECTION_ADDR(global_offsize,
				  0) + OFFSETOF(struct public_global, mfw_ver));

	if (p_running_bundle_id != OSAL_NULL) {
		*p_running_bundle_id = ecore_rd(p_hwfn, p_ptt,
						SECTION_ADDR(global_offsize,
							     0) +
						OFFSETOF(struct public_global,
							 running_bundle_id));
	}

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_mcp_get_media_type(struct ecore_dev *p_dev,
					      u32 *p_media_type)
{
	struct ecore_hwfn *p_hwfn = &p_dev->hwfns[0];
	struct ecore_ptt *p_ptt;

	/* TODO - Add support for VFs */
	if (IS_VF(p_dev))
		return ECORE_INVAL;

	if (!ecore_mcp_is_init(p_hwfn)) {
		DP_NOTICE(p_hwfn, true, "MFW is not initialized !\n");
		return ECORE_BUSY;
	}

	*p_media_type = MEDIA_UNSPECIFIED;

	p_ptt = ecore_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return ECORE_BUSY;

	*p_media_type = ecore_rd(p_hwfn, p_ptt, p_hwfn->mcp_info->port_addr +
				 OFFSETOF(struct public_port, media_type));

	ecore_ptt_release(p_hwfn, p_ptt);

	return ECORE_SUCCESS;
}

/* @DPDK */
/* Old MFW has a global configuration for all PFs regarding RDMA support */
static void
ecore_mcp_get_shmem_proto_legacy(struct ecore_hwfn *p_hwfn,
				 enum ecore_pci_personality *p_proto)
{
	*p_proto = ECORE_PCI_ETH;

	DP_VERBOSE(p_hwfn, ECORE_MSG_IFUP,
		   "According to Legacy capabilities, L2 personality is %08x\n",
		   (u32)*p_proto);
}

/* @DPDK */
static enum _ecore_status_t
ecore_mcp_get_shmem_proto_mfw(struct ecore_hwfn *p_hwfn,
			      struct ecore_ptt *p_ptt,
			      enum ecore_pci_personality *p_proto)
{
	u32 resp = 0, param = 0;
	enum _ecore_status_t rc;

	DP_VERBOSE(p_hwfn, ECORE_MSG_IFUP,
		   "According to capabilities, L2 personality is %08x [resp %08x param %08x]\n",
		   (u32)*p_proto, resp, param);
	return ECORE_SUCCESS;
}

static enum _ecore_status_t
ecore_mcp_get_shmem_proto(struct ecore_hwfn *p_hwfn,
			  struct public_func *p_info,
			  struct ecore_ptt *p_ptt,
			  enum ecore_pci_personality *p_proto)
{
	enum _ecore_status_t rc = ECORE_SUCCESS;

	switch (p_info->config & FUNC_MF_CFG_PROTOCOL_MASK) {
	case FUNC_MF_CFG_PROTOCOL_ETHERNET:
		if (ecore_mcp_get_shmem_proto_mfw(p_hwfn, p_ptt, p_proto) !=
		    ECORE_SUCCESS)
			ecore_mcp_get_shmem_proto_legacy(p_hwfn, p_proto);
		break;
	default:
		rc = ECORE_INVAL;
	}

	return rc;
}

enum _ecore_status_t ecore_mcp_fill_shmem_func_info(struct ecore_hwfn *p_hwfn,
						    struct ecore_ptt *p_ptt)
{
	struct ecore_mcp_function_info *info;
	struct public_func shmem_info;

	ecore_mcp_get_shmem_func(p_hwfn, p_ptt, &shmem_info, MCP_PF_ID(p_hwfn));
	info = &p_hwfn->mcp_info->func_info;

	info->pause_on_host = (shmem_info.config &
			       FUNC_MF_CFG_PAUSE_ON_HOST_RING) ? 1 : 0;

	if (ecore_mcp_get_shmem_proto(p_hwfn, &shmem_info, p_ptt,
				      &info->protocol)) {
		DP_ERR(p_hwfn, "Unknown personality %08x\n",
		       (u32)(shmem_info.config & FUNC_MF_CFG_PROTOCOL_MASK));
		return ECORE_INVAL;
	}

	ecore_read_pf_bandwidth(p_hwfn, &shmem_info);

	if (shmem_info.mac_upper || shmem_info.mac_lower) {
		info->mac[0] = (u8)(shmem_info.mac_upper >> 8);
		info->mac[1] = (u8)(shmem_info.mac_upper);
		info->mac[2] = (u8)(shmem_info.mac_lower >> 24);
		info->mac[3] = (u8)(shmem_info.mac_lower >> 16);
		info->mac[4] = (u8)(shmem_info.mac_lower >> 8);
		info->mac[5] = (u8)(shmem_info.mac_lower);
	} else {
		/* TODO - are there protocols for which there's no MAC? */
		DP_NOTICE(p_hwfn, false, "MAC is 0 in shmem\n");
	}

	/* TODO - are these calculations true for BE machine? */
	info->wwn_port = (u64)shmem_info.fcoe_wwn_port_name_upper |
			 (((u64)shmem_info.fcoe_wwn_port_name_lower) << 32);
	info->wwn_node = (u64)shmem_info.fcoe_wwn_node_name_upper |
			 (((u64)shmem_info.fcoe_wwn_node_name_lower) << 32);

	info->ovlan = (u16)(shmem_info.ovlan_stag & FUNC_MF_CFG_OV_STAG_MASK);

	info->mtu = (u16)shmem_info.mtu_size;

	if (info->mtu == 0)
		info->mtu = 1500;

	info->mtu = (u16)shmem_info.mtu_size;

	DP_VERBOSE(p_hwfn, (ECORE_MSG_SP | ECORE_MSG_IFUP),
		   "Read configuration from shmem: pause_on_host %02x"
		    " protocol %02x BW [%02x - %02x]"
		    " MAC %02x:%02x:%02x:%02x:%02x:%02x wwn port %lx"
		    " node %lx ovlan %04x\n",
		   info->pause_on_host, info->protocol,
		   info->bandwidth_min, info->bandwidth_max,
		   info->mac[0], info->mac[1], info->mac[2],
		   info->mac[3], info->mac[4], info->mac[5],
		   (unsigned long)info->wwn_port,
		   (unsigned long)info->wwn_node, info->ovlan);

	return ECORE_SUCCESS;
}

struct ecore_mcp_link_params
*ecore_mcp_get_link_params(struct ecore_hwfn *p_hwfn)
{
	if (!p_hwfn || !p_hwfn->mcp_info)
		return OSAL_NULL;
	return &p_hwfn->mcp_info->link_input;
}

struct ecore_mcp_link_state
*ecore_mcp_get_link_state(struct ecore_hwfn *p_hwfn)
{
	if (!p_hwfn || !p_hwfn->mcp_info)
		return OSAL_NULL;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_SLOW(p_hwfn->p_dev)) {
		DP_INFO(p_hwfn, "Non-ASIC - always notify that link is up\n");
		p_hwfn->mcp_info->link_output.link_up = true;
	}
#endif

	return &p_hwfn->mcp_info->link_output;
}

struct ecore_mcp_link_capabilities
*ecore_mcp_get_link_capabilities(struct ecore_hwfn *p_hwfn)
{
	if (!p_hwfn || !p_hwfn->mcp_info)
		return OSAL_NULL;
	return &p_hwfn->mcp_info->link_capabilities;
}

enum _ecore_status_t ecore_mcp_drain(struct ecore_hwfn *p_hwfn,
				     struct ecore_ptt *p_ptt)
{
	u32 resp = 0, param = 0;
	enum _ecore_status_t rc;

	rc = ecore_mcp_cmd(p_hwfn, p_ptt,
			   DRV_MSG_CODE_NIG_DRAIN, 1000, &resp, &param);

	/* Wait for the drain to complete before returning */
	OSAL_MSLEEP(1020);

	return rc;
}

const struct ecore_mcp_function_info
*ecore_mcp_get_function_info(struct ecore_hwfn *p_hwfn)
{
	if (!p_hwfn || !p_hwfn->mcp_info)
		return OSAL_NULL;
	return &p_hwfn->mcp_info->func_info;
}

enum _ecore_status_t ecore_mcp_nvm_command(struct ecore_hwfn *p_hwfn,
					   struct ecore_ptt *p_ptt,
					   struct ecore_mcp_nvm_params *params)
{
	enum _ecore_status_t rc;

	switch (params->type) {
	case ECORE_MCP_NVM_RD:
		rc = ecore_mcp_nvm_rd_cmd(p_hwfn, p_ptt, params->nvm_common.cmd,
					  params->nvm_common.offset,
					  &params->nvm_common.resp,
					  &params->nvm_common.param,
					  params->nvm_rd.buf_size,
					  params->nvm_rd.buf);
		break;
	case ECORE_MCP_CMD:
		rc = ecore_mcp_cmd(p_hwfn, p_ptt, params->nvm_common.cmd,
				   params->nvm_common.offset,
				   &params->nvm_common.resp,
				   &params->nvm_common.param);
		break;
	case ECORE_MCP_NVM_WR:
		rc = ecore_mcp_nvm_wr_cmd(p_hwfn, p_ptt, params->nvm_common.cmd,
					  params->nvm_common.offset,
					  &params->nvm_common.resp,
					  &params->nvm_common.param,
					  params->nvm_wr.buf_size,
					  params->nvm_wr.buf);
		break;
	default:
		rc = ECORE_NOTIMPL;
		break;
	}
	return rc;
}

int ecore_mcp_get_personality_cnt(struct ecore_hwfn *p_hwfn,
				  struct ecore_ptt *p_ptt, u32 personalities)
{
	enum ecore_pci_personality protocol = ECORE_PCI_DEFAULT;
	struct public_func shmem_info;
	int i, count = 0, num_pfs;

	num_pfs = NUM_OF_ENG_PFS(p_hwfn->p_dev);

	for (i = 0; i < num_pfs; i++) {
		ecore_mcp_get_shmem_func(p_hwfn, p_ptt, &shmem_info,
					 MCP_PF_ID_BY_REL(p_hwfn, i));
		if (shmem_info.config & FUNC_MF_CFG_FUNC_HIDE)
			continue;

		if (ecore_mcp_get_shmem_proto(p_hwfn, &shmem_info, p_ptt,
					      &protocol) !=
		    ECORE_SUCCESS)
			continue;

		if ((1 << ((u32)protocol)) & personalities)
			count++;
	}

	return count;
}

enum _ecore_status_t ecore_mcp_get_flash_size(struct ecore_hwfn *p_hwfn,
					      struct ecore_ptt *p_ptt,
					      u32 *p_flash_size)
{
	u32 flash_size;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_EMUL(p_hwfn->p_dev)) {
		DP_NOTICE(p_hwfn, false, "Emulation - can't get flash size\n");
		return ECORE_INVAL;
	}
#endif

	if (IS_VF(p_hwfn->p_dev))
		return ECORE_INVAL;

	flash_size = ecore_rd(p_hwfn, p_ptt, MCP_REG_NVM_CFG4);
	flash_size = (flash_size & MCP_REG_NVM_CFG4_FLASH_SIZE) >>
	    MCP_REG_NVM_CFG4_FLASH_SIZE_SHIFT;
	flash_size = (1 << (flash_size + MCP_BYTES_PER_MBIT_SHIFT));

	*p_flash_size = flash_size;

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_start_recovery_process(struct ecore_hwfn *p_hwfn,
						  struct ecore_ptt *p_ptt)
{
	struct ecore_dev *p_dev = p_hwfn->p_dev;

	if (p_dev->recov_in_prog) {
		DP_NOTICE(p_hwfn, false,
			  "Avoid triggering a recovery since such a process"
			  " is already in progress\n");
		return ECORE_AGAIN;
	}

	DP_NOTICE(p_hwfn, false, "Triggering a recovery process\n");
	ecore_wr(p_hwfn, p_ptt, MISC_REG_AEU_GENERAL_ATTN_35, 0x1);

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_mcp_config_vf_msix(struct ecore_hwfn *p_hwfn,
					      struct ecore_ptt *p_ptt,
					      u8 vf_id, u8 num)
{
	u32 resp = 0, param = 0, rc_param = 0;
	enum _ecore_status_t rc;

/* Only Leader can configure MSIX, and need to take CMT into account */

	if (!IS_LEAD_HWFN(p_hwfn))
		return ECORE_SUCCESS;
	num *= p_hwfn->p_dev->num_hwfns;

	param |= (vf_id << DRV_MB_PARAM_CFG_VF_MSIX_VF_ID_SHIFT) &
	    DRV_MB_PARAM_CFG_VF_MSIX_VF_ID_MASK;
	param |= (num << DRV_MB_PARAM_CFG_VF_MSIX_SB_NUM_SHIFT) &
	    DRV_MB_PARAM_CFG_VF_MSIX_SB_NUM_MASK;

	rc = ecore_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_CFG_VF_MSIX, param,
			   &resp, &rc_param);

	if (resp != FW_MSG_CODE_DRV_CFG_VF_MSIX_DONE) {
		DP_NOTICE(p_hwfn, true, "VF[%d]: MFW failed to set MSI-X\n",
			  vf_id);
		rc = ECORE_INVAL;
	} else {
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "Requested 0x%02x MSI-x interrupts from VF 0x%02x\n",
			    num, vf_id);
	}

	return rc;
}

enum _ecore_status_t
ecore_mcp_send_drv_version(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt,
			   struct ecore_mcp_drv_version *p_ver)
{
	struct ecore_mcp_mb_params mb_params;
	struct drv_version_stc drv_version;
	u32 num_words, i;
	void *p_name;
	OSAL_BE32 val;
	enum _ecore_status_t rc;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_SLOW(p_hwfn->p_dev))
		return ECORE_SUCCESS;
#endif

	OSAL_MEM_ZERO(&drv_version, sizeof(drv_version));
	drv_version.version = p_ver->version;
	num_words = (MCP_DRV_VER_STR_SIZE - 4) / 4;
	for (i = 0; i < num_words; i++) {
		/* The driver name is expected to be in a big-endian format */
		p_name = &p_ver->name[i * sizeof(u32)];
		val = OSAL_CPU_TO_BE32(*(u32 *)p_name);
		*(u32 *)&drv_version.name[i * sizeof(u32)] = val;
	}

	OSAL_MEM_ZERO(&mb_params, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_SET_VERSION;
	mb_params.p_data_src = &drv_version;
	mb_params.data_src_size = sizeof(drv_version);
	rc = ecore_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc != ECORE_SUCCESS)
		DP_ERR(p_hwfn, "MCP response failure, aborting\n");

	return rc;
}

enum _ecore_status_t ecore_mcp_halt(struct ecore_hwfn *p_hwfn,
				    struct ecore_ptt *p_ptt)
{
	enum _ecore_status_t rc;
	u32 resp = 0, param = 0;

	rc = ecore_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_MCP_HALT, 0, &resp,
			   &param);
	if (rc != ECORE_SUCCESS)
		DP_ERR(p_hwfn, "MCP response failure, aborting\n");

	return rc;
}

enum _ecore_status_t ecore_mcp_resume(struct ecore_hwfn *p_hwfn,
				      struct ecore_ptt *p_ptt)
{
	u32 value, cpu_mode;

	ecore_wr(p_hwfn, p_ptt, MCP_REG_CPU_STATE, 0xffffffff);

	value = ecore_rd(p_hwfn, p_ptt, MCP_REG_CPU_MODE);
	value &= ~MCP_REG_CPU_MODE_SOFT_HALT;
	ecore_wr(p_hwfn, p_ptt, MCP_REG_CPU_MODE, value);
	cpu_mode = ecore_rd(p_hwfn, p_ptt, MCP_REG_CPU_MODE);

	return (cpu_mode & MCP_REG_CPU_MODE_SOFT_HALT) ? -1 : 0;
}

enum _ecore_status_t
ecore_mcp_ov_update_current_config(struct ecore_hwfn *p_hwfn,
				   struct ecore_ptt *p_ptt,
				   enum ecore_ov_client client)
{
	enum _ecore_status_t rc;
	u32 resp = 0, param = 0;
	u32 drv_mb_param;

	switch (client) {
	case ECORE_OV_CLIENT_DRV:
		drv_mb_param = DRV_MB_PARAM_OV_CURR_CFG_OS;
		break;
	case ECORE_OV_CLIENT_USER:
		drv_mb_param = DRV_MB_PARAM_OV_CURR_CFG_OTHER;
		break;
	case ECORE_OV_CLIENT_VENDOR_SPEC:
		drv_mb_param = DRV_MB_PARAM_OV_CURR_CFG_VENDOR_SPEC;
		break;
	default:
		DP_NOTICE(p_hwfn, true, "Invalid client type %d\n", client);
		return ECORE_INVAL;
	}

	rc = ecore_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_OV_UPDATE_CURR_CFG,
			   drv_mb_param, &resp, &param);
	if (rc != ECORE_SUCCESS)
		DP_ERR(p_hwfn, "MCP response failure, aborting\n");

	return rc;
}

enum _ecore_status_t
ecore_mcp_ov_update_driver_state(struct ecore_hwfn *p_hwfn,
				 struct ecore_ptt *p_ptt,
				 enum ecore_ov_driver_state drv_state)
{
	enum _ecore_status_t rc;
	u32 resp = 0, param = 0;
	u32 drv_mb_param;

	switch (drv_state) {
	case ECORE_OV_DRIVER_STATE_NOT_LOADED:
		drv_mb_param = DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_NOT_LOADED;
		break;
	case ECORE_OV_DRIVER_STATE_DISABLED:
		drv_mb_param = DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_DISABLED;
		break;
	case ECORE_OV_DRIVER_STATE_ACTIVE:
		drv_mb_param = DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_ACTIVE;
		break;
	default:
		DP_NOTICE(p_hwfn, true, "Invalid driver state %d\n", drv_state);
		return ECORE_INVAL;
	}

	rc = ecore_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE,
			   drv_mb_param, &resp, &param);
	if (rc != ECORE_SUCCESS)
		DP_ERR(p_hwfn, "Failed to send driver state\n");

	return rc;
}

enum _ecore_status_t
ecore_mcp_ov_get_fc_npiv(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt,
			 struct ecore_fc_npiv_tbl *p_table)
{
	return 0;
}

enum _ecore_status_t
ecore_mcp_ov_update_mtu(struct ecore_hwfn *p_hwfn,
			struct ecore_ptt *p_ptt, u16 mtu)
{
	return 0;
}

enum _ecore_status_t ecore_mcp_set_led(struct ecore_hwfn *p_hwfn,
				       struct ecore_ptt *p_ptt,
				       enum ecore_led_mode mode)
{
	u32 resp = 0, param = 0, drv_mb_param;
	enum _ecore_status_t rc;

	switch (mode) {
	case ECORE_LED_MODE_ON:
		drv_mb_param = DRV_MB_PARAM_SET_LED_MODE_ON;
		break;
	case ECORE_LED_MODE_OFF:
		drv_mb_param = DRV_MB_PARAM_SET_LED_MODE_OFF;
		break;
	case ECORE_LED_MODE_RESTORE:
		drv_mb_param = DRV_MB_PARAM_SET_LED_MODE_OPER;
		break;
	default:
		DP_NOTICE(p_hwfn, true, "Invalid LED mode %d\n", mode);
		return ECORE_INVAL;
	}

	rc = ecore_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_SET_LED_MODE,
			   drv_mb_param, &resp, &param);
	if (rc != ECORE_SUCCESS)
		DP_ERR(p_hwfn, "MCP response failure, aborting\n");

	return rc;
}

enum _ecore_status_t ecore_mcp_mask_parities(struct ecore_hwfn *p_hwfn,
					     struct ecore_ptt *p_ptt,
					     u32 mask_parities)
{
	enum _ecore_status_t rc;
	u32 resp = 0, param = 0;

	rc = ecore_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_MASK_PARITIES,
			   mask_parities, &resp, &param);

	if (rc != ECORE_SUCCESS) {
		DP_ERR(p_hwfn,
		       "MCP response failure for mask parities, aborting\n");
	} else if (resp != FW_MSG_CODE_OK) {
		DP_ERR(p_hwfn,
		       "MCP did not ack mask parity request. Old MFW?\n");
		rc = ECORE_INVAL;
	}

	return rc;
}

enum _ecore_status_t ecore_mcp_nvm_read(struct ecore_dev *p_dev, u32 addr,
					u8 *p_buf, u32 len)
{
	struct ecore_hwfn *p_hwfn = ECORE_LEADING_HWFN(p_dev);
	u32 bytes_left, offset, bytes_to_copy, buf_size;
	struct ecore_mcp_nvm_params params;
	struct ecore_ptt *p_ptt;
	enum _ecore_status_t rc = ECORE_SUCCESS;

	p_ptt = ecore_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return ECORE_BUSY;

	OSAL_MEMSET(&params, 0, sizeof(struct ecore_mcp_nvm_params));
	bytes_left = len;
	offset = 0;
	params.type = ECORE_MCP_NVM_RD;
	params.nvm_rd.buf_size = &buf_size;
	params.nvm_common.cmd = DRV_MSG_CODE_NVM_READ_NVRAM;
	while (bytes_left > 0) {
		bytes_to_copy = OSAL_MIN_T(u32, bytes_left,
					   MCP_DRV_NVM_BUF_LEN);
		params.nvm_common.offset = (addr + offset) |
		    (bytes_to_copy << DRV_MB_PARAM_NVM_LEN_SHIFT);
		params.nvm_rd.buf = (u32 *)(p_buf + offset);
		rc = ecore_mcp_nvm_command(p_hwfn, p_ptt, &params);
		if (rc != ECORE_SUCCESS || (params.nvm_common.resp !=
					    FW_MSG_CODE_NVM_OK)) {
			DP_NOTICE(p_dev, false, "MCP command rc = %d\n", rc);
			break;
		}

		/* This can be a lengthy process, and it's possible scheduler
		 * isn't preemptible. Sleep a bit to prevent CPU hogging.
		 */
		if (bytes_left % 0x1000 <
		    (bytes_left - *params.nvm_rd.buf_size) % 0x1000)
			OSAL_MSLEEP(1);

		offset += *params.nvm_rd.buf_size;
		bytes_left -= *params.nvm_rd.buf_size;
	}

	p_dev->mcp_nvm_resp = params.nvm_common.resp;
	ecore_ptt_release(p_hwfn, p_ptt);

	return rc;
}

enum _ecore_status_t ecore_mcp_phy_read(struct ecore_dev *p_dev, u32 cmd,
					u32 addr, u8 *p_buf, u32 len)
{
	struct ecore_hwfn *p_hwfn = ECORE_LEADING_HWFN(p_dev);
	struct ecore_mcp_nvm_params params;
	struct ecore_ptt *p_ptt;
	enum _ecore_status_t rc;

	p_ptt = ecore_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return ECORE_BUSY;

	OSAL_MEMSET(&params, 0, sizeof(struct ecore_mcp_nvm_params));
	params.type = ECORE_MCP_NVM_RD;
	params.nvm_rd.buf_size = &len;
	params.nvm_common.cmd = (cmd == ECORE_PHY_CORE_READ) ?
	    DRV_MSG_CODE_PHY_CORE_READ : DRV_MSG_CODE_PHY_RAW_READ;
	params.nvm_common.offset = addr;
	params.nvm_rd.buf = (u32 *)p_buf;
	rc = ecore_mcp_nvm_command(p_hwfn, p_ptt, &params);
	if (rc != ECORE_SUCCESS)
		DP_NOTICE(p_dev, false, "MCP command rc = %d\n", rc);

	p_dev->mcp_nvm_resp = params.nvm_common.resp;
	ecore_ptt_release(p_hwfn, p_ptt);

	return rc;
}

enum _ecore_status_t ecore_mcp_nvm_resp(struct ecore_dev *p_dev, u8 *p_buf)
{
	struct ecore_hwfn *p_hwfn = ECORE_LEADING_HWFN(p_dev);
	struct ecore_mcp_nvm_params params;
	struct ecore_ptt *p_ptt;

	p_ptt = ecore_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return ECORE_BUSY;

	OSAL_MEMSET(&params, 0, sizeof(struct ecore_mcp_nvm_params));
	OSAL_MEMCPY(p_buf, &p_dev->mcp_nvm_resp, sizeof(p_dev->mcp_nvm_resp));
	ecore_ptt_release(p_hwfn, p_ptt);

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_mcp_nvm_del_file(struct ecore_dev *p_dev, u32 addr)
{
	struct ecore_hwfn *p_hwfn = ECORE_LEADING_HWFN(p_dev);
	struct ecore_mcp_nvm_params params;
	struct ecore_ptt *p_ptt;
	enum _ecore_status_t rc;

	p_ptt = ecore_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return ECORE_BUSY;
	OSAL_MEMSET(&params, 0, sizeof(struct ecore_mcp_nvm_params));
	params.type = ECORE_MCP_CMD;
	params.nvm_common.cmd = DRV_MSG_CODE_NVM_DEL_FILE;
	params.nvm_common.offset = addr;
	rc = ecore_mcp_nvm_command(p_hwfn, p_ptt, &params);
	p_dev->mcp_nvm_resp = params.nvm_common.resp;
	ecore_ptt_release(p_hwfn, p_ptt);

	return rc;
}

enum _ecore_status_t ecore_mcp_nvm_put_file_begin(struct ecore_dev *p_dev,
						  u32 addr)
{
	struct ecore_hwfn *p_hwfn = ECORE_LEADING_HWFN(p_dev);
	struct ecore_mcp_nvm_params params;
	struct ecore_ptt *p_ptt;
	enum _ecore_status_t rc;

	p_ptt = ecore_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return ECORE_BUSY;
	OSAL_MEMSET(&params, 0, sizeof(struct ecore_mcp_nvm_params));
	params.type = ECORE_MCP_CMD;
	params.nvm_common.cmd = DRV_MSG_CODE_NVM_PUT_FILE_BEGIN;
	params.nvm_common.offset = addr;
	rc = ecore_mcp_nvm_command(p_hwfn, p_ptt, &params);
	p_dev->mcp_nvm_resp = params.nvm_common.resp;
	ecore_ptt_release(p_hwfn, p_ptt);

	return rc;
}

/* rc receives ECORE_INVAL as default parameter because
 * it might not enter the while loop if the len is 0
 */
enum _ecore_status_t ecore_mcp_nvm_write(struct ecore_dev *p_dev, u32 cmd,
					 u32 addr, u8 *p_buf, u32 len)
{
	struct ecore_hwfn *p_hwfn = ECORE_LEADING_HWFN(p_dev);
	enum _ecore_status_t rc = ECORE_INVAL;
	struct ecore_mcp_nvm_params params;
	struct ecore_ptt *p_ptt;
	u32 buf_idx, buf_size;

	p_ptt = ecore_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return ECORE_BUSY;

	OSAL_MEMSET(&params, 0, sizeof(struct ecore_mcp_nvm_params));
	params.type = ECORE_MCP_NVM_WR;
	if (cmd == ECORE_PUT_FILE_DATA)
		params.nvm_common.cmd = DRV_MSG_CODE_NVM_PUT_FILE_DATA;
	else
		params.nvm_common.cmd = DRV_MSG_CODE_NVM_WRITE_NVRAM;
	buf_idx = 0;
	while (buf_idx < len) {
		buf_size = OSAL_MIN_T(u32, (len - buf_idx),
				      MCP_DRV_NVM_BUF_LEN);
		params.nvm_common.offset = ((buf_size <<
					     DRV_MB_PARAM_NVM_LEN_SHIFT)
					    | addr) + buf_idx;
		params.nvm_wr.buf_size = buf_size;
		params.nvm_wr.buf = (u32 *)&p_buf[buf_idx];
		rc = ecore_mcp_nvm_command(p_hwfn, p_ptt, &params);
		if (rc != ECORE_SUCCESS ||
		    ((params.nvm_common.resp != FW_MSG_CODE_NVM_OK) &&
		     (params.nvm_common.resp !=
		      FW_MSG_CODE_NVM_PUT_FILE_FINISH_OK)))
			DP_NOTICE(p_dev, false, "MCP command rc = %d\n", rc);

		/* This can be a lengthy process, and it's possible scheduler
		 * isn't preemptible. Sleep a bit to prevent CPU hogging.
		 */
		if (buf_idx % 0x1000 >
		    (buf_idx + buf_size) % 0x1000)
			OSAL_MSLEEP(1);

		buf_idx += buf_size;
	}

	p_dev->mcp_nvm_resp = params.nvm_common.resp;
	ecore_ptt_release(p_hwfn, p_ptt);

	return rc;
}

enum _ecore_status_t ecore_mcp_phy_write(struct ecore_dev *p_dev, u32 cmd,
					 u32 addr, u8 *p_buf, u32 len)
{
	struct ecore_hwfn *p_hwfn = ECORE_LEADING_HWFN(p_dev);
	struct ecore_mcp_nvm_params params;
	struct ecore_ptt *p_ptt;
	enum _ecore_status_t rc;

	p_ptt = ecore_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return ECORE_BUSY;

	OSAL_MEMSET(&params, 0, sizeof(struct ecore_mcp_nvm_params));
	params.type = ECORE_MCP_NVM_WR;
	params.nvm_wr.buf_size = len;
	params.nvm_common.cmd = (cmd == ECORE_PHY_CORE_WRITE) ?
	    DRV_MSG_CODE_PHY_CORE_WRITE : DRV_MSG_CODE_PHY_RAW_WRITE;
	params.nvm_common.offset = addr;
	params.nvm_wr.buf = (u32 *)p_buf;
	rc = ecore_mcp_nvm_command(p_hwfn, p_ptt, &params);
	if (rc != ECORE_SUCCESS)
		DP_NOTICE(p_dev, false, "MCP command rc = %d\n", rc);
	p_dev->mcp_nvm_resp = params.nvm_common.resp;
	ecore_ptt_release(p_hwfn, p_ptt);

	return rc;
}

enum _ecore_status_t ecore_mcp_nvm_set_secure_mode(struct ecore_dev *p_dev,
						   u32 addr)
{
	struct ecore_hwfn *p_hwfn = ECORE_LEADING_HWFN(p_dev);
	struct ecore_mcp_nvm_params params;
	struct ecore_ptt *p_ptt;
	enum _ecore_status_t rc;

	p_ptt = ecore_ptt_acquire(p_hwfn);
	if (!p_ptt)
		return ECORE_BUSY;

	OSAL_MEMSET(&params, 0, sizeof(struct ecore_mcp_nvm_params));
	params.type = ECORE_MCP_CMD;
	params.nvm_common.cmd = DRV_MSG_CODE_SET_SECURE_MODE;
	params.nvm_common.offset = addr;
	rc = ecore_mcp_nvm_command(p_hwfn, p_ptt, &params);
	p_dev->mcp_nvm_resp = params.nvm_common.resp;
	ecore_ptt_release(p_hwfn, p_ptt);

	return rc;
}

enum _ecore_status_t ecore_mcp_phy_sfp_read(struct ecore_hwfn *p_hwfn,
					    struct ecore_ptt *p_ptt,
					    u32 port, u32 addr, u32 offset,
					    u32 len, u8 *p_buf)
{
	struct ecore_mcp_nvm_params params;
	enum _ecore_status_t rc;
	u32 bytes_left, bytes_to_copy, buf_size;

	OSAL_MEMSET(&params, 0, sizeof(struct ecore_mcp_nvm_params));
	params.nvm_common.offset =
		(port << DRV_MB_PARAM_TRANSCEIVER_PORT_SHIFT) |
		(addr << DRV_MB_PARAM_TRANSCEIVER_I2C_ADDRESS_SHIFT);
	addr = offset;
	offset = 0;
	bytes_left = len;
	params.type = ECORE_MCP_NVM_RD;
	params.nvm_rd.buf_size = &buf_size;
	params.nvm_common.cmd = DRV_MSG_CODE_TRANSCEIVER_READ;
	while (bytes_left > 0) {
		bytes_to_copy = OSAL_MIN_T(u32, bytes_left,
					   MAX_I2C_TRANSACTION_SIZE);
		params.nvm_rd.buf = (u32 *)(p_buf + offset);
		params.nvm_common.offset &=
			(DRV_MB_PARAM_TRANSCEIVER_I2C_ADDRESS_MASK |
			 DRV_MB_PARAM_TRANSCEIVER_PORT_MASK);
		params.nvm_common.offset |=
			((addr + offset) <<
			 DRV_MB_PARAM_TRANSCEIVER_OFFSET_SHIFT);
		params.nvm_common.offset |=
			(bytes_to_copy << DRV_MB_PARAM_TRANSCEIVER_SIZE_SHIFT);
		rc = ecore_mcp_nvm_command(p_hwfn, p_ptt, &params);
		if ((params.nvm_common.resp & FW_MSG_CODE_MASK) ==
		    FW_MSG_CODE_TRANSCEIVER_NOT_PRESENT) {
			return ECORE_NODEV;
		} else if ((params.nvm_common.resp & FW_MSG_CODE_MASK) !=
			   FW_MSG_CODE_TRANSCEIVER_DIAG_OK)
			return ECORE_UNKNOWN_ERROR;

		offset += *params.nvm_rd.buf_size;
		bytes_left -= *params.nvm_rd.buf_size;
	}

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_mcp_phy_sfp_write(struct ecore_hwfn *p_hwfn,
					     struct ecore_ptt *p_ptt,
					     u32 port, u32 addr, u32 offset,
					     u32 len, u8 *p_buf)
{
	struct ecore_mcp_nvm_params params;
	enum _ecore_status_t rc;
	u32 buf_idx, buf_size;

	OSAL_MEMSET(&params, 0, sizeof(struct ecore_mcp_nvm_params));
	params.nvm_common.offset =
		(port << DRV_MB_PARAM_TRANSCEIVER_PORT_SHIFT) |
		(addr << DRV_MB_PARAM_TRANSCEIVER_I2C_ADDRESS_SHIFT);
	params.type = ECORE_MCP_NVM_WR;
	params.nvm_common.cmd = DRV_MSG_CODE_TRANSCEIVER_WRITE;
	buf_idx = 0;
	while (buf_idx < len) {
		buf_size = OSAL_MIN_T(u32, (len - buf_idx),
				      MAX_I2C_TRANSACTION_SIZE);
		params.nvm_common.offset &=
			(DRV_MB_PARAM_TRANSCEIVER_I2C_ADDRESS_MASK |
			 DRV_MB_PARAM_TRANSCEIVER_PORT_MASK);
		params.nvm_common.offset |=
			((offset + buf_idx) <<
			 DRV_MB_PARAM_TRANSCEIVER_OFFSET_SHIFT);
		params.nvm_common.offset |=
			(buf_size << DRV_MB_PARAM_TRANSCEIVER_SIZE_SHIFT);
		params.nvm_wr.buf_size = buf_size;
		params.nvm_wr.buf = (u32 *)&p_buf[buf_idx];
		rc = ecore_mcp_nvm_command(p_hwfn, p_ptt, &params);
		if ((params.nvm_common.resp & FW_MSG_CODE_MASK) ==
		    FW_MSG_CODE_TRANSCEIVER_NOT_PRESENT) {
			return ECORE_NODEV;
		} else if ((params.nvm_common.resp & FW_MSG_CODE_MASK) !=
			   FW_MSG_CODE_TRANSCEIVER_DIAG_OK)
			return ECORE_UNKNOWN_ERROR;

		buf_idx += buf_size;
	}

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_mcp_gpio_read(struct ecore_hwfn *p_hwfn,
					 struct ecore_ptt *p_ptt,
					 u16 gpio, u32 *gpio_val)
{
	enum _ecore_status_t rc = ECORE_SUCCESS;
	u32 drv_mb_param = 0, rsp;

	drv_mb_param = (gpio << DRV_MB_PARAM_GPIO_NUMBER_SHIFT);

	rc = ecore_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_GPIO_READ,
			   drv_mb_param, &rsp, gpio_val);

	if (rc != ECORE_SUCCESS)
		return rc;

	if ((rsp & FW_MSG_CODE_MASK) != FW_MSG_CODE_GPIO_OK)
		return ECORE_UNKNOWN_ERROR;

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_mcp_gpio_write(struct ecore_hwfn *p_hwfn,
					  struct ecore_ptt *p_ptt,
					  u16 gpio, u16 gpio_val)
{
	enum _ecore_status_t rc = ECORE_SUCCESS;
	u32 drv_mb_param = 0, param, rsp;

	drv_mb_param = (gpio << DRV_MB_PARAM_GPIO_NUMBER_SHIFT) |
		(gpio_val << DRV_MB_PARAM_GPIO_VALUE_SHIFT);

	rc = ecore_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_GPIO_WRITE,
			   drv_mb_param, &rsp, &param);

	if (rc != ECORE_SUCCESS)
		return rc;

	if ((rsp & FW_MSG_CODE_MASK) != FW_MSG_CODE_GPIO_OK)
		return ECORE_UNKNOWN_ERROR;

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_mcp_gpio_info(struct ecore_hwfn *p_hwfn,
					 struct ecore_ptt *p_ptt,
					 u16 gpio, u32 *gpio_direction,
					 u32 *gpio_ctrl)
{
	u32 drv_mb_param = 0, rsp, val = 0;
	enum _ecore_status_t rc = ECORE_SUCCESS;

	drv_mb_param = gpio << DRV_MB_PARAM_GPIO_NUMBER_SHIFT;

	rc = ecore_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_GPIO_INFO,
			   drv_mb_param, &rsp, &val);
	if (rc != ECORE_SUCCESS)
		return rc;

	*gpio_direction = (val & DRV_MB_PARAM_GPIO_DIRECTION_MASK) >>
			   DRV_MB_PARAM_GPIO_DIRECTION_SHIFT;
	*gpio_ctrl = (val & DRV_MB_PARAM_GPIO_CTRL_MASK) >>
		      DRV_MB_PARAM_GPIO_CTRL_SHIFT;

	if ((rsp & FW_MSG_CODE_MASK) != FW_MSG_CODE_GPIO_OK)
		return ECORE_UNKNOWN_ERROR;

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_mcp_bist_register_test(struct ecore_hwfn *p_hwfn,
						  struct ecore_ptt *p_ptt)
{
	u32 drv_mb_param = 0, rsp, param;
	enum _ecore_status_t rc = ECORE_SUCCESS;

	drv_mb_param = (DRV_MB_PARAM_BIST_REGISTER_TEST <<
			DRV_MB_PARAM_BIST_TEST_INDEX_SHIFT);

	rc = ecore_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_BIST_TEST,
			   drv_mb_param, &rsp, &param);

	if (rc != ECORE_SUCCESS)
		return rc;

	if (((rsp & FW_MSG_CODE_MASK) != FW_MSG_CODE_OK) ||
	    (param != DRV_MB_PARAM_BIST_RC_PASSED))
		rc = ECORE_UNKNOWN_ERROR;

	return rc;
}

enum _ecore_status_t ecore_mcp_bist_clock_test(struct ecore_hwfn *p_hwfn,
					       struct ecore_ptt *p_ptt)
{
	u32 drv_mb_param, rsp, param;
	enum _ecore_status_t rc = ECORE_SUCCESS;

	drv_mb_param = (DRV_MB_PARAM_BIST_CLOCK_TEST <<
			DRV_MB_PARAM_BIST_TEST_INDEX_SHIFT);

	rc = ecore_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_BIST_TEST,
			   drv_mb_param, &rsp, &param);

	if (rc != ECORE_SUCCESS)
		return rc;

	if (((rsp & FW_MSG_CODE_MASK) != FW_MSG_CODE_OK) ||
	    (param != DRV_MB_PARAM_BIST_RC_PASSED))
		rc = ECORE_UNKNOWN_ERROR;

	return rc;
}

enum _ecore_status_t ecore_mcp_bist_nvm_test_get_num_images(
	struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt, u32 *num_images)
{
	u32 drv_mb_param = 0, rsp;
	enum _ecore_status_t rc = ECORE_SUCCESS;

	drv_mb_param = (DRV_MB_PARAM_BIST_NVM_TEST_NUM_IMAGES <<
			DRV_MB_PARAM_BIST_TEST_INDEX_SHIFT);

	rc = ecore_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_BIST_TEST,
			   drv_mb_param, &rsp, num_images);

	if (rc != ECORE_SUCCESS)
		return rc;

	if (((rsp & FW_MSG_CODE_MASK) != FW_MSG_CODE_OK))
		rc = ECORE_UNKNOWN_ERROR;

	return rc;
}

enum _ecore_status_t ecore_mcp_bist_nvm_test_get_image_att(
	struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt,
	struct bist_nvm_image_att *p_image_att, u32 image_index)
{
	struct ecore_mcp_nvm_params params;
	enum _ecore_status_t rc;
	u32 buf_size;

	OSAL_MEMSET(&params, 0, sizeof(struct ecore_mcp_nvm_params));
	params.nvm_common.offset = (DRV_MB_PARAM_BIST_NVM_TEST_IMAGE_BY_INDEX <<
				    DRV_MB_PARAM_BIST_TEST_INDEX_SHIFT);
	params.nvm_common.offset |= (image_index <<
				    DRV_MB_PARAM_BIST_TEST_IMAGE_INDEX_SHIFT);

	params.type = ECORE_MCP_NVM_RD;
	params.nvm_rd.buf_size = &buf_size;
	params.nvm_common.cmd = DRV_MSG_CODE_BIST_TEST;
	params.nvm_rd.buf = (u32 *)p_image_att;

	rc = ecore_mcp_nvm_command(p_hwfn, p_ptt, &params);
	if (rc != ECORE_SUCCESS)
		return rc;

	if (((params.nvm_common.resp & FW_MSG_CODE_MASK) != FW_MSG_CODE_OK) ||
	    (p_image_att->return_code != 1))
		rc = ECORE_UNKNOWN_ERROR;

	return rc;
}

enum _ecore_status_t
ecore_mcp_get_temperature_info(struct ecore_hwfn *p_hwfn,
			       struct ecore_ptt *p_ptt,
			       struct ecore_temperature_info *p_temp_info)
{
	struct ecore_temperature_sensor *p_temp_sensor;
	struct temperature_status_stc mfw_temp_info;
	struct ecore_mcp_mb_params mb_params;
	u32 val;
	enum _ecore_status_t rc;
	u8 i;

	OSAL_MEM_ZERO(&mb_params, sizeof(mb_params));
	mb_params.cmd = DRV_MSG_CODE_GET_TEMPERATURE;
	mb_params.p_data_dst = &mfw_temp_info;
	mb_params.data_dst_size = sizeof(mfw_temp_info);
	rc = ecore_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc != ECORE_SUCCESS)
		return rc;

	OSAL_BUILD_BUG_ON(ECORE_MAX_NUM_OF_SENSORS != MAX_NUM_OF_SENSORS);
	p_temp_info->num_sensors = OSAL_MIN_T(u32, mfw_temp_info.num_of_sensors,
					      ECORE_MAX_NUM_OF_SENSORS);
	for (i = 0; i < p_temp_info->num_sensors; i++) {
		val = mfw_temp_info.sensor[i];
		p_temp_sensor = &p_temp_info->sensors[i];
		p_temp_sensor->sensor_location = (val & SENSOR_LOCATION_MASK) >>
						 SENSOR_LOCATION_SHIFT;
		p_temp_sensor->threshold_high = (val & THRESHOLD_HIGH_MASK) >>
						THRESHOLD_HIGH_SHIFT;
		p_temp_sensor->critical = (val & CRITICAL_TEMPERATURE_MASK) >>
					  CRITICAL_TEMPERATURE_SHIFT;
		p_temp_sensor->current_temp = (val & CURRENT_TEMP_MASK) >>
					      CURRENT_TEMP_SHIFT;
	}

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_mcp_get_mba_versions(
	struct ecore_hwfn *p_hwfn,
	struct ecore_ptt *p_ptt,
	struct ecore_mba_vers *p_mba_vers)
{
	struct ecore_mcp_nvm_params params;
	enum _ecore_status_t rc;
	u32 buf_size;

	OSAL_MEM_ZERO(&params, sizeof(params));
	params.type = ECORE_MCP_NVM_RD;
	params.nvm_common.cmd = DRV_MSG_CODE_GET_MBA_VERSION;
	params.nvm_common.offset = 0;
	params.nvm_rd.buf = &p_mba_vers->mba_vers[0];
	params.nvm_rd.buf_size = &buf_size;
	rc = ecore_mcp_nvm_command(p_hwfn, p_ptt, &params);

	if (rc != ECORE_SUCCESS)
		return rc;

	if ((params.nvm_common.resp & FW_MSG_CODE_MASK) !=
	    FW_MSG_CODE_NVM_OK)
		rc = ECORE_UNKNOWN_ERROR;

	if (buf_size != MCP_DRV_NVM_BUF_LEN)
		rc = ECORE_UNKNOWN_ERROR;

	return rc;
}

enum _ecore_status_t ecore_mcp_mem_ecc_events(struct ecore_hwfn *p_hwfn,
					      struct ecore_ptt *p_ptt,
					      u64 *num_events)
{
	u32 rsp;

	return ecore_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_MEM_ECC_EVENTS,
			     0, &rsp, (u32 *)num_events);
}

static enum resource_id_enum
ecore_mcp_get_mfw_res_id(enum ecore_resources res_id)
{
	enum resource_id_enum mfw_res_id = RESOURCE_NUM_INVALID;

	switch (res_id) {
	case ECORE_SB:
		mfw_res_id = RESOURCE_NUM_SB_E;
		break;
	case ECORE_L2_QUEUE:
		mfw_res_id = RESOURCE_NUM_L2_QUEUE_E;
		break;
	case ECORE_VPORT:
		mfw_res_id = RESOURCE_NUM_VPORT_E;
		break;
	case ECORE_RSS_ENG:
		mfw_res_id = RESOURCE_NUM_RSS_ENGINES_E;
		break;
	case ECORE_PQ:
		mfw_res_id = RESOURCE_NUM_PQ_E;
		break;
	case ECORE_RL:
		mfw_res_id = RESOURCE_NUM_RL_E;
		break;
	case ECORE_MAC:
	case ECORE_VLAN:
		/* Each VFC resource can accommodate both a MAC and a VLAN */
		mfw_res_id = RESOURCE_VFC_FILTER_E;
		break;
	case ECORE_ILT:
		mfw_res_id = RESOURCE_ILT_E;
		break;
	case ECORE_LL2_QUEUE:
		mfw_res_id = RESOURCE_LL2_QUEUE_E;
		break;
	case ECORE_RDMA_CNQ_RAM:
	case ECORE_CMDQS_CQS:
		/* CNQ/CMDQS are the same resource */
		mfw_res_id = RESOURCE_CQS_E;
		break;
	case ECORE_RDMA_STATS_QUEUE:
		mfw_res_id = RESOURCE_RDMA_STATS_QUEUE_E;
		break;
	case ECORE_BDQ:
		mfw_res_id = RESOURCE_BDQ_E;
		break;
	default:
		break;
	}

	return mfw_res_id;
}

#define ECORE_RESC_ALLOC_VERSION_MAJOR	2
#define ECORE_RESC_ALLOC_VERSION_MINOR	0
#define ECORE_RESC_ALLOC_VERSION				\
	((ECORE_RESC_ALLOC_VERSION_MAJOR <<			\
	  DRV_MB_PARAM_RESOURCE_ALLOC_VERSION_MAJOR_SHIFT) |	\
	 (ECORE_RESC_ALLOC_VERSION_MINOR <<			\
	  DRV_MB_PARAM_RESOURCE_ALLOC_VERSION_MINOR_SHIFT))

struct ecore_resc_alloc_in_params {
	u32 cmd;
	enum ecore_resources res_id;
	u32 resc_max_val;
};

struct ecore_resc_alloc_out_params {
	u32 mcp_resp;
	u32 mcp_param;
	u32 resc_num;
	u32 resc_start;
	u32 vf_resc_num;
	u32 vf_resc_start;
	u32 flags;
};

#define ECORE_RECOVERY_PROLOG_SLEEP_MS	100

enum _ecore_status_t ecore_recovery_prolog(struct ecore_dev *p_dev)
{
	struct ecore_hwfn *p_hwfn = ECORE_LEADING_HWFN(p_dev);
	struct ecore_ptt *p_ptt = p_hwfn->p_main_ptt;
	enum _ecore_status_t rc;

	/* Allow ongoing PCIe transactions to complete */
	OSAL_MSLEEP(ECORE_RECOVERY_PROLOG_SLEEP_MS);

	/* Clear the PF's internal FID_enable in the PXP */
	rc = ecore_pglueb_set_pfid_enable(p_hwfn, p_ptt, false);
	if (rc != ECORE_SUCCESS)
		DP_NOTICE(p_hwfn, false,
			  "ecore_pglueb_set_pfid_enable() failed. rc = %d.\n",
			  rc);

	return rc;
}

static enum _ecore_status_t
ecore_mcp_resc_allocation_msg(struct ecore_hwfn *p_hwfn,
			      struct ecore_ptt *p_ptt,
			      struct ecore_resc_alloc_in_params *p_in_params,
			      struct ecore_resc_alloc_out_params *p_out_params)
{
	struct ecore_mcp_mb_params mb_params;
	struct resource_info mfw_resc_info;
	enum _ecore_status_t rc;

	OSAL_MEM_ZERO(&mfw_resc_info, sizeof(mfw_resc_info));

	mfw_resc_info.res_id = ecore_mcp_get_mfw_res_id(p_in_params->res_id);
	if (mfw_resc_info.res_id == RESOURCE_NUM_INVALID) {
		DP_ERR(p_hwfn,
		       "Failed to match resource %d [%s] with the MFW resources\n",
		       p_in_params->res_id,
		       ecore_hw_get_resc_name(p_in_params->res_id));
		return ECORE_INVAL;
	}

	switch (p_in_params->cmd) {
	case DRV_MSG_SET_RESOURCE_VALUE_MSG:
		mfw_resc_info.size = p_in_params->resc_max_val;
		/* Fallthrough */
	case DRV_MSG_GET_RESOURCE_ALLOC_MSG:
		break;
	default:
		DP_ERR(p_hwfn, "Unexpected resource alloc command [0x%08x]\n",
		       p_in_params->cmd);
		return ECORE_INVAL;
	}

	OSAL_MEM_ZERO(&mb_params, sizeof(mb_params));
	mb_params.cmd = p_in_params->cmd;
	mb_params.param = ECORE_RESC_ALLOC_VERSION;
	mb_params.p_data_src = &mfw_resc_info;
	mb_params.data_src_size = sizeof(mfw_resc_info);
	mb_params.p_data_dst = mb_params.p_data_src;
	mb_params.data_dst_size = mb_params.data_src_size;

	DP_VERBOSE(p_hwfn, ECORE_MSG_SP,
		   "Resource message request: cmd 0x%08x, res_id %d [%s], hsi_version %d.%d, val 0x%x\n",
		   p_in_params->cmd, p_in_params->res_id,
		   ecore_hw_get_resc_name(p_in_params->res_id),
		   ECORE_MFW_GET_FIELD(mb_params.param,
			   DRV_MB_PARAM_RESOURCE_ALLOC_VERSION_MAJOR),
		   ECORE_MFW_GET_FIELD(mb_params.param,
			   DRV_MB_PARAM_RESOURCE_ALLOC_VERSION_MINOR),
		   p_in_params->resc_max_val);

	rc = ecore_mcp_cmd_and_union(p_hwfn, p_ptt, &mb_params);
	if (rc != ECORE_SUCCESS)
		return rc;

	p_out_params->mcp_resp = mb_params.mcp_resp;
	p_out_params->mcp_param = mb_params.mcp_param;
	p_out_params->resc_num = mfw_resc_info.size;
	p_out_params->resc_start = mfw_resc_info.offset;
	p_out_params->vf_resc_num = mfw_resc_info.vf_size;
	p_out_params->vf_resc_start = mfw_resc_info.vf_offset;
	p_out_params->flags = mfw_resc_info.flags;

	DP_VERBOSE(p_hwfn, ECORE_MSG_SP,
		   "Resource message response: mfw_hsi_version %d.%d, num 0x%x, start 0x%x, vf_num 0x%x, vf_start 0x%x, flags 0x%08x\n",
		   ECORE_MFW_GET_FIELD(p_out_params->mcp_param,
			   FW_MB_PARAM_RESOURCE_ALLOC_VERSION_MAJOR),
		   ECORE_MFW_GET_FIELD(p_out_params->mcp_param,
			   FW_MB_PARAM_RESOURCE_ALLOC_VERSION_MINOR),
		   p_out_params->resc_num, p_out_params->resc_start,
		   p_out_params->vf_resc_num, p_out_params->vf_resc_start,
		   p_out_params->flags);

	return ECORE_SUCCESS;
}

enum _ecore_status_t
ecore_mcp_set_resc_max_val(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt,
			   enum ecore_resources res_id, u32 resc_max_val,
			   u32 *p_mcp_resp)
{
	struct ecore_resc_alloc_out_params out_params;
	struct ecore_resc_alloc_in_params in_params;
	enum _ecore_status_t rc;

	OSAL_MEM_ZERO(&in_params, sizeof(in_params));
	in_params.cmd = DRV_MSG_SET_RESOURCE_VALUE_MSG;
	in_params.res_id = res_id;
	in_params.resc_max_val = resc_max_val;
	OSAL_MEM_ZERO(&out_params, sizeof(out_params));
	rc = ecore_mcp_resc_allocation_msg(p_hwfn, p_ptt, &in_params,
					   &out_params);
	if (rc != ECORE_SUCCESS)
		return rc;

	*p_mcp_resp = out_params.mcp_resp;

	return ECORE_SUCCESS;
}

enum _ecore_status_t
ecore_mcp_get_resc_info(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt,
			enum ecore_resources res_id, u32 *p_mcp_resp,
			u32 *p_resc_num, u32 *p_resc_start)
{
	struct ecore_resc_alloc_out_params out_params;
	struct ecore_resc_alloc_in_params in_params;
	enum _ecore_status_t rc;

	OSAL_MEM_ZERO(&in_params, sizeof(in_params));
	in_params.cmd = DRV_MSG_GET_RESOURCE_ALLOC_MSG;
	in_params.res_id = res_id;
	OSAL_MEM_ZERO(&out_params, sizeof(out_params));
	rc = ecore_mcp_resc_allocation_msg(p_hwfn, p_ptt, &in_params,
					   &out_params);
	if (rc != ECORE_SUCCESS)
		return rc;

	*p_mcp_resp = out_params.mcp_resp;

	if (*p_mcp_resp == FW_MSG_CODE_RESOURCE_ALLOC_OK) {
		*p_resc_num = out_params.resc_num;
		*p_resc_start = out_params.resc_start;
	}

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_mcp_initiate_pf_flr(struct ecore_hwfn *p_hwfn,
					       struct ecore_ptt *p_ptt)
{
	u32 mcp_resp, mcp_param;

	return ecore_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_INITIATE_PF_FLR, 0,
			     &mcp_resp, &mcp_param);
}

static enum _ecore_status_t ecore_mcp_resource_cmd(struct ecore_hwfn *p_hwfn,
						   struct ecore_ptt *p_ptt,
						   u32 param, u32 *p_mcp_resp,
						   u32 *p_mcp_param)
{
	enum _ecore_status_t rc;

	rc = ecore_mcp_cmd(p_hwfn, p_ptt, DRV_MSG_CODE_RESOURCE_CMD, param,
			   p_mcp_resp, p_mcp_param);
	if (rc != ECORE_SUCCESS)
		return rc;

	if (*p_mcp_resp == FW_MSG_CODE_UNSUPPORTED) {
		DP_INFO(p_hwfn,
			"The resource command is unsupported by the MFW\n");
		return ECORE_NOTIMPL;
	}

	if (*p_mcp_param == RESOURCE_OPCODE_UNKNOWN_CMD) {
		u8 opcode = ECORE_MFW_GET_FIELD(param, RESOURCE_CMD_REQ_OPCODE);

		DP_NOTICE(p_hwfn, false,
			  "The resource command is unknown to the MFW [param 0x%08x, opcode %d]\n",
			  param, opcode);
		return ECORE_INVAL;
	}

	return rc;
}

enum _ecore_status_t
__ecore_mcp_resc_lock(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt,
		      struct ecore_resc_lock_params *p_params)
{
	u32 param = 0, mcp_resp, mcp_param;
	u8 opcode;
	enum _ecore_status_t rc;

	switch (p_params->timeout) {
	case ECORE_MCP_RESC_LOCK_TO_DEFAULT:
		opcode = RESOURCE_OPCODE_REQ;
		p_params->timeout = 0;
		break;
	case ECORE_MCP_RESC_LOCK_TO_NONE:
		opcode = RESOURCE_OPCODE_REQ_WO_AGING;
		p_params->timeout = 0;
		break;
	default:
		opcode = RESOURCE_OPCODE_REQ_W_AGING;
		break;
	}

	ECORE_MFW_SET_FIELD(param, RESOURCE_CMD_REQ_RESC, p_params->resource);
	ECORE_MFW_SET_FIELD(param, RESOURCE_CMD_REQ_OPCODE, opcode);
	ECORE_MFW_SET_FIELD(param, RESOURCE_CMD_REQ_AGE, p_params->timeout);

	DP_VERBOSE(p_hwfn, ECORE_MSG_SP,
		   "Resource lock request: param 0x%08x [age %d, opcode %d, resource %d]\n",
		   param, p_params->timeout, opcode, p_params->resource);

	/* Attempt to acquire the resource */
	rc = ecore_mcp_resource_cmd(p_hwfn, p_ptt, param, &mcp_resp,
				    &mcp_param);
	if (rc != ECORE_SUCCESS)
		return rc;

	/* Analyze the response */
	p_params->owner = ECORE_MFW_GET_FIELD(mcp_param,
					     RESOURCE_CMD_RSP_OWNER);
	opcode = ECORE_MFW_GET_FIELD(mcp_param, RESOURCE_CMD_RSP_OPCODE);

	DP_VERBOSE(p_hwfn, ECORE_MSG_SP,
		   "Resource lock response: mcp_param 0x%08x [opcode %d, owner %d]\n",
		   mcp_param, opcode, p_params->owner);

	switch (opcode) {
	case RESOURCE_OPCODE_GNT:
		p_params->b_granted = true;
		break;
	case RESOURCE_OPCODE_BUSY:
		p_params->b_granted = false;
		break;
	default:
		DP_NOTICE(p_hwfn, false,
			  "Unexpected opcode in resource lock response [mcp_param 0x%08x, opcode %d]\n",
			  mcp_param, opcode);
		return ECORE_INVAL;
	}

	return ECORE_SUCCESS;
}

enum _ecore_status_t
ecore_mcp_resc_lock(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt,
		    struct ecore_resc_lock_params *p_params)
{
	u32 retry_cnt = 0;
	enum _ecore_status_t rc;

	do {
		/* No need for an interval before the first iteration */
		if (retry_cnt) {
			if (p_params->sleep_b4_retry) {
				u16 retry_interval_in_ms =
					DIV_ROUND_UP(p_params->retry_interval,
						     1000);

				OSAL_MSLEEP(retry_interval_in_ms);
			} else {
				OSAL_UDELAY(p_params->retry_interval);
			}
		}

		rc = __ecore_mcp_resc_lock(p_hwfn, p_ptt, p_params);
		if (rc != ECORE_SUCCESS)
			return rc;

		if (p_params->b_granted)
			break;
	} while (retry_cnt++ < p_params->retry_num);

	return ECORE_SUCCESS;
}

enum _ecore_status_t
ecore_mcp_resc_unlock(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt,
		      struct ecore_resc_unlock_params *p_params)
{
	u32 param = 0, mcp_resp, mcp_param;
	u8 opcode;
	enum _ecore_status_t rc;

	opcode = p_params->b_force ? RESOURCE_OPCODE_FORCE_RELEASE
				   : RESOURCE_OPCODE_RELEASE;
	ECORE_MFW_SET_FIELD(param, RESOURCE_CMD_REQ_RESC, p_params->resource);
	ECORE_MFW_SET_FIELD(param, RESOURCE_CMD_REQ_OPCODE, opcode);

	DP_VERBOSE(p_hwfn, ECORE_MSG_SP,
		   "Resource unlock request: param 0x%08x [opcode %d, resource %d]\n",
		   param, opcode, p_params->resource);

	/* Attempt to release the resource */
	rc = ecore_mcp_resource_cmd(p_hwfn, p_ptt, param, &mcp_resp,
				    &mcp_param);
	if (rc != ECORE_SUCCESS)
		return rc;

	/* Analyze the response */
	opcode = ECORE_MFW_GET_FIELD(mcp_param, RESOURCE_CMD_RSP_OPCODE);

	DP_VERBOSE(p_hwfn, ECORE_MSG_SP,
		   "Resource unlock response: mcp_param 0x%08x [opcode %d]\n",
		   mcp_param, opcode);

	switch (opcode) {
	case RESOURCE_OPCODE_RELEASED_PREVIOUS:
		DP_INFO(p_hwfn,
			"Resource unlock request for an already released resource [%d]\n",
			p_params->resource);
		/* Fallthrough */
	case RESOURCE_OPCODE_RELEASED:
		p_params->b_released = true;
		break;
	case RESOURCE_OPCODE_WRONG_OWNER:
		p_params->b_released = false;
		break;
	default:
		DP_NOTICE(p_hwfn, false,
			  "Unexpected opcode in resource unlock response [mcp_param 0x%08x, opcode %d]\n",
			  mcp_param, opcode);
		return ECORE_INVAL;
	}

	return ECORE_SUCCESS;
}
