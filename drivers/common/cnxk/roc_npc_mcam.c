/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include "roc_api.h"
#include "roc_priv.h"

static int
npc_mcam_alloc_counter(struct npc *npc, uint16_t *ctr)
{
	struct npc_mcam_alloc_counter_req *req;
	struct npc_mcam_alloc_counter_rsp *rsp;
	struct mbox *mbox = npc->mbox;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_npc_mcam_alloc_counter(mbox);
	if (req == NULL)
		return rc;
	req->count = 1;
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;
	*ctr = rsp->cntr_list[0];
	return rc;
}

int
npc_mcam_free_counter(struct npc *npc, uint16_t ctr_id)
{
	struct npc_mcam_oper_counter_req *req;
	struct mbox *mbox = npc->mbox;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_npc_mcam_free_counter(mbox);
	if (req == NULL)
		return rc;
	req->cntr = ctr_id;
	return mbox_process(mbox);
}

int
npc_mcam_read_counter(struct npc *npc, uint32_t ctr_id, uint64_t *count)
{
	struct npc_mcam_oper_counter_req *req;
	struct npc_mcam_oper_counter_rsp *rsp;
	struct mbox *mbox = npc->mbox;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_npc_mcam_counter_stats(mbox);
	if (req == NULL)
		return rc;
	req->cntr = ctr_id;
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;
	*count = rsp->stat;
	return rc;
}

int
npc_mcam_clear_counter(struct npc *npc, uint32_t ctr_id)
{
	struct npc_mcam_oper_counter_req *req;
	struct mbox *mbox = npc->mbox;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_npc_mcam_clear_counter(mbox);
	if (req == NULL)
		return rc;
	req->cntr = ctr_id;
	return mbox_process(mbox);
}

int
npc_mcam_free_entry(struct npc *npc, uint32_t entry)
{
	struct npc_mcam_free_entry_req *req;
	struct mbox *mbox = npc->mbox;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_npc_mcam_free_entry(mbox);
	if (req == NULL)
		return rc;
	req->entry = entry;
	return mbox_process(mbox);
}

int
npc_mcam_free_all_entries(struct npc *npc)
{
	struct npc_mcam_free_entry_req *req;
	struct mbox *mbox = npc->mbox;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_npc_mcam_free_entry(mbox);
	if (req == NULL)
		return rc;
	req->all = 1;
	return mbox_process(mbox);
}

static int
npc_supp_key_len(uint32_t supp_mask)
{
	int nib_count = 0;

	while (supp_mask) {
		nib_count++;
		supp_mask &= (supp_mask - 1);
	}
	return nib_count * 4;
}

/**
 * Returns true if any LDATA bits are extracted for specific LID+LTYPE.
 *
 * No LFLAG extraction is taken into account.
 */
static int
npc_lid_lt_in_kex(struct npc *npc, uint8_t lid, uint8_t lt)
{
	struct npc_xtract_info *x_info;
	int i;

	for (i = 0; i < NPC_MAX_LD; i++) {
		x_info = &npc->prx_dxcfg[NIX_INTF_RX][lid][lt].xtract[i];
		/* Check for LDATA */
		if (x_info->enable && x_info->len > 0)
			return true;
	}

	return false;
}

static void
npc_construct_ldata_mask(struct npc *npc, struct plt_bitmap *bmap, uint8_t lid,
			 uint8_t lt, uint8_t ld)
{
	struct npc_xtract_info *x_info, *infoflag;
	int hdr_off, keylen;
	npc_dxcfg_t *p;
	npc_fxcfg_t *q;
	int i, j;

	p = &npc->prx_dxcfg;
	x_info = &(*p)[0][lid][lt].xtract[ld];

	if (x_info->enable == 0)
		return;

	hdr_off = x_info->hdr_off * 8;
	keylen = x_info->len * 8;
	for (i = hdr_off; i < (hdr_off + keylen); i++)
		plt_bitmap_set(bmap, i);

	if (x_info->flags_enable == 0)
		return;

	if ((npc->prx_lfcfg[0].i & 0x7) != lid)
		return;

	q = &npc->prx_fxcfg;
	for (j = 0; j < NPC_MAX_LFL; j++) {
		infoflag = &(*q)[0][ld][j].xtract[0];
		if (infoflag->enable) {
			hdr_off = infoflag->hdr_off * 8;
			keylen = infoflag->len * 8;
			for (i = hdr_off; i < (hdr_off + keylen); i++)
				plt_bitmap_set(bmap, i);
		}
	}
}

