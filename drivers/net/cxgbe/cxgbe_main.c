/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Chelsio Communications.
 * All rights reserved.
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>
#include <netinet/in.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_memory.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_alarm.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_random.h>
#include <rte_dev.h>
#include <rte_kvargs.h>

#include "base/common.h"
#include "base/t4_regs.h"
#include "base/t4_msg.h"
#include "cxgbe.h"
#include "cxgbe_pfvf.h"
#include "clip_tbl.h"
#include "l2t.h"
#include "smt.h"
#include "mps_tcam.h"

static const u16 cxgbe_filter_mode_features[] = {
	(F_FRAGMENTATION | F_MPSHITTYPE | F_MACMATCH | F_ETHERTYPE |
	 F_PROTOCOL | F_PORT),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_MACMATCH | F_ETHERTYPE |
	 F_PROTOCOL | F_FCOE),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_MACMATCH | F_ETHERTYPE | F_TOS |
	 F_PORT),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_MACMATCH | F_ETHERTYPE | F_TOS |
	 F_FCOE),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_MACMATCH | F_ETHERTYPE | F_PORT |
	 F_FCOE),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_MACMATCH | F_PROTOCOL | F_TOS |
	 F_PORT | F_FCOE),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_MACMATCH | F_PROTOCOL | F_VLAN |
	 F_FCOE),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_MACMATCH | F_PROTOCOL | F_VNIC_ID |
	 F_FCOE),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_MACMATCH | F_TOS | F_VLAN |
	 F_FCOE),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_MACMATCH | F_TOS | F_VNIC_ID |
	 F_FCOE),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_MACMATCH | F_VLAN | F_PORT |
	 F_FCOE),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_MACMATCH | F_VNIC_ID | F_PORT |
	 F_FCOE),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_ETHERTYPE | F_PROTOCOL | F_TOS |
	 F_PORT | F_FCOE),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_ETHERTYPE | F_VLAN | F_PORT),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_ETHERTYPE | F_VLAN | F_FCOE),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_ETHERTYPE | F_VNIC_ID | F_PORT),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_ETHERTYPE | F_VNIC_ID | F_FCOE),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_PROTOCOL | F_TOS | F_VLAN | F_PORT),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_PROTOCOL | F_TOS | F_VLAN | F_FCOE),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_PROTOCOL | F_TOS | F_VNIC_ID |
	 F_PORT),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_PROTOCOL | F_TOS | F_VNIC_ID |
	 F_FCOE),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_PROTOCOL | F_VLAN | F_PORT |
	 F_FCOE),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_PROTOCOL | F_VNIC_ID | F_PORT |
	 F_FCOE),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_TOS | F_VLAN | F_PORT | F_FCOE),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_TOS | F_VNIC_ID | F_PORT | F_FCOE),
	(F_FRAGMENTATION | F_MPSHITTYPE | F_VLAN | F_VNIC_ID | F_FCOE),
	(F_FRAGMENTATION | F_MACMATCH | F_ETHERTYPE | F_PROTOCOL | F_PORT |
	 F_FCOE),
	(F_FRAGMENTATION | F_MACMATCH | F_ETHERTYPE | F_TOS | F_PORT | F_FCOE),
	(F_FRAGMENTATION | F_MACMATCH | F_PROTOCOL | F_VLAN | F_PORT | F_FCOE),
	(F_FRAGMENTATION | F_MACMATCH | F_PROTOCOL | F_VNIC_ID | F_PORT |
	 F_FCOE),
	(F_FRAGMENTATION | F_MACMATCH | F_TOS | F_VLAN | F_PORT | F_FCOE),
	(F_FRAGMENTATION | F_MACMATCH | F_TOS | F_VNIC_ID | F_PORT | F_FCOE),
	(F_FRAGMENTATION | F_ETHERTYPE | F_VLAN | F_PORT | F_FCOE),
	(F_FRAGMENTATION | F_ETHERTYPE | F_VNIC_ID | F_PORT | F_FCOE),
	(F_FRAGMENTATION | F_PROTOCOL | F_TOS | F_VLAN | F_FCOE),
	(F_FRAGMENTATION | F_PROTOCOL | F_TOS | F_VNIC_ID | F_FCOE),
	(F_FRAGMENTATION | F_VLAN | F_VNIC_ID | F_PORT | F_FCOE),
	(F_MPSHITTYPE | F_MACMATCH | F_ETHERTYPE | F_PROTOCOL | F_PORT |
	 F_FCOE),
	(F_MPSHITTYPE | F_MACMATCH | F_ETHERTYPE | F_TOS | F_PORT | F_FCOE),
	(F_MPSHITTYPE | F_MACMATCH | F_PROTOCOL | F_VLAN | F_PORT),
	(F_MPSHITTYPE | F_MACMATCH | F_PROTOCOL | F_VNIC_ID | F_PORT),
	(F_MPSHITTYPE | F_MACMATCH | F_TOS | F_VLAN | F_PORT),
	(F_MPSHITTYPE | F_MACMATCH | F_TOS | F_VNIC_ID | F_PORT),
	(F_MPSHITTYPE | F_ETHERTYPE | F_VLAN | F_PORT | F_FCOE),
	(F_MPSHITTYPE | F_ETHERTYPE | F_VNIC_ID | F_PORT | F_FCOE),
	(F_MPSHITTYPE | F_PROTOCOL | F_TOS | F_VLAN | F_PORT | F_FCOE),
	(F_MPSHITTYPE | F_PROTOCOL | F_TOS | F_VNIC_ID | F_PORT | F_FCOE),
	(F_MPSHITTYPE | F_VLAN | F_VNIC_ID | F_PORT),
};

/**
 * Allocate a chunk of memory. The allocated memory is cleared.
 */
void *t4_alloc_mem(size_t size)
{
	return rte_zmalloc(NULL, size, 0);
}

/**
 * Free memory allocated through t4_alloc_mem().
 */
void t4_free_mem(void *addr)
{
	rte_free(addr);
}

/*
 * Response queue handler for the FW event queue.
 */
static int fwevtq_handler(struct sge_rspq *q, const __be64 *rsp,
			  __rte_unused const struct pkt_gl *gl)
{
	u8 opcode = ((const struct rss_header *)rsp)->opcode;

	rsp++;                                          /* skip RSS header */

	/*
	 * FW can send EGR_UPDATEs encapsulated in a CPL_FW4_MSG.
	 */
	if (unlikely(opcode == CPL_FW4_MSG &&
		     ((const struct cpl_fw4_msg *)rsp)->type ==
		      FW_TYPE_RSSCPL)) {
		rsp++;
		opcode = ((const struct rss_header *)rsp)->opcode;
		rsp++;
		if (opcode != CPL_SGE_EGR_UPDATE) {
			dev_err(q->adapter, "unexpected FW4/CPL %#x on FW event queue\n",
				opcode);
			goto out;
		}
	}

	if (likely(opcode == CPL_SGE_EGR_UPDATE)) {
		/* do nothing */
	} else if (opcode == CPL_FW6_MSG || opcode == CPL_FW4_MSG) {
		const struct cpl_fw6_msg *msg = (const void *)rsp;

		t4_handle_fw_rpl(q->adapter, msg->data);
	} else if (opcode == CPL_ABORT_RPL_RSS) {
		const struct cpl_abort_rpl_rss *p = (const void *)rsp;

		cxgbe_hash_del_filter_rpl(q->adapter, p);
	} else if (opcode == CPL_SET_TCB_RPL) {
		const struct cpl_set_tcb_rpl *p = (const void *)rsp;

		cxgbe_filter_rpl(q->adapter, p);
	} else if (opcode == CPL_ACT_OPEN_RPL) {
		const struct cpl_act_open_rpl *p = (const void *)rsp;

		cxgbe_hash_filter_rpl(q->adapter, p);
	} else if (opcode == CPL_L2T_WRITE_RPL) {
		const struct cpl_l2t_write_rpl *p = (const void *)rsp;

		cxgbe_do_l2t_write_rpl(q->adapter, p);
	} else if (opcode == CPL_SMT_WRITE_RPL) {
		const struct cpl_smt_write_rpl *p = (const void *)rsp;

		cxgbe_do_smt_write_rpl(q->adapter, p);
	} else {
		dev_err(adapter, "unexpected CPL %#x on FW event queue\n",
			opcode);
	}
out:
	return 0;
}

/**
 * Setup sge control queues to pass control information.
 */
int cxgbe_setup_sge_ctrl_txq(struct adapter *adapter)
{
	struct sge *s = &adapter->sge;
	int err = 0, i = 0;

	for_each_port(adapter, i) {
		struct port_info *pi = adap2pinfo(adapter, i);
		char name[RTE_ETH_NAME_MAX_LEN];
		struct sge_ctrl_txq *q = &s->ctrlq[i];

		q->q.size = 1024;
		err = t4_sge_alloc_ctrl_txq(adapter, q,
					    adapter->eth_dev,  i,
					    s->fw_evtq.cntxt_id,
					    rte_socket_id());
		if (err) {
			dev_err(adapter, "Failed to alloc ctrl txq. Err: %d",
				err);
			goto out;
		}
		snprintf(name, sizeof(name), "%s_ctrl_pool_%d",
			 pi->eth_dev->device->driver->name,
			 pi->eth_dev->data->port_id);
		q->mb_pool = rte_pktmbuf_pool_create(name, s->ctrlq[i].q.size,
						     RTE_CACHE_LINE_SIZE,
						     RTE_MBUF_PRIV_ALIGN,
						     RTE_MBUF_DEFAULT_BUF_SIZE,
						     SOCKET_ID_ANY);
		if (!q->mb_pool) {
			err = -rte_errno;
			dev_err(adapter,
				"Can't create ctrl pool for port %d. Err: %d\n",
				pi->eth_dev->data->port_id, err);
			goto out;
		}
	}
	return 0;
out:
	t4_free_sge_resources(adapter);
	return err;
}

/**
 * cxgbe_poll_for_completion: Poll rxq for completion
 * @q: rxq to poll
 * @ms: milliseconds to delay
 * @cnt: number of times to poll
 * @c: completion to check for 'done' status
 *
 * Polls the rxq for reples until completion is done or the count
 * expires.
 */
