/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2014-2015 Chelsio Communications.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Chelsio Communications nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/if_ether.h>
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
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_alarm.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_atomic.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_dev.h>

#include "common.h"
#include "t4_regs.h"
#include "t4_msg.h"
#include "cxgbe.h"

/*
 * Max number of Rx buffers we replenish at a time.
 */
#define MAX_RX_REFILL 16U

#define NOMEM_TMR_IDX (SGE_NTIMERS - 1)

/*
 * Rx buffer sizes for "usembufs" Free List buffers (one ingress packet
 * per mbuf buffer).  We currently only support two sizes for 1500- and
 * 9000-byte MTUs. We could easily support more but there doesn't seem to be
 * much need for that ...
 */
#define FL_MTU_SMALL 1500
#define FL_MTU_LARGE 9000

static inline unsigned int fl_mtu_bufsize(struct adapter *adapter,
					  unsigned int mtu)
{
	struct sge *s = &adapter->sge;

	return ALIGN(s->pktshift + ETH_HLEN + VLAN_HLEN + mtu, s->fl_align);
}

#define FL_MTU_SMALL_BUFSIZE(adapter) fl_mtu_bufsize(adapter, FL_MTU_SMALL)
#define FL_MTU_LARGE_BUFSIZE(adapter) fl_mtu_bufsize(adapter, FL_MTU_LARGE)

/*
 * Bits 0..3 of rx_sw_desc.dma_addr have special meaning.  The hardware uses
 * these to specify the buffer size as an index into the SGE Free List Buffer
 * Size register array.  We also use bit 4, when the buffer has been unmapped
 * for DMA, but this is of course never sent to the hardware and is only used
 * to prevent double unmappings.  All of the above requires that the Free List
 * Buffers which we allocate have the bottom 5 bits free (0) -- i.e. are
 * 32-byte or or a power of 2 greater in alignment.  Since the SGE's minimal
 * Free List Buffer alignment is 32 bytes, this works out for us ...
 */
enum {
	RX_BUF_FLAGS     = 0x1f,   /* bottom five bits are special */
	RX_BUF_SIZE      = 0x0f,   /* bottom three bits are for buf sizes */
	RX_UNMAPPED_BUF  = 0x10,   /* buffer is not mapped */

	/*
	 * XXX We shouldn't depend on being able to use these indices.
	 * XXX Especially when some other Master PF has initialized the
	 * XXX adapter or we use the Firmware Configuration File.  We
	 * XXX should really search through the Host Buffer Size register
	 * XXX array for the appropriately sized buffer indices.
	 */
	RX_SMALL_PG_BUF  = 0x0,   /* small (PAGE_SIZE) page buffer */
	RX_LARGE_PG_BUF  = 0x1,   /* buffer large page buffer */

	RX_SMALL_MTU_BUF = 0x2,   /* small MTU buffer */
	RX_LARGE_MTU_BUF = 0x3,   /* large MTU buffer */
};

/**
 * fl_cap - return the capacity of a free-buffer list
 * @fl: the FL
 *
 * Returns the capacity of a free-buffer list.  The capacity is less than
 * the size because one descriptor needs to be left unpopulated, otherwise
 * HW will think the FL is empty.
 */
static inline unsigned int fl_cap(const struct sge_fl *fl)
{
	return fl->size - 8;   /* 1 descriptor = 8 buffers */
}

/**
 * fl_starving - return whether a Free List is starving.
 * @adapter: pointer to the adapter
 * @fl: the Free List
 *
 * Tests specified Free List to see whether the number of buffers
 * available to the hardware has falled below our "starvation"
 * threshold.
 */
static inline bool fl_starving(const struct adapter *adapter,
			       const struct sge_fl *fl)
{
	const struct sge *s = &adapter->sge;

	return fl->avail - fl->pend_cred <= s->fl_starve_thres;
}

static inline unsigned int get_buf_size(struct adapter *adapter,
					const struct rx_sw_desc *d)
{
	struct sge *s = &adapter->sge;
	unsigned int rx_buf_size_idx = d->dma_addr & RX_BUF_SIZE;
	unsigned int buf_size;

	switch (rx_buf_size_idx) {
	case RX_SMALL_PG_BUF:
		buf_size = PAGE_SIZE;
		break;

	case RX_LARGE_PG_BUF:
		buf_size = PAGE_SIZE << s->fl_pg_order;
		break;

	case RX_SMALL_MTU_BUF:
		buf_size = FL_MTU_SMALL_BUFSIZE(adapter);
		break;

	case RX_LARGE_MTU_BUF:
		buf_size = FL_MTU_LARGE_BUFSIZE(adapter);
		break;

	default:
		BUG_ON(1);
		buf_size = 0; /* deal with bogus compiler warnings */
		/* NOTREACHED */
	}

	return buf_size;
}

/**
 * free_rx_bufs - free the Rx buffers on an SGE free list
 * @q: the SGE free list to free buffers from
 * @n: how many buffers to free
 *
 * Release the next @n buffers on an SGE free-buffer Rx queue.   The
 * buffers must be made inaccessible to HW before calling this function.
 */
static void free_rx_bufs(struct sge_fl *q, int n)
{
	unsigned int cidx = q->cidx;
	struct rx_sw_desc *d;

	d = &q->sdesc[cidx];
	while (n--) {
		if (d->buf) {
			rte_pktmbuf_free(d->buf);
			d->buf = NULL;
		}
		++d;
		if (++cidx == q->size) {
			cidx = 0;
			d = q->sdesc;
		}
		q->avail--;
	}
	q->cidx = cidx;
}

