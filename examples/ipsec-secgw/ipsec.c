/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <rte_branch_prediction.h>
#include <rte_log.h>
#include <rte_crypto.h>
#include <rte_security.h>
#include <rte_cryptodev.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_hash.h>

#include "ipsec.h"
#include "esp.h"

static inline void
set_ipsec_conf(struct ipsec_sa *sa, struct rte_security_ipsec_xform *ipsec)
{
	if (ipsec->mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL) {
		struct rte_security_ipsec_tunnel_param *tunnel =
				&ipsec->tunnel;
		if (sa->flags == IP4_TUNNEL) {
			tunnel->type =
				RTE_SECURITY_IPSEC_TUNNEL_IPV4;
			tunnel->ipv4.ttl = IPDEFTTL;

			memcpy((uint8_t *)&tunnel->ipv4.src_ip,
				(uint8_t *)&sa->src.ip.ip4, 4);

			memcpy((uint8_t *)&tunnel->ipv4.dst_ip,
				(uint8_t *)&sa->dst.ip.ip4, 4);
		}
		/* TODO support for Transport and IPV6 tunnel */
	}
	ipsec->esn_soft_limit = IPSEC_OFFLOAD_ESN_SOFTLIMIT;
}

static inline int
create_session(struct ipsec_ctx *ipsec_ctx, struct ipsec_sa *sa)
{
	struct rte_cryptodev_info cdev_info;
	unsigned long cdev_id_qp = 0;
	int32_t ret = 0;
	struct cdev_key key = { 0 };

	key.lcore_id = (uint8_t)rte_lcore_id();

	key.cipher_algo = (uint8_t)sa->cipher_algo;
	key.auth_algo = (uint8_t)sa->auth_algo;
	key.aead_algo = (uint8_t)sa->aead_algo;

	if (sa->type == RTE_SECURITY_ACTION_TYPE_NONE) {
		ret = rte_hash_lookup_data(ipsec_ctx->cdev_map, &key,
				(void **)&cdev_id_qp);
		if (ret < 0) {
			RTE_LOG(ERR, IPSEC,
				"No cryptodev: core %u, cipher_algo %u, "
				"auth_algo %u, aead_algo %u\n",
				key.lcore_id,
				key.cipher_algo,
				key.auth_algo,
				key.aead_algo);
			return -1;
		}
	}

	RTE_LOG_DP(DEBUG, IPSEC, "Create session for SA spi %u on cryptodev "
			"%u qp %u\n", sa->spi,
			ipsec_ctx->tbl[cdev_id_qp].id,
			ipsec_ctx->tbl[cdev_id_qp].qp);

	if (sa->type != RTE_SECURITY_ACTION_TYPE_NONE) {
		struct rte_security_session_conf sess_conf = {
			.action_type = sa->type,
			.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
			{.ipsec = {
				.spi = sa->spi,
				.salt = sa->salt,
				.options = { 0 },
				.direction = sa->direction,
				.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
				.mode = (sa->flags == IP4_TUNNEL ||
						sa->flags == IP6_TUNNEL) ?
					RTE_SECURITY_IPSEC_SA_MODE_TUNNEL :
					RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT,
			} },
			.crypto_xform = sa->xforms,
			.userdata = NULL,

		};

		if (sa->type == RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL) {
			struct rte_security_ctx *ctx = (struct rte_security_ctx *)
							rte_cryptodev_get_sec_ctx(
							ipsec_ctx->tbl[cdev_id_qp].id);

			/* Set IPsec parameters in conf */
			set_ipsec_conf(sa, &(sess_conf.ipsec));

			sa->sec_session = rte_security_session_create(ctx,
					&sess_conf, ipsec_ctx->session_pool);
			if (sa->sec_session == NULL) {
				RTE_LOG(ERR, IPSEC,
				"SEC Session init failed: err: %d\n", ret);
				return -1;
			}
		} else if (sa->type == RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO) {
			struct rte_flow_error err;
			struct rte_security_ctx *ctx = (struct rte_security_ctx *)
							rte_eth_dev_get_sec_ctx(
							sa->portid);
			const struct rte_security_capability *sec_cap;
			int ret = 0;

			sa->sec_session = rte_security_session_create(ctx,
					&sess_conf, ipsec_ctx->session_pool);
			if (sa->sec_session == NULL) {
				RTE_LOG(ERR, IPSEC,
				"SEC Session init failed: err: %d\n", ret);
				return -1;
			}

			sec_cap = rte_security_capabilities_get(ctx);

			/* iterate until ESP tunnel*/
			while (sec_cap->action !=
					RTE_SECURITY_ACTION_TYPE_NONE) {

				if (sec_cap->action == sa->type &&
				    sec_cap->protocol ==
					RTE_SECURITY_PROTOCOL_IPSEC &&
				    sec_cap->ipsec.mode ==
					RTE_SECURITY_IPSEC_SA_MODE_TUNNEL &&
				    sec_cap->ipsec.direction == sa->direction)
					break;
				sec_cap++;
			}

			if (sec_cap->action == RTE_SECURITY_ACTION_TYPE_NONE) {
				RTE_LOG(ERR, IPSEC,
				"No suitable security capability found\n");
				return -1;
			}

			sa->ol_flags = sec_cap->ol_flags;
			sa->security_ctx = ctx;
			sa->pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;

			sa->pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
			sa->pattern[1].mask = &rte_flow_item_ipv4_mask;
			if (sa->flags & IP6_TUNNEL) {
				sa->pattern[1].spec = &sa->ipv6_spec;
				memcpy(sa->ipv6_spec.hdr.dst_addr,
					sa->dst.ip.ip6.ip6_b, 16);
				memcpy(sa->ipv6_spec.hdr.src_addr,
				       sa->src.ip.ip6.ip6_b, 16);
			} else {
				sa->pattern[1].spec = &sa->ipv4_spec;
				sa->ipv4_spec.hdr.dst_addr = sa->dst.ip.ip4;
				sa->ipv4_spec.hdr.src_addr = sa->src.ip.ip4;
			}

			sa->pattern[2].type = RTE_FLOW_ITEM_TYPE_ESP;
			sa->pattern[2].spec = &sa->esp_spec;
			sa->pattern[2].mask = &rte_flow_item_esp_mask;
			sa->esp_spec.hdr.spi = rte_cpu_to_be_32(sa->spi);

			sa->pattern[3].type = RTE_FLOW_ITEM_TYPE_END;

			sa->action[0].type = RTE_FLOW_ACTION_TYPE_SECURITY;
			sa->action[0].conf = sa->sec_session;

			sa->action[1].type = RTE_FLOW_ACTION_TYPE_END;

			sa->attr.egress = (sa->direction ==
					RTE_SECURITY_IPSEC_SA_DIR_EGRESS);
			sa->attr.ingress = (sa->direction ==
					RTE_SECURITY_IPSEC_SA_DIR_INGRESS);
			if (sa->attr.ingress) {
				uint8_t rss_key[40];
				struct rte_eth_rss_conf rss_conf = {
					.rss_key = rss_key,
					.rss_key_len = 40,
				};
				struct rte_eth_dev *eth_dev;
				uint16_t queue[RTE_MAX_QUEUES_PER_PORT];
				struct rte_flow_action_rss action_rss;
				unsigned int i;
				unsigned int j;

				sa->action[2].type = RTE_FLOW_ACTION_TYPE_END;
				/* Try RSS. */
				sa->action[1].type = RTE_FLOW_ACTION_TYPE_RSS;
				sa->action[1].conf = &action_rss;
				eth_dev = ctx->device;
				rte_eth_dev_rss_hash_conf_get(sa->portid,
							      &rss_conf);
				for (i = 0, j = 0;
				     i < eth_dev->data->nb_rx_queues; ++i)
					if (eth_dev->data->rx_queues[i])
						queue[j++] = i;
				action_rss = (struct rte_flow_action_rss){
					.types = rss_conf.rss_hf,
					.key_len = rss_conf.rss_key_len,
					.queue_num = j,
					.key = rss_key,
					.queue = queue,
				};
				ret = rte_flow_validate(sa->portid, &sa->attr,
							sa->pattern, sa->action,
							&err);
				if (!ret)
					goto flow_create;
				/* Try Queue. */
				sa->action[1].type = RTE_FLOW_ACTION_TYPE_QUEUE;
				sa->action[1].conf =
					&(struct rte_flow_action_queue){
					.index = 0,
				};
				ret = rte_flow_validate(sa->portid, &sa->attr,
							sa->pattern, sa->action,
							&err);
				/* Try End. */
				sa->action[1].type = RTE_FLOW_ACTION_TYPE_END;
				sa->action[1].conf = NULL;
				ret = rte_flow_validate(sa->portid, &sa->attr,
							sa->pattern, sa->action,
							&err);
				if (ret)
					goto flow_create_failure;
			} else if (sa->attr.egress &&
				   (sa->ol_flags &
				    RTE_SECURITY_TX_HW_TRAILER_OFFLOAD)) {
				sa->action[1].type =
					RTE_FLOW_ACTION_TYPE_PASSTHRU;
				sa->action[2].type =
					RTE_FLOW_ACTION_TYPE_END;
			}
flow_create:
			sa->flow = rte_flow_create(sa->portid,
				&sa->attr, sa->pattern, sa->action, &err);
			if (sa->flow == NULL) {
flow_create_failure:
				RTE_LOG(ERR, IPSEC,
					"Failed to create ipsec flow msg: %s\n",
					err.message);
				return -1;
			}
		} else if (sa->type ==
				RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL) {
			struct rte_security_ctx *ctx =
					(struct rte_security_ctx *)
					rte_eth_dev_get_sec_ctx(sa->portid);
			const struct rte_security_capability *sec_cap;

			if (ctx == NULL) {
				RTE_LOG(ERR, IPSEC,
				"Ethernet device doesn't have security features registered\n");
				return -1;
			}

			/* Set IPsec parameters in conf */
			set_ipsec_conf(sa, &(sess_conf.ipsec));

			/* Save SA as userdata for the security session. When
			 * the packet is received, this userdata will be
			 * retrieved using the metadata from the packet.
			 *
			 * The PMD is expected to set similar metadata for other
			 * operations, like rte_eth_event, which are tied to
			 * security session. In such cases, the userdata could
			 * be obtained to uniquely identify the security
			 * parameters denoted.
			 */

			sess_conf.userdata = (void *) sa;

			sa->sec_session = rte_security_session_create(ctx,
					&sess_conf, ipsec_ctx->session_pool);
			if (sa->sec_session == NULL) {
				RTE_LOG(ERR, IPSEC,
				"SEC Session init failed: err: %d\n", ret);
				return -1;
			}

			sec_cap = rte_security_capabilities_get(ctx);

			if (sec_cap == NULL) {
				RTE_LOG(ERR, IPSEC,
				"No capabilities registered\n");
				return -1;
			}

			/* iterate until ESP tunnel*/
			while (sec_cap->action !=
					RTE_SECURITY_ACTION_TYPE_NONE) {

				if (sec_cap->action == sa->type &&
				    sec_cap->protocol ==
					RTE_SECURITY_PROTOCOL_IPSEC &&
				    sec_cap->ipsec.mode ==
					RTE_SECURITY_IPSEC_SA_MODE_TUNNEL &&
				    sec_cap->ipsec.direction == sa->direction)
					break;
				sec_cap++;
			}

			if (sec_cap->action == RTE_SECURITY_ACTION_TYPE_NONE) {
				RTE_LOG(ERR, IPSEC,
				"No suitable security capability found\n");
				return -1;
			}

			sa->ol_flags = sec_cap->ol_flags;
			sa->security_ctx = ctx;
		}
	} else {
		sa->crypto_session = rte_cryptodev_sym_session_create(
				ipsec_ctx->session_pool);
		rte_cryptodev_sym_session_init(ipsec_ctx->tbl[cdev_id_qp].id,
				sa->crypto_session, sa->xforms,
				ipsec_ctx->session_pool);

		rte_cryptodev_info_get(ipsec_ctx->tbl[cdev_id_qp].id,
				&cdev_info);
	}
	sa->cdev_id_qp = cdev_id_qp;

	return 0;
}

