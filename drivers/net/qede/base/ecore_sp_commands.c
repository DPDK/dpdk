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
#include "ecore_chain.h"
#include "ecore_spq.h"
#include "ecore_init_fw_funcs.h"
#include "ecore_cxt.h"
#include "ecore_sp_commands.h"
#include "ecore_gtt_reg_addr.h"
#include "ecore_iro.h"
#include "reg_addr.h"
#include "ecore_int.h"
#include "ecore_hw.h"
#include "ecore_dcbx.h"

enum _ecore_status_t ecore_sp_init_request(struct ecore_hwfn *p_hwfn,
					   struct ecore_spq_entry **pp_ent,
					   u8 cmd,
					   u8 protocol,
					   struct ecore_sp_init_data *p_data)
{
	u32 opaque_cid = p_data->opaque_fid << 16 | p_data->cid;
	struct ecore_spq_entry *p_ent = OSAL_NULL;
	enum _ecore_status_t rc = ECORE_NOTIMPL;

	/* Get an SPQ entry */
	rc = ecore_spq_get_entry(p_hwfn, pp_ent);
	if (rc != ECORE_SUCCESS)
		return rc;

	/* Fill the SPQ entry */
	p_ent = *pp_ent;
	p_ent->elem.hdr.cid = OSAL_CPU_TO_LE32(opaque_cid);
	p_ent->elem.hdr.cmd_id = cmd;
	p_ent->elem.hdr.protocol_id = protocol;
	p_ent->priority = ECORE_SPQ_PRIORITY_NORMAL;
	p_ent->comp_mode = p_data->comp_mode;
	p_ent->comp_done.done = 0;

	switch (p_ent->comp_mode) {
	case ECORE_SPQ_MODE_EBLOCK:
		p_ent->comp_cb.cookie = &p_ent->comp_done;
		break;

	case ECORE_SPQ_MODE_BLOCK:
		if (!p_data->p_comp_data)
			return ECORE_INVAL;

		p_ent->comp_cb.cookie = p_data->p_comp_data->cookie;
		break;

	case ECORE_SPQ_MODE_CB:
		if (!p_data->p_comp_data)
			p_ent->comp_cb.function = OSAL_NULL;
		else
			p_ent->comp_cb = *p_data->p_comp_data;
		break;

	default:
		DP_NOTICE(p_hwfn, true, "Unknown SPQE completion mode %d\n",
			  p_ent->comp_mode);
		return ECORE_INVAL;
	}

	DP_VERBOSE(p_hwfn, ECORE_MSG_SPQ,
		   "Initialized: CID %08x cmd %02x protocol %02x data_addr %lu comp_mode [%s]\n",
		   opaque_cid, cmd, protocol,
		   (unsigned long)&p_ent->ramrod,
		   D_TRINE(p_ent->comp_mode, ECORE_SPQ_MODE_EBLOCK,
			   ECORE_SPQ_MODE_BLOCK, "MODE_EBLOCK", "MODE_BLOCK",
			   "MODE_CB"));

	OSAL_MEMSET(&p_ent->ramrod, 0, sizeof(p_ent->ramrod));

	return ECORE_SUCCESS;
}

static enum tunnel_clss ecore_tunn_get_clss_type(u8 type)
{
	switch (type) {
	case ECORE_TUNN_CLSS_MAC_VLAN:
		return TUNNEL_CLSS_MAC_VLAN;
	case ECORE_TUNN_CLSS_MAC_VNI:
		return TUNNEL_CLSS_MAC_VNI;
	case ECORE_TUNN_CLSS_INNER_MAC_VLAN:
		return TUNNEL_CLSS_INNER_MAC_VLAN;
	case ECORE_TUNN_CLSS_INNER_MAC_VNI:
		return TUNNEL_CLSS_INNER_MAC_VNI;
	default:
		return TUNNEL_CLSS_MAC_VLAN;
	}
}

static void
ecore_tunn_set_pf_fix_tunn_mode(struct ecore_hwfn *p_hwfn,
				struct ecore_tunn_update_params *p_src,
				struct pf_update_tunnel_config *p_tunn_cfg)
{
	unsigned long cached_tunn_mode = p_hwfn->p_dev->tunn_mode;
	unsigned long update_mask = p_src->tunn_mode_update_mask;
	unsigned long tunn_mode = p_src->tunn_mode;
	unsigned long new_tunn_mode = 0;

