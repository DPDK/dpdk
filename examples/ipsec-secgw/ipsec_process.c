/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <rte_branch_prediction.h>
#include <rte_log.h>
#include <rte_cryptodev.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>

#include "ipsec.h"

#define SATP_OUT_IPV4(t)	\
	((((t) & RTE_IPSEC_SATP_MODE_MASK) == RTE_IPSEC_SATP_MODE_TRANS && \
	(((t) & RTE_IPSEC_SATP_IPV_MASK) == RTE_IPSEC_SATP_IPV4)) || \
	((t) & RTE_IPSEC_SATP_MODE_MASK) == RTE_IPSEC_SATP_MODE_TUNLV4)


/* helper routine to free bulk of packets */
static inline void
free_pkts(struct rte_mbuf *mb[], uint32_t n)
{
	uint32_t i;

	for (i = 0; i != n; i++)
		rte_pktmbuf_free(mb[i]);
}

/* helper routine to free bulk of crypto-ops and related packets */
static inline void
free_cops(struct rte_crypto_op *cop[], uint32_t n)
{
	uint32_t i;

	for (i = 0; i != n; i++)
		rte_pktmbuf_free(cop[i]->sym->m_src);
}

/* helper routine to enqueue bulk of crypto ops */
static inline void
enqueue_cop_bulk(struct cdev_qp *cqp, struct rte_crypto_op *cop[], uint32_t num)
{
	uint32_t i, k, len, n;

	len = cqp->len;

	/*
	 * if cqp is empty and we have enough ops,
	 * then queue them to the PMD straightway.
	 */
	if (num >= RTE_DIM(cqp->buf) * 3 / 4 && len == 0) {
		n = rte_cryptodev_enqueue_burst(cqp->id, cqp->qp, cop, num);
		cqp->in_flight += n;
		free_cops(cop + n, num - n);
		return;
	}

	k = 0;

	do {
		n = RTE_DIM(cqp->buf) - len;
		n = RTE_MIN(num - k, n);

		/* put packets into cqp */
		for (i = 0; i != n; i++)
			cqp->buf[len + i] = cop[k + i];

		len += n;
		k += n;

		/* if cqp is full then, enqueue crypto-ops to PMD */
		if (len == RTE_DIM(cqp->buf)) {
			n = rte_cryptodev_enqueue_burst(cqp->id, cqp->qp,
					cqp->buf, len);
			cqp->in_flight += n;
			free_cops(cqp->buf + n, len - n);
			len = 0;
		}


	} while (k != num);

	cqp->len = len;
}

static inline int
fill_ipsec_session(struct rte_ipsec_session *ss, struct ipsec_ctx *ctx,
	struct ipsec_sa *sa)
{
	int32_t rc;

	/* setup crypto section */
	if (ss->type == RTE_SECURITY_ACTION_TYPE_NONE) {
		if (sa->crypto_session == NULL) {
			rc = create_session(ctx, sa);
			if (rc != 0)
				return rc;
		}
		ss->crypto.ses = sa->crypto_session;
	/* setup session action type */
	} else {
		if (sa->sec_session == NULL) {
			rc = create_session(ctx, sa);
			if (rc != 0)
				return rc;
		}
		ss->security.ses = sa->sec_session;
		ss->security.ctx = sa->security_ctx;
		ss->security.ol_flags = sa->ol_flags;
	}

	rc = rte_ipsec_session_prepare(ss);
	if (rc != 0)
		memset(ss, 0, sizeof(*ss));

	return rc;
}

/*
 * group input packets byt the SA they belong to.
 */
static uint32_t
sa_group(struct ipsec_sa *sa_ptr[], struct rte_mbuf *pkts[],
	struct rte_ipsec_group grp[], uint32_t num)
{
	uint32_t i, n, spi;
	void *sa;
	void * const nosa = &spi;

	sa = nosa;
	for (i = 0, n = 0; i != num; i++) {

		if (sa != sa_ptr[i]) {
			grp[n].cnt = pkts + i - grp[n].m;
			n += (sa != nosa);
			grp[n].id.ptr = sa_ptr[i];
			grp[n].m = pkts + i;
			sa = sa_ptr[i];
		}
	}

	/* terminate last group */
	if (sa != nosa) {
		grp[n].cnt = pkts + i - grp[n].m;
		n++;
	}

	return n;
}

/*
 * helper function, splits processed packets into ipv4/ipv6 traffic.
 */
static inline void
copy_to_trf(struct ipsec_traffic *trf, uint64_t satp, struct rte_mbuf *mb[],
	uint32_t num)
{
	uint32_t j, ofs, s;
	struct traffic_type *out;

	/*
	 * determine traffic type(ipv4/ipv6) and offset for ACL classify
	 * based on SA type
	 */
	if ((satp & RTE_IPSEC_SATP_DIR_MASK) == RTE_IPSEC_SATP_DIR_IB) {
		if ((satp & RTE_IPSEC_SATP_IPV_MASK) == RTE_IPSEC_SATP_IPV4) {
			out = &trf->ip4;
			ofs = offsetof(struct ip, ip_p);
		} else {
			out = &trf->ip6;
			ofs = offsetof(struct ip6_hdr, ip6_nxt);
		}
	} else if (SATP_OUT_IPV4(satp)) {
		out = &trf->ip4;
		ofs = offsetof(struct ip, ip_p);
	} else {
		out = &trf->ip6;
		ofs = offsetof(struct ip6_hdr, ip6_nxt);
	}

	for (j = 0, s = out->num; j != num; j++) {
		out->data[s + j] = rte_pktmbuf_mtod_offset(mb[j],
				void *, ofs);
		out->pkts[s + j] = mb[j];
	}

	out->num += num;
}