/**
 * unmap_rx_buf - unmap the current Rx buffer on an SGE free list
 * @q: the SGE free list
 *
 * Unmap the current buffer on an SGE free-buffer Rx queue.   The
 * buffer must be made inaccessible to HW before calling this function.
 *
 * This is similar to @free_rx_bufs above but does not free the buffer.
 * Do note that the FL still loses any further access to the buffer.
 */
static void unmap_rx_buf(struct sge_fl *q)
{
	if (++q->cidx == q->size)
		q->cidx = 0;
	q->avail--;
}

static inline void ring_fl_db(struct adapter *adap, struct sge_fl *q)
{
	if (q->pend_cred >= 8) {
		u32 val = adap->params.arch.sge_fl_db;

		if (is_t4(adap->params.chip))
			val |= V_PIDX(q->pend_cred / 8);
		else
			val |= V_PIDX_T5(q->pend_cred / 8);

		/*
		 * Make sure all memory writes to the Free List queue are
		 * committed before we tell the hardware about them.
		 */
		wmb();

		/*
		 * If we don't have access to the new User Doorbell (T5+), use
		 * the old doorbell mechanism; otherwise use the new BAR2
		 * mechanism.
		 */
		if (unlikely(!q->bar2_addr)) {
			t4_write_reg(adap, MYPF_REG(A_SGE_PF_KDOORBELL),
				     val | V_QID(q->cntxt_id));
		} else {
			writel(val | V_QID(q->bar2_qid),
			       (void *)((uintptr_t)q->bar2_addr +
			       SGE_UDB_KDOORBELL));

			/*
			 * This Write memory Barrier will force the write to
			 * the User Doorbell area to be flushed.
			 */
			wmb();
		}
		q->pend_cred &= 7;
	}
}

static inline struct rte_mbuf *cxgbe_rxmbuf_alloc(struct rte_mempool *mp)
{
	struct rte_mbuf *m;

	m = __rte_mbuf_raw_alloc(mp);
	__rte_mbuf_sanity_check_raw(m, 0);
	return m;
}

static inline void set_rx_sw_desc(struct rx_sw_desc *sd, void *buf,
				  dma_addr_t mapping)
{
	sd->buf = buf;
	sd->dma_addr = mapping;      /* includes size low bits */
}

/**
 * refill_fl_usembufs - refill an SGE Rx buffer ring with mbufs
 * @adap: the adapter
 * @q: the ring to refill
 * @n: the number of new buffers to allocate
 *
 * (Re)populate an SGE free-buffer queue with up to @n new packet buffers,
 * allocated with the supplied gfp flags.  The caller must assure that
 * @n does not exceed the queue's capacity.  If afterwards the queue is
 * found critically low mark it as starving in the bitmap of starving FLs.
 *
 * Returns the number of buffers allocated.
 */
static unsigned int refill_fl_usembufs(struct adapter *adap, struct sge_fl *q,
				       int n)
{
	struct sge_eth_rxq *rxq = container_of(q, struct sge_eth_rxq, fl);
	unsigned int cred = q->avail;
	__be64 *d = &q->desc[q->pidx];
	struct rx_sw_desc *sd = &q->sdesc[q->pidx];
	unsigned int buf_size_idx = RX_SMALL_MTU_BUF;

	while (n--) {
		struct rte_mbuf *mbuf = cxgbe_rxmbuf_alloc(rxq->rspq.mb_pool);
		dma_addr_t mapping;

		if (!mbuf) {
			dev_debug(adap, "%s: mbuf alloc failed\n", __func__);
			q->alloc_failed++;
			rxq->rspq.eth_dev->data->rx_mbuf_alloc_failed++;
			goto out;
		}

		mbuf->data_off = RTE_PKTMBUF_HEADROOM;
		mbuf->next = NULL;

		mapping = (dma_addr_t)(mbuf->buf_physaddr + mbuf->data_off);

		mapping |= buf_size_idx;
		*d++ = cpu_to_be64(mapping);
		set_rx_sw_desc(sd, mbuf, mapping);
		sd++;

		q->avail++;
		if (++q->pidx == q->size) {
			q->pidx = 0;
			sd = q->sdesc;
			d = q->desc;
		}
	}

out:    cred = q->avail - cred;
	q->pend_cred += cred;
	ring_fl_db(adap, q);

	if (unlikely(fl_starving(adap, q))) {
		/*
		 * Make sure data has been written to free list
		 */
		wmb();
		q->low++;
	}

	return cred;
}

/**
 * refill_fl - refill an SGE Rx buffer ring with mbufs
 * @adap: the adapter
 * @q: the ring to refill
 * @n: the number of new buffers to allocate
 *
 * (Re)populate an SGE free-buffer queue with up to @n new packet buffers,
 * allocated with the supplied gfp flags.  The caller must assure that
 * @n does not exceed the queue's capacity.  Returns the number of buffers
 * allocated.
 */
static unsigned int refill_fl(struct adapter *adap, struct sge_fl *q, int n)
{
	return refill_fl_usembufs(adap, q, n);
}

static inline void __refill_fl(struct adapter *adap, struct sge_fl *fl)
{
	refill_fl(adap, fl, min(MAX_RX_REFILL, fl_cap(fl) - fl->avail));
}

/**
 * alloc_ring - allocate resources for an SGE descriptor ring
 * @dev: the PCI device's core device
 * @nelem: the number of descriptors
 * @elem_size: the size of each descriptor
 * @sw_size: the size of the SW state associated with each ring element
 * @phys: the physical address of the allocated ring
 * @metadata: address of the array holding the SW state for the ring
 * @stat_size: extra space in HW ring for status information
 * @node: preferred node for memory allocations
 *
 * Allocates resources for an SGE descriptor ring, such as Tx queues,
 * free buffer lists, or response queues.  Each SGE ring requires
 * space for its HW descriptors plus, optionally, space for the SW state
 * associated with each HW entry (the metadata).  The function returns
 * three values: the virtual address for the HW ring (the return value
 * of the function), the bus address of the HW ring, and the address
 * of the SW ring.
 */
