/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_RX_H__
#define __OTX2_RX_H__

#include <rte_ether.h>

#include "otx2_common.h"
#include "otx2_ethdev_sec.h"
#include "otx2_ipsec_fp.h"

/* Default mark value used when none is provided. */
#define OTX2_FLOW_ACTION_FLAG_DEFAULT	0xffff

#define PTYPE_NON_TUNNEL_WIDTH		16
#define PTYPE_TUNNEL_WIDTH		12
#define PTYPE_NON_TUNNEL_ARRAY_SZ	BIT(PTYPE_NON_TUNNEL_WIDTH)
#define PTYPE_TUNNEL_ARRAY_SZ		BIT(PTYPE_TUNNEL_WIDTH)
#define PTYPE_ARRAY_SZ			((PTYPE_NON_TUNNEL_ARRAY_SZ +\
					 PTYPE_TUNNEL_ARRAY_SZ) *\
					 sizeof(uint16_t))

#define NIX_RX_OFFLOAD_NONE            (0)
#define NIX_RX_OFFLOAD_RSS_F           BIT(0)
#define NIX_RX_OFFLOAD_PTYPE_F         BIT(1)
#define NIX_RX_OFFLOAD_CHECKSUM_F      BIT(2)
#define NIX_RX_OFFLOAD_VLAN_STRIP_F    BIT(3)
#define NIX_RX_OFFLOAD_MARK_UPDATE_F   BIT(4)
#define NIX_RX_OFFLOAD_TSTAMP_F        BIT(5)
#define NIX_RX_OFFLOAD_SECURITY_F      BIT(6)

/* Flags to control cqe_to_mbuf conversion function.
 * Defining it from backwards to denote its been
 * not used as offload flags to pick function
 */
#define NIX_RX_MULTI_SEG_F            BIT(15)
#define NIX_TIMESYNC_RX_OFFSET		8

/* Inline IPsec offsets */

#define INLINE_INB_RPTR_HDR		16
/* nix_cqe_hdr_s + nix_rx_parse_s + nix_rx_sg_s + nix_iova_s */
#define INLINE_CPT_RESULT_OFFSET	80

struct otx2_timesync_info {
	uint64_t	rx_tstamp;
	rte_iova_t	tx_tstamp_iova;
	uint64_t	*tx_tstamp;
	uint8_t		tx_ready;
	uint8_t		rx_ready;
} __rte_cache_aligned;

union mbuf_initializer {
	struct {
		uint16_t data_off;
		uint16_t refcnt;
		uint16_t nb_segs;
		uint16_t port;
	} fields;
	uint64_t value;
};

static __rte_always_inline void
otx2_nix_mbuf_to_tstamp(struct rte_mbuf *mbuf,
			struct otx2_timesync_info *tstamp, const uint16_t flag,
			uint64_t *tstamp_ptr)
{
	if ((flag & NIX_RX_OFFLOAD_TSTAMP_F) &&
	    (mbuf->data_off == RTE_PKTMBUF_HEADROOM +
	     NIX_TIMESYNC_RX_OFFSET)) {

		mbuf->pkt_len -= NIX_TIMESYNC_RX_OFFSET;

		/* Reading the rx timestamp inserted by CGX, viz at
		 * starting of the packet data.
		 */
		mbuf->timestamp = rte_be_to_cpu_64(*tstamp_ptr);
		/* PKT_RX_IEEE1588_TMST flag needs to be set only in case
		 * PTP packets are received.
		 */
		if (mbuf->packet_type == RTE_PTYPE_L2_ETHER_TIMESYNC) {
			tstamp->rx_tstamp = mbuf->timestamp;
			tstamp->rx_ready = 1;
			mbuf->ol_flags |= PKT_RX_IEEE1588_PTP |
				PKT_RX_IEEE1588_TMST | PKT_RX_TIMESTAMP;
		}
	}
}

static __rte_always_inline uint64_t
nix_clear_data_off(uint64_t oldval)
{
	union mbuf_initializer mbuf_init = { .value = oldval };

	mbuf_init.fields.data_off = 0;
	return mbuf_init.value;
}

static __rte_always_inline struct rte_mbuf *
nix_get_mbuf_from_cqe(void *cq, const uint64_t data_off)
{
	rte_iova_t buff;

	/* Skip CQE, NIX_RX_PARSE_S and SG HDR(9 DWORDs) and peek buff addr */
	buff = *((rte_iova_t *)((uint64_t *)cq + 9));
	return (struct rte_mbuf *)(buff - data_off);
}


