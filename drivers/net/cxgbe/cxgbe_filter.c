/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */
#include <rte_net.h>

#include "base/common.h"
#include "base/t4_tcb.h"
#include "base/t4_regs.h"
#include "cxgbe_filter.h"
#include "clip_tbl.h"
#include "l2t.h"

/**
 * Initialize Hash Filters
 */
int init_hash_filter(struct adapter *adap)
{
	unsigned int n_user_filters;
	unsigned int user_filter_perc;
	int ret;
	u32 params[7], val[7];

#define FW_PARAM_DEV(param) \
	(V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) | \
	V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_##param))

#define FW_PARAM_PFVF(param) \
	(V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_PFVF) | \
	V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_PFVF_##param) |  \
	V_FW_PARAMS_PARAM_Y(0) | \
	V_FW_PARAMS_PARAM_Z(0))

	params[0] = FW_PARAM_DEV(NTID);
	ret = t4_query_params(adap, adap->mbox, adap->pf, 0, 1,
			      params, val);
	if (ret < 0)
		return ret;
	adap->tids.ntids = val[0];
	adap->tids.natids = min(adap->tids.ntids / 2, MAX_ATIDS);

	user_filter_perc = 100;
	n_user_filters = mult_frac(adap->tids.nftids,
				   user_filter_perc,
				   100);

	adap->tids.nftids = n_user_filters;
	adap->params.hash_filter = 1;
	return 0;
}

/**
 * Validate if the requested filter specification can be set by checking
 * if the requested features have been enabled
 */
int validate_filter(struct adapter *adapter, struct ch_filter_specification *fs)
{
	u32 fconf;

	/*
	 * Check for unconfigured fields being used.
	 */
	fconf = adapter->params.tp.vlan_pri_map;

#define S(_field) \
	(fs->val._field || fs->mask._field)
#define U(_mask, _field) \
	(!(fconf & (_mask)) && S(_field))

	if (U(F_PORT, iport) || U(F_ETHERTYPE, ethtype) ||
	    U(F_PROTOCOL, proto) || U(F_MACMATCH, macidx))
		return -EOPNOTSUPP;

#undef S
#undef U

	/*
	 * If the user is requesting that the filter action loop
	 * matching packets back out one of our ports, make sure that
	 * the egress port is in range.
	 */
	if (fs->action == FILTER_SWITCH &&
	    fs->eport >= adapter->params.nports)
		return -ERANGE;

	/*
	 * Don't allow various trivially obvious bogus out-of-range
	 * values ...
	 */
	if (fs->val.iport >= adapter->params.nports)
		return -ERANGE;

	if (!fs->cap && fs->nat_mode && !adapter->params.filter2_wr_support)
		return -EOPNOTSUPP;

	if (!fs->cap && fs->swapmac && !adapter->params.filter2_wr_support)
		return -EOPNOTSUPP;

	return 0;
}

/**
 * Get the queue to which the traffic must be steered to.
 */
static unsigned int get_filter_steerq(struct rte_eth_dev *dev,
				      struct ch_filter_specification *fs)
{
	struct port_info *pi = ethdev2pinfo(dev);
	struct adapter *adapter = pi->adapter;
	unsigned int iq;

	/*
	 * If the user has requested steering matching Ingress Packets
	 * to a specific Queue Set, we need to make sure it's in range
	 * for the port and map that into the Absolute Queue ID of the
	 * Queue Set's Response Queue.
	 */
	if (!fs->dirsteer) {
		iq = 0;
	} else {
		/*
		 * If the iq id is greater than the number of qsets,
		 * then assume it is an absolute qid.
		 */
		if (fs->iq < pi->n_rx_qsets)
			iq = adapter->sge.ethrxq[pi->first_qset +
						 fs->iq].rspq.abs_id;
		else
			iq = fs->iq;
	}

	return iq;
}

/* Return an error number if the indicated filter isn't writable ... */
int writable_filter(struct filter_entry *f)
{
	if (f->locked)
		return -EPERM;
	if (f->pending)
		return -EBUSY;

	return 0;
}

/**
 * Send CPL_SET_TCB_FIELD message
 */
static void set_tcb_field(struct adapter *adapter, unsigned int ftid,
			  u16 word, u64 mask, u64 val, int no_reply)
{
	struct rte_mbuf *mbuf;
	struct cpl_set_tcb_field *req;
	struct sge_ctrl_txq *ctrlq;

	ctrlq = &adapter->sge.ctrlq[0];
	mbuf = rte_pktmbuf_alloc(ctrlq->mb_pool);
	WARN_ON(!mbuf);

	mbuf->data_len = sizeof(*req);
	mbuf->pkt_len = mbuf->data_len;

	req = rte_pktmbuf_mtod(mbuf, struct cpl_set_tcb_field *);
	memset(req, 0, sizeof(*req));
	INIT_TP_WR_MIT_CPL(req, CPL_SET_TCB_FIELD, ftid);
	req->reply_ctrl = cpu_to_be16(V_REPLY_CHAN(0) |
				      V_QUEUENO(adapter->sge.fw_evtq.abs_id) |
				      V_NO_REPLY(no_reply));
	req->word_cookie = cpu_to_be16(V_WORD(word) | V_COOKIE(ftid));
	req->mask = cpu_to_be64(mask);
	req->val = cpu_to_be64(val);

	t4_mgmt_tx(ctrlq, mbuf);
}

/**
 * Set one of the t_flags bits in the TCB.
 */
static void set_tcb_tflag(struct adapter *adap, unsigned int ftid,
			  unsigned int bit_pos, unsigned int val, int no_reply)
{
	set_tcb_field(adap, ftid,  W_TCB_T_FLAGS, 1ULL << bit_pos,
		      (unsigned long long)val << bit_pos, no_reply);
}

/**
 * Build a CPL_SET_TCB_FIELD message as payload of a ULP_TX_PKT command.
 */