static inline void
enqueue_cop(struct cdev_qp *cqp, struct rte_crypto_op *cop)
{
	int32_t ret = 0, i;

	cqp->buf[cqp->len++] = cop;

	if (cqp->len == MAX_PKT_BURST) {
		int enq_size = cqp->len;
		if ((cqp->in_flight + enq_size) > MAX_INFLIGHT)
			enq_size -=
			    (int)((cqp->in_flight + enq_size) - MAX_INFLIGHT);

		if (enq_size > 0)
			ret = rte_cryptodev_enqueue_burst(cqp->id, cqp->qp,
					cqp->buf, enq_size);
		if (ret < cqp->len) {
			RTE_LOG_DP(DEBUG, IPSEC, "Cryptodev %u queue %u:"
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
	struct rte_crypto_sym_op *sym_cop;
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

		switch (sa->type) {
		case RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL:
			priv->cop.type = RTE_CRYPTO_OP_TYPE_SYMMETRIC;
			priv->cop.status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;

			rte_prefetch0(&priv->sym_cop);

			if ((unlikely(sa->sec_session == NULL)) &&
					create_session(ipsec_ctx, sa)) {
				rte_pktmbuf_free(pkts[i]);
				continue;
			}

			sym_cop = get_sym_cop(&priv->cop);
			sym_cop->m_src = pkts[i];

			rte_security_attach_session(&priv->cop,
					sa->sec_session);
			break;
		case RTE_SECURITY_ACTION_TYPE_NONE:

			priv->cop.type = RTE_CRYPTO_OP_TYPE_SYMMETRIC;
			priv->cop.status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;

			rte_prefetch0(&priv->sym_cop);

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
			break;
		case RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL:
			if ((unlikely(sa->sec_session == NULL)) &&
					create_session(ipsec_ctx, sa)) {
				rte_pktmbuf_free(pkts[i]);
				continue;
			}

			ipsec_ctx->ol_pkts[ipsec_ctx->ol_pkts_cnt++] = pkts[i];
			if (sa->ol_flags & RTE_SECURITY_TX_OLOAD_NEED_MDATA)
				rte_security_set_pkt_metadata(
						sa->security_ctx,
						sa->sec_session, pkts[i], NULL);
			continue;
		case RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO:
			priv->cop.type = RTE_CRYPTO_OP_TYPE_SYMMETRIC;
			priv->cop.status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;

			rte_prefetch0(&priv->sym_cop);

			if ((unlikely(sa->sec_session == NULL)) &&
					create_session(ipsec_ctx, sa)) {
				rte_pktmbuf_free(pkts[i]);
				continue;
			}

			rte_security_attach_session(&priv->cop,
					sa->sec_session);

			ret = xform_func(pkts[i], sa, &priv->cop);
			if (unlikely(ret)) {
				rte_pktmbuf_free(pkts[i]);
				continue;
			}

			ipsec_ctx->ol_pkts[ipsec_ctx->ol_pkts_cnt++] = pkts[i];
			if (sa->ol_flags & RTE_SECURITY_TX_OLOAD_NEED_MDATA)
				rte_security_set_pkt_metadata(
						sa->security_ctx,
						sa->sec_session, pkts[i], NULL);
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

		while (ipsec_ctx->ol_pkts_cnt > 0 && nb_pkts < max_pkts) {
			pkt = ipsec_ctx->ol_pkts[--ipsec_ctx->ol_pkts_cnt];
			rte_prefetch0(pkt);
			priv = get_priv(pkt);
			sa = priv->sa;
			ret = xform_func(pkt, sa, &priv->cop);
			if (unlikely(ret)) {
				rte_pktmbuf_free(pkt);
				continue;
			}
			pkts[nb_pkts++] = pkt;
		}

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

			if (sa->type == RTE_SECURITY_ACTION_TYPE_NONE) {
				ret = xform_func(pkt, sa, cops[j]);
				if (unlikely(ret)) {
					rte_pktmbuf_free(pkt);
					continue;
				}
			}
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