static __rte_always_inline uint32_t
nix_ptype_get(const void * const lookup_mem, const uint64_t in)
{
	const uint16_t * const ptype = lookup_mem;
	const uint16_t lh_lg_lf = (in & 0xFFF0000000000000) >> 52;
	const uint16_t tu_l2 = ptype[(in & 0x000FFFF000000000) >> 36];
	const uint16_t il4_tu = ptype[PTYPE_NON_TUNNEL_ARRAY_SZ + lh_lg_lf];

	return (il4_tu << PTYPE_NON_TUNNEL_WIDTH) | tu_l2;
}

static __rte_always_inline uint32_t
nix_rx_olflags_get(const void * const lookup_mem, const uint64_t in)
{
	const uint32_t * const ol_flags = (const uint32_t *)
			((const uint8_t *)lookup_mem + PTYPE_ARRAY_SZ);

	return ol_flags[(in & 0xfff00000) >> 20];
}

static inline uint64_t
nix_update_match_id(const uint16_t match_id, uint64_t ol_flags,
		    struct rte_mbuf *mbuf)
{
	/* There is no separate bit to check match_id
	 * is valid or not? and no flag to identify it is an
	 * RTE_FLOW_ACTION_TYPE_FLAG vs RTE_FLOW_ACTION_TYPE_MARK
	 * action. The former case addressed through 0 being invalid
	 * value and inc/dec match_id pair when MARK is activated.
	 * The later case addressed through defining
	 * OTX2_FLOW_MARK_DEFAULT as value for
	 * RTE_FLOW_ACTION_TYPE_MARK.
	 * This would translate to not use
	 * OTX2_FLOW_ACTION_FLAG_DEFAULT - 1 and
	 * OTX2_FLOW_ACTION_FLAG_DEFAULT for match_id.
	 * i.e valid mark_id's are from
	 * 0 to OTX2_FLOW_ACTION_FLAG_DEFAULT - 2
	 */
	if (likely(match_id)) {
		ol_flags |= PKT_RX_FDIR;
		if (match_id != OTX2_FLOW_ACTION_FLAG_DEFAULT) {
			ol_flags |= PKT_RX_FDIR_ID;
			mbuf->hash.fdir.hi = match_id - 1;
		}
	}

	return ol_flags;
}

static __rte_always_inline void
nix_cqe_xtract_mseg(const struct nix_rx_parse_s *rx,
		    struct rte_mbuf *mbuf, uint64_t rearm)
{
	const rte_iova_t *iova_list;
	struct rte_mbuf *head;
	const rte_iova_t *eol;
	uint8_t nb_segs;
	uint64_t sg;

	sg = *(const uint64_t *)(rx + 1);
	nb_segs = (sg >> 48) & 0x3;
	mbuf->nb_segs = nb_segs;
	mbuf->data_len = sg & 0xFFFF;
	sg = sg >> 16;

	eol = ((const rte_iova_t *)(rx + 1) + ((rx->desc_sizem1 + 1) << 1));
	/* Skip SG_S and first IOVA*/
	iova_list = ((const rte_iova_t *)(rx + 1)) + 2;
	nb_segs--;

	rearm = rearm & ~0xFFFF;

	head = mbuf;
	while (nb_segs) {
		mbuf->next = ((struct rte_mbuf *)*iova_list) - 1;
		mbuf = mbuf->next;

		__mempool_check_cookies(mbuf->pool, (void **)&mbuf, 1, 1);

		mbuf->data_len = sg & 0xFFFF;
		sg = sg >> 16;
		*(uint64_t *)(&mbuf->rearm_data) = rearm;
		nb_segs--;
		iova_list++;

		if (!nb_segs && (iova_list + 1 < eol)) {
			sg = *(const uint64_t *)(iova_list);
			nb_segs = (sg >> 48) & 0x3;
			head->nb_segs += nb_segs;
			iova_list = (const rte_iova_t *)(iova_list + 1);
		}
	}
}

static __rte_always_inline uint16_t
nix_rx_sec_cptres_get(const void *cq)
{
	volatile const struct otx2_cpt_res *res;

	res = (volatile const struct otx2_cpt_res *)((const char *)cq +
			INLINE_CPT_RESULT_OFFSET);

	return res->u16[0];
}