static void *alloc_ring(size_t nelem, size_t elem_size,
			size_t sw_size, dma_addr_t *phys, void *metadata,
			size_t stat_size, __rte_unused uint16_t queue_id,
			int socket_id, const char *z_name,
			const char *z_name_sw)
{
	size_t len = CXGBE_MAX_RING_DESC_SIZE * elem_size + stat_size;
	const struct rte_memzone *tz;
	void *s = NULL;

	dev_debug(adapter, "%s: nelem = %zu; elem_size = %zu; sw_size = %zu; "
		  "stat_size = %zu; queue_id = %u; socket_id = %d; z_name = %s;"
		  " z_name_sw = %s\n", __func__, nelem, elem_size, sw_size,
		  stat_size, queue_id, socket_id, z_name, z_name_sw);

	tz = rte_memzone_lookup(z_name);
	if (tz) {
		dev_debug(adapter, "%s: tz exists...returning existing..\n",
			  __func__);
		goto alloc_sw_ring;
	}

	/*
	 * Allocate TX/RX ring hardware descriptors. A memzone large enough to
	 * handle the maximum ring size is allocated in order to allow for
	 * resizing in later calls to the queue setup function.
	 */
	tz = rte_memzone_reserve_aligned(z_name, len, socket_id, 0, 4096);
	if (!tz)
		return NULL;

alloc_sw_ring:
	memset(tz->addr, 0, len);
	if (sw_size) {
		s = rte_zmalloc_socket(z_name_sw, nelem * sw_size,
				       RTE_CACHE_LINE_SIZE, socket_id);

		if (!s) {
			dev_err(adapter, "%s: failed to get sw_ring memory\n",
				__func__);
			return NULL;
		}
	}
	if (metadata)
		*(void **)metadata = s;

	*phys = (uint64_t)tz->phys_addr;
	return tz->addr;
}

/**
 * t4_pktgl_to_mbuf_usembufs - build an mbuf from a packet gather list
 * @gl: the gather list
 *
 * Builds an mbuf from the given packet gather list.  Returns the mbuf or
 * %NULL if mbuf allocation failed.
 */
static struct rte_mbuf *t4_pktgl_to_mbuf_usembufs(const struct pkt_gl *gl)
{
	/*
	 * If there's only one mbuf fragment, just return that.
	 */
	if (likely(gl->nfrags == 1))
		return gl->mbufs[0];

	return NULL;
}

/**
 * t4_pktgl_to_mbuf - build an mbuf from a packet gather list
 * @gl: the gather list
 *
 * Builds an mbuf from the given packet gather list.  Returns the mbuf or
 * %NULL if mbuf allocation failed.
 */
static struct rte_mbuf *t4_pktgl_to_mbuf(const struct pkt_gl *gl)
{
	return t4_pktgl_to_mbuf_usembufs(gl);
}

#define RTE_MBUF_DATA_DMA_ADDR_DEFAULT(mb) \
	((dma_addr_t) ((mb)->buf_physaddr + (mb)->data_off))

/**
 * t4_ethrx_handler - process an ingress ethernet packet
 * @q: the response queue that received the packet
 * @rsp: the response queue descriptor holding the RX_PKT message
 * @si: the gather list of packet fragments
 *
 * Process an ingress ethernet packet and deliver it to the stack.
 */
int t4_ethrx_handler(struct sge_rspq *q, const __be64 *rsp,
		     const struct pkt_gl *si)
{
	struct rte_mbuf *mbuf;
	const struct cpl_rx_pkt *pkt;
	const struct rss_header *rss_hdr;
	bool csum_ok;
	struct sge_eth_rxq *rxq = container_of(q, struct sge_eth_rxq, rspq);

	rss_hdr = (const void *)rsp;
	pkt = (const void *)&rsp[1];
	csum_ok = pkt->csum_calc && !pkt->err_vec;

	mbuf = t4_pktgl_to_mbuf(si);
	if (unlikely(!mbuf)) {
		rxq->stats.rx_drops++;
		return 0;
	}

	mbuf->port = pkt->iff;
	if (pkt->l2info & htonl(F_RXF_IP)) {
		mbuf->ol_flags |= PKT_RX_IPV4_HDR;
		if (unlikely(!csum_ok))
			mbuf->ol_flags |= PKT_RX_IP_CKSUM_BAD;

		if ((pkt->l2info & htonl(F_RXF_UDP | F_RXF_TCP)) && !csum_ok)
			mbuf->ol_flags |= PKT_RX_L4_CKSUM_BAD;
	} else if (pkt->l2info & htonl(F_RXF_IP6)) {
		mbuf->ol_flags |= PKT_RX_IPV6_HDR;
	}

	mbuf->port = pkt->iff;

	if (!rss_hdr->filter_tid && rss_hdr->hash_type) {
		mbuf->ol_flags |= PKT_RX_RSS_HASH;
		mbuf->hash.rss = ntohl(rss_hdr->hash_val);
	}

	if (pkt->vlan_ex) {
		mbuf->ol_flags |= PKT_RX_VLAN_PKT;
		mbuf->vlan_tci = ntohs(pkt->vlan);
	}
	rxq->stats.pkts++;
	rxq->stats.rx_bytes += mbuf->pkt_len;

	return 0;
}