/**
 * Check if given LID+LTYPE combination is present in KEX
 *
 * len is non-zero, this function will return true if KEX extracts len bytes
 * at given offset. Otherwise it'll return true if any bytes are extracted
 * specifically for given LID+LTYPE combination (meaning not LFLAG based).
 * The second case increases flexibility for custom frames whose extracted
 * bits may change depending on KEX profile loaded.
 *
 * @param npc NPC context structure
 * @param lid Layer ID to check for
 * @param lt Layer Type to check for
 * @param offset offset into the layer header to match
 * @param len length of the match
 */
static bool
npc_is_kex_enabled(struct npc *npc, uint8_t lid, uint8_t lt, int offset,
		   int len)
{
	struct plt_bitmap *bmap;
	uint32_t bmap_sz;
	uint8_t *mem;
	int i;

	if (!len)
		return npc_lid_lt_in_kex(npc, lid, lt);

	bmap_sz = plt_bitmap_get_memory_footprint(300 * 8);
	mem = plt_zmalloc(bmap_sz, 0);
	if (mem == NULL) {
		plt_err("mem alloc failed");
		return false;
	}
	bmap = plt_bitmap_init(300 * 8, mem, bmap_sz);
	if (bmap == NULL) {
		plt_err("mem alloc failed");
		plt_free(mem);
		return false;
	}

	npc_construct_ldata_mask(npc, bmap, lid, lt, 0);
	npc_construct_ldata_mask(npc, bmap, lid, lt, 1);

	for (i = offset; i < (offset + len); i++) {
		if (plt_bitmap_get(bmap, i) != 0x1) {
			plt_free(mem);
			return false;
		}
	}

	plt_free(mem);
	return true;
}

