/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include "roc_api.h"
#include "roc_priv.h"

int
npc_mcam_alloc_counter(struct mbox *mbox, uint16_t *ctr)
{
	struct npc_mcam_alloc_counter_req *req;
	struct npc_mcam_alloc_counter_rsp *rsp;
	int rc = -ENOSPC;

	/* For CN20K, counters are enabled by default */
	if (roc_model_is_cn20k())
		return 0;

	req = mbox_alloc_msg_npc_mcam_alloc_counter(mbox_get(mbox));
	if (req == NULL)
		goto exit;
	req->count = 1;
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;
	*ctr = rsp->cntr_list[0];
exit:
	mbox_put(mbox);
	return rc;
}

int
npc_mcam_free_counter(struct mbox *mbox, uint16_t ctr_id)
{
	struct npc_mcam_oper_counter_req *req;
	int rc = -ENOSPC;

	if (roc_model_is_cn20k())
		return 0;

	req = mbox_alloc_msg_npc_mcam_free_counter(mbox_get(mbox));
	if (req == NULL)
		goto exit;
	req->cntr = ctr_id;
	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
npc_mcam_get_stats(struct mbox *mbox, struct roc_npc_flow *flow, uint64_t *count)
{
	struct npc_mcam_get_stats_req *req;
	struct npc_mcam_get_stats_rsp *rsp;
	int rc = -ENOSPC;

	/* valid only for cn20k */
	if (!roc_model_is_cn20k())
		return 0;

	req = mbox_alloc_msg_npc_mcam_entry_stats(mbox_get(mbox));
	if (req == NULL)
		goto exit;
	req->entry = flow->mcam_id;
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;
	*count = rsp->stat;
exit:
	mbox_put(mbox);
	return rc;
}

int
npc_mcam_read_counter(struct mbox *mbox, uint32_t ctr_id, uint64_t *count)
{
	struct npc_mcam_oper_counter_req *req;
	struct npc_mcam_oper_counter_rsp *rsp;
	int rc = -ENOSPC;

	if (roc_model_is_cn20k())
		return 0;

	req = mbox_alloc_msg_npc_mcam_counter_stats(mbox_get(mbox));
	if (req == NULL)
		goto exit;
	req->cntr = ctr_id;
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;
	*count = rsp->stat;
exit:
	mbox_put(mbox);
	return rc;
}

int
npc_mcam_clear_counter(struct mbox *mbox, uint32_t ctr_id)
{
	struct npc_mcam_oper_counter_req *req;
	int rc = -ENOSPC;

	if (roc_model_is_cn20k())
		return 0;

	req = mbox_alloc_msg_npc_mcam_clear_counter(mbox_get(mbox));
	if (req == NULL)
		goto exit;
	req->cntr = ctr_id;
	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
npc_mcam_free_entry(struct mbox *mbox, uint32_t entry)
{
	struct npc_mcam_free_entry_req *req;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_npc_mcam_free_entry(mbox_get(mbox));
	if (req == NULL)
		goto exit;
	req->entry = entry;
	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
npc_mcam_free_all_entries(struct npc *npc)
{
	struct npc_mcam_free_entry_req *req;
	struct mbox *mbox = mbox_get(npc->mbox);
	int rc = -ENOSPC;

	req = mbox_alloc_msg_npc_mcam_free_entry(mbox);
	if (req == NULL)
		goto exit;
	req->all = 1;
	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
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

	if (!roc_model_is_cn20k()) {
		for (i = 0; i < NPC_MAX_LD; i++) {
			x_info = &npc->prx_dxcfg[NIX_INTF_RX][lid][lt].xtract[i];
			/* Check for LDATA */
			if (x_info->enable && x_info->len > 0)
				return true;
		}
	} else {
		for (i = 0; i < NPC_MAX_EXTRACTOR; i++) {
			union npc_kex_ldata_flags_cfg *lid_info = &npc->lid_cfg[NIX_INTF_RX][i];

			if (lid_info->s.lid != lid)
				continue;
			x_info = &npc->prx_dxcfg_cn20k[NIX_INTF_RX][i][lt].xtract;
			/* Check for LDATA */
			if (x_info->enable && x_info->len > 0)
				return true;
		}
	}

	return false;
}

static void
npc_construct_ldata_mask_cn20k(struct npc *npc, struct plt_bitmap *bmap, uint8_t lid, uint8_t lt)
{
	struct npc_xtract_info *x_info;
	int hdr_off, keylen;
	int i, j;

	for (i = 0; i < NPC_MAX_EXTRACTOR; i++) {
		union npc_kex_ldata_flags_cfg *lid_conf = &npc->lid_cfg[NIX_INTF_RX][i];

		if (lid_conf->s.lid != lid)
			continue;

		x_info = &npc->prx_dxcfg_cn20k[NIX_INTF_RX][i][lt].xtract;
		if (x_info->enable == 0)
			continue;

		hdr_off = x_info->hdr_off * 8;
		keylen = x_info->len * 8;
		for (j = hdr_off; j < (hdr_off + keylen); j++)
			plt_bitmap_set(bmap, j);
	}
}

static void
npc_construct_ldata_mask(struct npc *npc, struct plt_bitmap *bmap, uint8_t lid, uint8_t lt,
			 uint8_t ld)
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
npc_is_kex_enabled(struct npc *npc, uint8_t lid, uint8_t lt, int offset, int len)
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

	if (!roc_model_is_cn20k()) {
		npc_construct_ldata_mask(npc, bmap, lid, lt, 0);
		npc_construct_ldata_mask(npc, bmap, lid, lt, 1);
	} else {
		npc_construct_ldata_mask_cn20k(npc, bmap, lid, lt);
	}

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
	kex_cap.bit.ethtype_0 = npc_is_kex_enabled(npc, NPC_LID_LA, NPC_LT_LA_ETHER, 12 * 8, 2 * 8);
	/* QINQ VLAN Ethtype: offset 8B, len 2B */
	kex_cap.bit.ethtype_x =
		npc_is_kex_enabled(npc, NPC_LID_LB, NPC_LT_LB_STAG_QINQ, 8 * 8, 2 * 8);
	/* VLAN ID0 : Outer VLAN: Offset 2B, len 2B */
	kex_cap.bit.vlan_id_0 = npc_is_kex_enabled(npc, NPC_LID_LB, NPC_LT_LB_CTAG, 2 * 8, 2 * 8);
	/* VLAN PCP0 : Outer VLAN: Offset 2B, len 1B */
	kex_cap.bit.vlan_pcp_0 = npc_is_kex_enabled(npc, NPC_LID_LB, NPC_LT_LB_CTAG, 2 * 8, 2 * 1);
	/* VLAN IDX : Inner VLAN: offset 6B, len 2B */
	kex_cap.bit.vlan_id_x =
		npc_is_kex_enabled(npc, NPC_LID_LB, NPC_LT_LB_STAG_QINQ, 6 * 8, 2 * 8);
	/* DMCA: offset 0B, len 6B */
	kex_cap.bit.dmac = npc_is_kex_enabled(npc, NPC_LID_LA, NPC_LT_LA_ETHER, 0 * 8, 6 * 8);
	/* IP proto: offset 9B, len 1B */
	kex_cap.bit.ip_proto = npc_is_kex_enabled(npc, NPC_LID_LC, NPC_LT_LC_IP, 9 * 8, 1 * 8);
	/* IPv4 dscp: offset 1B, len 1B, IPv6 dscp: offset 0B, len 2B */
	kex_cap.bit.ip_dscp = npc_is_kex_enabled(npc, NPC_LID_LC, NPC_LT_LC_IP, 1 * 8, 1 * 8) &&
			      npc_is_kex_enabled(npc, NPC_LID_LC, NPC_LT_LC_IP6, 0, 2 * 8);
	/* UDP dport: offset 2B, len 2B */
	kex_cap.bit.udp_dport = npc_is_kex_enabled(npc, NPC_LID_LD, NPC_LT_LD_UDP, 2 * 8, 2 * 8);
	/* UDP sport: offset 0B, len 2B */
	kex_cap.bit.udp_sport = npc_is_kex_enabled(npc, NPC_LID_LD, NPC_LT_LD_UDP, 0 * 8, 2 * 8);
	/* TCP dport: offset 2B, len 2B */
	kex_cap.bit.tcp_dport = npc_is_kex_enabled(npc, NPC_LID_LD, NPC_LT_LD_TCP, 2 * 8, 2 * 8);
	/* TCP sport: offset 0B, len 2B */
	kex_cap.bit.tcp_sport = npc_is_kex_enabled(npc, NPC_LID_LD, NPC_LT_LD_TCP, 0 * 8, 2 * 8);
	/* IP SIP: offset 12B, len 4B */
	kex_cap.bit.sip_addr = npc_is_kex_enabled(npc, NPC_LID_LC, NPC_LT_LC_IP, 12 * 8, 4 * 8);
	/* IP DIP: offset 14B, len 4B */
	kex_cap.bit.dip_addr = npc_is_kex_enabled(npc, NPC_LID_LC, NPC_LT_LC_IP, 14 * 8, 4 * 8);
	/* IP6 SIP: offset 8B, len 16B */
	kex_cap.bit.sip6_addr = npc_is_kex_enabled(npc, NPC_LID_LC, NPC_LT_LC_IP6, 8 * 8, 16 * 8);
	/* IP6 DIP: offset 24B, len 16B */
	kex_cap.bit.dip6_addr = npc_is_kex_enabled(npc, NPC_LID_LC, NPC_LT_LC_IP6, 24 * 8, 16 * 8);
	/* ESP SPI: offset 0B, len 4B */
	kex_cap.bit.ipsec_spi = npc_is_kex_enabled(npc, NPC_LID_LE, NPC_LT_LE_ESP, 0 * 8, 4 * 8);
	/* VXLAN VNI: offset 4B, len 3B */
	kex_cap.bit.ld_vni = npc_is_kex_enabled(npc, NPC_LID_LE, NPC_LT_LE_VXLAN, 0 * 8, 3 * 8);

	/* Custom L3 frame: varied offset and lengths */
	kex_cap.bit.custom_l3 = npc_is_kex_enabled(npc, NPC_LID_LC, NPC_LT_LC_CUSTOM0, 0, 0);
	kex_cap.bit.custom_l3 |=
		(uint64_t)npc_is_kex_enabled(npc, NPC_LID_LC, NPC_LT_LC_CUSTOM1, 0, 0);
	/* SCTP sport : offset 0B, len 2B */
	kex_cap.bit.sctp_sport = npc_is_kex_enabled(npc, NPC_LID_LD, NPC_LT_LD_SCTP, 0 * 8, 2 * 8);
	/* SCTP dport : offset 2B, len 2B */
	kex_cap.bit.sctp_dport = npc_is_kex_enabled(npc, NPC_LID_LD, NPC_LT_LD_SCTP, 2 * 8, 2 * 8);
	/* ICMP type : offset 0B, len 1B */
	kex_cap.bit.icmp_type = npc_is_kex_enabled(npc, NPC_LID_LD, NPC_LT_LD_ICMP, 0 * 8, 1 * 8);
	/* ICMP code : offset 1B, len 1B */
	kex_cap.bit.icmp_code = npc_is_kex_enabled(npc, NPC_LID_LD, NPC_LT_LD_ICMP, 1 * 8, 1 * 8);
	/* ICMP id : offset 4B, len 2B */
	kex_cap.bit.icmp_id = npc_is_kex_enabled(npc, NPC_LID_LD, NPC_LT_LD_ICMP, 4 * 8, 2 * 8);
	/* IGMP grp_addr : offset 4B, len 4B */
	kex_cap.bit.igmp_grp_addr =
		npc_is_kex_enabled(npc, NPC_LID_LD, NPC_LT_LD_IGMP, 4 * 8, 4 * 8);
	/* GTPU teid : offset 4B, len 4B */
	kex_cap.bit.gtpv1_teid = npc_is_kex_enabled(npc, NPC_LID_LE, NPC_LT_LE_GTPU, 4 * 8, 4 * 8);
	return kex_cap.all_bits;
}

#define BYTESM1_SHIFT 16
#define HDR_OFF_SHIFT 8
static void
npc_update_kex_info(struct npc_xtract_info *xtract_info, uint64_t val)
{
	xtract_info->use_hash = ((val >> 20) & 0x1);
	xtract_info->len = ((val >> BYTESM1_SHIFT) & 0xf) + 1;
	xtract_info->hdr_off = (val >> HDR_OFF_SHIFT) & 0xff;
	xtract_info->key_off = val & 0x3f;
	xtract_info->enable = ((val >> 7) & 0x1);
	xtract_info->flags_enable = ((val >> 6) & 0x1);
}

int
npc_mcam_alloc_entries(struct mbox *mbox, int ref_mcam, int *alloc_entry, int req_count, int prio,
		       int *resp_count, bool is_conti)
{
	struct npc_mcam_alloc_entry_req *req;
	struct npc_mcam_alloc_entry_rsp *rsp;
	int rc = -ENOSPC;
	int i;

	req = mbox_alloc_msg_npc_mcam_alloc_entry(mbox_get(mbox));
	if (req == NULL)
		goto exit;
	req->contig = is_conti;
	req->count = req_count;
	req->ref_priority = prio;
	req->ref_entry = ref_mcam;

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;
	for (i = 0; i < rsp->count; i++)
		alloc_entry[i] = rsp->entry_list[i];
	*resp_count = rsp->count;
	if (is_conti)
		alloc_entry[0] = rsp->entry;
	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

uint8_t
npc_kex_key_type_config_get(struct npc *npc)
{
	/* KEX is configured just for X2 */
	if (npc->keyw[ROC_NPC_INTF_RX] == 1)
		return NPC_CN20K_MCAM_KEY_X2;

	/* KEX is configured just for X4 */
	if (npc->keyw[ROC_NPC_INTF_RX] == 2)
		return NPC_CN20K_MCAM_KEY_X4;

	/* KEX is configured for both X2 and X4 */
	return NPC_CN20K_MCAM_KEY_DYN;
}

int
npc_mcam_alloc_entry(struct npc *npc, struct roc_npc_flow *mcam, struct roc_npc_flow *ref_mcam,
		     uint8_t prio, int *resp_count)
{
	struct mbox *mbox = mbox_get(npc->mbox);
	struct npc_mcam_alloc_entry_req *req;
	struct npc_mcam_alloc_entry_rsp *rsp;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_npc_mcam_alloc_entry(mbox);
	if (req == NULL)
		goto exit;

	req->count = 1;
	req->ref_priority = prio;
	req->ref_entry = ref_mcam ? ref_mcam->mcam_id : 0;
	req->kw_type = mcam->key_type;

	if (npc_kex_key_type_config_get(npc) == NPC_CN20K_MCAM_KEY_DYN)
		req->virt = 1;

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	mcam->mcam_id = rsp->entry_list[0];
	mcam->nix_intf = ref_mcam ? ref_mcam->nix_intf : 0;
	*resp_count = rsp->count;

	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

int
npc_mcam_ena_dis_entry(struct npc *npc, struct roc_npc_flow *mcam, bool enable)
{
	struct npc_mcam_ena_dis_entry_req *req;
	struct mbox *mbox = mbox_get(npc->mbox);
	int rc = -ENOSPC;

	if (enable)
		req = mbox_alloc_msg_npc_mcam_ena_entry(mbox);
	else
		req = mbox_alloc_msg_npc_mcam_dis_entry(mbox);

	if (req == NULL)
		goto exit;
	req->entry = mcam->mcam_id;
	mcam->enable = enable;
	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
npc_mcam_write_entry(struct mbox *mbox, struct roc_npc_flow *mcam)
{
	struct npc_mcam_write_entry_req *req;
	struct npc_cn20k_mcam_write_entry_req *cn20k_req;
	struct mbox_msghdr *rsp;
	int rc = -ENOSPC;
	uint16_t ctr = 0;
	int i;

	if (mcam->use_ctr && mcam->ctr_id == NPC_COUNTER_NONE) {
		rc = npc_mcam_alloc_counter(mbox, &ctr);
		if (rc)
			return rc;
		mcam->ctr_id = ctr;

		rc = npc_mcam_clear_counter(mbox, mcam->ctr_id);
		if (rc)
			return rc;
	}

	if (roc_model_is_cn20k()) {
		cn20k_req = mbox_alloc_msg_npc_cn20k_mcam_write_entry(mbox_get(mbox));
		if (cn20k_req == NULL) {
			mbox_put(mbox);
			if (mcam->use_ctr)
				npc_mcam_free_counter(mbox, ctr);

			return rc;
		}
		cn20k_req->entry = mcam->mcam_id;
		cn20k_req->intf = mcam->nix_intf;
		cn20k_req->enable_entry = mcam->enable;
		cn20k_req->entry_data.action = mcam->npc_action;
		cn20k_req->entry_data.action2 = mcam->npc_action2;
		cn20k_req->entry_data.vtag_action = mcam->vtag_action;
		cn20k_req->hw_prio = mcam->priority;
		if (mcam->use_ctr)
			cn20k_req->cntr = mcam->ctr_id;

		for (i = 0; i < NPC_MCAM_KEY_X4_WORDS; i++) {
			cn20k_req->entry_data.kw[i] = mcam->mcam_data[i];
			cn20k_req->entry_data.kw_mask[i] = mcam->mcam_mask[i];
		}
	} else {
		req = mbox_alloc_msg_npc_mcam_write_entry(mbox_get(mbox));
		if (req == NULL) {
			mbox_put(mbox);
			if (mcam->use_ctr)
				npc_mcam_free_counter(mbox, ctr);

			return rc;
		}
		req->entry = mcam->mcam_id;
		req->intf = mcam->nix_intf;
		req->enable_entry = mcam->enable;
		req->entry_data.action = mcam->npc_action;
		req->entry_data.vtag_action = mcam->vtag_action;
		if (mcam->use_ctr) {
			req->set_cntr = 1;
			req->cntr = mcam->ctr_id;
		}

		for (i = 0; i < NPC_MCAM_KEY_X4_WORDS; i++) {
			req->entry_data.kw[i] = mcam->mcam_data[i];
			req->entry_data.kw_mask[i] = mcam->mcam_mask[i];
		}
	}
	rc = mbox_process_msg(mbox, (void *)&rsp);
	mbox_put(mbox);
	return rc;
}

static void
npc_mcam_kex_cfg_dump(struct npc_cn20k_get_kex_cfg_rsp *kex_rsp)
{
	for (int i = 0; i < NPC_MAX_INTF; i++) {
		for (int j = 0; j < NPC_MAX_EXTRACTOR; j++) {
			if (kex_rsp->intf_extr[i][j] == 0)
				continue;
			plt_info("Intf %d, Extr %d: 0x%" PRIx64, i, j, kex_rsp->intf_extr[i][j]);
		}
	}

	for (int i = 0; i < NPC_MAX_INTF; i++) {
		for (int j = 0; j < NPC_MAX_EXTRACTOR; j++) {
			for (int k = 0; k < NPC_MAX_LT; k++) {
				if (kex_rsp->intf_extr_lt[i][j][k] == 0)
					continue;
				plt_info("Intf %d, Extr %d, LT %d: 0x%" PRIx64, i, j, k,
					 kex_rsp->intf_extr_lt[i][j][k]);
			}
		}
	}
}

static void
npc_mcam_process_mkex_cfg_cn20k(struct npc *npc, struct npc_cn20k_get_kex_cfg_rsp *kex_rsp)
{
	volatile uint64_t(*q)[NPC_MAX_INTF][NPC_MAX_EXTRACTOR][NPC_MAX_LT];
	volatile uint64_t(*d)[NPC_MAX_INTF][NPC_MAX_EXTRACTOR];
	struct npc_xtract_info *x_info = NULL;
	union npc_kex_ldata_flags_cfg *ld_info = NULL;
	int ex, lt, ix;
	npc_dxcfg_cn20k_t *p;
	npc_lid_cn20k_t *l;
	uint64_t keyw;
	uint64_t val;

	npc->keyx_supp_nmask[NPC_MCAM_RX] = kex_rsp->rx_keyx_cfg & 0x7fffffffULL;
	npc->keyx_supp_nmask[NPC_MCAM_TX] = kex_rsp->tx_keyx_cfg & 0x7fffffffULL;
	npc->keyx_len[NPC_MCAM_RX] = npc_supp_key_len(npc->keyx_supp_nmask[NPC_MCAM_RX]);
	npc->keyx_len[NPC_MCAM_TX] = npc_supp_key_len(npc->keyx_supp_nmask[NPC_MCAM_TX]);

	keyw = (kex_rsp->rx_keyx_cfg >> 32) & 0x7ULL;
	npc->keyw[NPC_MCAM_RX] = keyw;
	keyw = (kex_rsp->tx_keyx_cfg >> 32) & 0x7ULL;
	npc->keyw[NPC_MCAM_TX] = keyw;

	p = &npc->prx_dxcfg_cn20k;
	l = &npc->lid_cfg;
	q = (volatile uint64_t(*)[][NPC_MAX_EXTRACTOR][NPC_MAX_LT])(&kex_rsp->intf_extr_lt);
	d = (volatile uint64_t(*)[][NPC_MAX_EXTRACTOR])(&kex_rsp->intf_extr);
	for (ix = 0; ix < NPC_MAX_INTF; ix++) {
		for (ex = 0; ex < NPC_MAX_EXTRACTOR; ex++) {
			val = (*d)[ix][ex];
			ld_info = &(*l)[ix][ex];
			ld_info->s.lid = (val & 0x7);
			for (lt = 0; lt < NPC_MAX_LT; lt++) {
				x_info = &(*p)[ix][ex][lt].xtract;
				val = (*q)[ix][ex][lt];
				npc_update_kex_info(x_info, val);
			}
		}
	}
}

static void
npc_mcam_process_mkex_cfg(struct npc *npc, struct npc_get_kex_cfg_rsp *kex_rsp)
{
	volatile uint64_t(*q)[NPC_MAX_INTF][NPC_MAX_LID][NPC_MAX_LT][NPC_MAX_LD];
	struct npc_xtract_info *x_info = NULL;
	int lid, lt, ld, fl, ix;
	npc_dxcfg_t *p;
	uint64_t keyw;
	uint64_t val;

	npc->keyx_supp_nmask[NPC_MCAM_RX] = kex_rsp->rx_keyx_cfg & 0x7fffffffULL;
	npc->keyx_supp_nmask[NPC_MCAM_TX] = kex_rsp->tx_keyx_cfg & 0x7fffffffULL;
	npc->keyx_len[NPC_MCAM_RX] = npc_supp_key_len(npc->keyx_supp_nmask[NPC_MCAM_RX]);
	npc->keyx_len[NPC_MCAM_TX] = npc_supp_key_len(npc->keyx_supp_nmask[NPC_MCAM_TX]);

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
	q = (volatile uint64_t(*)[][NPC_MAX_LID][NPC_MAX_LT][NPC_MAX_LD])(&kex_rsp->intf_lid_lt_ld);
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
npc_mcam_fetch_hw_cap(struct npc *npc, uint8_t *npc_hw_cap)
{
	struct get_hw_cap_rsp *hw_cap_rsp;
	struct mbox *mbox = mbox_get(npc->mbox);
	int rc = 0;

	*npc_hw_cap = 0;

	mbox_alloc_msg_get_hw_cap(mbox);
	rc = mbox_process_msg(mbox, (void *)&hw_cap_rsp);
	if (rc) {
		plt_err("Failed to fetch NPC HW capability");
		goto done;
	}

	*npc_hw_cap = hw_cap_rsp->npc_hash_extract;
done:
	mbox_put(mbox);
	return rc;
}

int
npc_mcam_fetch_kex_cfg(struct npc *npc)
{
	struct npc_get_kex_cfg_rsp *kex_rsp;
	struct npc_cn20k_get_kex_cfg_rsp *kex_rsp_20k;
	struct mbox *mbox = mbox_get(npc->mbox);
	int rc = 0;


	if (!roc_model_is_cn20k()) {
		mbox_alloc_msg_npc_get_kex_cfg(mbox);
		rc = mbox_process_msg(mbox, (void *)&kex_rsp);
		if (rc) {
			plt_err("Failed to fetch NPC KEX config");
			goto done;
		}

		mbox_memcpy((char *)npc->profile_name, kex_rsp->mkex_pfl_name, MKEX_NAME_LEN);

		npc->exact_match_ena = (kex_rsp->rx_keyx_cfg >> 40) & 0xF;
		npc_mcam_process_mkex_cfg(npc, kex_rsp);
	} else {
		mbox_alloc_msg_npc_cn20k_get_kex_cfg(mbox);
		rc = mbox_process_msg(mbox, (void *)&kex_rsp_20k);
		if (rc) {
			plt_err("Failed to fetch NPC KEX config");
			goto done;
		}

		mbox_memcpy((char *)npc->profile_name, kex_rsp_20k->mkex_pfl_name, MKEX_NAME_LEN);

		npc->exact_match_ena = (kex_rsp_20k->rx_keyx_cfg >> 40) & 0xF;
		npc_mcam_process_mkex_cfg_cn20k(npc, kex_rsp_20k);
		if (npc->enable_debug)
			npc_mcam_kex_cfg_dump(kex_rsp_20k);
	}

done:
	mbox_put(mbox);
	return rc;
}

static void
npc_mcam_set_channel(struct roc_npc_flow *flow, struct npc_cn20k_mcam_write_entry_req *req,
		     uint16_t channel, uint16_t chan_mask, bool is_second_pass)
{
	uint16_t chan = 0, mask = 0;

	req->entry_data.kw[0] &= ~(GENMASK(11, 0));
	req->entry_data.kw_mask[0] &= ~(GENMASK(11, 0));
	flow->mcam_data[0] &= ~(GENMASK(11, 0));
	flow->mcam_mask[0] &= ~(GENMASK(11, 0));

	chan = channel;
	mask = chan_mask;

	if (roc_model_runtime_is_cn10k()) {
		if (is_second_pass) {
			chan = (channel | NIX_CHAN_CPT_CH_START);
			mask = (chan_mask | NIX_CHAN_CPT_CH_START);
		} else {
			if (!roc_npc_action_is_rx_inline(flow->npc_action)) {
				/*
				 * Clear bits 10 & 11 corresponding to CPT
				 * channel. By default, rules should match
				 * both first pass packets and second pass
				 * packets from CPT.
				 */
				chan = (channel & NIX_CHAN_CPT_X2P_MASK);
				mask = (chan_mask & NIX_CHAN_CPT_X2P_MASK);
			}
		}
	}

	req->entry_data.kw[0] |= (uint64_t)chan;
	req->entry_data.kw_mask[0] |= (uint64_t)mask;
	flow->mcam_data[0] |= (uint64_t)chan;
	flow->mcam_mask[0] |= (uint64_t)mask;
}

#define NPC_PF_FUNC_WIDTH    2
#define NPC_KEX_PF_FUNC_MASK 0xFFFF

static int
npc_mcam_set_pf_func(struct npc *npc, struct roc_npc_flow *flow, uint16_t pf_func)
{
	uint16_t nr_bytes, hdr_offset, key_offset, pf_func_offset;
	uint8_t *flow_mcam_data, *flow_mcam_mask;
	struct npc_lid_lt_xtract_info *xinfo;
	bool pffunc_found = false;
	uint16_t mask = 0xFFFF;
	int i;

	flow_mcam_data = (uint8_t *)flow->mcam_data;
	flow_mcam_mask = (uint8_t *)flow->mcam_mask;

	xinfo = &npc->prx_dxcfg[NIX_INTF_TX][NPC_LID_LA][NPC_LT_LA_IH_NIX_ETHER];

	for (i = 0; i < NPC_MAX_LD; i++) {
		nr_bytes = xinfo->xtract[i].len;
		hdr_offset = xinfo->xtract[i].hdr_off;
		key_offset = xinfo->xtract[i].key_off;

		if (hdr_offset > 0 || nr_bytes < NPC_PF_FUNC_WIDTH)
			continue;
		else
			pffunc_found = true;

		pf_func_offset = key_offset + nr_bytes - NPC_PF_FUNC_WIDTH;
		memcpy((void *)&flow_mcam_data[pf_func_offset], (uint8_t *)&pf_func,
		       NPC_PF_FUNC_WIDTH);
		memcpy((void *)&flow_mcam_mask[pf_func_offset], (uint8_t *)&mask,
		       NPC_PF_FUNC_WIDTH);
	}
	if (!pffunc_found)
		return -EINVAL;

	return 0;
}

static int
npc_mcam_set_pf_func_cn20k(struct npc *npc, struct roc_npc_flow *flow, uint16_t pf_func)
{
	uint16_t nr_bytes, hdr_offset, key_offset, pf_func_offset;
	struct npc_lid_lt_xtract_info_cn20k *xinfo;
	uint8_t *flow_mcam_data, *flow_mcam_mask;
	bool pffunc_found = false;
	uint16_t mask = 0xFFFF;
	int i;

	flow_mcam_data = (uint8_t *)flow->mcam_data;
	flow_mcam_mask = (uint8_t *)flow->mcam_mask;

	xinfo = npc->prx_dxcfg_cn20k[NIX_INTF_TX][NPC_LID_LA];

	for (i = 0; i < NPC_MAX_LT; i++) {
		nr_bytes = xinfo[i].xtract.len;
		hdr_offset = xinfo[i].xtract.hdr_off;
		key_offset = xinfo[i].xtract.key_off;

		if (hdr_offset > 0 || nr_bytes < NPC_PF_FUNC_WIDTH)
			continue;
		else
			pffunc_found = true;

		pf_func_offset = key_offset + nr_bytes - NPC_PF_FUNC_WIDTH;
		memcpy((void *)&flow_mcam_data[pf_func_offset], (uint8_t *)&pf_func,
		       NPC_PF_FUNC_WIDTH);
		memcpy((void *)&flow_mcam_mask[pf_func_offset], (uint8_t *)&mask,
		       NPC_PF_FUNC_WIDTH);
	}
	if (!pffunc_found)
		return -EINVAL;

	return 0;
}

int
npc_mcam_alloc_and_write(struct npc *npc, struct roc_npc_flow *flow, struct npc_parse_state *pst)
{
	struct npc_cn20k_mcam_write_entry_req *cn20k_req;
	struct npc_cn20k_mcam_write_entry_req req;
	struct nix_inl_dev *inl_dev = NULL;
	struct mbox *mbox = npc->mbox;
	struct mbox_msghdr *rsp;
	struct idev_cfg *idev;
	uint16_t pf_func = 0;
	uint16_t ctr = ~(0);
	uint32_t la_offset;
	uint64_t mask;
	int rc, idx;
	int entry;

	PLT_SET_USED(pst);

	idev = idev_get_cfg();
	if (idev)
		inl_dev = idev->nix_inl_dev;

	if (inl_dev && inl_dev->ipsec_index) {
		if (flow->is_inline_dev)
			mbox = inl_dev->dev.mbox;
	}

	if (flow->use_ctr) {
		rc = npc_mcam_alloc_counter(mbox, &ctr);
		if (rc)
			return rc;

		flow->ctr_id = ctr;
		rc = npc_mcam_clear_counter(mbox, flow->ctr_id);
		if (rc)
			return rc;
	}

	if (roc_model_is_cn20k()) {
		req.hw_prio = flow->priority;
		flow->key_type = npc_get_key_type(npc, flow);
		req.req_kw_type = flow->key_type;
	}

	if (flow->nix_intf == NIX_INTF_RX && flow->is_inline_dev && inl_dev &&
	    inl_dev->ipsec_index && inl_dev->is_multi_channel) {
		if (inl_dev->curr_ipsec_idx >= inl_dev->alloc_ipsec_rules)
			return NPC_ERR_MCAM_ALLOC;
		entry = inl_dev->ipsec_index[inl_dev->curr_ipsec_idx];
		inl_dev->curr_ipsec_idx++;
		flow->use_pre_alloc = 1;
	} else {
		entry = npc_get_free_mcam_entry(mbox, flow, npc);
		if (entry < 0) {
			if (flow->use_ctr)
				npc_mcam_free_counter(mbox, ctr);
			return NPC_ERR_MCAM_ALLOC;
		}
	}

	if (flow->nix_intf == NIX_INTF_TX) {
		uint16_t pffunc = flow->tx_pf_func;

		if (flow->has_rep)
			pffunc = flow->rep_pf_func;

		pffunc = plt_cpu_to_be_16(pffunc);

		if (roc_model_is_cn20k())
			rc = npc_mcam_set_pf_func_cn20k(npc, flow, pffunc);
		else
			rc = npc_mcam_set_pf_func(npc, flow, pffunc);
		if (rc)
			return rc;
	}

	if (flow->is_sampling_rule) {
		/* Save and restore any mark value set */
		uint16_t mark = (flow->npc_action >> 40) & 0xffff;
		uint16_t mce_index = 0;
		uint32_t rqs[2] = {};

		rqs[1] = flow->recv_queue;
		rc = roc_nix_mcast_list_setup(npc->mbox, flow->nix_intf, 2, flow->mcast_pf_funcs,
					      flow->mcast_channels, rqs, &flow->mcast_grp_index,
					      &flow->mce_start_index);
		if (rc)
			return rc;

		flow->npc_action = NIX_RX_ACTIONOP_MCAST;
		mce_index = flow->mce_start_index;
		if (flow->nix_intf == NIX_INTF_TX) {
			flow->npc_action |= (uint64_t)mce_index << 12;
			flow->npc_action |= (uint64_t)mark << 32;
		} else {
			flow->npc_action |= (uint64_t)mce_index << 20;
			flow->npc_action |= (uint64_t)mark << 40;
		}
	}

	req.cntr = flow->ctr_id;
	req.entry = entry;

	req.intf = (flow->nix_intf == NIX_INTF_RX) ? NPC_MCAM_RX : NPC_MCAM_TX;
	req.enable_entry = 1;
	if (flow->nix_intf == NIX_INTF_RX)
		flow->npc_action |= (uint64_t)flow->recv_queue << 20;
	req.entry_data.action = flow->npc_action;
	req.entry_data.action2 = flow->npc_action2;

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
	req.entry_data.vtag_action = flow->vtag_action;

	for (idx = 0; idx < ROC_NPC_MAX_MCAM_WIDTH_DWORDS; idx++) {
		req.entry_data.kw[idx] = flow->mcam_data[idx];
		req.entry_data.kw_mask[idx] = flow->mcam_mask[idx];
	}

	if (flow->nix_intf == NIX_INTF_RX) {
		if (inl_dev && inl_dev->is_multi_channel &&
		    roc_npc_action_is_rx_inline(flow->npc_action)) {
			pf_func = nix_inl_dev_pffunc_get();
			req.entry_data.action &= ~(GENMASK(19, 4));
			req.entry_data.action |= (uint64_t)pf_func << 4;
			flow->npc_action &= ~(GENMASK(19, 4));
			flow->npc_action |= (uint64_t)pf_func << 4;

			npc_mcam_set_channel(flow, &req, inl_dev->channel, inl_dev->chan_mask,
					     false);
		} else if (flow->has_rep) {
			pf_func = (flow->rep_act_pf_func == 0) ? flow->rep_pf_func :
								 flow->rep_act_pf_func;
			req.entry_data.action &= ~(GENMASK(19, 4));
			req.entry_data.action |= (uint64_t)pf_func << 4;
			flow->npc_action &= ~(GENMASK(19, 4));
			flow->npc_action |= (uint64_t)pf_func << 4;
			npc_mcam_set_channel(flow, &req, flow->rep_channel, (BIT_ULL(12) - 1),
					     false);
		} else if (npc->is_sdp_link) {
			npc_mcam_set_channel(flow, &req, npc->sdp_channel, npc->sdp_channel_mask,
					     pst->is_second_pass_rule);
		} else {
			npc_mcam_set_channel(flow, &req, npc->channel, (BIT_ULL(12) - 1),
					     pst->is_second_pass_rule);
		}
		/*
		 * For second pass rule, set LA LTYPE to CPT_HDR.
		 * For all other rules, set LA LTYPE to match both 1st pass and 2nd pass ltypes.
		 */
		if (pst->is_second_pass_rule || (!pst->is_second_pass_rule && pst->has_eth_type)) {
			la_offset = plt_popcount32(npc->keyx_supp_nmask[flow->nix_intf] &
						   ((1ULL << 9 /* LA offset */) - 1));
			la_offset *= 4;

			mask = ~((0xfULL << la_offset));
			req.entry_data.kw[0] &= mask;
			req.entry_data.kw_mask[0] &= mask;
			flow->mcam_data[0] &= mask;
			flow->mcam_mask[0] &= mask;
			if (pst->is_second_pass_rule) {
				req.entry_data.kw[0] |= ((uint64_t)NPC_LT_LA_CPT_HDR) << la_offset;
				req.entry_data.kw_mask[0] |= (0xFULL << la_offset);
				flow->mcam_data[0] |= ((uint64_t)NPC_LT_LA_CPT_HDR) << la_offset;
				flow->mcam_mask[0] |= (0xFULL << la_offset);
			} else {
				/* Mask ltype ETHER (0x2) and CPT_HDR (0xa)  */
				req.entry_data.kw[0] |= (0x2ULL << la_offset);
				req.entry_data.kw_mask[0] |= (0x7ULL << la_offset);
				flow->mcam_data[0] |= (0x2ULL << la_offset);
				flow->mcam_mask[0] |= (0x7ULL << la_offset);
			}
		}
	}

	if (roc_model_is_cn20k()) {
		cn20k_req = mbox_alloc_msg_npc_cn20k_mcam_write_entry(mbox_get(mbox));
		if (cn20k_req == NULL) {
			mbox_put(mbox);
			if (flow->use_ctr)
				npc_mcam_free_counter(mbox, ctr);

			return rc;
		}
		cn20k_req->entry = req.entry;
		cn20k_req->intf = req.intf;
		cn20k_req->enable_entry = req.enable_entry;
		cn20k_req->entry_data.action = req.entry_data.action;
		cn20k_req->entry_data.vtag_action = req.entry_data.vtag_action;
		cn20k_req->hw_prio = req.hw_prio;
		cn20k_req->req_kw_type = req.req_kw_type;
		if (flow->use_ctr)
			cn20k_req->cntr = req.cntr;

		mbox_memcpy(&cn20k_req->entry_data, &req.entry_data,
			    sizeof(struct cn20k_mcam_entry));
	} else {
		struct npc_mcam_write_entry_req *req_leg;

		req_leg = mbox_alloc_msg_npc_mcam_write_entry(mbox_get(mbox));
		if (req_leg == NULL) {
			rc = -ENOSPC;
			goto exit;
		}
		for (idx = 0; idx < NPC_MAX_KWS_IN_KEY; idx++) {
			req_leg->entry_data.kw[idx] = req.entry_data.kw[idx];
			req_leg->entry_data.kw_mask[idx] = req.entry_data.kw_mask[idx];
		}
		req_leg->entry = req.entry;
		req_leg->intf = req.intf;
		req_leg->enable_entry = req.enable_entry;
		req_leg->cntr = req.cntr;
		req_leg->entry_data.action = req.entry_data.action;
		req_leg->entry_data.vtag_action = req.entry_data.vtag_action;
		req_leg->set_cntr = flow->use_ctr;
	}
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc != 0)
		goto exit;

	flow->mcam_id = entry;

	if (flow->use_ctr)
		flow->ctr_id = ctr;
	rc = 0;

exit:
	mbox_put(mbox);
	if (rc && flow->is_sampling_rule)
		roc_nix_mcast_list_free(npc->mbox, flow->mcast_grp_index);
	return rc;
}

static void
npc_set_vlan_ltype(struct npc_parse_state *pst)
{
	uint64_t val, mask;
	uint8_t lb_offset;

	if (roc_model_is_cn20k()) {
		lb_offset = plt_popcount32(pst->npc->keyx_supp_nmask[pst->nix_intf] &
					   ((1ULL << NPC_CN20K_LTYPE_LB_OFFSET) - 1));

	} else {
		lb_offset = plt_popcount32(pst->npc->keyx_supp_nmask[pst->nix_intf] &
					   ((1ULL << NPC_LTYPE_LB_OFFSET) - 1));
	}
	lb_offset *= 4;

	mask = ~((0xfULL << lb_offset));
	pst->flow->mcam_data[0] &= mask;
	pst->flow->mcam_mask[0] &= mask;
	/* NPC_LT_LB_CTAG: 0b0010, NPC_LT_LB_STAG_QINQ: 0b0011
	 * Set LB layertype/mask as 0b0010/0b1110 to match both.
	 */
	val = ((uint64_t)(NPC_LT_LB_CTAG & NPC_LT_LB_STAG_QINQ)) << lb_offset;
	pst->flow->mcam_data[0] |= val;
	pst->flow->mcam_mask[0] |= (0xeULL << lb_offset);
}

static void
npc_set_ipv6ext_ltype_mask(struct npc_parse_state *pst)
{
	uint8_t lc_offset, lcflag_offset;
	uint64_t val, mask;

	if (roc_model_is_cn20k()) {
		lc_offset = plt_popcount32(pst->npc->keyx_supp_nmask[pst->nix_intf] &
					   ((1ULL << NPC_CN20K_LTYPE_LC_OFFSET) - 1));

	} else {
		lc_offset = plt_popcount32(pst->npc->keyx_supp_nmask[pst->nix_intf] &
					   ((1ULL << NPC_LTYPE_LC_OFFSET) - 1));
	}
	lc_offset *= 4;

	mask = ~((0xfULL << lc_offset));
	pst->flow->mcam_data[0] &= mask;
	pst->flow->mcam_mask[0] &= mask;
	/* NPC_LT_LC_IP6: 0b0100, NPC_LT_LC_IP6_EXT: 0b0101
	 * Set LC layertype/mask as 0b0100/0b1110 to match both.
	 */
	val = ((uint64_t)(NPC_LT_LC_IP6 & NPC_LT_LC_IP6_EXT)) << lc_offset;
	pst->flow->mcam_data[0] |= val;
	pst->flow->mcam_mask[0] |= (0xeULL << lc_offset);

	/* If LC LFLAG is non-zero, set the LC LFLAG mask to 0xF. In general
	 * case flag mask is set same as the value in data. For example, to
	 * match 3 VLANs, flags have to match a range of values. But, for IPv6
	 * extended attributes matching, we need an exact match. Hence, set the
	 * mask as 0xF. This is done only if LC LFLAG value is non-zero,
	 * because for AH and ESP, LC LFLAG is zero and we don't want to match
	 * zero in LFLAG.
	 */
	if (pst->npc->keyx_supp_nmask[pst->nix_intf] & (1ULL << NPC_LFLAG_LC_OFFSET)) {
		if (roc_model_is_cn20k()) {
			lcflag_offset = plt_popcount32(pst->npc->keyx_supp_nmask[pst->nix_intf] &
						       ((1ULL << NPC_CN20K_LFLAG_LC_OFFSET) - 1));

		} else {
			lcflag_offset = plt_popcount32(pst->npc->keyx_supp_nmask[pst->nix_intf] &
						       ((1ULL << NPC_LFLAG_LC_OFFSET) - 1));
		}
		lcflag_offset *= 4;

		mask = (0xfULL << lcflag_offset);
		val = pst->flow->mcam_data[0] & mask;
		if (val)
			pst->flow->mcam_mask[0] |= mask;
	}
}

int
npc_program_mcam(struct npc *npc, struct npc_parse_state *pst, bool mcam_alloc)
{
	/* This is non-LDATA part in search key */
	uint64_t key_data[2] = {0ULL, 0ULL};
	uint64_t key_mask[2] = {0ULL, 0ULL};
	int key_len, bit = 0, index, rc = 0;
	struct nix_inl_dev *inl_dev = NULL;
	int intf = pst->flow->nix_intf;
	bool skip_base_rule = false;
	int off, idx, data_off = 0;
	uint8_t lid, mask, data;
	struct idev_cfg *idev;
	uint16_t layer_info;
	uint64_t lt, flags;
	struct mbox *mbox;

	/* Skip till Layer A data start */
	while (bit < NPC_PARSE_KEX_S_LA_OFFSET) {
		if (npc->keyx_supp_nmask[intf] & (1 << bit))
			data_off++;
		bit++;
	}

	/* Each bit represents 1 nibble */
	data_off *= 4;

	index = 0;
	if (!roc_model_is_cn20k()) {
		for (lid = 0; lid < NPC_MAX_LID; lid++) {
			/* Offset in key */
			off = NPC_PARSE_KEX_S_LID_OFFSET(lid);
			lt = pst->lt[lid] & 0xf;
			flags = pst->flags[lid] & 0xff;

			/* NPC_LAYER_KEX_S */
			layer_info = ((npc->keyx_supp_nmask[intf] >> off) & 0x7);
			if (!layer_info)
				continue;

			for (idx = 0; idx <= 2; idx++) {
				if (!(layer_info & (1 << idx)))
					continue;

				if (idx == 2) {
					data = lt;
					mask = 0xf;
				} else if (idx == 1) {
					data = ((flags >> 4) & 0xf);
					mask = ((flags >> 4) & 0xf);
				} else {
					data = (flags & 0xf);
					mask = (flags & 0xf);
				}

				if (data_off >= 64) {
					data_off = 0;
					index++;
				}
				key_data[index] |= ((uint64_t)data << data_off);

				if (lt == 0)
					mask = 0;
				key_mask[index] |= ((uint64_t)mask << data_off);
				data_off += 4;
			}
		}
	} else {
		for (lid = 0; lid < NPC_MAX_LID; lid++) {
			/* Offset in key */
			off = NPC_PARSE_KEX_S_LID_OFFSET_CN20K(lid);
			lt = pst->lt[lid] & 0xf;
			flags = pst->flags[lid] & 0xf;

			/* NPC_LAYER_KEX_S */
			layer_info = ((npc->keyx_supp_nmask[intf] >> off) & 0x3);
			if (!layer_info)
				continue;

			for (idx = 0; idx <= 1; idx++) {
				if (!(layer_info & (1 << idx)))
					continue;

				if (idx == 1) {
					data = lt;
					mask = 0xf;
				} else {
					data = (flags & 0xf);
					mask = (flags & 0xf);
				}

				if (data_off >= 64) {
					data_off = 0;
					index++;
				}
				key_data[index] |= ((uint64_t)data << data_off);

				if (lt == 0)
					mask = 0;
				key_mask[index] |= ((uint64_t)mask << data_off);
				data_off += 4;
			}
		}
	}

	/* Copy this into mcam string */
	key_len = (pst->npc->keyx_len[intf] + 7) / 8;
	memcpy(pst->flow->mcam_data, key_data, key_len);
	memcpy(pst->flow->mcam_mask, key_mask, key_len);

	if (pst->set_vlan_ltype_mask)
		npc_set_vlan_ltype(pst);

	if (pst->set_ipv6ext_ltype_mask)
		npc_set_ipv6ext_ltype_mask(pst);

	idev = idev_get_cfg();
	if (idev)
		inl_dev = idev->nix_inl_dev;
	if (inl_dev && inl_dev->is_multi_channel &&
	    roc_npc_action_is_rx_inline(pst->flow->npc_action))
		skip_base_rule = true;

	if ((pst->is_vf || pst->flow->is_rep_vf) && pst->flow->nix_intf == NIX_INTF_RX &&
	    !skip_base_rule) {
		if (pst->flow->has_rep)
			mbox = mbox_get(pst->flow->rep_mbox);
		else
			mbox = mbox_get(npc->mbox);
		if (roc_model_is_cn20k()) {
			struct npc_cn20k_mcam_read_base_rule_rsp *base_rule_rsp;
			struct cn20k_mcam_entry *base_entry;

			(void)mbox_alloc_msg_npc_cn20k_read_base_steer_rule(mbox);
			rc = mbox_process_msg(mbox, (void *)&base_rule_rsp);
			if (rc) {
				mbox_put(mbox);
				plt_err("Failed to fetch VF's base MCAM entry");
				return rc;
			}
			mbox_put(mbox);
			base_entry = &base_rule_rsp->entry;
			for (idx = 0; idx < ROC_NPC_MAX_MCAM_WIDTH_DWORDS; idx++) {
				pst->flow->mcam_data[idx] |= base_entry->kw[idx];
				pst->flow->mcam_mask[idx] |= base_entry->kw_mask[idx];
			}

		} else {
			struct npc_mcam_read_base_rule_rsp *base_rule_rsp;
			struct mcam_entry *base_entry;

			(void)mbox_alloc_msg_npc_read_base_steer_rule(mbox);
			rc = mbox_process_msg(mbox, (void *)&base_rule_rsp);
			if (rc) {
				mbox_put(mbox);
				plt_err("Failed to fetch VF's base MCAM entry");
				return rc;
			}
			mbox_put(mbox);
			base_entry = &base_rule_rsp->entry_data;
			for (idx = 0; idx < ROC_NPC_MAX_MCAM_WIDTH_DWORDS; idx++) {
				pst->flow->mcam_data[idx] |= base_entry->kw[idx];
				pst->flow->mcam_mask[idx] |= base_entry->kw_mask[idx];
			}
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
npc_flow_enable_all_entries(struct npc *npc, bool enable)
{
	struct nix_inl_dev *inl_dev;
	struct npc_flow_list *list;
	struct roc_npc_flow *flow;
	struct idev_cfg *idev;
	int rc = 0, idx;

	/* Free any MCAM counters and delete flow list */
	for (idx = 0; idx < npc->flow_max_priority; idx++) {
		list = &npc->flow_list[idx];
		TAILQ_FOREACH(flow, list, next) {
			flow->enable = enable;
			rc = npc_mcam_write_entry(npc->mbox, flow);
			if (rc)
				return rc;
		}
	}

	list = &npc->ipsec_list;
	idev = idev_get_cfg();
	if (!idev)
		return 0;
	inl_dev = idev->nix_inl_dev;

	if (inl_dev) {
		TAILQ_FOREACH(flow, list, next) {
			flow->enable = enable;
			rc = npc_mcam_write_entry(inl_dev->dev.mbox, flow);
			if (rc)
				return rc;
		}
	}
	return rc;
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
			if (flow->ctr_id != NPC_COUNTER_NONE) {
				rc |= npc_mcam_clear_counter(npc->mbox, flow->ctr_id);
				rc |= npc_mcam_free_counter(npc->mbox, flow->ctr_id);
			}

			if (flow->is_sampling_rule)
				roc_nix_mcast_list_free(npc->mbox, flow->mcast_grp_index);

			npc_delete_prio_list_entry(npc, flow);

			TAILQ_REMOVE(&npc->flow_list[idx], flow, next);
			plt_free(flow);
		}
	}
	return rc;
}
