/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

#define NIX_LSO_FRMT_IPV4_OFFSET_SHFT 3

static void
nix_lso_tcp(struct nix_lso_format_cfg *req, bool v4)
{
	__io struct nix_lso_format *field;

	/* Format works only with TCP packet marked by OL3/OL4 */
	field = (__io struct nix_lso_format *)&req->fields[0];
	req->field_mask = NIX_LSO_FIELD_MASK;
	/* Outer IPv4/IPv6 */
	field->layer = NIX_TXLAYER_OL3;
	field->offset = v4 ? 2 : 4;
	field->sizem1 = 1; /* 2B */
	field->alg = NIX_LSOALG_ADD_PAYLEN;
	field++;
	if (v4) {
		/* IPID field */
		field->layer = NIX_TXLAYER_OL3;
		field->offset = 4;
		field->sizem1 = 1;
		/* Incremented linearly per segment */
		field->alg = NIX_LSOALG_ADD_SEGNUM;
		field++;
	}

	/* TCP sequence number update */
	field->layer = NIX_TXLAYER_OL4;
	field->offset = 4;
	field->sizem1 = 3; /* 4 bytes */
	field->alg = NIX_LSOALG_ADD_OFFSET;
	field++;
	/* TCP flags field */
	field->layer = NIX_TXLAYER_OL4;
	field->offset = 12;
	field->sizem1 = 1;
	field->alg = NIX_LSOALG_TCP_FLAGS;
	field++;
}

static void
nix_lso_udp_tun_tcp(struct nix_lso_format_cfg *req, bool outer_v4,
		    bool inner_v4)
{
	__io struct nix_lso_format *field;

	field = (__io struct nix_lso_format *)&req->fields[0];
	req->field_mask = NIX_LSO_FIELD_MASK;
	/* Outer IPv4/IPv6 len */
	field->layer = NIX_TXLAYER_OL3;
	field->offset = outer_v4 ? 2 : 4;
	field->sizem1 = 1; /* 2B */
	field->alg = NIX_LSOALG_ADD_PAYLEN;
	field++;
	if (outer_v4) {
		/* IPID */
		field->layer = NIX_TXLAYER_OL3;
		field->offset = 4;
		field->sizem1 = 1;
		/* Incremented linearly per segment */
		field->alg = NIX_LSOALG_ADD_SEGNUM;
		field++;
	}

	/* Outer UDP length */
	field->layer = NIX_TXLAYER_OL4;
	field->offset = 4;
	field->sizem1 = 1;
	field->alg = NIX_LSOALG_ADD_PAYLEN;
	field++;

	/* Inner IPv4/IPv6 */
	field->layer = NIX_TXLAYER_IL3;
	field->offset = inner_v4 ? 2 : 4;
	field->sizem1 = 1; /* 2B */
	field->alg = NIX_LSOALG_ADD_PAYLEN;
	field++;
	if (inner_v4) {
		/* IPID field */
		field->layer = NIX_TXLAYER_IL3;
		field->offset = 4;
		field->sizem1 = 1;
		/* Incremented linearly per segment */
		field->alg = NIX_LSOALG_ADD_SEGNUM;
		field++;
	}

	/* TCP sequence number update */
	field->layer = NIX_TXLAYER_IL4;
	field->offset = 4;
	field->sizem1 = 3; /* 4 bytes */
	field->alg = NIX_LSOALG_ADD_OFFSET;
	field++;

	/* TCP flags field */
	field->layer = NIX_TXLAYER_IL4;
	field->offset = 12;
	field->sizem1 = 1;
	field->alg = NIX_LSOALG_TCP_FLAGS;
	field++;
}