int cxgbe_poll_for_completion(struct sge_rspq *q, unsigned int ms,
			      unsigned int cnt, struct t4_completion *c)
{
	unsigned int i;
	unsigned int work_done, budget = 32;

	if (!c)
		return -EINVAL;

	for (i = 0; i < cnt; i++) {
		cxgbe_poll(q, NULL, budget, &work_done);
		t4_os_lock(&c->lock);
		if (c->done) {
			t4_os_unlock(&c->lock);
			return 0;
		}
		t4_os_unlock(&c->lock);
		rte_delay_ms(ms);
	}
	return -ETIMEDOUT;
}

int cxgbe_setup_sge_fwevtq(struct adapter *adapter)
{
	struct sge *s = &adapter->sge;
	int err = 0;
	int msi_idx = 0;

	err = t4_sge_alloc_rxq(adapter, &s->fw_evtq, true, adapter->eth_dev,
			       msi_idx, NULL, fwevtq_handler, -1, NULL, 0,
			       rte_socket_id());
	return err;
}

static int closest_timer(const struct sge *s, int time)
{
	unsigned int i, match = 0;
	int delta, min_delta = INT_MAX;

	for (i = 0; i < ARRAY_SIZE(s->timer_val); i++) {
		delta = time - s->timer_val[i];
		if (delta < 0)
			delta = -delta;
		if (delta < min_delta) {
			min_delta = delta;
			match = i;
		}
	}
	return match;
}

static int closest_thres(const struct sge *s, int thres)
{
	unsigned int i, match = 0;
	int delta, min_delta = INT_MAX;

	for (i = 0; i < ARRAY_SIZE(s->counter_val); i++) {
		delta = thres - s->counter_val[i];
		if (delta < 0)
			delta = -delta;
		if (delta < min_delta) {
			min_delta = delta;
			match = i;
		}
	}
	return match;
}

/**
 * cxgb4_set_rspq_intr_params - set a queue's interrupt holdoff parameters
 * @q: the Rx queue
 * @us: the hold-off time in us, or 0 to disable timer
 * @cnt: the hold-off packet count, or 0 to disable counter
 *
 * Sets an Rx queue's interrupt hold-off time and packet count.  At least
 * one of the two needs to be enabled for the queue to generate interrupts.
 */
int cxgb4_set_rspq_intr_params(struct sge_rspq *q, unsigned int us,
			       unsigned int cnt)
{
	struct adapter *adap = q->adapter;
	unsigned int timer_val;

	if (cnt) {
		int err;
		u32 v, new_idx;

		new_idx = closest_thres(&adap->sge, cnt);
		if (q->desc && q->pktcnt_idx != new_idx) {
			/* the queue has already been created, update it */
			v = V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DMAQ) |
			    V_FW_PARAMS_PARAM_X(
			    FW_PARAMS_PARAM_DMAQ_IQ_INTCNTTHRESH) |
			    V_FW_PARAMS_PARAM_YZ(q->cntxt_id);
			err = t4_set_params(adap, adap->mbox, adap->pf, 0, 1,
					    &v, &new_idx);
			if (err)
				return err;
		}
		q->pktcnt_idx = new_idx;
	}

	timer_val = (us == 0) ? X_TIMERREG_RESTART_COUNTER :
				closest_timer(&adap->sge, us);

	if ((us | cnt) == 0)
		q->intr_params = V_QINTR_TIMER_IDX(X_TIMERREG_UPDATE_CIDX);
	else
		q->intr_params = V_QINTR_TIMER_IDX(timer_val) |
				 V_QINTR_CNT_EN(cnt > 0);
	return 0;
}

/**
 * Allocate an active-open TID and set it to the supplied value.
 */
int cxgbe_alloc_atid(struct tid_info *t, void *data)
{
	int atid = -1;

	t4_os_lock(&t->atid_lock);
	if (t->afree) {
		union aopen_entry *p = t->afree;

		atid = p - t->atid_tab;
		t->afree = p->next;
		p->data = data;
		t->atids_in_use++;
	}
	t4_os_unlock(&t->atid_lock);
	return atid;
}

/**
 * Release an active-open TID.
 */
void cxgbe_free_atid(struct tid_info *t, unsigned int atid)
{
	union aopen_entry *p = &t->atid_tab[atid];

	t4_os_lock(&t->atid_lock);
	p->next = t->afree;
	t->afree = p;
	t->atids_in_use--;
	t4_os_unlock(&t->atid_lock);
}

/**
 * Populate a TID_RELEASE WR.  Caller must properly size the skb.
 */
static void mk_tid_release(struct rte_mbuf *mbuf, unsigned int tid)
{
	struct cpl_tid_release *req;

	req = rte_pktmbuf_mtod(mbuf, struct cpl_tid_release *);
	INIT_TP_WR_MIT_CPL(req, CPL_TID_RELEASE, tid);
}

/**
 * Release a TID and inform HW.  If we are unable to allocate the release
 * message we defer to a work queue.
 */
void cxgbe_remove_tid(struct tid_info *t, unsigned int chan, unsigned int tid,
		      unsigned short family)
{
	struct rte_mbuf *mbuf;
	struct adapter *adap = container_of(t, struct adapter, tids);

	WARN_ON(tid >= t->ntids);

	if (t->tid_tab[tid]) {
		t->tid_tab[tid] = NULL;
		rte_atomic32_dec(&t->conns_in_use);
		if (t->hash_base && tid >= t->hash_base) {
			if (family == FILTER_TYPE_IPV4)
				rte_atomic32_dec(&t->hash_tids_in_use);
		} else {
			if (family == FILTER_TYPE_IPV4)
				rte_atomic32_dec(&t->tids_in_use);
		}
	}

	mbuf = rte_pktmbuf_alloc((&adap->sge.ctrlq[chan])->mb_pool);
	if (mbuf) {
		mbuf->data_len = sizeof(struct cpl_tid_release);
		mbuf->pkt_len = mbuf->data_len;
		mk_tid_release(mbuf, tid);
		t4_mgmt_tx(&adap->sge.ctrlq[chan], mbuf);
	}
}

/**
 * Insert a TID.
 */
void cxgbe_insert_tid(struct tid_info *t, void *data, unsigned int tid,
		      unsigned short family)
{
	t->tid_tab[tid] = data;
	if (t->hash_base && tid >= t->hash_base) {
		if (family == FILTER_TYPE_IPV4)
			rte_atomic32_inc(&t->hash_tids_in_use);
	} else {
		if (family == FILTER_TYPE_IPV4)
			rte_atomic32_inc(&t->tids_in_use);
	}

	rte_atomic32_inc(&t->conns_in_use);
}

/**
 * Free TID tables.
 */
static void tid_free(struct tid_info *t)
{
	if (t->tid_tab) {
		if (t->ftid_bmap)
			rte_bitmap_free(t->ftid_bmap);

		if (t->ftid_bmap_array)
			t4_os_free(t->ftid_bmap_array);

		t4_os_free(t->tid_tab);
	}

	memset(t, 0, sizeof(struct tid_info));
}

/**
 * Allocate and initialize the TID tables.  Returns 0 on success.
 */
static int tid_init(struct tid_info *t)
{
	size_t size;
	unsigned int ftid_bmap_size;
	unsigned int natids = t->natids;
	unsigned int max_ftids = t->nftids;

	ftid_bmap_size = rte_bitmap_get_memory_footprint(t->nftids);
	size = t->ntids * sizeof(*t->tid_tab) +
		max_ftids * sizeof(*t->ftid_tab) +
		natids * sizeof(*t->atid_tab);

	t->tid_tab = t4_os_alloc(size);
	if (!t->tid_tab)
		return -ENOMEM;

	t->atid_tab = (union aopen_entry *)&t->tid_tab[t->ntids];
	t->ftid_tab = (struct filter_entry *)&t->atid_tab[t->natids];
	t->ftid_bmap_array = t4_os_alloc(ftid_bmap_size);
	if (!t->ftid_bmap_array) {
		tid_free(t);
		return -ENOMEM;
	}

	t4_os_lock_init(&t->atid_lock);
	t4_os_lock_init(&t->ftid_lock);

	t->afree = NULL;
	t->atids_in_use = 0;
	rte_atomic32_init(&t->tids_in_use);
	rte_atomic32_set(&t->tids_in_use, 0);
	rte_atomic32_init(&t->conns_in_use);
	rte_atomic32_set(&t->conns_in_use, 0);

	/* Setup the free list for atid_tab and clear the stid bitmap. */
	if (natids) {
		while (--natids)
			t->atid_tab[natids - 1].next = &t->atid_tab[natids];
		t->afree = t->atid_tab;
	}

	t->ftid_bmap = rte_bitmap_init(t->nftids, t->ftid_bmap_array,
				       ftid_bmap_size);
	if (!t->ftid_bmap) {
		tid_free(t);
		return -ENOMEM;
	}

	return 0;
}

static inline bool is_x_1g_port(const struct link_config *lc)
{
	return (lc->pcaps & FW_PORT_CAP32_SPEED_1G) != 0;
}

static inline bool is_x_10g_port(const struct link_config *lc)
{
	unsigned int speeds, high_speeds;

	speeds = V_FW_PORT_CAP32_SPEED(G_FW_PORT_CAP32_SPEED(lc->pcaps));
	high_speeds = speeds &
		      ~(FW_PORT_CAP32_SPEED_100M | FW_PORT_CAP32_SPEED_1G);

	return high_speeds != 0;
}

static inline void init_rspq(struct adapter *adap, struct sge_rspq *q,
		      unsigned int us, unsigned int cnt,
		      unsigned int size, unsigned int iqe_size)
{
	q->adapter = adap;
	cxgb4_set_rspq_intr_params(q, us, cnt);
	q->iqe_len = iqe_size;
	q->size = size;
}

int cxgbe_cfg_queue_count(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = eth_dev->data->dev_private;
	struct adapter *adap = pi->adapter;
	struct sge *s = &adap->sge;
	unsigned int max_queues = s->max_ethqsets / adap->params.nports;

	if ((eth_dev->data->nb_rx_queues < 1) ||
	    (eth_dev->data->nb_tx_queues < 1))
		return -EINVAL;

	if ((eth_dev->data->nb_rx_queues > max_queues) ||
	    (eth_dev->data->nb_tx_queues > max_queues))
		return -EINVAL;

	if (eth_dev->data->nb_rx_queues > pi->rss_size)
		return -EINVAL;

	/* We must configure RSS, since config has changed*/
	pi->flags &= ~PORT_RSS_DONE;

	pi->n_rx_qsets = eth_dev->data->nb_rx_queues;
	pi->n_tx_qsets = eth_dev->data->nb_tx_queues;

	return 0;
}