	if (OSAL_TEST_BIT(ECORE_MODE_L2GRE_TUNN, &update_mask)) {
		if (OSAL_TEST_BIT(ECORE_MODE_L2GRE_TUNN, &tunn_mode))
			OSAL_SET_BIT(ECORE_MODE_L2GRE_TUNN, &new_tunn_mode);
	} else {
		if (OSAL_TEST_BIT(ECORE_MODE_L2GRE_TUNN, &cached_tunn_mode))
			OSAL_SET_BIT(ECORE_MODE_L2GRE_TUNN, &new_tunn_mode);
	}

	if (OSAL_TEST_BIT(ECORE_MODE_IPGRE_TUNN, &update_mask)) {
		if (OSAL_TEST_BIT(ECORE_MODE_IPGRE_TUNN, &tunn_mode))
			OSAL_SET_BIT(ECORE_MODE_IPGRE_TUNN, &new_tunn_mode);
	} else {
		if (OSAL_TEST_BIT(ECORE_MODE_IPGRE_TUNN, &cached_tunn_mode))
			OSAL_SET_BIT(ECORE_MODE_IPGRE_TUNN, &new_tunn_mode);
	}

	if (OSAL_TEST_BIT(ECORE_MODE_VXLAN_TUNN, &update_mask)) {
		if (OSAL_TEST_BIT(ECORE_MODE_VXLAN_TUNN, &tunn_mode))
			OSAL_SET_BIT(ECORE_MODE_VXLAN_TUNN, &new_tunn_mode);
	} else {
		if (OSAL_TEST_BIT(ECORE_MODE_VXLAN_TUNN, &cached_tunn_mode))
			OSAL_SET_BIT(ECORE_MODE_VXLAN_TUNN, &new_tunn_mode);
	}

	if (ECORE_IS_BB_A0(p_hwfn->p_dev)) {
		if (p_src->update_geneve_udp_port)
			DP_NOTICE(p_hwfn, true, "Geneve not supported\n");
		p_src->update_geneve_udp_port = 0;
		p_src->tunn_mode = new_tunn_mode;
		return;
	}

	if (p_src->update_geneve_udp_port) {
		p_tunn_cfg->set_geneve_udp_port_flg = 1;
		p_tunn_cfg->geneve_udp_port =
		    OSAL_CPU_TO_LE16(p_src->geneve_udp_port);
	}

	if (OSAL_TEST_BIT(ECORE_MODE_L2GENEVE_TUNN, &update_mask)) {
		if (OSAL_TEST_BIT(ECORE_MODE_L2GENEVE_TUNN, &tunn_mode))
			OSAL_SET_BIT(ECORE_MODE_L2GENEVE_TUNN, &new_tunn_mode);
	} else {
		if (OSAL_TEST_BIT(ECORE_MODE_L2GENEVE_TUNN, &cached_tunn_mode))
			OSAL_SET_BIT(ECORE_MODE_L2GENEVE_TUNN, &new_tunn_mode);
	}

	if (OSAL_TEST_BIT(ECORE_MODE_IPGENEVE_TUNN, &update_mask)) {
		if (OSAL_TEST_BIT(ECORE_MODE_IPGENEVE_TUNN, &tunn_mode))
			OSAL_SET_BIT(ECORE_MODE_IPGENEVE_TUNN, &new_tunn_mode);
	} else {
		if (OSAL_TEST_BIT(ECORE_MODE_IPGENEVE_TUNN, &cached_tunn_mode))
			OSAL_SET_BIT(ECORE_MODE_IPGENEVE_TUNN, &new_tunn_mode);
	}

	p_src->tunn_mode = new_tunn_mode;
}

static void
ecore_tunn_set_pf_update_params(struct ecore_hwfn *p_hwfn,
				struct ecore_tunn_update_params *p_src,
				struct pf_update_tunnel_config *p_tunn_cfg)
{
	unsigned long tunn_mode = p_src->tunn_mode;
	enum tunnel_clss type;

