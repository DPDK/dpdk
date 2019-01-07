/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright 2014 6WIND S.A.
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

#include <rte_compat.h>
#include <rte_debug.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_mbuf_pool_ops.h>
#include <rte_string_fns.h>
#include <rte_hexdump.h>
#include <rte_errno.h>
#include <rte_memcpy.h>

/*
 * pktmbuf pool constructor, given as a callback function to
 * rte_mempool_create(), or called directly if using
 * rte_mempool_create_empty()/rte_mempool_populate()
 */
void
rte_pktmbuf_pool_init(struct rte_mempool *mp, void *opaque_arg)
{
	struct rte_pktmbuf_pool_private *user_mbp_priv, *mbp_priv;
	struct rte_pktmbuf_pool_private default_mbp_priv;
	uint16_t roomsz;

	RTE_ASSERT(mp->elt_size >= sizeof(struct rte_mbuf));

	/* if no structure is provided, assume no mbuf private area */
	user_mbp_priv = opaque_arg;
	if (user_mbp_priv == NULL) {
		default_mbp_priv.mbuf_priv_size = 0;
		if (mp->elt_size > sizeof(struct rte_mbuf))
			roomsz = mp->elt_size - sizeof(struct rte_mbuf);
		else
			roomsz = 0;
		default_mbp_priv.mbuf_data_room_size = roomsz;
		user_mbp_priv = &default_mbp_priv;
	}

	RTE_ASSERT(mp->elt_size >= sizeof(struct rte_mbuf) +
		user_mbp_priv->mbuf_data_room_size +
		user_mbp_priv->mbuf_priv_size);

	mbp_priv = rte_mempool_get_priv(mp);
	memcpy(mbp_priv, user_mbp_priv, sizeof(*mbp_priv));
}

/*
 * pktmbuf constructor, given as a callback function to
 * rte_mempool_obj_iter() or rte_mempool_create().
 * Set the fields of a packet mbuf to their default values.
 */
void
rte_pktmbuf_init(struct rte_mempool *mp,
		 __attribute__((unused)) void *opaque_arg,
		 void *_m,
		 __attribute__((unused)) unsigned i)
{
	struct rte_mbuf *m = _m;
	uint32_t mbuf_size, buf_len, priv_size;

	priv_size = rte_pktmbuf_priv_size(mp);
	mbuf_size = sizeof(struct rte_mbuf) + priv_size;
	buf_len = rte_pktmbuf_data_room_size(mp);

	RTE_ASSERT(RTE_ALIGN(priv_size, RTE_MBUF_PRIV_ALIGN) == priv_size);
	RTE_ASSERT(mp->elt_size >= mbuf_size);
	RTE_ASSERT(buf_len <= UINT16_MAX);

	memset(m, 0, mbuf_size);
	/* start of buffer is after mbuf structure and priv data */
	m->priv_size = priv_size;
	m->buf_addr = (char *)m + mbuf_size;
	m->buf_iova = rte_mempool_virt2iova(m) + mbuf_size;
	m->buf_len = (uint16_t)buf_len;

	/* keep some headroom between start of buffer and data */
	m->data_off = RTE_MIN(RTE_PKTMBUF_HEADROOM, (uint16_t)m->buf_len);

	/* init some constant fields */
	m->pool = mp;
	m->nb_segs = 1;
	m->port = MBUF_INVALID_PORT;
	rte_mbuf_refcnt_set(m, 1);
	m->next = NULL;
}

/* Helper to create a mbuf pool with given mempool ops name*/
struct rte_mempool *
rte_pktmbuf_pool_create_by_ops(const char *name, unsigned int n,
	unsigned int cache_size, uint16_t priv_size, uint16_t data_room_size,
	int socket_id, const char *ops_name)
{
	struct rte_mempool *mp;
	struct rte_pktmbuf_pool_private mbp_priv;
	const char *mp_ops_name = ops_name;
	unsigned elt_size;
	int ret;

	if (RTE_ALIGN(priv_size, RTE_MBUF_PRIV_ALIGN) != priv_size) {
		RTE_LOG(ERR, MBUF, "mbuf priv_size=%u is not aligned\n",
			priv_size);
		rte_errno = EINVAL;
		return NULL;
	}
	elt_size = sizeof(struct rte_mbuf) + (unsigned)priv_size +
		(unsigned)data_room_size;
	mbp_priv.mbuf_data_room_size = data_room_size;
	mbp_priv.mbuf_priv_size = priv_size;

	mp = rte_mempool_create_empty(name, n, elt_size, cache_size,
		 sizeof(struct rte_pktmbuf_pool_private), socket_id, 0);
	if (mp == NULL)
		return NULL;

	if (mp_ops_name == NULL)
		mp_ops_name = rte_mbuf_best_mempool_ops();
	ret = rte_mempool_set_ops_byname(mp, mp_ops_name, NULL);
	if (ret != 0) {
		RTE_LOG(ERR, MBUF, "error setting mempool handler\n");
		rte_mempool_free(mp);
		rte_errno = -ret;
		return NULL;
	}
	rte_pktmbuf_pool_init(mp, &mbp_priv);

	ret = rte_mempool_populate_default(mp);
	if (ret < 0) {
		rte_mempool_free(mp);
		rte_errno = -ret;
		return NULL;
	}

	rte_mempool_obj_iter(mp, rte_pktmbuf_init, NULL);

	return mp;
}

