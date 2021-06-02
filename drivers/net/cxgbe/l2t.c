/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */

#include "base/common.h"
#include "l2t.h"

/**
 * cxgbe_l2t_release - Release associated L2T entry
 * @e: L2T entry to release
 *
 * Releases ref count and frees up an L2T entry from L2T table
 */
void cxgbe_l2t_release(struct l2t_entry *e)
{
	if (__atomic_load_n(&e->refcnt, __ATOMIC_RELAXED) != 0)
		__atomic_sub_fetch(&e->refcnt, 1, __ATOMIC_RELAXED);
}

/**
 * Process a CPL_L2T_WRITE_RPL. Note that the TID in the reply is really
 * the L2T index it refers to.
 */
void cxgbe_do_l2t_write_rpl(struct adapter *adap,
			    const struct cpl_l2t_write_rpl *rpl)
{
	struct l2t_data *d = adap->l2t;
	unsigned int tid = GET_TID(rpl);
	unsigned int l2t_idx = tid % L2T_SIZE;

	if (unlikely(rpl->status != CPL_ERR_NONE)) {
		dev_err(adap,
			"Unexpected L2T_WRITE_RPL status %u for entry %u\n",
			rpl->status, l2t_idx);
		return;
	}

	if (tid & F_SYNC_WR) {
		struct l2t_entry *e = &d->l2tab[l2t_idx - d->l2t_start];

		t4_os_lock(&e->lock);
		if (e->state != L2T_STATE_SWITCHING)
			e->state = L2T_STATE_VALID;
		t4_os_unlock(&e->lock);
	}
}

/**
 * Write an L2T entry.  Must be called with the entry locked.
 * The write may be synchronous or asynchronous.
 */
static int write_l2e(struct rte_eth_dev *dev, struct l2t_entry *e, int sync,
		     bool loopback, bool arpmiss)
{
	struct adapter *adap = ethdev2adap(dev);
	struct l2t_data *d = adap->l2t;
	struct rte_mbuf *mbuf;
	struct cpl_l2t_write_req *req;
	struct sge_ctrl_txq *ctrlq;
	unsigned int l2t_idx = e->idx + d->l2t_start;
	unsigned int port_id = ethdev2pinfo(dev)->port_id;

	ctrlq = &adap->sge.ctrlq[port_id];
	mbuf = rte_pktmbuf_alloc(ctrlq->mb_pool);
	if (!mbuf)
		return -ENOMEM;

	mbuf->data_len = sizeof(*req);
	mbuf->pkt_len = mbuf->data_len;

	req = rte_pktmbuf_mtod(mbuf, struct cpl_l2t_write_req *);
	INIT_TP_WR(req, 0);

	OPCODE_TID(req) =
		cpu_to_be32(MK_OPCODE_TID(CPL_L2T_WRITE_REQ,
					  l2t_idx | V_SYNC_WR(sync) |
					  V_TID_QID(adap->sge.fw_evtq.abs_id)));
	req->params = cpu_to_be16(V_L2T_W_PORT(e->lport) |
				  V_L2T_W_LPBK(loopback) |
				  V_L2T_W_ARPMISS(arpmiss) |
				  V_L2T_W_NOREPLY(!sync));
	req->l2t_idx = cpu_to_be16(l2t_idx);
	req->vlan = cpu_to_be16(e->vlan);
	rte_memcpy(req->dst_mac, e->dmac, RTE_ETHER_ADDR_LEN);

	if (loopback)
		memset(req->dst_mac, 0, RTE_ETHER_ADDR_LEN);

	t4_mgmt_tx(ctrlq, mbuf);

	if (sync && e->state != L2T_STATE_SWITCHING)
		e->state = L2T_STATE_SYNC_WRITE;

	return 0;
}

/**
 * find_or_alloc_l2e - Find/Allocate a free L2T entry
 * @d: L2T table
 * @vlan: VLAN id to compare/add
 * @port: port id to compare/add
 * @dmac: Destination MAC address to compare/add
 * Returns pointer to the L2T entry found/created
 *
 * Finds/Allocates an L2T entry to be used by switching rule of a filter.
 */