static inline void mk_set_tcb_field_ulp(struct filter_entry *f,
					struct cpl_set_tcb_field *req,
					unsigned int word,
					u64 mask, u64 val, u8 cookie,
					int no_reply)
{
	struct ulp_txpkt *txpkt = (struct ulp_txpkt *)req;
	struct ulptx_idata *sc = (struct ulptx_idata *)(txpkt + 1);

	txpkt->cmd_dest = cpu_to_be32(V_ULPTX_CMD(ULP_TX_PKT) |
				      V_ULP_TXPKT_DEST(0));
	txpkt->len = cpu_to_be32(DIV_ROUND_UP(sizeof(*req), 16));
	sc->cmd_more = cpu_to_be32(V_ULPTX_CMD(ULP_TX_SC_IMM));
	sc->len = cpu_to_be32(sizeof(*req) - sizeof(struct work_request_hdr));
	OPCODE_TID(req) = cpu_to_be32(MK_OPCODE_TID(CPL_SET_TCB_FIELD, f->tid));
	req->reply_ctrl = cpu_to_be16(V_NO_REPLY(no_reply) | V_REPLY_CHAN(0) |
				      V_QUEUENO(0));
	req->word_cookie = cpu_to_be16(V_WORD(word) | V_COOKIE(cookie));
	req->mask = cpu_to_be64(mask);
	req->val = cpu_to_be64(val);
	sc = (struct ulptx_idata *)(req + 1);
	sc->cmd_more = cpu_to_be32(V_ULPTX_CMD(ULP_TX_SC_NOOP));
	sc->len = cpu_to_be32(0);
}

/**
 * Check if entry already filled.
 */
bool is_filter_set(struct tid_info *t, int fidx, int family)
{
	bool result = FALSE;
	int i, max;

	/* IPv6 requires four slots and IPv4 requires only 1 slot.
	 * Ensure, there's enough slots available.
	 */
	max = family == FILTER_TYPE_IPV6 ? fidx + 3 : fidx;

	t4_os_lock(&t->ftid_lock);
	for (i = fidx; i <= max; i++) {
		if (rte_bitmap_get(t->ftid_bmap, i)) {
			result = TRUE;
			break;
		}
	}
	t4_os_unlock(&t->ftid_lock);
	return result;
}

/**
 * Allocate a available free entry
 */
int cxgbe_alloc_ftid(struct adapter *adap, unsigned int family)
{
	struct tid_info *t = &adap->tids;
	int pos;
	int size = t->nftids;

	t4_os_lock(&t->ftid_lock);
	if (family == FILTER_TYPE_IPV6)
		pos = cxgbe_bitmap_find_free_region(t->ftid_bmap, size, 4);
	else
		pos = cxgbe_find_first_zero_bit(t->ftid_bmap, size);
	t4_os_unlock(&t->ftid_lock);

	return pos < size ? pos : -1;
}

/**
 * Construct hash filter ntuple.
 */
static u64 hash_filter_ntuple(const struct filter_entry *f)
{
	struct adapter *adap = ethdev2adap(f->dev);
	struct tp_params *tp = &adap->params.tp;
	u64 ntuple = 0;
	u16 tcp_proto = IPPROTO_TCP; /* TCP Protocol Number */

	if (tp->port_shift >= 0 && f->fs.mask.iport)
		ntuple |= (u64)f->fs.val.iport << tp->port_shift;

	if (tp->protocol_shift >= 0) {
		if (!f->fs.val.proto)
			ntuple |= (u64)tcp_proto << tp->protocol_shift;
		else
			ntuple |= (u64)f->fs.val.proto << tp->protocol_shift;
	}

	if (tp->ethertype_shift >= 0 && f->fs.mask.ethtype)
		ntuple |= (u64)(f->fs.val.ethtype) << tp->ethertype_shift;
	if (tp->macmatch_shift >= 0 && f->fs.mask.macidx)
		ntuple |= (u64)(f->fs.val.macidx) << tp->macmatch_shift;

	return ntuple;
}

/**
 * Build a CPL_ABORT_REQ message as payload of a ULP_TX_PKT command.
 */
static void mk_abort_req_ulp(struct cpl_abort_req *abort_req,
			     unsigned int tid)
{
	struct ulp_txpkt *txpkt = (struct ulp_txpkt *)abort_req;
	struct ulptx_idata *sc = (struct ulptx_idata *)(txpkt + 1);

	txpkt->cmd_dest = cpu_to_be32(V_ULPTX_CMD(ULP_TX_PKT) |
				      V_ULP_TXPKT_DEST(0));
	txpkt->len = cpu_to_be32(DIV_ROUND_UP(sizeof(*abort_req), 16));
	sc->cmd_more = cpu_to_be32(V_ULPTX_CMD(ULP_TX_SC_IMM));
	sc->len = cpu_to_be32(sizeof(*abort_req) -
			      sizeof(struct work_request_hdr));
	OPCODE_TID(abort_req) = cpu_to_be32(MK_OPCODE_TID(CPL_ABORT_REQ, tid));
	abort_req->rsvd0 = cpu_to_be32(0);
	abort_req->rsvd1 = 0;
	abort_req->cmd = CPL_ABORT_NO_RST;
	sc = (struct ulptx_idata *)(abort_req + 1);
	sc->cmd_more = cpu_to_be32(V_ULPTX_CMD(ULP_TX_SC_NOOP));
	sc->len = cpu_to_be32(0);
}

/**
 * Build a CPL_ABORT_RPL message as payload of a ULP_TX_PKT command.
 */
static void mk_abort_rpl_ulp(struct cpl_abort_rpl *abort_rpl,
			     unsigned int tid)
{
	struct ulp_txpkt *txpkt = (struct ulp_txpkt *)abort_rpl;
	struct ulptx_idata *sc = (struct ulptx_idata *)(txpkt + 1);

	txpkt->cmd_dest = cpu_to_be32(V_ULPTX_CMD(ULP_TX_PKT) |
				      V_ULP_TXPKT_DEST(0));
	txpkt->len = cpu_to_be32(DIV_ROUND_UP(sizeof(*abort_rpl), 16));
	sc->cmd_more = cpu_to_be32(V_ULPTX_CMD(ULP_TX_SC_IMM));
	sc->len = cpu_to_be32(sizeof(*abort_rpl) -
			      sizeof(struct work_request_hdr));
	OPCODE_TID(abort_rpl) = cpu_to_be32(MK_OPCODE_TID(CPL_ABORT_RPL, tid));
	abort_rpl->rsvd0 = cpu_to_be32(0);
	abort_rpl->rsvd1 = 0;
	abort_rpl->cmd = CPL_ABORT_NO_RST;
	sc = (struct ulptx_idata *)(abort_rpl + 1);
	sc->cmd_more = cpu_to_be32(V_ULPTX_CMD(ULP_TX_SC_NOOP));
	sc->len = cpu_to_be32(0);
}

/**
 * Delete the specified hash filter.
 */
static int cxgbe_del_hash_filter(struct rte_eth_dev *dev,
				 unsigned int filter_id,
				 struct filter_ctx *ctx)
{
	struct adapter *adapter = ethdev2adap(dev);
	struct tid_info *t = &adapter->tids;
	struct filter_entry *f;
	struct sge_ctrl_txq *ctrlq;
	unsigned int port_id = ethdev2pinfo(dev)->port_id;
	int ret;