void cxgbe_cfg_queues(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = eth_dev->data->dev_private;
	struct adapter *adap = pi->adapter;
	struct sge *s = &adap->sge;
	unsigned int i, nb_ports = 0, qidx = 0;
	unsigned int q_per_port = 0;

	if (!(adap->flags & CFG_QUEUES)) {
		for_each_port(adap, i) {
			struct port_info *tpi = adap2pinfo(adap, i);

			nb_ports += (is_x_10g_port(&tpi->link_cfg)) ||
				     is_x_1g_port(&tpi->link_cfg) ? 1 : 0;
		}

		/*
		 * We default up to # of cores queues per 1G/10G port.
		 */
		if (nb_ports)
			q_per_port = (s->max_ethqsets -
				     (adap->params.nports - nb_ports)) /
				     nb_ports;

		if (q_per_port > rte_lcore_count())
			q_per_port = rte_lcore_count();

		for_each_port(adap, i) {
			struct port_info *pi = adap2pinfo(adap, i);

			pi->first_qset = qidx;

			/* Initially n_rx_qsets == n_tx_qsets */
			pi->n_rx_qsets = (is_x_10g_port(&pi->link_cfg) ||
					  is_x_1g_port(&pi->link_cfg)) ?
					  q_per_port : 1;
			pi->n_tx_qsets = pi->n_rx_qsets;

			if (pi->n_rx_qsets > pi->rss_size)
				pi->n_rx_qsets = pi->rss_size;

			qidx += pi->n_rx_qsets;
		}

		for (i = 0; i < ARRAY_SIZE(s->ethrxq); i++) {
			struct sge_eth_rxq *r = &s->ethrxq[i];

			init_rspq(adap, &r->rspq, 5, 32, 1024, 64);
			r->usembufs = 1;
			r->fl.size = (r->usembufs ? 1024 : 72);
		}

		for (i = 0; i < ARRAY_SIZE(s->ethtxq); i++)
			s->ethtxq[i].q.size = 1024;

		init_rspq(adap, &adap->sge.fw_evtq, 0, 0, 1024, 64);
		adap->flags |= CFG_QUEUES;
	}
}

void cxgbe_stats_get(struct port_info *pi, struct port_stats *stats)
{
	t4_get_port_stats_offset(pi->adapter, pi->tx_chan, stats,
				 &pi->stats_base);
}

void cxgbe_stats_reset(struct port_info *pi)
{
	t4_clr_port_stats(pi->adapter, pi->tx_chan);
}

static void setup_memwin(struct adapter *adap)
{
	u32 mem_win0_base;

	/* For T5, only relative offset inside the PCIe BAR is passed */
	mem_win0_base = MEMWIN0_BASE;

	/*
	 * Set up memory window for accessing adapter memory ranges.  (Read
	 * back MA register to ensure that changes propagate before we attempt
	 * to use the new values.)
	 */
	t4_write_reg(adap,
		     PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_BASE_WIN,
					 MEMWIN_NIC),
		     mem_win0_base | V_BIR(0) |
		     V_WINDOW(ilog2(MEMWIN0_APERTURE) - X_WINDOW_SHIFT));
	t4_read_reg(adap,
		    PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_BASE_WIN,
					MEMWIN_NIC));
}

int cxgbe_init_rss(struct adapter *adap)
{
	unsigned int i;

	if (is_pf4(adap)) {
		int err;

		err = t4_init_rss_mode(adap, adap->mbox);
		if (err)
			return err;
	}

	for_each_port(adap, i) {
		struct port_info *pi = adap2pinfo(adap, i);

		pi->rss = rte_zmalloc(NULL, pi->rss_size * sizeof(u16), 0);
		if (!pi->rss)
			return -ENOMEM;

		pi->rss_hf = CXGBE_RSS_HF_ALL;
	}
	return 0;
}

/**
 * Dump basic information about the adapter.
 */
void cxgbe_print_adapter_info(struct adapter *adap)
{
	/**
	 * Hardware/Firmware/etc. Version/Revision IDs.
	 */
	t4_dump_version_info(adap);
}

void cxgbe_print_port_info(struct adapter *adap)
{
	int i;
	char buf[80];
	struct rte_pci_addr *loc = &adap->pdev->addr;

	for_each_port(adap, i) {
		const struct port_info *pi = adap2pinfo(adap, i);
		char *bufp = buf;

		if (pi->link_cfg.pcaps & FW_PORT_CAP32_SPEED_100M)
			bufp += sprintf(bufp, "100M/");
		if (pi->link_cfg.pcaps & FW_PORT_CAP32_SPEED_1G)
			bufp += sprintf(bufp, "1G/");
		if (pi->link_cfg.pcaps & FW_PORT_CAP32_SPEED_10G)
			bufp += sprintf(bufp, "10G/");
		if (pi->link_cfg.pcaps & FW_PORT_CAP32_SPEED_25G)
			bufp += sprintf(bufp, "25G/");
		if (pi->link_cfg.pcaps & FW_PORT_CAP32_SPEED_40G)
			bufp += sprintf(bufp, "40G/");
		if (pi->link_cfg.pcaps & FW_PORT_CAP32_SPEED_50G)
			bufp += sprintf(bufp, "50G/");
		if (pi->link_cfg.pcaps & FW_PORT_CAP32_SPEED_100G)
			bufp += sprintf(bufp, "100G/");
		if (bufp != buf)
			--bufp;
		sprintf(bufp, "BASE-%s",
			t4_get_port_type_description(
					(enum fw_port_type)pi->port_type));

		dev_info(adap,
			 " " PCI_PRI_FMT " Chelsio rev %d %s %s\n",
			 loc->domain, loc->bus, loc->devid, loc->function,
			 CHELSIO_CHIP_RELEASE(adap->params.chip), buf,
			 (adap->flags & USING_MSIX) ? " MSI-X" :
			 (adap->flags & USING_MSI) ? " MSI" : "");
	}
}

static int check_devargs_handler(const char *key, const char *value, void *p)
{
	if (!strncmp(key, CXGBE_DEVARG_CMN_KEEP_OVLAN, strlen(key)) ||
	    !strncmp(key, CXGBE_DEVARG_CMN_TX_MODE_LATENCY, strlen(key)) ||
	    !strncmp(key, CXGBE_DEVARG_VF_FORCE_LINK_UP, strlen(key))) {
		if (!strncmp(value, "1", 1)) {
			bool *dst_val = (bool *)p;

			*dst_val = true;
		}
	}

	if (!strncmp(key, CXGBE_DEVARG_PF_FILTER_MODE, strlen(key)) ||
	    !strncmp(key, CXGBE_DEVARG_PF_FILTER_MASK, strlen(key))) {
		u32 *dst_val = (u32 *)p;
		char *endptr = NULL;
		u32 arg_val;

		arg_val = strtoul(value, &endptr, 16);
		if (errno || endptr == value)
			return -EINVAL;

		*dst_val = arg_val;
	}

	return 0;
}

static int cxgbe_get_devargs(struct rte_devargs *devargs, const char *key,
			     void *p)
{
	struct rte_kvargs *kvlist;
	int ret = 0;

	if (!devargs)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (!kvlist)
		return 0;

	if (!rte_kvargs_count(kvlist, key))
		goto out;

	ret = rte_kvargs_process(kvlist, key, check_devargs_handler, p);

out:
	rte_kvargs_free(kvlist);

	return ret;
}

static void cxgbe_get_devargs_int(struct adapter *adap, bool *dst,
				  const char *key, bool default_value)
{
	struct rte_pci_device *pdev = adap->pdev;
	int ret;
	bool devarg_value = default_value;

	*dst = default_value;
	if (!pdev)
		return;

	ret = cxgbe_get_devargs(pdev->device.devargs, key, &devarg_value);
	if (ret)
		return;

	*dst = devarg_value;
}

static void cxgbe_get_devargs_u32(struct adapter *adap, u32 *dst,
				  const char *key, u32 default_value)
{
	struct rte_pci_device *pdev = adap->pdev;
	u32 devarg_value = default_value;
	int ret;

	*dst = default_value;
	if (!pdev)
		return;

	ret = cxgbe_get_devargs(pdev->device.devargs, key, &devarg_value);
	if (ret)
		return;

	*dst = devarg_value;
}

void cxgbe_process_devargs(struct adapter *adap)
{
	cxgbe_get_devargs_int(adap, &adap->devargs.keep_ovlan,
			      CXGBE_DEVARG_CMN_KEEP_OVLAN, false);
	cxgbe_get_devargs_int(adap, &adap->devargs.tx_mode_latency,
			      CXGBE_DEVARG_CMN_TX_MODE_LATENCY, false);
	cxgbe_get_devargs_int(adap, &adap->devargs.force_link_up,
			      CXGBE_DEVARG_VF_FORCE_LINK_UP, false);
	cxgbe_get_devargs_u32(adap, &adap->devargs.filtermode,
			      CXGBE_DEVARG_PF_FILTER_MODE, 0);
	cxgbe_get_devargs_u32(adap, &adap->devargs.filtermask,
			      CXGBE_DEVARG_PF_FILTER_MASK, 0);
}

static void configure_vlan_types(struct adapter *adapter)
{
	int i;

	for_each_port(adapter, i) {
		/* OVLAN Type 0x88a8 */
		t4_set_reg_field(adapter, MPS_PORT_RX_OVLAN_REG(i, A_RX_OVLAN0),
				 V_OVLAN_MASK(M_OVLAN_MASK) |
				 V_OVLAN_ETYPE(M_OVLAN_ETYPE),
				 V_OVLAN_MASK(M_OVLAN_MASK) |
				 V_OVLAN_ETYPE(0x88a8));
		/* OVLAN Type 0x9100 */
		t4_set_reg_field(adapter, MPS_PORT_RX_OVLAN_REG(i, A_RX_OVLAN1),
				 V_OVLAN_MASK(M_OVLAN_MASK) |
				 V_OVLAN_ETYPE(M_OVLAN_ETYPE),
				 V_OVLAN_MASK(M_OVLAN_MASK) |
				 V_OVLAN_ETYPE(0x9100));

		/* IVLAN 0X8100 */
		t4_set_reg_field(adapter, MPS_PORT_RX_IVLAN(i),
				 V_IVLAN_ETYPE(M_IVLAN_ETYPE),
				 V_IVLAN_ETYPE(0x8100));

		t4_set_reg_field(adapter, MPS_PORT_RX_CTL(i),
				 F_OVLAN_EN0 | F_OVLAN_EN1 |
				 F_IVLAN_EN,
				 F_OVLAN_EN0 | F_OVLAN_EN1 |
				 F_IVLAN_EN);
	}

	t4_tp_wr_bits_indirect(adapter, A_TP_INGRESS_CONFIG, V_RM_OVLAN(1),
			       V_RM_OVLAN(!adapter->devargs.keep_ovlan));
}