uint64_t
npc_get_kex_capability(struct npc *npc)
{
	npc_kex_cap_terms_t kex_cap;

	memset(&kex_cap, 0, sizeof(kex_cap));

	/* Ethtype: Offset 12B, len 2B */
	kex_cap.bit.ethtype_0 = npc_is_kex_enabled(
		npc, NPC_LID_LA, NPC_LT_LA_ETHER, 12 * 8, 2 * 8);
	/* QINQ VLAN Ethtype: offset 8B, len 2B */
	kex_cap.bit.ethtype_x = npc_is_kex_enabled(
		npc, NPC_LID_LB, NPC_LT_LB_STAG_QINQ, 8 * 8, 2 * 8);
	/* VLAN ID0 : Outer VLAN: Offset 2B, len 2B */
	kex_cap.bit.vlan_id_0 = npc_is_kex_enabled(
		npc, NPC_LID_LB, NPC_LT_LB_CTAG, 2 * 8, 2 * 8);
	/* VLAN ID0 : Inner VLAN: offset 6B, len 2B */
	kex_cap.bit.vlan_id_x = npc_is_kex_enabled(
		npc, NPC_LID_LB, NPC_LT_LB_STAG_QINQ, 6 * 8, 2 * 8);
	/* DMCA: offset 0B, len 6B */
	kex_cap.bit.dmac = npc_is_kex_enabled(npc, NPC_LID_LA, NPC_LT_LA_ETHER,
					      0 * 8, 6 * 8);
	/* IP proto: offset 9B, len 1B */
	kex_cap.bit.ip_proto =
		npc_is_kex_enabled(npc, NPC_LID_LC, NPC_LT_LC_IP, 9 * 8, 1 * 8);
	/* UDP dport: offset 2B, len 2B */
	kex_cap.bit.udp_dport = npc_is_kex_enabled(npc, NPC_LID_LD,
						   NPC_LT_LD_UDP, 2 * 8, 2 * 8);
	/* UDP sport: offset 0B, len 2B */
	kex_cap.bit.udp_sport = npc_is_kex_enabled(npc, NPC_LID_LD,
						   NPC_LT_LD_UDP, 0 * 8, 2 * 8);
	/* TCP dport: offset 2B, len 2B */
	kex_cap.bit.tcp_dport = npc_is_kex_enabled(npc, NPC_LID_LD,
						   NPC_LT_LD_TCP, 2 * 8, 2 * 8);
	/* TCP sport: offset 0B, len 2B */
	kex_cap.bit.tcp_sport = npc_is_kex_enabled(npc, NPC_LID_LD,
						   NPC_LT_LD_TCP, 0 * 8, 2 * 8);
	/* IP SIP: offset 12B, len 4B */
	kex_cap.bit.sip_addr = npc_is_kex_enabled(npc, NPC_LID_LC, NPC_LT_LC_IP,
						  12 * 8, 4 * 8);
	/* IP DIP: offset 14B, len 4B */
	kex_cap.bit.dip_addr = npc_is_kex_enabled(npc, NPC_LID_LC, NPC_LT_LC_IP,
						  14 * 8, 4 * 8);
	/* IP6 SIP: offset 8B, len 16B */
	kex_cap.bit.sip6_addr = npc_is_kex_enabled(
		npc, NPC_LID_LC, NPC_LT_LC_IP6, 8 * 8, 16 * 8);
	/* IP6 DIP: offset 24B, len 16B */
	kex_cap.bit.dip6_addr = npc_is_kex_enabled(
		npc, NPC_LID_LC, NPC_LT_LC_IP6, 24 * 8, 16 * 8);
	/* ESP SPI: offset 0B, len 4B */
	kex_cap.bit.ipsec_spi = npc_is_kex_enabled(npc, NPC_LID_LE,
						   NPC_LT_LE_ESP, 0 * 8, 4 * 8);
	/* VXLAN VNI: offset 4B, len 3B */
	kex_cap.bit.ld_vni = npc_is_kex_enabled(npc, NPC_LID_LE,
						NPC_LT_LE_VXLAN, 0 * 8, 3 * 8);

	/* Custom L3 frame: varied offset and lengths */
	kex_cap.bit.custom_l3 =
		npc_is_kex_enabled(npc, NPC_LID_LC, NPC_LT_LC_CUSTOM0, 0, 0);
	kex_cap.bit.custom_l3 |=
		npc_is_kex_enabled(npc, NPC_LID_LC, NPC_LT_LC_CUSTOM1, 0, 0);
	/* SCTP sport : offset 0B, len 2B */
	kex_cap.bit.sctp_sport = npc_is_kex_enabled(
		npc, NPC_LID_LD, NPC_LT_LD_SCTP, 0 * 8, 2 * 8);
	/* SCTP dport : offset 2B, len 2B */
	kex_cap.bit.sctp_dport = npc_is_kex_enabled(
		npc, NPC_LID_LD, NPC_LT_LD_SCTP, 2 * 8, 2 * 8);
	/* ICMP type : offset 0B, len 1B */
	kex_cap.bit.icmp_type = npc_is_kex_enabled(
		npc, NPC_LID_LD, NPC_LT_LD_ICMP, 0 * 8, 1 * 8);
	/* ICMP code : offset 1B, len 1B */
	kex_cap.bit.icmp_code = npc_is_kex_enabled(
		npc, NPC_LID_LD, NPC_LT_LD_ICMP, 1 * 8, 1 * 8);
	/* ICMP id : offset 4B, len 2B */
	kex_cap.bit.icmp_id = npc_is_kex_enabled(npc, NPC_LID_LD,
						 NPC_LT_LD_ICMP, 4 * 8, 2 * 8);
	/* IGMP grp_addr : offset 4B, len 4B */
	kex_cap.bit.igmp_grp_addr = npc_is_kex_enabled(
		npc, NPC_LID_LD, NPC_LT_LD_IGMP, 4 * 8, 4 * 8);
	/* GTPU teid : offset 4B, len 4B */
	kex_cap.bit.gtpu_teid = npc_is_kex_enabled(
		npc, NPC_LID_LE, NPC_LT_LE_GTPU, 4 * 8, 4 * 8);
	return kex_cap.all_bits;
}