	ecore_tunn_set_pf_fix_tunn_mode(p_hwfn, p_src, p_tunn_cfg);
	p_tunn_cfg->update_rx_pf_clss = p_src->update_rx_pf_clss;
	p_tunn_cfg->update_tx_pf_clss = p_src->update_tx_pf_clss;

	type = ecore_tunn_get_clss_type(p_src->tunn_clss_vxlan);
	p_tunn_cfg->tunnel_clss_vxlan = type;
	type = ecore_tunn_get_clss_type(p_src->tunn_clss_l2gre);
	p_tunn_cfg->tunnel_clss_l2gre = type;
	type = ecore_tunn_get_clss_type(p_src->tunn_clss_ipgre);
	p_tunn_cfg->tunnel_clss_ipgre = type;

	if (p_src->update_vxlan_udp_port) {
		p_tunn_cfg->set_vxlan_udp_port_flg = 1;
		p_tunn_cfg->vxlan_udp_port =
		    OSAL_CPU_TO_LE16(p_src->vxlan_udp_port);
	}

	if (OSAL_TEST_BIT(ECORE_MODE_L2GRE_TUNN, &tunn_mode))
		p_tunn_cfg->tx_enable_l2gre = 1;

	if (OSAL_TEST_BIT(ECORE_MODE_IPGRE_TUNN, &tunn_mode))
		p_tunn_cfg->tx_enable_ipgre = 1;

	if (OSAL_TEST_BIT(ECORE_MODE_VXLAN_TUNN, &tunn_mode))
		p_tunn_cfg->tx_enable_vxlan = 1;

	if (ECORE_IS_BB_A0(p_hwfn->p_dev)) {
		if (p_src->update_geneve_udp_port)
			DP_NOTICE(p_hwfn, true, "Geneve not supported\n");
		p_src->update_geneve_udp_port = 0;
		return;
	}

	if (p_src->update_geneve_udp_port) {
		p_tunn_cfg->set_geneve_udp_port_flg = 1;
		p_tunn_cfg->geneve_udp_port =
		    OSAL_CPU_TO_LE16(p_src->geneve_udp_port);
	}

	if (OSAL_TEST_BIT(ECORE_MODE_L2GENEVE_TUNN, &tunn_mode))
		p_tunn_cfg->tx_enable_l2geneve = 1;

	if (OSAL_TEST_BIT(ECORE_MODE_IPGENEVE_TUNN, &tunn_mode))
		p_tunn_cfg->tx_enable_ipgeneve = 1;

	type = ecore_tunn_get_clss_type(p_src->tunn_clss_l2geneve);
	p_tunn_cfg->tunnel_clss_l2geneve = type;
	type = ecore_tunn_get_clss_type(p_src->tunn_clss_ipgeneve);
	p_tunn_cfg->tunnel_clss_ipgeneve = type;
}

static void ecore_set_hw_tunn_mode(struct ecore_hwfn *p_hwfn,
				   struct ecore_ptt *p_ptt,
				   unsigned long tunn_mode)
{
	u8 l2gre_enable = 0, ipgre_enable = 0, vxlan_enable = 0;
	u8 l2geneve_enable = 0, ipgeneve_enable = 0;

	if (OSAL_TEST_BIT(ECORE_MODE_L2GRE_TUNN, &tunn_mode))
		l2gre_enable = 1;

	if (OSAL_TEST_BIT(ECORE_MODE_IPGRE_TUNN, &tunn_mode))
		ipgre_enable = 1;

	if (OSAL_TEST_BIT(ECORE_MODE_VXLAN_TUNN, &tunn_mode))
		vxlan_enable = 1;

	ecore_set_gre_enable(p_hwfn, p_ptt, l2gre_enable, ipgre_enable);
	ecore_set_vxlan_enable(p_hwfn, p_ptt, vxlan_enable);

	if (ECORE_IS_BB_A0(p_hwfn->p_dev))
		return;

	if (OSAL_TEST_BIT(ECORE_MODE_L2GENEVE_TUNN, &tunn_mode))
		l2geneve_enable = 1;

	if (OSAL_TEST_BIT(ECORE_MODE_IPGENEVE_TUNN, &tunn_mode))
		ipgeneve_enable = 1;

	ecore_set_geneve_enable(p_hwfn, p_ptt, l2geneve_enable,
				ipgeneve_enable);
}

