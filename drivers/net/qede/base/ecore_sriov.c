/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#include "bcm_osal.h"
#include "ecore.h"
#include "reg_addr.h"
#include "ecore_sriov.h"
#include "ecore_status.h"
#include "ecore_hw.h"
#include "ecore_hw_defs.h"
#include "ecore_int.h"
#include "ecore_hsi_eth.h"
#include "ecore_l2.h"
#include "ecore_vfpf_if.h"
#include "ecore_rt_defs.h"
#include "ecore_init_ops.h"
#include "ecore_gtt_reg_addr.h"
#include "ecore_iro.h"
#include "ecore_mcp.h"
#include "ecore_cxt.h"
#include "ecore_vf.h"
#include "ecore_init_fw_funcs.h"
#include "ecore_sp_commands.h"

const char *ecore_channel_tlvs_string[] = {
	"CHANNEL_TLV_NONE",	/* ends tlv sequence */
	"CHANNEL_TLV_ACQUIRE",
	"CHANNEL_TLV_VPORT_START",
	"CHANNEL_TLV_VPORT_UPDATE",
	"CHANNEL_TLV_VPORT_TEARDOWN",
	"CHANNEL_TLV_START_RXQ",
	"CHANNEL_TLV_START_TXQ",
	"CHANNEL_TLV_STOP_RXQ",
	"CHANNEL_TLV_STOP_TXQ",
	"CHANNEL_TLV_UPDATE_RXQ",
	"CHANNEL_TLV_INT_CLEANUP",
	"CHANNEL_TLV_CLOSE",
	"CHANNEL_TLV_RELEASE",
	"CHANNEL_TLV_LIST_END",
	"CHANNEL_TLV_UCAST_FILTER",
	"CHANNEL_TLV_VPORT_UPDATE_ACTIVATE",
	"CHANNEL_TLV_VPORT_UPDATE_TX_SWITCH",
	"CHANNEL_TLV_VPORT_UPDATE_VLAN_STRIP",
	"CHANNEL_TLV_VPORT_UPDATE_MCAST",
	"CHANNEL_TLV_VPORT_UPDATE_ACCEPT_PARAM",
	"CHANNEL_TLV_VPORT_UPDATE_RSS",
	"CHANNEL_TLV_VPORT_UPDATE_ACCEPT_ANY_VLAN",
	"CHANNEL_TLV_VPORT_UPDATE_SGE_TPA",
	"CHANNEL_TLV_MAX"
};

/* IOV ramrods */
static enum _ecore_status_t ecore_sp_vf_start(struct ecore_hwfn *p_hwfn,
					      struct ecore_vf_info *p_vf)
{
	struct vf_start_ramrod_data *p_ramrod = OSAL_NULL;
	struct ecore_spq_entry *p_ent = OSAL_NULL;
	struct ecore_sp_init_data init_data;
	enum _ecore_status_t rc = ECORE_NOTIMPL;
	u8 fp_minor;

	/* Get SPQ entry */
	OSAL_MEMSET(&init_data, 0, sizeof(init_data));
	init_data.cid = ecore_spq_get_cid(p_hwfn);
	init_data.opaque_fid = p_vf->opaque_fid;
	init_data.comp_mode = ECORE_SPQ_MODE_EBLOCK;

	rc = ecore_sp_init_request(p_hwfn, &p_ent,
				   COMMON_RAMROD_VF_START,
				   PROTOCOLID_COMMON, &init_data);
	if (rc != ECORE_SUCCESS)
		return rc;

	p_ramrod = &p_ent->ramrod.vf_start;

	p_ramrod->vf_id = GET_FIELD(p_vf->concrete_fid, PXP_CONCRETE_FID_VFID);
	p_ramrod->opaque_fid = OSAL_CPU_TO_LE16(p_vf->opaque_fid);

	switch (p_hwfn->hw_info.personality) {
	case ECORE_PCI_ETH:
		p_ramrod->personality = PERSONALITY_ETH;
		break;
	case ECORE_PCI_ETH_ROCE:
		p_ramrod->personality = PERSONALITY_RDMA_AND_ETH;
		break;
	default:
		DP_NOTICE(p_hwfn, true, "Unknown VF personality %d\n",
			  p_hwfn->hw_info.personality);
		return ECORE_INVAL;
	}

	fp_minor = p_vf->acquire.vfdev_info.eth_fp_hsi_minor;
	if (fp_minor > ETH_HSI_VER_MINOR &&
	    fp_minor != ETH_HSI_VER_NO_PKT_LEN_TUNN) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "VF [%d] - Requested fp hsi %02x.%02x which is"
			   " slightly newer than PF's %02x.%02x; Configuring"
			   " PFs version\n",
			   p_vf->abs_vf_id,
			   ETH_HSI_VER_MAJOR, fp_minor,
			   ETH_HSI_VER_MAJOR, ETH_HSI_VER_MINOR);
		fp_minor = ETH_HSI_VER_MINOR;
	}

	p_ramrod->hsi_fp_ver.major_ver_arr[ETH_VER_KEY] = ETH_HSI_VER_MAJOR;
	p_ramrod->hsi_fp_ver.minor_ver_arr[ETH_VER_KEY] = fp_minor;

	DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
		   "VF[%d] - Starting using HSI %02x.%02x\n",
		   p_vf->abs_vf_id, ETH_HSI_VER_MAJOR, fp_minor);

	return ecore_spq_post(p_hwfn, p_ent, OSAL_NULL);
}

static enum _ecore_status_t ecore_sp_vf_stop(struct ecore_hwfn *p_hwfn,
					     u32 concrete_vfid,
					     u16 opaque_vfid)
{
	struct vf_stop_ramrod_data *p_ramrod = OSAL_NULL;
	struct ecore_spq_entry *p_ent = OSAL_NULL;
	struct ecore_sp_init_data init_data;
	enum _ecore_status_t rc = ECORE_NOTIMPL;

	/* Get SPQ entry */
	OSAL_MEMSET(&init_data, 0, sizeof(init_data));
	init_data.cid = ecore_spq_get_cid(p_hwfn);
	init_data.opaque_fid = opaque_vfid;
	init_data.comp_mode = ECORE_SPQ_MODE_EBLOCK;

	rc = ecore_sp_init_request(p_hwfn, &p_ent,
				   COMMON_RAMROD_VF_STOP,
				   PROTOCOLID_COMMON, &init_data);
	if (rc != ECORE_SUCCESS)
		return rc;

	p_ramrod = &p_ent->ramrod.vf_stop;

	p_ramrod->vf_id = GET_FIELD(concrete_vfid, PXP_CONCRETE_FID_VFID);

	return ecore_spq_post(p_hwfn, p_ent, OSAL_NULL);
}

bool ecore_iov_is_valid_vfid(struct ecore_hwfn *p_hwfn, int rel_vf_id,
			     bool b_enabled_only)
{
	if (!p_hwfn->pf_iov_info) {
		DP_NOTICE(p_hwfn->p_dev, true, "No iov info\n");
		return false;
	}

	if ((rel_vf_id >= p_hwfn->p_dev->p_iov_info->total_vfs) ||
	    (rel_vf_id < 0))
		return false;

	if ((!p_hwfn->pf_iov_info->vfs_array[rel_vf_id].b_init) &&
	    b_enabled_only)
		return false;

	return true;
}

struct ecore_vf_info *ecore_iov_get_vf_info(struct ecore_hwfn *p_hwfn,
					    u16 relative_vf_id,
					    bool b_enabled_only)
{
	struct ecore_vf_info *vf = OSAL_NULL;

	if (!p_hwfn->pf_iov_info) {
		DP_NOTICE(p_hwfn->p_dev, true, "No iov info\n");
		return OSAL_NULL;
	}

	if (ecore_iov_is_valid_vfid(p_hwfn, relative_vf_id, b_enabled_only))
		vf = &p_hwfn->pf_iov_info->vfs_array[relative_vf_id];
	else
		DP_ERR(p_hwfn, "ecore_iov_get_vf_info: VF[%d] is not enabled\n",
		       relative_vf_id);

	return vf;
}

static bool ecore_iov_validate_rxq(struct ecore_hwfn *p_hwfn,
				   struct ecore_vf_info *p_vf,
				   u16 rx_qid)
{
	if (rx_qid >= p_vf->num_rxqs)
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "VF[0x%02x] - can't touch Rx queue[%04x];"
			   " Only 0x%04x are allocated\n",
			   p_vf->abs_vf_id, rx_qid, p_vf->num_rxqs);
	return rx_qid < p_vf->num_rxqs;
}

static bool ecore_iov_validate_txq(struct ecore_hwfn *p_hwfn,
				   struct ecore_vf_info *p_vf,
				   u16 tx_qid)
{
	if (tx_qid >= p_vf->num_txqs)
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "VF[0x%02x] - can't touch Tx queue[%04x];"
			   " Only 0x%04x are allocated\n",
			   p_vf->abs_vf_id, tx_qid, p_vf->num_txqs);
	return tx_qid < p_vf->num_txqs;
}

static bool ecore_iov_validate_sb(struct ecore_hwfn *p_hwfn,
				  struct ecore_vf_info *p_vf,
				  u16 sb_idx)
{
	int i;

	for (i = 0; i < p_vf->num_sbs; i++)
		if (p_vf->igu_sbs[i] == sb_idx)
			return true;

	DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
		   "VF[0%02x] - tried using sb_idx %04x which doesn't exist as"
		   " one of its 0x%02x SBs\n",
		   p_vf->abs_vf_id, sb_idx, p_vf->num_sbs);

	return false;
}

/* TODO - this is linux crc32; Need a way to ifdef it out for linux */
u32 ecore_crc32(u32 crc, u8 *ptr, u32 length)
{
	int i;

	while (length--) {
		crc ^= *ptr++;
		for (i = 0; i < 8; i++)
			crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
	}
	return crc;
}

enum _ecore_status_t ecore_iov_post_vf_bulletin(struct ecore_hwfn *p_hwfn,
						int vfid,
						struct ecore_ptt *p_ptt)
{
	struct ecore_bulletin_content *p_bulletin;
	int crc_size = sizeof(p_bulletin->crc);
	struct ecore_dmae_params params;
	struct ecore_vf_info *p_vf;

	p_vf = ecore_iov_get_vf_info(p_hwfn, (u16)vfid, true);
	if (!p_vf)
		return ECORE_INVAL;

	/* TODO - check VF is in a state where it can accept message */
	if (!p_vf->vf_bulletin)
		return ECORE_INVAL;

	p_bulletin = p_vf->bulletin.p_virt;

	/* Increment bulletin board version and compute crc */
	p_bulletin->version++;
	p_bulletin->crc = ecore_crc32(0, (u8 *)p_bulletin + crc_size,
				      p_vf->bulletin.size - crc_size);

	DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
		   "Posting Bulletin 0x%08x to VF[%d] (CRC 0x%08x)\n",
		   p_bulletin->version, p_vf->relative_vf_id, p_bulletin->crc);

	/* propagate bulletin board via dmae to vm memory */
	OSAL_MEMSET(&params, 0, sizeof(params));
	params.flags = ECORE_DMAE_FLAG_VF_DST;
	params.dst_vfid = p_vf->abs_vf_id;
	return ecore_dmae_host2host(p_hwfn, p_ptt, p_vf->bulletin.phys,
				    p_vf->vf_bulletin, p_vf->bulletin.size / 4,
				    &params);
}

static enum _ecore_status_t ecore_iov_pci_cfg_info(struct ecore_dev *p_dev)
{
	struct ecore_hw_sriov_info *iov = p_dev->p_iov_info;
	int pos = iov->pos;

	DP_VERBOSE(p_dev, ECORE_MSG_IOV, "sriov ext pos %d\n", pos);
	OSAL_PCI_READ_CONFIG_WORD(p_dev, pos + PCI_SRIOV_CTRL, &iov->ctrl);

	OSAL_PCI_READ_CONFIG_WORD(p_dev,
				  pos + PCI_SRIOV_TOTAL_VF, &iov->total_vfs);
	OSAL_PCI_READ_CONFIG_WORD(p_dev,
				  pos + PCI_SRIOV_INITIAL_VF,
				  &iov->initial_vfs);

	OSAL_PCI_READ_CONFIG_WORD(p_dev, pos + PCI_SRIOV_NUM_VF, &iov->num_vfs);
	if (iov->num_vfs) {
		/* @@@TODO - in future we might want to add an OSAL here to
		 * allow each OS to decide on its own how to act.
		 */
		DP_VERBOSE(p_dev, ECORE_MSG_IOV,
			   "Number of VFs are already set to non-zero value."
			   " Ignoring PCI configuration value\n");
		iov->num_vfs = 0;
	}

	OSAL_PCI_READ_CONFIG_WORD(p_dev,
				  pos + PCI_SRIOV_VF_OFFSET, &iov->offset);

	OSAL_PCI_READ_CONFIG_WORD(p_dev,
				  pos + PCI_SRIOV_VF_STRIDE, &iov->stride);

	OSAL_PCI_READ_CONFIG_WORD(p_dev,
				  pos + PCI_SRIOV_VF_DID, &iov->vf_device_id);

	OSAL_PCI_READ_CONFIG_DWORD(p_dev,
				   pos + PCI_SRIOV_SUP_PGSIZE, &iov->pgsz);

	OSAL_PCI_READ_CONFIG_DWORD(p_dev, pos + PCI_SRIOV_CAP, &iov->cap);

	OSAL_PCI_READ_CONFIG_BYTE(p_dev, pos + PCI_SRIOV_FUNC_LINK, &iov->link);

	DP_VERBOSE(p_dev, ECORE_MSG_IOV, "IOV info[%d]: nres %d, cap 0x%x,"
		   "ctrl 0x%x, total %d, initial %d, num vfs %d, offset %d,"
		   " stride %d, page size 0x%x\n", 0,
		   /* @@@TBD MichalK - function id */
		   iov->nres, iov->cap, iov->ctrl,
		   iov->total_vfs, iov->initial_vfs, iov->nr_virtfn,
		   iov->offset, iov->stride, iov->pgsz);

	/* Some sanity checks */
	if (iov->num_vfs > NUM_OF_VFS(p_dev) ||
	    iov->total_vfs > NUM_OF_VFS(p_dev)) {
		/* This can happen only due to a bug. In this case we set
		 * num_vfs to zero to avoid memory corruption in the code that
		 * assumes max number of vfs
		 */
		DP_NOTICE(p_dev, false,
			  "IOV: Unexpected number of vfs set: %d"
			  " setting num_vf to zero\n",
			  iov->num_vfs);

		iov->num_vfs = 0;
		iov->total_vfs = 0;
	}

	return ECORE_SUCCESS;
}

static void ecore_iov_clear_vf_igu_blocks(struct ecore_hwfn *p_hwfn,
					  struct ecore_ptt *p_ptt)
{
	struct ecore_igu_block *p_sb;
	u16 sb_id;
	u32 val;

	if (!p_hwfn->hw_info.p_igu_info) {
		DP_ERR(p_hwfn,
		       "ecore_iov_clear_vf_igu_blocks IGU Info not inited\n");
		return;
	}

	for (sb_id = 0;
	     sb_id < ECORE_MAPPING_MEMORY_SIZE(p_hwfn->p_dev); sb_id++) {
		p_sb = &p_hwfn->hw_info.p_igu_info->igu_map.igu_blocks[sb_id];
		if ((p_sb->status & ECORE_IGU_STATUS_FREE) &&
		    !(p_sb->status & ECORE_IGU_STATUS_PF)) {
			val = ecore_rd(p_hwfn, p_ptt,
				       IGU_REG_MAPPING_MEMORY + sb_id * 4);
			SET_FIELD(val, IGU_MAPPING_LINE_VALID, 0);
			ecore_wr(p_hwfn, p_ptt,
				 IGU_REG_MAPPING_MEMORY + 4 * sb_id, val);
		}
	}
}

static void ecore_iov_setup_vfdb(struct ecore_hwfn *p_hwfn)
{
	struct ecore_hw_sriov_info *p_iov = p_hwfn->p_dev->p_iov_info;
	struct ecore_pf_iov *p_iov_info = p_hwfn->pf_iov_info;
	struct ecore_bulletin_content *p_bulletin_virt;
	dma_addr_t req_p, rply_p, bulletin_p;
	union pfvf_tlvs *p_reply_virt_addr;
	union vfpf_tlvs *p_req_virt_addr;
	u8 idx = 0;

	OSAL_MEMSET(p_iov_info->vfs_array, 0, sizeof(p_iov_info->vfs_array));

	p_req_virt_addr = p_iov_info->mbx_msg_virt_addr;
	req_p = p_iov_info->mbx_msg_phys_addr;
	p_reply_virt_addr = p_iov_info->mbx_reply_virt_addr;
	rply_p = p_iov_info->mbx_reply_phys_addr;
	p_bulletin_virt = p_iov_info->p_bulletins;
	bulletin_p = p_iov_info->bulletins_phys;
	if (!p_req_virt_addr || !p_reply_virt_addr || !p_bulletin_virt) {
		DP_ERR(p_hwfn,
		       "ecore_iov_setup_vfdb called without alloc mem first\n");
		return;
	}

	p_iov_info->base_vport_id = 1;	/* @@@TBD resource allocation */

	for (idx = 0; idx < p_iov->total_vfs; idx++) {
		struct ecore_vf_info *vf = &p_iov_info->vfs_array[idx];
		u32 concrete;

		vf->vf_mbx.req_virt = p_req_virt_addr + idx;
		vf->vf_mbx.req_phys = req_p + idx * sizeof(union vfpf_tlvs);
		vf->vf_mbx.reply_virt = p_reply_virt_addr + idx;
		vf->vf_mbx.reply_phys = rply_p + idx * sizeof(union pfvf_tlvs);

#ifdef CONFIG_ECORE_SW_CHANNEL
		vf->vf_mbx.sw_mbx.request_size = sizeof(union vfpf_tlvs);
		vf->vf_mbx.sw_mbx.mbx_state = VF_PF_WAIT_FOR_START_REQUEST;
#endif
		vf->state = VF_STOPPED;
		vf->b_init = false;

		vf->bulletin.phys = idx *
		    sizeof(struct ecore_bulletin_content) + bulletin_p;
		vf->bulletin.p_virt = p_bulletin_virt + idx;
		vf->bulletin.size = sizeof(struct ecore_bulletin_content);

		vf->relative_vf_id = idx;
		vf->abs_vf_id = idx + p_iov->first_vf_in_pf;
		concrete = ecore_vfid_to_concrete(p_hwfn, vf->abs_vf_id);
		vf->concrete_fid = concrete;
		/* TODO - need to devise a better way of getting opaque */
		vf->opaque_fid = (p_hwfn->hw_info.opaque_fid & 0xff) |
		    (vf->abs_vf_id << 8);
		/* @@TBD MichalK - add base vport_id of VFs to equation */
		vf->vport_id = p_iov_info->base_vport_id + idx;

		vf->num_mac_filters = ECORE_ETH_VF_NUM_MAC_FILTERS;
		vf->num_vlan_filters = ECORE_ETH_VF_NUM_VLAN_FILTERS;
	}
}

static enum _ecore_status_t ecore_iov_allocate_vfdb(struct ecore_hwfn *p_hwfn)
{
	struct ecore_pf_iov *p_iov_info = p_hwfn->pf_iov_info;
	void **p_v_addr;
	u16 num_vfs = 0;

	num_vfs = p_hwfn->p_dev->p_iov_info->total_vfs;

	DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
		   "ecore_iov_allocate_vfdb for %d VFs\n", num_vfs);

	/* Allocate PF Mailbox buffer (per-VF) */
	p_iov_info->mbx_msg_size = sizeof(union vfpf_tlvs) * num_vfs;
	p_v_addr = &p_iov_info->mbx_msg_virt_addr;
	*p_v_addr = OSAL_DMA_ALLOC_COHERENT(p_hwfn->p_dev,
					    &p_iov_info->mbx_msg_phys_addr,
					    p_iov_info->mbx_msg_size);
	if (!*p_v_addr)
		return ECORE_NOMEM;

	/* Allocate PF Mailbox Reply buffer (per-VF) */
	p_iov_info->mbx_reply_size = sizeof(union pfvf_tlvs) * num_vfs;
	p_v_addr = &p_iov_info->mbx_reply_virt_addr;
	*p_v_addr = OSAL_DMA_ALLOC_COHERENT(p_hwfn->p_dev,
					    &p_iov_info->mbx_reply_phys_addr,
					    p_iov_info->mbx_reply_size);
	if (!*p_v_addr)
		return ECORE_NOMEM;

	p_iov_info->bulletins_size = sizeof(struct ecore_bulletin_content) *
	    num_vfs;
	p_v_addr = &p_iov_info->p_bulletins;
	*p_v_addr = OSAL_DMA_ALLOC_COHERENT(p_hwfn->p_dev,
					    &p_iov_info->bulletins_phys,
					    p_iov_info->bulletins_size);
	if (!*p_v_addr)
		return ECORE_NOMEM;

	DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
		   "PF's Requests mailbox [%p virt 0x%lx phys],  "
		   "Response mailbox [%p virt 0x%lx phys] Bulletinsi"
		   " [%p virt 0x%lx phys]\n",
		   p_iov_info->mbx_msg_virt_addr,
		   (unsigned long)p_iov_info->mbx_msg_phys_addr,
		   p_iov_info->mbx_reply_virt_addr,
		   (unsigned long)p_iov_info->mbx_reply_phys_addr,
		   p_iov_info->p_bulletins,
		   (unsigned long)p_iov_info->bulletins_phys);

	return ECORE_SUCCESS;
}

static void ecore_iov_free_vfdb(struct ecore_hwfn *p_hwfn)
{
	struct ecore_pf_iov *p_iov_info = p_hwfn->pf_iov_info;

	if (p_hwfn->pf_iov_info->mbx_msg_virt_addr)
		OSAL_DMA_FREE_COHERENT(p_hwfn->p_dev,
				       p_iov_info->mbx_msg_virt_addr,
				       p_iov_info->mbx_msg_phys_addr,
				       p_iov_info->mbx_msg_size);

	if (p_hwfn->pf_iov_info->mbx_reply_virt_addr)
		OSAL_DMA_FREE_COHERENT(p_hwfn->p_dev,
				       p_iov_info->mbx_reply_virt_addr,
				       p_iov_info->mbx_reply_phys_addr,
				       p_iov_info->mbx_reply_size);

	if (p_iov_info->p_bulletins)
		OSAL_DMA_FREE_COHERENT(p_hwfn->p_dev,
				       p_iov_info->p_bulletins,
				       p_iov_info->bulletins_phys,
				       p_iov_info->bulletins_size);
}

enum _ecore_status_t ecore_iov_alloc(struct ecore_hwfn *p_hwfn)
{
	struct ecore_pf_iov *p_sriov;

	if (!IS_PF_SRIOV(p_hwfn)) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "No SR-IOV - no need for IOV db\n");
		return ECORE_SUCCESS;
	}

	p_sriov = OSAL_ZALLOC(p_hwfn->p_dev, GFP_KERNEL, sizeof(*p_sriov));
	if (!p_sriov) {
		DP_NOTICE(p_hwfn, true,
			  "Failed to allocate `struct ecore_sriov'\n");
		return ECORE_NOMEM;
	}

	p_hwfn->pf_iov_info = p_sriov;

	return ecore_iov_allocate_vfdb(p_hwfn);
}

void ecore_iov_setup(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt)
{
	if (!IS_PF_SRIOV(p_hwfn) || !IS_PF_SRIOV_ALLOC(p_hwfn))
		return;

	ecore_iov_setup_vfdb(p_hwfn);
	ecore_iov_clear_vf_igu_blocks(p_hwfn, p_ptt);
}

void ecore_iov_free(struct ecore_hwfn *p_hwfn)
{
	if (IS_PF_SRIOV_ALLOC(p_hwfn)) {
		ecore_iov_free_vfdb(p_hwfn);
		OSAL_FREE(p_hwfn->p_dev, p_hwfn->pf_iov_info);
	}
}

void ecore_iov_free_hw_info(struct ecore_dev *p_dev)
{
	OSAL_FREE(p_dev, p_dev->p_iov_info);
	p_dev->p_iov_info = OSAL_NULL;
}

enum _ecore_status_t ecore_iov_hw_info(struct ecore_hwfn *p_hwfn)
{
	struct ecore_dev *p_dev = p_hwfn->p_dev;
	int pos;
	enum _ecore_status_t rc;

	if (IS_VF(p_hwfn->p_dev))
		return ECORE_SUCCESS;

	/* Learn the PCI configuration */
	pos = OSAL_PCI_FIND_EXT_CAPABILITY(p_hwfn->p_dev,
					   PCI_EXT_CAP_ID_SRIOV);
	if (!pos) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV, "No PCIe IOV support\n");
		return ECORE_SUCCESS;
	}

	/* Allocate a new struct for IOV information */
	/* TODO - can change to VALLOC when its available */
	p_dev->p_iov_info = OSAL_ZALLOC(p_dev, GFP_KERNEL,
					sizeof(*p_dev->p_iov_info));
	if (!p_dev->p_iov_info) {
		DP_NOTICE(p_hwfn, true,
			  "Can't support IOV due to lack of memory\n");
		return ECORE_NOMEM;
	}
	p_dev->p_iov_info->pos = pos;

	rc = ecore_iov_pci_cfg_info(p_dev);
	if (rc)
		return rc;

	/* We want PF IOV to be synonemous with the existence of p_iov_info;
	 * In case the capability is published but there are no VFs, simply
	 * de-allocate the struct.
	 */
	if (!p_dev->p_iov_info->total_vfs) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "IOV capabilities, but no VFs are published\n");
		OSAL_FREE(p_dev, p_dev->p_iov_info);
		p_dev->p_iov_info = OSAL_NULL;
		return ECORE_SUCCESS;
	}

	/* Calculate the first VF index - this is a bit tricky; Basically,
	 * VFs start at offset 16 relative to PF0, and 2nd engine VFs begin
	 * after the first engine's VFs.
	 */
	p_dev->p_iov_info->first_vf_in_pf = p_hwfn->p_dev->p_iov_info->offset +
					    p_hwfn->abs_pf_id - 16;
	if (ECORE_PATH_ID(p_hwfn))
		p_dev->p_iov_info->first_vf_in_pf -= MAX_NUM_VFS_BB;

	DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
		   "First VF in hwfn 0x%08x\n",
		   p_dev->p_iov_info->first_vf_in_pf);

	return ECORE_SUCCESS;
}

bool ecore_iov_pf_sanity_check(struct ecore_hwfn *p_hwfn, int vfid)
{
	/* Check PF supports sriov */
	if (IS_VF(p_hwfn->p_dev) || !IS_ECORE_SRIOV(p_hwfn->p_dev) ||
	    !IS_PF_SRIOV_ALLOC(p_hwfn))
		return false;

	/* Check VF validity */
	if (!ecore_iov_is_valid_vfid(p_hwfn, vfid, true))
		return false;

	return true;
}

void ecore_iov_set_vf_to_disable(struct ecore_dev *p_dev,
				 u16 rel_vf_id, u8 to_disable)
{
	struct ecore_vf_info *vf;
	int i;

	for_each_hwfn(p_dev, i) {
		struct ecore_hwfn *p_hwfn = &p_dev->hwfns[i];

		vf = ecore_iov_get_vf_info(p_hwfn, rel_vf_id, false);
		if (!vf)
			continue;

		vf->to_disable = to_disable;
	}
}

void ecore_iov_set_vfs_to_disable(struct ecore_dev *p_dev,
				  u8 to_disable)
{
	u16 i;

	if (!IS_ECORE_SRIOV(p_dev))
		return;

	for (i = 0; i < p_dev->p_iov_info->total_vfs; i++)
		ecore_iov_set_vf_to_disable(p_dev, i, to_disable);
}

#ifndef LINUX_REMOVE
/* @@@TBD Consider taking outside of ecore... */
enum _ecore_status_t ecore_iov_set_vf_ctx(struct ecore_hwfn *p_hwfn,
					  u16		    vf_id,
					  void		    *ctx)
{
	enum _ecore_status_t rc = ECORE_SUCCESS;
	struct ecore_vf_info *vf = ecore_iov_get_vf_info(p_hwfn, vf_id, true);

	if (vf != OSAL_NULL) {
		vf->ctx = ctx;
#ifdef CONFIG_ECORE_SW_CHANNEL
		vf->vf_mbx.sw_mbx.mbx_state = VF_PF_WAIT_FOR_START_REQUEST;
#endif
	} else {
		rc = ECORE_UNKNOWN_ERROR;
	}
	return rc;
}
#endif

static void ecore_iov_vf_pglue_clear_err(struct ecore_hwfn      *p_hwfn,
					 struct ecore_ptt	*p_ptt,
					 u8			abs_vfid)
{
	ecore_wr(p_hwfn, p_ptt,
		 PGLUE_B_REG_WAS_ERROR_VF_31_0_CLR + (abs_vfid >> 5) * 4,
		 1 << (abs_vfid & 0x1f));
}

static void ecore_iov_vf_igu_reset(struct ecore_hwfn *p_hwfn,
				   struct ecore_ptt *p_ptt,
				   struct ecore_vf_info *vf)
{
	int i;

	/* Set VF masks and configuration - pretend */
	ecore_fid_pretend(p_hwfn, p_ptt, (u16)vf->concrete_fid);

	ecore_wr(p_hwfn, p_ptt, IGU_REG_STATISTIC_NUM_VF_MSG_SENT, 0);

	/* unpretend */
	ecore_fid_pretend(p_hwfn, p_ptt, (u16)p_hwfn->hw_info.concrete_fid);

	/* iterate over all queues, clear sb consumer */
	for (i = 0; i < vf->num_sbs; i++)
		ecore_int_igu_init_pure_rt_single(p_hwfn, p_ptt,
						  vf->igu_sbs[i],
						  vf->opaque_fid, true);
}

static void ecore_iov_vf_igu_set_int(struct ecore_hwfn *p_hwfn,
				     struct ecore_ptt *p_ptt,
				     struct ecore_vf_info *vf, bool enable)
{
	u32 igu_vf_conf;

	ecore_fid_pretend(p_hwfn, p_ptt, (u16)vf->concrete_fid);

	igu_vf_conf = ecore_rd(p_hwfn, p_ptt, IGU_REG_VF_CONFIGURATION);

	if (enable)
		igu_vf_conf |= IGU_VF_CONF_MSI_MSIX_EN;
	else
		igu_vf_conf &= ~IGU_VF_CONF_MSI_MSIX_EN;

	ecore_wr(p_hwfn, p_ptt, IGU_REG_VF_CONFIGURATION, igu_vf_conf);

	/* unpretend */
	ecore_fid_pretend(p_hwfn, p_ptt, (u16)p_hwfn->hw_info.concrete_fid);
}

static enum _ecore_status_t
ecore_iov_enable_vf_access(struct ecore_hwfn *p_hwfn,
			   struct ecore_ptt *p_ptt, struct ecore_vf_info *vf)
{
	u32 igu_vf_conf = IGU_VF_CONF_FUNC_EN;
	enum _ecore_status_t rc;

	if (vf->to_disable)
		return ECORE_SUCCESS;

	DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
		   "Enable internal access for vf %x [abs %x]\n", vf->abs_vf_id,
		   ECORE_VF_ABS_ID(p_hwfn, vf));

	ecore_iov_vf_pglue_clear_err(p_hwfn, p_ptt,
				     ECORE_VF_ABS_ID(p_hwfn, vf));

	ecore_iov_vf_igu_reset(p_hwfn, p_ptt, vf);

	rc = ecore_mcp_config_vf_msix(p_hwfn, p_ptt,
				      vf->abs_vf_id, vf->num_sbs);
	if (rc != ECORE_SUCCESS)
		return rc;

	ecore_fid_pretend(p_hwfn, p_ptt, (u16)vf->concrete_fid);

	SET_FIELD(igu_vf_conf, IGU_VF_CONF_PARENT, p_hwfn->rel_pf_id);
	STORE_RT_REG(p_hwfn, IGU_REG_VF_CONFIGURATION_RT_OFFSET, igu_vf_conf);

	ecore_init_run(p_hwfn, p_ptt, PHASE_VF, vf->abs_vf_id,
		       p_hwfn->hw_info.hw_mode);

	/* unpretend */
	ecore_fid_pretend(p_hwfn, p_ptt, (u16)p_hwfn->hw_info.concrete_fid);

	vf->state = VF_FREE;

	return rc;
}

/**
 *
 * @brief ecore_iov_config_perm_table - configure the permission
 *      zone table.
 *      In E4, queue zone permission table size is 320x9. There
 *      are 320 VF queues for single engine device (256 for dual
 *      engine device), and each entry has the following format:
 *      {Valid, VF[7:0]}
 * @param p_hwfn
 * @param p_ptt
 * @param vf
 * @param enable
 */
static void ecore_iov_config_perm_table(struct ecore_hwfn *p_hwfn,
					struct ecore_ptt *p_ptt,
					struct ecore_vf_info *vf, u8 enable)
{
	u32 reg_addr, val;
	u16 qzone_id = 0;
	int qid;

	for (qid = 0; qid < vf->num_rxqs; qid++) {
		ecore_fw_l2_queue(p_hwfn, vf->vf_queues[qid].fw_rx_qid,
				  &qzone_id);

		reg_addr = PSWHST_REG_ZONE_PERMISSION_TABLE + qzone_id * 4;
		val = enable ? (vf->abs_vf_id | (1 << 8)) : 0;
		ecore_wr(p_hwfn, p_ptt, reg_addr, val);
	}
}

static void ecore_iov_enable_vf_traffic(struct ecore_hwfn *p_hwfn,
					struct ecore_ptt *p_ptt,
					struct ecore_vf_info *vf)
{
	/* Reset vf in IGU - interrupts are still disabled */
	ecore_iov_vf_igu_reset(p_hwfn, p_ptt, vf);

	ecore_iov_vf_igu_set_int(p_hwfn, p_ptt, vf, 1);

	/* Permission Table */
	ecore_iov_config_perm_table(p_hwfn, p_ptt, vf, true);
}

static u8 ecore_iov_alloc_vf_igu_sbs(struct ecore_hwfn *p_hwfn,
				     struct ecore_ptt *p_ptt,
				     struct ecore_vf_info *vf,
				     u16 num_rx_queues)
{
	struct ecore_igu_block *igu_blocks;
	int qid = 0, igu_id = 0;
	u32 val = 0;

	igu_blocks = p_hwfn->hw_info.p_igu_info->igu_map.igu_blocks;

	if (num_rx_queues > p_hwfn->hw_info.p_igu_info->free_blks)
		num_rx_queues = p_hwfn->hw_info.p_igu_info->free_blks;

	p_hwfn->hw_info.p_igu_info->free_blks -= num_rx_queues;

	SET_FIELD(val, IGU_MAPPING_LINE_FUNCTION_NUMBER, vf->abs_vf_id);
	SET_FIELD(val, IGU_MAPPING_LINE_VALID, 1);
	SET_FIELD(val, IGU_MAPPING_LINE_PF_VALID, 0);

	while ((qid < num_rx_queues) &&
	       (igu_id < ECORE_MAPPING_MEMORY_SIZE(p_hwfn->p_dev))) {
		if (igu_blocks[igu_id].status & ECORE_IGU_STATUS_FREE) {
			struct cau_sb_entry sb_entry;

			vf->igu_sbs[qid] = (u16)igu_id;
			igu_blocks[igu_id].status &= ~ECORE_IGU_STATUS_FREE;

			SET_FIELD(val, IGU_MAPPING_LINE_VECTOR_NUMBER, qid);

			ecore_wr(p_hwfn, p_ptt,
				 IGU_REG_MAPPING_MEMORY + sizeof(u32) * igu_id,
				 val);

			/* Configure igu sb in CAU which were marked valid */
			ecore_init_cau_sb_entry(p_hwfn, &sb_entry,
						p_hwfn->rel_pf_id,
						vf->abs_vf_id, 1);
			ecore_dmae_host2grc(p_hwfn, p_ptt,
					    (u64)(osal_uintptr_t)&sb_entry,
					    CAU_REG_SB_VAR_MEMORY +
					    igu_id * sizeof(u64), 2, 0);
			qid++;
		}
		igu_id++;
	}

	vf->num_sbs = (u8)num_rx_queues;

	return vf->num_sbs;
}

/**
 *
 * @brief The function invalidates all the VF entries,
 *        technically this isn't required, but added for
 *        cleaness and ease of debugging incase a VF attempts to
 *        produce an interrupt after it has been taken down.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param vf
 */
static void ecore_iov_free_vf_igu_sbs(struct ecore_hwfn *p_hwfn,
				      struct ecore_ptt *p_ptt,
				      struct ecore_vf_info *vf)
{
	struct ecore_igu_info *p_info = p_hwfn->hw_info.p_igu_info;
	int idx, igu_id;
	u32 addr, val;

	/* Invalidate igu CAM lines and mark them as free */
	for (idx = 0; idx < vf->num_sbs; idx++) {
		igu_id = vf->igu_sbs[idx];
		addr = IGU_REG_MAPPING_MEMORY + sizeof(u32) * igu_id;

		val = ecore_rd(p_hwfn, p_ptt, addr);
		SET_FIELD(val, IGU_MAPPING_LINE_VALID, 0);
		ecore_wr(p_hwfn, p_ptt, addr, val);

		p_info->igu_map.igu_blocks[igu_id].status |=
		    ECORE_IGU_STATUS_FREE;

		p_hwfn->hw_info.p_igu_info->free_blks++;
	}

	vf->num_sbs = 0;
}

enum _ecore_status_t ecore_iov_init_hw_for_vf(struct ecore_hwfn *p_hwfn,
					      struct ecore_ptt *p_ptt,
					      u16 rel_vf_id, u16 num_rx_queues)
{
	u8 num_of_vf_available_chains  = 0;
	struct ecore_vf_info *vf = OSAL_NULL;
	enum _ecore_status_t rc = ECORE_SUCCESS;
	u32 cids;
	u8 i;

	vf = ecore_iov_get_vf_info(p_hwfn, rel_vf_id, false);
	if (!vf) {
		DP_ERR(p_hwfn, "ecore_iov_init_hw_for_vf : vf is OSAL_NULL\n");
		return ECORE_UNKNOWN_ERROR;
	}

	if (vf->b_init) {
		DP_NOTICE(p_hwfn, true, "VF[%d] is already active.\n",
			  rel_vf_id);
		return ECORE_INVAL;
	}

	/* Limit number of queues according to number of CIDs */
	ecore_cxt_get_proto_cid_count(p_hwfn, PROTOCOLID_ETH, &cids);
	DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
		   "VF[%d] - requesting to initialize for 0x%04x queues"
		   " [0x%04x CIDs available]\n",
		   vf->relative_vf_id, num_rx_queues, (u16)cids);
	num_rx_queues = OSAL_MIN_T(u16, num_rx_queues, ((u16)cids));

	num_of_vf_available_chains = ecore_iov_alloc_vf_igu_sbs(p_hwfn,
							       p_ptt,
							       vf,
							       num_rx_queues);
	if (num_of_vf_available_chains == 0) {
		DP_ERR(p_hwfn, "no available igu sbs\n");
		return ECORE_NOMEM;
	}

	/* Choose queue number and index ranges */
	vf->num_rxqs = num_of_vf_available_chains;
	vf->num_txqs = num_of_vf_available_chains;

	for (i = 0; i < vf->num_rxqs; i++) {
		u16 queue_id = ecore_int_queue_id_from_sb_id(p_hwfn,
							     vf->igu_sbs[i]);

		if (queue_id > RESC_NUM(p_hwfn, ECORE_L2_QUEUE)) {
			DP_NOTICE(p_hwfn, true,
				  "VF[%d] will require utilizing of"
				  " out-of-bounds queues - %04x\n",
				  vf->relative_vf_id, queue_id);
			/* TODO - cleanup the already allocate SBs */
			return ECORE_INVAL;
		}

		/* CIDs are per-VF, so no problem having them 0-based. */
		vf->vf_queues[i].fw_rx_qid = queue_id;
		vf->vf_queues[i].fw_tx_qid = queue_id;
		vf->vf_queues[i].fw_cid = i;

		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "VF[%d] - [%d] SB %04x, Tx/Rx queue %04x CID %04x\n",
			   vf->relative_vf_id, i, vf->igu_sbs[i], queue_id, i);
	}

	rc = ecore_iov_enable_vf_access(p_hwfn, p_ptt, vf);

	if (rc == ECORE_SUCCESS) {
		vf->b_init = true;
		p_hwfn->pf_iov_info->active_vfs[vf->relative_vf_id / 64] |=
			(1ULL << (vf->relative_vf_id % 64));

		if (IS_LEAD_HWFN(p_hwfn))
			p_hwfn->p_dev->p_iov_info->num_vfs++;
	}

	return rc;
}