#define BYTESM1_SHIFT 16
#define HDR_OFF_SHIFT 8
static void
npc_update_kex_info(struct npc_xtract_info *xtract_info, uint64_t val)
{
	xtract_info->len = ((val >> BYTESM1_SHIFT) & 0xf) + 1;
	xtract_info->hdr_off = (val >> HDR_OFF_SHIFT) & 0xff;
	xtract_info->key_off = val & 0x3f;
	xtract_info->enable = ((val >> 7) & 0x1);
	xtract_info->flags_enable = ((val >> 6) & 0x1);
}

int
npc_mcam_alloc_entries(struct npc *npc, int ref_mcam, int *alloc_entry,
		       int req_count, int prio, int *resp_count)
{
	struct npc_mcam_alloc_entry_req *req;
	struct npc_mcam_alloc_entry_rsp *rsp;
	struct mbox *mbox = npc->mbox;
	int rc = -ENOSPC;
	int i;

	req = mbox_alloc_msg_npc_mcam_alloc_entry(mbox);
	if (req == NULL)
		return rc;
	req->contig = 0;
	req->count = req_count;
	req->priority = prio;
	req->ref_entry = ref_mcam;

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;
	for (i = 0; i < rsp->count; i++)
		alloc_entry[i] = rsp->entry_list[i];
	*resp_count = rsp->count;
	return 0;
}

int
npc_mcam_alloc_entry(struct npc *npc, struct roc_npc_flow *mcam,
		     struct roc_npc_flow *ref_mcam, int prio, int *resp_count)
{
	struct npc_mcam_alloc_entry_req *req;
	struct npc_mcam_alloc_entry_rsp *rsp;
	struct mbox *mbox = npc->mbox;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_npc_mcam_alloc_entry(mbox);
	if (req == NULL)
		return rc;
	req->contig = 1;
	req->count = 1;
	req->priority = prio;
	req->ref_entry = ref_mcam->mcam_id;

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;
	memset(mcam, 0, sizeof(struct roc_npc_flow));
	mcam->mcam_id = rsp->entry;
	mcam->nix_intf = ref_mcam->nix_intf;
	*resp_count = rsp->count;
	return 0;
}

int
npc_mcam_ena_dis_entry(struct npc *npc, struct roc_npc_flow *mcam, bool enable)
{
	struct npc_mcam_ena_dis_entry_req *req;
	struct mbox *mbox = npc->mbox;
	int rc = -ENOSPC;

	if (enable)
		req = mbox_alloc_msg_npc_mcam_ena_entry(mbox);
	else
		req = mbox_alloc_msg_npc_mcam_dis_entry(mbox);

	if (req == NULL)
		return rc;
	req->entry = mcam->mcam_id;
	mcam->enable = enable;
	return mbox_process(mbox);
}

int
npc_mcam_write_entry(struct npc *npc, struct roc_npc_flow *mcam)
{
	struct npc_mcam_write_entry_req *req;
	struct mbox *mbox = npc->mbox;
	struct mbox_msghdr *rsp;
	int rc = -ENOSPC;
	int i;

	req = mbox_alloc_msg_npc_mcam_write_entry(mbox);
	if (req == NULL)
		return rc;
	req->entry = mcam->mcam_id;
	req->intf = mcam->nix_intf;
	req->enable_entry = mcam->enable;
	req->entry_data.action = mcam->npc_action;
	req->entry_data.vtag_action = mcam->vtag_action;
	for (i = 0; i < NPC_MCAM_KEY_X4_WORDS; i++) {
		req->entry_data.kw[i] = mcam->mcam_data[i];
		req->entry_data.kw_mask[i] = mcam->mcam_mask[i];
	}
	return mbox_process_msg(mbox, (void *)&rsp);
}