static __rte_always_inline void *
nix_rx_sec_sa_get(const void * const lookup_mem, int spi, uint16_t port)
{
	const uint64_t *const *sa_tbl = (const uint64_t * const *)
			((const uint8_t *)lookup_mem + OTX2_NIX_SA_TBL_START);

	return (void *)sa_tbl[port][spi];
}

static __rte_always_inline uint64_t
nix_rx_sec_mbuf_update(const struct nix_cqe_hdr_s *cq, struct rte_mbuf *m,
		       const void * const lookup_mem)
{
	struct otx2_ipsec_fp_in_sa *sa;
	struct rte_ipv4_hdr *ipv4;
	uint16_t m_len;
	uint32_t spi;
	char *data;

	if (unlikely(nix_rx_sec_cptres_get(cq) != OTX2_SEC_COMP_GOOD))
		return PKT_RX_SEC_OFFLOAD | PKT_RX_SEC_OFFLOAD_FAILED;

	/* 20 bits of tag would have the SPI */
	spi = cq->tag & 0xFFFFF;

	sa = nix_rx_sec_sa_get(lookup_mem, spi, m->port);
	m->udata64 = (uint64_t)sa->userdata;

	data = rte_pktmbuf_mtod(m, char *);
	memcpy(data + INLINE_INB_RPTR_HDR, data, RTE_ETHER_HDR_LEN);

	m->data_off += INLINE_INB_RPTR_HDR;

	ipv4 = (struct rte_ipv4_hdr *)(data + INLINE_INB_RPTR_HDR +
				       RTE_ETHER_HDR_LEN);

	m_len = rte_be_to_cpu_16(ipv4->total_length) + RTE_ETHER_HDR_LEN;

	m->data_len = m_len;
	m->pkt_len = m_len;
	return PKT_RX_SEC_OFFLOAD;
}

static __rte_always_inline void
otx2_nix_cqe_to_mbuf(const struct nix_cqe_hdr_s *cq, const uint32_t tag,
		     struct rte_mbuf *mbuf, const void *lookup_mem,
		     const uint64_t val, const uint16_t flag)
{
	const struct nix_rx_parse_s *rx =
		 (const struct nix_rx_parse_s *)((const uint64_t *)cq + 1);
	const uint64_t w1 = *(const uint64_t *)rx;
	const uint16_t len = rx->pkt_lenm1 + 1;
	uint64_t ol_flags = 0;

	/* Mark mempool obj as "get" as it is alloc'ed by NIX */
	__mempool_check_cookies(mbuf->pool, (void **)&mbuf, 1, 1);

	if (flag & NIX_RX_OFFLOAD_PTYPE_F)
		mbuf->packet_type = nix_ptype_get(lookup_mem, w1);
	else
		mbuf->packet_type = 0;

	if (flag & NIX_RX_OFFLOAD_RSS_F) {
		mbuf->hash.rss = tag;
		ol_flags |= PKT_RX_RSS_HASH;
	}

	if (flag & NIX_RX_OFFLOAD_CHECKSUM_F)
		ol_flags |= nix_rx_olflags_get(lookup_mem, w1);

	if (flag & NIX_RX_OFFLOAD_VLAN_STRIP_F) {
		if (rx->vtag0_gone) {
			ol_flags |= PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED;
			mbuf->vlan_tci = rx->vtag0_tci;
		}
		if (rx->vtag1_gone) {
			ol_flags |= PKT_RX_QINQ | PKT_RX_QINQ_STRIPPED;
			mbuf->vlan_tci_outer = rx->vtag1_tci;
		}
	}

	if (flag & NIX_RX_OFFLOAD_MARK_UPDATE_F)
		ol_flags = nix_update_match_id(rx->match_id, ol_flags, mbuf);

	if ((flag & NIX_RX_OFFLOAD_SECURITY_F) &&
	    cq->cqe_type == NIX_XQE_TYPE_RX_IPSECH) {
		*(uint64_t *)(&mbuf->rearm_data) = val;
		ol_flags |= nix_rx_sec_mbuf_update(cq, mbuf, lookup_mem);
		mbuf->ol_flags = ol_flags;
		return;
	}

	mbuf->ol_flags = ol_flags;
	*(uint64_t *)(&mbuf->rearm_data) = val;
	mbuf->pkt_len = len;

	if (flag & NIX_RX_MULTI_SEG_F)
		nix_cqe_xtract_mseg(rx, mbuf, val);
	else
		mbuf->data_len = len;
}