/**
 * restore_rx_bufs - put back a packet's Rx buffers
 * @q: the SGE free list
 * @frags: number of FL buffers to restore
 *
 * Puts back on an FL the Rx buffers.  The buffers have already been
 * unmapped and are left unmapped, we mark them so to prevent further
 * unmapping attempts.
 *
 * This function undoes a series of @unmap_rx_buf calls when we find out
 * that the current packet can't be processed right away afterall and we
 * need to come back to it later.  This is a very rare event and there's
 * no effort to make this particularly efficient.
 */
static void restore_rx_bufs(struct sge_fl *q, int frags)
{
	while (frags--) {
		if (q->cidx == 0)
			q->cidx = q->size - 1;
		else
			q->cidx--;
		q->avail++;
	}
}

/**
 * is_new_response - check if a response is newly written
 * @r: the response descriptor
 * @q: the response queue
 *
 * Returns true if a response descriptor contains a yet unprocessed
 * response.
 */
static inline bool is_new_response(const struct rsp_ctrl *r,
				   const struct sge_rspq *q)
{
	return (r->u.type_gen >> S_RSPD_GEN) == q->gen;
}

#define CXGB4_MSG_AN ((void *)1)

/**
 * rspq_next - advance to the next entry in a response queue
 * @q: the queue
 *
 * Updates the state of a response queue to advance it to the next entry.
 */
static inline void rspq_next(struct sge_rspq *q)
{
	q->cur_desc = (const __be64 *)((const char *)q->cur_desc + q->iqe_len);
	if (unlikely(++q->cidx == q->size)) {
		q->cidx = 0;
		q->gen ^= 1;
		q->cur_desc = q->desc;
	}
}

/**
 * process_responses - process responses from an SGE response queue
 * @q: the ingress queue to process
 * @budget: how many responses can be processed in this round
 * @rx_pkts: mbuf to put the pkts
 *
 * Process responses from an SGE response queue up to the supplied budget.
 * Responses include received packets as well as control messages from FW
 * or HW.
 *
 * Additionally choose the interrupt holdoff time for the next interrupt
 * on this queue.  If the system is under memory shortage use a fairly
 * long delay to help recovery.
 */
static int process_responses(struct sge_rspq *q, int budget,
			     struct rte_mbuf **rx_pkts)
{
	int ret = 0, rsp_type;
	int budget_left = budget;
	const struct rsp_ctrl *rc;
	struct sge_eth_rxq *rxq = container_of(q, struct sge_eth_rxq, rspq);
	struct adapter *adapter = q->adapter;

	while (likely(budget_left)) {
		rc = (const struct rsp_ctrl *)
		     ((const char *)q->cur_desc + (q->iqe_len - sizeof(*rc)));

		if (!is_new_response(rc, q))
			break;

		/*
		 * Ensure response has been read
		 */
		rmb();
		rsp_type = G_RSPD_TYPE(rc->u.type_gen);

		if (likely(rsp_type == X_RSPD_TYPE_FLBUF)) {
			struct pkt_gl si;
			const struct rx_sw_desc *rsd;
			struct rte_mbuf *pkt = NULL;
			u32 len = ntohl(rc->pldbuflen_qid), bufsz, frags;

			si.usembufs = rxq->usembufs;
			/*
			 * In "use mbufs" mode, we don't pack multiple
			 * ingress packets per buffer (mbuf) so we
			 * should _always_ get a "New Buffer" flags
			 * from the SGE.  Also, since we hand the
			 * mbuf's up to the host stack for it to
			 * eventually free, we don't release the mbuf's
			 * in the driver (in contrast to the "packed
			 * page" mode where the driver needs to
			 * release its reference on the page buffers).
			 */
			BUG_ON(!(len & F_RSPD_NEWBUF));
			len = G_RSPD_LEN(len);
			si.tot_len = len;

			/* gather packet fragments */
			for (frags = 0; len; frags++) {
				rsd = &rxq->fl.sdesc[rxq->fl.cidx];
				bufsz = min(get_buf_size(adapter, rsd),	len);
				pkt = rsd->buf;
				pkt->data_len = bufsz;
				pkt->pkt_len = bufsz;
				si.mbufs[frags] = pkt;
				len -= bufsz;
				unmap_rx_buf(&rxq->fl);
			}

			si.va = RTE_PTR_ADD(si.mbufs[0]->buf_addr,
					    si.mbufs[0]->data_off);
			rte_prefetch1(si.va);

			/*
			 * For the "use mbuf" case here, we can end up
			 * chewing through our Free List very rapidly
			 * with one entry per Ingress packet getting
			 * consumed.  So if the handler() successfully
			 * consumed the mbuf, check to see if we can
			 * refill the Free List incrementally in the
			 * loop ...
			 */
			si.nfrags = frags;
			ret = q->handler(q, q->cur_desc, &si);

			if (unlikely(ret != 0)) {
				restore_rx_bufs(&rxq->fl, frags);
			} else {
				rx_pkts[budget - budget_left] = pkt;
				if (fl_cap(&rxq->fl) - rxq->fl.avail >= 8)
					__refill_fl(q->adapter, &rxq->fl);
			}

		} else if (likely(rsp_type == X_RSPD_TYPE_CPL)) {
			ret = q->handler(q, q->cur_desc, NULL);
		} else {
			ret = q->handler(q, (const __be64 *)rc, CXGB4_MSG_AN);
		}

		if (unlikely(ret)) {
			/* couldn't process descriptor, back off for recovery */
			q->next_intr_params = V_QINTR_TIMER_IDX(NOMEM_TMR_IDX);
			break;
		}

		rspq_next(q);
		budget_left--;
	}

	/*
	 * If this is a Response Queue with an associated Free List and
	 * there's room for another chunk of new Free List buffer pointers,
	 * refill the Free List.
	 */

	if (q->offset >= 0 && fl_cap(&rxq->fl) - rxq->fl.avail >= 8)
		__refill_fl(q->adapter, &rxq->fl);