static void
npc_mcam_process_mkex_cfg(struct npc *npc, struct npc_get_kex_cfg_rsp *kex_rsp)
{
	volatile uint64_t(
		*q)[NPC_MAX_INTF][NPC_MAX_LID][NPC_MAX_LT][NPC_MAX_LD];
	struct npc_xtract_info *x_info = NULL;
	int lid, lt, ld, fl, ix;
	npc_dxcfg_t *p;
	uint64_t keyw;
	uint64_t val;

	npc->keyx_supp_nmask[NPC_MCAM_RX] =
		kex_rsp->rx_keyx_cfg & 0x7fffffffULL;
	npc->keyx_supp_nmask[NPC_MCAM_TX] =
		kex_rsp->tx_keyx_cfg & 0x7fffffffULL;
	npc->keyx_len[NPC_MCAM_RX] =
		npc_supp_key_len(npc->keyx_supp_nmask[NPC_MCAM_RX]);
	npc->keyx_len[NPC_MCAM_TX] =
		npc_supp_key_len(npc->keyx_supp_nmask[NPC_MCAM_TX]);

	keyw = (kex_rsp->rx_keyx_cfg >> 32) & 0x7ULL;
	npc->keyw[NPC_MCAM_RX] = keyw;
	keyw = (kex_rsp->tx_keyx_cfg >> 32) & 0x7ULL;
	npc->keyw[NPC_MCAM_TX] = keyw;

	/* Update KEX_LD_FLAG */
	for (ix = 0; ix < NPC_MAX_INTF; ix++) {
		for (ld = 0; ld < NPC_MAX_LD; ld++) {
			for (fl = 0; fl < NPC_MAX_LFL; fl++) {
				x_info = &npc->prx_fxcfg[ix][ld][fl].xtract[0];
				val = kex_rsp->intf_ld_flags[ix][ld][fl];
				npc_update_kex_info(x_info, val);
			}
		}
	}

	/* Update LID, LT and LDATA cfg */
	p = &npc->prx_dxcfg;
	q = (volatile uint64_t(*)[][NPC_MAX_LID][NPC_MAX_LT][NPC_MAX_LD])(
		&kex_rsp->intf_lid_lt_ld);
	for (ix = 0; ix < NPC_MAX_INTF; ix++) {
		for (lid = 0; lid < NPC_MAX_LID; lid++) {
			for (lt = 0; lt < NPC_MAX_LT; lt++) {
				for (ld = 0; ld < NPC_MAX_LD; ld++) {
					x_info = &(*p)[ix][lid][lt].xtract[ld];
					val = (*q)[ix][lid][lt][ld];
					npc_update_kex_info(x_info, val);
				}
			}
		}
	}
	/* Update LDATA Flags cfg */
	npc->prx_lfcfg[0].i = kex_rsp->kex_ld_flags[0];
	npc->prx_lfcfg[1].i = kex_rsp->kex_ld_flags[1];
}

int
npc_mcam_fetch_kex_cfg(struct npc *npc)
{
	struct npc_get_kex_cfg_rsp *kex_rsp;
	struct mbox *mbox = npc->mbox;
	int rc = 0;

	mbox_alloc_msg_npc_get_kex_cfg(mbox);
	rc = mbox_process_msg(mbox, (void *)&kex_rsp);
	if (rc) {
		plt_err("Failed to fetch NPC KEX config");
		goto done;
	}

	mbox_memcpy((char *)npc->profile_name, kex_rsp->mkex_pfl_name,
		    MKEX_NAME_LEN);

	npc_mcam_process_mkex_cfg(npc, kex_rsp);

done:
	return rc;
}

int
npc_mcam_alloc_and_write(struct npc *npc, struct roc_npc_flow *flow,
			 struct npc_parse_state *pst)
{
	int use_ctr = (flow->ctr_id == NPC_COUNTER_NONE ? 0 : 1);
	struct npc_mcam_write_entry_req *req;
	struct nix_inl_dev *inl_dev = NULL;
	struct mbox *mbox = npc->mbox;
	struct mbox_msghdr *rsp;
	struct idev_cfg *idev;
	uint16_t pf_func = 0;
	uint16_t ctr = ~(0);
	int rc, idx;
	int entry;