	if (filter_id > adapter->tids.ntids)
		return -E2BIG;

	f = lookup_tid(t, filter_id);
	if (!f) {
		dev_err(adapter, "%s: no filter entry for filter_id = %d\n",
			__func__, filter_id);
		return -EINVAL;
	}

	ret = writable_filter(f);
	if (ret)
		return ret;

	if (f->valid) {
		unsigned int wrlen;
		struct rte_mbuf *mbuf;
		struct work_request_hdr *wr;
		struct ulptx_idata *aligner;
		struct cpl_set_tcb_field *req;
		struct cpl_abort_req *abort_req;
		struct cpl_abort_rpl *abort_rpl;

		f->ctx = ctx;
		f->pending = 1;

		wrlen = cxgbe_roundup(sizeof(*wr) +
				      (sizeof(*req) + sizeof(*aligner)) +
				      sizeof(*abort_req) + sizeof(*abort_rpl),
				      16);

		ctrlq = &adapter->sge.ctrlq[port_id];
		mbuf = rte_pktmbuf_alloc(ctrlq->mb_pool);
		if (!mbuf) {
			dev_err(adapter, "%s: could not allocate skb ..\n",
				__func__);
			goto out_err;
		}

		mbuf->data_len = wrlen;
		mbuf->pkt_len = mbuf->data_len;

		req = rte_pktmbuf_mtod(mbuf, struct cpl_set_tcb_field *);
		INIT_ULPTX_WR(req, wrlen, 0, 0);
		wr = (struct work_request_hdr *)req;
		wr++;
		req = (struct cpl_set_tcb_field *)wr;
		mk_set_tcb_field_ulp(f, req, W_TCB_RSS_INFO,
				V_TCB_RSS_INFO(M_TCB_RSS_INFO),
				V_TCB_RSS_INFO(adapter->sge.fw_evtq.abs_id),
				0, 1);
		aligner = (struct ulptx_idata *)(req + 1);
		abort_req = (struct cpl_abort_req *)(aligner + 1);
		mk_abort_req_ulp(abort_req, f->tid);
		abort_rpl = (struct cpl_abort_rpl *)(abort_req + 1);
		mk_abort_rpl_ulp(abort_rpl, f->tid);
		t4_mgmt_tx(ctrlq, mbuf);
	}
	return 0;

out_err:
	return -ENOMEM;
}

/**
 * Build a ACT_OPEN_REQ6 message for setting IPv6 hash filter.
 */
static void mk_act_open_req6(struct filter_entry *f, struct rte_mbuf *mbuf,
			     unsigned int qid_filterid, struct adapter *adap)
{
	struct cpl_t6_act_open_req6 *req = NULL;
	u64 local_lo, local_hi, peer_lo, peer_hi;
	u32 *lip = (u32 *)f->fs.val.lip;
	u32 *fip = (u32 *)f->fs.val.fip;

	switch (CHELSIO_CHIP_VERSION(adap->params.chip)) {
	case CHELSIO_T6:
		req = rte_pktmbuf_mtod(mbuf, struct cpl_t6_act_open_req6 *);

		INIT_TP_WR(req, 0);
		break;
	default:
		dev_err(adap, "%s: unsupported chip type!\n", __func__);
		return;
	}

	local_hi = ((u64)lip[1]) << 32 | lip[0];
	local_lo = ((u64)lip[3]) << 32 | lip[2];
	peer_hi = ((u64)fip[1]) << 32 | fip[0];
	peer_lo = ((u64)fip[3]) << 32 | fip[2];

	OPCODE_TID(req) = cpu_to_be32(MK_OPCODE_TID(CPL_ACT_OPEN_REQ6,
						    qid_filterid));
	req->local_port = cpu_to_be16(f->fs.val.lport);
	req->peer_port = cpu_to_be16(f->fs.val.fport);
	req->local_ip_hi = local_hi;
	req->local_ip_lo = local_lo;
	req->peer_ip_hi = peer_hi;
	req->peer_ip_lo = peer_lo;
	req->opt0 = cpu_to_be64(V_NAGLE(f->fs.newvlan == VLAN_REMOVE ||
					f->fs.newvlan == VLAN_REWRITE) |
				V_DELACK(f->fs.hitcnts) |
				V_L2T_IDX(f->l2t ? f->l2t->idx : 0) |
				V_SMAC_SEL((cxgbe_port_viid(f->dev) & 0x7F)
					   << 1) |
				V_TX_CHAN(f->fs.eport) |
				V_ULP_MODE(ULP_MODE_NONE) |
				F_TCAM_BYPASS | F_NON_OFFLOAD);
	req->params = cpu_to_be64(V_FILTER_TUPLE(hash_filter_ntuple(f)));
	req->opt2 = cpu_to_be32(F_RSS_QUEUE_VALID |
			    V_RSS_QUEUE(f->fs.iq) |
			    F_T5_OPT_2_VALID |
			    F_RX_CHANNEL |
			    V_SACK_EN(f->fs.swapmac) |
			    V_CONG_CNTRL((f->fs.action == FILTER_DROP) |
					 (f->fs.dirsteer << 1)) |
			    V_CCTRL_ECN(f->fs.action == FILTER_SWITCH));
}

/**
 * Build a ACT_OPEN_REQ message for setting IPv4 hash filter.
 */