static int cxgbe_get_filter_vnic_mode_from_devargs(u32 val)
{
	u32 vnic_mode;

	vnic_mode = val & (CXGBE_DEVARGS_FILTER_MODE_PF_VF |
			   CXGBE_DEVARGS_FILTER_MODE_VLAN_OUTER);
	if (vnic_mode) {
		switch (vnic_mode) {
		case CXGBE_DEVARGS_FILTER_MODE_VLAN_OUTER:
			return CXGBE_FILTER_VNIC_MODE_OVLAN;
		case CXGBE_DEVARGS_FILTER_MODE_PF_VF:
			return CXGBE_FILTER_VNIC_MODE_PFVF;
		default:
			return -EINVAL;
		}
	}

	return CXGBE_FILTER_VNIC_MODE_NONE;
}

static int cxgbe_get_filter_mode_from_devargs(u32 val, bool closest_match)
{
	int vnic_mode, fmode = 0;
	bool found = false;
	u8 i;

	if (val >= CXGBE_DEVARGS_FILTER_MODE_MAX) {
		pr_err("Unsupported flags set in filter mode. Must be < 0x%x\n",
		       CXGBE_DEVARGS_FILTER_MODE_MAX);
		return -ERANGE;
	}

	vnic_mode = cxgbe_get_filter_vnic_mode_from_devargs(val);
	if (vnic_mode < 0) {
		pr_err("Unsupported Vnic-mode, more than 1 Vnic-mode selected\n");
		return vnic_mode;
	}

	if (vnic_mode)
		fmode |= F_VNIC_ID;
	if (val & CXGBE_DEVARGS_FILTER_MODE_PHYSICAL_PORT)
		fmode |= F_PORT;
	if (val & CXGBE_DEVARGS_FILTER_MODE_ETHERNET_DSTMAC)
		fmode |= F_MACMATCH;
	if (val & CXGBE_DEVARGS_FILTER_MODE_ETHERNET_ETHTYPE)
		fmode |= F_ETHERTYPE;
	if (val & CXGBE_DEVARGS_FILTER_MODE_VLAN_INNER)
		fmode |= F_VLAN;
	if (val & CXGBE_DEVARGS_FILTER_MODE_IP_TOS)
		fmode |= F_TOS;
	if (val & CXGBE_DEVARGS_FILTER_MODE_IP_PROTOCOL)
		fmode |= F_PROTOCOL;

	for (i = 0; i < ARRAY_SIZE(cxgbe_filter_mode_features); i++) {
		if ((cxgbe_filter_mode_features[i] & fmode) == fmode) {
			found = true;
			break;
		}
	}

	if (!found)
		return -EINVAL;

	return closest_match ? cxgbe_filter_mode_features[i] : fmode;
}

static int configure_filter_mode_mask(struct adapter *adap)
{
	u32 params[2], val[2], nparams = 0;
	int ret;

	if (!adap->devargs.filtermode && !adap->devargs.filtermask)
		return 0;

	if (!adap->devargs.filtermode || !adap->devargs.filtermask) {
		pr_err("Unsupported, Provide both filtermode and filtermask devargs\n");
		return -EINVAL;
	}

	if (adap->devargs.filtermask & ~adap->devargs.filtermode) {
		pr_err("Unsupported, filtermask (0x%x) must be subset of filtermode (0x%x)\n",
		       adap->devargs.filtermask, adap->devargs.filtermode);

		return -EINVAL;
	}

	params[0] = CXGBE_FW_PARAM_DEV(FILTER) |
		    V_FW_PARAMS_PARAM_Y(FW_PARAM_DEV_FILTER_MODE_MASK);

	ret = cxgbe_get_filter_mode_from_devargs(adap->devargs.filtermode,
						 true);
	if (ret < 0) {
		pr_err("Unsupported filtermode devargs combination:0x%x\n",
		       adap->devargs.filtermode);
		return ret;
	}

	val[0] = V_FW_PARAMS_PARAM_FILTER_MODE(ret);

	ret = cxgbe_get_filter_mode_from_devargs(adap->devargs.filtermask,
						 false);
	if (ret < 0) {
		pr_err("Unsupported filtermask devargs combination:0x%x\n",
		       adap->devargs.filtermask);
		return ret;
	}

	val[0] |= V_FW_PARAMS_PARAM_FILTER_MASK(ret);

	nparams++;

	ret = cxgbe_get_filter_vnic_mode_from_devargs(adap->devargs.filtermode);
	if (ret < 0)
		return ret;

	if (ret) {
		params[1] = CXGBE_FW_PARAM_DEV(FILTER) |
			    V_FW_PARAMS_PARAM_Y(FW_PARAM_DEV_FILTER_VNIC_MODE);

		val[1] = ret - 1;

		nparams++;
	}

	return t4_set_params(adap, adap->mbox, adap->pf, 0, nparams,
			     params, val);
}

static void configure_pcie_ext_tag(struct adapter *adapter)
{
	u16 v;
	int pos = t4_os_find_pci_capability(adapter, PCI_CAP_ID_EXP);

	if (!pos)
		return;

	if (pos > 0) {
		t4_os_pci_read_cfg2(adapter, pos + PCI_EXP_DEVCTL, &v);
		v |= PCI_EXP_DEVCTL_EXT_TAG;
		t4_os_pci_write_cfg2(adapter, pos + PCI_EXP_DEVCTL, v);
		if (is_t6(adapter->params.chip)) {
			t4_set_reg_field(adapter, A_PCIE_CFG2,
					 V_T6_TOTMAXTAG(M_T6_TOTMAXTAG),
					 V_T6_TOTMAXTAG(7));
			t4_set_reg_field(adapter, A_PCIE_CMD_CFG,
					 V_T6_MINTAG(M_T6_MINTAG),
					 V_T6_MINTAG(8));
		} else {
			t4_set_reg_field(adapter, A_PCIE_CFG2,
					 V_TOTMAXTAG(M_TOTMAXTAG),
					 V_TOTMAXTAG(3));
			t4_set_reg_field(adapter, A_PCIE_CMD_CFG,
					 V_MINTAG(M_MINTAG),
					 V_MINTAG(8));
		}
	}
}

/* Figure out how many Queue Sets we can support */
void cxgbe_configure_max_ethqsets(struct adapter *adapter)
{
	unsigned int ethqsets;

	/*
	 * We need to reserve an Ingress Queue for the Asynchronous Firmware
	 * Event Queue.
	 *
	 * For each Queue Set, we'll need the ability to allocate two Egress
	 * Contexts -- one for the Ingress Queue Free List and one for the TX
	 * Ethernet Queue.
	 */
	if (is_pf4(adapter)) {
		struct pf_resources *pfres = &adapter->params.pfres;

		ethqsets = pfres->niqflint - 1;
		if (pfres->neq < ethqsets * 2)
			ethqsets = pfres->neq / 2;
	} else {
		struct vf_resources *vfres = &adapter->params.vfres;

		ethqsets = vfres->niqflint - 1;
		if (vfres->nethctrl != ethqsets)
			ethqsets = min(vfres->nethctrl, ethqsets);
		if (vfres->neq < ethqsets * 2)
			ethqsets = vfres->neq / 2;
	}

	if (ethqsets > MAX_ETH_QSETS)
		ethqsets = MAX_ETH_QSETS;
	adapter->sge.max_ethqsets = ethqsets;
}

/*
 * Tweak configuration based on system architecture, etc.  Most of these have
 * defaults assigned to them by Firmware Configuration Files (if we're using
 * them) but need to be explicitly set if we're using hard-coded
 * initialization. So these are essentially common tweaks/settings for
 * Configuration Files and hard-coded initialization ...
 */
static int adap_init0_tweaks(struct adapter *adapter)
{
	u8 rx_dma_offset;

	/*
	 * Fix up various Host-Dependent Parameters like Page Size, Cache
	 * Line Size, etc.  The firmware default is for a 4KB Page Size and
	 * 64B Cache Line Size ...
	 */
	t4_fixup_host_params_compat(adapter, CXGBE_PAGE_SIZE, L1_CACHE_BYTES,
				    T5_LAST_REV);

	/*
	 * Keep the chip default offset to deliver Ingress packets into our
	 * DMA buffers to zero
	 */
	rx_dma_offset = 0;
	t4_set_reg_field(adapter, A_SGE_CONTROL, V_PKTSHIFT(M_PKTSHIFT),
			 V_PKTSHIFT(rx_dma_offset));

	t4_set_reg_field(adapter, A_SGE_FLM_CFG,
			 V_CREDITCNT(M_CREDITCNT) | M_CREDITCNTPACKING,
			 V_CREDITCNT(3) | V_CREDITCNTPACKING(1));

	t4_set_reg_field(adapter, A_SGE_INGRESS_RX_THRESHOLD,
			 V_THRESHOLD_3(M_THRESHOLD_3), V_THRESHOLD_3(32U));

	t4_set_reg_field(adapter, A_SGE_CONTROL2, V_IDMAARBROUNDROBIN(1U),
			 V_IDMAARBROUNDROBIN(1U));

	/*
	 * Don't include the "IP Pseudo Header" in CPL_RX_PKT checksums: Linux
	 * adds the pseudo header itself.
	 */
	t4_tp_wr_bits_indirect(adapter, A_TP_INGRESS_CONFIG,
			       F_CSUM_HAS_PSEUDO_HDR, 0);

	return 0;
}

/*
 * Attempt to initialize the adapter via a Firmware Configuration File.
 */
