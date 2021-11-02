/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

static inline struct mbox *
get_mbox(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;

	return dev->mbox;
}

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
roc_nix_lso_custom_fmt_setup(struct roc_nix *roc_nix,
			     struct nix_lso_format *fields, uint16_t nb_fields)
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_lso_format_cfg_rsp *rsp;
	struct nix_lso_format_cfg *req;
	int rc = -ENOSPC;

	if (nb_fields > NIX_LSO_FIELD_MAX)
		return -EINVAL;

	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL)
		return rc;

	req->field_mask = NIX_LSO_FIELD_MASK;
	mbox_memcpy(req->fields, fields,
		    sizeof(struct nix_lso_format) * nb_fields);

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	plt_nix_dbg("Setup custom format %u", rsp->lso_format_idx);
	return rsp->lso_format_idx;
}

int
roc_nix_lso_fmt_setup(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_lso_format_cfg_rsp *rsp;
	struct nix_lso_format_cfg *req;
	int rc = -ENOSPC;

	/*
	 * IPv4/TCP LSO
	 */
	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL)
		return rc;
	nix_lso_tcp(req, true);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	if (rsp->lso_format_idx != NIX_LSO_FORMAT_IDX_TSOV4)
		return NIX_ERR_INTERNAL;

	plt_nix_dbg("tcpv4 lso fmt=%u\n", rsp->lso_format_idx);

	/*
	 * IPv6/TCP LSO
	 */
	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL)
		return -ENOSPC;
	nix_lso_tcp(req, false);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	if (rsp->lso_format_idx != NIX_LSO_FORMAT_IDX_TSOV6)
		return NIX_ERR_INTERNAL;

	plt_nix_dbg("tcpv6 lso fmt=%u\n", rsp->lso_format_idx);

	/*
	 * IPv4/UDP/TUN HDR/IPv4/TCP LSO
	 */
	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL)
		return -ENOSPC;
	nix_lso_udp_tun_tcp(req, true, true);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	nix->lso_udp_tun_idx[ROC_NIX_LSO_TUN_V4V4] = rsp->lso_format_idx;
	plt_nix_dbg("udp tun v4v4 fmt=%u\n", rsp->lso_format_idx);

	/*
	 * IPv4/UDP/TUN HDR/IPv6/TCP LSO
	 */
	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL)
		return -ENOSPC;
	nix_lso_udp_tun_tcp(req, true, false);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	nix->lso_udp_tun_idx[ROC_NIX_LSO_TUN_V4V6] = rsp->lso_format_idx;
	plt_nix_dbg("udp tun v4v6 fmt=%u\n", rsp->lso_format_idx);

	/*
	 * IPv6/UDP/TUN HDR/IPv4/TCP LSO
	 */
	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL)
		return -ENOSPC;
	nix_lso_udp_tun_tcp(req, false, true);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	nix->lso_udp_tun_idx[ROC_NIX_LSO_TUN_V6V4] = rsp->lso_format_idx;
	plt_nix_dbg("udp tun v6v4 fmt=%u\n", rsp->lso_format_idx);

	/*
	 * IPv6/UDP/TUN HDR/IPv6/TCP LSO
	 */
	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL)
		return -ENOSPC;
	nix_lso_udp_tun_tcp(req, false, false);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	nix->lso_udp_tun_idx[ROC_NIX_LSO_TUN_V6V6] = rsp->lso_format_idx;
	plt_nix_dbg("udp tun v6v6 fmt=%u\n", rsp->lso_format_idx);

	/*
	 * IPv4/TUN HDR/IPv4/TCP LSO
	 */
	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL)
		return -ENOSPC;
	nix_lso_tun_tcp(req, true, true);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	nix->lso_tun_idx[ROC_NIX_LSO_TUN_V4V4] = rsp->lso_format_idx;
	plt_nix_dbg("tun v4v4 fmt=%u\n", rsp->lso_format_idx);

	/*
	 * IPv4/TUN HDR/IPv6/TCP LSO
	 */
	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL)
		return -ENOSPC;
	nix_lso_tun_tcp(req, true, false);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	nix->lso_tun_idx[ROC_NIX_LSO_TUN_V4V6] = rsp->lso_format_idx;
	plt_nix_dbg("tun v4v6 fmt=%u\n", rsp->lso_format_idx);

	/*
	 * IPv6/TUN HDR/IPv4/TCP LSO
	 */
	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL)
		return -ENOSPC;
	nix_lso_tun_tcp(req, false, true);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	nix->lso_tun_idx[ROC_NIX_LSO_TUN_V6V4] = rsp->lso_format_idx;
	plt_nix_dbg("tun v6v4 fmt=%u\n", rsp->lso_format_idx);

	/*
	 * IPv6/TUN HDR/IPv6/TCP LSO
	 */
	req = mbox_alloc_msg_nix_lso_format_cfg(mbox);
	if (req == NULL)
		return -ENOSPC;
	nix_lso_tun_tcp(req, false, false);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	nix->lso_tun_idx[ROC_NIX_LSO_TUN_V6V6] = rsp->lso_format_idx;
	plt_nix_dbg("tun v6v6 fmt=%u\n", rsp->lso_format_idx);
	return 0;
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
roc_nix_switch_hdr_set(struct roc_nix *roc_nix, uint64_t switch_header_type)
{
	struct mbox *mbox = get_mbox(roc_nix);
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
	    switch_header_type != ROC_PRIV_FLAGS_CUSTOM) {
		plt_err("switch header type is not supported");
		return NIX_ERR_PARAM;
	}

	if (switch_header_type == ROC_PRIV_FLAGS_LEN_90B &&
	    !roc_nix_is_sdp(roc_nix)) {
		plt_err("chlen90b is not supported on non-SDP device");
		return NIX_ERR_PARAM;
	}

	if (switch_header_type == ROC_PRIV_FLAGS_HIGIG &&
	    roc_nix_is_vf_or_sdp(roc_nix)) {
		plt_err("higig2 is supported on PF devices only");
		return NIX_ERR_PARAM;
	}

	req = mbox_alloc_msg_npc_set_pkind(mbox);
	if (req == NULL)
		return rc;
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
	}

	req->dir = PKIND_RX;
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	req = mbox_alloc_msg_npc_set_pkind(mbox);
	if (req == NULL)
		return -ENOSPC;
	req->mode = switch_header_type;
	req->dir = PKIND_TX;
	return mbox_process_msg(mbox, (void *)&rsp);
}