void ecore_iov_set_link(struct ecore_hwfn *p_hwfn,
			u16 vfid,
			struct ecore_mcp_link_params *params,
			struct ecore_mcp_link_state *link,
			struct ecore_mcp_link_capabilities *p_caps)
{
	struct ecore_vf_info *p_vf = ecore_iov_get_vf_info(p_hwfn, vfid, false);
	struct ecore_bulletin_content *p_bulletin;

	if (!p_vf)
		return;

	p_bulletin = p_vf->bulletin.p_virt;
	p_bulletin->req_autoneg = params->speed.autoneg;
	p_bulletin->req_adv_speed = params->speed.advertised_speeds;
	p_bulletin->req_forced_speed = params->speed.forced_speed;
	p_bulletin->req_autoneg_pause = params->pause.autoneg;
	p_bulletin->req_forced_rx = params->pause.forced_rx;
	p_bulletin->req_forced_tx = params->pause.forced_tx;
	p_bulletin->req_loopback = params->loopback_mode;

	p_bulletin->link_up = link->link_up;
	p_bulletin->speed = link->speed;
	p_bulletin->full_duplex = link->full_duplex;
	p_bulletin->autoneg = link->an;
	p_bulletin->autoneg_complete = link->an_complete;
	p_bulletin->parallel_detection = link->parallel_detection;
	p_bulletin->pfc_enabled = link->pfc_enabled;
	p_bulletin->partner_adv_speed = link->partner_adv_speed;
	p_bulletin->partner_tx_flow_ctrl_en = link->partner_tx_flow_ctrl_en;
	p_bulletin->partner_rx_flow_ctrl_en = link->partner_rx_flow_ctrl_en;
	p_bulletin->partner_adv_pause = link->partner_adv_pause;
	p_bulletin->sfp_tx_fault = link->sfp_tx_fault;

	p_bulletin->capability_speed = p_caps->speed_capabilities;
}

enum _ecore_status_t ecore_iov_release_hw_for_vf(struct ecore_hwfn *p_hwfn,
						 struct ecore_ptt *p_ptt,
						 u16 rel_vf_id)
{
	struct ecore_mcp_link_capabilities caps;
	struct ecore_mcp_link_params params;
	struct ecore_mcp_link_state link;
	struct ecore_vf_info *vf = OSAL_NULL;

	vf = ecore_iov_get_vf_info(p_hwfn, rel_vf_id, true);
	if (!vf) {
		DP_ERR(p_hwfn, "ecore_iov_release_hw_for_vf : vf is NULL\n");
		return ECORE_UNKNOWN_ERROR;
	}

	if (vf->bulletin.p_virt)
		OSAL_MEMSET(vf->bulletin.p_virt, 0,
			    sizeof(*vf->bulletin.p_virt));

	OSAL_MEMSET(&vf->p_vf_info, 0, sizeof(vf->p_vf_info));

	/* Get the link configuration back in bulletin so
	 * that when VFs are re-enabled they get the actual
	 * link configuration.
	 */
	OSAL_MEMCPY(&params, ecore_mcp_get_link_params(p_hwfn), sizeof(params));
	OSAL_MEMCPY(&link, ecore_mcp_get_link_state(p_hwfn), sizeof(link));
	OSAL_MEMCPY(&caps, ecore_mcp_get_link_capabilities(p_hwfn),
		    sizeof(caps));
	ecore_iov_set_link(p_hwfn, rel_vf_id, &params, &link, &caps);

	/* Forget the VF's acquisition message */
	OSAL_MEMSET(&vf->acquire, 0, sizeof(vf->acquire));

	/* disablng interrupts and resetting permission table was done during
	 * vf-close, however, we could get here without going through vf_close
	 */
	/* Disable Interrupts for VF */
	ecore_iov_vf_igu_set_int(p_hwfn, p_ptt, vf, 0);

	/* Reset Permission table */
	ecore_iov_config_perm_table(p_hwfn, p_ptt, vf, 0);

	vf->num_rxqs = 0;
	vf->num_txqs = 0;
	ecore_iov_free_vf_igu_sbs(p_hwfn, p_ptt, vf);

	if (vf->b_init) {
		vf->b_init = false;
		p_hwfn->pf_iov_info->active_vfs[vf->relative_vf_id / 64] &=
					~(1ULL << (vf->relative_vf_id / 64));

		if (IS_LEAD_HWFN(p_hwfn))
			p_hwfn->p_dev->p_iov_info->num_vfs--;
	}

	return ECORE_SUCCESS;
}

static bool ecore_iov_tlv_supported(u16 tlvtype)
{
	return tlvtype > CHANNEL_TLV_NONE && tlvtype < CHANNEL_TLV_MAX;
}

static void ecore_iov_lock_vf_pf_channel(struct ecore_hwfn *p_hwfn,
					 struct ecore_vf_info *vf, u16 tlv)
{
	/* lock the channel */
	/* mutex_lock(&vf->op_mutex); @@@TBD MichalK - add lock... */

	/* record the locking op */
	/* vf->op_current = tlv; @@@TBD MichalK */

	/* log the lock */
	if (ecore_iov_tlv_supported(tlv))
		DP_VERBOSE(p_hwfn,
			   ECORE_MSG_IOV,
			   "VF[%d]: vf pf channel locked by %s\n",
			   vf->abs_vf_id,
			   ecore_channel_tlvs_string[tlv]);
	else
		DP_VERBOSE(p_hwfn,
			   ECORE_MSG_IOV,
			   "VF[%d]: vf pf channel locked by %04x\n",
			   vf->abs_vf_id, tlv);
}

static void ecore_iov_unlock_vf_pf_channel(struct ecore_hwfn *p_hwfn,
					   struct ecore_vf_info *vf,
					   u16 expected_tlv)
{
	/* log the unlock */
	if (ecore_iov_tlv_supported(expected_tlv))
		DP_VERBOSE(p_hwfn,
			   ECORE_MSG_IOV,
			   "VF[%d]: vf pf channel unlocked by %s\n",
			   vf->abs_vf_id,
			   ecore_channel_tlvs_string[expected_tlv]);
	else
		DP_VERBOSE(p_hwfn,
			   ECORE_MSG_IOV,
			   "VF[%d]: vf pf channel unlocked by %04x\n",
			   vf->abs_vf_id, expected_tlv);

	/* record the locking op */
	/* vf->op_current = CHANNEL_TLV_NONE; */
}

/* place a given tlv on the tlv buffer, continuing current tlv list */
void *ecore_add_tlv(struct ecore_hwfn *p_hwfn,
		    u8 **offset, u16 type, u16 length)
{
	struct channel_tlv *tl = (struct channel_tlv *)*offset;

	tl->type = type;
	tl->length = length;

	/* Offset should keep pointing to next TLV (the end of the last) */
	*offset += length;

	/* Return a pointer to the start of the added tlv */
	return *offset - length;
}

/* list the types and lengths of the tlvs on the buffer */
void ecore_dp_tlv_list(struct ecore_hwfn *p_hwfn, void *tlvs_list)
{
	u16 i = 1, total_length = 0;
	struct channel_tlv *tlv;

	do {
		/* cast current tlv list entry to channel tlv header */
		tlv = (struct channel_tlv *)((u8 *)tlvs_list + total_length);

		/* output tlv */
		if (ecore_iov_tlv_supported(tlv->type))
			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "TLV number %d: type %s, length %d\n",
				   i, ecore_channel_tlvs_string[tlv->type],
				   tlv->length);
		else
			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "TLV number %d: type %d, length %d\n",
				   i, tlv->type, tlv->length);

		if (tlv->type == CHANNEL_TLV_LIST_END)
			return;

		/* Validate entry - protect against malicious VFs */
		if (!tlv->length) {
			DP_NOTICE(p_hwfn, false, "TLV of length 0 found\n");
			return;
		}
		total_length += tlv->length;
		if (total_length >= sizeof(struct tlv_buffer_size)) {
			DP_NOTICE(p_hwfn, false, "TLV ==> Buffer overflow\n");
			return;
		}

		i++;
	} while (1);
}

static void ecore_iov_send_response(struct ecore_hwfn *p_hwfn,
				    struct ecore_ptt *p_ptt,
				    struct ecore_vf_info *p_vf,
				    u16 length, u8 status)
{
	struct ecore_iov_vf_mbx *mbx = &p_vf->vf_mbx;
	struct ecore_dmae_params params;
	u8 eng_vf_id;

	mbx->reply_virt->default_resp.hdr.status = status;

	ecore_dp_tlv_list(p_hwfn, mbx->reply_virt);

#ifdef CONFIG_ECORE_SW_CHANNEL
	mbx->sw_mbx.response_size =
	    length + sizeof(struct channel_list_end_tlv);

	if (!p_hwfn->p_dev->b_hw_channel)
		return;
#endif

	eng_vf_id = p_vf->abs_vf_id;

	OSAL_MEMSET(&params, 0, sizeof(struct ecore_dmae_params));
	params.flags = ECORE_DMAE_FLAG_VF_DST;
	params.dst_vfid = eng_vf_id;

	ecore_dmae_host2host(p_hwfn, p_ptt, mbx->reply_phys + sizeof(u64),
			     mbx->req_virt->first_tlv.reply_address +
			     sizeof(u64),
			     (sizeof(union pfvf_tlvs) - sizeof(u64)) / 4,
			     &params);

	ecore_dmae_host2host(p_hwfn, p_ptt, mbx->reply_phys,
			     mbx->req_virt->first_tlv.reply_address,
			     sizeof(u64) / 4, &params);

	REG_WR(p_hwfn,
	       GTT_BAR0_MAP_REG_USDM_RAM +
	       USTORM_VF_PF_CHANNEL_READY_OFFSET(eng_vf_id), 1);
}

static u16 ecore_iov_vport_to_tlv(struct ecore_hwfn *p_hwfn,
				  enum ecore_iov_vport_update_flag flag)
{
	switch (flag) {
	case ECORE_IOV_VP_UPDATE_ACTIVATE:
		return CHANNEL_TLV_VPORT_UPDATE_ACTIVATE;
	case ECORE_IOV_VP_UPDATE_VLAN_STRIP:
		return CHANNEL_TLV_VPORT_UPDATE_VLAN_STRIP;
	case ECORE_IOV_VP_UPDATE_TX_SWITCH:
		return CHANNEL_TLV_VPORT_UPDATE_TX_SWITCH;
	case ECORE_IOV_VP_UPDATE_MCAST:
		return CHANNEL_TLV_VPORT_UPDATE_MCAST;
	case ECORE_IOV_VP_UPDATE_ACCEPT_PARAM:
		return CHANNEL_TLV_VPORT_UPDATE_ACCEPT_PARAM;
	case ECORE_IOV_VP_UPDATE_RSS:
		return CHANNEL_TLV_VPORT_UPDATE_RSS;
	case ECORE_IOV_VP_UPDATE_ACCEPT_ANY_VLAN:
		return CHANNEL_TLV_VPORT_UPDATE_ACCEPT_ANY_VLAN;
	case ECORE_IOV_VP_UPDATE_SGE_TPA:
		return CHANNEL_TLV_VPORT_UPDATE_SGE_TPA;
	default:
		return 0;
	}
}

static u16 ecore_iov_prep_vp_update_resp_tlvs(struct ecore_hwfn *p_hwfn,
					      struct ecore_vf_info *p_vf,
					      struct ecore_iov_vf_mbx *p_mbx,
					      u8 status, u16 tlvs_mask,
					      u16 tlvs_accepted)
{
	struct pfvf_def_resp_tlv *resp;
	u16 size, total_len, i;

	OSAL_MEMSET(p_mbx->reply_virt, 0, sizeof(union pfvf_tlvs));
	p_mbx->offset = (u8 *)p_mbx->reply_virt;
	size = sizeof(struct pfvf_def_resp_tlv);
	total_len = size;

	ecore_add_tlv(p_hwfn, &p_mbx->offset, CHANNEL_TLV_VPORT_UPDATE, size);

	/* Prepare response for all extended tlvs if they are found by PF */
	for (i = 0; i < ECORE_IOV_VP_UPDATE_MAX; i++) {
		if (!(tlvs_mask & (1 << i)))
			continue;

		resp = ecore_add_tlv(p_hwfn, &p_mbx->offset,
				     ecore_iov_vport_to_tlv(p_hwfn, i), size);

		if (tlvs_accepted & (1 << i))
			resp->hdr.status = status;
		else
			resp->hdr.status = PFVF_STATUS_NOT_SUPPORTED;

		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "VF[%d] - vport_update resp: TLV %d, status %02x\n",
			   p_vf->relative_vf_id,
			   ecore_iov_vport_to_tlv(p_hwfn, i), resp->hdr.status);

		total_len += size;
	}

	ecore_add_tlv(p_hwfn, &p_mbx->offset, CHANNEL_TLV_LIST_END,
		      sizeof(struct channel_list_end_tlv));

	return total_len;
}

static void ecore_iov_prepare_resp(struct ecore_hwfn *p_hwfn,
				   struct ecore_ptt *p_ptt,
				   struct ecore_vf_info *vf_info,
				   u16 type, u16 length, u8 status)
{
	struct ecore_iov_vf_mbx *mbx = &vf_info->vf_mbx;

	mbx->offset = (u8 *)mbx->reply_virt;

	ecore_add_tlv(p_hwfn, &mbx->offset, type, length);
	ecore_add_tlv(p_hwfn, &mbx->offset, CHANNEL_TLV_LIST_END,
		      sizeof(struct channel_list_end_tlv));

	ecore_iov_send_response(p_hwfn, p_ptt, vf_info, length, status);

	OSAL_IOV_PF_RESP_TYPE(p_hwfn, vf_info->relative_vf_id, status);
}

struct ecore_public_vf_info
*ecore_iov_get_public_vf_info(struct ecore_hwfn *p_hwfn,
			      u16 relative_vf_id,
			      bool b_enabled_only)
{
	struct ecore_vf_info *vf = OSAL_NULL;

	vf = ecore_iov_get_vf_info(p_hwfn, relative_vf_id, b_enabled_only);
	if (!vf)
		return OSAL_NULL;

	return &vf->p_vf_info;
}

static void ecore_iov_vf_cleanup(struct ecore_hwfn *p_hwfn,
				 struct ecore_vf_info *p_vf)
{
	u32 i;
	p_vf->vf_bulletin = 0;
	p_vf->vport_instance = 0;
	p_vf->configured_features = 0;

	/* If VF previously requested less resources, go back to default */
	p_vf->num_rxqs = p_vf->num_sbs;
	p_vf->num_txqs = p_vf->num_sbs;

	p_vf->num_active_rxqs = 0;

	for (i = 0; i < ECORE_MAX_VF_CHAINS_PER_PF; i++)
		p_vf->vf_queues[i].rxq_active = 0;

	OSAL_MEMSET(&p_vf->shadow_config, 0, sizeof(p_vf->shadow_config));
	OSAL_MEMSET(&p_vf->acquire, 0, sizeof(p_vf->acquire));
	OSAL_IOV_VF_CLEANUP(p_hwfn, p_vf->relative_vf_id);
}

static u8 ecore_iov_vf_mbx_acquire_resc(struct ecore_hwfn *p_hwfn,
					struct ecore_ptt *p_ptt,
					struct ecore_vf_info *p_vf,
					struct vf_pf_resc_request *p_req,
					struct pf_vf_resc *p_resp)
{
	int i;

	/* Queue related information */
	p_resp->num_rxqs = p_vf->num_rxqs;
	p_resp->num_txqs = p_vf->num_txqs;
	p_resp->num_sbs = p_vf->num_sbs;

	for (i = 0; i < p_resp->num_sbs; i++) {
		p_resp->hw_sbs[i].hw_sb_id = p_vf->igu_sbs[i];
		/* TODO - what's this sb_qid field? Is it deprecated?
		 * or is there an ecore_client that looks at this?
		 */
		p_resp->hw_sbs[i].sb_qid = 0;
	}

	/* These fields are filled for backward compatibility.
	 * Unused by modern vfs.
	 */
	for (i = 0; i < p_resp->num_rxqs; i++) {
		ecore_fw_l2_queue(p_hwfn, p_vf->vf_queues[i].fw_rx_qid,
				  (u16 *)&p_resp->hw_qid[i]);
		p_resp->cid[i] = p_vf->vf_queues[i].fw_cid;
	}

	/* Filter related information */
	p_resp->num_mac_filters = OSAL_MIN_T(u8, p_vf->num_mac_filters,
					     p_req->num_mac_filters);
	p_resp->num_vlan_filters = OSAL_MIN_T(u8, p_vf->num_vlan_filters,
					      p_req->num_vlan_filters);

	/* This isn't really needed/enforced, but some legacy VFs might depend
	 * on the correct filling of this field.
	 */
	p_resp->num_mc_filters = ECORE_MAX_MC_ADDRS;

	/* Validate sufficient resources for VF */
	if (p_resp->num_rxqs < p_req->num_rxqs ||
	    p_resp->num_txqs < p_req->num_txqs ||
	    p_resp->num_sbs < p_req->num_sbs ||
	    p_resp->num_mac_filters < p_req->num_mac_filters ||
	    p_resp->num_vlan_filters < p_req->num_vlan_filters ||
	    p_resp->num_mc_filters < p_req->num_mc_filters) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "VF[%d] - Insufficient resources: rxq [%02x/%02x]"
			   " txq [%02x/%02x] sbs [%02x/%02x] mac [%02x/%02x]"
			   " vlan [%02x/%02x] mc [%02x/%02x]\n",
			   p_vf->abs_vf_id,
			   p_req->num_rxqs, p_resp->num_rxqs,
			   p_req->num_rxqs, p_resp->num_txqs,
			   p_req->num_sbs, p_resp->num_sbs,
			   p_req->num_mac_filters, p_resp->num_mac_filters,
			   p_req->num_vlan_filters, p_resp->num_vlan_filters,
			   p_req->num_mc_filters, p_resp->num_mc_filters);

		/* Some legacy OSes are incapable of correctly handling this
		 * failure.
		 */
		if ((p_vf->acquire.vfdev_info.eth_fp_hsi_minor ==
		     ETH_HSI_VER_NO_PKT_LEN_TUNN) &&
		    (p_vf->acquire.vfdev_info.os_type ==
		     VFPF_ACQUIRE_OS_WINDOWS))
			return PFVF_STATUS_SUCCESS;

		return PFVF_STATUS_NO_RESOURCE;
	}

	return PFVF_STATUS_SUCCESS;
}

