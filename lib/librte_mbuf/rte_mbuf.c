/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <inttypes.h>
#include <errno.h>
#include <ctype.h>
#include <sys/queue.h>

#include <rte_debug.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>
#include <rte_hexdump.h>

/*
 * ctrlmbuf constructor, given as a callback function to
 * rte_mempool_create()
 */
void
rte_ctrlmbuf_init(struct rte_mempool *mp,
		  __attribute__((unused)) void *opaque_arg,
		  void *_m,
		  __attribute__((unused)) unsigned i)
{
	struct rte_mbuf *m = _m;

	memset(m, 0, mp->elt_size);

	/* start of buffer is just after mbuf structure */
	m->buf_addr = (char *)m + sizeof(struct rte_mbuf);
	m->buf_physaddr = rte_mempool_virt2phy(mp, m) +
			sizeof(struct rte_mbuf);
	m->buf_len = (uint16_t) (mp->elt_size - sizeof(struct rte_mbuf));

	/* init some constant fields */
	m->type = RTE_MBUF_CTRL;
	m->ctrl.data = (char *)m->buf_addr;
	m->pool = (struct rte_mempool *)mp;
}

/*
 * pktmbuf pool constructor, given as a callback function to
 * rte_mempool_create()
 */
void
rte_pktmbuf_pool_init(struct rte_mempool *mp, void *opaque_arg)
{
	struct rte_pktmbuf_pool_private *mbp_priv;
	uint16_t roomsz;

	mbp_priv = rte_mempool_get_priv(mp);
	roomsz = (uint16_t)(uintptr_t)opaque_arg;

	/* Use default data room size. */
	if (0 == roomsz)
		roomsz = 2048 + RTE_PKTMBUF_HEADROOM;

	mbp_priv->mbuf_data_room_size = roomsz;
}

/*
 * pktmbuf constructor, given as a callback function to
 * rte_mempool_create().
 * Set the fields of a packet mbuf to their default values.
 */
void
rte_pktmbuf_init(struct rte_mempool *mp,
		 __attribute__((unused)) void *opaque_arg,
		 void *_m,
		 __attribute__((unused)) unsigned i)
{
	struct rte_mbuf *m = _m;
	uint32_t buf_len = mp->elt_size - sizeof(struct rte_mbuf);

	RTE_MBUF_ASSERT(mp->elt_size >= sizeof(struct rte_mbuf));

	memset(m, 0, mp->elt_size);

	/* start of buffer is just after mbuf structure */
	m->buf_addr = (char *)m + sizeof(struct rte_mbuf);
	m->buf_physaddr = rte_mempool_virt2phy(mp, m) +
			sizeof(struct rte_mbuf);
	m->buf_len = (uint16_t)buf_len;

	/* keep some headroom between start of buffer and data */
	m->pkt.data = (char*) m->buf_addr + RTE_MIN(RTE_PKTMBUF_HEADROOM, m->buf_len);

	/* init some constant fields */
	m->type = RTE_MBUF_PKT;
	m->pool = mp;
	m->pkt.nb_segs = 1;
	m->pkt.in_port = 0xff;
}

/* do some sanity checks on a mbuf: panic if it fails */
void
rte_mbuf_sanity_check(const struct rte_mbuf *m, enum rte_mbuf_type t,
		      int is_header)
{
	const struct rte_mbuf *m_seg;
	unsigned nb_segs;

	if (m == NULL)
		rte_panic("mbuf is NULL\n");
	if (m->type != (uint8_t)t)
		rte_panic("bad mbuf type\n");

	/* generic checks */
	if (m->pool == NULL)
		rte_panic("bad mbuf pool\n");
	if (m->buf_physaddr == 0)
		rte_panic("bad phys addr\n");
	if (m->buf_addr == NULL)
		rte_panic("bad virt addr\n");

#ifdef RTE_MBUF_SCATTER_GATHER
	uint16_t cnt = rte_mbuf_refcnt_read(m);
	if ((cnt == 0) || (cnt == UINT16_MAX))
		rte_panic("bad ref cnt\n");
#endif

	/* nothing to check for ctrl messages */
	if (m->type == RTE_MBUF_CTRL)
		return;

	/* check pkt consistency */
	else if (m->type == RTE_MBUF_PKT) {

		/* nothing to check for sub-segments */
		if (is_header == 0)
			return;

		nb_segs = m->pkt.nb_segs;
		m_seg = m;
		while (m_seg && nb_segs != 0) {
			m_seg = m_seg->pkt.next;
			nb_segs --;
		}
		if (nb_segs != 0)
			rte_panic("bad nb_segs\n");
		return;
	}

	rte_panic("unknown mbuf type\n");
}

/* dump a mbuf on console */
void
rte_pktmbuf_dump(FILE *f, const struct rte_mbuf *m, unsigned dump_len)
{
	unsigned int len;
	unsigned nb_segs;

	__rte_mbuf_sanity_check(m, RTE_MBUF_PKT, 1);

	fprintf(f, "dump mbuf at 0x%p, phys=%"PRIx64", buf_len=%u\n",
	       m, (uint64_t)m->buf_physaddr, (unsigned)m->buf_len);
	fprintf(f, "  pkt_len=%"PRIu32", ol_flags=%"PRIx16", nb_segs=%u, "
	       "in_port=%u\n", m->pkt.pkt_len, m->ol_flags,
	       (unsigned)m->pkt.nb_segs, (unsigned)m->pkt.in_port);
	nb_segs = m->pkt.nb_segs;

	while (m && nb_segs != 0) {
		__rte_mbuf_sanity_check(m, RTE_MBUF_PKT, 0);

		fprintf(f, "  segment at 0x%p, data=0x%p, data_len=%u\n",
		       m, m->pkt.data, (unsigned)m->pkt.data_len);
		len = dump_len;
		if (len > m->pkt.data_len)
			len = m->pkt.data_len;
		if (len != 0)
			rte_hexdump(f, NULL, m->pkt.data, len);
		dump_len -= len;
		m = m->pkt.next;
		nb_segs --;
	}
}