int
roc_nix_eeprom_info_get(struct roc_nix *roc_nix,
			struct roc_nix_eeprom_info *info)
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct cgx_fw_data *rsp = NULL;
	int rc;

	if (!info) {
		plt_err("Input buffer is NULL");
		return NIX_ERR_PARAM;
	}

	mbox_alloc_msg_cgx_get_aux_link_info(mbox);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc) {
		plt_err("Failed to get fw data: %d", rc);
		return rc;
	}

	info->sff_id = rsp->fwdata.sfp_eeprom.sff_id;
	mbox_memcpy(info->buf, rsp->fwdata.sfp_eeprom.buf, SFP_EEPROM_SIZE);
	return 0;
}

int
roc_nix_rx_drop_re_set(struct roc_nix *roc_nix, bool ena)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_rx_cfg *req;
	int rc = -EIO;

	/* No-op if no change */
	if (ena == !!(nix->rx_cfg & ROC_NIX_LF_RX_CFG_DROP_RE))
		return 0;

	req = mbox_alloc_msg_nix_set_rx_cfg(mbox);
	if (req == NULL)
		return rc;

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
		return rc;

	if (ena)
		nix->rx_cfg |= ROC_NIX_LF_RX_CFG_DROP_RE;
	else
		nix->rx_cfg &= ~ROC_NIX_LF_RX_CFG_DROP_RE;
	return 0;
}