#define CKSUM_F NIX_RX_OFFLOAD_CHECKSUM_F
#define PTYPE_F NIX_RX_OFFLOAD_PTYPE_F
#define RSS_F	NIX_RX_OFFLOAD_RSS_F
#define RX_VLAN_F  NIX_RX_OFFLOAD_VLAN_STRIP_F
#define MARK_F  NIX_RX_OFFLOAD_MARK_UPDATE_F
#define TS_F	NIX_RX_OFFLOAD_TSTAMP_F
#define RX_SEC_F   NIX_RX_OFFLOAD_SECURITY_F

/* [SEC] [TSMP] [MARK] [VLAN] [CKSUM] [PTYPE] [RSS] */
#define NIX_RX_FASTPATH_MODES						       \
R(no_offload,			0, 0, 0, 0, 0, 0, 0, NIX_RX_OFFLOAD_NONE)      \
R(rss,				0, 0, 0, 0, 0, 0, 1, RSS_F)		       \
R(ptype,			0, 0, 0, 0, 0, 1, 0, PTYPE_F)		       \
R(ptype_rss,			0, 0, 0, 0, 0, 1, 1, PTYPE_F | RSS_F)	       \
R(cksum,			0, 0, 0, 0, 1, 0, 0, CKSUM_F)		       \
R(cksum_rss,			0, 0, 0, 0, 1, 0, 1, CKSUM_F | RSS_F)	       \
R(cksum_ptype,			0, 0, 0, 0, 1, 1, 0, CKSUM_F | PTYPE_F)	       \
R(cksum_ptype_rss,		0, 0, 0, 0, 1, 1, 1, CKSUM_F | PTYPE_F | RSS_F)\
R(vlan,				0, 0, 0, 1, 0, 0, 0, RX_VLAN_F)		       \
R(vlan_rss,			0, 0, 0, 1, 0, 0, 1, RX_VLAN_F | RSS_F)	       \
R(vlan_ptype,			0, 0, 0, 1, 0, 1, 0, RX_VLAN_F | PTYPE_F)      \
R(vlan_ptype_rss,		0, 0, 0, 1, 0, 1, 1,			       \
			RX_VLAN_F | PTYPE_F | RSS_F)			       \
R(vlan_cksum,			0, 0, 0, 1, 1, 0, 0, RX_VLAN_F | CKSUM_F)      \
R(vlan_cksum_rss,		0, 0, 0, 1, 1, 0, 1,			       \
			RX_VLAN_F | CKSUM_F | RSS_F)			       \
R(vlan_cksum_ptype,		0, 0, 0, 1, 1, 1, 0,			       \
			RX_VLAN_F | CKSUM_F | PTYPE_F)			       \
R(vlan_cksum_ptype_rss,		0, 0, 0, 1, 1, 1, 1,			       \
			RX_VLAN_F | CKSUM_F | PTYPE_F | RSS_F)		       \
R(mark,				0, 0, 1, 0, 0, 0, 0, MARK_F)		       \
R(mark_rss,			0, 0, 1, 0, 0, 0, 1, MARK_F | RSS_F)	       \
R(mark_ptype,			0, 0, 1, 0, 0, 1, 0, MARK_F | PTYPE_F)	       \
R(mark_ptype_rss,		0, 0, 1, 0, 0, 1, 1, MARK_F | PTYPE_F | RSS_F) \
R(mark_cksum,			0, 0, 1, 0, 1, 0, 0, MARK_F | CKSUM_F)	       \
R(mark_cksum_rss,		0, 0, 1, 0, 1, 0, 1, MARK_F | CKSUM_F | RSS_F) \
R(mark_cksum_ptype,		0, 0, 1, 0, 1, 1, 0,			       \
			MARK_F | CKSUM_F | PTYPE_F)			       \
R(mark_cksum_ptype_rss,		0, 0, 1, 0, 1, 1, 1,			       \
			MARK_F | CKSUM_F | PTYPE_F | RSS_F)		       \
R(mark_vlan,			0, 0, 1, 1, 0, 0, 0, MARK_F | RX_VLAN_F)       \
R(mark_vlan_rss,		0, 0, 1, 1, 0, 0, 1,			       \
			MARK_F | RX_VLAN_F | RSS_F)			       \
R(mark_vlan_ptype,		0, 0, 1, 1, 0, 1, 0,			       \
			MARK_F | RX_VLAN_F | PTYPE_F)			       \