static int adap_init0_config(struct adapter *adapter, int reset)
{
	u32 finiver, finicsum, cfcsum, param, val;
	struct fw_caps_config_cmd caps_cmd;
	unsigned long mtype = 0, maddr = 0;
	u8 config_issued = 0;
	char config_name[20];
	int cfg_addr, ret;

	/*
	 * Reset device if necessary.
	 */
	if (reset) {
		ret = t4_fw_reset(adapter, adapter->mbox,
				  F_PIORSTMODE | F_PIORST);
		if (ret < 0) {
			dev_warn(adapter, "Firmware reset failed, error %d\n",
				 -ret);
			goto bye;
		}
	}

	cfg_addr = t4_flash_cfg_addr(adapter);
	if (cfg_addr < 0) {
		ret = cfg_addr;
		dev_warn(adapter, "Finding address for firmware config file in flash failed, error %d\n",
			 -ret);
		goto bye;
	}

	strcpy(config_name, "On Flash");
	mtype = FW_MEMTYPE_CF_FLASH;
	maddr = cfg_addr;

	/* Enable HASH filter region when support is available. */
	val = 1;
	param = CXGBE_FW_PARAM_DEV(HASHFILTER_WITH_OFLD);
	t4_set_params(adapter, adapter->mbox, adapter->pf, 0, 1,
		      &param, &val);

	/*
	 * Issue a Capability Configuration command to the firmware to get it
	 * to parse the Configuration File.  We don't use t4_fw_config_file()
	 * because we want the ability to modify various features after we've
	 * processed the configuration file ...
	 */
	memset(&caps_cmd, 0, sizeof(caps_cmd));
	caps_cmd.op_to_write = cpu_to_be32(V_FW_CMD_OP(FW_CAPS_CONFIG_CMD) |
					   F_FW_CMD_REQUEST | F_FW_CMD_READ);
	caps_cmd.cfvalid_to_len16 =
		cpu_to_be32(F_FW_CAPS_CONFIG_CMD_CFVALID |
			    V_FW_CAPS_CONFIG_CMD_MEMTYPE_CF(mtype) |
			    V_FW_CAPS_CONFIG_CMD_MEMADDR64K_CF(maddr >> 16) |
			    FW_LEN16(caps_cmd));
	ret = t4_wr_mbox(adapter, adapter->mbox, &caps_cmd, sizeof(caps_cmd),
			 &caps_cmd);
	/*
	 * If the CAPS_CONFIG failed with an ENOENT (for a Firmware
	 * Configuration File in FLASH), our last gasp effort is to use the
	 * Firmware Configuration File which is embedded in the firmware.  A
	 * very few early versions of the firmware didn't have one embedded
	 * but we can ignore those.
	 */
	if (ret == -ENOENT) {
		dev_info(adapter, "%s: Going for embedded config in firmware..\n",
			 __func__);

		memset(&caps_cmd, 0, sizeof(caps_cmd));
		caps_cmd.op_to_write =
			cpu_to_be32(V_FW_CMD_OP(FW_CAPS_CONFIG_CMD) |
				    F_FW_CMD_REQUEST | F_FW_CMD_READ);
		caps_cmd.cfvalid_to_len16 = cpu_to_be32(FW_LEN16(caps_cmd));
		ret = t4_wr_mbox(adapter, adapter->mbox, &caps_cmd,
				 sizeof(caps_cmd), &caps_cmd);
		strcpy(config_name, "Firmware Default");
	}

	config_issued = 1;
	if (ret < 0)
		goto bye;

	finiver = be32_to_cpu(caps_cmd.finiver);
	finicsum = be32_to_cpu(caps_cmd.finicsum);
	cfcsum = be32_to_cpu(caps_cmd.cfcsum);
	if (finicsum != cfcsum)
		dev_warn(adapter, "Configuration File checksum mismatch: [fini] csum=%#x, computed csum=%#x\n",
			 finicsum, cfcsum);

	/*
	 * If we're a pure NIC driver then disable all offloading facilities.
	 * This will allow the firmware to optimize aspects of the hardware
	 * configuration which will result in improved performance.
	 */
	caps_cmd.niccaps &= cpu_to_be16(~FW_CAPS_CONFIG_NIC_ETHOFLD);
	caps_cmd.toecaps = 0;
	caps_cmd.iscsicaps = 0;
	caps_cmd.rdmacaps = 0;
	caps_cmd.fcoecaps = 0;
	caps_cmd.cryptocaps = 0;

	/*
	 * And now tell the firmware to use the configuration we just loaded.
	 */
	caps_cmd.op_to_write = cpu_to_be32(V_FW_CMD_OP(FW_CAPS_CONFIG_CMD) |
					   F_FW_CMD_REQUEST | F_FW_CMD_WRITE);
	caps_cmd.cfvalid_to_len16 = htonl(FW_LEN16(caps_cmd));
	ret = t4_wr_mbox(adapter, adapter->mbox, &caps_cmd, sizeof(caps_cmd),
			 NULL);
	if (ret < 0) {
		dev_warn(adapter, "Unable to finalize Firmware Capabilities %d\n",
			 -ret);
		goto bye;
	}

	/*
	 * Tweak configuration based on system architecture, etc.
	 */
	ret = adap_init0_tweaks(adapter);
	if (ret < 0) {
		dev_warn(adapter, "Unable to do init0-tweaks %d\n", -ret);
		goto bye;
	}

	/*
	 * And finally tell the firmware to initialize itself using the
	 * parameters from the Configuration File.
	 */
	ret = t4_fw_initialize(adapter, adapter->mbox);
	if (ret < 0) {
		dev_warn(adapter, "Initializing Firmware failed, error %d\n",
			 -ret);
		goto bye;
	}

	/*
	 * Return successfully and note that we're operating with parameters
	 * not supplied by the driver, rather than from hard-wired
	 * initialization constants buried in the driver.
	 */
	dev_info(adapter,
		 "Successfully configured using Firmware Configuration File \"%s\", version %#x, computed checksum %#x\n",
		 config_name, finiver, cfcsum);

	return 0;

	/*
	 * Something bad happened.  Return the error ...  (If the "error"
	 * is that there's no Configuration File on the adapter we don't
	 * want to issue a warning since this is fairly common.)
	 */
bye:
	if (config_issued && ret != -ENOENT)
		dev_warn(adapter, "\"%s\" configuration file error %d\n",
			 config_name, -ret);

	dev_debug(adapter, "%s: returning ret = %d ..\n", __func__, ret);
	return ret;
}

