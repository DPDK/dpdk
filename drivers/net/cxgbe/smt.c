/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Chelsio Communications.
 * All rights reserved.
 */

#include "base/common.h"
#include "smt.h"

void cxgbe_do_smt_write_rpl(struct adapter *adap,
			    const struct cpl_smt_write_rpl *rpl)
{
	unsigned int smtidx = G_TID_TID(GET_TID(rpl));
	struct smt_data *s = adap->smt;

	if (unlikely(rpl->status != CPL_ERR_NONE)) {
		struct smt_entry *e = &s->smtab[smtidx];

		dev_err(adap,
			"Unexpected SMT_WRITE_RPL status %u for entry %u\n",
			rpl->status, smtidx);
		t4_os_lock(&e->lock);
		e->state = SMT_STATE_ERROR;
		t4_os_unlock(&e->lock);
	}
}

static int write_smt_entry(struct rte_eth_dev *dev, struct smt_entry *e)
{
	unsigned int port_id = ethdev2pinfo(dev)->port_id;
	struct adapter *adap = ethdev2adap(dev);
	struct cpl_t6_smt_write_req *t6req;
	struct smt_data *s = adap->smt;
	struct cpl_smt_write_req *req;
	struct sge_ctrl_txq *ctrlq;
	struct rte_mbuf *mbuf;
	u8 row;

	ctrlq = &adap->sge.ctrlq[port_id];
	mbuf = rte_pktmbuf_alloc(ctrlq->mb_pool);
	if (!mbuf)
		return -ENOMEM;

	if (CHELSIO_CHIP_VERSION(adap->params.chip) <= CHELSIO_T5) {
		mbuf->data_len = sizeof(*req);
		mbuf->pkt_len = mbuf->data_len;

		/* Source MAC Table (SMT) contains 256 SMAC entries
		 * organized in 128 rows of 2 entries each.
		 */
		req = rte_pktmbuf_mtod(mbuf, struct cpl_smt_write_req *);
		INIT_TP_WR(req, 0);

		/* Each row contains an SMAC pair.
		 * LSB selects the SMAC entry within a row
		 */
		if (e->idx & 1) {
			req->pfvf1 = 0x0;
			rte_memcpy(req->src_mac1, e->src_mac,
				   RTE_ETHER_ADDR_LEN);

			/* fill pfvf0/src_mac0 with entry
			 * at prev index from smt-tab.
			 */
			req->pfvf0 = 0x0;
			rte_memcpy(req->src_mac0, s->smtab[e->idx - 1].src_mac,
				   RTE_ETHER_ADDR_LEN);
		} else {
			req->pfvf0 = 0x0;
			rte_memcpy(req->src_mac0, e->src_mac,
				   RTE_ETHER_ADDR_LEN);

			/* fill pfvf1/src_mac1 with entry
			 * at next index from smt-tab
			 */
			req->pfvf1 = 0x0;
			rte_memcpy(req->src_mac1, s->smtab[e->idx + 1].src_mac,
				   RTE_ETHER_ADDR_LEN);
		}
		row = (e->hw_idx >> 1);
	} else {
		mbuf->data_len = sizeof(*t6req);
		mbuf->pkt_len = mbuf->data_len;

		/* Source MAC Table (SMT) contains 256 SMAC entries */
		t6req = rte_pktmbuf_mtod(mbuf, struct cpl_t6_smt_write_req *);
		INIT_TP_WR(t6req, 0);

		/* fill pfvf0/src_mac0 from smt-tab */
		t6req->pfvf0 = 0x0;
		rte_memcpy(t6req->src_mac0, s->smtab[e->idx].src_mac,
			   RTE_ETHER_ADDR_LEN);
		row = e->hw_idx;
		req = (struct cpl_smt_write_req *)t6req;
	}

	OPCODE_TID(req) =
		cpu_to_be32(MK_OPCODE_TID(CPL_SMT_WRITE_REQ,
					  e->hw_idx |
					  V_TID_QID(adap->sge.fw_evtq.abs_id)));

	req->params = cpu_to_be32(V_SMTW_NORPL(0) |
				  V_SMTW_IDX(row) |
				  V_SMTW_OVLAN_IDX(0));
	t4_mgmt_tx(ctrlq, mbuf);

	return 0;
}

