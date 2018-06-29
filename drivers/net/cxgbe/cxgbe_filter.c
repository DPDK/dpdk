/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */

#include "common.h"
#include "t4_regs.h"
#include "cxgbe_filter.h"

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

	if (U(F_ETHERTYPE, ethtype) || U(F_PROTOCOL, proto))
		return -EOPNOTSUPP;

#undef S
#undef U
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
 * Clear a filter and release any of its resources that we own.  This also
 * clears the filter's "pending" status.
 */
void clear_filter(struct filter_entry *f)
{
	/*
	 * The zeroing of the filter rule below clears the filter valid,
	 * pending, locked flags etc. so it's all we need for
	 * this operation.
	 */
	memset(f, 0, sizeof(*f));
}

/**
 * t4_mk_filtdelwr - create a delete filter WR
 * @ftid: the filter ID
 * @wr: the filter work request to populate
 * @qid: ingress queue to receive the delete notification
 *
 * Creates a filter work request to delete the supplied filter.  If @qid is
 * negative the delete notification is suppressed.
 */
static void t4_mk_filtdelwr(unsigned int ftid, struct fw_filter_wr *wr, int qid)
{
	memset(wr, 0, sizeof(*wr));
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
	struct fw_filter_wr *fwr;
	struct sge_ctrl_txq *ctrlq;
	unsigned int port_id = ethdev2pinfo(dev)->port_id;

	ctrlq = &adapter->sge.ctrlq[port_id];
	mbuf = rte_pktmbuf_alloc(ctrlq->mb_pool);
	if (!mbuf)
		return -ENOMEM;

	mbuf->data_len = sizeof(*fwr);
	mbuf->pkt_len = mbuf->data_len;

	fwr = rte_pktmbuf_mtod(mbuf, struct fw_filter_wr *);
	t4_mk_filtdelwr(f->tid, fwr, adapter->sge.fw_evtq.abs_id);

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
	struct fw_filter_wr *fwr;
	struct sge_ctrl_txq *ctrlq;
	unsigned int port_id = ethdev2pinfo(dev)->port_id;
	int ret;

	ctrlq = &adapter->sge.ctrlq[port_id];
	mbuf = rte_pktmbuf_alloc(ctrlq->mb_pool);
	if (!mbuf) {
		ret = -ENOMEM;
		goto out;
	}

	mbuf->data_len = sizeof(*fwr);
	mbuf->pkt_len = mbuf->data_len;

	fwr = rte_pktmbuf_mtod(mbuf, struct fw_filter_wr *);
	memset(fwr, 0, sizeof(*fwr));

	/*
	 * Construct the work request to set the filter.
	 */
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
			    V_FW_FILTER_WR_HITCNTS(f->fs.hitcnts) |
			    V_FW_FILTER_WR_PRIO(f->fs.prio));
	fwr->ethtype = cpu_to_be16(f->fs.val.ethtype);
	fwr->ethtypem = cpu_to_be16(f->fs.mask.ethtype);
	fwr->smac_sel = 0;
	fwr->rx_chan_rx_rpl_iq =
		cpu_to_be16(V_FW_FILTER_WR_RX_CHAN(0) |
			    V_FW_FILTER_WR_RX_RPL_IQ(adapter->sge.fw_evtq.abs_id
						     ));
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
	int ret;

	if (filter_id >= adapter->tids.nftids)
		return -ERANGE;

	ret = is_filter_set(&adapter->tids, filter_id, fs->type);
	if (!ret) {
		dev_warn(adap, "%s: could not find filter entry: %u\n",
			 __func__, filter_id);
		return -EINVAL;
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
	int ret;

	if (filter_id >= adapter->tids.nftids)
		return -ERANGE;

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
	 * IPv6 filters occupy four slots and must be aligned on
	 * four-slot boundaries.  IPv4 filters only occupy a single
	 * slot and have no alignment requirements but writing a new
	 * IPv4 filter into the middle of an existing IPv6 filter
	 * requires clearing the old IPv6 filter.
	 */
	if (fs->type == FILTER_TYPE_IPV4) { /* IPv4 */
		/*
		 * If our IPv4 filter isn't being written to a
		 * multiple of four filter index and there's an IPv6
		 * filter at the multiple of 4 base slot, then we need
		 * to delete that IPv6 filter ...
		 */
		fidx = filter_id & ~0x3;
		if (fidx != filter_id && adapter->tids.ftid_tab[fidx].fs.type) {
			f = &adapter->tids.ftid_tab[fidx];
			if (f->valid)
				return -EBUSY;
		}
	} else { /* IPv6 */
		/*
		 * Ensure that the IPv6 filter is aligned on a
		 * multiple of 4 boundary.
		 */
		if (filter_id & 0x3)
			return -EINVAL;

		/*
		 * Check all except the base overlapping IPv4 filter
		 * slots.
		 */
		for (fidx = filter_id + 1; fidx < filter_id + 4; fidx++) {
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
		cxgbe_clear_ftid(&adapter->tids, fid_bit,
				 fs->type ? FILTER_TYPE_IPV6 :
					    FILTER_TYPE_IPV4);
		clear_filter(f);
	}

	return ret;
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
			   u64 *c, bool get_byte)
{
	struct filter_entry *f;
	unsigned int tcb_base, tcbaddr;
	int ret;

	tcb_base = t4_read_reg(adapter, A_TP_CMM_TCB_BASE);
	if (fidx >= adapter->tids.nftids)
		return -ERANGE;

	f = &adapter->tids.ftid_tab[fidx];
	if (!f->valid)
		return -EINVAL;

	tcbaddr = tcb_base + f->tid * TCB_SIZE;

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