	PLT_SET_USED(pst);

	if (use_ctr) {
		rc = npc_mcam_alloc_counter(npc, &ctr);
		if (rc)
			return rc;
	}

	entry = npc_get_free_mcam_entry(mbox, flow, npc);
	if (entry < 0) {
		if (use_ctr)
			npc_mcam_free_counter(npc, ctr);
		return NPC_ERR_MCAM_ALLOC;
	}

	req = mbox_alloc_msg_npc_mcam_write_entry(mbox);
	if (req == NULL)
		return -ENOSPC;
	req->set_cntr = use_ctr;
	req->cntr = ctr;
	req->entry = entry;

	req->intf = (flow->nix_intf == NIX_INTF_RX) ? NPC_MCAM_RX : NPC_MCAM_TX;
	req->enable_entry = 1;
	req->entry_data.action = flow->npc_action;

	/*
	 * Driver sets vtag action on per interface basis, not
	 * per flow basis. It is a matter of how we decide to support
	 * this pmd specific behavior. There are two ways:
	 *	1. Inherit the vtag action from the one configured
	 *	   for this interface. This can be read from the
	 *	   vtag_action configured for default mcam entry of
	 *	   this pf_func.
	 *	2. Do not support vtag action with npc_flow.
	 *
	 * Second approach is used now.
	 */
	req->entry_data.vtag_action = flow->vtag_action;

	for (idx = 0; idx < ROC_NPC_MAX_MCAM_WIDTH_DWORDS; idx++) {
		req->entry_data.kw[idx] = flow->mcam_data[idx];
		req->entry_data.kw_mask[idx] = flow->mcam_mask[idx];
	}

	idev = idev_get_cfg();
	if (idev)
		inl_dev = idev->nix_inl_dev;

	if (flow->nix_intf == NIX_INTF_RX) {
		if (inl_dev && inl_dev->is_multi_channel &&
		    (flow->npc_action & NIX_RX_ACTIONOP_UCAST_IPSEC)) {
			req->entry_data.kw[0] |= (uint64_t)inl_dev->channel;
			req->entry_data.kw_mask[0] |=
				(uint64_t)inl_dev->chan_mask;
			pf_func = nix_inl_dev_pffunc_get();
			req->entry_data.action &= ~(GENMASK(19, 4));
			req->entry_data.action |= (uint64_t)pf_func << 4;

			flow->npc_action &= ~(GENMASK(19, 4));
			flow->npc_action |= (uint64_t)pf_func << 4;
			flow->mcam_data[0] |= (uint64_t)inl_dev->channel;
			flow->mcam_mask[0] |= (uint64_t)inl_dev->chan_mask;
		} else {
			req->entry_data.kw[0] |= (uint64_t)npc->channel;
			req->entry_data.kw_mask[0] |= (BIT_ULL(12) - 1);
			flow->mcam_data[0] |= (uint64_t)npc->channel;
			flow->mcam_mask[0] |= (BIT_ULL(12) - 1);
		}
	} else {
		uint16_t pf_func = (flow->npc_action >> 4) & 0xffff;

		pf_func = plt_cpu_to_be_16(pf_func);
		req->entry_data.kw[0] |= ((uint64_t)pf_func << 32);
		req->entry_data.kw_mask[0] |= ((uint64_t)0xffff << 32);

		flow->mcam_data[0] |= ((uint64_t)pf_func << 32);
		flow->mcam_mask[0] |= ((uint64_t)0xffff << 32);
	}

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc != 0)
		return rc;

	flow->mcam_id = entry;

	if (use_ctr)
		flow->ctr_id = ctr;
	return 0;
}