static void mk_act_open_req(struct filter_entry *f, struct rte_mbuf *mbuf,
			    unsigned int qid_filterid, struct adapter *adap)
{
	struct cpl_t6_act_open_req *req = NULL;

	switch (CHELSIO_CHIP_VERSION(adap->params.chip)) {
	case CHELSIO_T6:
		req = rte_pktmbuf_mtod(mbuf, struct cpl_t6_act_open_req *);

		INIT_TP_WR(req, 0);
		break;
	default:
		dev_err(adap, "%s: unsupported chip type!\n", __func__);
		return;
	}

	OPCODE_TID(req) = cpu_to_be32(MK_OPCODE_TID(CPL_ACT_OPEN_REQ,
						    qid_filterid));
	req->local_port = cpu_to_be16(f->fs.val.lport);
	req->peer_port = cpu_to_be16(f->fs.val.fport);
	req->local_ip = f->fs.val.lip[0] | f->fs.val.lip[1] << 8 |
			f->fs.val.lip[2] << 16 | f->fs.val.lip[3] << 24;
	req->peer_ip = f->fs.val.fip[0] | f->fs.val.fip[1] << 8 |
			f->fs.val.fip[2] << 16 | f->fs.val.fip[3] << 24;
	req->opt0 = cpu_to_be64(V_NAGLE(f->fs.newvlan == VLAN_REMOVE ||
					f->fs.newvlan == VLAN_REWRITE) |
				V_DELACK(f->fs.hitcnts) |
				V_L2T_IDX(f->l2t ? f->l2t->idx : 0) |
				V_SMAC_SEL((cxgbe_port_viid(f->dev) & 0x7F)
					   << 1) |
				V_TX_CHAN(f->fs.eport) |
				V_ULP_MODE(ULP_MODE_NONE) |
				F_TCAM_BYPASS | F_NON_OFFLOAD);
	req->params = cpu_to_be64(V_FILTER_TUPLE(hash_filter_ntuple(f)));
	req->opt2 = cpu_to_be32(F_RSS_QUEUE_VALID |
			    V_RSS_QUEUE(f->fs.iq) |
			    F_T5_OPT_2_VALID |
			    F_RX_CHANNEL |
			    V_SACK_EN(f->fs.swapmac) |
			    V_CONG_CNTRL((f->fs.action == FILTER_DROP) |
					 (f->fs.dirsteer << 1)) |
			    V_CCTRL_ECN(f->fs.action == FILTER_SWITCH));
}

/**
 * Set the specified hash filter.
 */
static int cxgbe_set_hash_filter(struct rte_eth_dev *dev,
				 struct ch_filter_specification *fs,
				 struct filter_ctx *ctx)
{
	struct port_info *pi = ethdev2pinfo(dev);
	struct adapter *adapter = pi->adapter;
	struct tid_info *t = &adapter->tids;
	struct filter_entry *f;
	struct rte_mbuf *mbuf;
	struct sge_ctrl_txq *ctrlq;
	unsigned int iq;
	int atid, size;
	int ret = 0;

	ret = validate_filter(adapter, fs);
	if (ret)
		return ret;

	iq = get_filter_steerq(dev, fs);

	ctrlq = &adapter->sge.ctrlq[pi->port_id];

	f = t4_os_alloc(sizeof(*f));
	if (!f)
		goto out_err;

	f->fs = *fs;
	f->ctx = ctx;
	f->dev = dev;
	f->fs.iq = iq;

	/*
	 * If the new filter requires loopback Destination MAC and/or VLAN
	 * rewriting then we need to allocate a Layer 2 Table (L2T) entry for
	 * the filter.
	 */
	if (f->fs.newvlan == VLAN_INSERT ||
	    f->fs.newvlan == VLAN_REWRITE) {
		/* allocate L2T entry for new filter */
		f->l2t = cxgbe_l2t_alloc_switching(dev, f->fs.vlan,
						   f->fs.eport, f->fs.dmac);
		if (!f->l2t) {
			ret = -ENOMEM;
			goto out_err;
		}
	}

	atid = cxgbe_alloc_atid(t, f);
	if (atid < 0)
		goto out_err;

	if (f->fs.type) {
		/* IPv6 hash filter */
		f->clipt = cxgbe_clip_alloc(f->dev, (u32 *)&f->fs.val.lip);
		if (!f->clipt)
			goto free_atid;

		size = sizeof(struct cpl_t6_act_open_req6);
		mbuf = rte_pktmbuf_alloc(ctrlq->mb_pool);
		if (!mbuf) {
			ret = -ENOMEM;
			goto free_clip;
		}

		mbuf->data_len = size;
		mbuf->pkt_len = mbuf->data_len;

		mk_act_open_req6(f, mbuf,
				 ((adapter->sge.fw_evtq.abs_id << 14) | atid),
				 adapter);
	} else {
		/* IPv4 hash filter */
		size = sizeof(struct cpl_t6_act_open_req);
		mbuf = rte_pktmbuf_alloc(ctrlq->mb_pool);
		if (!mbuf) {
			ret = -ENOMEM;
			goto free_atid;
		}

		mbuf->data_len = size;
		mbuf->pkt_len = mbuf->data_len;

		mk_act_open_req(f, mbuf,
				((adapter->sge.fw_evtq.abs_id << 14) | atid),
				adapter);
	}

	f->pending = 1;
	t4_mgmt_tx(ctrlq, mbuf);
	return 0;

free_clip:
	cxgbe_clip_release(f->dev, f->clipt);
free_atid:
	cxgbe_free_atid(t, atid);

out_err:
	t4_os_free(f);
	return ret;
}

/**
 * Clear a filter and release any of its resources that we own.  This also
 * clears the filter's "pending" status.
 */
void clear_filter(struct filter_entry *f)
{
	if (f->clipt)
		cxgbe_clip_release(f->dev, f->clipt);

	/*
	 * The zeroing of the filter rule below clears the filter valid,
	 * pending, locked flags etc. so it's all we need for
	 * this operation.
	 */
	memset(f, 0, sizeof(*f));
}

/**
 * t4_mk_filtdelwr - create a delete filter WR
 * @adap: adapter context
 * @ftid: the filter ID
 * @wr: the filter work request to populate
 * @qid: ingress queue to receive the delete notification
 *
 * Creates a filter work request to delete the supplied filter.  If @qid is
 * negative the delete notification is suppressed.
 */
static void t4_mk_filtdelwr(struct adapter *adap, unsigned int ftid,
			    struct fw_filter2_wr *wr, int qid)
{
	memset(wr, 0, sizeof(*wr));
	if (adap->params.filter2_wr_support)
		wr->op_pkd = cpu_to_be32(V_FW_WR_OP(FW_FILTER2_WR));
	else
		wr->op_pkd = cpu_to_be32(V_FW_WR_OP(FW_FILTER_WR));
	wr->len16_pkd = cpu_to_be32(V_FW_WR_LEN16(sizeof(*wr) / 16));
	wr->tid_to_iq = cpu_to_be32(V_FW_FILTER_WR_TID(ftid) |
				    V_FW_FILTER_WR_NOREPLY(qid < 0));
	wr->del_filter_to_l2tix = cpu_to_be32(F_FW_FILTER_WR_DEL_FILTER);
	if (qid >= 0)
		wr->rx_chan_rx_rpl_iq =
				cpu_to_be16(V_FW_FILTER_WR_RX_RPL_IQ(qid));
}

/**
 * Create FW work request to delete the filter at a specified index
 */