R(mark_vlan_ptype_rss,		0, 0, 1, 1, 0, 1, 1,			       \
			MARK_F | RX_VLAN_F | PTYPE_F | RSS_F)		       \
R(mark_vlan_cksum,		0, 0, 1, 1, 1, 0, 0,			       \
			MARK_F | RX_VLAN_F | CKSUM_F)			       \
R(mark_vlan_cksum_rss,		0, 0, 1, 1, 1, 0, 1,			       \
			MARK_F | RX_VLAN_F | CKSUM_F | RSS_F)		       \
R(mark_vlan_cksum_ptype,	0, 0, 1, 1, 1, 1, 0,			       \
			MARK_F | RX_VLAN_F | CKSUM_F | PTYPE_F)		       \
R(mark_vlan_cksum_ptype_rss,	0, 0, 1, 1, 1, 1, 1,			       \
			MARK_F | RX_VLAN_F | CKSUM_F | PTYPE_F | RSS_F)	       \
R(ts,				0, 1, 0, 0, 0, 0, 0, TS_F)		       \
R(ts_rss,			0, 1, 0, 0, 0, 0, 1, TS_F | RSS_F)	       \
R(ts_ptype,			0, 1, 0, 0, 0, 1, 0, TS_F | PTYPE_F)	       \
R(ts_ptype_rss,			0, 1, 0, 0, 0, 1, 1, TS_F | PTYPE_F | RSS_F)   \
R(ts_cksum,			0, 1, 0, 0, 1, 0, 0, TS_F | CKSUM_F)	       \
R(ts_cksum_rss,			0, 1, 0, 0, 1, 0, 1, TS_F | CKSUM_F | RSS_F)   \
R(ts_cksum_ptype,		0, 1, 0, 0, 1, 1, 0, TS_F | CKSUM_F | PTYPE_F) \
R(ts_cksum_ptype_rss,		0, 1, 0, 0, 1, 1, 1,			       \
			TS_F | CKSUM_F | PTYPE_F | RSS_F)		       \
R(ts_vlan,			0, 1, 0, 1, 0, 0, 0, TS_F | RX_VLAN_F)	       \
R(ts_vlan_rss,			0, 1, 0, 1, 0, 0, 1, TS_F | RX_VLAN_F | RSS_F) \
R(ts_vlan_ptype,		0, 1, 0, 1, 0, 1, 0,			       \
			TS_F | RX_VLAN_F | PTYPE_F)			       \
R(ts_vlan_ptype_rss,		0, 1, 0, 1, 0, 1, 1,			       \
			TS_F | RX_VLAN_F | PTYPE_F | RSS_F)		       \
R(ts_vlan_cksum,		0, 1, 0, 1, 1, 0, 0,			       \
			TS_F | RX_VLAN_F | CKSUM_F)			       \
R(ts_vlan_cksum_rss,		0, 1, 0, 1, 1, 0, 1,			       \
			MARK_F | RX_VLAN_F | CKSUM_F | RSS_F)		       \
R(ts_vlan_cksum_ptype,		0, 1, 0, 1, 1, 1, 0,			       \
			TS_F | RX_VLAN_F | CKSUM_F | PTYPE_F)		       \
R(ts_vlan_cksum_ptype_rss,	0, 1, 0, 1, 1, 1, 1,			       \
			TS_F | RX_VLAN_F | CKSUM_F | PTYPE_F | RSS_F)	       \
R(ts_mark,			0, 1, 1, 0, 0, 0, 0, TS_F | MARK_F)	       \
R(ts_mark_rss,			0, 1, 1, 0, 0, 0, 1, TS_F | MARK_F | RSS_F)    \
R(ts_mark_ptype,		0, 1, 1, 0, 0, 1, 0, TS_F | MARK_F | PTYPE_F)  \
R(ts_mark_ptype_rss,		0, 1, 1, 0, 0, 1, 1,			       \
			TS_F | MARK_F | PTYPE_F | RSS_F)		       \
R(ts_mark_cksum,		0, 1, 1, 0, 1, 0, 0, TS_F | MARK_F | CKSUM_F)  \
R(ts_mark_cksum_rss,		0, 1, 1, 0, 1, 0, 1,			       \
			TS_F | MARK_F | CKSUM_F | RSS_F)		       \
R(ts_mark_cksum_ptype,		0, 1, 1, 0, 1, 1, 0,			       \
			TS_F | MARK_F | CKSUM_F | PTYPE_F)		       \
