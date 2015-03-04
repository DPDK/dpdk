/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   Copyright 2014 6WIND S.A.
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
	rte_pktmbuf_init(mp, opaque_arg, _m, i);
	m->ol_flags |= CTRL_MBUF_FLAG;
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
	m->data_off = RTE_MIN(RTE_PKTMBUF_HEADROOM, (uint16_t)m->buf_len);

	/* init some constant fields */
	m->pool = mp;
	m->nb_segs = 1;
	m->port = 0xff;
}

/* do some sanity checks on a mbuf: panic if it fails */
void
rte_mbuf_sanity_check(const struct rte_mbuf *m, int is_header)
{
	const struct rte_mbuf *m_seg;
	unsigned nb_segs;

	if (m == NULL)
		rte_panic("mbuf is NULL\n");

	/* generic checks */
	if (m->pool == NULL)
		rte_panic("bad mbuf pool\n");
	if (m->buf_physaddr == 0)
		rte_panic("bad phys addr\n");
	if (m->buf_addr == NULL)
		rte_panic("bad virt addr\n");

	uint16_t cnt = rte_mbuf_refcnt_read(m);
	if ((cnt == 0) || (cnt == UINT16_MAX))
		rte_panic("bad ref cnt\n");

	/* nothing to check for sub-segments */
	if (is_header == 0)
		return;

	nb_segs = m->nb_segs;
	m_seg = m;
	while (m_seg && nb_segs != 0) {
		m_seg = m_seg->next;
		nb_segs--;
	}
	if (nb_segs != 0)
		rte_panic("bad nb_segs\n");
}

/* dump a mbuf on console */
void
rte_pktmbuf_dump(FILE *f, const struct rte_mbuf *m, unsigned dump_len)
{
	unsigned int len;
	unsigned nb_segs;

	__rte_mbuf_sanity_check(m, 1);

	fprintf(f, "dump mbuf at 0x%p, phys=%"PRIx64", buf_len=%u\n",
	       m, (uint64_t)m->buf_physaddr, (unsigned)m->buf_len);
	fprintf(f, "  pkt_len=%"PRIu32", ol_flags=%"PRIx64", nb_segs=%u, "
	       "in_port=%u\n", m->pkt_len, m->ol_flags,
	       (unsigned)m->nb_segs, (unsigned)m->port);
	nb_segs = m->nb_segs;

	while (m && nb_segs != 0) {
		__rte_mbuf_sanity_check(m, 0);

		fprintf(f, "  segment at 0x%p, data=0x%p, data_len=%u\n",
			m, rte_pktmbuf_mtod(m, void *), (unsigned)m->data_len);
		len = dump_len;
		if (len > m->data_len)
			len = m->data_len;
		if (len != 0)
			rte_hexdump(f, NULL, rte_pktmbuf_mtod(m, void *), len);
		dump_len -= len;
		m = m->next;
		nb_segs --;
	}
}

/*
 * Get the name of a RX offload flag. Must be kept synchronized with flag
 * definitions in rte_mbuf.h.
 */
const char *rte_get_rx_ol_flag_name(uint64_t mask)
{
	switch (mask) {
	case PKT_RX_VLAN_PKT: return "PKT_RX_VLAN_PKT";
	case PKT_RX_RSS_HASH: return "PKT_RX_RSS_HASH";
	case PKT_RX_FDIR: return "PKT_RX_FDIR";
	case PKT_RX_L4_CKSUM_BAD: return "PKT_RX_L4_CKSUM_BAD";
	case PKT_RX_IP_CKSUM_BAD: return "PKT_RX_IP_CKSUM_BAD";
	/* case PKT_RX_EIP_CKSUM_BAD: return "PKT_RX_EIP_CKSUM_BAD"; */
	/* case PKT_RX_OVERSIZE: return "PKT_RX_OVERSIZE"; */
	/* case PKT_RX_HBUF_OVERFLOW: return "PKT_RX_HBUF_OVERFLOW"; */
	/* case PKT_RX_RECIP_ERR: return "PKT_RX_RECIP_ERR"; */
	/* case PKT_RX_MAC_ERR: return "PKT_RX_MAC_ERR"; */
	case PKT_RX_IPV4_HDR: return "PKT_RX_IPV4_HDR";
	case PKT_RX_IPV4_HDR_EXT: return "PKT_RX_IPV4_HDR_EXT";
	case PKT_RX_IPV6_HDR: return "PKT_RX_IPV6_HDR";
	case PKT_RX_IPV6_HDR_EXT: return "PKT_RX_IPV6_HDR_EXT";
	case PKT_RX_IEEE1588_PTP: return "PKT_RX_IEEE1588_PTP";
	case PKT_RX_IEEE1588_TMST: return "PKT_RX_IEEE1588_TMST";
	case PKT_RX_TUNNEL_IPV4_HDR: return "PKT_RX_TUNNEL_IPV4_HDR";
	case PKT_RX_TUNNEL_IPV6_HDR: return "PKT_RX_TUNNEL_IPV6_HDR";
	default: return NULL;
	}
}

/*
 * Get the name of a TX offload flag. Must be kept synchronized with flag
 * definitions in rte_mbuf.h.
 */
const char *rte_get_tx_ol_flag_name(uint64_t mask)
{
	switch (mask) {
	case PKT_TX_VLAN_PKT: return "PKT_TX_VLAN_PKT";
	case PKT_TX_IP_CKSUM: return "PKT_TX_IP_CKSUM";
	case PKT_TX_TCP_CKSUM: return "PKT_TX_TCP_CKSUM";
	case PKT_TX_SCTP_CKSUM: return "PKT_TX_SCTP_CKSUM";
	case PKT_TX_UDP_CKSUM: return "PKT_TX_UDP_CKSUM";
	case PKT_TX_IEEE1588_TMST: return "PKT_TX_IEEE1588_TMST";
	case PKT_TX_TCP_SEG: return "PKT_TX_TCP_SEG";
	case PKT_TX_IPV4: return "PKT_TX_IPV4";
	case PKT_TX_IPV6: return "PKT_TX_IPV6";
	case PKT_TX_OUTER_IP_CKSUM: return "PKT_TX_OUTER_IP_CKSUM";
	case PKT_TX_OUTER_IPV4: return "PKT_TX_OUTER_IPV4";
	case PKT_TX_OUTER_IPV6: return "PKT_TX_OUTER_IPV6";
	default: return NULL;
	}
}