/* helper to create a mbuf pool */
struct rte_mempool *
rte_pktmbuf_pool_create(const char *name, unsigned int n,
	unsigned int cache_size, uint16_t priv_size, uint16_t data_room_size,
	int socket_id)
{
	return rte_pktmbuf_pool_create_by_ops(name, n, cache_size, priv_size,
			data_room_size, socket_id, NULL);
}

/* do some sanity checks on a mbuf: panic if it fails */
void
rte_mbuf_sanity_check(const struct rte_mbuf *m, int is_header)
{
	const char *reason;

	if (rte_mbuf_check(m, is_header, &reason))
		rte_panic("%s\n", reason);
}

__rte_experimental
int rte_mbuf_check(const struct rte_mbuf *m, int is_header,
		   const char **reason)
{
	unsigned int nb_segs, pkt_len;

	if (m == NULL) {
		*reason = "mbuf is NULL";
		return -1;
	}

	/* generic checks */
	if (m->pool == NULL) {
		*reason = "bad mbuf pool";
		return -1;
	}
	if (m->buf_iova == 0) {
		*reason = "bad IO addr";
		return -1;
	}
	if (m->buf_addr == NULL) {
		*reason = "bad virt addr";
		return -1;
	}

	uint16_t cnt = rte_mbuf_refcnt_read(m);
	if ((cnt == 0) || (cnt == UINT16_MAX)) {
		*reason = "bad ref cnt";
		return -1;
	}

	/* nothing to check for sub-segments */
	if (is_header == 0)
		return 0;

	/* data_len is supposed to be not more than pkt_len */
	if (m->data_len > m->pkt_len) {
		*reason = "bad data_len";
		return -1;
	}

	nb_segs = m->nb_segs;
	pkt_len = m->pkt_len;

	do {
		if (m->data_off > m->buf_len) {
			*reason = "data offset too big in mbuf segment";
			return -1;
		}
		if (m->data_off + m->data_len > m->buf_len) {
			*reason = "data length too big in mbuf segment";
			return -1;
		}
		nb_segs -= 1;
		pkt_len -= m->data_len;
	} while ((m = m->next) != NULL);

	if (nb_segs) {
		*reason = "bad nb_segs";
		return -1;
	}
	if (pkt_len) {
		*reason = "bad pkt_len";
		return -1;
	}

	return 0;
}