static int del_filter_wr(struct rte_eth_dev *dev, unsigned int fidx)
{
	struct adapter *adapter = ethdev2adap(dev);
	struct filter_entry *f = &adapter->tids.ftid_tab[fidx];
	struct rte_mbuf *mbuf;
	struct fw_filter2_wr *fwr;
	struct sge_ctrl_txq *ctrlq;
	unsigned int port_id = ethdev2pinfo(dev)->port_id;

	ctrlq = &adapter->sge.ctrlq[port_id];
	mbuf = rte_pktmbuf_alloc(ctrlq->mb_pool);
	if (!mbuf)
		return -ENOMEM;

	mbuf->data_len = sizeof(*fwr);
	mbuf->pkt_len = mbuf->data_len;

	fwr = rte_pktmbuf_mtod(mbuf, struct fw_filter2_wr *);
	t4_mk_filtdelwr(adapter, f->tid, fwr, adapter->sge.fw_evtq.abs_id);

	/*
	 * Mark the filter as "pending" and ship off the Filter Work Request.
	 * When we get the Work Request Reply we'll clear the pending status.
	 */
	f->pending = 1;
	t4_mgmt_tx(ctrlq, mbuf);
	return 0;
}

int set_filter_wr(struct rte_eth_dev *dev, unsigned int fidx)
{
	struct adapter *adapter = ethdev2adap(dev);
	struct filter_entry *f = &adapter->tids.ftid_tab[fidx];
	struct rte_mbuf *mbuf;
	struct fw_filter2_wr *fwr;
	struct sge_ctrl_txq *ctrlq;
	unsigned int port_id = ethdev2pinfo(dev)->port_id;
	int ret;

	/*
	 * If the new filter requires loopback Destination MAC and/or VLAN
	 * rewriting then we need to allocate a Layer 2 Table (L2T) entry for
	 * the filter.
	 */
	if (f->fs.newvlan) {
		/* allocate L2T entry for new filter */
		f->l2t = cxgbe_l2t_alloc_switching(f->dev, f->fs.vlan,
						   f->fs.eport, f->fs.dmac);
		if (!f->l2t)
			return -ENOMEM;
	}

	ctrlq = &adapter->sge.ctrlq[port_id];
	mbuf = rte_pktmbuf_alloc(ctrlq->mb_pool);
	if (!mbuf) {
		ret = -ENOMEM;
		goto out;
	}

	mbuf->data_len = sizeof(*fwr);
	mbuf->pkt_len = mbuf->data_len;

	fwr = rte_pktmbuf_mtod(mbuf, struct fw_filter2_wr *);
	memset(fwr, 0, sizeof(*fwr));

	/*
	 * Construct the work request to set the filter.
	 */
	if (adapter->params.filter2_wr_support)
		fwr->op_pkd = cpu_to_be32(V_FW_WR_OP(FW_FILTER2_WR));
	else
		fwr->op_pkd = cpu_to_be32(V_FW_WR_OP(FW_FILTER_WR));
	fwr->len16_pkd = cpu_to_be32(V_FW_WR_LEN16(sizeof(*fwr) / 16));
	fwr->tid_to_iq =
		cpu_to_be32(V_FW_FILTER_WR_TID(f->tid) |
			    V_FW_FILTER_WR_RQTYPE(f->fs.type) |
			    V_FW_FILTER_WR_NOREPLY(0) |
			    V_FW_FILTER_WR_IQ(f->fs.iq));
	fwr->del_filter_to_l2tix =
		cpu_to_be32(V_FW_FILTER_WR_DROP(f->fs.action == FILTER_DROP) |
			    V_FW_FILTER_WR_DIRSTEER(f->fs.dirsteer) |
			    V_FW_FILTER_WR_LPBK(f->fs.action == FILTER_SWITCH) |
			    V_FW_FILTER_WR_INSVLAN
				(f->fs.newvlan == VLAN_INSERT ||
				 f->fs.newvlan == VLAN_REWRITE) |
			    V_FW_FILTER_WR_RMVLAN
				(f->fs.newvlan == VLAN_REMOVE ||
				 f->fs.newvlan == VLAN_REWRITE) |
			    V_FW_FILTER_WR_HITCNTS(f->fs.hitcnts) |
			    V_FW_FILTER_WR_TXCHAN(f->fs.eport) |
			    V_FW_FILTER_WR_PRIO(f->fs.prio) |
			    V_FW_FILTER_WR_L2TIX(f->l2t ? f->l2t->idx : 0));
	fwr->ethtype = cpu_to_be16(f->fs.val.ethtype);
	fwr->ethtypem = cpu_to_be16(f->fs.mask.ethtype);
	fwr->smac_sel = 0;
	fwr->rx_chan_rx_rpl_iq =
		cpu_to_be16(V_FW_FILTER_WR_RX_CHAN(0) |
			    V_FW_FILTER_WR_RX_RPL_IQ(adapter->sge.fw_evtq.abs_id
						     ));
	fwr->maci_to_matchtypem =
		cpu_to_be32(V_FW_FILTER_WR_MACI(f->fs.val.macidx) |
			    V_FW_FILTER_WR_MACIM(f->fs.mask.macidx) |
			    V_FW_FILTER_WR_PORT(f->fs.val.iport) |
			    V_FW_FILTER_WR_PORTM(f->fs.mask.iport));
	fwr->ptcl = f->fs.val.proto;
	fwr->ptclm = f->fs.mask.proto;
	rte_memcpy(fwr->lip, f->fs.val.lip, sizeof(fwr->lip));
	rte_memcpy(fwr->lipm, f->fs.mask.lip, sizeof(fwr->lipm));
	rte_memcpy(fwr->fip, f->fs.val.fip, sizeof(fwr->fip));
	rte_memcpy(fwr->fipm, f->fs.mask.fip, sizeof(fwr->fipm));
	fwr->lp = cpu_to_be16(f->fs.val.lport);
	fwr->lpm = cpu_to_be16(f->fs.mask.lport);
	fwr->fp = cpu_to_be16(f->fs.val.fport);
	fwr->fpm = cpu_to_be16(f->fs.mask.fport);

	if (adapter->params.filter2_wr_support) {
		fwr->filter_type_swapmac =
			 V_FW_FILTER2_WR_SWAPMAC(f->fs.swapmac);
		fwr->natmode_to_ulp_type =
			V_FW_FILTER2_WR_ULP_TYPE(f->fs.nat_mode ?
						 ULP_MODE_TCPDDP :
						 ULP_MODE_NONE) |
			V_FW_FILTER2_WR_NATMODE(f->fs.nat_mode);
		memcpy(fwr->newlip, f->fs.nat_lip, sizeof(fwr->newlip));
		memcpy(fwr->newfip, f->fs.nat_fip, sizeof(fwr->newfip));
		fwr->newlport = cpu_to_be16(f->fs.nat_lport);
		fwr->newfport = cpu_to_be16(f->fs.nat_fport);
	}

	/*
	 * Mark the filter as "pending" and ship off the Filter Work Request.
	 * When we get the Work Request Reply we'll clear the pending status.
	 */
	f->pending = 1;
	t4_mgmt_tx(ctrlq, mbuf);
	return 0;

out:
	return ret;
}