static int adap_init0(struct adapter *adap)
{
	struct fw_caps_config_cmd caps_cmd;
	int ret = 0;
	u32 v, port_vec;
	enum dev_state state;
	u32 params[7], val[7];
	int reset = 1;
	int mbox = adap->mbox;

	/*
	 * Contact FW, advertising Master capability.
	 */
	ret = t4_fw_hello(adap, adap->mbox, adap->mbox, MASTER_MAY, &state);
	if (ret < 0) {
		dev_err(adap, "%s: could not connect to FW, error %d\n",
			__func__, -ret);
		goto bye;
	}

	CXGBE_DEBUG_MBOX(adap, "%s: adap->mbox = %d; ret = %d\n", __func__,
			 adap->mbox, ret);

	if (ret == mbox)
		adap->flags |= MASTER_PF;

	if (state == DEV_STATE_INIT) {
		/*
		 * Force halt and reset FW because a previous instance may have
		 * exited abnormally without properly shutting down
		 */
		ret = t4_fw_halt(adap, adap->mbox, reset);
		if (ret < 0) {
			dev_err(adap, "Failed to halt. Exit.\n");
			goto bye;
		}

		ret = t4_fw_restart(adap, adap->mbox, reset);
		if (ret < 0) {
			dev_err(adap, "Failed to restart. Exit.\n");
			goto bye;
		}
		state = (enum dev_state)((unsigned)state & ~DEV_STATE_INIT);
	}

	t4_get_version_info(adap);

	ret = t4_get_core_clock(adap, &adap->params.vpd);
	if (ret < 0) {
		dev_err(adap, "%s: could not get core clock, error %d\n",
			__func__, -ret);
		goto bye;
	}

	/*
	 * If the firmware is initialized already (and we're not forcing a
	 * master initialization), note that we're living with existing
	 * adapter parameters.  Otherwise, it's time to try initializing the
	 * adapter ...
	 */
	if (state == DEV_STATE_INIT) {
		dev_info(adap, "Coming up as %s: Adapter already initialized\n",
			 adap->flags & MASTER_PF ? "MASTER" : "SLAVE");
	} else {
		dev_info(adap, "Coming up as MASTER: Initializing adapter\n");

		ret = adap_init0_config(adap, reset);
		if (ret == -ENOENT) {
			dev_err(adap,
				"No Configuration File present on adapter. Using hard-wired configuration parameters.\n");
			goto bye;
		}
	}
	if (ret < 0) {
		dev_err(adap, "could not initialize adapter, error %d\n", -ret);
		goto bye;
	}

	/* Now that we've successfully configured and initialized the adapter
	 * (or found it already initialized), we can ask the Firmware what
	 * resources it has provisioned for us.
	 */
	ret = t4_get_pfres(adap);
	if (ret) {
		dev_err(adap->pdev_dev,
			"Unable to retrieve resource provisioning info\n");
		goto bye;
	}

	/* Find out what ports are available to us. */
	v = V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) |
	    V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_PORTVEC);
	ret = t4_query_params(adap, adap->mbox, adap->pf, 0, 1, &v, &port_vec);
	if (ret < 0) {
		dev_err(adap, "%s: failure in t4_query_params; error = %d\n",
			__func__, ret);
		goto bye;
	}

	adap->params.nports = hweight32(port_vec);
	adap->params.portvec = port_vec;

	dev_debug(adap, "%s: adap->params.nports = %u\n", __func__,
		  adap->params.nports);

	/*
	 * Give the SGE code a chance to pull in anything that it needs ...
	 * Note that this must be called after we retrieve our VPD parameters
	 * in order to know how to convert core ticks to seconds, etc.
	 */
	ret = t4_sge_init(adap);
	if (ret < 0) {
		dev_err(adap, "t4_sge_init failed with error %d\n",
			-ret);
		goto bye;
	}

	/*
	 * Grab some of our basic fundamental operating parameters.
	 */
	params[0] = CXGBE_FW_PARAM_PFVF(L2T_START);
	params[1] = CXGBE_FW_PARAM_PFVF(L2T_END);
	params[2] = CXGBE_FW_PARAM_PFVF(FILTER_START);
	params[3] = CXGBE_FW_PARAM_PFVF(FILTER_END);
	ret = t4_query_params(adap, adap->mbox, adap->pf, 0, 4, params, val);
	if (ret < 0)
		goto bye;
	adap->l2t_start = val[0];
	adap->l2t_end = val[1];
	adap->tids.ftid_base = val[2];
	adap->tids.nftids = val[3] - val[2] + 1;

	params[0] = CXGBE_FW_PARAM_PFVF(CLIP_START);
	params[1] = CXGBE_FW_PARAM_PFVF(CLIP_END);
	ret = t4_query_params(adap, adap->mbox, adap->pf, 0, 2, params, val);
	if (ret < 0)
		goto bye;
	adap->clipt_start = val[0];
	adap->clipt_end = val[1];

	/*
	 * Get device capabilities so we can determine what resources we need
	 * to manage.
	 */
	memset(&caps_cmd, 0, sizeof(caps_cmd));
	caps_cmd.op_to_write = htonl(V_FW_CMD_OP(FW_CAPS_CONFIG_CMD) |
				     F_FW_CMD_REQUEST | F_FW_CMD_READ);
	caps_cmd.cfvalid_to_len16 = htonl(FW_LEN16(caps_cmd));
	ret = t4_wr_mbox(adap, adap->mbox, &caps_cmd, sizeof(caps_cmd),
			 &caps_cmd);
	if (ret < 0)
		goto bye;

	if ((caps_cmd.niccaps & cpu_to_be16(FW_CAPS_CONFIG_NIC_HASHFILTER)) &&
	    is_t6(adap->params.chip)) {
		if (cxgbe_init_hash_filter(adap) < 0)
			goto bye;
	}

	/* See if FW supports FW_FILTER2 work request */
	if (is_t4(adap->params.chip)) {
		adap->params.filter2_wr_support = 0;
	} else {
		params[0] = CXGBE_FW_PARAM_DEV(FILTER2_WR);
		ret = t4_query_params(adap, adap->mbox, adap->pf, 0,
				      1, params, val);
		adap->params.filter2_wr_support = (ret == 0 && val[0] != 0);
	}

	/* Check if FW supports returning vin.
	 * If this is not supported, driver will interpret
	 * these values from viid.
	 */
	params[0] = CXGBE_FW_PARAM_DEV(OPAQUE_VIID_SMT_EXTN);
	ret = t4_query_params(adap, adap->mbox, adap->pf, 0,
			      1, params, val);
	adap->params.viid_smt_extn_support = (ret == 0 && val[0] != 0);

	/* query tid-related parameters */
	params[0] = CXGBE_FW_PARAM_DEV(NTID);
	ret = t4_query_params(adap, adap->mbox, adap->pf, 0, 1,
			      params, val);
	if (ret < 0)
		goto bye;
	adap->tids.ntids = val[0];
	adap->tids.natids = min(adap->tids.ntids / 2, MAX_ATIDS);

	/* If we're running on newer firmware, let it know that we're
	 * prepared to deal with encapsulated CPL messages.  Older
	 * firmware won't understand this and we'll just get
	 * unencapsulated messages ...
	 */
	params[0] = CXGBE_FW_PARAM_PFVF(CPLFW4MSG_ENCAP);
	val[0] = 1;
	(void)t4_set_params(adap, adap->mbox, adap->pf, 0, 1, params, val);

	/*
	 * Find out whether we're allowed to use the T5+ ULPTX MEMWRITE DSGL
	 * capability.  Earlier versions of the firmware didn't have the
	 * ULPTX_MEMWRITE_DSGL so we'll interpret a query failure as no
	 * permission to use ULPTX MEMWRITE DSGL.
	 */
	if (is_t4(adap->params.chip)) {
		adap->params.ulptx_memwrite_dsgl = false;
	} else {
		params[0] = CXGBE_FW_PARAM_DEV(ULPTX_MEMWRITE_DSGL);
		ret = t4_query_params(adap, adap->mbox, adap->pf, 0,
				      1, params, val);
		adap->params.ulptx_memwrite_dsgl = (ret == 0 && val[0] != 0);
	}

	/* Query for max number of packets that can be coalesced for Tx */
	params[0] = CXGBE_FW_PARAM_PFVF(MAX_PKTS_PER_ETH_TX_PKTS_WR);
	ret = t4_query_params(adap, adap->mbox, adap->pf, 0, 1, params, val);
	if (!ret && val[0] > 0)
		adap->params.max_tx_coalesce_num = val[0];
	else
		adap->params.max_tx_coalesce_num = ETH_COALESCE_PKT_NUM;

	/*
	 * The MTU/MSS Table is initialized by now, so load their values.  If
	 * we're initializing the adapter, then we'll make any modifications
	 * we want to the MTU/MSS Table and also initialize the congestion
	 * parameters.
	 */
	t4_read_mtu_tbl(adap, adap->params.mtus, NULL);
	if (state != DEV_STATE_INIT) {
		int i;

		/*
		 * The default MTU Table contains values 1492 and 1500.
		 * However, for TCP, it's better to have two values which are
		 * a multiple of 8 +/- 4 bytes apart near this popular MTU.
		 * This allows us to have a TCP Data Payload which is a
		 * multiple of 8 regardless of what combination of TCP Options
		 * are in use (always a multiple of 4 bytes) which is
		 * important for performance reasons.  For instance, if no
		 * options are in use, then we have a 20-byte IP header and a
		 * 20-byte TCP header.  In this case, a 1500-byte MSS would
		 * result in a TCP Data Payload of 1500 - 40 == 1460 bytes
		 * which is not a multiple of 8.  So using an MSS of 1488 in
		 * this case results in a TCP Data Payload of 1448 bytes which
		 * is a multiple of 8.  On the other hand, if 12-byte TCP Time
		 * Stamps have been negotiated, then an MTU of 1500 bytes
		 * results in a TCP Data Payload of 1448 bytes which, as
		 * above, is a multiple of 8 bytes ...
		 */
		for (i = 0; i < NMTUS; i++)
			if (adap->params.mtus[i] == 1492) {
				adap->params.mtus[i] = 1488;
				break;
			}

		t4_load_mtus(adap, adap->params.mtus, adap->params.a_wnd,
			     adap->params.b_wnd);
	}
	t4_init_sge_params(adap);
	ret = configure_filter_mode_mask(adap);
	if (ret < 0)
		goto bye;
	t4_init_tp_params(adap);
	configure_pcie_ext_tag(adap);
	configure_vlan_types(adap);
	cxgbe_configure_max_ethqsets(adap);

	adap->params.drv_memwin = MEMWIN_NIC;
	adap->flags |= FW_OK;
	dev_debug(adap, "%s: returning zero..\n", __func__);
	return 0;

	/*
	 * Something bad happened.  If a command timed out or failed with EIO
	 * FW does not operate within its spec or something catastrophic
	 * happened to HW/FW, stop issuing commands.
	 */
bye:
	if (ret != -ETIMEDOUT && ret != -EIO)
		t4_fw_bye(adap, adap->mbox);
	return ret;
}

/**
 * t4_os_portmod_changed - handle port module changes
 * @adap: the adapter associated with the module change
 * @port_id: the port index whose module status has changed
 *
 * This is the OS-dependent handler for port module changes.  It is
 * invoked when a port module is removed or inserted for any OS-specific
 * processing.
 */
void t4_os_portmod_changed(const struct adapter *adap, int port_id)
{
	static const char * const mod_str[] = {
		NULL, "LR", "SR", "ER", "passive DA", "active DA", "LRM"
	};

	const struct port_info *pi = adap2pinfo(adap, port_id);

	if (pi->mod_type == FW_PORT_MOD_TYPE_NONE)
		dev_info(adap, "Port%d: port module unplugged\n", pi->port_id);
	else if (pi->mod_type < ARRAY_SIZE(mod_str))
		dev_info(adap, "Port%d: %s port module inserted\n", pi->port_id,
			 mod_str[pi->mod_type]);
	else if (pi->mod_type == FW_PORT_MOD_TYPE_NOTSUPPORTED)
		dev_info(adap, "Port%d: unsupported port module inserted\n",
			 pi->port_id);
	else if (pi->mod_type == FW_PORT_MOD_TYPE_UNKNOWN)
		dev_info(adap, "Port%d: unknown port module inserted\n",
			 pi->port_id);
	else if (pi->mod_type == FW_PORT_MOD_TYPE_ERROR)
		dev_info(adap, "Port%d: transceiver module error\n",
			 pi->port_id);
	else
		dev_info(adap, "Port%d: unknown module type %d inserted\n",
			 pi->port_id, pi->mod_type);
}

bool cxgbe_force_linkup(struct adapter *adap)
{
	if (is_pf4(adap))
		return false;	/* force_linkup not required for pf driver */

	return adap->devargs.force_link_up;
}

/**
 * link_start - enable a port
 * @dev: the port to enable
 *
 * Performs the MAC and PHY actions needed to enable a port.
 */
int cxgbe_link_start(struct port_info *pi)
{
	struct adapter *adapter = pi->adapter;
	u64 conf_offloads;
	unsigned int mtu;
	int ret;

	mtu = pi->eth_dev->data->dev_conf.rxmode.max_rx_pkt_len -
	      (RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN);

	conf_offloads = pi->eth_dev->data->dev_conf.rxmode.offloads;

	/*
	 * We do not set address filters and promiscuity here, the stack does
	 * that step explicitly.
	 */
	ret = t4_set_rxmode(adapter, adapter->mbox, pi->viid, mtu, -1, -1, -1,
			    !!(conf_offloads & DEV_RX_OFFLOAD_VLAN_STRIP),
			    true);
	if (ret == 0) {
		ret = cxgbe_mpstcam_modify(pi, (int)pi->xact_addr_filt,
				(u8 *)&pi->eth_dev->data->mac_addrs[0]);
		if (ret >= 0) {
			pi->xact_addr_filt = ret;
			ret = 0;
		}
	}
	if (ret == 0 && is_pf4(adapter))
		ret = t4_link_l1cfg(adapter, adapter->mbox, pi->tx_chan,
				    &pi->link_cfg);
	if (ret == 0) {
		/*
		 * Enabling a Virtual Interface can result in an interrupt
		 * during the processing of the VI Enable command and, in some
		 * paths, result in an attempt to issue another command in the
		 * interrupt context.  Thus, we disable interrupts during the
		 * course of the VI Enable command ...
		 */
		ret = t4_enable_vi_params(adapter, adapter->mbox, pi->viid,
					  true, true, false);
	}

	if (ret == 0 && cxgbe_force_linkup(adapter))
		pi->eth_dev->data->dev_link.link_status = ETH_LINK_UP;
	return ret;
}