R(ts_mark_cksum_ptype_rss,	0, 1, 1, 0, 1, 1, 1,			       \
			TS_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)	       \
R(ts_mark_vlan,			0, 1, 1, 1, 0, 0, 0, TS_F | MARK_F | RX_VLAN_F)\
R(ts_mark_vlan_rss,		0, 1, 1, 1, 0, 0, 1,			       \
			TS_F | MARK_F | RX_VLAN_F | RSS_F)		       \
R(ts_mark_vlan_ptype,		0, 1, 1, 1, 0, 1, 0,			       \
			TS_F | MARK_F | RX_VLAN_F | PTYPE_F)		       \
R(ts_mark_vlan_ptype_rss,	0, 1, 1, 1, 0, 1, 1,			       \
			TS_F | MARK_F | RX_VLAN_F | PTYPE_F | RSS_F)	       \
R(ts_mark_vlan_cksum_ptype,	0, 1, 1, 1, 1, 1, 0,			       \
			TS_F | MARK_F | RX_VLAN_F | CKSUM_F | PTYPE_F)	       \
R(ts_mark_vlan_cksum_ptype_rss,	0, 1, 1, 1, 1, 1, 1,			       \
			TS_F | MARK_F | RX_VLAN_F | CKSUM_F | PTYPE_F | RSS_F) \
R(sec,				1, 0, 0, 0, 0, 0, 0, RX_SEC_F)		       \
R(sec_rss,			1, 0, 0, 0, 0, 0, 1, RX_SEC_F | RSS_F)	       \
R(sec_ptype,			1, 0, 0, 0, 0, 1, 0, RX_SEC_F | PTYPE_F)       \
R(sec_ptype_rss,		1, 0, 0, 0, 0, 1, 1,			       \
			RX_SEC_F | PTYPE_F | RSS_F)			       \
R(sec_cksum,			1, 0, 0, 0, 1, 0, 0, RX_SEC_F | CKSUM_F)       \
R(sec_cksum_rss,		1, 0, 0, 0, 1, 0, 1,			       \
			RX_SEC_F | CKSUM_F | RSS_F)			       \
R(sec_cksum_ptype,		1, 0, 0, 0, 1, 1, 0,			       \
			RX_SEC_F | CKSUM_F | PTYPE_F)			       \
R(sec_cksum_ptype_rss,		1, 0, 0, 0, 1, 1, 1,			       \
			RX_SEC_F | CKSUM_F | PTYPE_F | RSS_F)		       \
R(sec_vlan,			1, 0, 0, 1, 0, 0, 0, RX_SEC_F | RX_VLAN_F)     \
R(sec_vlan_rss,			1, 0, 0, 1, 0, 0, 1,			       \
			RX_SEC_F | RX_VLAN_F | RSS_F)			       \
R(sec_vlan_ptype,		1, 0, 0, 1, 0, 1, 0,			       \
			RX_SEC_F | RX_VLAN_F | PTYPE_F)			       \
R(sec_vlan_ptype_rss,		1, 0, 0, 1, 0, 1, 1,			       \
			RX_SEC_F | RX_VLAN_F | PTYPE_F | RSS_F)		       \
R(sec_vlan_cksum,		1, 0, 0, 1, 1, 0, 0,			       \
			RX_SEC_F | RX_VLAN_F | CKSUM_F)			       \
R(sec_vlan_cksum_rss,		1, 0, 0, 1, 1, 0, 1,			       \
			RX_SEC_F | RX_VLAN_F | CKSUM_F | RSS_F)		       \
R(sec_vlan_cksum_ptype,		1, 0, 0, 1, 1, 1, 0,			       \
			RX_SEC_F | RX_VLAN_F | CKSUM_F | PTYPE_F)	       \
R(sec_vlan_cksum_ptype_rss,	1, 0, 0, 1, 1, 1, 1,			       \
			RX_SEC_F | RX_VLAN_F | CKSUM_F | PTYPE_F | RSS_F)      \
R(sec_mark,			1, 0, 1, 0, 0, 0, 0, RX_SEC_F | MARK_F)	       \
R(sec_mark_rss,			1, 0, 1, 0, 0, 0, 1, RX_SEC_F | MARK_F | RSS_F)\
R(sec_mark_ptype,		1, 0, 1, 0, 0, 1, 0,			       \
			RX_SEC_F | MARK_F | PTYPE_F)			       \