/**
 * Set the corresponding entry in the bitmap. 4 slots are
 * marked for IPv6, whereas only 1 slot is marked for IPv4.
 */
static int cxgbe_set_ftid(struct tid_info *t, int fidx, int family)
{
	t4_os_lock(&t->ftid_lock);
	if (rte_bitmap_get(t->ftid_bmap, fidx)) {
		t4_os_unlock(&t->ftid_lock);
		return -EBUSY;
	}

	if (family == FILTER_TYPE_IPV4) {
		rte_bitmap_set(t->ftid_bmap, fidx);
	} else {
		rte_bitmap_set(t->ftid_bmap, fidx);
		rte_bitmap_set(t->ftid_bmap, fidx + 1);
		rte_bitmap_set(t->ftid_bmap, fidx + 2);
		rte_bitmap_set(t->ftid_bmap, fidx + 3);
	}
	t4_os_unlock(&t->ftid_lock);
	return 0;
}

/**
 * Clear the corresponding entry in the bitmap. 4 slots are
 * cleared for IPv6, whereas only 1 slot is cleared for IPv4.
 */
static void cxgbe_clear_ftid(struct tid_info *t, int fidx, int family)
{
	t4_os_lock(&t->ftid_lock);
	if (family == FILTER_TYPE_IPV4) {
		rte_bitmap_clear(t->ftid_bmap, fidx);
	} else {
		rte_bitmap_clear(t->ftid_bmap, fidx);
		rte_bitmap_clear(t->ftid_bmap, fidx + 1);
		rte_bitmap_clear(t->ftid_bmap, fidx + 2);
		rte_bitmap_clear(t->ftid_bmap, fidx + 3);
	}
	t4_os_unlock(&t->ftid_lock);
}

/**
 * Check a delete filter request for validity and send it to the hardware.
 * Return 0 on success, an error number otherwise.  We attach any provided
 * filter operation context to the internal filter specification in order to
 * facilitate signaling completion of the operation.
 */
int cxgbe_del_filter(struct rte_eth_dev *dev, unsigned int filter_id,
		     struct ch_filter_specification *fs,
		     struct filter_ctx *ctx)
{
	struct port_info *pi = (struct port_info *)(dev->data->dev_private);
	struct adapter *adapter = pi->adapter;
	struct filter_entry *f;
	unsigned int chip_ver;
	int ret;

	if (is_hashfilter(adapter) && fs->cap)
		return cxgbe_del_hash_filter(dev, filter_id, ctx);

	if (filter_id >= adapter->tids.nftids)
		return -ERANGE;

	chip_ver = CHELSIO_CHIP_VERSION(adapter->params.chip);

	ret = is_filter_set(&adapter->tids, filter_id, fs->type);
	if (!ret) {
		dev_warn(adap, "%s: could not find filter entry: %u\n",
			 __func__, filter_id);
		return -EINVAL;
	}

	/*
	 * Ensure filter id is aligned on the 2 slot boundary for T6,
	 * and 4 slot boundary for cards below T6.
	 */
	if (fs->type) {
		if (chip_ver < CHELSIO_T6)
			filter_id &= ~(0x3);
		else
			filter_id &= ~(0x1);
	}

	f = &adapter->tids.ftid_tab[filter_id];
	ret = writable_filter(f);
	if (ret)
		return ret;

	if (f->valid) {
		f->ctx = ctx;
		cxgbe_clear_ftid(&adapter->tids,
				 f->tid - adapter->tids.ftid_base,
				 f->fs.type ? FILTER_TYPE_IPV6 :
					      FILTER_TYPE_IPV4);
		return del_filter_wr(dev, filter_id);
	}

	/*
	 * If the caller has passed in a Completion Context then we need to
	 * mark it as a successful completion so they don't stall waiting
	 * for it.
	 */
	if (ctx) {
		ctx->result = 0;
		t4_complete(&ctx->completion);
	}

	return 0;
}

/**
 * Check a Chelsio Filter Request for validity, convert it into our internal
 * format and send it to the hardware.  Return 0 on success, an error number
 * otherwise.  We attach any provided filter operation context to the internal
 * filter specification in order to facilitate signaling completion of the
 * operation.
 */
int cxgbe_set_filter(struct rte_eth_dev *dev, unsigned int filter_id,
		     struct ch_filter_specification *fs,
		     struct filter_ctx *ctx)
{
	struct port_info *pi = ethdev2pinfo(dev);
	struct adapter *adapter = pi->adapter;
	unsigned int fidx, iq, fid_bit = 0;
	struct filter_entry *f;
	unsigned int chip_ver;
	uint8_t bitoff[16] = {0};
	int ret;

	if (is_hashfilter(adapter) && fs->cap)
		return cxgbe_set_hash_filter(dev, fs, ctx);

	if (filter_id >= adapter->tids.nftids)
		return -ERANGE;

	chip_ver = CHELSIO_CHIP_VERSION(adapter->params.chip);

	ret = validate_filter(adapter, fs);
	if (ret)
		return ret;

	/*
	 * Ensure filter id is aligned on the 4 slot boundary for IPv6
	 * maskfull filters.
	 */
	if (fs->type)
		filter_id &= ~(0x3);

	ret = is_filter_set(&adapter->tids, filter_id, fs->type);
	if (ret)
		return -EBUSY;

	iq = get_filter_steerq(dev, fs);