static void
nix_lso_tun_tcp(struct nix_lso_format_cfg *req, bool outer_v4, bool inner_v4)
{
	__io struct nix_lso_format *field;

	field = (__io struct nix_lso_format *)&req->fields[0];
	req->field_mask = NIX_LSO_FIELD_MASK;
	/* Outer IPv4/IPv6 len */
	field->layer = NIX_TXLAYER_OL3;
	field->offset = outer_v4 ? 2 : 4;
	field->sizem1 = 1; /* 2B */
	field->alg = NIX_LSOALG_ADD_PAYLEN;
	field++;
	if (outer_v4) {
		/* IPID */
		field->layer = NIX_TXLAYER_OL3;
		field->offset = 4;
		field->sizem1 = 1;
		/* Incremented linearly per segment */
		field->alg = NIX_LSOALG_ADD_SEGNUM;
		field++;
	}

	/* Inner IPv4/IPv6 */
	field->layer = NIX_TXLAYER_IL3;
	field->offset = inner_v4 ? 2 : 4;
	field->sizem1 = 1; /* 2B */
	field->alg = NIX_LSOALG_ADD_PAYLEN;
	field++;
	if (inner_v4) {
		/* IPID field */
		field->layer = NIX_TXLAYER_IL3;
		field->offset = 4;
		field->sizem1 = 1;
		/* Incremented linearly per segment */
		field->alg = NIX_LSOALG_ADD_SEGNUM;
		field++;
	}

	/* TCP sequence number update */
	field->layer = NIX_TXLAYER_IL4;
	field->offset = 4;
	field->sizem1 = 3; /* 4 bytes */
	field->alg = NIX_LSOALG_ADD_OFFSET;
	field++;

	/* TCP flags field */
	field->layer = NIX_TXLAYER_IL4;
	field->offset = 12;
	field->sizem1 = 1;
	field->alg = NIX_LSOALG_TCP_FLAGS;
	field++;
}

int
roc_nix_lso_alt_flags_profile_setup(struct roc_nix *roc_nix, nix_lso_alt_flg_format_t *fmt)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct nix_lso_alt_flags_cfg_rsp *rsp;
	struct nix_lso_alt_flags_cfg_req *req;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_nix_lso_alt_flags_cfg(mbox);
	if (req == NULL)
		goto exit;

	req->cfg = fmt->u[0];
	req->cfg1 = fmt->u[1];

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	plt_nix_dbg("Setup alt flags format %u", rsp->lso_alt_flags_idx);
	rc = rsp->lso_alt_flags_idx;
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_lso_custom_fmt_setup(struct roc_nix *roc_nix,
			     struct nix_lso_format *fields, uint16_t nb_fields)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct nix_lso_format_cfg_rsp *rsp;
	struct nix_lso_format_cfg *req;
	int rc = -ENOSPC;

	if (nb_fields > NIX_LSO_FIELD_MAX) {
		rc = -EINVAL;
		goto exit;
	}

	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL)
		goto exit;

	req->field_mask = NIX_LSO_FIELD_MASK;
	mbox_memcpy(req->fields, fields,
		    sizeof(struct nix_lso_format) * nb_fields);

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	plt_nix_dbg("Setup custom format %u", rsp->lso_format_idx);
	rc = rsp->lso_format_idx;
exit:
	mbox_put(mbox);
	return rc;
}