static void
ecore_tunn_set_pf_start_params(struct ecore_hwfn *p_hwfn,
			       struct ecore_tunn_start_params *p_src,
			       struct pf_start_tunnel_config *p_tunn_cfg)
{
	unsigned long tunn_mode;
	enum tunnel_clss type;

	if (!p_src)
		return;

	tunn_mode = p_src->tunn_mode;
	type = ecore_tunn_get_clss_type(p_src->tunn_clss_vxlan);
	p_tunn_cfg->tunnel_clss_vxlan = type;
	type = ecore_tunn_get_clss_type(p_src->tunn_clss_l2gre);
	p_tunn_cfg->tunnel_clss_l2gre = type;
	type = ecore_tunn_get_clss_type(p_src->tunn_clss_ipgre);
	p_tunn_cfg->tunnel_clss_ipgre = type;

	if (p_src->update_vxlan_udp_port) {
		p_tunn_cfg->set_vxlan_udp_port_flg = 1;
		p_tunn_cfg->vxlan_udp_port =
		    OSAL_CPU_TO_LE16(p_src->vxlan_udp_port);
	}

	if (OSAL_TEST_BIT(ECORE_MODE_L2GRE_TUNN, &tunn_mode))
		p_tunn_cfg->tx_enable_l2gre = 1;

	if (OSAL_TEST_BIT(ECORE_MODE_IPGRE_TUNN, &tunn_mode))
		p_tunn_cfg->tx_enable_ipgre = 1;

	if (OSAL_TEST_BIT(ECORE_MODE_VXLAN_TUNN, &tunn_mode))
		p_tunn_cfg->tx_enable_vxlan = 1;

	if (ECORE_IS_BB_A0(p_hwfn->p_dev)) {
		if (p_src->update_geneve_udp_port)
			DP_NOTICE(p_hwfn, true, "Geneve not supported\n");
		p_src->update_geneve_udp_port = 0;
		return;
	}

	if (p_src->update_geneve_udp_port) {
		p_tunn_cfg->set_geneve_udp_port_flg = 1;
		p_tunn_cfg->geneve_udp_port =
		    OSAL_CPU_TO_LE16(p_src->geneve_udp_port);
	}

	if (OSAL_TEST_BIT(ECORE_MODE_L2GENEVE_TUNN, &tunn_mode))
		p_tunn_cfg->tx_enable_l2geneve = 1;

	if (OSAL_TEST_BIT(ECORE_MODE_IPGENEVE_TUNN, &tunn_mode))
		p_tunn_cfg->tx_enable_ipgeneve = 1;

	type = ecore_tunn_get_clss_type(p_src->tunn_clss_l2geneve);
	p_tunn_cfg->tunnel_clss_l2geneve = type;
	type = ecore_tunn_get_clss_type(p_src->tunn_clss_ipgeneve);
	p_tunn_cfg->tunnel_clss_ipgeneve = type;
}

