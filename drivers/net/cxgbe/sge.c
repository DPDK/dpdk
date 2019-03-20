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
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_alarm.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_dev.h>

#include "base/common.h"
#include "base/t4_regs.h"
#include "base/t4_msg.h"
#include "cxgbe.h"

static inline void ship_tx_pkt_coalesce_wr(struct adapter *adap,
					   struct sge_eth_txq *txq);

/*
 * Max number of Rx buffers we replenish at a time.
 */
#define MAX_RX_REFILL 64U

#define NOMEM_TMR_IDX (SGE_NTIMERS - 1)

/*
 * Max Tx descriptor space we allow for an Ethernet packet to be inlined
 * into a WR.
 */
#define MAX_IMM_TX_PKT_LEN 256

/*
 * Max size of a WR sent through a control Tx queue.
 */
#define MAX_CTRL_WR_LEN SGE_MAX_WR_LEN

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

	return CXGBE_ALIGN(s->pktshift + ETHER_HDR_LEN + VLAN_HLEN + mtu,
			   s->fl_align);
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
 * txq_avail - return the number of available slots in a Tx queue
 * @q: the Tx queue
 *
 * Returns the number of descriptors in a Tx queue available to write new
 * packets.
 */
static inline unsigned int txq_avail(const struct sge_txq *q)
{
	return q->size - 1 - q->in_use;
}

static int map_mbuf(struct rte_mbuf *mbuf, dma_addr_t *addr)
{
	struct rte_mbuf *m = mbuf;

	for (; m; m = m->next, addr++) {
		*addr = m->buf_iova + rte_pktmbuf_headroom(m);
		if (*addr == 0)
			goto out_err;
	}
	return 0;

out_err:
	return -ENOMEM;
}

/**
 * free_tx_desc - reclaims Tx descriptors and their buffers
 * @q: the Tx queue to reclaim descriptors from
 * @n: the number of descriptors to reclaim
 *
 * Reclaims Tx descriptors from an SGE Tx queue and frees the associated
 * Tx buffers.  Called with the Tx queue lock held.
 */
static void free_tx_desc(struct sge_txq *q, unsigned int n)
{
	struct tx_sw_desc *d;
	unsigned int cidx = 0;

	d = &q->sdesc[cidx];
	while (n--) {
		if (d->mbuf) {                       /* an SGL is present */
			rte_pktmbuf_free(d->mbuf);
			d->mbuf = NULL;
		}
		if (d->coalesce.idx) {
			int i;

			for (i = 0; i < d->coalesce.idx; i++) {
				rte_pktmbuf_free(d->coalesce.mbuf[i]);
				d->coalesce.mbuf[i] = NULL;
			}
			d->coalesce.idx = 0;
		}
		++d;
		if (++cidx == q->size) {
			cidx = 0;
			d = q->sdesc;
		}
		RTE_MBUF_PREFETCH_TO_FREE(&q->sdesc->mbuf->pool);
	}
}