static struct l2t_entry *find_or_alloc_l2e(struct l2t_data *d, u16 vlan,
					   u8 port, u8 *dmac)
{
	struct l2t_entry *end, *e;
	struct l2t_entry *first_free = NULL;

	for (e = &d->l2tab[0], end = &d->l2tab[d->l2t_size]; e != end; ++e) {
		if (__atomic_load_n(&e->refcnt, __ATOMIC_RELAXED) == 0) {
			if (!first_free)
				first_free = e;
		} else {
			if (e->state == L2T_STATE_SWITCHING) {
				if ((!memcmp(e->dmac, dmac, RTE_ETHER_ADDR_LEN)) &&
				    e->vlan == vlan && e->lport == port)
					goto exists;
			}
		}
	}

	if (first_free) {
		e = first_free;
		goto found;
	}

	return NULL;

found:
	e->state = L2T_STATE_UNUSED;

exists:
	return e;
}

static struct l2t_entry *t4_l2t_alloc_switching(struct rte_eth_dev *dev,
						u16 vlan, u8 port,
						u8 *eth_addr)
{
	struct adapter *adap = ethdev2adap(dev);
	struct l2t_data *d = adap->l2t;
	struct l2t_entry *e;
	int ret = 0;

	t4_os_write_lock(&d->lock);
	e = find_or_alloc_l2e(d, vlan, port, eth_addr);
	if (e) {
		t4_os_lock(&e->lock);
		if (__atomic_load_n(&e->refcnt, __ATOMIC_RELAXED) == 0) {
			e->state = L2T_STATE_SWITCHING;
			e->vlan = vlan;
			e->lport = port;
			rte_memcpy(e->dmac, eth_addr, RTE_ETHER_ADDR_LEN);
			__atomic_store_n(&e->refcnt, 1, __ATOMIC_RELAXED);
			ret = write_l2e(dev, e, 0, !L2T_LPBK, !L2T_ARPMISS);
			if (ret < 0)
				dev_debug(adap, "Failed to write L2T entry: %d",
					  ret);
		} else {
			__atomic_add_fetch(&e->refcnt, 1, __ATOMIC_RELAXED);
		}
		t4_os_unlock(&e->lock);
	}
	t4_os_write_unlock(&d->lock);

	return ret ? NULL : e;
}

/**
 * cxgbe_l2t_alloc_switching - Allocate a L2T entry for switching rule
 * @dev: rte_eth_dev pointer
 * @vlan: VLAN Id
 * @port: Associated port
 * @dmac: Destination MAC address to add to L2T
 * Returns pointer to the allocated l2t entry
 *
 * Allocates a L2T entry for use by switching rule of a filter
 */
struct l2t_entry *cxgbe_l2t_alloc_switching(struct rte_eth_dev *dev, u16 vlan,
					    u8 port, u8 *dmac)
{
	return t4_l2t_alloc_switching(dev, vlan, port, dmac);
}

/**
 * Initialize L2 Table
 */
struct l2t_data *t4_init_l2t(unsigned int l2t_start, unsigned int l2t_end)
{
	unsigned int l2t_size;
	unsigned int i;
	struct l2t_data *d;

	if (l2t_start >= l2t_end || l2t_end >= L2T_SIZE)
		return NULL;
	l2t_size = l2t_end - l2t_start + 1;

	d = t4_os_alloc(sizeof(*d) + l2t_size * sizeof(struct l2t_entry));
	if (!d)
		return NULL;

	d->l2t_start = l2t_start;
	d->l2t_size = l2t_size;

	t4_os_rwlock_init(&d->lock);

	for (i = 0; i < d->l2t_size; ++i) {
		d->l2tab[i].idx = i;
		d->l2tab[i].state = L2T_STATE_UNUSED;
		t4_os_lock_init(&d->l2tab[i].lock);
		d->l2tab[i].refcnt = 0;
	}

	return d;
}

/**
 * Cleanup L2 Table
 */
void t4_cleanup_l2t(struct adapter *adap)
{
	if (adap->l2t)
		t4_os_free(adap->l2t);
}