/**
 * find_or_alloc_smte - Find/Allocate a free SMT entry
 * @s: SMT table
 * @smac: Source MAC address to compare/add
 * Returns pointer to the SMT entry found/created
 *
 * Finds/Allocates an SMT entry to be used by switching rule of a filter.
 */
static struct smt_entry *find_or_alloc_smte(struct smt_data *s, u8 *smac)
{
	struct smt_entry *e, *end, *first_free = NULL;

	for (e = &s->smtab[0], end = &s->smtab[s->smt_size]; e != end; ++e) {
		if (!rte_atomic32_read(&e->refcnt)) {
			if (!first_free)
				first_free = e;
		} else {
			if (e->state == SMT_STATE_SWITCHING) {
				/* This entry is actually in use. See if we can
				 * re-use it ?
				 */
				if (!memcmp(e->src_mac, smac,
					    RTE_ETHER_ADDR_LEN))
					goto found;
			}
		}
	}

	if (!first_free)
		return NULL;

	e = first_free;
	e->state = SMT_STATE_UNUSED;

found:
	return e;
}

static struct smt_entry *t4_smt_alloc_switching(struct rte_eth_dev *dev,
						u16 pfvf, u8 *smac)
{
	struct adapter *adap = ethdev2adap(dev);
	struct smt_data *s = adap->smt;
	struct smt_entry *e;
	int ret;

	t4_os_write_lock(&s->lock);
	e = find_or_alloc_smte(s, smac);
	if (e) {
		t4_os_lock(&e->lock);
		if (!rte_atomic32_read(&e->refcnt)) {
			e->pfvf = pfvf;
			rte_memcpy(e->src_mac, smac, RTE_ETHER_ADDR_LEN);
			ret = write_smt_entry(dev, e);
			if (ret) {
				e->pfvf = 0;
				memset(e->src_mac, 0, RTE_ETHER_ADDR_LEN);
				t4_os_unlock(&e->lock);
				e = NULL;
				goto out_write_unlock;
			}
			e->state = SMT_STATE_SWITCHING;
			rte_atomic32_set(&e->refcnt, 1);
		} else {
			rte_atomic32_inc(&e->refcnt);
		}
		t4_os_unlock(&e->lock);
	}

out_write_unlock:
	t4_os_write_unlock(&s->lock);
	return e;
}

/**
 * cxgbe_smt_alloc_switching - Allocate an SMT entry for switching rule
 * @dev: rte_eth_dev pointer
 * @smac: MAC address to add to SMT
 * Returns pointer to the SMT entry created
 *
 * Allocates an SMT entry to be used by switching rule of a filter.
 */
struct smt_entry *cxgbe_smt_alloc_switching(struct rte_eth_dev *dev, u8 *smac)
{
	return t4_smt_alloc_switching(dev, 0x0, smac);
}

void cxgbe_smt_release(struct smt_entry *e)
{
	if (rte_atomic32_read(&e->refcnt))
		rte_atomic32_dec(&e->refcnt);
}

/**
 * Initialize Source MAC Table
 */
struct smt_data *t4_init_smt(u32 smt_start_idx, u32 smt_size)
{
	struct smt_data *s;
	u32 i;

	s = t4_alloc_mem(sizeof(*s) + smt_size * sizeof(struct smt_entry));
	if (!s)
		return NULL;

	s->smt_start = smt_start_idx;
	s->smt_size = smt_size;
	t4_os_rwlock_init(&s->lock);

	for (i = 0; i < s->smt_size; ++i) {
		s->smtab[i].idx = i;
		s->smtab[i].hw_idx = smt_start_idx + i;
		s->smtab[i].state = SMT_STATE_UNUSED;
		memset(&s->smtab[i].src_mac, 0, RTE_ETHER_ADDR_LEN);
		t4_os_lock_init(&s->smtab[i].lock);
		rte_atomic32_set(&s->smtab[i].refcnt, 0);
	}
	return s;
}

/**
 * Cleanup Source MAC Table
 */
void t4_cleanup_smt(struct adapter *adap)
{
	if (adap->smt)
		t4_os_free(adap->smt);
}