static int
nix_lso_ipv4(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_lso_format_cfg_rsp *rsp;
	nix_lso_alt_flg_format_t alt_flags;

	__io struct nix_lso_format *field;
	struct nix_lso_format_cfg *req;
	int flag_idx = 0, rc = -ENOSPC;
	struct dev *dev = &nix->dev;
	struct mbox *mbox;

	/* First get flags profile to update v4 flags */
	memset(&alt_flags, 0, sizeof(alt_flags));
	alt_flags.s.alt_fsf_set = 0x2000;
	alt_flags.s.alt_fsf_mask = 0x1FFF;
	alt_flags.s.alt_msf_set = 0x2000;
	alt_flags.s.alt_msf_mask = 0x1FFF;
	alt_flags.s.alt_lsf_set = 0x0000;
	alt_flags.s.alt_lsf_mask = 0x1FFF;
	flag_idx = roc_nix_lso_alt_flags_profile_setup(roc_nix, &alt_flags);
	if (flag_idx < 0)
		return rc;

	mbox = mbox_get(dev->mbox);

	/*
	 * IPv4 Fragmentation
	 */
	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	/* Format works only with TCP packet marked by OL3/OL4 */
	field = (__io struct nix_lso_format *)&req->fields[0];
	req->field_mask = NIX_LSO_FIELD_MASK;
	/* Update Payload Length */
	field->layer = NIX_TXLAYER_OL3;
	field->offset = 2;
	field->sizem1 = 1; /* 2B */
	field->alg = NIX_LSOALG_ADD_PAYLEN;
	field++;

	/* Update fragment offset and flags */
	field->layer = NIX_TXLAYER_OL3;
	field->offset = 6;
	field->sizem1 = 1;
	field->shift = NIX_LSO_FRMT_IPV4_OFFSET_SHFT;
	field->alt_flags_index = flag_idx;
	field->alt_flags = 1;
	/* Cumulative length of previous segments */
	field->alg = NIX_LSOALG_ADD_OFFSET;
	field++;
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	/* IPv4 fragment offset shifted by 3 bits, store this value in profile ID */
	nix->lso_ipv4_idx = (NIX_LSO_FRMT_IPV4_OFFSET_SHFT << 8) | (rsp->lso_format_idx & 0x1F);
	plt_nix_dbg("ipv4 fmt=%u", rsp->lso_format_idx);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_lso_fmt_setup(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct nix_lso_format_cfg_rsp *rsp;
	struct nix_lso_format_cfg *req;
	int rc = -ENOSPC;

	/*
	 * IPv4/TCP LSO
	 */
	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL)
		goto exit;
	nix_lso_tcp(req, true);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	if (rsp->lso_format_idx != NIX_LSO_FORMAT_IDX_TSOV4) {
		rc = NIX_ERR_INTERNAL;
		goto exit;
	}

	plt_nix_dbg("tcpv4 lso fmt=%u", rsp->lso_format_idx);

	/*
	 * IPv6/TCP LSO
	 */
	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL) {
		rc =  -ENOSPC;
		goto exit;
	}
	nix_lso_tcp(req, false);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	if (rsp->lso_format_idx != NIX_LSO_FORMAT_IDX_TSOV6) {
		rc = NIX_ERR_INTERNAL;
		goto exit;
	}

	plt_nix_dbg("tcpv6 lso fmt=%u", rsp->lso_format_idx);

	/*
	 * IPv4/UDP/TUN HDR/IPv4/TCP LSO
	 */
	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL) {
		rc =  -ENOSPC;
		goto exit;
	}
	nix_lso_udp_tun_tcp(req, true, true);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	nix->lso_udp_tun_idx[ROC_NIX_LSO_TUN_V4V4] = rsp->lso_format_idx;
	plt_nix_dbg("udp tun v4v4 fmt=%u", rsp->lso_format_idx);

	/*
	 * IPv4/UDP/TUN HDR/IPv6/TCP LSO
	 */
	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL) {
		rc =  -ENOSPC;
		goto exit;
	}
	nix_lso_udp_tun_tcp(req, true, false);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	nix->lso_udp_tun_idx[ROC_NIX_LSO_TUN_V4V6] = rsp->lso_format_idx;
	plt_nix_dbg("udp tun v4v6 fmt=%u", rsp->lso_format_idx);

	/*
	 * IPv6/UDP/TUN HDR/IPv4/TCP LSO
	 */
	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL) {
		rc =  -ENOSPC;
		goto exit;
	}
	nix_lso_udp_tun_tcp(req, false, true);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	nix->lso_udp_tun_idx[ROC_NIX_LSO_TUN_V6V4] = rsp->lso_format_idx;
	plt_nix_dbg("udp tun v6v4 fmt=%u", rsp->lso_format_idx);

	/*
	 * IPv6/UDP/TUN HDR/IPv6/TCP LSO
	 */
	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL) {
		rc =  -ENOSPC;
		goto exit;
	}
	nix_lso_udp_tun_tcp(req, false, false);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	nix->lso_udp_tun_idx[ROC_NIX_LSO_TUN_V6V6] = rsp->lso_format_idx;
	plt_nix_dbg("udp tun v6v6 fmt=%u", rsp->lso_format_idx);

	/*
	 * IPv4/TUN HDR/IPv4/TCP LSO
	 */
	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL) {
		rc =  -ENOSPC;
		goto exit;
	}
	nix_lso_tun_tcp(req, true, true);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	nix->lso_tun_idx[ROC_NIX_LSO_TUN_V4V4] = rsp->lso_format_idx;
	plt_nix_dbg("tun v4v4 fmt=%u", rsp->lso_format_idx);

	/*
	 * IPv4/TUN HDR/IPv6/TCP LSO
	 */
	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL) {
		rc =  -ENOSPC;
		goto exit;
	}
	nix_lso_tun_tcp(req, true, false);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	nix->lso_tun_idx[ROC_NIX_LSO_TUN_V4V6] = rsp->lso_format_idx;
	plt_nix_dbg("tun v4v6 fmt=%u", rsp->lso_format_idx);

	/*
	 * IPv6/TUN HDR/IPv4/TCP LSO
	 */
	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL) {
		rc =  -ENOSPC;
		goto exit;
	}
	nix_lso_tun_tcp(req, false, true);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	nix->lso_tun_idx[ROC_NIX_LSO_TUN_V6V4] = rsp->lso_format_idx;
	plt_nix_dbg("tun v6v4 fmt=%u", rsp->lso_format_idx);

	/*
	 * IPv6/TUN HDR/IPv6/TCP LSO
	 */
	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL) {
		rc =  -ENOSPC;
		goto exit;
	}

	nix_lso_tun_tcp(req, false, false);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	nix->lso_tun_idx[ROC_NIX_LSO_TUN_V6V6] = rsp->lso_format_idx;
	plt_nix_dbg("tun v6v6 fmt=%u", rsp->lso_format_idx);