	return budget - budget_left;
}

int cxgbe_poll(struct sge_rspq *q, struct rte_mbuf **rx_pkts,
	       unsigned int budget, unsigned int *work_done)
{
	unsigned int params;
	u32 val;
	int err = 0;

	*work_done = process_responses(q, budget, rx_pkts);
	params = V_QINTR_TIMER_IDX(X_TIMERREG_UPDATE_CIDX);
	q->next_intr_params = params;
	val = V_CIDXINC(*work_done) | V_SEINTARM(params);

	if (*work_done) {
		/*
		 * If we don't have access to the new User GTS (T5+),
		 * use the old doorbell mechanism; otherwise use the new
		 * BAR2 mechanism.
		 */
		if (unlikely(!q->bar2_addr))
			t4_write_reg(q->adapter, MYPF_REG(A_SGE_PF_GTS),
				     val | V_INGRESSQID((u32)q->cntxt_id));
		else {
			writel(val | V_INGRESSQID(q->bar2_qid),
			       (void *)((uintptr_t)q->bar2_addr +
			       SGE_UDB_GTS));
			/*
			 * This Write memory Barrier will force the write to
			 * the User Doorbell area to be flushed.
			 */
			wmb();
		}
	}

	return err;
}

/**
 * bar2_address - return the BAR2 address for an SGE Queue's Registers
 * @adapter: the adapter
 * @qid: the SGE Queue ID
 * @qtype: the SGE Queue Type (Egress or Ingress)
 * @pbar2_qid: BAR2 Queue ID or 0 for Queue ID inferred SGE Queues
 *
 * Returns the BAR2 address for the SGE Queue Registers associated with
 * @qid.  If BAR2 SGE Registers aren't available, returns NULL.  Also
 * returns the BAR2 Queue ID to be used with writes to the BAR2 SGE
 * Queue Registers.  If the BAR2 Queue ID is 0, then "Inferred Queue ID"
 * Registers are supported (e.g. the Write Combining Doorbell Buffer).
 */
static void __iomem *bar2_address(struct adapter *adapter, unsigned int qid,
				  enum t4_bar2_qtype qtype,
				  unsigned int *pbar2_qid)
{
	u64 bar2_qoffset;
	int ret;

	ret = t4_bar2_sge_qregs(adapter, qid, qtype, &bar2_qoffset, pbar2_qid);
	if (ret)
		return NULL;

	return adapter->bar2 + bar2_qoffset;
}

int t4_sge_eth_rxq_start(struct adapter *adap, struct sge_rspq *rq)
{
	struct sge_eth_rxq *rxq = container_of(rq, struct sge_eth_rxq, rspq);
	unsigned int fl_id = rxq->fl.size ? rxq->fl.cntxt_id : 0xffff;

	return t4_iq_start_stop(adap, adap->mbox, true, adap->pf, 0,
				rq->cntxt_id, fl_id, 0xffff);
}

int t4_sge_eth_rxq_stop(struct adapter *adap, struct sge_rspq *rq)
{
	struct sge_eth_rxq *rxq = container_of(rq, struct sge_eth_rxq, rspq);
	unsigned int fl_id = rxq->fl.size ? rxq->fl.cntxt_id : 0xffff;

	return t4_iq_start_stop(adap, adap->mbox, false, adap->pf, 0,
				rq->cntxt_id, fl_id, 0xffff);
}

/*
 * @intr_idx: MSI/MSI-X vector if >=0, -(absolute qid + 1) if < 0
 * @cong: < 0 -> no congestion feedback, >= 0 -> congestion channel map
 */
int t4_sge_alloc_rxq(struct adapter *adap, struct sge_rspq *iq, bool fwevtq,
		     struct rte_eth_dev *eth_dev, int intr_idx,
		     struct sge_fl *fl, rspq_handler_t hnd, int cong,
		     struct rte_mempool *mp, int queue_id, int socket_id)
{
	int ret, flsz = 0;
	struct fw_iq_cmd c;
	struct sge *s = &adap->sge;
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	char z_name[RTE_MEMZONE_NAMESIZE];
	char z_name_sw[RTE_MEMZONE_NAMESIZE];
	unsigned int nb_refill;

	/* Size needs to be multiple of 16, including status entry. */
	iq->size = roundup(iq->size, 16);

	snprintf(z_name, sizeof(z_name), "%s_%s_%d_%d",
		 eth_dev->driver->pci_drv.name, fwevtq ? "fwq_ring" : "rx_ring",
		 eth_dev->data->port_id, queue_id);
	snprintf(z_name_sw, sizeof(z_name_sw), "%s_sw_ring", z_name);

	iq->desc = alloc_ring(iq->size, iq->iqe_len, 0, &iq->phys_addr, NULL, 0,
			      queue_id, socket_id, z_name, z_name_sw);
	if (!iq->desc)
		return -ENOMEM;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = htonl(V_FW_CMD_OP(FW_IQ_CMD) | F_FW_CMD_REQUEST |
			    F_FW_CMD_WRITE | F_FW_CMD_EXEC |
			    V_FW_IQ_CMD_PFN(adap->pf) | V_FW_IQ_CMD_VFN(0));
	c.alloc_to_len16 = htonl(F_FW_IQ_CMD_ALLOC | F_FW_IQ_CMD_IQSTART |
				 (sizeof(c) / 16));
	c.type_to_iqandstindex =
		htonl(V_FW_IQ_CMD_TYPE(FW_IQ_TYPE_FL_INT_CAP) |
		      V_FW_IQ_CMD_IQASYNCH(fwevtq) |
		      V_FW_IQ_CMD_VIID(pi->viid) |
		      V_FW_IQ_CMD_IQANDST(intr_idx < 0) |
		      V_FW_IQ_CMD_IQANUD(X_UPDATEDELIVERY_INTERRUPT) |
		      V_FW_IQ_CMD_IQANDSTINDEX(intr_idx >= 0 ? intr_idx :
							       -intr_idx - 1));
	c.iqdroprss_to_iqesize =
		htons(V_FW_IQ_CMD_IQPCIECH(pi->tx_chan) |
		      F_FW_IQ_CMD_IQGTSMODE |
		      V_FW_IQ_CMD_IQINTCNTTHRESH(iq->pktcnt_idx) |
		      V_FW_IQ_CMD_IQESIZE(ilog2(iq->iqe_len) - 4));
	c.iqsize = htons(iq->size);
	c.iqaddr = cpu_to_be64(iq->phys_addr);
	if (cong >= 0)
		c.iqns_to_fl0congen = htonl(F_FW_IQ_CMD_IQFLINTCONGEN);