/*
 * Process ipsec packets.
 * If packet belong to SA that is subject of inline-crypto,
 * then process it immediately.
 * Otherwise do necessary preparations and queue it to related
 * crypto-dev queue.
 */
void
ipsec_process(struct ipsec_ctx *ctx, struct ipsec_traffic *trf)
{
	uint64_t satp;
	uint32_t i, j, k, n;
	struct ipsec_sa *sa;
	struct ipsec_mbuf_metadata *priv;
	struct rte_ipsec_group *pg;
	struct rte_ipsec_session *ips;
	struct cdev_qp *cqp;
	struct rte_crypto_op *cop[RTE_DIM(trf->ipsec.pkts)];
	struct rte_ipsec_group grp[RTE_DIM(trf->ipsec.pkts)];

	n = sa_group(trf->ipsec.saptr, trf->ipsec.pkts, grp, trf->ipsec.num);

	for (i = 0; i != n; i++) {

		pg = grp + i;
		sa = pg->id.ptr;

		ips = &sa->ips;

		/* no valid HW session for that SA, try to create one */
		if (sa == NULL || (ips->crypto.ses == NULL &&
				fill_ipsec_session(ips, ctx, sa) != 0))
			k = 0;

		/* process packets inline */
		else if (sa->type == RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO ||
				sa->type ==
				RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL) {

			satp = rte_ipsec_sa_type(ips->sa);

			/*
			 * This is just to satisfy inbound_sa_check()
			 * and get_hop_for_offload_pkt().
			 * Should be removed in future.
			 */
			for (j = 0; j != pg->cnt; j++) {
				priv = get_priv(pg->m[j]);
				priv->sa = sa;
			}

			k = rte_ipsec_pkt_process(ips, pg->m, pg->cnt);
			copy_to_trf(trf, satp, pg->m, k);

		/* enqueue packets to crypto dev */
		} else {

			cqp = &ctx->tbl[sa->cdev_id_qp];

			/* for that app each mbuf has it's own crypto op */
			for (j = 0; j != pg->cnt; j++) {
				priv = get_priv(pg->m[j]);
				cop[j] = &priv->cop;
				/*
				 * this is just to satisfy inbound_sa_check()
				 * should be removed in future.
				 */
				priv->sa = sa;
			}

			/* prepare and enqueue crypto ops */
			k = rte_ipsec_pkt_crypto_prepare(ips, pg->m, cop,
				pg->cnt);
			if (k != 0)
				enqueue_cop_bulk(cqp, cop, k);
		}

		/* drop packets that cannot be enqueued/processed */
		if (k != pg->cnt)
			free_pkts(pg->m + k, pg->cnt - k);
	}
}

static inline uint32_t
cqp_dequeue(struct cdev_qp *cqp, struct rte_crypto_op *cop[], uint32_t num)
{
	uint32_t n;

	if (cqp->in_flight == 0)
		return 0;

	n = rte_cryptodev_dequeue_burst(cqp->id, cqp->qp, cop, num);
	RTE_ASSERT(cqp->in_flight >= n);
	cqp->in_flight -= n;

	return n;
}

static inline uint32_t
ctx_dequeue(struct ipsec_ctx *ctx, struct rte_crypto_op *cop[], uint32_t num)
{
	uint32_t i, n;

	n = 0;

	for (i = ctx->last_qp; n != num && i != ctx->nb_qps; i++)
		n += cqp_dequeue(ctx->tbl + i, cop + n, num - n);

	for (i = 0; n != num && i != ctx->last_qp; i++)
		n += cqp_dequeue(ctx->tbl + i, cop + n, num - n);

	ctx->last_qp = i;
	return n;
}

/*
 * dequeue packets from crypto-queues and finalize processing.
 */
void
ipsec_cqp_process(struct ipsec_ctx *ctx, struct ipsec_traffic *trf)
{
	uint64_t satp;
	uint32_t i, k, n, ng;
	struct rte_ipsec_session *ss;
	struct traffic_type *out;
	struct rte_ipsec_group *pg;
	struct rte_crypto_op *cop[RTE_DIM(trf->ipsec.pkts)];
	struct rte_ipsec_group grp[RTE_DIM(trf->ipsec.pkts)];

	trf->ip4.num = 0;
	trf->ip6.num = 0;

	out = &trf->ipsec;

	/* dequeue completed crypto-ops */
	n = ctx_dequeue(ctx, cop, RTE_DIM(cop));
	if (n == 0)
		return;

	/* group them by ipsec session */
	ng = rte_ipsec_pkt_crypto_group((const struct rte_crypto_op **)
		(uintptr_t)cop, out->pkts, grp, n);

	/* process each group of packets */
	for (i = 0; i != ng; i++) {

		pg = grp + i;
		ss = pg->id.ptr;
		satp = rte_ipsec_sa_type(ss->sa);

		k = rte_ipsec_pkt_process(ss, pg->m, pg->cnt);
		copy_to_trf(trf, satp, pg->m, k);

		/* free bad packets, if any */
		free_pkts(pg->m + k, pg->cnt - k);

		n -= pg->cnt;
	}

	/* we should never have packet with unknown SA here */
	RTE_VERIFY(n == 0);
}