exit:
	mbox_put(mbox);

	nix->lso_ipv4_idx = 0; /* IPv4 fragmentation not supported */
	if (!rc && roc_model_is_cn20k())
		return nix_lso_ipv4(roc_nix);

	return rc;
}

int
roc_nix_lso_fmt_ipv4_frag_get(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	return nix->lso_ipv4_idx;
}

int
roc_nix_lso_fmt_get(struct roc_nix *roc_nix,
		    uint8_t udp_tun[ROC_NIX_LSO_TUN_MAX],
		    uint8_t tun[ROC_NIX_LSO_TUN_MAX])
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	memcpy(udp_tun, nix->lso_udp_tun_idx, ROC_NIX_LSO_TUN_MAX);
	memcpy(tun, nix->lso_tun_idx, ROC_NIX_LSO_TUN_MAX);
	return 0;
}

int
roc_nix_switch_hdr_set(struct roc_nix *roc_nix, uint64_t switch_header_type,
		       uint8_t pre_l2_size_offset,
		       uint8_t pre_l2_size_offset_mask,
		       uint8_t pre_l2_size_shift_dir)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct npc_set_pkind *req;
	struct msg_resp *rsp;
	int rc = -ENOSPC;

	if (switch_header_type == 0)
		switch_header_type = ROC_PRIV_FLAGS_DEFAULT;

	if (switch_header_type != ROC_PRIV_FLAGS_DEFAULT &&
	    switch_header_type != ROC_PRIV_FLAGS_EDSA &&
	    switch_header_type != ROC_PRIV_FLAGS_HIGIG &&
	    switch_header_type != ROC_PRIV_FLAGS_LEN_90B &&
	    switch_header_type != ROC_PRIV_FLAGS_EXDSA &&
	    switch_header_type != ROC_PRIV_FLAGS_VLAN_EXDSA &&
	    switch_header_type != ROC_PRIV_FLAGS_PRE_L2 &&
	    switch_header_type != ROC_PRIV_FLAGS_CUSTOM) {
		plt_err("switch header type is not supported");
		rc = NIX_ERR_PARAM;
		goto exit;
	}

	if (switch_header_type == ROC_PRIV_FLAGS_LEN_90B &&
	    !roc_nix_is_sdp(roc_nix)) {
		plt_err("chlen90b is not supported on non-SDP device");
		rc = NIX_ERR_PARAM;
		goto exit;
	}

	if (switch_header_type == ROC_PRIV_FLAGS_HIGIG &&
	    roc_nix_is_vf_or_sdp(roc_nix)) {
		plt_err("higig2 is supported on PF devices only");
		rc = NIX_ERR_PARAM;
		goto exit;
	}

	req = mbox_alloc_msg_npc_set_pkind(mbox);
	if (req == NULL)
		goto exit;
	req->mode = switch_header_type;

	if (switch_header_type == ROC_PRIV_FLAGS_LEN_90B) {
		req->mode = ROC_PRIV_FLAGS_CUSTOM;
		req->pkind = NPC_RX_CHLEN90B_PKIND;
	} else if (switch_header_type == ROC_PRIV_FLAGS_EXDSA) {
		req->mode = ROC_PRIV_FLAGS_CUSTOM;
		req->pkind = NPC_RX_EXDSA_PKIND;
	} else if (switch_header_type == ROC_PRIV_FLAGS_VLAN_EXDSA) {
		req->mode = ROC_PRIV_FLAGS_CUSTOM;
		req->pkind = NPC_RX_VLAN_EXDSA_PKIND;
	} else if (switch_header_type == ROC_PRIV_FLAGS_PRE_L2) {
		req->mode = ROC_PRIV_FLAGS_CUSTOM;
		req->pkind = NPC_RX_CUSTOM_PRE_L2_PKIND;
		req->var_len_off = pre_l2_size_offset;
		req->var_len_off_mask = pre_l2_size_offset_mask;
		req->shift_dir = pre_l2_size_shift_dir;
	}

	req->dir = PKIND_RX;
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	req = mbox_alloc_msg_npc_set_pkind(mbox);
	if (req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}
	req->mode = switch_header_type;
	req->dir = PKIND_TX;
	rc = mbox_process_msg(mbox, (void *)&rsp);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_eeprom_info_get(struct roc_nix *roc_nix,
			struct roc_nix_eeprom_info *info)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct cgx_fw_data *rsp = NULL;
	int rc;

	if (!info) {
		plt_err("Input buffer is NULL");
		rc = NIX_ERR_PARAM;
		goto exit;
	}

	mbox_alloc_msg_cgx_get_aux_link_info(mbox);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc) {
		plt_err("Failed to get fw data: %d", rc);
		goto exit;
	}

	info->sff_id = rsp->fwdata.sfp_eeprom.sff_id;
	mbox_memcpy(info->buf, rsp->fwdata.sfp_eeprom.buf, SFP_EEPROM_SIZE);
	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_rx_drop_re_set(struct roc_nix *roc_nix, bool ena)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct nix_rx_cfg *req;
	int rc = -EIO;

	/* No-op if no change */
	if (ena == !!(nix->rx_cfg & ROC_NIX_LF_RX_CFG_DROP_RE)) {
		rc = 0;
		goto exit;
	}

	req = mbox_alloc_msg_nix_set_rx_cfg(mbox);
	if (req == NULL)
		goto exit;

	if (ena)
		req->len_verify |= NIX_RX_DROP_RE;
	/* Keep other flags intact */
	if (nix->rx_cfg & ROC_NIX_LF_RX_CFG_LEN_OL3)
		req->len_verify |= NIX_RX_OL3_VERIFY;

	if (nix->rx_cfg & ROC_NIX_LF_RX_CFG_LEN_OL4)
		req->len_verify |= NIX_RX_OL4_VERIFY;

	if (nix->rx_cfg & ROC_NIX_LF_RX_CFG_CSUM_OL4)
		req->csum_verify |= NIX_RX_CSUM_OL4_VERIFY;

	rc = mbox_process(mbox);
	if (rc)
		goto exit;

	if (ena)
		nix->rx_cfg |= ROC_NIX_LF_RX_CFG_DROP_RE;
	else
		nix->rx_cfg &= ~ROC_NIX_LF_RX_CFG_DROP_RE;
	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}