enum _ecore_status_t ecore_sp_pf_start(struct ecore_hwfn *p_hwfn,
				       struct ecore_tunn_start_params *p_tunn,
				       enum ecore_mf_mode mode,
				       bool allow_npar_tx_switch)
{
	struct pf_start_ramrod_data *p_ramrod = OSAL_NULL;
	struct ecore_spq_entry *p_ent = OSAL_NULL;
	u16 sb = ecore_int_get_sp_sb_id(p_hwfn);
	u8 sb_index = p_hwfn->p_eq->eq_sb_index;
	enum _ecore_status_t rc = ECORE_NOTIMPL;
	struct ecore_sp_init_data init_data;
	u8 page_cnt;

	/* update initial eq producer */
	ecore_eq_prod_update(p_hwfn,
			     ecore_chain_get_prod_idx(&p_hwfn->p_eq->chain));

	/* Initialize the SPQ entry for the ramrod */
	OSAL_MEMSET(&init_data, 0, sizeof(init_data));
	init_data.cid = ecore_spq_get_cid(p_hwfn);
	init_data.opaque_fid = p_hwfn->hw_info.opaque_fid;
	init_data.comp_mode = ECORE_SPQ_MODE_EBLOCK;

	rc = ecore_sp_init_request(p_hwfn, &p_ent,
				   COMMON_RAMROD_PF_START,
				   PROTOCOLID_COMMON, &init_data);
	if (rc != ECORE_SUCCESS)
		return rc;

	/* Fill the ramrod data */
	p_ramrod = &p_ent->ramrod.pf_start;
	p_ramrod->event_ring_sb_id = OSAL_CPU_TO_LE16(sb);
	p_ramrod->event_ring_sb_index = sb_index;
	p_ramrod->path_id = ECORE_PATH_ID(p_hwfn);
	p_ramrod->outer_tag = p_hwfn->hw_info.ovlan;

	/* For easier debugging */
	p_ramrod->dont_log_ramrods = 0;
	p_ramrod->log_type_mask = OSAL_CPU_TO_LE16(0xf);

	switch (mode) {
	case ECORE_MF_DEFAULT:
	case ECORE_MF_NPAR:
		p_ramrod->mf_mode = MF_NPAR;
		break;
	case ECORE_MF_OVLAN:
		p_ramrod->mf_mode = MF_OVLAN;
		break;
	default:
		DP_NOTICE(p_hwfn, true,
			  "Unsupported MF mode, init as DEFAULT\n");
		p_ramrod->mf_mode = MF_NPAR;
	}

	/* Place EQ address in RAMROD */
	DMA_REGPAIR_LE(p_ramrod->event_ring_pbl_addr,
		       p_hwfn->p_eq->chain.pbl.p_phys_table);
	page_cnt = (u8)ecore_chain_get_page_cnt(&p_hwfn->p_eq->chain);
	p_ramrod->event_ring_num_pages = page_cnt;
	DMA_REGPAIR_LE(p_ramrod->consolid_q_pbl_addr,
		       p_hwfn->p_consq->chain.pbl.p_phys_table);

	ecore_tunn_set_pf_start_params(p_hwfn, p_tunn,
				       &p_ramrod->tunnel_config);

	if (IS_MF_SI(p_hwfn))
		p_ramrod->allow_npar_tx_switching = allow_npar_tx_switch;

	switch (p_hwfn->hw_info.personality) {
	case ECORE_PCI_ETH:
		p_ramrod->personality = PERSONALITY_ETH;
		break;
	default:
		DP_NOTICE(p_hwfn, true, "Unknown personality %d\n",
			  p_hwfn->hw_info.personality);
		p_ramrod->personality = PERSONALITY_ETH;
	}

	p_ramrod->base_vf_id = (u8)p_hwfn->hw_info.first_vf_in_pf;
	p_ramrod->num_vfs = (u8)p_hwfn->p_dev->sriov_info.total_vfs;

	DP_VERBOSE(p_hwfn, ECORE_MSG_SPQ,
		   "Setting event_ring_sb [id %04x index %02x], outer_tag [%d]\n",
		   sb, sb_index, p_ramrod->outer_tag);

	rc = ecore_spq_post(p_hwfn, p_ent, OSAL_NULL);

	if (p_tunn) {
		ecore_set_hw_tunn_mode(p_hwfn, p_hwfn->p_main_ptt,
				       p_tunn->tunn_mode);
		p_hwfn->p_dev->tunn_mode = p_tunn->tunn_mode;
	}

	return rc;
}

enum _ecore_status_t ecore_sp_pf_update(struct ecore_hwfn *p_hwfn)
{
	struct ecore_spq_entry *p_ent = OSAL_NULL;
	enum _ecore_status_t rc = ECORE_NOTIMPL;
	struct ecore_sp_init_data init_data;

	/* Get SPQ entry */
	OSAL_MEMSET(&init_data, 0, sizeof(init_data));
	init_data.cid = ecore_spq_get_cid(p_hwfn);
	init_data.opaque_fid = p_hwfn->hw_info.opaque_fid;
	init_data.comp_mode = ECORE_SPQ_MODE_CB;

	rc = ecore_sp_init_request(p_hwfn, &p_ent,
				   COMMON_RAMROD_PF_UPDATE, PROTOCOLID_COMMON,
				   &init_data);
	if (rc != ECORE_SUCCESS)
		return rc;