static void ecore_iov_vf_mbx_acquire_stats(struct ecore_hwfn *p_hwfn,
					   struct pfvf_stats_info *p_stats)
{
	p_stats->mstats.address = PXP_VF_BAR0_START_MSDM_ZONE_B +
				  OFFSETOF(struct mstorm_vf_zone,
					   non_trigger.eth_queue_stat);
	p_stats->mstats.len = sizeof(struct eth_mstorm_per_queue_stat);
	p_stats->ustats.address = PXP_VF_BAR0_START_USDM_ZONE_B +
				  OFFSETOF(struct ustorm_vf_zone,
					   non_trigger.eth_queue_stat);
	p_stats->ustats.len = sizeof(struct eth_ustorm_per_queue_stat);
	p_stats->pstats.address = PXP_VF_BAR0_START_PSDM_ZONE_B +
				  OFFSETOF(struct pstorm_vf_zone,
					   non_trigger.eth_queue_stat);
	p_stats->pstats.len = sizeof(struct eth_pstorm_per_queue_stat);
	p_stats->tstats.address = 0;
	p_stats->tstats.len = 0;
}

static void ecore_iov_vf_mbx_acquire(struct ecore_hwfn       *p_hwfn,
				     struct ecore_ptt	     *p_ptt,
				     struct ecore_vf_info    *vf)
{
	struct ecore_iov_vf_mbx *mbx = &vf->vf_mbx;
	struct pfvf_acquire_resp_tlv *resp = &mbx->reply_virt->acquire_resp;
	struct pf_vf_pfdev_info *pfdev_info = &resp->pfdev_info;
	struct vfpf_acquire_tlv *req = &mbx->req_virt->acquire;
	u8 vfpf_status = PFVF_STATUS_NOT_SUPPORTED;
	struct pf_vf_resc *resc = &resp->resc;
	enum _ecore_status_t rc;

	OSAL_MEMSET(resp, 0, sizeof(*resp));

	/* Write the PF version so that VF would know which version
	 * is supported - might be later overridden. This guarantees that
	 * VF could recognize legacy PF based on lack of versions in reply.
	 */
	pfdev_info->major_fp_hsi = ETH_HSI_VER_MAJOR;
	pfdev_info->minor_fp_hsi = ETH_HSI_VER_MINOR;

	/* Validate FW compatibility */
	if (req->vfdev_info.eth_fp_hsi_major != ETH_HSI_VER_MAJOR) {
		if (req->vfdev_info.capabilities &
		    VFPF_ACQUIRE_CAP_PRE_FP_HSI) {
			struct vf_pf_vfdev_info *p_vfdev = &req->vfdev_info;

			/* This legacy support would need to be removed once
			 * the major has changed.
			 */
			OSAL_BUILD_BUG_ON(ETH_HSI_VER_MAJOR != 3);

			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "VF[%d] is pre-fastpath HSI\n",
				   vf->abs_vf_id);
			p_vfdev->eth_fp_hsi_major = ETH_HSI_VER_MAJOR;
			p_vfdev->eth_fp_hsi_minor = ETH_HSI_VER_NO_PKT_LEN_TUNN;
		} else {
			DP_INFO(p_hwfn,
				"VF[%d] needs fastpath HSI %02x.%02x, which is"
				" incompatible with loaded FW's faspath"
				" HSI %02x.%02x\n",
				vf->abs_vf_id,
				req->vfdev_info.eth_fp_hsi_major,
				req->vfdev_info.eth_fp_hsi_minor,
				ETH_HSI_VER_MAJOR, ETH_HSI_VER_MINOR);

			goto out;
		}
	}

	/* On 100g PFs, prevent old VFs from loading */
	if ((p_hwfn->p_dev->num_hwfns > 1) &&
	    !(req->vfdev_info.capabilities & VFPF_ACQUIRE_CAP_100G)) {
		DP_INFO(p_hwfn,
			"VF[%d] is running an old driver that doesn't support"
			" 100g\n",
			vf->abs_vf_id);
		goto out;
	}

#ifndef __EXTRACT__LINUX__
	if (OSAL_IOV_VF_ACQUIRE(p_hwfn, vf->relative_vf_id) != ECORE_SUCCESS) {
		vfpf_status = PFVF_STATUS_NOT_SUPPORTED;
		goto out;
	}
#endif

	/* Store the acquire message */
	OSAL_MEMCPY(&vf->acquire, req, sizeof(vf->acquire));

	vf->opaque_fid = req->vfdev_info.opaque_fid;

	vf->vf_bulletin = req->bulletin_addr;
	vf->bulletin.size = (vf->bulletin.size < req->bulletin_size) ?
	    vf->bulletin.size : req->bulletin_size;

	/* fill in pfdev info */
	pfdev_info->chip_num = p_hwfn->p_dev->chip_num;
	pfdev_info->db_size = 0;	/* @@@ TBD MichalK Vf Doorbells */
	pfdev_info->indices_per_sb = PIS_PER_SB;

	pfdev_info->capabilities = PFVF_ACQUIRE_CAP_DEFAULT_UNTAGGED |
				   PFVF_ACQUIRE_CAP_POST_FW_OVERRIDE;
	if (p_hwfn->p_dev->num_hwfns > 1)
		pfdev_info->capabilities |= PFVF_ACQUIRE_CAP_100G;

	ecore_iov_vf_mbx_acquire_stats(p_hwfn, &pfdev_info->stats_info);

	OSAL_MEMCPY(pfdev_info->port_mac, p_hwfn->hw_info.hw_mac_addr,
		    ETH_ALEN);

	pfdev_info->fw_major = FW_MAJOR_VERSION;
	pfdev_info->fw_minor = FW_MINOR_VERSION;
	pfdev_info->fw_rev = FW_REVISION_VERSION;
	pfdev_info->fw_eng = FW_ENGINEERING_VERSION;

	/* Incorrect when legacy, but doesn't matter as legacy isn't reading
	 * this field.
	 */
	pfdev_info->minor_fp_hsi = OSAL_MIN_T(u8, ETH_HSI_VER_MINOR,
					      req->vfdev_info.eth_fp_hsi_minor);
	pfdev_info->os_type = OSAL_IOV_GET_OS_TYPE();
	ecore_mcp_get_mfw_ver(p_hwfn, p_ptt, &pfdev_info->mfw_ver,
			      OSAL_NULL);

	pfdev_info->dev_type = p_hwfn->p_dev->type;
	pfdev_info->chip_rev = p_hwfn->p_dev->chip_rev;

	/* Fill resources available to VF; Make sure there are enough to
	 * satisfy the VF's request.
	 */
	vfpf_status = ecore_iov_vf_mbx_acquire_resc(p_hwfn, p_ptt, vf,
						    &req->resc_request, resc);
	if (vfpf_status != PFVF_STATUS_SUCCESS)
		goto out;

	/* Start the VF in FW */
	rc = ecore_sp_vf_start(p_hwfn, vf);
	if (rc != ECORE_SUCCESS) {
		DP_NOTICE(p_hwfn, true, "Failed to start VF[%02x]\n",
			  vf->abs_vf_id);
		vfpf_status = PFVF_STATUS_FAILURE;
		goto out;
	}

	/* Fill agreed size of bulletin board in response, and post
	 * an initial image to the bulletin board.
	 */
	resp->bulletin_size = vf->bulletin.size;
	ecore_iov_post_vf_bulletin(p_hwfn, vf->relative_vf_id, p_ptt);

	DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
		   "VF[%d] ACQUIRE_RESPONSE: pfdev_info- chip_num=0x%x,"
		   " db_size=%d, idx_per_sb=%d, pf_cap=0x%lx\n"
		   "resources- n_rxq-%d, n_txq-%d, n_sbs-%d, n_macs-%d,"
		   " n_vlans-%d, n_mcs-%d\n",
		   vf->abs_vf_id, resp->pfdev_info.chip_num,
		   resp->pfdev_info.db_size, resp->pfdev_info.indices_per_sb,
		   (unsigned long)resp->pfdev_info.capabilities, resc->num_rxqs,
		   resc->num_txqs, resc->num_sbs, resc->num_mac_filters,
		   resc->num_vlan_filters, resc->num_mc_filters);

	vf->state = VF_ACQUIRED;

out:
	/* Prepare Response */
	ecore_iov_prepare_resp(p_hwfn, p_ptt, vf, CHANNEL_TLV_ACQUIRE,
			       sizeof(struct pfvf_acquire_resp_tlv),
			       vfpf_status);
}

static enum _ecore_status_t
__ecore_iov_spoofchk_set(struct ecore_hwfn *p_hwfn,
			 struct ecore_vf_info *p_vf, bool val)
{
	struct ecore_sp_vport_update_params params;
	enum _ecore_status_t rc;

	if (val == p_vf->spoof_chk) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "Spoofchk value[%d] is already configured\n", val);
		return ECORE_SUCCESS;
	}

	OSAL_MEMSET(&params, 0, sizeof(struct ecore_sp_vport_update_params));
	params.opaque_fid = p_vf->opaque_fid;
	params.vport_id = p_vf->vport_id;
	params.update_anti_spoofing_en_flg = 1;
	params.anti_spoofing_en = val;

	rc = ecore_sp_vport_update(p_hwfn, &params, ECORE_SPQ_MODE_EBLOCK,
				   OSAL_NULL);
	if (rc == ECORE_SUCCESS) {
		p_vf->spoof_chk = val;
		p_vf->req_spoofchk_val = p_vf->spoof_chk;
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "Spoofchk val[%d] configured\n", val);
	} else {
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "Spoofchk configuration[val:%d] failed for VF[%d]\n",
			   val, p_vf->relative_vf_id);
	}

	return rc;
}

static enum _ecore_status_t
ecore_iov_reconfigure_unicast_vlan(struct ecore_hwfn *p_hwfn,
				   struct ecore_vf_info *p_vf)
{
	struct ecore_filter_ucast filter;
	enum _ecore_status_t rc = ECORE_SUCCESS;
	int i;

	OSAL_MEMSET(&filter, 0, sizeof(filter));
	filter.is_rx_filter = 1;
	filter.is_tx_filter = 1;
	filter.vport_to_add_to = p_vf->vport_id;
	filter.opcode = ECORE_FILTER_ADD;

	/* Reconfigure vlans */
	for (i = 0; i < ECORE_ETH_VF_NUM_VLAN_FILTERS + 1; i++) {
		if (!p_vf->shadow_config.vlans[i].used)
			continue;

		filter.type = ECORE_FILTER_VLAN;
		filter.vlan = p_vf->shadow_config.vlans[i].vid;
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "Reconfiguring VLAN [0x%04x] for VF [%04x]\n",
			   filter.vlan, p_vf->relative_vf_id);
		rc = ecore_sp_eth_filter_ucast(p_hwfn,
					       p_vf->opaque_fid,
					       &filter,
					       ECORE_SPQ_MODE_CB,
						       OSAL_NULL);
		if (rc) {
			DP_NOTICE(p_hwfn, true,
				  "Failed to configure VLAN [%04x]"
				  " to VF [%04x]\n",
				  filter.vlan, p_vf->relative_vf_id);
			break;
		}
	}

	return rc;
}

static enum _ecore_status_t
ecore_iov_reconfigure_unicast_shadow(struct ecore_hwfn *p_hwfn,
				     struct ecore_vf_info *p_vf, u64 events)
{
	enum _ecore_status_t rc = ECORE_SUCCESS;

	/*TODO - what about MACs? */

	if ((events & (1 << VLAN_ADDR_FORCED)) &&
	    !(p_vf->configured_features & (1 << VLAN_ADDR_FORCED)))
		rc = ecore_iov_reconfigure_unicast_vlan(p_hwfn, p_vf);

	return rc;
}

static int ecore_iov_configure_vport_forced(struct ecore_hwfn *p_hwfn,
					    struct ecore_vf_info *p_vf,
					    u64 events)
{
	enum _ecore_status_t rc = ECORE_SUCCESS;
	struct ecore_filter_ucast filter;

	if (!p_vf->vport_instance)
		return ECORE_INVAL;

	if (events & (1 << MAC_ADDR_FORCED)) {
		/* Since there's no way [currently] of removing the MAC,
		 * we can always assume this means we need to force it.
		 */
		OSAL_MEMSET(&filter, 0, sizeof(filter));
		filter.type = ECORE_FILTER_MAC;
		filter.opcode = ECORE_FILTER_REPLACE;
		filter.is_rx_filter = 1;
		filter.is_tx_filter = 1;
		filter.vport_to_add_to = p_vf->vport_id;
		OSAL_MEMCPY(filter.mac, p_vf->bulletin.p_virt->mac, ETH_ALEN);

		rc = ecore_sp_eth_filter_ucast(p_hwfn, p_vf->opaque_fid,
					       &filter,
					       ECORE_SPQ_MODE_CB, OSAL_NULL);
		if (rc) {
			DP_NOTICE(p_hwfn, true,
				  "PF failed to configure MAC for VF\n");
			return rc;
		}

		p_vf->configured_features |= 1 << MAC_ADDR_FORCED;
	}

	if (events & (1 << VLAN_ADDR_FORCED)) {
		struct ecore_sp_vport_update_params vport_update;
		u8 removal;
		int i;

		OSAL_MEMSET(&filter, 0, sizeof(filter));
		filter.type = ECORE_FILTER_VLAN;
		filter.is_rx_filter = 1;
		filter.is_tx_filter = 1;
		filter.vport_to_add_to = p_vf->vport_id;
		filter.vlan = p_vf->bulletin.p_virt->pvid;
		filter.opcode = filter.vlan ? ECORE_FILTER_REPLACE :
		    ECORE_FILTER_FLUSH;

		/* Send the ramrod */
		rc = ecore_sp_eth_filter_ucast(p_hwfn, p_vf->opaque_fid,
					       &filter,
					       ECORE_SPQ_MODE_CB, OSAL_NULL);
		if (rc) {
			DP_NOTICE(p_hwfn, true,
				  "PF failed to configure VLAN for VF\n");
			return rc;
		}

		/* Update the default-vlan & silent vlan stripping */
		OSAL_MEMSET(&vport_update, 0, sizeof(vport_update));
		vport_update.opaque_fid = p_vf->opaque_fid;
		vport_update.vport_id = p_vf->vport_id;
		vport_update.update_default_vlan_enable_flg = 1;
		vport_update.default_vlan_enable_flg = filter.vlan ? 1 : 0;
		vport_update.update_default_vlan_flg = 1;
		vport_update.default_vlan = filter.vlan;

		vport_update.update_inner_vlan_removal_flg = 1;
		removal = filter.vlan ?
		    1 : p_vf->shadow_config.inner_vlan_removal;
		vport_update.inner_vlan_removal_flg = removal;
		vport_update.silent_vlan_removal_flg = filter.vlan ? 1 : 0;
		rc = ecore_sp_vport_update(p_hwfn, &vport_update,
					   ECORE_SPQ_MODE_EBLOCK, OSAL_NULL);
		if (rc) {
			DP_NOTICE(p_hwfn, true,
				  "PF failed to configure VF vport for vlan\n");
			return rc;
		}

		/* Update all the Rx queues */
		for (i = 0; i < ECORE_MAX_VF_CHAINS_PER_PF; i++) {
			u16 qid;

			if (!p_vf->vf_queues[i].rxq_active)
				continue;

			qid = p_vf->vf_queues[i].fw_rx_qid;

			rc = ecore_sp_eth_rx_queues_update(p_hwfn, qid,
						   1, 0, 1,
						   ECORE_SPQ_MODE_EBLOCK,
						   OSAL_NULL);
			if (rc) {
				DP_NOTICE(p_hwfn, true,
					  "Failed to send Rx update"
					  " fo queue[0x%04x]\n",
					  qid);
				return rc;
			}
		}

		if (filter.vlan)
			p_vf->configured_features |= 1 << VLAN_ADDR_FORCED;
		else
			p_vf->configured_features &= ~(1 << VLAN_ADDR_FORCED);
	}

	/* If forced features are terminated, we need to configure the shadow
	 * configuration back again.
	 */
	if (events)
		ecore_iov_reconfigure_unicast_shadow(p_hwfn, p_vf, events);

	return rc;
}

static void ecore_iov_vf_mbx_start_vport(struct ecore_hwfn *p_hwfn,
					 struct ecore_ptt *p_ptt,
					 struct ecore_vf_info *vf)
{
	struct ecore_sp_vport_start_params params = { 0 };
	struct ecore_iov_vf_mbx *mbx = &vf->vf_mbx;
	struct vfpf_vport_start_tlv *start;
	u8 status = PFVF_STATUS_SUCCESS;
	struct ecore_vf_info *vf_info;
	u64 *p_bitmap;
	int sb_id;
	enum _ecore_status_t rc;

	vf_info = ecore_iov_get_vf_info(p_hwfn, (u16)vf->relative_vf_id, true);
	if (!vf_info) {
		DP_NOTICE(p_hwfn->p_dev, true,
			  "Failed to get VF info, invalid vfid [%d]\n",
			  vf->relative_vf_id);
		return;
	}

	vf->state = VF_ENABLED;
	start = &mbx->req_virt->start_vport;

	/* Initialize Status block in CAU */
	for (sb_id = 0; sb_id < vf->num_sbs; sb_id++) {
		if (!start->sb_addr[sb_id]) {
			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "VF[%d] did not fill the address of SB %d\n",
				   vf->relative_vf_id, sb_id);
			break;
		}

		ecore_int_cau_conf_sb(p_hwfn, p_ptt,
				      start->sb_addr[sb_id],
				      vf->igu_sbs[sb_id],
				      vf->abs_vf_id, 1);
	}
	ecore_iov_enable_vf_traffic(p_hwfn, p_ptt, vf);

	vf->mtu = start->mtu;
	vf->shadow_config.inner_vlan_removal = start->inner_vlan_removal;

	/* Take into consideration configuration forced by hypervisor;
	 * If none is configured, use the supplied VF values [for old
	 * vfs that would still be fine, since they passed '0' as padding].
	 */
	p_bitmap = &vf_info->bulletin.p_virt->valid_bitmap;
	if (!(*p_bitmap & (1 << VFPF_BULLETIN_UNTAGGED_DEFAULT_FORCED))) {
		u8 vf_req = start->only_untagged;

		vf_info->bulletin.p_virt->default_only_untagged = vf_req;
		*p_bitmap |= 1 << VFPF_BULLETIN_UNTAGGED_DEFAULT;
	}

	params.tpa_mode = start->tpa_mode;
	params.remove_inner_vlan = start->inner_vlan_removal;
	params.tx_switching = true;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_FPGA(p_hwfn->p_dev)) {
		DP_NOTICE(p_hwfn, false,
			  "FPGA: Don't config VF for Tx-switching [no pVFC]\n");
		params.tx_switching = false;
	}
#endif

	params.only_untagged = vf_info->bulletin.p_virt->default_only_untagged;
	params.drop_ttl0 = false;
	params.concrete_fid = vf->concrete_fid;
	params.opaque_fid = vf->opaque_fid;
	params.vport_id = vf->vport_id;
	params.max_buffers_per_cqe = start->max_buffers_per_cqe;
	params.mtu = vf->mtu;
	params.check_mac = true;

	rc = ecore_sp_eth_vport_start(p_hwfn, &params);
	if (rc != ECORE_SUCCESS) {
		DP_ERR(p_hwfn,
		       "ecore_iov_vf_mbx_start_vport returned error %d\n", rc);
		status = PFVF_STATUS_FAILURE;
	} else {
		vf->vport_instance++;

		/* Force configuration if needed on the newly opened vport */
		ecore_iov_configure_vport_forced(p_hwfn, vf, *p_bitmap);
		OSAL_IOV_POST_START_VPORT(p_hwfn, vf->relative_vf_id,
					  vf->vport_id, vf->opaque_fid);
		__ecore_iov_spoofchk_set(p_hwfn, vf, vf->req_spoofchk_val);
	}

	ecore_iov_prepare_resp(p_hwfn, p_ptt, vf, CHANNEL_TLV_VPORT_START,
			       sizeof(struct pfvf_def_resp_tlv), status);
}