	/*
	 * IPv6 filters occupy four slots and must be aligned on four-slot
	 * boundaries for T5. On T6, IPv6 filters occupy two-slots and
	 * must be aligned on two-slot boundaries.
	 *
	 * IPv4 filters only occupy a single slot and have no alignment
	 * requirements but writing a new IPv4 filter into the middle
	 * of an existing IPv6 filter requires clearing the old IPv6
	 * filter.
	 */
	if (fs->type == FILTER_TYPE_IPV4) { /* IPv4 */
		/*
		 * For T6, If our IPv4 filter isn't being written to a
		 * multiple of two filter index and there's an IPv6
		 * filter at the multiple of 2 base slot, then we need
		 * to delete that IPv6 filter ...
		 * For adapters below T6, IPv6 filter occupies 4 entries.
		 */
		if (chip_ver < CHELSIO_T6)
			fidx = filter_id & ~0x3;
		else
			fidx = filter_id & ~0x1;

		if (fidx != filter_id && adapter->tids.ftid_tab[fidx].fs.type) {
			f = &adapter->tids.ftid_tab[fidx];
			if (f->valid)
				return -EBUSY;
		}
	} else { /* IPv6 */
		unsigned int max_filter_id;

		if (chip_ver < CHELSIO_T6) {
			/*
			 * Ensure that the IPv6 filter is aligned on a
			 * multiple of 4 boundary.
			 */
			if (filter_id & 0x3)
				return -EINVAL;

			max_filter_id = filter_id + 4;
		} else {
			/*
			 * For T6, CLIP being enabled, IPv6 filter would occupy
			 * 2 entries.
			 */
			if (filter_id & 0x1)
				return -EINVAL;

			max_filter_id = filter_id + 2;
		}

		/*
		 * Check all except the base overlapping IPv4 filter
		 * slots.
		 */
		for (fidx = filter_id + 1; fidx < max_filter_id; fidx++) {
			f = &adapter->tids.ftid_tab[fidx];
			if (f->valid)
				return -EBUSY;
		}
	}

	/*
	 * Check to make sure that provided filter index is not
	 * already in use by someone else
	 */
	f = &adapter->tids.ftid_tab[filter_id];
	if (f->valid)
		return -EBUSY;

	fidx = adapter->tids.ftid_base + filter_id;
	fid_bit = filter_id;
	ret = cxgbe_set_ftid(&adapter->tids, fid_bit,
			     fs->type ? FILTER_TYPE_IPV6 : FILTER_TYPE_IPV4);
	if (ret)
		return ret;

	/*
	 * Check to make sure the filter requested is writable ...
	 */
	ret = writable_filter(f);
	if (ret) {
		/* Clear the bits we have set above */
		cxgbe_clear_ftid(&adapter->tids, fid_bit,
				 fs->type ? FILTER_TYPE_IPV6 :
					    FILTER_TYPE_IPV4);
		return ret;
	}

	/*
	 * Allocate a clip table entry only if we have non-zero IPv6 address
	 */
	if (chip_ver > CHELSIO_T5 && fs->type &&
	    memcmp(fs->val.lip, bitoff, sizeof(bitoff))) {
		f->clipt = cxgbe_clip_alloc(f->dev, (u32 *)&f->fs.val.lip);
		if (!f->clipt)
			goto free_tid;
	}

	/*
	 * Convert the filter specification into our internal format.
	 * We copy the PF/VF specification into the Outer VLAN field
	 * here so the rest of the code -- including the interface to
	 * the firmware -- doesn't have to constantly do these checks.
	 */
	f->fs = *fs;
	f->fs.iq = iq;
	f->dev = dev;

	/*
	 * Attempt to set the filter.  If we don't succeed, we clear
	 * it and return the failure.
	 */
	f->ctx = ctx;
	f->tid = fidx; /* Save the actual tid */
	ret = set_filter_wr(dev, filter_id);
	if (ret) {
		fid_bit = f->tid - adapter->tids.ftid_base;
		goto free_tid;
	}

	return ret;

free_tid:
	cxgbe_clear_ftid(&adapter->tids, fid_bit,
			 fs->type ? FILTER_TYPE_IPV6 :
				    FILTER_TYPE_IPV4);
	clear_filter(f);
	return ret;
}

/**
 * Handle a Hash filter write reply.
 */
void hash_filter_rpl(struct adapter *adap, const struct cpl_act_open_rpl *rpl)
{
	struct tid_info *t = &adap->tids;
	struct filter_entry *f;
	struct filter_ctx *ctx = NULL;
	unsigned int tid = GET_TID(rpl);
	unsigned int ftid = G_TID_TID(G_AOPEN_ATID
				      (be32_to_cpu(rpl->atid_status)));
	unsigned int status  = G_AOPEN_STATUS(be32_to_cpu(rpl->atid_status));

	f = lookup_atid(t, ftid);
	if (!f) {
		dev_warn(adap, "%s: could not find filter entry: %d\n",
			 __func__, ftid);
		return;
	}

	ctx = f->ctx;
	f->ctx = NULL;

	switch (status) {
	case CPL_ERR_NONE: {
		f->tid = tid;
		f->pending = 0;  /* asynchronous setup completed */
		f->valid = 1;

		cxgbe_insert_tid(t, f, f->tid, 0);
		cxgbe_free_atid(t, ftid);
		if (ctx) {
			ctx->tid = f->tid;
			ctx->result = 0;
		}
		if (f->fs.hitcnts)
			set_tcb_field(adap, tid,
				      W_TCB_TIMESTAMP,
				      V_TCB_TIMESTAMP(M_TCB_TIMESTAMP) |
				      V_TCB_T_RTT_TS_RECENT_AGE
					      (M_TCB_T_RTT_TS_RECENT_AGE),
				      V_TCB_TIMESTAMP(0ULL) |
				      V_TCB_T_RTT_TS_RECENT_AGE(0ULL),
				      1);
		if (f->fs.newvlan == VLAN_INSERT ||
		    f->fs.newvlan == VLAN_REWRITE)
			set_tcb_tflag(adap, tid, S_TF_CCTRL_RFR, 1, 1);
		break;
	}
	default:
		dev_warn(adap, "%s: filter creation failed with status = %u\n",
			 __func__, status);

		if (ctx) {
			if (status == CPL_ERR_TCAM_FULL)
				ctx->result = -EAGAIN;
			else
				ctx->result = -EINVAL;
		}

		cxgbe_free_atid(t, ftid);
		t4_os_free(f);
	}

	if (ctx)
		t4_complete(&ctx->completion);
}

/**
 * Handle a LE-TCAM filter write/deletion reply.
 */