	ecore_dcbx_set_pf_update_params(&p_hwfn->p_dcbx_info->results,
					&p_ent->ramrod.pf_update);

	return ecore_spq_post(p_hwfn, p_ent, OSAL_NULL);
}

/* Set pf update ramrod command params */
enum _ecore_status_t
ecore_sp_pf_update_tunn_cfg(struct ecore_hwfn *p_hwfn,
			    struct ecore_tunn_update_params *p_tunn,
			    enum spq_mode comp_mode,
			    struct ecore_spq_comp_cb *p_comp_data)
{
	struct ecore_spq_entry *p_ent = OSAL_NULL;
	enum _ecore_status_t rc = ECORE_NOTIMPL;
	struct ecore_sp_init_data init_data;

	/* Get SPQ entry */
	OSAL_MEMSET(&init_data, 0, sizeof(init_data));
	init_data.cid = ecore_spq_get_cid(p_hwfn);
	init_data.opaque_fid = p_hwfn->hw_info.opaque_fid;
	init_data.comp_mode = comp_mode;
	init_data.p_comp_data = p_comp_data;

	rc = ecore_sp_init_request(p_hwfn, &p_ent,
				   COMMON_RAMROD_PF_UPDATE, PROTOCOLID_COMMON,
				   &init_data);
	if (rc != ECORE_SUCCESS)
		return rc;

	ecore_tunn_set_pf_update_params(p_hwfn, p_tunn,
					&p_ent->ramrod.pf_update.tunnel_config);

	rc = ecore_spq_post(p_hwfn, p_ent, OSAL_NULL);

	if ((rc == ECORE_SUCCESS) && p_tunn) {
		if (p_tunn->update_vxlan_udp_port)
			ecore_set_vxlan_dest_port(p_hwfn, p_hwfn->p_main_ptt,
						  p_tunn->vxlan_udp_port);
		if (p_tunn->update_geneve_udp_port)
			ecore_set_geneve_dest_port(p_hwfn, p_hwfn->p_main_ptt,
						   p_tunn->geneve_udp_port);

		ecore_set_hw_tunn_mode(p_hwfn, p_hwfn->p_main_ptt,
				       p_tunn->tunn_mode);
		p_hwfn->p_dev->tunn_mode = p_tunn->tunn_mode;
	}

	return rc;
}

enum _ecore_status_t ecore_sp_pf_stop(struct ecore_hwfn *p_hwfn)
{
	enum _ecore_status_t rc = ECORE_NOTIMPL;
	struct ecore_spq_entry *p_ent = OSAL_NULL;
	struct ecore_sp_init_data init_data;

	/* Get SPQ entry */
	OSAL_MEMSET(&init_data, 0, sizeof(init_data));
	init_data.cid = ecore_spq_get_cid(p_hwfn);
	init_data.opaque_fid = p_hwfn->hw_info.opaque_fid;
	init_data.comp_mode = ECORE_SPQ_MODE_EBLOCK;

	rc = ecore_sp_init_request(p_hwfn, &p_ent,
				   COMMON_RAMROD_PF_STOP, PROTOCOLID_COMMON,
				   &init_data);
	if (rc != ECORE_SUCCESS)
		return rc;

	return ecore_spq_post(p_hwfn, p_ent, OSAL_NULL);
}

enum _ecore_status_t ecore_sp_heartbeat_ramrod(struct ecore_hwfn *p_hwfn)
{
	struct ecore_spq_entry *p_ent = OSAL_NULL;
	enum _ecore_status_t rc = ECORE_NOTIMPL;
	struct ecore_sp_init_data init_data;

	/* Get SPQ entry */
	OSAL_MEMSET(&init_data, 0, sizeof(init_data));
	init_data.cid = ecore_spq_get_cid(p_hwfn);
	init_data.opaque_fid = p_hwfn->hw_info.opaque_fid;
	init_data.comp_mode = ECORE_SPQ_MODE_EBLOCK;

	rc = ecore_sp_init_request(p_hwfn, &p_ent,
				   COMMON_RAMROD_EMPTY, PROTOCOLID_COMMON,
				   &init_data);
	if (rc != ECORE_SUCCESS)
		return rc;

	return ecore_spq_post(p_hwfn, p_ent, OSAL_NULL);
}