int
npc_program_mcam(struct npc *npc, struct npc_parse_state *pst, bool mcam_alloc)
{
	struct npc_mcam_read_base_rule_rsp *base_rule_rsp;
	/* This is non-LDATA part in search key */
	uint64_t key_data[2] = {0ULL, 0ULL};
	uint64_t key_mask[2] = {0ULL, 0ULL};
	int key_len, bit = 0, index, rc = 0;
	int intf = pst->flow->nix_intf;
	struct mcam_entry *base_entry;
	int off, idx, data_off = 0;
	uint8_t lid, mask, data;
	uint16_t layer_info;
	uint64_t lt, flags;

	/* Skip till Layer A data start */
	while (bit < NPC_PARSE_KEX_S_LA_OFFSET) {
		if (npc->keyx_supp_nmask[intf] & (1 << bit))
			data_off++;
		bit++;
	}

	/* Each bit represents 1 nibble */
	data_off *= 4;

	index = 0;
	for (lid = 0; lid < NPC_MAX_LID; lid++) {
		/* Offset in key */
		off = NPC_PARSE_KEX_S_LID_OFFSET(lid);
		lt = pst->lt[lid] & 0xf;
		flags = pst->flags[lid] & 0xff;

		/* NPC_LAYER_KEX_S */
		layer_info = ((npc->keyx_supp_nmask[intf] >> off) & 0x7);

		if (layer_info) {
			for (idx = 0; idx <= 2; idx++) {
				if (layer_info & (1 << idx)) {
					if (idx == 2)
						data = lt;
					else if (idx == 1)
						data = ((flags >> 4) & 0xf);
					else
						data = (flags & 0xf);

					if (data_off >= 64) {
						data_off = 0;
						index++;
					}
					key_data[index] |=
						((uint64_t)data << data_off);
					mask = 0xf;
					if (lt == 0)
						mask = 0;
					key_mask[index] |=
						((uint64_t)mask << data_off);
					data_off += 4;
				}
			}
		}
	}

	/* Copy this into mcam string */
	key_len = (pst->npc->keyx_len[intf] + 7) / 8;
	memcpy(pst->flow->mcam_data, key_data, key_len);
	memcpy(pst->flow->mcam_mask, key_mask, key_len);

	if (pst->is_vf && pst->flow->nix_intf == NIX_INTF_RX) {
		(void)mbox_alloc_msg_npc_read_base_steer_rule(npc->mbox);
		rc = mbox_process_msg(npc->mbox, (void *)&base_rule_rsp);
		if (rc) {
			plt_err("Failed to fetch VF's base MCAM entry");
			return rc;
		}
		base_entry = &base_rule_rsp->entry_data;
		for (idx = 0; idx < ROC_NPC_MAX_MCAM_WIDTH_DWORDS; idx++) {
			pst->flow->mcam_data[idx] |= base_entry->kw[idx];
			pst->flow->mcam_mask[idx] |= base_entry->kw_mask[idx];
		}
	}

	/*
	 * Now we have mcam data and mask formatted as
	 * [Key_len/4 nibbles][0 or 1 nibble hole][data]
	 * hole is present if key_len is odd number of nibbles.
	 * mcam data must be split into 64 bits + 48 bits segments
	 * for each back W0, W1.
	 */

	if (mcam_alloc)
		return npc_mcam_alloc_and_write(npc, pst->flow, pst);
	else
		return 0;
}

int
npc_flow_free_all_resources(struct npc *npc)
{
	struct roc_npc_flow *flow;
	int rc, idx;

	/* Free all MCAM entries allocated */
	rc = npc_mcam_free_all_entries(npc);

	/* Free any MCAM counters and delete flow list */
	for (idx = 0; idx < npc->flow_max_priority; idx++) {
		while ((flow = TAILQ_FIRST(&npc->flow_list[idx])) != NULL) {
			npc_rss_group_free(npc, flow);
			if (flow->ctr_id != NPC_COUNTER_NONE)
				rc |= npc_mcam_free_counter(npc, flow->ctr_id);

			npc_delete_prio_list_entry(npc, flow);

			TAILQ_REMOVE(&npc->flow_list[idx], flow, next);
			plt_free(flow);
		}
	}
	return rc;
}