R(sec_mark_ptype_rss,		1, 0, 1, 0, 0, 1, 1,			       \
			RX_SEC_F | MARK_F | PTYPE_F | RSS_F)		       \
R(sec_mark_cksum,		1, 0, 1, 0, 1, 0, 0,			       \
			RX_SEC_F | MARK_F | CKSUM_F)			       \
R(sec_mark_cksum_rss,		1, 0, 1, 0, 1, 0, 1,			       \
			RX_SEC_F | MARK_F | CKSUM_F | RSS_F)		       \
R(sec_mark_cksum_ptype,		1, 0, 1, 0, 1, 1, 0,			       \
			RX_SEC_F | MARK_F | CKSUM_F | PTYPE_F)		       \
R(sec_mark_cksum_ptype_rss,	1, 0, 1, 0, 1, 1, 1,			       \
			RX_SEC_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)	       \
R(sec_mark_vlan,		1, 0, 1, 1, 0, 0, 0, RX_SEC_F | RX_VLAN_F)     \
R(sec_mark_vlan_rss,		1, 0, 1, 1, 0, 0, 1,			       \
			RX_SEC_F | MARK_F | RX_VLAN_F | RSS_F)		       \
R(sec_mark_vlan_ptype,		1, 0, 1, 1, 0, 1, 0,			       \
			RX_SEC_F | MARK_F | RX_VLAN_F | PTYPE_F)	       \
R(sec_mark_vlan_ptype_rss,	1, 0, 1, 1, 0, 1, 1,			       \
			RX_SEC_F | MARK_F | RX_VLAN_F | PTYPE_F | RSS_F)       \
R(sec_mark_vlan_cksum,		1, 0, 1, 1, 1, 0, 0,			       \
			RX_SEC_F | MARK_F | RX_VLAN_F | CKSUM_F)	       \
R(sec_mark_vlan_cksum_rss,	1, 0, 1, 1, 1, 0, 1,			       \
			RX_SEC_F | MARK_F | RX_VLAN_F | CKSUM_F | RSS_F)       \
R(sec_mark_vlan_cksum_ptype,	1, 0, 1, 1, 1, 1, 0,			       \
			RX_SEC_F | MARK_F | RX_VLAN_F | CKSUM_F | PTYPE_F)     \
R(sec_mark_vlan_cksum_ptype_rss,					       \
				1, 0, 1, 1, 1, 1, 1,			       \
			RX_SEC_F | MARK_F | RX_VLAN_F | CKSUM_F | PTYPE_F |    \
			RSS_F)						       \
R(sec_ts,			1, 1, 0, 0, 0, 0, 0, RX_SEC_F | TS_F)	       \
R(sec_ts_rss,			1, 1, 0, 0, 0, 0, 1, RX_SEC_F | TS_F | RSS_F)  \
R(sec_ts_ptype,			1, 1, 0, 0, 0, 1, 0, RX_SEC_F | TS_F | PTYPE_F)\
R(sec_ts_ptype_rss,		1, 1, 0, 0, 0, 1, 1,			       \
			RX_SEC_F | TS_F | PTYPE_F | RSS_F)		       \
R(sec_ts_cksum,			1, 1, 0, 0, 1, 0, 0, RX_SEC_F | TS_F | CKSUM_F)\
R(sec_ts_cksum_rss,		1, 1, 0, 0, 1, 0, 1,			       \
			RX_SEC_F | TS_F | CKSUM_F | RSS_F)		       \
R(sec_ts_cksum_ptype,		1, 1, 0, 0, 1, 1, 0,			       \
			RX_SEC_F | CKSUM_F | PTYPE_F)			       \
R(sec_ts_cksum_ptype_rss,	1, 1, 0, 0, 1, 1, 1,			       \
			RX_SEC_F | TS_F | CKSUM_F | PTYPE_F | RSS_F)	       \
R(sec_ts_vlan,			1, 1, 0, 1, 0, 0, 0,			       \
			RX_SEC_F | TS_F | RX_VLAN_F)			       \
R(sec_ts_vlan_rss,		1, 1, 0, 1, 0, 0, 1,			       \
			RX_SEC_F | TS_F | RX_VLAN_F | RSS_F)		       \
R(sec_ts_vlan_ptype,		1, 1, 0, 1, 0, 1, 0,			       \
			RX_SEC_F | TS_F | RX_VLAN_F | PTYPE_F)		       \