	if (fl) {
		struct sge_eth_rxq *rxq = container_of(fl, struct sge_eth_rxq,
						       fl);
		enum chip_type chip = CHELSIO_CHIP_VERSION(adap->params.chip);

		/*
		 * Allocate the ring for the hardware free list (with space
		 * for its status page) along with the associated software
		 * descriptor ring.  The free list size needs to be a multiple
		 * of the Egress Queue Unit and at least 2 Egress Units larger
		 * than the SGE's Egress Congrestion Threshold
		 * (fl_starve_thres - 1).
		 */
		if (fl->size < s->fl_starve_thres - 1 + 2 * 8)
			fl->size = s->fl_starve_thres - 1 + 2 * 8;
		fl->size = roundup(fl->size, 8);

		snprintf(z_name, sizeof(z_name), "%s_%s_%d_%d",
			 eth_dev->driver->pci_drv.name,
			 fwevtq ? "fwq_ring" : "fl_ring",
			 eth_dev->data->port_id, queue_id);
		snprintf(z_name_sw, sizeof(z_name_sw), "%s_sw_ring", z_name);

		fl->desc = alloc_ring(fl->size, sizeof(__be64),
				      sizeof(struct rx_sw_desc),
				      &fl->addr, &fl->sdesc, s->stat_len,
				      queue_id, socket_id, z_name, z_name_sw);

		if (!fl->desc)
			goto fl_nomem;

		flsz = fl->size / 8 + s->stat_len / sizeof(struct tx_desc);
		c.iqns_to_fl0congen |=
			htonl(V_FW_IQ_CMD_FL0HOSTFCMODE(X_HOSTFCMODE_NONE) |
			      (unlikely(rxq->usembufs) ?
			       0 : F_FW_IQ_CMD_FL0PACKEN) |
			      F_FW_IQ_CMD_FL0FETCHRO | F_FW_IQ_CMD_FL0DATARO |
			      F_FW_IQ_CMD_FL0PADEN);
		if (cong >= 0)
			c.iqns_to_fl0congen |=
				htonl(V_FW_IQ_CMD_FL0CNGCHMAP(cong) |
				      F_FW_IQ_CMD_FL0CONGCIF |
				      F_FW_IQ_CMD_FL0CONGEN);

		/* In T6, for egress queue type FL there is internal overhead
		 * of 16B for header going into FLM module.
		 * Hence maximum allowed burst size will be 448 bytes.
		 */
		c.fl0dcaen_to_fl0cidxfthresh =
			htons(V_FW_IQ_CMD_FL0FBMIN(X_FETCHBURSTMIN_64B) |
			      V_FW_IQ_CMD_FL0FBMAX((chip <= CHELSIO_T5) ?
			      X_FETCHBURSTMAX_512B : X_FETCHBURSTMAX_256B));
		c.fl0size = htons(flsz);
		c.fl0addr = cpu_to_be64(fl->addr);
	}

	ret = t4_wr_mbox(adap, adap->mbox, &c, sizeof(c), &c);
	if (ret)
		goto err;

	iq->cur_desc = iq->desc;
	iq->cidx = 0;
	iq->gen = 1;
	iq->next_intr_params = iq->intr_params;
	iq->cntxt_id = ntohs(c.iqid);
	iq->abs_id = ntohs(c.physiqid);
	iq->bar2_addr = bar2_address(adap, iq->cntxt_id, T4_BAR2_QTYPE_INGRESS,
				     &iq->bar2_qid);
	iq->size--;                           /* subtract status entry */
	iq->eth_dev = eth_dev;
	iq->handler = hnd;
	iq->mb_pool = mp;

	/* set offset to -1 to distinguish ingress queues without FL */
	iq->offset = fl ? 0 : -1;

	if (fl) {
		fl->cntxt_id = ntohs(c.fl0id);
		fl->avail = 0;
		fl->pend_cred = 0;
		fl->pidx = 0;
		fl->cidx = 0;
		fl->alloc_failed = 0;

		/*
		 * Note, we must initialize the BAR2 Free List User Doorbell
		 * information before refilling the Free List!
		 */
		fl->bar2_addr = bar2_address(adap, fl->cntxt_id,
					     T4_BAR2_QTYPE_EGRESS,
					     &fl->bar2_qid);

		nb_refill = refill_fl(adap, fl, fl_cap(fl));
		if (nb_refill != fl_cap(fl)) {
			ret = -ENOMEM;
			dev_err(adap, "%s: mbuf alloc failed with error: %d\n",
				__func__, ret);
			goto refill_fl_err;
		}
	}