static void ecore_iov_vf_mbx_stop_vport(struct ecore_hwfn *p_hwfn,
					struct ecore_ptt *p_ptt,
					struct ecore_vf_info *vf)
{
	u8 status = PFVF_STATUS_SUCCESS;
	enum _ecore_status_t rc;

	vf->vport_instance--;
	vf->spoof_chk = false;

	rc = ecore_sp_vport_stop(p_hwfn, vf->opaque_fid, vf->vport_id);
	if (rc != ECORE_SUCCESS) {
		DP_ERR(p_hwfn,
		       "ecore_iov_vf_mbx_stop_vport returned error %d\n", rc);
		status = PFVF_STATUS_FAILURE;
	}

	/* Forget the configuration on the vport */
	vf->configured_features = 0;
	OSAL_MEMSET(&vf->shadow_config, 0, sizeof(vf->shadow_config));

	ecore_iov_prepare_resp(p_hwfn, p_ptt, vf, CHANNEL_TLV_VPORT_TEARDOWN,
			       sizeof(struct pfvf_def_resp_tlv), status);
}

static void ecore_iov_vf_mbx_start_rxq_resp(struct ecore_hwfn *p_hwfn,
					    struct ecore_ptt *p_ptt,
					    struct ecore_vf_info *vf,
					    u8 status, bool b_legacy)
{
	struct ecore_iov_vf_mbx *mbx = &vf->vf_mbx;
	struct pfvf_start_queue_resp_tlv *p_tlv;
	struct vfpf_start_rxq_tlv *req;
	u16 length;

	mbx->offset = (u8 *)mbx->reply_virt;

	/* Taking a bigger struct instead of adding a TLV to list was a
	 * mistake, but one which we're now stuck with, as some older
	 * clients assume the size of the previous response.
	 */
	if (!b_legacy)
		length = sizeof(*p_tlv);
	else
		length = sizeof(struct pfvf_def_resp_tlv);

	p_tlv = ecore_add_tlv(p_hwfn, &mbx->offset, CHANNEL_TLV_START_RXQ,
			      length);
	ecore_add_tlv(p_hwfn, &mbx->offset, CHANNEL_TLV_LIST_END,
		      sizeof(struct channel_list_end_tlv));

	/* Update the TLV with the response */
	if ((status == PFVF_STATUS_SUCCESS) && !b_legacy) {
		req = &mbx->req_virt->start_rxq;
		p_tlv->offset = PXP_VF_BAR0_START_MSDM_ZONE_B +
				OFFSETOF(struct mstorm_vf_zone,
					 non_trigger.eth_rx_queue_producers) +
				sizeof(struct eth_rx_prod_data) * req->rx_qid;
	}

	ecore_iov_send_response(p_hwfn, p_ptt, vf, length, status);
}

static void ecore_iov_vf_mbx_start_rxq(struct ecore_hwfn *p_hwfn,
				       struct ecore_ptt *p_ptt,
				       struct ecore_vf_info *vf)
{
	struct ecore_queue_start_common_params p_params;
	struct ecore_iov_vf_mbx *mbx = &vf->vf_mbx;
	u8 status = PFVF_STATUS_NO_RESOURCE;
	struct vfpf_start_rxq_tlv *req;
	bool b_legacy_vf = false;
	enum _ecore_status_t rc;

	req = &mbx->req_virt->start_rxq;
	OSAL_MEMSET(&p_params, 0, sizeof(p_params));
	p_params.queue_id = (u8)vf->vf_queues[req->rx_qid].fw_rx_qid;
	p_params.vf_qid = req->rx_qid;
	p_params.vport_id = vf->vport_id;
	p_params.stats_id = vf->abs_vf_id + 0x10,
	p_params.sb = req->hw_sb;
	p_params.sb_idx = req->sb_index;

	if (!ecore_iov_validate_rxq(p_hwfn, vf, req->rx_qid) ||
	    !ecore_iov_validate_sb(p_hwfn, vf, req->hw_sb))
		goto out;

	/* Legacy VFs have their Producers in a different location, which they
	 * calculate on their own and clean the producer prior to this.
	 */
	if (vf->acquire.vfdev_info.eth_fp_hsi_minor ==
	    ETH_HSI_VER_NO_PKT_LEN_TUNN)
		b_legacy_vf = true;
	else
		REG_WR(p_hwfn,
		       GTT_BAR0_MAP_REG_MSDM_RAM +
		       MSTORM_ETH_VF_PRODS_OFFSET(vf->abs_vf_id, req->rx_qid),
		       0);

	rc = ecore_sp_eth_rxq_start_ramrod(p_hwfn, vf->opaque_fid,
					   vf->vf_queues[req->rx_qid].fw_cid,
					   &p_params,
					   req->bd_max_bytes,
					   req->rxq_addr,
					   req->cqe_pbl_addr,
					   req->cqe_pbl_size,
					   b_legacy_vf);

	if (rc) {
		status = PFVF_STATUS_FAILURE;
	} else {
		status = PFVF_STATUS_SUCCESS;
		vf->vf_queues[req->rx_qid].rxq_active = true;
		vf->num_active_rxqs++;
	}

out:
	ecore_iov_vf_mbx_start_rxq_resp(p_hwfn, p_ptt, vf,
					status, b_legacy_vf);
}

static void ecore_iov_vf_mbx_start_txq_resp(struct ecore_hwfn *p_hwfn,
					    struct ecore_ptt *p_ptt,
					    struct ecore_vf_info *p_vf,
					    u8 status)
{
	struct ecore_iov_vf_mbx *mbx = &p_vf->vf_mbx;
	struct pfvf_start_queue_resp_tlv *p_tlv;
	bool b_legacy = false;
	u16 length;

	mbx->offset = (u8 *)mbx->reply_virt;

	/* Taking a bigger struct instead of adding a TLV to list was a
	 * mistake, but one which we're now stuck with, as some older
	 * clients assume the size of the previous response.
	 */
	if (p_vf->acquire.vfdev_info.eth_fp_hsi_minor ==
	    ETH_HSI_VER_NO_PKT_LEN_TUNN)
		b_legacy = true;

	if (!b_legacy)
		length = sizeof(*p_tlv);
	else
		length = sizeof(struct pfvf_def_resp_tlv);

	p_tlv = ecore_add_tlv(p_hwfn, &mbx->offset, CHANNEL_TLV_START_TXQ,
			      length);
	ecore_add_tlv(p_hwfn, &mbx->offset, CHANNEL_TLV_LIST_END,
		      sizeof(struct channel_list_end_tlv));

	/* Update the TLV with the response */
	if ((status == PFVF_STATUS_SUCCESS) && !b_legacy) {
		u16 qid = mbx->req_virt->start_txq.tx_qid;

		p_tlv->offset = DB_ADDR_VF(p_vf->vf_queues[qid].fw_cid,
					   DQ_DEMS_LEGACY);
	}

	ecore_iov_send_response(p_hwfn, p_ptt, p_vf, length, status);
}

static void ecore_iov_vf_mbx_start_txq(struct ecore_hwfn *p_hwfn,
				       struct ecore_ptt *p_ptt,
				       struct ecore_vf_info *vf)
{
	struct ecore_queue_start_common_params p_params;
	struct ecore_iov_vf_mbx *mbx = &vf->vf_mbx;
	u8 status = PFVF_STATUS_NO_RESOURCE;
	union ecore_qm_pq_params pq_params;
	struct vfpf_start_txq_tlv *req;
	enum _ecore_status_t rc;

	/* Prepare the parameters which would choose the right PQ */
	OSAL_MEMSET(&pq_params, 0, sizeof(pq_params));
	pq_params.eth.is_vf = 1;
	pq_params.eth.vf_id = vf->relative_vf_id;

	req = &mbx->req_virt->start_txq;
	OSAL_MEMSET(&p_params, 0, sizeof(p_params));
	p_params.queue_id = (u8)vf->vf_queues[req->tx_qid].fw_tx_qid;
	p_params.vport_id = vf->vport_id;
	p_params.stats_id = vf->abs_vf_id + 0x10,
	p_params.sb = req->hw_sb;
	p_params.sb_idx = req->sb_index;

	if (!ecore_iov_validate_txq(p_hwfn, vf, req->tx_qid) ||
	    !ecore_iov_validate_sb(p_hwfn, vf, req->hw_sb))
		goto out;

	rc = ecore_sp_eth_txq_start_ramrod(
		p_hwfn,
		vf->opaque_fid,
		vf->vf_queues[req->tx_qid].fw_cid,
		&p_params,
		req->pbl_addr,
		req->pbl_size,
		&pq_params);

	if (rc)
		status = PFVF_STATUS_FAILURE;
	else {
		status = PFVF_STATUS_SUCCESS;
		vf->vf_queues[req->tx_qid].txq_active = true;
	}

out:
	ecore_iov_vf_mbx_start_txq_resp(p_hwfn, p_ptt, vf, status);
}

static enum _ecore_status_t ecore_iov_vf_stop_rxqs(struct ecore_hwfn *p_hwfn,
						   struct ecore_vf_info *vf,
						   u16 rxq_id,
						   u8 num_rxqs,
						   bool cqe_completion)
{
	enum _ecore_status_t rc = ECORE_SUCCESS;
	int qid;

	if (rxq_id + num_rxqs > OSAL_ARRAY_SIZE(vf->vf_queues))
		return ECORE_INVAL;

	for (qid = rxq_id; qid < rxq_id + num_rxqs; qid++) {
		if (vf->vf_queues[qid].rxq_active) {
			rc = ecore_sp_eth_rx_queue_stop(p_hwfn,
							vf->vf_queues[qid].
							fw_rx_qid, false,
							cqe_completion);

			if (rc)
				return rc;
		}
		vf->vf_queues[qid].rxq_active = false;
		vf->num_active_rxqs--;
	}

	return rc;
}

static enum _ecore_status_t ecore_iov_vf_stop_txqs(struct ecore_hwfn *p_hwfn,
						   struct ecore_vf_info *vf,
						   u16 txq_id, u8 num_txqs)
{
	enum _ecore_status_t rc = ECORE_SUCCESS;
	int qid;

	if (txq_id + num_txqs > OSAL_ARRAY_SIZE(vf->vf_queues))
		return ECORE_INVAL;

	for (qid = txq_id; qid < txq_id + num_txqs; qid++) {
		if (vf->vf_queues[qid].txq_active) {
			rc = ecore_sp_eth_tx_queue_stop(p_hwfn,
							vf->vf_queues[qid].
							fw_tx_qid);

			if (rc)
				return rc;
		}
		vf->vf_queues[qid].txq_active = false;
	}
	return rc;
}

static void ecore_iov_vf_mbx_stop_rxqs(struct ecore_hwfn *p_hwfn,
				       struct ecore_ptt *p_ptt,
				       struct ecore_vf_info *vf)
{
	u16 length = sizeof(struct pfvf_def_resp_tlv);
	struct ecore_iov_vf_mbx *mbx = &vf->vf_mbx;
	u8 status = PFVF_STATUS_SUCCESS;
	struct vfpf_stop_rxqs_tlv *req;
	enum _ecore_status_t rc;

	/* We give the option of starting from qid != 0, in this case we
	 * need to make sure that qid + num_qs doesn't exceed the actual
	 * amount of queues that exist.
	 */
	req = &mbx->req_virt->stop_rxqs;
	rc = ecore_iov_vf_stop_rxqs(p_hwfn, vf, req->rx_qid,
				    req->num_rxqs, req->cqe_completion);
	if (rc)
		status = PFVF_STATUS_FAILURE;

	ecore_iov_prepare_resp(p_hwfn, p_ptt, vf, CHANNEL_TLV_STOP_RXQS,
			       length, status);
}

static void ecore_iov_vf_mbx_stop_txqs(struct ecore_hwfn *p_hwfn,
				       struct ecore_ptt *p_ptt,
				       struct ecore_vf_info *vf)
{
	u16 length = sizeof(struct pfvf_def_resp_tlv);
	struct ecore_iov_vf_mbx *mbx = &vf->vf_mbx;
	u8 status = PFVF_STATUS_SUCCESS;
	struct vfpf_stop_txqs_tlv *req;
	enum _ecore_status_t rc;

	/* We give the option of starting from qid != 0, in this case we
	 * need to make sure that qid + num_qs doesn't exceed the actual
	 * amount of queues that exist.
	 */
	req = &mbx->req_virt->stop_txqs;
	rc = ecore_iov_vf_stop_txqs(p_hwfn, vf, req->tx_qid, req->num_txqs);
	if (rc)
		status = PFVF_STATUS_FAILURE;

	ecore_iov_prepare_resp(p_hwfn, p_ptt, vf, CHANNEL_TLV_STOP_TXQS,
			       length, status);
}

static void ecore_iov_vf_mbx_update_rxqs(struct ecore_hwfn *p_hwfn,
					 struct ecore_ptt *p_ptt,
					 struct ecore_vf_info *vf)
{
	u16 length = sizeof(struct pfvf_def_resp_tlv);
	struct ecore_iov_vf_mbx *mbx = &vf->vf_mbx;
	struct vfpf_update_rxq_tlv *req;
	u8 status = PFVF_STATUS_SUCCESS;
	u8 complete_event_flg;
	u8 complete_cqe_flg;
	u16 qid;
	enum _ecore_status_t rc;
	u8 i;

	req = &mbx->req_virt->update_rxq;
	complete_cqe_flg = !!(req->flags & VFPF_RXQ_UPD_COMPLETE_CQE_FLAG);
	complete_event_flg = !!(req->flags & VFPF_RXQ_UPD_COMPLETE_EVENT_FLAG);

	for (i = 0; i < req->num_rxqs; i++) {
		qid = req->rx_qid + i;

		if (!vf->vf_queues[qid].rxq_active) {
			DP_NOTICE(p_hwfn, true,
				  "VF rx_qid = %d isn`t active!\n", qid);
			status = PFVF_STATUS_FAILURE;
			break;
		}

		rc = ecore_sp_eth_rx_queues_update(p_hwfn,
						   vf->vf_queues[qid].fw_rx_qid,
						   1,
						   complete_cqe_flg,
						   complete_event_flg,
						   ECORE_SPQ_MODE_EBLOCK,
						   OSAL_NULL);

		if (rc) {
			status = PFVF_STATUS_FAILURE;
			break;
		}
	}

	ecore_iov_prepare_resp(p_hwfn, p_ptt, vf, CHANNEL_TLV_UPDATE_RXQ,
			       length, status);
}

void *ecore_iov_search_list_tlvs(struct ecore_hwfn *p_hwfn,
				 void *p_tlvs_list, u16 req_type)
{
	struct channel_tlv *p_tlv = (struct channel_tlv *)p_tlvs_list;
	int len = 0;

	do {
		if (!p_tlv->length) {
			DP_NOTICE(p_hwfn, true, "Zero length TLV found\n");
			return OSAL_NULL;
		}

		if (p_tlv->type == req_type) {
			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "Extended tlv type %s, length %d found\n",
				   ecore_channel_tlvs_string[p_tlv->type],
				   p_tlv->length);
			return p_tlv;
		}

		len += p_tlv->length;
		p_tlv = (struct channel_tlv *)((u8 *)p_tlv + p_tlv->length);

		if ((len + p_tlv->length) > TLV_BUFFER_SIZE) {
			DP_NOTICE(p_hwfn, true,
				  "TLVs has overrun the buffer size\n");
			return OSAL_NULL;
		}
	} while (p_tlv->type != CHANNEL_TLV_LIST_END);

	return OSAL_NULL;
}

static void
ecore_iov_vp_update_act_param(struct ecore_hwfn *p_hwfn,
			      struct ecore_sp_vport_update_params *p_data,
			      struct ecore_iov_vf_mbx *p_mbx, u16 *tlvs_mask)
{
	struct vfpf_vport_update_activate_tlv *p_act_tlv;
	u16 tlv = CHANNEL_TLV_VPORT_UPDATE_ACTIVATE;

	p_act_tlv = (struct vfpf_vport_update_activate_tlv *)
	    ecore_iov_search_list_tlvs(p_hwfn, p_mbx->req_virt, tlv);
	if (!p_act_tlv)
		return;

	p_data->update_vport_active_rx_flg = p_act_tlv->update_rx;
	p_data->vport_active_rx_flg = p_act_tlv->active_rx;
	p_data->update_vport_active_tx_flg = p_act_tlv->update_tx;
	p_data->vport_active_tx_flg = p_act_tlv->active_tx;
	*tlvs_mask |= 1 << ECORE_IOV_VP_UPDATE_ACTIVATE;
}

static void
ecore_iov_vp_update_vlan_param(struct ecore_hwfn *p_hwfn,
			       struct ecore_sp_vport_update_params *p_data,
			       struct ecore_vf_info *p_vf,
			       struct ecore_iov_vf_mbx *p_mbx, u16 *tlvs_mask)
{
	struct vfpf_vport_update_vlan_strip_tlv *p_vlan_tlv;
	u16 tlv = CHANNEL_TLV_VPORT_UPDATE_VLAN_STRIP;

	p_vlan_tlv = (struct vfpf_vport_update_vlan_strip_tlv *)
	    ecore_iov_search_list_tlvs(p_hwfn, p_mbx->req_virt, tlv);
	if (!p_vlan_tlv)
		return;

	p_vf->shadow_config.inner_vlan_removal = p_vlan_tlv->remove_vlan;

	/* Ignore the VF request if we're forcing a vlan */
	if (!(p_vf->configured_features & (1 << VLAN_ADDR_FORCED))) {
		p_data->update_inner_vlan_removal_flg = 1;
		p_data->inner_vlan_removal_flg = p_vlan_tlv->remove_vlan;
	}

	*tlvs_mask |= 1 << ECORE_IOV_VP_UPDATE_VLAN_STRIP;
}

static void
ecore_iov_vp_update_tx_switch(struct ecore_hwfn *p_hwfn,
			      struct ecore_sp_vport_update_params *p_data,
			      struct ecore_iov_vf_mbx *p_mbx, u16 *tlvs_mask)
{
	struct vfpf_vport_update_tx_switch_tlv *p_tx_switch_tlv;
	u16 tlv = CHANNEL_TLV_VPORT_UPDATE_TX_SWITCH;

	p_tx_switch_tlv = (struct vfpf_vport_update_tx_switch_tlv *)
	    ecore_iov_search_list_tlvs(p_hwfn, p_mbx->req_virt, tlv);
	if (!p_tx_switch_tlv)
		return;

#ifndef ASIC_ONLY
	if (CHIP_REV_IS_FPGA(p_hwfn->p_dev)) {
		DP_NOTICE(p_hwfn, false,
			  "FPGA: Ignore tx-switching configuration originating"
			  " from VFs\n");
		return;
	}
#endif

	p_data->update_tx_switching_flg = 1;
	p_data->tx_switching_flg = p_tx_switch_tlv->tx_switching;
	*tlvs_mask |= 1 << ECORE_IOV_VP_UPDATE_TX_SWITCH;
}

static void
ecore_iov_vp_update_mcast_bin_param(struct ecore_hwfn *p_hwfn,
				    struct ecore_sp_vport_update_params *p_data,
				    struct ecore_iov_vf_mbx *p_mbx,
				    u16 *tlvs_mask)
{
	struct vfpf_vport_update_mcast_bin_tlv *p_mcast_tlv;
	u16 tlv = CHANNEL_TLV_VPORT_UPDATE_MCAST;

	p_mcast_tlv = (struct vfpf_vport_update_mcast_bin_tlv *)
	    ecore_iov_search_list_tlvs(p_hwfn, p_mbx->req_virt, tlv);
	if (!p_mcast_tlv)
		return;

	p_data->update_approx_mcast_flg = 1;
	OSAL_MEMCPY(p_data->bins, p_mcast_tlv->bins,
		    sizeof(unsigned long) *
		    ETH_MULTICAST_MAC_BINS_IN_REGS);
	*tlvs_mask |= 1 << ECORE_IOV_VP_UPDATE_MCAST;
}