static void reclaim_tx_desc(struct sge_txq *q, unsigned int n)
{
	struct tx_sw_desc *d;
	unsigned int cidx = q->cidx;

	d = &q->sdesc[cidx];
	while (n--) {
		if (d->mbuf) {                       /* an SGL is present */
			rte_pktmbuf_free(d->mbuf);
			d->mbuf = NULL;
		}
		++d;
		if (++cidx == q->size) {
			cidx = 0;
			d = q->sdesc;
		}
	}
	q->cidx = cidx;
}

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
	unsigned int rx_buf_size_idx = d->dma_addr & RX_BUF_SIZE;
	unsigned int buf_size = 0;

	switch (rx_buf_size_idx) {
	case RX_SMALL_MTU_BUF:
		buf_size = FL_MTU_SMALL_BUFSIZE(adapter);
		break;

	case RX_LARGE_MTU_BUF:
		buf_size = FL_MTU_LARGE_BUFSIZE(adapter);
		break;

	default:
		BUG_ON(1);
		/* NOT REACHED */
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
	if (q->pend_cred >= 64) {
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
			u32 reg = is_pf4(adap) ? MYPF_REG(A_SGE_PF_KDOORBELL) :
						 T4VF_SGE_BASE_ADDR +
						 A_SGE_VF_KDOORBELL;

			t4_write_reg_relaxed(adap, reg,
					     val | V_QID(q->cntxt_id));
		} else {
			writel_relaxed(val | V_QID(q->bar2_qid),
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
	struct rte_mbuf *buf_bulk[n];
	int ret, i;
	struct rte_pktmbuf_pool_private *mbp_priv;
	u8 jumbo_en = rxq->rspq.eth_dev->data->dev_conf.rxmode.offloads &
		DEV_RX_OFFLOAD_JUMBO_FRAME;

	/* Use jumbo mtu buffers if mbuf data room size can fit jumbo data. */
	mbp_priv = rte_mempool_get_priv(rxq->rspq.mb_pool);
	if (jumbo_en &&
	    ((mbp_priv->mbuf_data_room_size - RTE_PKTMBUF_HEADROOM) >= 9000))
		buf_size_idx = RX_LARGE_MTU_BUF;

	ret = rte_mempool_get_bulk(rxq->rspq.mb_pool, (void *)buf_bulk, n);
	if (unlikely(ret != 0)) {
		dev_debug(adap, "%s: failed to allocated fl entries in bulk ..\n",
			  __func__);
		q->alloc_failed++;
		rxq->rspq.eth_dev->data->rx_mbuf_alloc_failed++;
		goto out;
	}

	for (i = 0; i < n; i++) {
		struct rte_mbuf *mbuf = buf_bulk[i];
		dma_addr_t mapping;

		if (!mbuf) {
			dev_debug(adap, "%s: mbuf alloc failed\n", __func__);
			q->alloc_failed++;
			rxq->rspq.eth_dev->data->rx_mbuf_alloc_failed++;
			goto out;
		}

		rte_mbuf_refcnt_set(mbuf, 1);
		mbuf->data_off =
			(uint16_t)((char *)
				   RTE_PTR_ALIGN((char *)mbuf->buf_addr +
						 RTE_PKTMBUF_HEADROOM,
						 adap->sge.fl_align) -
				   (char *)mbuf->buf_addr);
		mbuf->next = NULL;
		mbuf->nb_segs = 1;
		mbuf->port = rxq->rspq.port_id;

		mapping = (dma_addr_t)RTE_ALIGN(mbuf->buf_iova +
						mbuf->data_off,
						adap->sge.fl_align);
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

/*
 * Return the number of reclaimable descriptors in a Tx queue.
 */
static inline int reclaimable(const struct sge_txq *q)
{
	int hw_cidx = ntohs(q->stat->cidx);

	hw_cidx -= q->cidx;
	if (hw_cidx < 0)
		return hw_cidx + q->size;
	return hw_cidx;
}

/**
 * reclaim_completed_tx - reclaims completed Tx descriptors
 * @q: the Tx queue to reclaim completed descriptors from
 *
 * Reclaims Tx descriptors that the SGE has indicated it has processed.
 */
void reclaim_completed_tx(struct sge_txq *q)
{
	unsigned int avail = reclaimable(q);

	do {
		/* reclaim as much as possible */
		reclaim_tx_desc(q, avail);
		q->in_use -= avail;
		avail = reclaimable(q);
	} while (avail);
}

/**
 * sgl_len - calculates the size of an SGL of the given capacity
 * @n: the number of SGL entries
 *
 * Calculates the number of flits needed for a scatter/gather list that
 * can hold the given number of entries.
 */
static inline unsigned int sgl_len(unsigned int n)
{
	/*
	 * A Direct Scatter Gather List uses 32-bit lengths and 64-bit PCI DMA
	 * addresses.  The DSGL Work Request starts off with a 32-bit DSGL
	 * ULPTX header, then Length0, then Address0, then, for 1 <= i <= N,
	 * repeated sequences of { Length[i], Length[i+1], Address[i],
	 * Address[i+1] } (this ensures that all addresses are on 64-bit
	 * boundaries).  If N is even, then Length[N+1] should be set to 0 and
	 * Address[N+1] is omitted.
	 *
	 * The following calculation incorporates all of the above.  It's
	 * somewhat hard to follow but, briefly: the "+2" accounts for the
	 * first two flits which include the DSGL header, Length0 and
	 * Address0; the "(3*(n-1))/2" covers the main body of list entries (3
	 * flits for every pair of the remaining N) +1 if (n-1) is odd; and
	 * finally the "+((n-1)&1)" adds the one remaining flit needed if
	 * (n-1) is odd ...
	 */
	n--;
	return (3 * n) / 2 + (n & 1) + 2;
}

/**
 * flits_to_desc - returns the num of Tx descriptors for the given flits
 * @n: the number of flits
 *
 * Returns the number of Tx descriptors needed for the supplied number
 * of flits.
 */
static inline unsigned int flits_to_desc(unsigned int n)
{
	return DIV_ROUND_UP(n, 8);
}

/**
 * is_eth_imm - can an Ethernet packet be sent as immediate data?
 * @m: the packet
 *
 * Returns whether an Ethernet packet is small enough to fit as
 * immediate data. Return value corresponds to the headroom required.
 */
static inline int is_eth_imm(const struct rte_mbuf *m)
{
	unsigned int hdrlen = (m->ol_flags & PKT_TX_TCP_SEG) ?
			      sizeof(struct cpl_tx_pkt_lso_core) : 0;

	hdrlen += sizeof(struct cpl_tx_pkt);
	if (m->pkt_len <= MAX_IMM_TX_PKT_LEN - hdrlen)
		return hdrlen;

	return 0;
}

/**
 * calc_tx_flits - calculate the number of flits for a packet Tx WR
 * @m: the packet
 * @adap: adapter structure pointer
 *
 * Returns the number of flits needed for a Tx WR for the given Ethernet
 * packet, including the needed WR and CPL headers.
 */
static inline unsigned int calc_tx_flits(const struct rte_mbuf *m,
					 struct adapter *adap)
{
	size_t wr_size = is_pf4(adap) ? sizeof(struct fw_eth_tx_pkt_wr) :
					sizeof(struct fw_eth_tx_pkt_vm_wr);
	unsigned int flits;
	int hdrlen;

	/*
	 * If the mbuf is small enough, we can pump it out as a work request
	 * with only immediate data.  In that case we just have to have the
	 * TX Packet header plus the mbuf data in the Work Request.
	 */

	hdrlen = is_eth_imm(m);
	if (hdrlen)
		return DIV_ROUND_UP(m->pkt_len + hdrlen, sizeof(__be64));

	/*
	 * Otherwise, we're going to have to construct a Scatter gather list
	 * of the mbuf body and fragments.  We also include the flits necessary
	 * for the TX Packet Work Request and CPL.  We always have a firmware
	 * Write Header (incorporated as part of the cpl_tx_pkt_lso and
	 * cpl_tx_pkt structures), followed by either a TX Packet Write CPL
	 * message or, if we're doing a Large Send Offload, an LSO CPL message
	 * with an embedded TX Packet Write CPL message.
	 */
	flits = sgl_len(m->nb_segs);
	if (m->tso_segsz)
		flits += (wr_size + sizeof(struct cpl_tx_pkt_lso_core) +
			  sizeof(struct cpl_tx_pkt_core)) / sizeof(__be64);
	else
		flits += (wr_size +
			  sizeof(struct cpl_tx_pkt_core)) / sizeof(__be64);
	return flits;
}

/**
 * write_sgl - populate a scatter/gather list for a packet
 * @mbuf: the packet
 * @q: the Tx queue we are writing into
 * @sgl: starting location for writing the SGL
 * @end: points right after the end of the SGL
 * @start: start offset into mbuf main-body data to include in the SGL
 * @addr: address of mapped region
 *
 * Generates a scatter/gather list for the buffers that make up a packet.
 * The caller must provide adequate space for the SGL that will be written.
 * The SGL includes all of the packet's page fragments and the data in its
 * main body except for the first @start bytes.  @sgl must be 16-byte
 * aligned and within a Tx descriptor with available space.  @end points
 * write after the end of the SGL but does not account for any potential
 * wrap around, i.e., @end > @sgl.
 */
static void write_sgl(struct rte_mbuf *mbuf, struct sge_txq *q,
		      struct ulptx_sgl *sgl, u64 *end, unsigned int start,
		      const dma_addr_t *addr)
{
	unsigned int i, len;
	struct ulptx_sge_pair *to;
	struct rte_mbuf *m = mbuf;
	unsigned int nfrags = m->nb_segs;
	struct ulptx_sge_pair buf[nfrags / 2];

	len = m->data_len - start;
	sgl->len0 = htonl(len);
	sgl->addr0 = rte_cpu_to_be_64(addr[0]);

	sgl->cmd_nsge = htonl(V_ULPTX_CMD(ULP_TX_SC_DSGL) |
			      V_ULPTX_NSGE(nfrags));
	if (likely(--nfrags == 0))
		return;
	/*
	 * Most of the complexity below deals with the possibility we hit the
	 * end of the queue in the middle of writing the SGL.  For this case
	 * only we create the SGL in a temporary buffer and then copy it.
	 */
	to = (u8 *)end > (u8 *)q->stat ? buf : sgl->sge;

	for (i = 0; nfrags >= 2; nfrags -= 2, to++) {
		m = m->next;
		to->len[0] = rte_cpu_to_be_32(m->data_len);
		to->addr[0] = rte_cpu_to_be_64(addr[++i]);
		m = m->next;
		to->len[1] = rte_cpu_to_be_32(m->data_len);
		to->addr[1] = rte_cpu_to_be_64(addr[++i]);
	}
	if (nfrags) {
		m = m->next;
		to->len[0] = rte_cpu_to_be_32(m->data_len);
		to->len[1] = rte_cpu_to_be_32(0);
		to->addr[0] = rte_cpu_to_be_64(addr[i + 1]);
	}
	if (unlikely((u8 *)end > (u8 *)q->stat)) {
		unsigned int part0 = RTE_PTR_DIFF((u8 *)q->stat,
						  (u8 *)sgl->sge);
		unsigned int part1;

		if (likely(part0))
			memcpy(sgl->sge, buf, part0);
		part1 = RTE_PTR_DIFF((u8 *)end, (u8 *)q->stat);
		rte_memcpy(q->desc, RTE_PTR_ADD((u8 *)buf, part0), part1);
		end = RTE_PTR_ADD((void *)q->desc, part1);
	}
	if ((uintptr_t)end & 8)           /* 0-pad to multiple of 16 */
		*(u64 *)end = 0;
}

#define IDXDIFF(head, tail, wrap) \
	((head) >= (tail) ? (head) - (tail) : (wrap) - (tail) + (head))

#define Q_IDXDIFF(q, idx) IDXDIFF((q)->pidx, (q)->idx, (q)->size)
#define R_IDXDIFF(q, idx) IDXDIFF((q)->cidx, (q)->idx, (q)->size)

#define PIDXDIFF(head, tail, wrap) \
	((tail) >= (head) ? (tail) - (head) : (wrap) - (head) + (tail))
#define P_IDXDIFF(q, idx) PIDXDIFF((q)->cidx, idx, (q)->size)

/**
 * ring_tx_db - ring a Tx queue's doorbell
 * @adap: the adapter
 * @q: the Tx queue
 * @n: number of new descriptors to give to HW
 *
 * Ring the doorbel for a Tx queue.
 */
static inline void ring_tx_db(struct adapter *adap, struct sge_txq *q)
{
	int n = Q_IDXDIFF(q, dbidx);

	/*
	 * Make sure that all writes to the TX Descriptors are committed
	 * before we tell the hardware about them.
	 */
	rte_wmb();

	/*
	 * If we don't have access to the new User Doorbell (T5+), use the old
	 * doorbell mechanism; otherwise use the new BAR2 mechanism.
	 */
	if (unlikely(!q->bar2_addr)) {
		u32 val = V_PIDX(n);

		/*
		 * For T4 we need to participate in the Doorbell Recovery
		 * mechanism.
		 */
		if (!q->db_disabled)
			t4_write_reg(adap, MYPF_REG(A_SGE_PF_KDOORBELL),
				     V_QID(q->cntxt_id) | val);
		else
			q->db_pidx_inc += n;
		q->db_pidx = q->pidx;
	} else {
		u32 val = V_PIDX_T5(n);

		/*
		 * T4 and later chips share the same PIDX field offset within
		 * the doorbell, but T5 and later shrank the field in order to
		 * gain a bit for Doorbell Priority.  The field was absurdly
		 * large in the first place (14 bits) so we just use the T5
		 * and later limits and warn if a Queue ID is too large.
		 */
		WARN_ON(val & F_DBPRIO);

		writel(val | V_QID(q->bar2_qid),
		       (void *)((uintptr_t)q->bar2_addr + SGE_UDB_KDOORBELL));

		/*
		 * This Write Memory Barrier will force the write to the User
		 * Doorbell area to be flushed.  This is needed to prevent
		 * writes on different CPUs for the same queue from hitting
		 * the adapter out of order.  This is required when some Work
		 * Requests take the Write Combine Gather Buffer path (user
		 * doorbell area offset [SGE_UDB_WCDOORBELL..+63]) and some
		 * take the traditional path where we simply increment the
		 * PIDX (User Doorbell area SGE_UDB_KDOORBELL) and have the
		 * hardware DMA read the actual Work Request.
		 */
		rte_wmb();
	}
	q->dbidx = q->pidx;
}

/*
 * Figure out what HW csum a packet wants and return the appropriate control
 * bits.
 */
static u64 hwcsum(enum chip_type chip, const struct rte_mbuf *m)
{
	int csum_type;

	if (m->ol_flags & PKT_TX_IP_CKSUM) {
		switch (m->ol_flags & PKT_TX_L4_MASK) {
		case PKT_TX_TCP_CKSUM:
			csum_type = TX_CSUM_TCPIP;
			break;
		case PKT_TX_UDP_CKSUM:
			csum_type = TX_CSUM_UDPIP;
			break;
		default:
			goto nocsum;
		}
	} else {
		goto nocsum;
	}

	if (likely(csum_type >= TX_CSUM_TCPIP)) {
		u64 hdr_len = V_TXPKT_IPHDR_LEN(m->l3_len);
		int eth_hdr_len = m->l2_len;

		if (CHELSIO_CHIP_VERSION(chip) <= CHELSIO_T5)
			hdr_len |= V_TXPKT_ETHHDR_LEN(eth_hdr_len);
		else
			hdr_len |= V_T6_TXPKT_ETHHDR_LEN(eth_hdr_len);
		return V_TXPKT_CSUM_TYPE(csum_type) | hdr_len;
	}
nocsum:
	/*
	 * unknown protocol, disable HW csum
	 * and hope a bad packet is detected
	 */
	return F_TXPKT_L4CSUM_DIS;
}

static inline void txq_advance(struct sge_txq *q, unsigned int n)
{
	q->in_use += n;
	q->pidx += n;
	if (q->pidx >= q->size)
		q->pidx -= q->size;
}

#define MAX_COALESCE_LEN 64000

static inline int wraps_around(struct sge_txq *q, int ndesc)
{
	return (q->pidx + ndesc) > q->size ? 1 : 0;
}

static void tx_timer_cb(void *data)
{
	struct adapter *adap = (struct adapter *)data;
	struct sge_eth_txq *txq = &adap->sge.ethtxq[0];
	int i;
	unsigned int coal_idx;

	/* monitor any pending tx */
	for (i = 0; i < adap->sge.max_ethqsets; i++, txq++) {
		if (t4_os_trylock(&txq->txq_lock)) {
			coal_idx = txq->q.coalesce.idx;
			if (coal_idx) {
				if (coal_idx == txq->q.last_coal_idx &&
				    txq->q.pidx == txq->q.last_pidx) {
					ship_tx_pkt_coalesce_wr(adap, txq);
				} else {
					txq->q.last_coal_idx = coal_idx;
					txq->q.last_pidx = txq->q.pidx;
				}
			}
			t4_os_unlock(&txq->txq_lock);
		}
	}
	rte_eal_alarm_set(50, tx_timer_cb, (void *)adap);
}

/**
 * ship_tx_pkt_coalesce_wr - finalizes and ships a coalesce WR
 * @ adap: adapter structure
 * @txq: tx queue
 *
 * writes the different fields of the pkts WR and sends it.
 */
static inline void ship_tx_pkt_coalesce_wr(struct adapter *adap,
					   struct sge_eth_txq *txq)
{
	struct fw_eth_tx_pkts_vm_wr *vmwr;
	const size_t fw_hdr_copy_len = (sizeof(vmwr->ethmacdst) +
					sizeof(vmwr->ethmacsrc) +
					sizeof(vmwr->ethtype) +
					sizeof(vmwr->vlantci));
	struct fw_eth_tx_pkts_wr *wr;
	struct sge_txq *q = &txq->q;
	unsigned int ndesc;
	u32 wr_mid;

	/* fill the pkts WR header */
	wr = (void *)&q->desc[q->pidx];
	wr->op_pkd = htonl(V_FW_WR_OP(FW_ETH_TX_PKTS2_WR));
	vmwr = (void *)&q->desc[q->pidx];

	wr_mid = V_FW_WR_LEN16(DIV_ROUND_UP(q->coalesce.flits, 2));
	ndesc = flits_to_desc(q->coalesce.flits);
	wr->equiq_to_len16 = htonl(wr_mid);
	wr->plen = cpu_to_be16(q->coalesce.len);
	wr->npkt = q->coalesce.idx;
	wr->r3 = 0;
	if (is_pf4(adap)) {
		wr->op_pkd = htonl(V_FW_WR_OP(FW_ETH_TX_PKTS2_WR));
		wr->type = q->coalesce.type;
	} else {
		wr->op_pkd = htonl(V_FW_WR_OP(FW_ETH_TX_PKTS_VM_WR));
		vmwr->r4 = 0;
		memcpy((void *)vmwr->ethmacdst, (void *)q->coalesce.ethmacdst,
		       fw_hdr_copy_len);
	}

	/* zero out coalesce structure members */
	memset((void *)&q->coalesce, 0, sizeof(struct eth_coalesce));

	txq_advance(q, ndesc);
	txq->stats.coal_wr++;
	txq->stats.coal_pkts += wr->npkt;

	if (Q_IDXDIFF(q, equeidx) >= q->size / 2) {
		q->equeidx = q->pidx;
		wr_mid |= F_FW_WR_EQUEQ;
		wr->equiq_to_len16 = htonl(wr_mid);
	}
	ring_tx_db(adap, q);
}

/**
 * should_tx_packet_coalesce - decides wether to coalesce an mbuf or not
 * @txq: tx queue where the mbuf is sent
 * @mbuf: mbuf to be sent
 * @nflits: return value for number of flits needed
 * @adap: adapter structure
 *
 * This function decides if a packet should be coalesced or not.
 */
static inline int should_tx_packet_coalesce(struct sge_eth_txq *txq,
					    struct rte_mbuf *mbuf,
					    unsigned int *nflits,
					    struct adapter *adap)
{
	struct fw_eth_tx_pkts_vm_wr *wr;
	const size_t fw_hdr_copy_len = (sizeof(wr->ethmacdst) +
					sizeof(wr->ethmacsrc) +
					sizeof(wr->ethtype) +
					sizeof(wr->vlantci));
	struct sge_txq *q = &txq->q;
	unsigned int flits, ndesc;
	unsigned char type = 0;
	int credits, wr_size;

	/* use coal WR type 1 when no frags are present */
	type = (mbuf->nb_segs == 1) ? 1 : 0;
	if (!is_pf4(adap)) {
		if (!type)
			return 0;

		if (q->coalesce.idx && memcmp((void *)q->coalesce.ethmacdst,
					      rte_pktmbuf_mtod(mbuf, void *),
					      fw_hdr_copy_len))
			ship_tx_pkt_coalesce_wr(adap, txq);
	}

	if (unlikely(type != q->coalesce.type && q->coalesce.idx))
		ship_tx_pkt_coalesce_wr(adap, txq);

	/* calculate the number of flits required for coalescing this packet
	 * without the 2 flits of the WR header. These are added further down
	 * if we are just starting in new PKTS WR. sgl_len doesn't account for
	 * the possible 16 bytes alignment ULP TX commands so we do it here.
	 */
	flits = (sgl_len(mbuf->nb_segs) + 1) & ~1U;
	if (type == 0)
		flits += (sizeof(struct ulp_txpkt) +
			  sizeof(struct ulptx_idata)) / sizeof(__be64);
	flits += sizeof(struct cpl_tx_pkt_core) / sizeof(__be64);
	*nflits = flits;

	/* If coalescing is on, the mbuf is added to a pkts WR */
	if (q->coalesce.idx) {
		ndesc = DIV_ROUND_UP(q->coalesce.flits + flits, 8);
		credits = txq_avail(q) - ndesc;

		/* If we are wrapping or this is last mbuf then, send the
		 * already coalesced mbufs and let the non-coalesce pass
		 * handle the mbuf.
		 */
		if (unlikely(credits < 0 || wraps_around(q, ndesc))) {
			ship_tx_pkt_coalesce_wr(adap, txq);
			return 0;
		}

		/* If the max coalesce len or the max WR len is reached
		 * ship the WR and keep coalescing on.
		 */
		if (unlikely((q->coalesce.len + mbuf->pkt_len >
						MAX_COALESCE_LEN) ||
			     (q->coalesce.flits + flits >
			      q->coalesce.max))) {
			ship_tx_pkt_coalesce_wr(adap, txq);
			goto new;
		}
		return 1;
	}

new:
	/* start a new pkts WR, the WR header is not filled below */
	wr_size = is_pf4(adap) ? sizeof(struct fw_eth_tx_pkts_wr) :
				 sizeof(struct fw_eth_tx_pkts_vm_wr);
	flits += wr_size / sizeof(__be64);
	ndesc = flits_to_desc(q->coalesce.flits + flits);
	credits = txq_avail(q) - ndesc;

	if (unlikely(credits < 0 || wraps_around(q, ndesc)))
		return 0;
	q->coalesce.flits += wr_size / sizeof(__be64);
	q->coalesce.type = type;
	q->coalesce.ptr = (unsigned char *)&q->desc[q->pidx] +
			   q->coalesce.flits * sizeof(__be64);
	if (!is_pf4(adap))
		memcpy((void *)q->coalesce.ethmacdst,
		       rte_pktmbuf_mtod(mbuf, void *), fw_hdr_copy_len);
	return 1;
}

/**
 * tx_do_packet_coalesce - add an mbuf to a coalesce WR
 * @txq: sge_eth_txq used send the mbuf
 * @mbuf: mbuf to be sent
 * @flits: flits needed for this mbuf
 * @adap: adapter structure
 * @pi: port_info structure
 * @addr: mapped address of the mbuf
 *
 * Adds an mbuf to be sent as part of a coalesce WR by filling a
 * ulp_tx_pkt command, ulp_tx_sc_imm command, cpl message and
 * ulp_tx_sc_dsgl command.
 */
static inline int tx_do_packet_coalesce(struct sge_eth_txq *txq,
					struct rte_mbuf *mbuf,
					int flits, struct adapter *adap,
					const struct port_info *pi,
					dma_addr_t *addr, uint16_t nb_pkts)
{
	u64 cntrl, *end;
	struct sge_txq *q = &txq->q;
	struct ulp_txpkt *mc;
	struct ulptx_idata *sc_imm;
	struct cpl_tx_pkt_core *cpl;
	struct tx_sw_desc *sd;
	unsigned int idx = q->coalesce.idx, len = mbuf->pkt_len;
	unsigned int max_coal_pkt_num = is_pf4(adap) ? ETH_COALESCE_PKT_NUM :
						       ETH_COALESCE_VF_PKT_NUM;

#ifdef RTE_LIBRTE_CXGBE_TPUT
	RTE_SET_USED(nb_pkts);
#endif

	if (q->coalesce.type == 0) {
		mc = (struct ulp_txpkt *)q->coalesce.ptr;
		mc->cmd_dest = htonl(V_ULPTX_CMD(4) | V_ULP_TXPKT_DEST(0) |
				     V_ULP_TXPKT_FID(adap->sge.fw_evtq.cntxt_id) |
				     F_ULP_TXPKT_RO);
		mc->len = htonl(DIV_ROUND_UP(flits, 2));
		sc_imm = (struct ulptx_idata *)(mc + 1);
		sc_imm->cmd_more = htonl(V_ULPTX_CMD(ULP_TX_SC_IMM) |
					 F_ULP_TX_SC_MORE);
		sc_imm->len = htonl(sizeof(*cpl));
		end = (u64 *)mc + flits;
		cpl = (struct cpl_tx_pkt_core *)(sc_imm + 1);
	} else {
		end = (u64 *)q->coalesce.ptr + flits;
		cpl = (struct cpl_tx_pkt_core *)q->coalesce.ptr;
	}

	/* update coalesce structure for this txq */
	q->coalesce.flits += flits;
	q->coalesce.ptr += flits * sizeof(__be64);
	q->coalesce.len += mbuf->pkt_len;

	/* fill the cpl message, same as in t4_eth_xmit, this should be kept
	 * similar to t4_eth_xmit
	 */
	if (mbuf->ol_flags & PKT_TX_IP_CKSUM) {
		cntrl = hwcsum(adap->params.chip, mbuf) |
			       F_TXPKT_IPCSUM_DIS;
		txq->stats.tx_cso++;
	} else {
		cntrl = F_TXPKT_L4CSUM_DIS | F_TXPKT_IPCSUM_DIS;
	}

	if (mbuf->ol_flags & PKT_TX_VLAN_PKT) {
		txq->stats.vlan_ins++;
		cntrl |= F_TXPKT_VLAN_VLD | V_TXPKT_VLAN(mbuf->vlan_tci);
	}

	cpl->ctrl0 = htonl(V_TXPKT_OPCODE(CPL_TX_PKT_XT));
	if (is_pf4(adap))
		cpl->ctrl0 |= htonl(V_TXPKT_INTF(pi->tx_chan) |
				    V_TXPKT_PF(adap->pf));
	else
		cpl->ctrl0 |= htonl(V_TXPKT_INTF(pi->port_id));
	cpl->pack = htons(0);
	cpl->len = htons(len);
	cpl->ctrl1 = cpu_to_be64(cntrl);
	write_sgl(mbuf, q, (struct ulptx_sgl *)(cpl + 1), end, 0,  addr);
	txq->stats.pkts++;
	txq->stats.tx_bytes += len;

	sd = &q->sdesc[q->pidx + (idx >> 1)];
	if (!(idx & 1)) {
		if (sd->coalesce.idx) {
			int i;

			for (i = 0; i < sd->coalesce.idx; i++) {
				rte_pktmbuf_free(sd->coalesce.mbuf[i]);
				sd->coalesce.mbuf[i] = NULL;
			}
		}
	}

	/* store pointers to the mbuf and the sgl used in free_tx_desc.
	 * each tx desc can hold two pointers corresponding to the value
	 * of ETH_COALESCE_PKT_PER_DESC
	 */
	sd->coalesce.mbuf[idx & 1] = mbuf;
	sd->coalesce.sgl[idx & 1] = (struct ulptx_sgl *)(cpl + 1);
	sd->coalesce.idx = (idx & 1) + 1;

	/* send the coaelsced work request if max reached */
	if (++q->coalesce.idx == max_coal_pkt_num
#ifndef RTE_LIBRTE_CXGBE_TPUT
	    || q->coalesce.idx >= nb_pkts
#endif
	    )
		ship_tx_pkt_coalesce_wr(adap, txq);
	return 0;
}

/**
 * t4_eth_xmit - add a packet to an Ethernet Tx queue
 * @txq: the egress queue
 * @mbuf: the packet
 *
 * Add a packet to an SGE Ethernet Tx queue.  Runs with softirqs disabled.
 */
int t4_eth_xmit(struct sge_eth_txq *txq, struct rte_mbuf *mbuf,
		uint16_t nb_pkts)
{
	const struct port_info *pi;
	struct cpl_tx_pkt_lso_core *lso;
	struct adapter *adap;
	struct rte_mbuf *m = mbuf;
	struct fw_eth_tx_pkt_wr *wr;
	struct fw_eth_tx_pkt_vm_wr *vmwr;
	struct cpl_tx_pkt_core *cpl;
	struct tx_sw_desc *d;
	dma_addr_t addr[m->nb_segs];
	unsigned int flits, ndesc, cflits;
	int l3hdr_len, l4hdr_len, eth_xtra_len;
	int len, last_desc;
	int credits;
	u32 wr_mid;
	u64 cntrl, *end;
	bool v6;
	u32 max_pkt_len = txq->data->dev_conf.rxmode.max_rx_pkt_len;

	/* Reject xmit if queue is stopped */
	if (unlikely(txq->flags & EQ_STOPPED))
		return -(EBUSY);

	/*
	 * The chip min packet length is 10 octets but play safe and reject
	 * anything shorter than an Ethernet header.
	 */
	if (unlikely(m->pkt_len < ETHER_HDR_LEN)) {
out_free:
		rte_pktmbuf_free(m);
		return 0;
	}

	if ((!(m->ol_flags & PKT_TX_TCP_SEG)) &&
	    (unlikely(m->pkt_len > max_pkt_len)))
		goto out_free;

	pi = (struct port_info *)txq->data->dev_private;
	adap = pi->adapter;

	cntrl = F_TXPKT_L4CSUM_DIS | F_TXPKT_IPCSUM_DIS;
	/* align the end of coalesce WR to a 512 byte boundary */
	txq->q.coalesce.max = (8 - (txq->q.pidx & 7)) * 8;

	if (!((m->ol_flags & PKT_TX_TCP_SEG) || (m->pkt_len > ETHER_MAX_LEN))) {
		if (should_tx_packet_coalesce(txq, mbuf, &cflits, adap)) {
			if (unlikely(map_mbuf(mbuf, addr) < 0)) {
				dev_warn(adap, "%s: mapping err for coalesce\n",
					 __func__);
				txq->stats.mapping_err++;
				goto out_free;
			}
			rte_prefetch0((volatile void *)addr);
			return tx_do_packet_coalesce(txq, mbuf, cflits, adap,
						     pi, addr, nb_pkts);
		} else {
			return -EBUSY;
		}
	}

	if (txq->q.coalesce.idx)
		ship_tx_pkt_coalesce_wr(adap, txq);

	flits = calc_tx_flits(m, adap);
	ndesc = flits_to_desc(flits);
	credits = txq_avail(&txq->q) - ndesc;

	if (unlikely(credits < 0)) {
		dev_debug(adap, "%s: Tx ring %u full; credits = %d\n",
			  __func__, txq->q.cntxt_id, credits);
		return -EBUSY;
	}

	if (unlikely(map_mbuf(m, addr) < 0)) {
		txq->stats.mapping_err++;
		goto out_free;
	}

	wr_mid = V_FW_WR_LEN16(DIV_ROUND_UP(flits, 2));
	if (Q_IDXDIFF(&txq->q, equeidx)  >= 64) {
		txq->q.equeidx = txq->q.pidx;
		wr_mid |= F_FW_WR_EQUEQ;
	}

	wr = (void *)&txq->q.desc[txq->q.pidx];
	vmwr = (void *)&txq->q.desc[txq->q.pidx];
	wr->equiq_to_len16 = htonl(wr_mid);
	if (is_pf4(adap)) {
		wr->r3 = rte_cpu_to_be_64(0);
		end = (u64 *)wr + flits;
	} else {
		const size_t fw_hdr_copy_len = (sizeof(vmwr->ethmacdst) +
						sizeof(vmwr->ethmacsrc) +
						sizeof(vmwr->ethtype) +
						sizeof(vmwr->vlantci));

		vmwr->r3[0] = rte_cpu_to_be_32(0);
		vmwr->r3[1] = rte_cpu_to_be_32(0);
		memcpy((void *)vmwr->ethmacdst, rte_pktmbuf_mtod(m, void *),
		       fw_hdr_copy_len);
		end = (u64 *)vmwr + flits;
	}

	len = 0;
	len += sizeof(*cpl);

	/* Coalescing skipped and we send through normal path */
	if (!(m->ol_flags & PKT_TX_TCP_SEG)) {
		wr->op_immdlen = htonl(V_FW_WR_OP(is_pf4(adap) ?
						  FW_ETH_TX_PKT_WR :
						  FW_ETH_TX_PKT_VM_WR) |
				       V_FW_WR_IMMDLEN(len));
		if (is_pf4(adap))
			cpl = (void *)(wr + 1);
		else
			cpl = (void *)(vmwr + 1);
		if (m->ol_flags & PKT_TX_IP_CKSUM) {
			cntrl = hwcsum(adap->params.chip, m) |
				F_TXPKT_IPCSUM_DIS;
			txq->stats.tx_cso++;
		}
	} else {
		if (is_pf4(adap))
			lso = (void *)(wr + 1);
		else
			lso = (void *)(vmwr + 1);
		v6 = (m->ol_flags & PKT_TX_IPV6) != 0;
		l3hdr_len = m->l3_len;
		l4hdr_len = m->l4_len;
		eth_xtra_len = m->l2_len - ETHER_HDR_LEN;
		len += sizeof(*lso);
		wr->op_immdlen = htonl(V_FW_WR_OP(is_pf4(adap) ?
						  FW_ETH_TX_PKT_WR :
						  FW_ETH_TX_PKT_VM_WR) |
				       V_FW_WR_IMMDLEN(len));
		lso->lso_ctrl = htonl(V_LSO_OPCODE(CPL_TX_PKT_LSO) |
				      F_LSO_FIRST_SLICE | F_LSO_LAST_SLICE |
				      V_LSO_IPV6(v6) |
				      V_LSO_ETHHDR_LEN(eth_xtra_len / 4) |
				      V_LSO_IPHDR_LEN(l3hdr_len / 4) |
				      V_LSO_TCPHDR_LEN(l4hdr_len / 4));
		lso->ipid_ofst = htons(0);
		lso->mss = htons(m->tso_segsz);
		lso->seqno_offset = htonl(0);
		if (is_t4(adap->params.chip))
			lso->len = htonl(m->pkt_len);
		else
			lso->len = htonl(V_LSO_T5_XFER_SIZE(m->pkt_len));
		cpl = (void *)(lso + 1);

		if (CHELSIO_CHIP_VERSION(adap->params.chip) <= CHELSIO_T5)
			cntrl = V_TXPKT_ETHHDR_LEN(eth_xtra_len);
		else
			cntrl = V_T6_TXPKT_ETHHDR_LEN(eth_xtra_len);

		cntrl |= V_TXPKT_CSUM_TYPE(v6 ? TX_CSUM_TCPIP6 :
						TX_CSUM_TCPIP) |
			 V_TXPKT_IPHDR_LEN(l3hdr_len);
		txq->stats.tso++;
		txq->stats.tx_cso += m->tso_segsz;
	}

	if (m->ol_flags & PKT_TX_VLAN_PKT) {
		txq->stats.vlan_ins++;
		cntrl |= F_TXPKT_VLAN_VLD | V_TXPKT_VLAN(m->vlan_tci);
	}

	cpl->ctrl0 = htonl(V_TXPKT_OPCODE(CPL_TX_PKT_XT));
	if (is_pf4(adap))
		cpl->ctrl0 |= htonl(V_TXPKT_INTF(pi->tx_chan) |
				    V_TXPKT_PF(adap->pf));
	else
		cpl->ctrl0 |= htonl(V_TXPKT_INTF(pi->port_id) |
				    V_TXPKT_PF(0));

	cpl->pack = htons(0);
	cpl->len = htons(m->pkt_len);
	cpl->ctrl1 = cpu_to_be64(cntrl);

	txq->stats.pkts++;
	txq->stats.tx_bytes += m->pkt_len;
	last_desc = txq->q.pidx + ndesc - 1;
	if (last_desc >= (int)txq->q.size)
		last_desc -= txq->q.size;

	d = &txq->q.sdesc[last_desc];
	if (d->coalesce.idx) {
		int i;

		for (i = 0; i < d->coalesce.idx; i++) {
			rte_pktmbuf_free(d->coalesce.mbuf[i]);
			d->coalesce.mbuf[i] = NULL;
		}
		d->coalesce.idx = 0;
	}
	write_sgl(m, &txq->q, (struct ulptx_sgl *)(cpl + 1), end, 0,
		  addr);
	txq->q.sdesc[last_desc].mbuf = m;
	txq->q.sdesc[last_desc].sgl = (struct ulptx_sgl *)(cpl + 1);
	txq_advance(&txq->q, ndesc);
	ring_tx_db(adap, &txq->q);
	return 0;
}

/**
 * reclaim_completed_tx_imm - reclaim completed control-queue Tx descs
 * @q: the SGE control Tx queue
 *
 * This is a variant of reclaim_completed_tx() that is used for Tx queues
 * that send only immediate data (presently just the control queues) and
 * thus do not have any mbufs to release.
 */
static inline void reclaim_completed_tx_imm(struct sge_txq *q)
{
	int hw_cidx = ntohs(q->stat->cidx);
	int reclaim = hw_cidx - q->cidx;

	if (reclaim < 0)
		reclaim += q->size;

	q->in_use -= reclaim;
	q->cidx = hw_cidx;
}

/**
 * is_imm - check whether a packet can be sent as immediate data
 * @mbuf: the packet
 *
 * Returns true if a packet can be sent as a WR with immediate data.
 */
static inline int is_imm(const struct rte_mbuf *mbuf)
{
	return mbuf->pkt_len <= MAX_CTRL_WR_LEN;
}

/**
 * inline_tx_mbuf: inline a packet's data into TX descriptors
 * @q: the TX queue where the packet will be inlined
 * @from: pointer to data portion of packet
 * @to: pointer after cpl where data has to be inlined
 * @len: length of data to inline
 *
 * Inline a packet's contents directly to TX descriptors, starting at
 * the given position within the TX DMA ring.
 * Most of the complexity of this operation is dealing with wrap arounds
 * in the middle of the packet we want to inline.
 */
static void inline_tx_mbuf(const struct sge_txq *q, caddr_t from, caddr_t *to,
			   int len)
{
	int left = RTE_PTR_DIFF(q->stat, *to);

	if (likely((uintptr_t)*to + len <= (uintptr_t)q->stat)) {
		rte_memcpy(*to, from, len);
		*to = RTE_PTR_ADD(*to, len);
	} else {
		rte_memcpy(*to, from, left);
		from = RTE_PTR_ADD(from, left);
		left = len - left;
		rte_memcpy((void *)q->desc, from, left);
		*to = RTE_PTR_ADD((void *)q->desc, left);
	}
}

/**
 * ctrl_xmit - send a packet through an SGE control Tx queue
 * @q: the control queue
 * @mbuf: the packet
 *
 * Send a packet through an SGE control Tx queue.  Packets sent through
 * a control queue must fit entirely as immediate data.
 */
static int ctrl_xmit(struct sge_ctrl_txq *q, struct rte_mbuf *mbuf)
{
	unsigned int ndesc;
	struct fw_wr_hdr *wr;
	caddr_t dst;

	if (unlikely(!is_imm(mbuf))) {
		WARN_ON(1);
		rte_pktmbuf_free(mbuf);
		return -1;
	}

	reclaim_completed_tx_imm(&q->q);
	ndesc = DIV_ROUND_UP(mbuf->pkt_len, sizeof(struct tx_desc));
	t4_os_lock(&q->ctrlq_lock);

	q->full = txq_avail(&q->q) < ndesc ? 1 : 0;
	if (unlikely(q->full)) {
		t4_os_unlock(&q->ctrlq_lock);
		return -1;
	}

	wr = (struct fw_wr_hdr *)&q->q.desc[q->q.pidx];
	dst = (void *)wr;
	inline_tx_mbuf(&q->q, rte_pktmbuf_mtod(mbuf, caddr_t),
		       &dst, mbuf->data_len);

	txq_advance(&q->q, ndesc);
	if (unlikely(txq_avail(&q->q) < 64))
		wr->lo |= htonl(F_FW_WR_EQUEQ);

	q->txp++;

	ring_tx_db(q->adapter, &q->q);
	t4_os_unlock(&q->ctrlq_lock);

	rte_pktmbuf_free(mbuf);
	return 0;
}

/**
 * t4_mgmt_tx - send a management message
 * @q: the control queue
 * @mbuf: the packet containing the management message
 *
 * Send a management message through control queue.
 */
int t4_mgmt_tx(struct sge_ctrl_txq *q, struct rte_mbuf *mbuf)
{
	return ctrl_xmit(q, mbuf);
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
	tz = rte_memzone_reserve_aligned(z_name, len, socket_id,
			RTE_MEMZONE_IOVA_CONTIG, 4096);
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

	*phys = (uint64_t)tz->iova;
	return tz->addr;
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

static inline void cxgbe_set_mbuf_info(struct rte_mbuf *pkt, uint32_t ptype,
				       uint64_t ol_flags)
{
	pkt->packet_type |= ptype;
	pkt->ol_flags |= ol_flags;
}

static inline void cxgbe_fill_mbuf_info(struct adapter *adap,
					const struct cpl_rx_pkt *cpl,
					struct rte_mbuf *pkt)
{
	bool csum_ok;
	u16 err_vec;

	if (adap->params.tp.rx_pkt_encap)
		err_vec = G_T6_COMPR_RXERR_VEC(ntohs(cpl->err_vec));
	else
		err_vec = ntohs(cpl->err_vec);

	csum_ok = cpl->csum_calc && !err_vec;

	if (cpl->vlan_ex)
		cxgbe_set_mbuf_info(pkt, RTE_PTYPE_L2_ETHER_VLAN,
				    PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED);
	else
		cxgbe_set_mbuf_info(pkt, RTE_PTYPE_L2_ETHER, 0);

	if (cpl->l2info & htonl(F_RXF_IP))
		cxgbe_set_mbuf_info(pkt, RTE_PTYPE_L3_IPV4,
				    csum_ok ? PKT_RX_IP_CKSUM_GOOD :
					      PKT_RX_IP_CKSUM_BAD);
	else if (cpl->l2info & htonl(F_RXF_IP6))
		cxgbe_set_mbuf_info(pkt, RTE_PTYPE_L3_IPV6,
				    csum_ok ? PKT_RX_IP_CKSUM_GOOD :
					      PKT_RX_IP_CKSUM_BAD);

	if (cpl->l2info & htonl(F_RXF_TCP))
		cxgbe_set_mbuf_info(pkt, RTE_PTYPE_L4_TCP,
				    csum_ok ? PKT_RX_L4_CKSUM_GOOD :
					      PKT_RX_L4_CKSUM_BAD);
	else if (cpl->l2info & htonl(F_RXF_UDP))
		cxgbe_set_mbuf_info(pkt, RTE_PTYPE_L4_UDP,
				    csum_ok ? PKT_RX_L4_CKSUM_GOOD :
					      PKT_RX_L4_CKSUM_BAD);
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

	while (likely(budget_left)) {
		if (q->cidx == ntohs(q->stat->pidx))
			break;

		rc = (const struct rsp_ctrl *)
		     ((const char *)q->cur_desc + (q->iqe_len - sizeof(*rc)));

		/*
		 * Ensure response has been read
		 */
		rmb();
		rsp_type = G_RSPD_TYPE(rc->u.type_gen);

		if (likely(rsp_type == X_RSPD_TYPE_FLBUF)) {
			struct sge *s = &q->adapter->sge;
			unsigned int stat_pidx;
			int stat_pidx_diff;

			stat_pidx = ntohs(q->stat->pidx);
			stat_pidx_diff = P_IDXDIFF(q, stat_pidx);
			while (stat_pidx_diff && budget_left) {
				const struct rx_sw_desc *rsd =
					&rxq->fl.sdesc[rxq->fl.cidx];
				const struct rss_header *rss_hdr =
					(const void *)q->cur_desc;
				const struct cpl_rx_pkt *cpl =
					(const void *)&q->cur_desc[1];
				struct rte_mbuf *pkt, *npkt;
				u32 len, bufsz;

				rc = (const struct rsp_ctrl *)
				     ((const char *)q->cur_desc +
				      (q->iqe_len - sizeof(*rc)));

				rsp_type = G_RSPD_TYPE(rc->u.type_gen);
				if (unlikely(rsp_type != X_RSPD_TYPE_FLBUF))
					break;

				len = ntohl(rc->pldbuflen_qid);
				BUG_ON(!(len & F_RSPD_NEWBUF));
				pkt = rsd->buf;
				npkt = pkt;
				len = G_RSPD_LEN(len);
				pkt->pkt_len = len;

				/* Chain mbufs into len if necessary */
				while (len) {
					struct rte_mbuf *new_pkt = rsd->buf;

					bufsz = min(get_buf_size(q->adapter,
								 rsd), len);
					new_pkt->data_len = bufsz;
					unmap_rx_buf(&rxq->fl);
					len -= bufsz;
					npkt->next = new_pkt;
					npkt = new_pkt;
					pkt->nb_segs++;
					rsd = &rxq->fl.sdesc[rxq->fl.cidx];
				}
				npkt->next = NULL;
				pkt->nb_segs--;

				cxgbe_fill_mbuf_info(q->adapter, cpl, pkt);

				if (!rss_hdr->filter_tid &&
				    rss_hdr->hash_type) {
					pkt->ol_flags |= PKT_RX_RSS_HASH;
					pkt->hash.rss =
						ntohl(rss_hdr->hash_val);
				}

				if (cpl->vlan_ex)
					pkt->vlan_tci = ntohs(cpl->vlan);

				rte_pktmbuf_adj(pkt, s->pktshift);
				rxq->stats.pkts++;
				rxq->stats.rx_bytes += pkt->pkt_len;
				rx_pkts[budget - budget_left] = pkt;

				rspq_next(q);
				budget_left--;
				stat_pidx_diff--;
			}
			continue;
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

	if (q->offset >= 0 && fl_cap(&rxq->fl) - rxq->fl.avail >= 64)
		__refill_fl(q->adapter, &rxq->fl);

	return budget - budget_left;
}

int cxgbe_poll(struct sge_rspq *q, struct rte_mbuf **rx_pkts,
	       unsigned int budget, unsigned int *work_done)
{
	struct sge_eth_rxq *rxq = container_of(q, struct sge_eth_rxq, rspq);
	unsigned int cidx_inc;
	unsigned int params;
	u32 val;

	*work_done = process_responses(q, budget, rx_pkts);

	if (*work_done) {
		cidx_inc = R_IDXDIFF(q, gts_idx);

		if (q->offset >= 0 && fl_cap(&rxq->fl) - rxq->fl.avail >= 64)
			__refill_fl(q->adapter, &rxq->fl);

		params = q->intr_params;
		q->next_intr_params = params;
		val = V_CIDXINC(cidx_inc) | V_SEINTARM(params);

		if (unlikely(!q->bar2_addr)) {
			u32 reg = is_pf4(q->adapter) ? MYPF_REG(A_SGE_PF_GTS) :
						       T4VF_SGE_BASE_ADDR +
						       A_SGE_VF_GTS;

			t4_write_reg(q->adapter, reg,
				     val | V_INGRESSQID((u32)q->cntxt_id));
		} else {
			writel(val | V_INGRESSQID(q->bar2_qid),
			       (void *)((uintptr_t)q->bar2_addr + SGE_UDB_GTS));
			/* This Write memory Barrier will force the
			 * write to the User Doorbell area to be
			 * flushed.
			 */
			wmb();
		}
		q->gts_idx = q->cidx;
	}
	return 0;
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
	u8 pciechan;

	/* Size needs to be multiple of 16, including status entry. */
	iq->size = cxgbe_roundup(iq->size, 16);

	snprintf(z_name, sizeof(z_name), "eth_p%d_q%d_%s",
			eth_dev->data->port_id, queue_id,
			fwevtq ? "fwq_ring" : "rx_ring");
	snprintf(z_name_sw, sizeof(z_name_sw), "%s_sw_ring", z_name);

	iq->desc = alloc_ring(iq->size, iq->iqe_len, 0, &iq->phys_addr, NULL, 0,
			      queue_id, socket_id, z_name, z_name_sw);
	if (!iq->desc)
		return -ENOMEM;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = htonl(V_FW_CMD_OP(FW_IQ_CMD) | F_FW_CMD_REQUEST |
			    F_FW_CMD_WRITE | F_FW_CMD_EXEC);

	if (is_pf4(adap)) {
		pciechan = pi->tx_chan;
		c.op_to_vfn |= htonl(V_FW_IQ_CMD_PFN(adap->pf) |
				     V_FW_IQ_CMD_VFN(0));
		if (cong >= 0)
			c.iqns_to_fl0congen =
				htonl(F_FW_IQ_CMD_IQFLINTCONGEN |
				      V_FW_IQ_CMD_IQTYPE(cong ?
							 FW_IQ_IQTYPE_NIC :
							 FW_IQ_IQTYPE_OFLD) |
				      F_FW_IQ_CMD_IQRO);
	} else {
		pciechan = pi->port_id;
	}

	c.alloc_to_len16 = htonl(F_FW_IQ_CMD_ALLOC | F_FW_IQ_CMD_IQSTART |
				 (sizeof(c) / 16));
	c.type_to_iqandstindex =
		htonl(V_FW_IQ_CMD_TYPE(FW_IQ_TYPE_FL_INT_CAP) |
		      V_FW_IQ_CMD_IQASYNCH(fwevtq) |
		      V_FW_IQ_CMD_VIID(pi->viid) |
		      V_FW_IQ_CMD_IQANDST(intr_idx < 0) |
		      V_FW_IQ_CMD_IQANUD(X_UPDATEDELIVERY_STATUS_PAGE) |
		      V_FW_IQ_CMD_IQANDSTINDEX(intr_idx >= 0 ? intr_idx :
							       -intr_idx - 1));
	c.iqdroprss_to_iqesize =
		htons(V_FW_IQ_CMD_IQPCIECH(pciechan) |
		      F_FW_IQ_CMD_IQGTSMODE |
		      V_FW_IQ_CMD_IQINTCNTTHRESH(iq->pktcnt_idx) |
		      V_FW_IQ_CMD_IQESIZE(ilog2(iq->iqe_len) - 4));
	c.iqsize = htons(iq->size);
	c.iqaddr = cpu_to_be64(iq->phys_addr);

	if (fl) {
		struct sge_eth_rxq *rxq = container_of(fl, struct sge_eth_rxq,
						       fl);
		unsigned int chip_ver = CHELSIO_CHIP_VERSION(adap->params.chip);

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
		fl->size = cxgbe_roundup(fl->size, 8);

		snprintf(z_name, sizeof(z_name), "eth_p%d_q%d_%s",
				eth_dev->data->port_id, queue_id,
				fwevtq ? "fwq_ring" : "fl_ring");
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
		if (is_pf4(adap) && cong >= 0)
			c.iqns_to_fl0congen |=
				htonl(V_FW_IQ_CMD_FL0CNGCHMAP(cong) |
				      F_FW_IQ_CMD_FL0CONGCIF |
				      F_FW_IQ_CMD_FL0CONGEN);

		/* In T6, for egress queue type FL there is internal overhead
		 * of 16B for header going into FLM module.
		 * Hence maximum allowed burst size will be 448 bytes.
		 */
		c.fl0dcaen_to_fl0cidxfthresh =
			htons(V_FW_IQ_CMD_FL0FBMIN(chip_ver <= CHELSIO_T5 ?
						   X_FETCHBURSTMIN_128B :
						   X_FETCHBURSTMIN_64B) |
			      V_FW_IQ_CMD_FL0FBMAX(chip_ver <= CHELSIO_T5 ?
						   X_FETCHBURSTMAX_512B :
						   X_FETCHBURSTMAX_256B));
		c.fl0size = htons(flsz);
		c.fl0addr = cpu_to_be64(fl->addr);
	}

	if (is_pf4(adap))
		ret = t4_wr_mbox(adap, adap->mbox, &c, sizeof(c), &c);
	else
		ret = t4vf_wr_mbox(adap, &c, sizeof(c), &c);
	if (ret)
		goto err;

	iq->cur_desc = iq->desc;
	iq->cidx = 0;
	iq->gts_idx = 0;
	iq->gen = 1;
	iq->next_intr_params = iq->intr_params;
	iq->cntxt_id = ntohs(c.iqid);
	iq->abs_id = ntohs(c.physiqid);
	iq->bar2_addr = bar2_address(adap, iq->cntxt_id, T4_BAR2_QTYPE_INGRESS,
				     &iq->bar2_qid);
	iq->size--;                           /* subtract status entry */
	iq->stat = (void *)&iq->desc[iq->size * 8];
	iq->eth_dev = eth_dev;
	iq->handler = hnd;
	iq->port_id = pi->pidx;
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
	if (is_pf4(adap) && !is_t4(adap->params.chip) && cong >= 0) {
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
		   iq->cntxt_id, fl->cntxt_id, 0xffff);
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

static void init_txq(struct adapter *adap, struct sge_txq *q, unsigned int id,
		     unsigned int abs_id)
{
	q->cntxt_id = id;
	q->abs_id = abs_id;
	q->bar2_addr = bar2_address(adap, q->cntxt_id, T4_BAR2_QTYPE_EGRESS,
				    &q->bar2_qid);
	q->cidx = 0;
	q->pidx = 0;
	q->dbidx = 0;
	q->in_use = 0;
	q->equeidx = 0;
	q->coalesce.idx = 0;
	q->coalesce.len = 0;
	q->coalesce.flits = 0;
	q->last_coal_idx = 0;
	q->last_pidx = 0;
	q->stat = (void *)&q->desc[q->size];
}

int t4_sge_eth_txq_start(struct sge_eth_txq *txq)
{
	/*
	 *  TODO: For flow-control, queue may be stopped waiting to reclaim
	 *  credits.
	 *  Ensure queue is in EQ_STOPPED state before starting it.
	 */
	if (!(txq->flags & EQ_STOPPED))
		return -(EBUSY);

	txq->flags &= ~EQ_STOPPED;

	return 0;
}

int t4_sge_eth_txq_stop(struct sge_eth_txq *txq)
{
	txq->flags |= EQ_STOPPED;

	return 0;
}

int t4_sge_alloc_eth_txq(struct adapter *adap, struct sge_eth_txq *txq,
			 struct rte_eth_dev *eth_dev, uint16_t queue_id,
			 unsigned int iqid, int socket_id)
{
	int ret, nentries;
	struct fw_eq_eth_cmd c;
	struct sge *s = &adap->sge;
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	char z_name[RTE_MEMZONE_NAMESIZE];
	char z_name_sw[RTE_MEMZONE_NAMESIZE];
	u8 pciechan;

	/* Add status entries */
	nentries = txq->q.size + s->stat_len / sizeof(struct tx_desc);

	snprintf(z_name, sizeof(z_name), "eth_p%d_q%d_%s",
			eth_dev->data->port_id, queue_id, "tx_ring");
	snprintf(z_name_sw, sizeof(z_name_sw), "%s_sw_ring", z_name);

	txq->q.desc = alloc_ring(txq->q.size, sizeof(struct tx_desc),
				 sizeof(struct tx_sw_desc), &txq->q.phys_addr,
				 &txq->q.sdesc, s->stat_len, queue_id,
				 socket_id, z_name, z_name_sw);
	if (!txq->q.desc)
		return -ENOMEM;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = htonl(V_FW_CMD_OP(FW_EQ_ETH_CMD) | F_FW_CMD_REQUEST |
			    F_FW_CMD_WRITE | F_FW_CMD_EXEC);
	if (is_pf4(adap)) {
		pciechan = pi->tx_chan;
		c.op_to_vfn |= htonl(V_FW_EQ_ETH_CMD_PFN(adap->pf) |
				     V_FW_EQ_ETH_CMD_VFN(0));
	} else {
		pciechan = pi->port_id;
	}

	c.alloc_to_len16 = htonl(F_FW_EQ_ETH_CMD_ALLOC |
				 F_FW_EQ_ETH_CMD_EQSTART | (sizeof(c) / 16));
	c.autoequiqe_to_viid = htonl(F_FW_EQ_ETH_CMD_AUTOEQUEQE |
				     V_FW_EQ_ETH_CMD_VIID(pi->viid));
	c.fetchszm_to_iqid =
		htonl(V_FW_EQ_ETH_CMD_HOSTFCMODE(X_HOSTFCMODE_NONE) |
		      V_FW_EQ_ETH_CMD_PCIECHN(pciechan) |
		      F_FW_EQ_ETH_CMD_FETCHRO | V_FW_EQ_ETH_CMD_IQID(iqid));
	c.dcaen_to_eqsize =
		htonl(V_FW_EQ_ETH_CMD_FBMIN(X_FETCHBURSTMIN_64B) |
		      V_FW_EQ_ETH_CMD_FBMAX(X_FETCHBURSTMAX_512B) |
		      V_FW_EQ_ETH_CMD_EQSIZE(nentries));
	c.eqaddr = rte_cpu_to_be_64(txq->q.phys_addr);

	if (is_pf4(adap))
		ret = t4_wr_mbox(adap, adap->mbox, &c, sizeof(c), &c);
	else
		ret = t4vf_wr_mbox(adap, &c, sizeof(c), &c);
	if (ret) {
		rte_free(txq->q.sdesc);
		txq->q.sdesc = NULL;
		txq->q.desc = NULL;
		return ret;
	}

	init_txq(adap, &txq->q, G_FW_EQ_ETH_CMD_EQID(ntohl(c.eqid_pkd)),
		 G_FW_EQ_ETH_CMD_PHYSEQID(ntohl(c.physeqid_pkd)));
	txq->stats.tso = 0;
	txq->stats.pkts = 0;
	txq->stats.tx_cso = 0;
	txq->stats.coal_wr = 0;
	txq->stats.vlan_ins = 0;
	txq->stats.tx_bytes = 0;
	txq->stats.coal_pkts = 0;
	txq->stats.mapping_err = 0;
	txq->flags |= EQ_STOPPED;
	txq->eth_dev = eth_dev;
	txq->data = eth_dev->data;
	t4_os_lock_init(&txq->txq_lock);
	return 0;
}

int t4_sge_alloc_ctrl_txq(struct adapter *adap, struct sge_ctrl_txq *txq,
			  struct rte_eth_dev *eth_dev, uint16_t queue_id,
			  unsigned int iqid, int socket_id)
{
	int ret, nentries;
	struct fw_eq_ctrl_cmd c;
	struct sge *s = &adap->sge;
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	char z_name[RTE_MEMZONE_NAMESIZE];
	char z_name_sw[RTE_MEMZONE_NAMESIZE];

	/* Add status entries */
	nentries = txq->q.size + s->stat_len / sizeof(struct tx_desc);

	snprintf(z_name, sizeof(z_name), "eth_p%d_q%d_%s",
			eth_dev->data->port_id, queue_id, "ctrl_tx_ring");
	snprintf(z_name_sw, sizeof(z_name_sw), "%s_sw_ring", z_name);

	txq->q.desc = alloc_ring(txq->q.size, sizeof(struct tx_desc),
				 0, &txq->q.phys_addr,
				 NULL, 0, queue_id,
				 socket_id, z_name, z_name_sw);
	if (!txq->q.desc)
		return -ENOMEM;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = htonl(V_FW_CMD_OP(FW_EQ_CTRL_CMD) | F_FW_CMD_REQUEST |
			    F_FW_CMD_WRITE | F_FW_CMD_EXEC |
			    V_FW_EQ_CTRL_CMD_PFN(adap->pf) |
			    V_FW_EQ_CTRL_CMD_VFN(0));
	c.alloc_to_len16 = htonl(F_FW_EQ_CTRL_CMD_ALLOC |
				 F_FW_EQ_CTRL_CMD_EQSTART | (sizeof(c) / 16));
	c.cmpliqid_eqid = htonl(V_FW_EQ_CTRL_CMD_CMPLIQID(0));
	c.physeqid_pkd = htonl(0);
	c.fetchszm_to_iqid =
		htonl(V_FW_EQ_CTRL_CMD_HOSTFCMODE(X_HOSTFCMODE_NONE) |
		      V_FW_EQ_CTRL_CMD_PCIECHN(pi->tx_chan) |
		      F_FW_EQ_CTRL_CMD_FETCHRO | V_FW_EQ_CTRL_CMD_IQID(iqid));
	c.dcaen_to_eqsize =
		htonl(V_FW_EQ_CTRL_CMD_FBMIN(X_FETCHBURSTMIN_64B) |
		      V_FW_EQ_CTRL_CMD_FBMAX(X_FETCHBURSTMAX_512B) |
		      V_FW_EQ_CTRL_CMD_EQSIZE(nentries));
	c.eqaddr = cpu_to_be64(txq->q.phys_addr);

	ret = t4_wr_mbox(adap, adap->mbox, &c, sizeof(c), &c);
	if (ret) {
		txq->q.desc = NULL;
		return ret;
	}

	init_txq(adap, &txq->q, G_FW_EQ_CTRL_CMD_EQID(ntohl(c.cmpliqid_eqid)),
		 G_FW_EQ_CTRL_CMD_EQID(ntohl(c. physeqid_pkd)));
	txq->adapter = adap;
	txq->full = 0;
	return 0;
}

static void free_txq(struct sge_txq *q)
{
	q->cntxt_id = 0;
	q->sdesc = NULL;
	q->desc = NULL;
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

/*
 * Clear all queues of the port
 *
 * Note:  This function must only be called after rx and tx path
 * of the port have been disabled.
 */
void t4_sge_eth_clear_queues(struct port_info *pi)
{
	int i;
	struct adapter *adap = pi->adapter;
	struct sge_eth_rxq *rxq = &adap->sge.ethrxq[pi->first_qset];
	struct sge_eth_txq *txq = &adap->sge.ethtxq[pi->first_qset];

	for (i = 0; i < pi->n_rx_qsets; i++, rxq++) {
		if (rxq->rspq.desc)
			t4_sge_eth_rxq_stop(adap, &rxq->rspq);
	}
	for (i = 0; i < pi->n_tx_qsets; i++, txq++) {
		if (txq->q.desc) {
			struct sge_txq *q = &txq->q;

			t4_sge_eth_txq_stop(txq);
			reclaim_completed_tx(q);
			free_tx_desc(q, q->size);
			q->equeidx = q->pidx;
		}
	}
}

void t4_sge_eth_rxq_release(struct adapter *adap, struct sge_eth_rxq *rxq)
{
	if (rxq->rspq.desc) {
		t4_sge_eth_rxq_stop(adap, &rxq->rspq);
		free_rspq_fl(adap, &rxq->rspq, rxq->fl.size ? &rxq->fl : NULL);
	}
}

void t4_sge_eth_txq_release(struct adapter *adap, struct sge_eth_txq *txq)
{
	if (txq->q.desc) {
		t4_sge_eth_txq_stop(txq);
		reclaim_completed_tx(&txq->q);
		t4_eth_eq_free(adap, adap->mbox, adap->pf, 0, txq->q.cntxt_id);
		free_tx_desc(&txq->q, txq->q.size);
		rte_free(txq->q.sdesc);
		free_txq(&txq->q);
	}
}

void t4_sge_tx_monitor_start(struct adapter *adap)
{
	rte_eal_alarm_set(50, tx_timer_cb, (void *)adap);
}

void t4_sge_tx_monitor_stop(struct adapter *adap)
{
	rte_eal_alarm_cancel(tx_timer_cb, (void *)adap);
}

/**
 * t4_free_sge_resources - free SGE resources
 * @adap: the adapter
 *
 * Frees resources used by the SGE queue sets.
 */
void t4_free_sge_resources(struct adapter *adap)
{
	unsigned int i;
	struct sge_eth_rxq *rxq = &adap->sge.ethrxq[0];
	struct sge_eth_txq *txq = &adap->sge.ethtxq[0];

	/* clean up Ethernet Tx/Rx queues */
	for (i = 0; i < adap->sge.max_ethqsets; i++, rxq++, txq++) {
		/* Free only the queues allocated */
		if (rxq->rspq.desc) {
			t4_sge_eth_rxq_release(adap, rxq);
			rxq->rspq.eth_dev = NULL;
		}
		if (txq->q.desc) {
			t4_sge_eth_txq_release(adap, txq);
			txq->eth_dev = NULL;
		}
	}

	/* clean up control Tx queues */
	for (i = 0; i < ARRAY_SIZE(adap->sge.ctrlq); i++) {
		struct sge_ctrl_txq *cq = &adap->sge.ctrlq[i];

		if (cq->q.desc) {
			reclaim_completed_tx_imm(&cq->q);
			t4_ctrl_eq_free(adap, adap->mbox, adap->pf, 0,
					cq->q.cntxt_id);
			free_txq(&cq->q);
		}
	}

	if (adap->sge.fw_evtq.desc)
		free_rspq_fl(adap, &adap->sge.fw_evtq, NULL);
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
	if (fl_small_pg != CXGBE_PAGE_SIZE ||
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
	u32 sge_control, sge_conm_ctrl;
	int ret, egress_threshold;

	/*
	 * Ingress Padding Boundary and Egress Status Page Size are set up by
	 * t4_fixup_host_params().
	 */
	sge_control = t4_read_reg(adap, A_SGE_CONTROL);
	s->pktshift = G_PKTSHIFT(sge_control);
	s->stat_len = (sge_control & F_EGRSTATUSPAGESIZE) ? 128 : 64;
	s->fl_align = t4_fl_pkt_align(adap);
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

int t4vf_sge_init(struct adapter *adap)
{
	struct sge_params *sge_params = &adap->params.sge;
	u32 sge_ingress_queues_per_page;
	u32 sge_egress_queues_per_page;
	u32 sge_control, sge_control2;
	u32 fl_small_pg, fl_large_pg;
	u32 sge_ingress_rx_threshold;
	u32 sge_timer_value_0_and_1;
	u32 sge_timer_value_2_and_3;
	u32 sge_timer_value_4_and_5;
	u32 sge_congestion_control;
	struct sge *s = &adap->sge;
	unsigned int s_hps, s_qpp;
	u32 sge_host_page_size;
	u32 params[7], vals[7];
	int v;

	/* query basic params from fw */
	params[0] = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_REG) |
		     V_FW_PARAMS_PARAM_XYZ(A_SGE_CONTROL));
	params[1] = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_REG) |
		     V_FW_PARAMS_PARAM_XYZ(A_SGE_HOST_PAGE_SIZE));
	params[2] = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_REG) |
		     V_FW_PARAMS_PARAM_XYZ(A_SGE_FL_BUFFER_SIZE0));
	params[3] = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_REG) |
		     V_FW_PARAMS_PARAM_XYZ(A_SGE_FL_BUFFER_SIZE1));
	params[4] = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_REG) |
		     V_FW_PARAMS_PARAM_XYZ(A_SGE_TIMER_VALUE_0_AND_1));
	params[5] = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_REG) |
		     V_FW_PARAMS_PARAM_XYZ(A_SGE_TIMER_VALUE_2_AND_3));
	params[6] = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_REG) |
		     V_FW_PARAMS_PARAM_XYZ(A_SGE_TIMER_VALUE_4_AND_5));
	v = t4vf_query_params(adap, 7, params, vals);
	if (v != FW_SUCCESS)
		return v;

	sge_control = vals[0];
	sge_host_page_size = vals[1];
	fl_small_pg = vals[2];
	fl_large_pg = vals[3];
	sge_timer_value_0_and_1 = vals[4];
	sge_timer_value_2_and_3 = vals[5];
	sge_timer_value_4_and_5 = vals[6];

	/*
	 * Start by vetting the basic SGE parameters which have been set up by
	 * the Physical Function Driver.
	 */

	/* We only bother using the Large Page logic if the Large Page Buffer
	 * is larger than our Page Size Buffer.
	 */
	if (fl_large_pg <= fl_small_pg)
		fl_large_pg = 0;

	/* The Page Size Buffer must be exactly equal to our Page Size and the
	 * Large Page Size Buffer should be 0 (per above) or a power of 2.
	 */
	if (fl_small_pg != CXGBE_PAGE_SIZE ||
	    (fl_large_pg & (fl_large_pg - 1)) != 0) {
		dev_err(adapter->pdev_dev, "bad SGE FL buffer sizes [%d, %d]\n",
			fl_small_pg, fl_large_pg);
		return -EINVAL;
	}

	if ((sge_control & F_RXPKTCPLMODE) !=
	    V_RXPKTCPLMODE(X_RXPKTCPLMODE_SPLIT)) {
		dev_err(adapter->pdev_dev, "bad SGE CPL MODE\n");
		return -EINVAL;
	}


	/* Grab ingress packing boundary from SGE_CONTROL2 for */
	params[0] = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_REG) |
		     V_FW_PARAMS_PARAM_XYZ(A_SGE_CONTROL2));
	v = t4vf_query_params(adap, 1, params, vals);
	if (v != FW_SUCCESS) {
		dev_err(adapter, "Unable to get SGE Control2; "
			"probably old firmware.\n");
		return v;
	}
	sge_control2 = vals[0];

	params[0] = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_REG) |
		     V_FW_PARAMS_PARAM_XYZ(A_SGE_INGRESS_RX_THRESHOLD));
	params[1] = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_REG) |
		     V_FW_PARAMS_PARAM_XYZ(A_SGE_CONM_CTRL));
	v = t4vf_query_params(adap, 2, params, vals);
	if (v != FW_SUCCESS)
		return v;
	sge_ingress_rx_threshold = vals[0];
	sge_congestion_control = vals[1];
	params[0] = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_REG) |
		     V_FW_PARAMS_PARAM_XYZ(A_SGE_EGRESS_QUEUES_PER_PAGE_VF));
	params[1] = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_REG) |
		     V_FW_PARAMS_PARAM_XYZ(A_SGE_INGRESS_QUEUES_PER_PAGE_VF));
	v = t4vf_query_params(adap, 2, params, vals);
	if (v != FW_SUCCESS) {
		dev_warn(adap, "Unable to get VF SGE Queues/Page; "
			 "probably old firmware.\n");
		return v;
	}
	sge_egress_queues_per_page = vals[0];
	sge_ingress_queues_per_page = vals[1];

	/*
	 * We need the Queues/Page for our VF.  This is based on the
	 * PF from which we're instantiated and is indexed in the
	 * register we just read.
	 */
	s_hps = (S_HOSTPAGESIZEPF0 +
		 (S_HOSTPAGESIZEPF1 - S_HOSTPAGESIZEPF0) * adap->pf);
	sge_params->hps =
		((sge_host_page_size >> s_hps) & M_HOSTPAGESIZEPF0);

	s_qpp = (S_QUEUESPERPAGEPF0 +
		 (S_QUEUESPERPAGEPF1 - S_QUEUESPERPAGEPF0) * adap->pf);
	sge_params->eq_qpp =
		((sge_egress_queues_per_page >> s_qpp)
		 & M_QUEUESPERPAGEPF0);
	sge_params->iq_qpp =
		((sge_ingress_queues_per_page >> s_qpp)
		 & M_QUEUESPERPAGEPF0);

	/*
	 * Now translate the queried parameters into our internal forms.
	 */
	if (fl_large_pg)
		s->fl_pg_order = ilog2(fl_large_pg) - PAGE_SHIFT;
	s->stat_len = ((sge_control & F_EGRSTATUSPAGESIZE)
			? 128 : 64);
	s->pktshift = G_PKTSHIFT(sge_control);
	s->fl_align = t4vf_fl_pkt_align(adap, sge_control, sge_control2);

	/*
	 * A FL with <= fl_starve_thres buffers is starving and a periodic
	 * timer will attempt to refill it.  This needs to be larger than the
	 * SGE's Egress Congestion Threshold.  If it isn't, then we can get
	 * stuck waiting for new packets while the SGE is waiting for us to
	 * give it more Free List entries.  (Note that the SGE's Egress
	 * Congestion Threshold is in units of 2 Free List pointers.)
	 */
	switch (CHELSIO_CHIP_VERSION(adap->params.chip)) {
	case CHELSIO_T5:
		s->fl_starve_thres =
			G_EGRTHRESHOLDPACKING(sge_congestion_control);
		break;
	case CHELSIO_T6:
	default:
		s->fl_starve_thres =
			G_T6_EGRTHRESHOLDPACKING(sge_congestion_control);
		break;
	}
	s->fl_starve_thres = s->fl_starve_thres * 2 + 1;

	/*
	 * Save RX interrupt holdoff timer values and counter
	 * threshold values from the SGE parameters.
	 */
	s->timer_val[0] = core_ticks_to_us(adap,
			G_TIMERVALUE0(sge_timer_value_0_and_1));
	s->timer_val[1] = core_ticks_to_us(adap,
			G_TIMERVALUE1(sge_timer_value_0_and_1));
	s->timer_val[2] = core_ticks_to_us(adap,
			G_TIMERVALUE2(sge_timer_value_2_and_3));
	s->timer_val[3] = core_ticks_to_us(adap,
			G_TIMERVALUE3(sge_timer_value_2_and_3));
	s->timer_val[4] = core_ticks_to_us(adap,
			G_TIMERVALUE4(sge_timer_value_4_and_5));
	s->timer_val[5] = core_ticks_to_us(adap,
			G_TIMERVALUE5(sge_timer_value_4_and_5));
	s->counter_val[0] = G_THRESHOLD_0(sge_ingress_rx_threshold);
	s->counter_val[1] = G_THRESHOLD_1(sge_ingress_rx_threshold);
	s->counter_val[2] = G_THRESHOLD_2(sge_ingress_rx_threshold);
	s->counter_val[3] = G_THRESHOLD_3(sge_ingress_rx_threshold);
	return 0;
}