void filter_rpl(struct adapter *adap, const struct cpl_set_tcb_rpl *rpl)
{
	struct filter_entry *f = NULL;
	unsigned int tid = GET_TID(rpl);
	int idx, max_fidx = adap->tids.nftids;

	/* Get the corresponding filter entry for this tid */
	if (adap->tids.ftid_tab) {
		/* Check this in normal filter region */
		idx = tid - adap->tids.ftid_base;
		if (idx >= max_fidx)
			return;

		f = &adap->tids.ftid_tab[idx];
		if (f->tid != tid)
			return;
	}

	/* We found the filter entry for this tid */
	if (f) {
		unsigned int ret = G_COOKIE(rpl->cookie);
		struct filter_ctx *ctx;

		/*
		 * Pull off any filter operation context attached to the
		 * filter.
		 */
		ctx = f->ctx;
		f->ctx = NULL;

		if (ret == FW_FILTER_WR_FLT_ADDED) {
			f->pending = 0;  /* asynchronous setup completed */
			f->valid = 1;
			if (ctx) {
				ctx->tid = f->tid;
				ctx->result = 0;
			}
		} else if (ret == FW_FILTER_WR_FLT_DELETED) {
			/*
			 * Clear the filter when we get confirmation from the
			 * hardware that the filter has been deleted.
			 */
			clear_filter(f);
			if (ctx)
				ctx->result = 0;
		} else {
			/*
			 * Something went wrong.  Issue a warning about the
			 * problem and clear everything out.
			 */
			dev_warn(adap, "filter %u setup failed with error %u\n",
				 idx, ret);
			clear_filter(f);
			if (ctx)
				ctx->result = -EINVAL;
		}

		if (ctx)
			t4_complete(&ctx->completion);
	}
}

/*
 * Retrieve the packet count for the specified filter.
 */
int cxgbe_get_filter_count(struct adapter *adapter, unsigned int fidx,
			   u64 *c, int hash, bool get_byte)
{
	struct filter_entry *f;
	unsigned int tcb_base, tcbaddr;
	int ret;

	tcb_base = t4_read_reg(adapter, A_TP_CMM_TCB_BASE);
	if (is_hashfilter(adapter) && hash) {
		if (fidx < adapter->tids.ntids) {
			f = adapter->tids.tid_tab[fidx];
			if (!f)
				return -EINVAL;

			if (is_t5(adapter->params.chip)) {
				*c = 0;
				return 0;
			}
			tcbaddr = tcb_base + (fidx * TCB_SIZE);
			goto get_count;
		} else {
			return -ERANGE;
		}
	} else {
		if (fidx >= adapter->tids.nftids)
			return -ERANGE;

		f = &adapter->tids.ftid_tab[fidx];
		if (!f->valid)
			return -EINVAL;

		tcbaddr = tcb_base + f->tid * TCB_SIZE;
	}

	f = &adapter->tids.ftid_tab[fidx];
	if (!f->valid)
		return -EINVAL;

get_count:
	if (is_t5(adapter->params.chip) || is_t6(adapter->params.chip)) {
		/*
		 * For T5, the Filter Packet Hit Count is maintained as a
		 * 32-bit Big Endian value in the TCB field {timestamp}.
		 * Similar to the craziness above, instead of the filter hit
		 * count showing up at offset 20 ((W_TCB_TIMESTAMP == 5) *
		 * sizeof(u32)), it actually shows up at offset 24.  Whacky.
		 */
		if (get_byte) {
			unsigned int word_offset = 4;
			__be64 be64_byte_count;

			t4_os_lock(&adapter->win0_lock);
			ret = t4_memory_rw(adapter, MEMWIN_NIC, MEM_EDC0,
					   tcbaddr +
					   (word_offset * sizeof(__be32)),
					   sizeof(be64_byte_count),
					   &be64_byte_count,
					   T4_MEMORY_READ);
			t4_os_unlock(&adapter->win0_lock);
			if (ret < 0)
				return ret;
			*c = be64_to_cpu(be64_byte_count);
		} else {
			unsigned int word_offset = 6;
			__be32 be32_count;

			t4_os_lock(&adapter->win0_lock);
			ret = t4_memory_rw(adapter, MEMWIN_NIC, MEM_EDC0,
					   tcbaddr +
					   (word_offset * sizeof(__be32)),
					   sizeof(be32_count), &be32_count,
					   T4_MEMORY_READ);
			t4_os_unlock(&adapter->win0_lock);
			if (ret < 0)
				return ret;
			*c = (u64)be32_to_cpu(be32_count);
		}
	}
	return 0;
}

/*
 * Clear the packet count for the specified filter.
 */
int cxgbe_clear_filter_count(struct adapter *adapter, unsigned int fidx,
			     int hash, bool clear_byte)
{
	u64 tcb_mask = 0, tcb_val = 0;
	struct filter_entry *f = NULL;
	u16 tcb_word = 0;

	if (is_hashfilter(adapter) && hash) {
		if (fidx >= adapter->tids.ntids)
			return -ERANGE;

		/* No hitcounts supported for T5 hashfilters */
		if (is_t5(adapter->params.chip))
			return 0;

		f = adapter->tids.tid_tab[fidx];
	} else {
		if (fidx >= adapter->tids.nftids)
			return -ERANGE;

		f = &adapter->tids.ftid_tab[fidx];
	}

	if (!f || !f->valid)
		return -EINVAL;

	tcb_word = W_TCB_TIMESTAMP;
	tcb_mask = V_TCB_TIMESTAMP(M_TCB_TIMESTAMP);
	tcb_val = V_TCB_TIMESTAMP(0ULL);

	set_tcb_field(adapter, f->tid, tcb_word, tcb_mask, tcb_val, 1);

	if (clear_byte) {
		tcb_word = W_TCB_T_RTT_TS_RECENT_AGE;
		tcb_mask =
			V_TCB_T_RTT_TS_RECENT_AGE(M_TCB_T_RTT_TS_RECENT_AGE) |
			V_TCB_T_RTSEQ_RECENT(M_TCB_T_RTSEQ_RECENT);
		tcb_val = V_TCB_T_RTT_TS_RECENT_AGE(0ULL) |
			  V_TCB_T_RTSEQ_RECENT(0ULL);

		set_tcb_field(adapter, f->tid, tcb_word, tcb_mask, tcb_val, 1);
	}

	return 0;
}

/**
 * Handle a Hash filter delete reply.
 */
void hash_del_filter_rpl(struct adapter *adap,
			 const struct cpl_abort_rpl_rss *rpl)
{
	struct tid_info *t = &adap->tids;
	struct filter_entry *f;
	struct filter_ctx *ctx = NULL;
	unsigned int tid = GET_TID(rpl);

	f = lookup_tid(t, tid);
	if (!f) {
		dev_warn(adap, "%s: could not find filter entry: %u\n",
			 __func__, tid);
		return;
	}

	ctx = f->ctx;
	f->ctx = NULL;

	f->valid = 0;

	if (f->clipt)
		cxgbe_clip_release(f->dev, f->clipt);

	cxgbe_remove_tid(t, 0, tid, 0);
	t4_os_free(f);

	if (ctx) {
		ctx->result = 0;
		t4_complete(&ctx->completion);
	}
}