static void
ecore_iov_vp_update_accept_flag(struct ecore_hwfn *p_hwfn,
				struct ecore_sp_vport_update_params *p_data,
				struct ecore_iov_vf_mbx *p_mbx, u16 *tlvs_mask)
{
	struct ecore_filter_accept_flags *p_flags = &p_data->accept_flags;
	struct vfpf_vport_update_accept_param_tlv *p_accept_tlv;
	u16 tlv = CHANNEL_TLV_VPORT_UPDATE_ACCEPT_PARAM;

	p_accept_tlv = (struct vfpf_vport_update_accept_param_tlv *)
	    ecore_iov_search_list_tlvs(p_hwfn, p_mbx->req_virt, tlv);
	if (!p_accept_tlv)
		return;

	p_flags->update_rx_mode_config = p_accept_tlv->update_rx_mode;
	p_flags->rx_accept_filter = p_accept_tlv->rx_accept_filter;
	p_flags->update_tx_mode_config = p_accept_tlv->update_tx_mode;
	p_flags->tx_accept_filter = p_accept_tlv->tx_accept_filter;
	*tlvs_mask |= 1 << ECORE_IOV_VP_UPDATE_ACCEPT_PARAM;
}

static void
ecore_iov_vp_update_accept_any_vlan(struct ecore_hwfn *p_hwfn,
				    struct ecore_sp_vport_update_params *p_data,
				    struct ecore_iov_vf_mbx *p_mbx,
				    u16 *tlvs_mask)
{
	struct vfpf_vport_update_accept_any_vlan_tlv *p_accept_any_vlan;
	u16 tlv = CHANNEL_TLV_VPORT_UPDATE_ACCEPT_ANY_VLAN;

	p_accept_any_vlan = (struct vfpf_vport_update_accept_any_vlan_tlv *)
	    ecore_iov_search_list_tlvs(p_hwfn, p_mbx->req_virt, tlv);
	if (!p_accept_any_vlan)
		return;

	p_data->accept_any_vlan = p_accept_any_vlan->accept_any_vlan;
	p_data->update_accept_any_vlan_flg =
			p_accept_any_vlan->update_accept_any_vlan_flg;
	*tlvs_mask |= 1 << ECORE_IOV_VP_UPDATE_ACCEPT_ANY_VLAN;
}

static void
ecore_iov_vp_update_rss_param(struct ecore_hwfn *p_hwfn,
			      struct ecore_vf_info *vf,
			      struct ecore_sp_vport_update_params *p_data,
			      struct ecore_rss_params *p_rss,
			      struct ecore_iov_vf_mbx *p_mbx, u16 *tlvs_mask)
{
	struct vfpf_vport_update_rss_tlv *p_rss_tlv;
	u16 tlv = CHANNEL_TLV_VPORT_UPDATE_RSS;
	u16 i, q_idx, max_q_idx;
	u16 table_size;

	p_rss_tlv = (struct vfpf_vport_update_rss_tlv *)
	    ecore_iov_search_list_tlvs(p_hwfn, p_mbx->req_virt, tlv);
	if (!p_rss_tlv) {
		p_data->rss_params = OSAL_NULL;
		return;
	}

	OSAL_MEMSET(p_rss, 0, sizeof(struct ecore_rss_params));

	p_rss->update_rss_config =
	    !!(p_rss_tlv->update_rss_flags &
		VFPF_UPDATE_RSS_CONFIG_FLAG);
	p_rss->update_rss_capabilities =
	    !!(p_rss_tlv->update_rss_flags &
		VFPF_UPDATE_RSS_CAPS_FLAG);
	p_rss->update_rss_ind_table =
	    !!(p_rss_tlv->update_rss_flags &
		VFPF_UPDATE_RSS_IND_TABLE_FLAG);
	p_rss->update_rss_key =
	    !!(p_rss_tlv->update_rss_flags &
		VFPF_UPDATE_RSS_KEY_FLAG);

	p_rss->rss_enable = p_rss_tlv->rss_enable;
	p_rss->rss_eng_id = vf->relative_vf_id + 1;
	p_rss->rss_caps = p_rss_tlv->rss_caps;
	p_rss->rss_table_size_log = p_rss_tlv->rss_table_size_log;
	OSAL_MEMCPY(p_rss->rss_ind_table, p_rss_tlv->rss_ind_table,
		    sizeof(p_rss->rss_ind_table));
	OSAL_MEMCPY(p_rss->rss_key, p_rss_tlv->rss_key,
		    sizeof(p_rss->rss_key));

	table_size = OSAL_MIN_T(u16, OSAL_ARRAY_SIZE(p_rss->rss_ind_table),
				(1 << p_rss_tlv->rss_table_size_log));

	max_q_idx = OSAL_ARRAY_SIZE(vf->vf_queues);

	for (i = 0; i < table_size; i++) {
		u16 index = vf->vf_queues[0].fw_rx_qid;

		q_idx = p_rss->rss_ind_table[i];
		if (q_idx >= max_q_idx)
			DP_NOTICE(p_hwfn, true,
				  "rss_ind_table[%d] = %d,"
				  " rxq is out of range\n",
				  i, q_idx);
		else if (!vf->vf_queues[q_idx].rxq_active)
			DP_NOTICE(p_hwfn, true,
				  "rss_ind_table[%d] = %d, rxq is not active\n",
				  i, q_idx);
		else
			index = vf->vf_queues[q_idx].fw_rx_qid;
		p_rss->rss_ind_table[i] = index;
	}

	p_data->rss_params = p_rss;
	*tlvs_mask |= 1 << ECORE_IOV_VP_UPDATE_RSS;
}

static void
ecore_iov_vp_update_sge_tpa_param(struct ecore_hwfn *p_hwfn,
				  struct ecore_vf_info *vf,
				  struct ecore_sp_vport_update_params *p_data,
				  struct ecore_sge_tpa_params *p_sge_tpa,
				  struct ecore_iov_vf_mbx *p_mbx,
				  u16 *tlvs_mask)
{
	struct vfpf_vport_update_sge_tpa_tlv *p_sge_tpa_tlv;
	u16 tlv = CHANNEL_TLV_VPORT_UPDATE_SGE_TPA;

	p_sge_tpa_tlv = (struct vfpf_vport_update_sge_tpa_tlv *)
	    ecore_iov_search_list_tlvs(p_hwfn, p_mbx->req_virt, tlv);

	if (!p_sge_tpa_tlv) {
		p_data->sge_tpa_params = OSAL_NULL;
		return;
	}

	OSAL_MEMSET(p_sge_tpa, 0, sizeof(struct ecore_sge_tpa_params));

	p_sge_tpa->update_tpa_en_flg =
	    !!(p_sge_tpa_tlv->update_sge_tpa_flags & VFPF_UPDATE_TPA_EN_FLAG);
	p_sge_tpa->update_tpa_param_flg =
	    !!(p_sge_tpa_tlv->update_sge_tpa_flags &
		VFPF_UPDATE_TPA_PARAM_FLAG);

	p_sge_tpa->tpa_ipv4_en_flg =
	    !!(p_sge_tpa_tlv->sge_tpa_flags & VFPF_TPA_IPV4_EN_FLAG);
	p_sge_tpa->tpa_ipv6_en_flg =
	    !!(p_sge_tpa_tlv->sge_tpa_flags & VFPF_TPA_IPV6_EN_FLAG);
	p_sge_tpa->tpa_pkt_split_flg =
	    !!(p_sge_tpa_tlv->sge_tpa_flags & VFPF_TPA_PKT_SPLIT_FLAG);
	p_sge_tpa->tpa_hdr_data_split_flg =
	    !!(p_sge_tpa_tlv->sge_tpa_flags & VFPF_TPA_HDR_DATA_SPLIT_FLAG);
	p_sge_tpa->tpa_gro_consistent_flg =
	    !!(p_sge_tpa_tlv->sge_tpa_flags & VFPF_TPA_GRO_CONSIST_FLAG);

	p_sge_tpa->tpa_max_aggs_num = p_sge_tpa_tlv->tpa_max_aggs_num;
	p_sge_tpa->tpa_max_size = p_sge_tpa_tlv->tpa_max_size;
	p_sge_tpa->tpa_min_size_to_start = p_sge_tpa_tlv->tpa_min_size_to_start;
	p_sge_tpa->tpa_min_size_to_cont = p_sge_tpa_tlv->tpa_min_size_to_cont;
	p_sge_tpa->max_buffers_per_cqe = p_sge_tpa_tlv->max_buffers_per_cqe;

	p_data->sge_tpa_params = p_sge_tpa;

	*tlvs_mask |= 1 << ECORE_IOV_VP_UPDATE_SGE_TPA;
}

static void ecore_iov_vf_mbx_vport_update(struct ecore_hwfn *p_hwfn,
					  struct ecore_ptt *p_ptt,
					  struct ecore_vf_info *vf)
{
	struct ecore_sp_vport_update_params params;
	struct ecore_iov_vf_mbx *mbx = &vf->vf_mbx;
	struct ecore_sge_tpa_params sge_tpa_params;
	u16 tlvs_mask = 0, tlvs_accepted = 0;
	struct ecore_rss_params rss_params;
	u8 status = PFVF_STATUS_SUCCESS;
	u16 length;
	enum _ecore_status_t rc;

	/* Valiate PF can send such a request */
	if (!vf->vport_instance) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "No VPORT instance available for VF[%d],"
			   " failing vport update\n",
			   vf->abs_vf_id);
		status = PFVF_STATUS_FAILURE;
		goto out;
	}

	OSAL_MEMSET(&params, 0, sizeof(params));
	params.opaque_fid = vf->opaque_fid;
	params.vport_id = vf->vport_id;
	params.rss_params = OSAL_NULL;

	/* Search for extended tlvs list and update values
	 * from VF in struct ecore_sp_vport_update_params.
	 */
	ecore_iov_vp_update_act_param(p_hwfn, &params, mbx, &tlvs_mask);
	ecore_iov_vp_update_vlan_param(p_hwfn, &params, vf, mbx, &tlvs_mask);
	ecore_iov_vp_update_tx_switch(p_hwfn, &params, mbx, &tlvs_mask);
	ecore_iov_vp_update_mcast_bin_param(p_hwfn, &params, mbx, &tlvs_mask);
	ecore_iov_vp_update_accept_flag(p_hwfn, &params, mbx, &tlvs_mask);
	ecore_iov_vp_update_rss_param(p_hwfn, vf, &params, &rss_params,
				      mbx, &tlvs_mask);
	ecore_iov_vp_update_accept_any_vlan(p_hwfn, &params, mbx, &tlvs_mask);
	ecore_iov_vp_update_sge_tpa_param(p_hwfn, vf, &params,
					  &sge_tpa_params, mbx, &tlvs_mask);

	/* Just log a message if there is no single extended tlv in buffer.
	 * When all features of vport update ramrod would be requested by VF
	 * as extended TLVs in buffer then an error can be returned in response
	 * if there is no extended TLV present in buffer.
	 */
	tlvs_accepted = tlvs_mask;

#ifndef LINUX_REMOVE
	if (OSAL_IOV_VF_VPORT_UPDATE(p_hwfn, vf->relative_vf_id,
				     &params, &tlvs_accepted) !=
	    ECORE_SUCCESS) {
		tlvs_accepted = 0;
		status = PFVF_STATUS_NOT_SUPPORTED;
		goto out;
	}
#endif

	if (!tlvs_accepted) {
		if (tlvs_mask)
			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "Upper-layer prevents said VF"
				   " configuration\n");
		else
			DP_NOTICE(p_hwfn, true,
				  "No feature tlvs found for vport update\n");
		status = PFVF_STATUS_NOT_SUPPORTED;
		goto out;
	}

	rc = ecore_sp_vport_update(p_hwfn, &params, ECORE_SPQ_MODE_EBLOCK,
				   OSAL_NULL);

	if (rc)
		status = PFVF_STATUS_FAILURE;

out:
	length = ecore_iov_prep_vp_update_resp_tlvs(p_hwfn, vf, mbx, status,
						    tlvs_mask, tlvs_accepted);
	ecore_iov_send_response(p_hwfn, p_ptt, vf, length, status);
}

static enum _ecore_status_t
ecore_iov_vf_update_vlan_shadow(struct ecore_hwfn *p_hwfn,
				struct ecore_vf_info *p_vf,
				struct ecore_filter_ucast *p_params)
{
	int i;

	/* First remove entries and then add new ones */
	if (p_params->opcode == ECORE_FILTER_REMOVE) {
		for (i = 0; i < ECORE_ETH_VF_NUM_VLAN_FILTERS + 1; i++)
			if (p_vf->shadow_config.vlans[i].used &&
			    p_vf->shadow_config.vlans[i].vid ==
			    p_params->vlan) {
				p_vf->shadow_config.vlans[i].used = false;
				break;
			}
		if (i == ECORE_ETH_VF_NUM_VLAN_FILTERS + 1) {
			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "VF [%d] - Tries to remove a non-existing"
				   " vlan\n",
				   p_vf->relative_vf_id);
			return ECORE_INVAL;
		}
	} else if (p_params->opcode == ECORE_FILTER_REPLACE ||
		   p_params->opcode == ECORE_FILTER_FLUSH) {
		for (i = 0; i < ECORE_ETH_VF_NUM_VLAN_FILTERS + 1; i++)
			p_vf->shadow_config.vlans[i].used = false;
	}

	/* In forced mode, we're willing to remove entries - but we don't add
	 * new ones.
	 */
	if (p_vf->bulletin.p_virt->valid_bitmap & (1 << VLAN_ADDR_FORCED))
		return ECORE_SUCCESS;

	if (p_params->opcode == ECORE_FILTER_ADD ||
	    p_params->opcode == ECORE_FILTER_REPLACE) {
		for (i = 0; i < ECORE_ETH_VF_NUM_VLAN_FILTERS + 1; i++) {
			if (p_vf->shadow_config.vlans[i].used)
				continue;

			p_vf->shadow_config.vlans[i].used = true;
			p_vf->shadow_config.vlans[i].vid = p_params->vlan;
			break;
		}

		if (i == ECORE_ETH_VF_NUM_VLAN_FILTERS + 1) {
			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "VF [%d] - Tries to configure more than %d"
				   " vlan filters\n",
				   p_vf->relative_vf_id,
				   ECORE_ETH_VF_NUM_VLAN_FILTERS + 1);
			return ECORE_INVAL;
		}
	}

	return ECORE_SUCCESS;
}

static enum _ecore_status_t
ecore_iov_vf_update_mac_shadow(struct ecore_hwfn *p_hwfn,
			       struct ecore_vf_info *p_vf,
			       struct ecore_filter_ucast *p_params)
{
	char empty_mac[ETH_ALEN];
	int i;

	OSAL_MEM_ZERO(empty_mac, ETH_ALEN);

	/* If we're in forced-mode, we don't allow any change */
	/* TODO - this would change if we were ever to implement logic for
	 * removing a forced MAC altogether [in which case, like for vlans,
	 * we should be able to re-trace previous configuration.
	 */
	if (p_vf->bulletin.p_virt->valid_bitmap & (1 << MAC_ADDR_FORCED))
		return ECORE_SUCCESS;

	/* First remove entries and then add new ones */
	if (p_params->opcode == ECORE_FILTER_REMOVE) {
		for (i = 0; i < ECORE_ETH_VF_NUM_MAC_FILTERS; i++) {
			if (!OSAL_MEMCMP(p_vf->shadow_config.macs[i],
					 p_params->mac, ETH_ALEN)) {
				OSAL_MEM_ZERO(p_vf->shadow_config.macs[i],
					      ETH_ALEN);
				break;
			}
		}

		if (i == ECORE_ETH_VF_NUM_MAC_FILTERS) {
			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "MAC isn't configured\n");
			return ECORE_INVAL;
		}
	} else if (p_params->opcode == ECORE_FILTER_REPLACE ||
		   p_params->opcode == ECORE_FILTER_FLUSH) {
		for (i = 0; i < ECORE_ETH_VF_NUM_MAC_FILTERS; i++)
			OSAL_MEM_ZERO(p_vf->shadow_config.macs[i], ETH_ALEN);
	}

	/* List the new MAC address */
	if (p_params->opcode != ECORE_FILTER_ADD &&
	    p_params->opcode != ECORE_FILTER_REPLACE)
		return ECORE_SUCCESS;

	for (i = 0; i < ECORE_ETH_VF_NUM_MAC_FILTERS; i++) {
		if (!OSAL_MEMCMP(p_vf->shadow_config.macs[i],
				 empty_mac, ETH_ALEN)) {
			OSAL_MEMCPY(p_vf->shadow_config.macs[i],
				    p_params->mac, ETH_ALEN);
			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "Added MAC at %d entry in shadow\n", i);
			break;
		}
	}

	if (i == ECORE_ETH_VF_NUM_MAC_FILTERS) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "No available place for MAC\n");
		return ECORE_INVAL;
	}

	return ECORE_SUCCESS;
}

static enum _ecore_status_t
ecore_iov_vf_update_unicast_shadow(struct ecore_hwfn *p_hwfn,
				   struct ecore_vf_info *p_vf,
				   struct ecore_filter_ucast *p_params)
{
	enum _ecore_status_t rc = ECORE_SUCCESS;

	if (p_params->type == ECORE_FILTER_MAC) {
		rc = ecore_iov_vf_update_mac_shadow(p_hwfn, p_vf, p_params);
		if (rc != ECORE_SUCCESS)
			return rc;
	}

	if (p_params->type == ECORE_FILTER_VLAN)
		rc = ecore_iov_vf_update_vlan_shadow(p_hwfn, p_vf, p_params);

	return rc;
}

static void ecore_iov_vf_mbx_ucast_filter(struct ecore_hwfn *p_hwfn,
					  struct ecore_ptt *p_ptt,
					  struct ecore_vf_info *vf)
{
	struct ecore_bulletin_content *p_bulletin = vf->bulletin.p_virt;
	struct ecore_iov_vf_mbx *mbx = &vf->vf_mbx;
	struct vfpf_ucast_filter_tlv *req;
	u8 status = PFVF_STATUS_SUCCESS;
	struct ecore_filter_ucast params;
	enum _ecore_status_t rc;

	/* Prepare the unicast filter params */
	OSAL_MEMSET(&params, 0, sizeof(struct ecore_filter_ucast));
	req = &mbx->req_virt->ucast_filter;
	params.opcode = (enum ecore_filter_opcode)req->opcode;
	params.type = (enum ecore_filter_ucast_type)req->type;

	/* @@@TBD - We might need logic on HV side in determining this */
	params.is_rx_filter = 1;
	params.is_tx_filter = 1;
	params.vport_to_remove_from = vf->vport_id;
	params.vport_to_add_to = vf->vport_id;
	OSAL_MEMCPY(params.mac, req->mac, ETH_ALEN);
	params.vlan = req->vlan;

	DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
		   "VF[%d]: opcode 0x%02x type 0x%02x [%s %s] [vport 0x%02x]"
		   " MAC %02x:%02x:%02x:%02x:%02x:%02x, vlan 0x%04x\n",
		   vf->abs_vf_id, params.opcode, params.type,
		   params.is_rx_filter ? "RX" : "",
		   params.is_tx_filter ? "TX" : "",
		   params.vport_to_add_to,
		   params.mac[0], params.mac[1], params.mac[2],
		   params.mac[3], params.mac[4], params.mac[5], params.vlan);

	if (!vf->vport_instance) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "No VPORT instance available for VF[%d],"
			   " failing ucast MAC configuration\n",
			   vf->abs_vf_id);
		status = PFVF_STATUS_FAILURE;
		goto out;
	}

	/* Update shadow copy of the VF configuration */
	if (ecore_iov_vf_update_unicast_shadow(p_hwfn, vf, &params) !=
	    ECORE_SUCCESS) {
		status = PFVF_STATUS_FAILURE;
		goto out;
	}

	/* Determine if the unicast filtering is acceptible by PF */
	if ((p_bulletin->valid_bitmap & (1 << VLAN_ADDR_FORCED)) &&
	    (params.type == ECORE_FILTER_VLAN ||
	     params.type == ECORE_FILTER_MAC_VLAN)) {
		/* Once VLAN is forced or PVID is set, do not allow
		 * to add/replace any further VLANs.
		 */
		if (params.opcode == ECORE_FILTER_ADD ||
		    params.opcode == ECORE_FILTER_REPLACE)
			status = PFVF_STATUS_FORCED;
		goto out;
	}

	if ((p_bulletin->valid_bitmap & (1 << MAC_ADDR_FORCED)) &&
	    (params.type == ECORE_FILTER_MAC ||
	     params.type == ECORE_FILTER_MAC_VLAN)) {
		if (OSAL_MEMCMP(p_bulletin->mac, params.mac, ETH_ALEN) ||
		    (params.opcode != ECORE_FILTER_ADD &&
		     params.opcode != ECORE_FILTER_REPLACE))
			status = PFVF_STATUS_FORCED;
		goto out;
	}

	rc = OSAL_IOV_CHK_UCAST(p_hwfn, vf->relative_vf_id, &params);
	if (rc == ECORE_EXISTS) {
		goto out;
	} else if (rc == ECORE_INVAL) {
		status = PFVF_STATUS_FAILURE;
		goto out;
	}

	rc = ecore_sp_eth_filter_ucast(p_hwfn, vf->opaque_fid, &params,
				       ECORE_SPQ_MODE_CB, OSAL_NULL);
	if (rc)
		status = PFVF_STATUS_FAILURE;

out:
	ecore_iov_prepare_resp(p_hwfn, p_ptt, vf, CHANNEL_TLV_UCAST_FILTER,
			       sizeof(struct pfvf_def_resp_tlv), status);
}

static void ecore_iov_vf_mbx_int_cleanup(struct ecore_hwfn *p_hwfn,
					 struct ecore_ptt *p_ptt,
					 struct ecore_vf_info *vf)
{
	int i;

	/* Reset the SBs */
	for (i = 0; i < vf->num_sbs; i++)
		ecore_int_igu_init_pure_rt_single(p_hwfn, p_ptt,
						  vf->igu_sbs[i],
						  vf->opaque_fid, false);

	ecore_iov_prepare_resp(p_hwfn, p_ptt, vf, CHANNEL_TLV_INT_CLEANUP,
			       sizeof(struct pfvf_def_resp_tlv),
			       PFVF_STATUS_SUCCESS);
}

static void ecore_iov_vf_mbx_close(struct ecore_hwfn *p_hwfn,
				   struct ecore_ptt *p_ptt,
				   struct ecore_vf_info *vf)
{
	u16 length = sizeof(struct pfvf_def_resp_tlv);
	u8 status = PFVF_STATUS_SUCCESS;

	/* Disable Interrupts for VF */
	ecore_iov_vf_igu_set_int(p_hwfn, p_ptt, vf, 0);

	/* Reset Permission table */
	ecore_iov_config_perm_table(p_hwfn, p_ptt, vf, 0);

	ecore_iov_prepare_resp(p_hwfn, p_ptt, vf, CHANNEL_TLV_CLOSE,
			       length, status);
}

static void ecore_iov_vf_mbx_release(struct ecore_hwfn *p_hwfn,
				     struct ecore_ptt *p_ptt,
				     struct ecore_vf_info *p_vf)
{
	u16 length = sizeof(struct pfvf_def_resp_tlv);
	u8 status = PFVF_STATUS_SUCCESS;
	enum _ecore_status_t rc = ECORE_SUCCESS;

	ecore_iov_vf_cleanup(p_hwfn, p_vf);

	if (p_vf->state != VF_STOPPED && p_vf->state != VF_FREE) {
		/* Stopping the VF */
		rc = ecore_sp_vf_stop(p_hwfn, p_vf->concrete_fid,
				      p_vf->opaque_fid);

		if (rc != ECORE_SUCCESS) {
			DP_ERR(p_hwfn, "ecore_sp_vf_stop returned error %d\n",
			       rc);
			status = PFVF_STATUS_FAILURE;
		}

		p_vf->state = VF_STOPPED;
	}

	ecore_iov_prepare_resp(p_hwfn, p_ptt, p_vf, CHANNEL_TLV_RELEASE,
			       length, status);
}

static enum _ecore_status_t
ecore_iov_vf_flr_poll_dorq(struct ecore_hwfn *p_hwfn,
			   struct ecore_vf_info *p_vf, struct ecore_ptt *p_ptt)
{
	int cnt;
	u32 val;

	ecore_fid_pretend(p_hwfn, p_ptt, (u16)p_vf->concrete_fid);

	for (cnt = 0; cnt < 50; cnt++) {
		val = ecore_rd(p_hwfn, p_ptt, DORQ_REG_VF_USAGE_CNT);
		if (!val)
			break;
		OSAL_MSLEEP(20);
	}
	ecore_fid_pretend(p_hwfn, p_ptt, (u16)p_hwfn->hw_info.concrete_fid);

	if (cnt == 50) {
		DP_ERR(p_hwfn,
		       "VF[%d] - dorq failed to cleanup [usage 0x%08x]\n",
		       p_vf->abs_vf_id, val);
		return ECORE_TIMEOUT;
	}

	return ECORE_SUCCESS;
}

static enum _ecore_status_t
ecore_iov_vf_flr_poll_pbf(struct ecore_hwfn *p_hwfn,
			  struct ecore_vf_info *p_vf, struct ecore_ptt *p_ptt)
{
	u32 cons[MAX_NUM_VOQS], distance[MAX_NUM_VOQS];
	int i, cnt;

	/* Read initial consumers & producers */
	for (i = 0; i < MAX_NUM_VOQS; i++) {
		u32 prod;

		cons[i] = ecore_rd(p_hwfn, p_ptt,
				   PBF_REG_NUM_BLOCKS_ALLOCATED_CONS_VOQ0 +
				   i * 0x40);
		prod = ecore_rd(p_hwfn, p_ptt,
				PBF_REG_NUM_BLOCKS_ALLOCATED_PROD_VOQ0 +
				i * 0x40);
		distance[i] = prod - cons[i];
	}

	/* Wait for consumers to pass the producers */
	i = 0;
	for (cnt = 0; cnt < 50; cnt++) {
		for (; i < MAX_NUM_VOQS; i++) {
			u32 tmp;

			tmp = ecore_rd(p_hwfn, p_ptt,
				       PBF_REG_NUM_BLOCKS_ALLOCATED_CONS_VOQ0 +
				       i * 0x40);
			if (distance[i] > tmp - cons[i])
				break;
		}

		if (i == MAX_NUM_VOQS)
			break;

		OSAL_MSLEEP(20);
	}

	if (cnt == 50) {
		DP_ERR(p_hwfn, "VF[%d] - pbf polling failed on VOQ %d\n",
		       p_vf->abs_vf_id, i);
		return ECORE_TIMEOUT;
	}

	return ECORE_SUCCESS;
}

static enum _ecore_status_t ecore_iov_vf_flr_poll(struct ecore_hwfn *p_hwfn,
						  struct ecore_vf_info *p_vf,
						  struct ecore_ptt *p_ptt)
{
	enum _ecore_status_t rc;

	/* TODO - add SRC and TM polling once we add storage IOV */

	rc = ecore_iov_vf_flr_poll_dorq(p_hwfn, p_vf, p_ptt);
	if (rc)
		return rc;

	rc = ecore_iov_vf_flr_poll_pbf(p_hwfn, p_vf, p_ptt);
	if (rc)
		return rc;

	return ECORE_SUCCESS;
}

static enum _ecore_status_t
ecore_iov_execute_vf_flr_cleanup(struct ecore_hwfn *p_hwfn,
				 struct ecore_ptt *p_ptt,
				 u16 rel_vf_id, u32 *ack_vfs)
{
	struct ecore_vf_info *p_vf;
	enum _ecore_status_t rc = ECORE_SUCCESS;

	p_vf = ecore_iov_get_vf_info(p_hwfn, rel_vf_id, false);
	if (!p_vf)
		return ECORE_SUCCESS;

	if (p_hwfn->pf_iov_info->pending_flr[rel_vf_id / 64] &
	    (1ULL << (rel_vf_id % 64))) {
		u16 vfid = p_vf->abs_vf_id;

		/* TODO - should we lock channel? */

		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "VF[%d] - Handling FLR\n", vfid);

		ecore_iov_vf_cleanup(p_hwfn, p_vf);

		/* If VF isn't active, no need for anything but SW */
		if (!p_vf->b_init)
			goto cleanup;

		/* TODO - what to do in case of failure? */
		rc = ecore_iov_vf_flr_poll(p_hwfn, p_vf, p_ptt);
		if (rc != ECORE_SUCCESS)
			goto cleanup;

		rc = ecore_final_cleanup(p_hwfn, p_ptt, vfid, true);
		if (rc) {
			/* TODO - what's now? What a mess.... */
			DP_ERR(p_hwfn, "Failed handle FLR of VF[%d]\n", vfid);
			return rc;
		}

		/* VF_STOPPED has to be set only after final cleanup
		 * but prior to re-enabling the VF.
		 */
		p_vf->state = VF_STOPPED;

		rc = ecore_iov_enable_vf_access(p_hwfn, p_ptt, p_vf);
		if (rc) {
			/* TODO - again, a mess... */
			DP_ERR(p_hwfn, "Failed to re-enable VF[%d] acces\n",
			       vfid);
			return rc;
		}
cleanup:
		/* Mark VF for ack and clean pending state */
		if (p_vf->state == VF_RESET)
			p_vf->state = VF_STOPPED;
		ack_vfs[vfid / 32] |= (1 << (vfid % 32));
		p_hwfn->pf_iov_info->pending_flr[rel_vf_id / 64] &=
		    ~(1ULL << (rel_vf_id % 64));
		p_hwfn->pf_iov_info->pending_events[rel_vf_id / 64] &=
		    ~(1ULL << (rel_vf_id % 64));
	}

	return rc;
}

enum _ecore_status_t ecore_iov_vf_flr_cleanup(struct ecore_hwfn *p_hwfn,
					      struct ecore_ptt *p_ptt)
{
	u32 ack_vfs[VF_MAX_STATIC / 32];
	enum _ecore_status_t rc = ECORE_SUCCESS;
	u16 i;

	OSAL_MEMSET(ack_vfs, 0, sizeof(u32) * (VF_MAX_STATIC / 32));

	/* Since BRB <-> PRS interface can't be tested as part of the flr
	 * polling due to HW limitations, simply sleep a bit. And since
	 * there's no need to wait per-vf, do it before looping.
	 */
	OSAL_MSLEEP(100);

	for (i = 0; i < p_hwfn->p_dev->p_iov_info->total_vfs; i++)
		ecore_iov_execute_vf_flr_cleanup(p_hwfn, p_ptt, i, ack_vfs);

	rc = ecore_mcp_ack_vf_flr(p_hwfn, p_ptt, ack_vfs);
	return rc;
}

enum _ecore_status_t
ecore_iov_single_vf_flr_cleanup(struct ecore_hwfn *p_hwfn,
				struct ecore_ptt *p_ptt, u16 rel_vf_id)
{
	u32 ack_vfs[VF_MAX_STATIC / 32];
	enum _ecore_status_t rc = ECORE_SUCCESS;

	OSAL_MEMSET(ack_vfs, 0, sizeof(u32) * (VF_MAX_STATIC / 32));

	/* Wait instead of polling the BRB <-> PRS interface */
	OSAL_MSLEEP(100);

	ecore_iov_execute_vf_flr_cleanup(p_hwfn, p_ptt, rel_vf_id, ack_vfs);

	rc = ecore_mcp_ack_vf_flr(p_hwfn, p_ptt, ack_vfs);
	return rc;
}

int ecore_iov_mark_vf_flr(struct ecore_hwfn *p_hwfn, u32 *p_disabled_vfs)
{
	u16 i, found = 0;

	DP_VERBOSE(p_hwfn, ECORE_MSG_IOV, "Marking FLR-ed VFs\n");
	for (i = 0; i < (VF_MAX_STATIC / 32); i++)
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "[%08x,...,%08x]: %08x\n",
			   i * 32, (i + 1) * 32 - 1, p_disabled_vfs[i]);

	if (!p_hwfn->p_dev->p_iov_info) {
		DP_NOTICE(p_hwfn, true, "VF flr but no IOV\n");
		return 0;
	}

	/* Mark VFs */
	for (i = 0; i < p_hwfn->p_dev->p_iov_info->total_vfs; i++) {
		struct ecore_vf_info *p_vf;
		u8 vfid;

		p_vf = ecore_iov_get_vf_info(p_hwfn, i, false);
		if (!p_vf)
			continue;

		vfid = p_vf->abs_vf_id;
		if ((1 << (vfid % 32)) & p_disabled_vfs[vfid / 32]) {
			u64 *p_flr = p_hwfn->pf_iov_info->pending_flr;
			u16 rel_vf_id = p_vf->relative_vf_id;

			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "VF[%d] [rel %d] got FLR-ed\n",
				   vfid, rel_vf_id);

			p_vf->state = VF_RESET;

			/* No need to lock here, since pending_flr should
			 * only change here and before ACKing MFw. Since
			 * MFW will not trigger an additional attention for
			 * VF flr until ACKs, we're safe.
			 */
			p_flr[rel_vf_id / 64] |= 1ULL << (rel_vf_id % 64);
			found = 1;
		}
	}

	return found;
}

void ecore_iov_get_link(struct ecore_hwfn *p_hwfn,
			u16 vfid,
			struct ecore_mcp_link_params *p_params,
			struct ecore_mcp_link_state *p_link,
			struct ecore_mcp_link_capabilities *p_caps)
{
	struct ecore_vf_info *p_vf = ecore_iov_get_vf_info(p_hwfn, vfid, false);
	struct ecore_bulletin_content *p_bulletin;

	if (!p_vf)
		return;

	p_bulletin = p_vf->bulletin.p_virt;

	if (p_params)
		__ecore_vf_get_link_params(p_hwfn, p_params, p_bulletin);
	if (p_link)
		__ecore_vf_get_link_state(p_hwfn, p_link, p_bulletin);
	if (p_caps)
		__ecore_vf_get_link_caps(p_hwfn, p_caps, p_bulletin);
}

void ecore_iov_process_mbx_req(struct ecore_hwfn *p_hwfn,
			       struct ecore_ptt *p_ptt, int vfid)
{
	struct ecore_iov_vf_mbx *mbx;
	struct ecore_vf_info *p_vf;

	p_vf = ecore_iov_get_vf_info(p_hwfn, (u16)vfid, true);
	if (!p_vf)
		return;

	mbx = &p_vf->vf_mbx;

	/* ecore_iov_process_mbx_request */
	DP_VERBOSE(p_hwfn,
		   ECORE_MSG_IOV,
		   "VF[%02x]: Processing mailbox message\n", p_vf->abs_vf_id);

	mbx->first_tlv = mbx->req_virt->first_tlv;

	OSAL_IOV_VF_MSG_TYPE(p_hwfn,
			     p_vf->relative_vf_id,
			     mbx->first_tlv.tl.type);

	/* Lock the per vf op mutex and note the locker's identity.
	 * The unlock will take place in mbx response.
	 */
	ecore_iov_lock_vf_pf_channel(p_hwfn,
				     p_vf, mbx->first_tlv.tl.type);

	/* check if tlv type is known */
	if (ecore_iov_tlv_supported(mbx->first_tlv.tl.type)) {
		/* switch on the opcode */
		switch (mbx->first_tlv.tl.type) {
		case CHANNEL_TLV_ACQUIRE:
			ecore_iov_vf_mbx_acquire(p_hwfn, p_ptt, p_vf);
			break;
		case CHANNEL_TLV_VPORT_START:
			ecore_iov_vf_mbx_start_vport(p_hwfn, p_ptt, p_vf);
			break;
		case CHANNEL_TLV_VPORT_TEARDOWN:
			ecore_iov_vf_mbx_stop_vport(p_hwfn, p_ptt, p_vf);
			break;
		case CHANNEL_TLV_START_RXQ:
			ecore_iov_vf_mbx_start_rxq(p_hwfn, p_ptt, p_vf);
			break;
		case CHANNEL_TLV_START_TXQ:
			ecore_iov_vf_mbx_start_txq(p_hwfn, p_ptt, p_vf);
			break;
		case CHANNEL_TLV_STOP_RXQS:
			ecore_iov_vf_mbx_stop_rxqs(p_hwfn, p_ptt, p_vf);
			break;
		case CHANNEL_TLV_STOP_TXQS:
			ecore_iov_vf_mbx_stop_txqs(p_hwfn, p_ptt, p_vf);
			break;
		case CHANNEL_TLV_UPDATE_RXQ:
			ecore_iov_vf_mbx_update_rxqs(p_hwfn, p_ptt, p_vf);
			break;
		case CHANNEL_TLV_VPORT_UPDATE:
			ecore_iov_vf_mbx_vport_update(p_hwfn, p_ptt, p_vf);
			break;
		case CHANNEL_TLV_UCAST_FILTER:
			ecore_iov_vf_mbx_ucast_filter(p_hwfn, p_ptt, p_vf);
			break;
		case CHANNEL_TLV_CLOSE:
			ecore_iov_vf_mbx_close(p_hwfn, p_ptt, p_vf);
			break;
		case CHANNEL_TLV_INT_CLEANUP:
			ecore_iov_vf_mbx_int_cleanup(p_hwfn, p_ptt, p_vf);
			break;
		case CHANNEL_TLV_RELEASE:
			ecore_iov_vf_mbx_release(p_hwfn, p_ptt, p_vf);
			break;
		}
	} else {
		/* unknown TLV - this may belong to a VF driver from the future
		 * - a version written after this PF driver was written, which
		 * supports features unknown as of yet. Too bad since we don't
		 * support them. Or this may be because someone wrote a crappy
		 * VF driver and is sending garbage over the channel.
		 */
		DP_NOTICE(p_hwfn, false,
			  "VF[%02x]: unknown TLV. type %04x length %04x"
			  " padding %08x reply address %lu\n",
			  p_vf->abs_vf_id,
			  mbx->first_tlv.tl.type,
			  mbx->first_tlv.tl.length,
			  mbx->first_tlv.padding,
			  (unsigned long)mbx->first_tlv.reply_address);

		/* Try replying in case reply address matches the acquisition's
		 * posted address.
		 */
		if (p_vf->acquire.first_tlv.reply_address &&
		    (mbx->first_tlv.reply_address ==
		     p_vf->acquire.first_tlv.reply_address))
			ecore_iov_prepare_resp(p_hwfn, p_ptt, p_vf,
					       mbx->first_tlv.tl.type,
					       sizeof(struct pfvf_def_resp_tlv),
					       PFVF_STATUS_NOT_SUPPORTED);
		else
			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "VF[%02x]: Can't respond to TLV -"
				   " no valid reply address\n",
				   p_vf->abs_vf_id);
	}

	ecore_iov_unlock_vf_pf_channel(p_hwfn, p_vf,
				       mbx->first_tlv.tl.type);

#ifdef CONFIG_ECORE_SW_CHANNEL
	mbx->sw_mbx.mbx_state = VF_PF_RESPONSE_READY;
	mbx->sw_mbx.response_offset = 0;
#endif
}

void ecore_iov_pf_add_pending_events(struct ecore_hwfn *p_hwfn, u8 vfid)
{
	u64 add_bit = 1ULL << (vfid % 64);

	/* TODO - add locking mechanisms [no atomics in ecore, so we can't
	* add the lock inside the ecore_pf_iov struct].
	*/
	p_hwfn->pf_iov_info->pending_events[vfid / 64] |= add_bit;
}