/**
 * cxgbe_write_rss_conf - flash the RSS configuration for a given port
 * @pi: the port
 * @rss_hf: Hash configuration to apply
 */
int cxgbe_write_rss_conf(const struct port_info *pi, uint64_t rss_hf)
{
	struct adapter *adapter = pi->adapter;
	const struct sge_eth_rxq *rxq;
	u64 flags = 0;
	u16 rss;
	int err;

	/*  Should never be called before setting up sge eth rx queues */
	if (!(adapter->flags & FULL_INIT_DONE)) {
		dev_err(adap, "%s No RXQs available on port %d\n",
			__func__, pi->port_id);
		return -EINVAL;
	}

	/* Don't allow unsupported hash functions */
	if (rss_hf & ~CXGBE_RSS_HF_ALL)
		return -EINVAL;

	if (rss_hf & CXGBE_RSS_HF_IPV4_MASK)
		flags |= F_FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN;

	if (rss_hf & ETH_RSS_NONFRAG_IPV4_TCP)
		flags |= F_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN;

	if (rss_hf & ETH_RSS_NONFRAG_IPV4_UDP)
		flags |= F_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN |
			 F_FW_RSS_VI_CONFIG_CMD_UDPEN;

	if (rss_hf & CXGBE_RSS_HF_IPV6_MASK)
		flags |= F_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN;

	if (rss_hf & CXGBE_RSS_HF_TCP_IPV6_MASK)
		flags |= F_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN |
			 F_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN;

	if (rss_hf & CXGBE_RSS_HF_UDP_IPV6_MASK)
		flags |= F_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN |
			 F_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN |
			 F_FW_RSS_VI_CONFIG_CMD_UDPEN;

	rxq = &adapter->sge.ethrxq[pi->first_qset];
	rss = rxq[0].rspq.abs_id;

	/* If Tunnel All Lookup isn't specified in the global RSS
	 * Configuration, then we need to specify a default Ingress
	 * Queue for any ingress packets which aren't hashed.  We'll
	 * use our first ingress queue ...
	 */
	err = t4_config_vi_rss(adapter, adapter->mbox, pi->viid,
			       flags, rss);
	return err;
}

/**
 * cxgbe_write_rss - write the RSS table for a given port
 * @pi: the port
 * @queues: array of queue indices for RSS
 *
 * Sets up the portion of the HW RSS table for the port's VI to distribute
 * packets to the Rx queues in @queues.
 */
int cxgbe_write_rss(const struct port_info *pi, const u16 *queues)
{
	u16 *rss;
	int i, err;
	struct adapter *adapter = pi->adapter;
	const struct sge_eth_rxq *rxq;

	/*  Should never be called before setting up sge eth rx queues */
	BUG_ON(!(adapter->flags & FULL_INIT_DONE));

	rxq = &adapter->sge.ethrxq[pi->first_qset];
	rss = rte_zmalloc(NULL, pi->rss_size * sizeof(u16), 0);
	if (!rss)
		return -ENOMEM;

	/* map the queue indices to queue ids */
	for (i = 0; i < pi->rss_size; i++, queues++)
		rss[i] = rxq[*queues].rspq.abs_id;

	err = t4_config_rss_range(adapter, adapter->pf, pi->viid, 0,
				  pi->rss_size, rss, pi->rss_size);
	rte_free(rss);
	return err;
}

/**
 * setup_rss - configure RSS
 * @adapter: the adapter
 *
 * Sets up RSS to distribute packets to multiple receive queues.  We
 * configure the RSS CPU lookup table to distribute to the number of HW
 * receive queues, and the response queue lookup table to narrow that
 * down to the response queues actually configured for each port.
 * We always configure the RSS mapping for all ports since the mapping
 * table has plenty of entries.
 */
int cxgbe_setup_rss(struct port_info *pi)
{
	int j, err;
	struct adapter *adapter = pi->adapter;

	dev_debug(adapter, "%s:  pi->rss_size = %u; pi->n_rx_qsets = %u\n",
		  __func__, pi->rss_size, pi->n_rx_qsets);

	if (!(pi->flags & PORT_RSS_DONE)) {
		if (adapter->flags & FULL_INIT_DONE) {
			/* Fill default values with equal distribution */
			for (j = 0; j < pi->rss_size; j++)
				pi->rss[j] = j % pi->n_rx_qsets;

			err = cxgbe_write_rss(pi, pi->rss);
			if (err)
				return err;

			err = cxgbe_write_rss_conf(pi, pi->rss_hf);
			if (err)
				return err;
			pi->flags |= PORT_RSS_DONE;
		}
	}
	return 0;
}

/*
 * Enable NAPI scheduling and interrupt generation for all Rx queues.
 */
static void enable_rx(struct adapter *adap, struct sge_rspq *q)
{
	/* 0-increment GTS to start the timer and enable interrupts */
	t4_write_reg(adap, is_pf4(adap) ? MYPF_REG(A_SGE_PF_GTS) :
					  T4VF_SGE_BASE_ADDR + A_SGE_VF_GTS,
		     V_SEINTARM(q->intr_params) |
		     V_INGRESSQID(q->cntxt_id));
}

void cxgbe_enable_rx_queues(struct port_info *pi)
{
	struct adapter *adap = pi->adapter;
	struct sge *s = &adap->sge;
	unsigned int i;

	for (i = 0; i < pi->n_rx_qsets; i++)
		enable_rx(adap, &s->ethrxq[pi->first_qset + i].rspq);
}

/**
 * fw_caps_to_speed_caps - translate Firmware Port Caps to Speed Caps.
 * @port_type: Firmware Port Type
 * @fw_caps: Firmware Port Capabilities
 * @speed_caps: Device Info Speed Capabilities
 *
 * Translate a Firmware Port Capabilities specification to Device Info
 * Speed Capabilities.
 */