	/*
	 * For T5 and later we attempt to set up the Congestion Manager values
	 * of the new RX Ethernet Queue.  This should really be handled by
	 * firmware because it's more complex than any host driver wants to
	 * get involved with and it's different per chip and this is almost
	 * certainly wrong.  Formware would be wrong as well, but it would be
	 * a lot easier to fix in one place ...  For now we do something very
	 * simple (and hopefully less wrong).
	 */
	if (!is_t4(adap->params.chip) && cong >= 0) {
		u32 param, val;
		int i;

		param = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DMAQ) |
			 V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DMAQ_CONM_CTXT) |
			 V_FW_PARAMS_PARAM_YZ(iq->cntxt_id));
		if (cong == 0) {
			val = V_CONMCTXT_CNGTPMODE(X_CONMCTXT_CNGTPMODE_QUEUE);
		} else {
			val = V_CONMCTXT_CNGTPMODE(
					X_CONMCTXT_CNGTPMODE_CHANNEL);
			for (i = 0; i < 4; i++) {
				if (cong & (1 << i))
					val |= V_CONMCTXT_CNGCHMAP(1 <<
								   (i << 2));
			}
		}
		ret = t4_set_params(adap, adap->mbox, adap->pf, 0, 1,
				    &param, &val);
		if (ret)
			dev_warn(adap->pdev_dev, "Failed to set Congestion Manager Context for Ingress Queue %d: %d\n",
				 iq->cntxt_id, -ret);
	}

	return 0;

refill_fl_err:
	t4_iq_free(adap, adap->mbox, adap->pf, 0, FW_IQ_TYPE_FL_INT_CAP,
		   iq->cntxt_id, fl ? fl->cntxt_id : 0xffff, 0xffff);
fl_nomem:
	ret = -ENOMEM;
err:
	iq->cntxt_id = 0;
	iq->abs_id = 0;
	if (iq->desc)
		iq->desc = NULL;

	if (fl && fl->desc) {
		rte_free(fl->sdesc);
		fl->cntxt_id = 0;
		fl->sdesc = NULL;
		fl->desc = NULL;
	}
	return ret;
}

static void free_rspq_fl(struct adapter *adap, struct sge_rspq *rq,
			 struct sge_fl *fl)
{
	unsigned int fl_id = fl ? fl->cntxt_id : 0xffff;

	t4_iq_free(adap, adap->mbox, adap->pf, 0, FW_IQ_TYPE_FL_INT_CAP,
		   rq->cntxt_id, fl_id, 0xffff);
	rq->cntxt_id = 0;
	rq->abs_id = 0;
	rq->desc = NULL;

	if (fl) {
		free_rx_bufs(fl, fl->avail);
		rte_free(fl->sdesc);
		fl->sdesc = NULL;
		fl->cntxt_id = 0;
		fl->desc = NULL;
	}
}

void t4_sge_eth_rxq_release(struct adapter *adap, struct sge_eth_rxq *rxq)
{
	if (rxq->rspq.desc) {
		t4_sge_eth_rxq_stop(adap, &rxq->rspq);
		free_rspq_fl(adap, &rxq->rspq, rxq->fl.size ? &rxq->fl : NULL);
	}
}

/**
 * t4_sge_init - initialize SGE
 * @adap: the adapter
 *
 * Performs SGE initialization needed every time after a chip reset.
 * We do not initialize any of the queues here, instead the driver
 * top-level must request those individually.
 *
 * Called in two different modes:
 *
 *  1. Perform actual hardware initialization and record hard-coded
 *     parameters which were used.  This gets used when we're the
 *     Master PF and the Firmware Configuration File support didn't
 *     work for some reason.
 *
 *  2. We're not the Master PF or initialization was performed with
 *     a Firmware Configuration File.  In this case we need to grab
 *     any of the SGE operating parameters that we need to have in
 *     order to do our job and make sure we can live with them ...
 */
