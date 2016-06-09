/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
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
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <rte_branch_prediction.h>
#include <rte_log.h>
#include <rte_crypto.h>
#include <rte_cryptodev.h>
#include <rte_mbuf.h>
#include <rte_hash.h>

#include "ipsec.h"
#include "esp.h"

static inline int
create_session(struct ipsec_ctx *ipsec_ctx __rte_unused, struct ipsec_sa *sa)
{
	unsigned long cdev_id_qp = 0;
	int32_t ret;
	struct cdev_key key = { 0 };

	key.lcore_id = (uint8_t)rte_lcore_id();

	key.cipher_algo = (uint8_t)sa->cipher_algo;
	key.auth_algo = (uint8_t)sa->auth_algo;

	ret = rte_hash_lookup_data(ipsec_ctx->cdev_map, &key,
			(void **)&cdev_id_qp);
	if (ret < 0) {
		RTE_LOG(ERR, IPSEC, "No cryptodev: core %u, cipher_algo %u, "
				"auth_algo %u\n", key.lcore_id, key.cipher_algo,
				key.auth_algo);
		return -1;
	}

	RTE_LOG(DEBUG, IPSEC, "Create session for SA spi %u on cryptodev "
			"%u qp %u\n", sa->spi,
			ipsec_ctx->tbl[cdev_id_qp].id,
			ipsec_ctx->tbl[cdev_id_qp].qp);

	sa->crypto_session = rte_cryptodev_sym_session_create(
			ipsec_ctx->tbl[cdev_id_qp].id, sa->xforms);

	sa->cdev_id_qp = cdev_id_qp;

	return 0;
}

static inline void
enqueue_cop(struct cdev_qp *cqp, struct rte_crypto_op *cop)
{
	int32_t ret, i;

	cqp->buf[cqp->len++] = cop;

	if (cqp->len == MAX_PKT_BURST) {
		ret = rte_cryptodev_enqueue_burst(cqp->id, cqp->qp,
				cqp->buf, cqp->len);
		if (ret < cqp->len) {
			RTE_LOG(DEBUG, IPSEC, "Cryptodev %u queue %u:"
					" enqueued %u crypto ops out of %u\n",
					 cqp->id, cqp->qp,
					 ret, cqp->len);
			for (i = ret; i < cqp->len; i++)
				rte_pktmbuf_free(cqp->buf[i]->sym->m_src);
		}
		cqp->in_flight += ret;
		cqp->len = 0;
	}
}

static inline void
ipsec_enqueue(ipsec_xform_fn xform_func, struct ipsec_ctx *ipsec_ctx,
		struct rte_mbuf *pkts[], struct ipsec_sa *sas[],
		uint16_t nb_pkts)
{
	int32_t ret = 0, i;
	struct ipsec_mbuf_metadata *priv;
	struct ipsec_sa *sa;

	for (i = 0; i < nb_pkts; i++) {
		if (unlikely(sas[i] == NULL)) {
			rte_pktmbuf_free(pkts[i]);
			continue;
		}

		rte_prefetch0(sas[i]);
		rte_prefetch0(pkts[i]);

		priv = get_priv(pkts[i]);
		sa = sas[i];
		priv->sa = sa;

		priv->cop.type = RTE_CRYPTO_OP_TYPE_SYMMETRIC;

		rte_prefetch0(&priv->sym_cop);
		priv->cop.sym = &priv->sym_cop;

		if ((unlikely(sa->crypto_session == NULL)) &&
				create_session(ipsec_ctx, sa)) {
			rte_pktmbuf_free(pkts[i]);
			continue;
		}

		rte_crypto_op_attach_sym_session(&priv->cop,
				sa->crypto_session);

		ret = xform_func(pkts[i], sa, &priv->cop);
		if (unlikely(ret)) {
			rte_pktmbuf_free(pkts[i]);
			continue;
		}

		RTE_ASSERT(sa->cdev_id_qp < ipsec_ctx->nb_qps);
		enqueue_cop(&ipsec_ctx->tbl[sa->cdev_id_qp], &priv->cop);
	}
}

static inline int
ipsec_dequeue(ipsec_xform_fn xform_func, struct ipsec_ctx *ipsec_ctx,
		struct rte_mbuf *pkts[], uint16_t max_pkts)
{
	int32_t nb_pkts = 0, ret = 0, i, j, nb_cops;
	struct ipsec_mbuf_metadata *priv;
	struct rte_crypto_op *cops[max_pkts];
	struct ipsec_sa *sa;
	struct rte_mbuf *pkt;

	for (i = 0; i < ipsec_ctx->nb_qps && nb_pkts < max_pkts; i++) {
		struct cdev_qp *cqp;

		cqp = &ipsec_ctx->tbl[ipsec_ctx->last_qp++];
		if (ipsec_ctx->last_qp == ipsec_ctx->nb_qps)
			ipsec_ctx->last_qp %= ipsec_ctx->nb_qps;

		if (cqp->in_flight == 0)
			continue;

		nb_cops = rte_cryptodev_dequeue_burst(cqp->id, cqp->qp,
				cops, max_pkts - nb_pkts);

		cqp->in_flight -= nb_cops;

		for (j = 0; j < nb_cops; j++) {
			pkt = cops[j]->sym->m_src;
			rte_prefetch0(pkt);

			priv = get_priv(pkt);
			sa = priv->sa;

			RTE_ASSERT(sa != NULL);

			ret = xform_func(pkt, sa, cops[j]);
			if (unlikely(ret))
				rte_pktmbuf_free(pkt);
			else
				pkts[nb_pkts++] = pkt;
		}
	}

	/* return packets */
	return nb_pkts;
}

uint16_t
ipsec_inbound(struct ipsec_ctx *ctx, struct rte_mbuf *pkts[],
		uint16_t nb_pkts, uint16_t len)
{
	struct ipsec_sa *sas[nb_pkts];

	inbound_sa_lookup(ctx->sa_ctx, pkts, sas, nb_pkts);

	ipsec_enqueue(esp_inbound, ctx, pkts, sas, nb_pkts);

	return ipsec_dequeue(esp_inbound_post, ctx, pkts, len);
}

uint16_t
ipsec_outbound(struct ipsec_ctx *ctx, struct rte_mbuf *pkts[],
		uint32_t sa_idx[], uint16_t nb_pkts, uint16_t len)
{
	struct ipsec_sa *sas[nb_pkts];

	outbound_sa_lookup(ctx->sa_ctx, sa_idx, sas, nb_pkts);

	ipsec_enqueue(esp_outbound, ctx, pkts, sas, nb_pkts);

	return ipsec_dequeue(esp_outbound_post, ctx, pkts, len);
}