R(sec_ts_vlan_ptype_rss,	1, 1, 0, 1, 0, 1, 1,			       \
			RX_SEC_F | TS_F | RX_VLAN_F | PTYPE_F | RSS_F)	       \
R(sec_ts_vlan_cksum,		1, 1, 0, 1, 1, 0, 0,			       \
			RX_SEC_F | TS_F | RX_VLAN_F | CKSUM_F)		       \
R(sec_ts_vlan_cksum_rss,	1, 1, 0, 1, 1, 0, 1,			       \
			RX_SEC_F | TS_F | RX_VLAN_F | CKSUM_F | RSS_F)	       \
R(sec_ts_vlan_cksum_ptype,	1, 1, 0, 1, 1, 1, 0,			       \
			RX_SEC_F | TS_F | RX_VLAN_F | CKSUM_F | PTYPE_F)       \
R(sec_ts_vlan_cksum_ptype_rss,	1, 1, 0, 1, 1, 1, 1,			       \
			RX_SEC_F | TS_F | RX_VLAN_F | CKSUM_F | PTYPE_F |      \
			RSS_F)						       \
R(sec_ts_mark,			1, 1, 1, 0, 0, 0, 0, RX_SEC_F | TS_F | MARK_F) \
R(sec_ts_mark_rss,		1, 1, 1, 0, 0, 0, 1,			       \
			RX_SEC_F | TS_F | MARK_F | RSS_F)		       \
R(sec_ts_mark_ptype,		1, 1, 1, 0, 0, 1, 0,			       \
			RX_SEC_F | TS_F | MARK_F | PTYPE_F)		       \
R(sec_ts_mark_ptype_rss,	1, 1, 1, 0, 0, 1, 1,			       \
			RX_SEC_F | TS_F | MARK_F | PTYPE_F | RSS_F)	       \
R(sec_ts_mark_cksum,		1, 1, 1, 0, 1, 0, 0,			       \
			RX_SEC_F | TS_F | MARK_F | CKSUM_F)		       \
R(sec_ts_mark_cksum_rss,	1, 1, 1, 0, 1, 0, 1,			       \
			RX_SEC_F | TS_F | MARK_F | CKSUM_F | RSS_F)	       \
R(sec_ts_mark_cksum_ptype,	1, 1, 1, 0, 1, 1, 0,			       \
			RX_SEC_F | TS_F | MARK_F | CKSUM_F | PTYPE_F)	       \
R(sec_ts_mark_cksum_ptype_rss,	1, 1, 1, 0, 1, 1, 1,			       \
			RX_SEC_F | TS_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)  \
R(sec_ts_mark_vlan,		1, 1, 1, 1, 0, 0, 0,			       \
			RX_SEC_F | TS_F | MARK_F | RX_VLAN_F)		       \
R(sec_ts_mark_vlan_rss,		1, 1, 1, 1, 0, 0, 1,			       \
			RX_SEC_F | RX_VLAN_F | RSS_F)			       \
R(sec_ts_mark_vlan_ptype,	1, 1, 1, 1, 0, 1, 0,			       \
			RX_SEC_F | TS_F | MARK_F | RX_VLAN_F | PTYPE_F)	       \
R(sec_ts_mark_vlan_ptype_rss,	1, 1, 1, 1, 0, 1, 1,			       \
			RX_SEC_F | TS_F | MARK_F | RX_VLAN_F | PTYPE_F | RSS_F)\
R(sec_ts_mark_vlan_cksum,	1, 1, 1, 1, 1, 0, 0,			       \
			RX_SEC_F | TS_F | MARK_F | RX_VLAN_F | CKSUM_F)	       \
R(sec_ts_mark_vlan_cksum_rss,	1, 1, 1, 1, 1, 0, 1,			       \
			RX_SEC_F | TS_F | MARK_F | RX_VLAN_F | CKSUM_F | RSS_F)\
R(sec_ts_mark_vlan_cksum_ptype,	1, 1, 1, 1, 1, 1, 0,			       \
			RX_SEC_F | TS_F | MARK_F | RX_VLAN_F | CKSUM_F |       \
			PTYPE_F)					       \
R(sec_ts_mark_vlan_cksum_ptype_rss,					       \
				1, 1, 1, 1, 1, 1, 1,			       \
			RX_SEC_F | TS_F | MARK_F | RX_VLAN_F | CKSUM_F |       \
			PTYPE_F | RSS_F)
#endif /* __OTX2_RX_H__ */