static int t4_sge_init_soft(struct adapter *adap)
{
	struct sge *s = &adap->sge;
	u32 fl_small_pg, fl_large_pg, fl_small_mtu, fl_large_mtu;
	u32 timer_value_0_and_1, timer_value_2_and_3, timer_value_4_and_5;
	u32 ingress_rx_threshold;

	/*
	 * Verify that CPL messages are going to the Ingress Queue for
	 * process_responses() and that only packet data is going to the
	 * Free Lists.
	 */
	if ((t4_read_reg(adap, A_SGE_CONTROL) & F_RXPKTCPLMODE) !=
	    V_RXPKTCPLMODE(X_RXPKTCPLMODE_SPLIT)) {
		dev_err(adap, "bad SGE CPL MODE\n");
		return -EINVAL;
	}

	/*
	 * Validate the Host Buffer Register Array indices that we want to
	 * use ...
	 *
	 * XXX Note that we should really read through the Host Buffer Size
	 * XXX register array and find the indices of the Buffer Sizes which
	 * XXX meet our needs!
	 */
#define READ_FL_BUF(x) \
	t4_read_reg(adap, A_SGE_FL_BUFFER_SIZE0 + (x) * sizeof(u32))

	fl_small_pg = READ_FL_BUF(RX_SMALL_PG_BUF);
	fl_large_pg = READ_FL_BUF(RX_LARGE_PG_BUF);
	fl_small_mtu = READ_FL_BUF(RX_SMALL_MTU_BUF);
	fl_large_mtu = READ_FL_BUF(RX_LARGE_MTU_BUF);

	/*
	 * We only bother using the Large Page logic if the Large Page Buffer
	 * is larger than our Page Size Buffer.
	 */
	if (fl_large_pg <= fl_small_pg)
		fl_large_pg = 0;

#undef READ_FL_BUF

	/*
	 * The Page Size Buffer must be exactly equal to our Page Size and the
	 * Large Page Size Buffer should be 0 (per above) or a power of 2.
	 */
	if (fl_small_pg != PAGE_SIZE ||
	    (fl_large_pg & (fl_large_pg - 1)) != 0) {
		dev_err(adap, "bad SGE FL page buffer sizes [%d, %d]\n",
			fl_small_pg, fl_large_pg);
		return -EINVAL;
	}
	if (fl_large_pg)
		s->fl_pg_order = ilog2(fl_large_pg) - PAGE_SHIFT;

	if (adap->use_unpacked_mode) {
		int err = 0;

		if (fl_small_mtu < FL_MTU_SMALL_BUFSIZE(adap)) {
			dev_err(adap, "bad SGE FL small MTU %d\n",
				fl_small_mtu);
			err = -EINVAL;
		}
		if (fl_large_mtu < FL_MTU_LARGE_BUFSIZE(adap)) {
			dev_err(adap, "bad SGE FL large MTU %d\n",
				fl_large_mtu);
			err = -EINVAL;
		}
		if (err)
			return err;
	}

	/*
	 * Retrieve our RX interrupt holdoff timer values and counter
	 * threshold values from the SGE parameters.
	 */
	timer_value_0_and_1 = t4_read_reg(adap, A_SGE_TIMER_VALUE_0_AND_1);
	timer_value_2_and_3 = t4_read_reg(adap, A_SGE_TIMER_VALUE_2_AND_3);
	timer_value_4_and_5 = t4_read_reg(adap, A_SGE_TIMER_VALUE_4_AND_5);
	s->timer_val[0] = core_ticks_to_us(adap,
					   G_TIMERVALUE0(timer_value_0_and_1));
	s->timer_val[1] = core_ticks_to_us(adap,
					   G_TIMERVALUE1(timer_value_0_and_1));
	s->timer_val[2] = core_ticks_to_us(adap,
					   G_TIMERVALUE2(timer_value_2_and_3));
	s->timer_val[3] = core_ticks_to_us(adap,
					   G_TIMERVALUE3(timer_value_2_and_3));
	s->timer_val[4] = core_ticks_to_us(adap,
					   G_TIMERVALUE4(timer_value_4_and_5));
	s->timer_val[5] = core_ticks_to_us(adap,
					   G_TIMERVALUE5(timer_value_4_and_5));

	ingress_rx_threshold = t4_read_reg(adap, A_SGE_INGRESS_RX_THRESHOLD);
	s->counter_val[0] = G_THRESHOLD_0(ingress_rx_threshold);
	s->counter_val[1] = G_THRESHOLD_1(ingress_rx_threshold);
	s->counter_val[2] = G_THRESHOLD_2(ingress_rx_threshold);
	s->counter_val[3] = G_THRESHOLD_3(ingress_rx_threshold);

	return 0;
}

int t4_sge_init(struct adapter *adap)
{
	struct sge *s = &adap->sge;
	u32 sge_control, sge_control2, sge_conm_ctrl;
	unsigned int ingpadboundary, ingpackboundary;
	int ret, egress_threshold;

	/*
	 * Ingress Padding Boundary and Egress Status Page Size are set up by
	 * t4_fixup_host_params().
	 */
	sge_control = t4_read_reg(adap, A_SGE_CONTROL);
	s->pktshift = G_PKTSHIFT(sge_control);
	s->stat_len = (sge_control & F_EGRSTATUSPAGESIZE) ? 128 : 64;

	/*
	 * T4 uses a single control field to specify both the PCIe Padding and
	 * Packing Boundary.  T5 introduced the ability to specify these
	 * separately.  The actual Ingress Packet Data alignment boundary
	 * within Packed Buffer Mode is the maximum of these two
	 * specifications.
	 */
	ingpadboundary = 1 << (G_INGPADBOUNDARY(sge_control) +
			 X_INGPADBOUNDARY_SHIFT);
	s->fl_align = ingpadboundary;

	if (!is_t4(adap->params.chip) && !adap->use_unpacked_mode) {
		/*
		 * T5 has a weird interpretation of one of the PCIe Packing
		 * Boundary values.  No idea why ...
		 */
		sge_control2 = t4_read_reg(adap, A_SGE_CONTROL2);
		ingpackboundary = G_INGPACKBOUNDARY(sge_control2);
		if (ingpackboundary == X_INGPACKBOUNDARY_16B)
			ingpackboundary = 16;
		else
			ingpackboundary = 1 << (ingpackboundary +
					  X_INGPACKBOUNDARY_SHIFT);

		s->fl_align = max(ingpadboundary, ingpackboundary);
	}

	ret = t4_sge_init_soft(adap);
	if (ret < 0) {
		dev_err(adap, "%s: t4_sge_init_soft failed, error %d\n",
			__func__, -ret);
		return ret;
	}

	/*
	 * A FL with <= fl_starve_thres buffers is starving and a periodic
	 * timer will attempt to refill it.  This needs to be larger than the
	 * SGE's Egress Congestion Threshold.  If it isn't, then we can get
	 * stuck waiting for new packets while the SGE is waiting for us to
	 * give it more Free List entries.  (Note that the SGE's Egress
	 * Congestion Threshold is in units of 2 Free List pointers.)  For T4,
	 * there was only a single field to control this.  For T5 there's the
	 * original field which now only applies to Unpacked Mode Free List
	 * buffers and a new field which only applies to Packed Mode Free List
	 * buffers.
	 */
	sge_conm_ctrl = t4_read_reg(adap, A_SGE_CONM_CTRL);
	if (is_t4(adap->params.chip) || adap->use_unpacked_mode)
		egress_threshold = G_EGRTHRESHOLD(sge_conm_ctrl);
	else
		egress_threshold = G_EGRTHRESHOLDPACKING(sge_conm_ctrl);
	s->fl_starve_thres = 2 * egress_threshold + 1;

	return 0;
}