void ecore_iov_pf_get_and_clear_pending_events(struct ecore_hwfn *p_hwfn,
					       u64 *events)
{
	u64 *p_pending_events = p_hwfn->pf_iov_info->pending_events;

	/* TODO - Take a lock */
	OSAL_MEMCPY(events, p_pending_events,
		    sizeof(u64) * ECORE_VF_ARRAY_LENGTH);
	OSAL_MEMSET(p_pending_events, 0,
		    sizeof(u64) * ECORE_VF_ARRAY_LENGTH);
}

static enum _ecore_status_t ecore_sriov_vfpf_msg(struct ecore_hwfn *p_hwfn,
						 u16 abs_vfid,
						 struct regpair *vf_msg)
{
	u8 min = (u8)p_hwfn->p_dev->p_iov_info->first_vf_in_pf;
	struct ecore_vf_info *p_vf;

	if (!ecore_iov_pf_sanity_check(p_hwfn, (int)abs_vfid - min)) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "Got a message from VF [abs 0x%08x] that cannot be"
			   " handled by PF\n",
			   abs_vfid);
		return ECORE_SUCCESS;
	}
	p_vf = &p_hwfn->pf_iov_info->vfs_array[(u8)abs_vfid - min];

	/* List the physical address of the request so that handler
	 * could later on copy the message from it.
	 */
	p_vf->vf_mbx.pending_req = (((u64)vf_msg->hi) << 32) | vf_msg->lo;

	return OSAL_PF_VF_MSG(p_hwfn, p_vf->relative_vf_id);
}

enum _ecore_status_t ecore_sriov_eqe_event(struct ecore_hwfn *p_hwfn,
					   u8 opcode,
					   __le16 echo,
					   union event_ring_data *data)
{
	switch (opcode) {
	case COMMON_EVENT_VF_PF_CHANNEL:
		return ecore_sriov_vfpf_msg(p_hwfn, OSAL_LE16_TO_CPU(echo),
					    &data->vf_pf_channel.msg_addr);
	case COMMON_EVENT_VF_FLR:
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "VF-FLR is still not supported\n");
		return ECORE_SUCCESS;
	default:
		DP_INFO(p_hwfn->p_dev, "Unknown sriov eqe event 0x%02x\n",
			opcode);
		return ECORE_INVAL;
	}
}

bool ecore_iov_is_vf_pending_flr(struct ecore_hwfn *p_hwfn, u16 rel_vf_id)
{
	return !!(p_hwfn->pf_iov_info->pending_flr[rel_vf_id / 64] &
		   (1ULL << (rel_vf_id % 64)));
}

u16 ecore_iov_get_next_active_vf(struct ecore_hwfn *p_hwfn, u16 rel_vf_id)
{
	struct ecore_hw_sriov_info *p_iov = p_hwfn->p_dev->p_iov_info;
	u16 i;

	if (!p_iov)
		goto out;

	for (i = rel_vf_id; i < p_iov->total_vfs; i++)
		if (ecore_iov_is_valid_vfid(p_hwfn, rel_vf_id, true))
			return i;

out:
	return MAX_NUM_VFS;
}

enum _ecore_status_t ecore_iov_copy_vf_msg(struct ecore_hwfn *p_hwfn,
					   struct ecore_ptt *ptt, int vfid)
{
	struct ecore_dmae_params params;
	struct ecore_vf_info *vf_info;

	vf_info = ecore_iov_get_vf_info(p_hwfn, (u16)vfid, true);
	if (!vf_info)
		return ECORE_INVAL;

	OSAL_MEMSET(&params, 0, sizeof(struct ecore_dmae_params));
	params.flags = ECORE_DMAE_FLAG_VF_SRC | ECORE_DMAE_FLAG_COMPLETION_DST;
	params.src_vfid = vf_info->abs_vf_id;

	if (ecore_dmae_host2host(p_hwfn, ptt,
				 vf_info->vf_mbx.pending_req,
				 vf_info->vf_mbx.req_phys,
				 sizeof(union vfpf_tlvs) / 4, &params)) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "Failed to copy message from VF 0x%02x\n", vfid);

		return ECORE_IO;
	}

	return ECORE_SUCCESS;
}

void ecore_iov_bulletin_set_forced_mac(struct ecore_hwfn *p_hwfn,
				       u8 *mac, int vfid)
{
	struct ecore_vf_info *vf_info;
	u64 feature;

	vf_info = ecore_iov_get_vf_info(p_hwfn, (u16)vfid, true);
	if (!vf_info) {
		DP_NOTICE(p_hwfn->p_dev, true,
			  "Can not set forced MAC, invalid vfid [%d]\n", vfid);
		return;
	}

	feature = 1 << MAC_ADDR_FORCED;
	OSAL_MEMCPY(vf_info->bulletin.p_virt->mac, mac, ETH_ALEN);

	vf_info->bulletin.p_virt->valid_bitmap |= feature;
	/* Forced MAC will disable MAC_ADDR */
	vf_info->bulletin.p_virt->valid_bitmap &=
	    ~(1 << VFPF_BULLETIN_MAC_ADDR);

	ecore_iov_configure_vport_forced(p_hwfn, vf_info, feature);
}

enum _ecore_status_t ecore_iov_bulletin_set_mac(struct ecore_hwfn *p_hwfn,
						u8 *mac, int vfid)
{
	struct ecore_vf_info *vf_info;
	u64 feature;

	vf_info = ecore_iov_get_vf_info(p_hwfn, (u16)vfid, true);
	if (!vf_info) {
		DP_NOTICE(p_hwfn->p_dev, true,
			  "Can not set MAC, invalid vfid [%d]\n", vfid);
		return ECORE_INVAL;
	}

	if (vf_info->bulletin.p_virt->valid_bitmap & (1 << MAC_ADDR_FORCED)) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "Can not set MAC, Forced MAC is configured\n");
		return ECORE_INVAL;
	}

	feature = 1 << VFPF_BULLETIN_MAC_ADDR;
	OSAL_MEMCPY(vf_info->bulletin.p_virt->mac, mac, ETH_ALEN);

	vf_info->bulletin.p_virt->valid_bitmap |= feature;

	return ECORE_SUCCESS;
}

enum _ecore_status_t
ecore_iov_bulletin_set_forced_untagged_default(struct ecore_hwfn *p_hwfn,
					       bool b_untagged_only, int vfid)
{
	struct ecore_vf_info *vf_info;
	u64 feature;

	vf_info = ecore_iov_get_vf_info(p_hwfn, (u16)vfid, true);
	if (!vf_info) {
		DP_NOTICE(p_hwfn->p_dev, true,
			  "Can not set forced MAC, invalid vfid [%d]\n", vfid);
		return ECORE_INVAL;
	}

	/* Since this is configurable only during vport-start, don't take it
	 * if we're past that point.
	 */
	if (vf_info->state == VF_ENABLED) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "Can't support untagged change for vfid[%d] -"
			   " VF is already active\n",
			   vfid);
		return ECORE_INVAL;
	}

	/* Set configuration; This will later be taken into account during the
	 * VF initialization.
	 */
	feature = (1 << VFPF_BULLETIN_UNTAGGED_DEFAULT) |
	    (1 << VFPF_BULLETIN_UNTAGGED_DEFAULT_FORCED);
	vf_info->bulletin.p_virt->valid_bitmap |= feature;

	vf_info->bulletin.p_virt->default_only_untagged = b_untagged_only ? 1
	    : 0;

	return ECORE_SUCCESS;
}

void ecore_iov_get_vfs_opaque_fid(struct ecore_hwfn *p_hwfn, int vfid,
				  u16 *opaque_fid)
{
	struct ecore_vf_info *vf_info;

	vf_info = ecore_iov_get_vf_info(p_hwfn, (u16)vfid, true);
	if (!vf_info)
		return;

	*opaque_fid = vf_info->opaque_fid;
}

void ecore_iov_get_vfs_vport_id(struct ecore_hwfn *p_hwfn, int vfid,
				u8 *p_vort_id)
{
	struct ecore_vf_info *vf_info;

	vf_info = ecore_iov_get_vf_info(p_hwfn, (u16)vfid, true);
	if (!vf_info)
		return;

	*p_vort_id = vf_info->vport_id;
}

void ecore_iov_bulletin_set_forced_vlan(struct ecore_hwfn *p_hwfn,
					u16 pvid, int vfid)
{
	struct ecore_vf_info *vf_info;
	u64 feature;

	vf_info = ecore_iov_get_vf_info(p_hwfn, (u16)vfid, true);
	if (!vf_info) {
		DP_NOTICE(p_hwfn->p_dev, true,
			  "Can not set forced MAC, invalid vfid [%d]\n",
			  vfid);
		return;
	}

	feature = 1 << VLAN_ADDR_FORCED;
	vf_info->bulletin.p_virt->pvid = pvid;
	if (pvid)
		vf_info->bulletin.p_virt->valid_bitmap |= feature;
	else
		vf_info->bulletin.p_virt->valid_bitmap &= ~feature;

	ecore_iov_configure_vport_forced(p_hwfn, vf_info, feature);
}

bool ecore_iov_vf_has_vport_instance(struct ecore_hwfn *p_hwfn, int vfid)
{
	struct ecore_vf_info *p_vf_info;

	p_vf_info = ecore_iov_get_vf_info(p_hwfn, (u16)vfid, true);
	if (!p_vf_info)
		return false;

	return !!p_vf_info->vport_instance;
}

bool ecore_iov_is_vf_stopped(struct ecore_hwfn *p_hwfn, int vfid)
{
	struct ecore_vf_info *p_vf_info;

	p_vf_info = ecore_iov_get_vf_info(p_hwfn, (u16)vfid, true);
	if (!p_vf_info)
		return true;

	return p_vf_info->state == VF_STOPPED;
}

bool ecore_iov_spoofchk_get(struct ecore_hwfn *p_hwfn, int vfid)
{
	struct ecore_vf_info *vf_info;

	vf_info = ecore_iov_get_vf_info(p_hwfn, (u16)vfid, true);
	if (!vf_info)
		return false;

	return vf_info->spoof_chk;
}

enum _ecore_status_t ecore_iov_spoofchk_set(struct ecore_hwfn *p_hwfn,
					    int vfid, bool val)
{
	struct ecore_vf_info *vf;
	enum _ecore_status_t rc = ECORE_INVAL;

	if (!ecore_iov_pf_sanity_check(p_hwfn, vfid)) {
		DP_NOTICE(p_hwfn, true,
			  "SR-IOV sanity check failed, can't set spoofchk\n");
		goto out;
	}

	vf = ecore_iov_get_vf_info(p_hwfn, (u16)vfid, true);
	if (!vf)
		goto out;

	if (!ecore_iov_vf_has_vport_instance(p_hwfn, vfid)) {
		/* After VF VPORT start PF will configure spoof check */
		vf->req_spoofchk_val = val;
		rc = ECORE_SUCCESS;
		goto out;
	}

	rc = __ecore_iov_spoofchk_set(p_hwfn, vf, val);

out:
	return rc;
}

u8 ecore_iov_vf_chains_per_pf(struct ecore_hwfn *p_hwfn)
{
	u8 max_chains_per_vf = p_hwfn->hw_info.max_chains_per_vf;

	max_chains_per_vf = (max_chains_per_vf) ? max_chains_per_vf
	    : ECORE_MAX_VF_CHAINS_PER_PF;

	return max_chains_per_vf;
}

void ecore_iov_get_vf_req_virt_mbx_params(struct ecore_hwfn *p_hwfn,
					  u16 rel_vf_id,
					  void **pp_req_virt_addr,
					  u16 *p_req_virt_size)
{
	struct ecore_vf_info *vf_info =
	    ecore_iov_get_vf_info(p_hwfn, rel_vf_id, true);

	if (!vf_info)
		return;

	if (pp_req_virt_addr)
		*pp_req_virt_addr = vf_info->vf_mbx.req_virt;

	if (p_req_virt_size)
		*p_req_virt_size = sizeof(*vf_info->vf_mbx.req_virt);
}

void ecore_iov_get_vf_reply_virt_mbx_params(struct ecore_hwfn *p_hwfn,
					    u16 rel_vf_id,
					    void **pp_reply_virt_addr,
					    u16 *p_reply_virt_size)
{
	struct ecore_vf_info *vf_info =
	    ecore_iov_get_vf_info(p_hwfn, rel_vf_id, true);

	if (!vf_info)
		return;

	if (pp_reply_virt_addr)
		*pp_reply_virt_addr = vf_info->vf_mbx.reply_virt;

	if (p_reply_virt_size)
		*p_reply_virt_size = sizeof(*vf_info->vf_mbx.reply_virt);
}

#ifdef CONFIG_ECORE_SW_CHANNEL
struct ecore_iov_sw_mbx *ecore_iov_get_vf_sw_mbx(struct ecore_hwfn *p_hwfn,
						 u16 rel_vf_id)
{
	struct ecore_vf_info *vf_info =
	    ecore_iov_get_vf_info(p_hwfn, rel_vf_id, true);

	if (!vf_info)
		return OSAL_NULL;

	return &vf_info->vf_mbx.sw_mbx;
}
#endif

bool ecore_iov_is_valid_vfpf_msg_length(u32 length)
{
	return (length >= sizeof(struct vfpf_first_tlv) &&
		(length <= sizeof(union vfpf_tlvs)));
}

u32 ecore_iov_pfvf_msg_length(void)
{
	return sizeof(union pfvf_tlvs);
}

u8 *ecore_iov_bulletin_get_forced_mac(struct ecore_hwfn *p_hwfn, u16 rel_vf_id)
{
	struct ecore_vf_info *p_vf;

	p_vf = ecore_iov_get_vf_info(p_hwfn, rel_vf_id, true);
	if (!p_vf || !p_vf->bulletin.p_virt)
		return OSAL_NULL;

	if (!(p_vf->bulletin.p_virt->valid_bitmap & (1 << MAC_ADDR_FORCED)))
		return OSAL_NULL;

	return p_vf->bulletin.p_virt->mac;
}

u16 ecore_iov_bulletin_get_forced_vlan(struct ecore_hwfn *p_hwfn,
				       u16 rel_vf_id)
{
	struct ecore_vf_info *p_vf;

	p_vf = ecore_iov_get_vf_info(p_hwfn, rel_vf_id, true);
	if (!p_vf || !p_vf->bulletin.p_virt)
		return 0;

	if (!(p_vf->bulletin.p_virt->valid_bitmap & (1 << VLAN_ADDR_FORCED)))
		return 0;

	return p_vf->bulletin.p_virt->pvid;
}

enum _ecore_status_t ecore_iov_configure_tx_rate(struct ecore_hwfn *p_hwfn,
						 struct ecore_ptt *p_ptt,
						 int vfid, int val)
{
	struct ecore_vf_info *vf;
	u8 abs_vp_id = 0;
	enum _ecore_status_t rc;

	vf = ecore_iov_get_vf_info(p_hwfn, (u16)vfid, true);

	if (!vf)
		return ECORE_INVAL;

	rc = ecore_fw_vport(p_hwfn, vf->vport_id, &abs_vp_id);
	if (rc != ECORE_SUCCESS)
		return rc;

	return ecore_init_vport_rl(p_hwfn, p_ptt, abs_vp_id, (u32)val);
}

enum _ecore_status_t ecore_iov_configure_min_tx_rate(struct ecore_dev *p_dev,
						     int vfid, u32 rate)
{
	struct ecore_vf_info *vf;
	u8 vport_id;
	int i;

	for_each_hwfn(p_dev, i) {
		struct ecore_hwfn *p_hwfn = &p_dev->hwfns[i];

		if (!ecore_iov_pf_sanity_check(p_hwfn, vfid)) {
			DP_NOTICE(p_hwfn, true,
				  "SR-IOV sanity check failed,"
				  " can't set min rate\n");
			return ECORE_INVAL;
		}
	}

	vf = ecore_iov_get_vf_info(ECORE_LEADING_HWFN(p_dev), (u16)vfid, true);
	vport_id = vf->vport_id;

	return ecore_configure_vport_wfq(p_dev, vport_id, rate);
}

enum _ecore_status_t ecore_iov_get_vf_stats(struct ecore_hwfn *p_hwfn,
					    struct ecore_ptt *p_ptt,
					    int vfid,
					    struct ecore_eth_stats *p_stats)
{
	struct ecore_vf_info *vf;

	vf = ecore_iov_get_vf_info(p_hwfn, (u16)vfid, true);
	if (!vf)
		return ECORE_INVAL;

	if (vf->state != VF_ENABLED)
		return ECORE_INVAL;

	__ecore_get_vport_stats(p_hwfn, p_ptt, p_stats,
				vf->abs_vf_id + 0x10, false);

	return ECORE_SUCCESS;
}

u8 ecore_iov_get_vf_num_rxqs(struct ecore_hwfn *p_hwfn, u16 rel_vf_id)
{
	struct ecore_vf_info *p_vf;

	p_vf = ecore_iov_get_vf_info(p_hwfn, rel_vf_id, true);
	if (!p_vf)
		return 0;

	return p_vf->num_rxqs;
}

u8 ecore_iov_get_vf_num_active_rxqs(struct ecore_hwfn *p_hwfn, u16 rel_vf_id)
{
	struct ecore_vf_info *p_vf;

	p_vf = ecore_iov_get_vf_info(p_hwfn, rel_vf_id, true);
	if (!p_vf)
		return 0;

	return p_vf->num_active_rxqs;
}

void *ecore_iov_get_vf_ctx(struct ecore_hwfn *p_hwfn, u16 rel_vf_id)
{
	struct ecore_vf_info *p_vf;

	p_vf = ecore_iov_get_vf_info(p_hwfn, rel_vf_id, true);
	if (!p_vf)
		return OSAL_NULL;

	return p_vf->ctx;
}

u8 ecore_iov_get_vf_num_sbs(struct ecore_hwfn *p_hwfn, u16 rel_vf_id)
{
	struct ecore_vf_info *p_vf;

	p_vf = ecore_iov_get_vf_info(p_hwfn, rel_vf_id, true);
	if (!p_vf)
		return 0;

	return p_vf->num_sbs;
}

bool ecore_iov_is_vf_wait_for_acquire(struct ecore_hwfn *p_hwfn, u16 rel_vf_id)
{
	struct ecore_vf_info *p_vf;

	p_vf = ecore_iov_get_vf_info(p_hwfn, rel_vf_id, true);
	if (!p_vf)
		return false;

	return (p_vf->state == VF_FREE);
}

bool ecore_iov_is_vf_acquired_not_initialized(struct ecore_hwfn *p_hwfn,
					      u16 rel_vf_id)
{
	struct ecore_vf_info *p_vf;

	p_vf = ecore_iov_get_vf_info(p_hwfn, rel_vf_id, true);
	if (!p_vf)
		return false;

	return (p_vf->state == VF_ACQUIRED);
}

bool ecore_iov_is_vf_initialized(struct ecore_hwfn *p_hwfn, u16 rel_vf_id)
{
	struct ecore_vf_info *p_vf;

	p_vf = ecore_iov_get_vf_info(p_hwfn, rel_vf_id, true);
	if (!p_vf)
		return false;

	return (p_vf->state == VF_ENABLED);
}

int ecore_iov_get_vf_min_rate(struct ecore_hwfn *p_hwfn, int vfid)
{
	struct ecore_wfq_data *vf_vp_wfq;
	struct ecore_vf_info *vf_info;

	vf_info = ecore_iov_get_vf_info(p_hwfn, (u16)vfid, true);
	if (!vf_info)
		return 0;

	vf_vp_wfq = &p_hwfn->qm_info.wfq_data[vf_info->vport_id];

	if (vf_vp_wfq->configured)
		return vf_vp_wfq->min_speed;
	else
		return 0;
}