static void fw_caps_to_speed_caps(enum fw_port_type port_type,
				  unsigned int fw_caps,
				  u32 *speed_caps)
{
#define SET_SPEED(__speed_name) \
	do { \
		*speed_caps |= ETH_LINK_ ## __speed_name; \
	} while (0)

#define FW_CAPS_TO_SPEED(__fw_name) \
	do { \
		if (fw_caps & FW_PORT_CAP32_ ## __fw_name) \
			SET_SPEED(__fw_name); \
	} while (0)

	switch (port_type) {
	case FW_PORT_TYPE_BT_SGMII:
	case FW_PORT_TYPE_BT_XFI:
	case FW_PORT_TYPE_BT_XAUI:
		FW_CAPS_TO_SPEED(SPEED_100M);
		FW_CAPS_TO_SPEED(SPEED_1G);
		FW_CAPS_TO_SPEED(SPEED_10G);
		break;

	case FW_PORT_TYPE_KX4:
	case FW_PORT_TYPE_KX:
	case FW_PORT_TYPE_FIBER_XFI:
	case FW_PORT_TYPE_FIBER_XAUI:
	case FW_PORT_TYPE_SFP:
	case FW_PORT_TYPE_QSFP_10G:
	case FW_PORT_TYPE_QSA:
		FW_CAPS_TO_SPEED(SPEED_1G);
		FW_CAPS_TO_SPEED(SPEED_10G);
		break;

	case FW_PORT_TYPE_KR:
		SET_SPEED(SPEED_10G);
		break;

	case FW_PORT_TYPE_BP_AP:
	case FW_PORT_TYPE_BP4_AP:
		SET_SPEED(SPEED_1G);
		SET_SPEED(SPEED_10G);
		break;

	case FW_PORT_TYPE_BP40_BA:
	case FW_PORT_TYPE_QSFP:
		SET_SPEED(SPEED_40G);
		break;

	case FW_PORT_TYPE_CR_QSFP:
	case FW_PORT_TYPE_SFP28:
	case FW_PORT_TYPE_KR_SFP28:
		FW_CAPS_TO_SPEED(SPEED_1G);
		FW_CAPS_TO_SPEED(SPEED_10G);
		FW_CAPS_TO_SPEED(SPEED_25G);
		break;

	case FW_PORT_TYPE_CR2_QSFP:
		SET_SPEED(SPEED_50G);
		break;

	case FW_PORT_TYPE_KR4_100G:
	case FW_PORT_TYPE_CR4_QSFP:
		FW_CAPS_TO_SPEED(SPEED_25G);
		FW_CAPS_TO_SPEED(SPEED_40G);
		FW_CAPS_TO_SPEED(SPEED_50G);
		FW_CAPS_TO_SPEED(SPEED_100G);
		break;

	default:
		break;
	}

#undef FW_CAPS_TO_SPEED
#undef SET_SPEED
}

/**
 * cxgbe_get_speed_caps - Fetch supported speed capabilities
 * @pi: Underlying port's info
 * @speed_caps: Device Info speed capabilities
 *
 * Fetch supported speed capabilities of the underlying port.
 */
void cxgbe_get_speed_caps(struct port_info *pi, u32 *speed_caps)
{
	*speed_caps = 0;

	fw_caps_to_speed_caps(pi->port_type, pi->link_cfg.pcaps,
			      speed_caps);

	if (!(pi->link_cfg.pcaps & FW_PORT_CAP32_ANEG))
		*speed_caps |= ETH_LINK_SPEED_FIXED;
}

/**
 * cxgbe_set_link_status - Set device link up or down.
 * @pi: Underlying port's info
 * @status: 0 - down, 1 - up
 *
 * Set the device link up or down.
 */
int cxgbe_set_link_status(struct port_info *pi, bool status)
{
	struct adapter *adapter = pi->adapter;
	int err = 0;

	err = t4_enable_vi(adapter, adapter->mbox, pi->viid, status, status);
	if (err) {
		dev_err(adapter, "%s: disable_vi failed: %d\n", __func__, err);
		return err;
	}

	if (!status)
		t4_reset_link_config(adapter, pi->pidx);

	return 0;
}

/**
 * cxgb_up - enable the adapter
 * @adap: adapter being enabled
 *
 * Called when the first port is enabled, this function performs the
 * actions necessary to make an adapter operational, such as completing
 * the initialization of HW modules, and enabling interrupts.
 */
int cxgbe_up(struct adapter *adap)
{
	enable_rx(adap, &adap->sge.fw_evtq);
	t4_sge_tx_monitor_start(adap);
	if (is_pf4(adap))
		t4_intr_enable(adap);
	adap->flags |= FULL_INIT_DONE;

	/* TODO: deadman watchdog ?? */
	return 0;
}

/*
 * Close the port
 */
int cxgbe_down(struct port_info *pi)
{
	return cxgbe_set_link_status(pi, false);
}

/*
 * Release resources when all the ports have been stopped.
 */
void cxgbe_close(struct adapter *adapter)
{
	struct port_info *pi;
	int i;

	if (adapter->flags & FULL_INIT_DONE) {
		tid_free(&adapter->tids);
		t4_cleanup_mpstcam(adapter);
		t4_cleanup_clip_tbl(adapter);
		t4_cleanup_l2t(adapter);
		t4_cleanup_smt(adapter);
		if (is_pf4(adapter))
			t4_intr_disable(adapter);
		t4_sge_tx_monitor_stop(adapter);
		t4_free_sge_resources(adapter);
		for_each_port(adapter, i) {
			pi = adap2pinfo(adapter, i);
			if (pi->viid != 0)
				t4_free_vi(adapter, adapter->mbox,
					   adapter->pf, 0, pi->viid);
			rte_eth_dev_release_port(pi->eth_dev);
		}
		adapter->flags &= ~FULL_INIT_DONE;
	}

	if (is_pf4(adapter) && (adapter->flags & FW_OK))
		t4_fw_bye(adapter, adapter->mbox);
}

static void adap_smt_index(struct adapter *adapter, u32 *smt_start_idx,
			   u32 *smt_size)
{
	u32 params[2], smt_val[2];
	int ret;

	params[0] = CXGBE_FW_PARAM_PFVF(GET_SMT_START);
	params[1] = CXGBE_FW_PARAM_PFVF(GET_SMT_SIZE);

	ret = t4_query_params(adapter, adapter->mbox, adapter->pf, 0,
			      2, params, smt_val);

	/* if FW doesn't recognize this command then set it to default setting
	 * which is start index as 0 and size as 256.
	 */
	if (ret < 0) {
		*smt_start_idx = 0;
		*smt_size = SMT_SIZE;
	} else {
		*smt_start_idx = smt_val[0];
		/* smt size can be zero, if nsmt is not yet configured in
		 * the config file or set as zero, then configure all the
		 * remaining entries to this PF itself.
		 */
		if (!smt_val[1])
			*smt_size = SMT_SIZE - *smt_start_idx;
		else
			*smt_size = smt_val[1];
	}
}

int cxgbe_probe(struct adapter *adapter)
{
	u32 smt_start_idx, smt_size;
	struct port_info *pi;
	int func, i;
	int err = 0;
	u32 whoami;
	int chip;

	whoami = t4_read_reg(adapter, A_PL_WHOAMI);
	chip = t4_get_chip_type(adapter,
			CHELSIO_PCI_ID_VER(adapter->pdev->id.device_id));
	if (chip < 0)
		return chip;

	func = CHELSIO_CHIP_VERSION(chip) <= CHELSIO_T5 ?
	       G_SOURCEPF(whoami) : G_T6_SOURCEPF(whoami);

	adapter->mbox = func;
	adapter->pf = func;

	t4_os_lock_init(&adapter->mbox_lock);
	TAILQ_INIT(&adapter->mbox_list);
	t4_os_lock_init(&adapter->win0_lock);

	err = t4_prep_adapter(adapter);
	if (err)
		return err;

	setup_memwin(adapter);
	err = adap_init0(adapter);
	if (err) {
		dev_err(adapter, "%s: Adapter initialization failed, error %d\n",
			__func__, err);
		goto out_free;
	}

	if (!is_t4(adapter->params.chip)) {
		/*
		 * The userspace doorbell BAR is split evenly into doorbell
		 * regions, each associated with an egress queue.  If this
		 * per-queue region is large enough (at least UDBS_SEG_SIZE)
		 * then it can be used to submit a tx work request with an
		 * implied doorbell.  Enable write combining on the BAR if
		 * there is room for such work requests.
		 */
		int s_qpp, qpp, num_seg;

		s_qpp = (S_QUEUESPERPAGEPF0 +
			(S_QUEUESPERPAGEPF1 - S_QUEUESPERPAGEPF0) *
			adapter->pf);
		qpp = 1 << ((t4_read_reg(adapter,
				A_SGE_EGRESS_QUEUES_PER_PAGE_PF) >> s_qpp)
				& M_QUEUESPERPAGEPF0);
		num_seg = CXGBE_PAGE_SIZE / UDBS_SEG_SIZE;
		if (qpp > num_seg)
			dev_warn(adapter, "Incorrect SGE EGRESS QUEUES_PER_PAGE configuration, continuing in debug mode\n");

		adapter->bar2 = (void *)adapter->pdev->mem_resource[2].addr;
		if (!adapter->bar2) {
			dev_err(adapter, "cannot map device bar2 region\n");
			err = -ENOMEM;
			goto out_free;
		}
		t4_write_reg(adapter, A_SGE_STAT_CFG, V_STATSOURCE_T5(7) |
			     V_STATMODE(0));
	}

	for_each_port(adapter, i) {
		const unsigned int numa_node = rte_socket_id();
		char name[RTE_ETH_NAME_MAX_LEN];
		struct rte_eth_dev *eth_dev;

		snprintf(name, sizeof(name), "%s_%d",
			 adapter->pdev->device.name, i);

		if (i == 0) {
			/* First port is already allocated by DPDK */
			eth_dev = adapter->eth_dev;
			goto allocate_mac;
		}

		/*
		 * now do all data allocation - for eth_dev structure,
		 * and internal (private) data for the remaining ports
		 */

		/* reserve an ethdev entry */
		eth_dev = rte_eth_dev_allocate(name);
		if (!eth_dev)
			goto out_free;

		eth_dev->data->dev_private =
			rte_zmalloc_socket(name, sizeof(struct port_info),
					   RTE_CACHE_LINE_SIZE, numa_node);
		if (!eth_dev->data->dev_private)
			goto out_free;

allocate_mac:
		pi = eth_dev->data->dev_private;
		adapter->port[i] = pi;
		pi->eth_dev = eth_dev;
		pi->adapter = adapter;
		pi->xact_addr_filt = -1;
		pi->port_id = i;
		pi->pidx = i;

		pi->eth_dev->device = &adapter->pdev->device;
		pi->eth_dev->dev_ops = adapter->eth_dev->dev_ops;
		pi->eth_dev->tx_pkt_burst = adapter->eth_dev->tx_pkt_burst;
		pi->eth_dev->rx_pkt_burst = adapter->eth_dev->rx_pkt_burst;

		rte_eth_copy_pci_info(pi->eth_dev, adapter->pdev);

		pi->eth_dev->data->mac_addrs = rte_zmalloc(name,
							RTE_ETHER_ADDR_LEN, 0);
		if (!pi->eth_dev->data->mac_addrs) {
			dev_err(adapter, "%s: Mem allocation failed for storing mac addr, aborting\n",
				__func__);
			err = -1;
			goto out_free;
		}

		if (i > 0) {
			/* First port will be notified by upper layer */
			rte_eth_dev_probing_finish(eth_dev);
		}
	}

	if (adapter->flags & FW_OK) {
		err = t4_port_init(adapter, adapter->mbox, adapter->pf, 0);
		if (err) {
			dev_err(adapter, "%s: t4_port_init failed with err %d\n",
				__func__, err);
			goto out_free;
		}
	}

	cxgbe_cfg_queues(adapter->eth_dev);

	cxgbe_print_adapter_info(adapter);
	cxgbe_print_port_info(adapter);

	adapter->clipt = t4_init_clip_tbl(adapter->clipt_start,
					  adapter->clipt_end);
	if (!adapter->clipt) {
		/* We tolerate a lack of clip_table, giving up some
		 * functionality
		 */
		dev_warn(adapter, "could not allocate CLIP. Continuing\n");
	}

	adap_smt_index(adapter, &smt_start_idx, &smt_size);
	adapter->smt = t4_init_smt(smt_start_idx, smt_size);
	if (!adapter->smt)
		dev_warn(adapter, "could not allocate SMT, continuing\n");

	adapter->l2t = t4_init_l2t(adapter->l2t_start, adapter->l2t_end);
	if (!adapter->l2t) {
		/* We tolerate a lack of L2T, giving up some functionality */
		dev_warn(adapter, "could not allocate L2T. Continuing\n");
	}

	if (tid_init(&adapter->tids) < 0) {
		/* Disable filtering support */
		dev_warn(adapter, "could not allocate TID table, "
			 "filter support disabled. Continuing\n");
	}

	t4_os_lock_init(&adapter->flow_lock);

	adapter->mpstcam = t4_init_mpstcam(adapter);
	if (!adapter->mpstcam)
		dev_warn(adapter, "could not allocate mps tcam table."
			 " Continuing\n");

	if (is_hashfilter(adapter)) {
		if (t4_read_reg(adapter, A_LE_DB_CONFIG) & F_HASHEN) {
			u32 hash_base, hash_reg;

			hash_reg = A_LE_DB_TID_HASHBASE;
			hash_base = t4_read_reg(adapter, hash_reg);
			adapter->tids.hash_base = hash_base / 4;
		}
	} else {
		/* Disable hash filtering support */
		dev_warn(adapter,
			 "Maskless filter support disabled. Continuing\n");
	}

	err = cxgbe_init_rss(adapter);
	if (err)
		goto out_free;

	return 0;

out_free:
	for_each_port(adapter, i) {
		pi = adap2pinfo(adapter, i);
		if (pi->viid != 0)
			t4_free_vi(adapter, adapter->mbox, adapter->pf,
				   0, pi->viid);
		rte_eth_dev_release_port(pi->eth_dev);
	}

	if (adapter->flags & FW_OK)
		t4_fw_bye(adapter, adapter->mbox);
	return -err;
}