/* dump a mbuf on console */
void
rte_pktmbuf_dump(FILE *f, const struct rte_mbuf *m, unsigned dump_len)
{
	unsigned int len;
	unsigned int nb_segs;

	__rte_mbuf_sanity_check(m, 1);

	fprintf(f, "dump mbuf at %p, iova=%"PRIx64", buf_len=%u\n",
	       m, (uint64_t)m->buf_iova, (unsigned)m->buf_len);
	fprintf(f, "  pkt_len=%"PRIu32", ol_flags=%"PRIx64", nb_segs=%u, "
	       "in_port=%u\n", m->pkt_len, m->ol_flags,
	       (unsigned)m->nb_segs, (unsigned)m->port);
	nb_segs = m->nb_segs;

	while (m && nb_segs != 0) {
		__rte_mbuf_sanity_check(m, 0);

		fprintf(f, "  segment at %p, data=%p, data_len=%u\n",
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

/* read len data bytes in a mbuf at specified offset (internal) */
const void *__rte_pktmbuf_read(const struct rte_mbuf *m, uint32_t off,
	uint32_t len, void *buf)
{
	const struct rte_mbuf *seg = m;
	uint32_t buf_off = 0, copy_len;

	if (off + len > rte_pktmbuf_pkt_len(m))
		return NULL;

	while (off >= rte_pktmbuf_data_len(seg)) {
		off -= rte_pktmbuf_data_len(seg);
		seg = seg->next;
	}

	if (off + len <= rte_pktmbuf_data_len(seg))
		return rte_pktmbuf_mtod_offset(seg, char *, off);

	/* rare case: header is split among several segments */
	while (len > 0) {
		copy_len = rte_pktmbuf_data_len(seg) - off;
		if (copy_len > len)
			copy_len = len;
		rte_memcpy((char *)buf + buf_off,
			rte_pktmbuf_mtod_offset(seg, char *, off), copy_len);
		off = 0;
		buf_off += copy_len;
		len -= copy_len;
		seg = seg->next;
	}

	return buf;
}

/*
 * Get the name of a RX offload flag. Must be kept synchronized with flag
 * definitions in rte_mbuf.h.
 */
const char *rte_get_rx_ol_flag_name(uint64_t mask)
{
	switch (mask) {
	case PKT_RX_VLAN: return "PKT_RX_VLAN";
	case PKT_RX_RSS_HASH: return "PKT_RX_RSS_HASH";
	case PKT_RX_FDIR: return "PKT_RX_FDIR";
	case PKT_RX_L4_CKSUM_BAD: return "PKT_RX_L4_CKSUM_BAD";
	case PKT_RX_L4_CKSUM_GOOD: return "PKT_RX_L4_CKSUM_GOOD";
	case PKT_RX_L4_CKSUM_NONE: return "PKT_RX_L4_CKSUM_NONE";
	case PKT_RX_IP_CKSUM_BAD: return "PKT_RX_IP_CKSUM_BAD";
	case PKT_RX_IP_CKSUM_GOOD: return "PKT_RX_IP_CKSUM_GOOD";
	case PKT_RX_IP_CKSUM_NONE: return "PKT_RX_IP_CKSUM_NONE";
	case PKT_RX_EIP_CKSUM_BAD: return "PKT_RX_EIP_CKSUM_BAD";
	case PKT_RX_VLAN_STRIPPED: return "PKT_RX_VLAN_STRIPPED";
	case PKT_RX_IEEE1588_PTP: return "PKT_RX_IEEE1588_PTP";
	case PKT_RX_IEEE1588_TMST: return "PKT_RX_IEEE1588_TMST";
	case PKT_RX_FDIR_ID: return "PKT_RX_FDIR_ID";
	case PKT_RX_FDIR_FLX: return "PKT_RX_FDIR_FLX";
	case PKT_RX_QINQ_STRIPPED: return "PKT_RX_QINQ_STRIPPED";
	case PKT_RX_QINQ: return "PKT_RX_QINQ";
	case PKT_RX_LRO: return "PKT_RX_LRO";
	case PKT_RX_TIMESTAMP: return "PKT_RX_TIMESTAMP";
	case PKT_RX_SEC_OFFLOAD: return "PKT_RX_SEC_OFFLOAD";
	case PKT_RX_SEC_OFFLOAD_FAILED: return "PKT_RX_SEC_OFFLOAD_FAILED";
	case PKT_RX_OUTER_L4_CKSUM_BAD: return "PKT_RX_OUTER_L4_CKSUM_BAD";
	case PKT_RX_OUTER_L4_CKSUM_GOOD: return "PKT_RX_OUTER_L4_CKSUM_GOOD";
	case PKT_RX_OUTER_L4_CKSUM_INVALID:
		return "PKT_RX_OUTER_L4_CKSUM_INVALID";

	default: return NULL;
	}
}

struct flag_mask {
	uint64_t flag;
	uint64_t mask;
	const char *default_name;
};

/* write the list of rx ol flags in buffer buf */
int
rte_get_rx_ol_flag_list(uint64_t mask, char *buf, size_t buflen)
{
	const struct flag_mask rx_flags[] = {
		{ PKT_RX_VLAN, PKT_RX_VLAN, NULL },
		{ PKT_RX_RSS_HASH, PKT_RX_RSS_HASH, NULL },
		{ PKT_RX_FDIR, PKT_RX_FDIR, NULL },
		{ PKT_RX_L4_CKSUM_BAD, PKT_RX_L4_CKSUM_MASK, NULL },
		{ PKT_RX_L4_CKSUM_GOOD, PKT_RX_L4_CKSUM_MASK, NULL },
		{ PKT_RX_L4_CKSUM_NONE, PKT_RX_L4_CKSUM_MASK, NULL },
		{ PKT_RX_L4_CKSUM_UNKNOWN, PKT_RX_L4_CKSUM_MASK,
		  "PKT_RX_L4_CKSUM_UNKNOWN" },
		{ PKT_RX_IP_CKSUM_BAD, PKT_RX_IP_CKSUM_MASK, NULL },
		{ PKT_RX_IP_CKSUM_GOOD, PKT_RX_IP_CKSUM_MASK, NULL },
		{ PKT_RX_IP_CKSUM_NONE, PKT_RX_IP_CKSUM_MASK, NULL },
		{ PKT_RX_IP_CKSUM_UNKNOWN, PKT_RX_IP_CKSUM_MASK,
		  "PKT_RX_IP_CKSUM_UNKNOWN" },
		{ PKT_RX_EIP_CKSUM_BAD, PKT_RX_EIP_CKSUM_BAD, NULL },
		{ PKT_RX_VLAN_STRIPPED, PKT_RX_VLAN_STRIPPED, NULL },
		{ PKT_RX_IEEE1588_PTP, PKT_RX_IEEE1588_PTP, NULL },
		{ PKT_RX_IEEE1588_TMST, PKT_RX_IEEE1588_TMST, NULL },
		{ PKT_RX_FDIR_ID, PKT_RX_FDIR_ID, NULL },
		{ PKT_RX_FDIR_FLX, PKT_RX_FDIR_FLX, NULL },
		{ PKT_RX_QINQ_STRIPPED, PKT_RX_QINQ_STRIPPED, NULL },
		{ PKT_RX_LRO, PKT_RX_LRO, NULL },
		{ PKT_RX_TIMESTAMP, PKT_RX_TIMESTAMP, NULL },
		{ PKT_RX_SEC_OFFLOAD, PKT_RX_SEC_OFFLOAD, NULL },
		{ PKT_RX_SEC_OFFLOAD_FAILED, PKT_RX_SEC_OFFLOAD_FAILED, NULL },
		{ PKT_RX_QINQ, PKT_RX_QINQ, NULL },
		{ PKT_RX_OUTER_L4_CKSUM_BAD, PKT_RX_OUTER_L4_CKSUM_MASK, NULL },
		{ PKT_RX_OUTER_L4_CKSUM_GOOD, PKT_RX_OUTER_L4_CKSUM_MASK,
		  NULL },
		{ PKT_RX_OUTER_L4_CKSUM_INVALID, PKT_RX_OUTER_L4_CKSUM_MASK,
		  NULL },
		{ PKT_RX_OUTER_L4_CKSUM_UNKNOWN, PKT_RX_OUTER_L4_CKSUM_MASK,
		  "PKT_RX_OUTER_L4_CKSUM_UNKNOWN" },
	};
	const char *name;
	unsigned int i;
	int ret;

	if (buflen == 0)
		return -1;

	buf[0] = '\0';
	for (i = 0; i < RTE_DIM(rx_flags); i++) {
		if ((mask & rx_flags[i].mask) != rx_flags[i].flag)
			continue;
		name = rte_get_rx_ol_flag_name(rx_flags[i].flag);
		if (name == NULL)
			name = rx_flags[i].default_name;
		ret = snprintf(buf, buflen, "%s ", name);
		if (ret < 0)
			return -1;
		if ((size_t)ret >= buflen)
			return -1;
		buf += ret;
		buflen -= ret;
	}

	return 0;
}

/*
 * Get the name of a TX offload flag. Must be kept synchronized with flag
 * definitions in rte_mbuf.h.
 */
const char *rte_get_tx_ol_flag_name(uint64_t mask)
{
	switch (mask) {
	case PKT_TX_VLAN: return "PKT_TX_VLAN";
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
	case PKT_TX_TUNNEL_VXLAN: return "PKT_TX_TUNNEL_VXLAN";
	case PKT_TX_TUNNEL_GRE: return "PKT_TX_TUNNEL_GRE";
	case PKT_TX_TUNNEL_IPIP: return "PKT_TX_TUNNEL_IPIP";
	case PKT_TX_TUNNEL_GENEVE: return "PKT_TX_TUNNEL_GENEVE";
	case PKT_TX_TUNNEL_MPLSINUDP: return "PKT_TX_TUNNEL_MPLSINUDP";
	case PKT_TX_TUNNEL_VXLAN_GPE: return "PKT_TX_TUNNEL_VXLAN_GPE";
	case PKT_TX_TUNNEL_IP: return "PKT_TX_TUNNEL_IP";
	case PKT_TX_TUNNEL_UDP: return "PKT_TX_TUNNEL_UDP";
	case PKT_TX_QINQ: return "PKT_TX_QINQ";
	case PKT_TX_MACSEC: return "PKT_TX_MACSEC";
	case PKT_TX_SEC_OFFLOAD: return "PKT_TX_SEC_OFFLOAD";
	case PKT_TX_UDP_SEG: return "PKT_TX_UDP_SEG";
	case PKT_TX_OUTER_UDP_CKSUM: return "PKT_TX_OUTER_UDP_CKSUM";
	case PKT_TX_METADATA: return "PKT_TX_METADATA";
	default: return NULL;
	}
}

/* write the list of tx ol flags in buffer buf */
int
rte_get_tx_ol_flag_list(uint64_t mask, char *buf, size_t buflen)
{
	const struct flag_mask tx_flags[] = {
		{ PKT_TX_VLAN, PKT_TX_VLAN, NULL },
		{ PKT_TX_IP_CKSUM, PKT_TX_IP_CKSUM, NULL },
		{ PKT_TX_TCP_CKSUM, PKT_TX_L4_MASK, NULL },
		{ PKT_TX_SCTP_CKSUM, PKT_TX_L4_MASK, NULL },
		{ PKT_TX_UDP_CKSUM, PKT_TX_L4_MASK, NULL },
		{ PKT_TX_L4_NO_CKSUM, PKT_TX_L4_MASK, "PKT_TX_L4_NO_CKSUM" },
		{ PKT_TX_IEEE1588_TMST, PKT_TX_IEEE1588_TMST, NULL },
		{ PKT_TX_TCP_SEG, PKT_TX_TCP_SEG, NULL },
		{ PKT_TX_IPV4, PKT_TX_IPV4, NULL },
		{ PKT_TX_IPV6, PKT_TX_IPV6, NULL },
		{ PKT_TX_OUTER_IP_CKSUM, PKT_TX_OUTER_IP_CKSUM, NULL },
		{ PKT_TX_OUTER_IPV4, PKT_TX_OUTER_IPV4, NULL },
		{ PKT_TX_OUTER_IPV6, PKT_TX_OUTER_IPV6, NULL },
		{ PKT_TX_TUNNEL_VXLAN, PKT_TX_TUNNEL_MASK, NULL },
		{ PKT_TX_TUNNEL_GRE, PKT_TX_TUNNEL_MASK, NULL },
		{ PKT_TX_TUNNEL_IPIP, PKT_TX_TUNNEL_MASK, NULL },
		{ PKT_TX_TUNNEL_GENEVE, PKT_TX_TUNNEL_MASK, NULL },
		{ PKT_TX_TUNNEL_MPLSINUDP, PKT_TX_TUNNEL_MASK, NULL },
		{ PKT_TX_TUNNEL_VXLAN_GPE, PKT_TX_TUNNEL_MASK, NULL },
		{ PKT_TX_TUNNEL_IP, PKT_TX_TUNNEL_MASK, NULL },
		{ PKT_TX_TUNNEL_UDP, PKT_TX_TUNNEL_MASK, NULL },
		{ PKT_TX_QINQ, PKT_TX_QINQ, NULL },
		{ PKT_TX_MACSEC, PKT_TX_MACSEC, NULL },
		{ PKT_TX_SEC_OFFLOAD, PKT_TX_SEC_OFFLOAD, NULL },
		{ PKT_TX_UDP_SEG, PKT_TX_UDP_SEG, NULL },
		{ PKT_TX_OUTER_UDP_CKSUM, PKT_TX_OUTER_UDP_CKSUM, NULL },
		{ PKT_TX_METADATA, PKT_TX_METADATA, NULL },
	};
	const char *name;
	unsigned int i;
	int ret;

	if (buflen == 0)
		return -1;

	buf[0] = '\0';
	for (i = 0; i < RTE_DIM(tx_flags); i++) {
		if ((mask & tx_flags[i].mask) != tx_flags[i].flag)
			continue;
		name = rte_get_tx_ol_flag_name(tx_flags[i].flag);
		if (name == NULL)
			name = tx_flags[i].default_name;
		ret = snprintf(buf, buflen, "%s ", name);
		if (ret < 0)
			return -1;
		if ((size_t)ret >= buflen)
			return -1;
		buf += ret;
		buflen -= ret;
	}

	return 0;
}
