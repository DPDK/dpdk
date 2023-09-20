/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <rte_branch_prediction.h>
#include <rte_event_crypto_adapter.h>
#include <rte_log.h>
#include <rte_crypto.h>
#include <rte_security.h>
#include <rte_cryptodev.h>
#include <rte_ipsec.h>
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
		if (IS_IP4_TUNNEL(sa->flags)) {
			tunnel->type =
				RTE_SECURITY_IPSEC_TUNNEL_IPV4;
			tunnel->ipv4.ttl = IPDEFTTL;

			memcpy((uint8_t *)&tunnel->ipv4.src_ip,
				(uint8_t *)&sa->src.ip.ip4, 4);

			memcpy((uint8_t *)&tunnel->ipv4.dst_ip,
				(uint8_t *)&sa->dst.ip.ip4, 4);
		} else if (IS_IP6_TUNNEL(sa->flags)) {
			tunnel->type =
				RTE_SECURITY_IPSEC_TUNNEL_IPV6;
			tunnel->ipv6.hlimit = IPDEFTTL;
			tunnel->ipv6.dscp = 0;
			tunnel->ipv6.flabel = 0;

			memcpy((uint8_t *)&tunnel->ipv6.src_addr,
				(uint8_t *)&sa->src.ip.ip6.ip6_b, 16);

			memcpy((uint8_t *)&tunnel->ipv6.dst_addr,
				(uint8_t *)&sa->dst.ip.ip6.ip6_b, 16);
		}
		/* TODO support for Transport */
	}
	ipsec->replay_win_sz = app_sa_prm.window_size;
	ipsec->options.esn = app_sa_prm.enable_esn;
	ipsec->options.udp_encap = sa->udp_encap;
	if (IS_HW_REASSEMBLY_EN(sa->flags))
		ipsec->options.ip_reassembly_en = 1;
}

static inline int
verify_crypto_xform(const struct rte_cryptodev_capabilities *capabilities,
		struct rte_crypto_sym_xform *crypto_xform)
{
	const struct rte_cryptodev_capabilities *crypto_cap;
	int j = 0;

	while ((crypto_cap = &capabilities[j++])->op != RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		if (crypto_cap->op == RTE_CRYPTO_OP_TYPE_SYMMETRIC &&
				crypto_cap->sym.xform_type == crypto_xform->type) {
			if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AEAD &&
					crypto_cap->sym.aead.algo == crypto_xform->aead.algo) {
				if (rte_cryptodev_sym_capability_check_aead(&crypto_cap->sym,
						crypto_xform->aead.key.length,
						crypto_xform->aead.digest_length,
						crypto_xform->aead.aad_length,
						crypto_xform->aead.iv.length) == 0)
					return 0;
			}
			if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
					crypto_cap->sym.cipher.algo == crypto_xform->cipher.algo) {
				if (rte_cryptodev_sym_capability_check_cipher(&crypto_cap->sym,
						crypto_xform->cipher.key.length,
						crypto_xform->cipher.iv.length) == 0)
					return 0;
			}
			if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
					crypto_cap->sym.auth.algo == crypto_xform->auth.algo) {
				if (rte_cryptodev_sym_capability_check_auth(&crypto_cap->sym,
						crypto_xform->auth.key.length,
						crypto_xform->auth.digest_length,
						crypto_xform->auth.iv.length) == 0)
					return 0;
			}
		}
	}

	return -ENOTSUP;
}

static inline int
verify_crypto_capabilities(const struct rte_cryptodev_capabilities *capabilities,
		struct rte_crypto_sym_xform *crypto_xform)
{
	if (crypto_xform->next != NULL)
		return (verify_crypto_xform(capabilities, crypto_xform) ||
		    verify_crypto_xform(capabilities, crypto_xform->next));
	else
		return verify_crypto_xform(capabilities, crypto_xform);
}

static inline int
verify_ipsec_capabilities(struct rte_security_ipsec_xform *ipsec_xform,
		const struct rte_security_capability *sec_cap)
{
	/* Verify security capabilities */

	if (ipsec_xform->options.esn == 1 && sec_cap->ipsec.options.esn == 0) {
		RTE_LOG(INFO, USER1, "ESN is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->options.udp_encap == 1 &&
	    sec_cap->ipsec.options.udp_encap == 0) {
		RTE_LOG(INFO, USER1, "UDP encapsulation is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->options.udp_ports_verify == 1 &&
	    sec_cap->ipsec.options.udp_ports_verify == 0) {
		RTE_LOG(DEBUG, USER1,
			"UDP encapsulation ports verification is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->options.copy_dscp == 1 &&
	    sec_cap->ipsec.options.copy_dscp == 0) {
		RTE_LOG(DEBUG, USER1, "Copy DSCP is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->options.copy_flabel == 1 &&
	    sec_cap->ipsec.options.copy_flabel == 0) {
		RTE_LOG(DEBUG, USER1, "Copy Flow Label is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->options.copy_df == 1 &&
	    sec_cap->ipsec.options.copy_df == 0) {
		RTE_LOG(DEBUG, USER1, "Copy DP bit is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->options.dec_ttl == 1 &&
	    sec_cap->ipsec.options.dec_ttl == 0) {
		RTE_LOG(DEBUG, USER1, "Decrement TTL is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->options.ecn == 1 && sec_cap->ipsec.options.ecn == 0) {
		RTE_LOG(DEBUG, USER1, "ECN is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->options.stats == 1 &&
	    sec_cap->ipsec.options.stats == 0) {
		RTE_LOG(DEBUG, USER1, "Stats is not supported\n");
		return -ENOTSUP;
	}

	if ((ipsec_xform->direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) &&
	    (ipsec_xform->options.iv_gen_disable == 1) &&
	    (sec_cap->ipsec.options.iv_gen_disable != 1)) {
		RTE_LOG(DEBUG, USER1, "Application provided IV is not supported\n");
		return -ENOTSUP;
	}

	if ((ipsec_xform->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) &&
	    (ipsec_xform->options.tunnel_hdr_verify >
	    sec_cap->ipsec.options.tunnel_hdr_verify)) {
		RTE_LOG(DEBUG, USER1, "Tunnel header verify is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->options.ip_csum_enable == 1 &&
	    sec_cap->ipsec.options.ip_csum_enable == 0) {
		RTE_LOG(DEBUG, USER1, "Inner IP checksum is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->options.l4_csum_enable == 1 &&
	    sec_cap->ipsec.options.l4_csum_enable == 0) {
		RTE_LOG(DEBUG, USER1, "Inner L4 checksum is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) {
		if (ipsec_xform->replay_win_sz > sec_cap->ipsec.replay_win_sz_max) {
			RTE_LOG(DEBUG, USER1, "Replay window size is not supported\n");
			return -ENOTSUP;
		}
	}

	return 0;
}


static inline int
verify_security_capabilities(void *ctx,
		struct rte_security_session_conf *sess_conf,
		uint32_t *ol_flags)
{
	struct rte_security_capability_idx sec_cap_idx;
	const struct rte_security_capability *sec_cap;

	sec_cap_idx.action = sess_conf->action_type;
	sec_cap_idx.protocol = sess_conf->protocol;
	sec_cap_idx.ipsec.proto = sess_conf->ipsec.proto;
	sec_cap_idx.ipsec.mode = sess_conf->ipsec.mode;
	sec_cap_idx.ipsec.direction = sess_conf->ipsec.direction;

	sec_cap = rte_security_capability_get(ctx, &sec_cap_idx);
	if (sec_cap == NULL)
		return -ENOTSUP;

	if (verify_crypto_capabilities(sec_cap->crypto_capabilities,
				sess_conf->crypto_xform))
		return -ENOTSUP;

	if (verify_ipsec_capabilities(&sess_conf->ipsec, sec_cap))
		return -ENOTSUP;

	if (ol_flags != NULL)
		*ol_flags = sec_cap->ol_flags;

	return 0;
}

int
create_lookaside_session(struct ipsec_ctx *ipsec_ctx_lcore[],
	struct socket_ctx *skt_ctx, const struct eventmode_conf *em_conf,
	struct ipsec_sa *sa, struct rte_ipsec_session *ips)
{
	uint16_t cdev_id = RTE_CRYPTO_MAX_DEVS;
	enum rte_crypto_op_sess_type sess_type;
	struct rte_cryptodev_info cdev_info;
	enum rte_crypto_op_type op_type;
	unsigned long cdev_id_qp = 0;
	struct ipsec_ctx *ipsec_ctx;
	struct cdev_key key = { 0 };
	void *sess = NULL;
	uint32_t lcore_id;
	int32_t ret = 0;

	RTE_LCORE_FOREACH(lcore_id) {
		ipsec_ctx = ipsec_ctx_lcore[lcore_id];

		/* Core is not bound to any cryptodev, skip it */
		if (ipsec_ctx->cdev_map == NULL)
			continue;

		/* Looking for cryptodev, which can handle this SA */
		key.lcore_id = (uint8_t)lcore_id;
		key.cipher_algo = (uint8_t)sa->cipher_algo;
		key.auth_algo = (uint8_t)sa->auth_algo;
		key.aead_algo = (uint8_t)sa->aead_algo;

		ret = rte_hash_lookup_data(ipsec_ctx->cdev_map, &key,
				(void **)&cdev_id_qp);
		if (ret == -ENOENT)
			continue;
		if (ret < 0) {
			RTE_LOG(ERR, IPSEC,
					"No cryptodev: core %u, cipher_algo %u, "
					"auth_algo %u, aead_algo %u\n",
					key.lcore_id,
					key.cipher_algo,
					key.auth_algo,
					key.aead_algo);
			return ret;
		}

		/* Verify that all cores are using same cryptodev for current
		 * algorithm combination, required by SA.
		 * Current cryptodev mapping process will map SA to the first
		 * cryptodev that matches requirements, so it's a double check,
		 * not an additional restriction.
		 */
		if (cdev_id == RTE_CRYPTO_MAX_DEVS)
			cdev_id = ipsec_ctx->tbl[cdev_id_qp].id;
		else if (cdev_id != ipsec_ctx->tbl[cdev_id_qp].id) {
			RTE_LOG(ERR, IPSEC,
					"SA mapping to multiple cryptodevs is "
					"not supported!");
			return -EINVAL;
		}

		/* Store per core queue pair information */
		sa->cqp[lcore_id] = &ipsec_ctx->tbl[cdev_id_qp];
	}
	if (cdev_id == RTE_CRYPTO_MAX_DEVS) {
		RTE_LOG(WARNING, IPSEC, "No cores found to handle SA\n");
		return 0;
	}

	RTE_LOG(DEBUG, IPSEC, "Create session for SA spi %u on cryptodev "
			"%u\n", sa->spi, cdev_id);

	if (ips->type != RTE_SECURITY_ACTION_TYPE_NONE &&
		ips->type != RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO) {
		struct rte_security_session_conf sess_conf = {
			.action_type = ips->type,
			.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
			{.ipsec = {
				.spi = sa->spi,
				.salt = sa->salt,
				.options = { 0 },
				.replay_win_sz = 0,
				.direction = sa->direction,
				.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
				.mode = (IS_TUNNEL(sa->flags)) ?
					RTE_SECURITY_IPSEC_SA_MODE_TUNNEL :
					RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT,
			} },
			.crypto_xform = sa->xforms,
			.userdata = NULL,

		};

		if (ips->type == RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL) {
			void *ctx = rte_cryptodev_get_sec_ctx(cdev_id);

			/* Set IPsec parameters in conf */
			set_ipsec_conf(sa, &(sess_conf.ipsec));

			if (verify_security_capabilities(ctx, &sess_conf, NULL)) {
				RTE_LOG(ERR, IPSEC,
					"Requested security session config not supported\n");
				return -1;
			}

			ips->security.ses = rte_security_session_create(ctx,
					&sess_conf, skt_ctx->session_pool);
			if (ips->security.ses == NULL) {
				RTE_LOG(ERR, IPSEC,
				"SEC Session init failed: err: %d\n", ret);
				return -1;
			}
			ips->security.ctx = ctx;

			sess = ips->security.ses;
			op_type = RTE_CRYPTO_OP_TYPE_SYMMETRIC;
			sess_type = RTE_CRYPTO_OP_SECURITY_SESSION;
		} else {
			RTE_LOG(ERR, IPSEC, "Inline not supported\n");
			return -1;
		}
	} else {
		struct rte_cryptodev_info info;

		rte_cryptodev_info_get(cdev_id, &info);

		if (ips->type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO) {
			if (!(info.feature_flags &
				RTE_CRYPTODEV_FF_SYM_CPU_CRYPTO))
				return -ENOTSUP;

		}

		if (verify_crypto_capabilities(info.capabilities, sa->xforms)) {
			RTE_LOG(ERR, IPSEC,
				"Requested crypto session config not supported\n");
			return -1;
		}

		ips->crypto.dev_id = cdev_id;
		ips->crypto.ses = rte_cryptodev_sym_session_create(cdev_id,
				sa->xforms, skt_ctx->session_pool);

		rte_cryptodev_info_get(cdev_id, &cdev_info);
	}

	/* Setup meta data required by event crypto adapter */
	if (em_conf->enable_event_crypto_adapter && sess != NULL) {
		union rte_event_crypto_metadata m_data;
		const struct eventdev_params *eventdev_conf;

		eventdev_conf = &(em_conf->eventdev_config[0]);
		memset(&m_data, 0, sizeof(m_data));

		/* Fill in response information */
		m_data.response_info.sched_type = em_conf->ext_params.sched_type;
		m_data.response_info.op = RTE_EVENT_OP_NEW;
		m_data.response_info.queue_id = eventdev_conf->ev_cpt_queue_id;

		/* Fill in request information */
		m_data.request_info.cdev_id = cdev_id;
		m_data.request_info.queue_pair_id = 0;

		/* Attach meta info to session */
		rte_cryptodev_session_event_mdata_set(cdev_id, sess, op_type,
				sess_type, &m_data, sizeof(m_data));
	}

	return 0;
}

int
create_inline_session(struct socket_ctx *skt_ctx, struct ipsec_sa *sa,
		struct rte_ipsec_session *ips)
{
	int32_t ret = 0;
	void *sec_ctx;
	struct rte_security_session_conf sess_conf = {
		.action_type = ips->type,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		{.ipsec = {
			.spi = sa->spi,
			.salt = sa->salt,
			.options = { 0 },
			.replay_win_sz = 0,
			.direction = sa->direction,
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP
		} },
		.crypto_xform = sa->xforms,
		.userdata = NULL,
	};

	if (IS_TRANSPORT(sa->flags)) {
		sess_conf.ipsec.mode = RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT;
		if (IS_IP4(sa->flags)) {
			sess_conf.ipsec.tunnel.type =
				RTE_SECURITY_IPSEC_TUNNEL_IPV4;

			sess_conf.ipsec.tunnel.ipv4.src_ip.s_addr =
				sa->src.ip.ip4;
			sess_conf.ipsec.tunnel.ipv4.dst_ip.s_addr =
				sa->dst.ip.ip4;
		} else if (IS_IP6(sa->flags)) {
			sess_conf.ipsec.tunnel.type =
				RTE_SECURITY_IPSEC_TUNNEL_IPV6;

			memcpy(sess_conf.ipsec.tunnel.ipv6.src_addr.s6_addr,
				sa->src.ip.ip6.ip6_b, 16);
			memcpy(sess_conf.ipsec.tunnel.ipv6.dst_addr.s6_addr,
				sa->dst.ip.ip6.ip6_b, 16);
		}
	} else if (IS_TUNNEL(sa->flags)) {
		sess_conf.ipsec.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;

		if (IS_IP4(sa->flags)) {
			sess_conf.ipsec.tunnel.type =
				RTE_SECURITY_IPSEC_TUNNEL_IPV4;

			sess_conf.ipsec.tunnel.ipv4.src_ip.s_addr =
				sa->src.ip.ip4;
			sess_conf.ipsec.tunnel.ipv4.dst_ip.s_addr =
				sa->dst.ip.ip4;
		} else if (IS_IP6(sa->flags)) {
			sess_conf.ipsec.tunnel.type =
				RTE_SECURITY_IPSEC_TUNNEL_IPV6;

			memcpy(sess_conf.ipsec.tunnel.ipv6.src_addr.s6_addr,
				sa->src.ip.ip6.ip6_b, 16);
			memcpy(sess_conf.ipsec.tunnel.ipv6.dst_addr.s6_addr,
				sa->dst.ip.ip6.ip6_b, 16);
		} else {
			RTE_LOG(ERR, IPSEC, "invalid tunnel type\n");
			return -1;
		}
	}

	if (sa->udp_encap) {
		sess_conf.ipsec.options.udp_encap = 1;
		sess_conf.ipsec.udp.sport = htons(sa->udp.sport);
		sess_conf.ipsec.udp.dport = htons(sa->udp.dport);
	}

	if (sa->esn > 0) {
		sess_conf.ipsec.options.esn = 1;
		sess_conf.ipsec.esn.value = sa->esn;
	}


	RTE_LOG_DP(DEBUG, IPSEC, "Create session for SA spi %u on port %u\n",
		sa->spi, sa->portid);

	if (ips->type == RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO) {
		struct rte_flow_error err;
		int ret = 0;

		sec_ctx = rte_eth_dev_get_sec_ctx(sa->portid);
		if (sec_ctx == NULL) {
			RTE_LOG(ERR, IPSEC,
				" rte_eth_dev_get_sec_ctx failed\n");
			return -1;
		}

		if (verify_security_capabilities(sec_ctx, &sess_conf,
					&ips->security.ol_flags)) {
			RTE_LOG(ERR, IPSEC,
				"Requested security session config not supported\n");
			return -1;
		}

		ips->security.ses = rte_security_session_create(sec_ctx,
				&sess_conf, skt_ctx->session_pool);
		if (ips->security.ses == NULL) {
			RTE_LOG(ERR, IPSEC,
				"SEC Session init failed: err: %d\n", ret);
			return -1;
		}

		ips->security.ctx = sec_ctx;
		sa->pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;

		if (IS_IP6(sa->flags)) {
			sa->pattern[1].mask = &rte_flow_item_ipv6_mask;
			sa->pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV6;
			sa->pattern[1].spec = &sa->ipv6_spec;

			memcpy(sa->ipv6_spec.hdr.dst_addr,
				sa->dst.ip.ip6.ip6_b, 16);
			memcpy(sa->ipv6_spec.hdr.src_addr,
			       sa->src.ip.ip6.ip6_b, 16);
		} else if (IS_IP4(sa->flags)) {
			sa->pattern[1].mask = &rte_flow_item_ipv4_mask;
			sa->pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
			sa->pattern[1].spec = &sa->ipv4_spec;

			sa->ipv4_spec.hdr.dst_addr = sa->dst.ip.ip4;
			sa->ipv4_spec.hdr.src_addr = sa->src.ip.ip4;
		}

		sa->esp_spec.hdr.spi = rte_cpu_to_be_32(sa->spi);

		if (sa->udp_encap) {

			sa->udp_spec.hdr.dst_port =
					rte_cpu_to_be_16(sa->udp.dport);
			sa->udp_spec.hdr.src_port =
					rte_cpu_to_be_16(sa->udp.sport);

			sa->pattern[2].mask = &rte_flow_item_udp_mask;
			sa->pattern[2].type = RTE_FLOW_ITEM_TYPE_UDP;
			sa->pattern[2].spec = &sa->udp_spec;

			sa->pattern[3].type = RTE_FLOW_ITEM_TYPE_ESP;
			sa->pattern[3].spec = &sa->esp_spec;
			sa->pattern[3].mask = &rte_flow_item_esp_mask;

			sa->pattern[4].type = RTE_FLOW_ITEM_TYPE_END;
		} else {
			sa->pattern[2].type = RTE_FLOW_ITEM_TYPE_ESP;
			sa->pattern[2].spec = &sa->esp_spec;
			sa->pattern[2].mask = &rte_flow_item_esp_mask;

			sa->pattern[3].type = RTE_FLOW_ITEM_TYPE_END;
		}

		sa->action[0].type = RTE_FLOW_ACTION_TYPE_SECURITY;
		sa->action[0].conf = ips->security.ses;

		sa->action[1].type = RTE_FLOW_ACTION_TYPE_END;

		sa->attr.egress = (sa->direction ==
				RTE_SECURITY_IPSEC_SA_DIR_EGRESS);
		sa->attr.ingress = (sa->direction ==
				RTE_SECURITY_IPSEC_SA_DIR_INGRESS);
		if (sa->attr.ingress) {
			uint8_t rss_key[64];
			struct rte_eth_rss_conf rss_conf = {
				.rss_key = rss_key,
				.rss_key_len = sizeof(rss_key),
			};
			struct rte_eth_dev_info dev_info;
			uint16_t queue[RTE_MAX_QUEUES_PER_PORT];
			struct rte_flow_action_rss action_rss;
			unsigned int i;
			unsigned int j;

			/* Don't create flow if default flow is created */
			if (flow_info_tbl[sa->portid].rx_def_flow)
				return 0;

			ret = rte_eth_dev_info_get(sa->portid, &dev_info);
			if (ret != 0) {
				RTE_LOG(ERR, IPSEC,
					"Error during getting device (port %u) info: %s\n",
					sa->portid, strerror(-ret));
				return ret;
			}

			sa->action[2].type = RTE_FLOW_ACTION_TYPE_END;
			/* Try RSS. */
			sa->action[1].type = RTE_FLOW_ACTION_TYPE_RSS;
			sa->action[1].conf = &action_rss;
			ret = rte_eth_dev_rss_hash_conf_get(sa->portid,
					&rss_conf);
			if (ret != 0) {
				RTE_LOG(ERR, IPSEC,
					"rte_eth_dev_rss_hash_conf_get:ret=%d\n",
					ret);
				return -1;
			}
			for (i = 0, j = 0; i < dev_info.nb_rx_queues; ++i)
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
				(ips->security.ol_flags &
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
	} else if (ips->type ==	RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL) {
		sec_ctx = rte_eth_dev_get_sec_ctx(sa->portid);

		if (sec_ctx == NULL) {
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

		if (verify_security_capabilities(sec_ctx, &sess_conf,
					&ips->security.ol_flags)) {
			RTE_LOG(ERR, IPSEC,
				"Requested security session config not supported\n");
			return -1;
		}

		ips->security.ses = rte_security_session_create(sec_ctx,
					&sess_conf, skt_ctx->session_pool);
		if (ips->security.ses == NULL) {
			RTE_LOG(ERR, IPSEC,
				"SEC Session init failed: err: %d\n", ret);
			return -1;
		}

		ips->security.ctx = sec_ctx;
	}

	return 0;
}

int
create_ipsec_esp_flow(struct ipsec_sa *sa)
{
	int ret = 0;
	struct rte_flow_error err = {};
	if (sa->direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
		RTE_LOG(ERR, IPSEC,
			"No Flow director rule for Egress traffic\n");
		return -1;
	}
	if (sa->flags == TRANSPORT) {
		RTE_LOG(ERR, IPSEC,
			"No Flow director rule for transport mode\n");
		return -1;
	}
	sa->action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	sa->pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	sa->action[0].conf = &(struct rte_flow_action_queue) {
				.index = sa->fdir_qid,
	};
	sa->attr.egress = 0;
	sa->attr.ingress = 1;
	if (IS_IP6(sa->flags)) {
		sa->pattern[1].mask = &rte_flow_item_ipv6_mask;
		sa->pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV6;
		sa->pattern[1].spec = &sa->ipv6_spec;
		memcpy(sa->ipv6_spec.hdr.dst_addr,
			sa->dst.ip.ip6.ip6_b, sizeof(sa->dst.ip.ip6.ip6_b));
		memcpy(sa->ipv6_spec.hdr.src_addr,
			sa->src.ip.ip6.ip6_b, sizeof(sa->src.ip.ip6.ip6_b));
		sa->pattern[2].type = RTE_FLOW_ITEM_TYPE_ESP;
		sa->pattern[2].spec = &sa->esp_spec;
		sa->pattern[2].mask = &rte_flow_item_esp_mask;
		sa->esp_spec.hdr.spi = rte_cpu_to_be_32(sa->spi);
		sa->pattern[3].type = RTE_FLOW_ITEM_TYPE_END;
	} else if (IS_IP4(sa->flags)) {
		sa->pattern[1].mask = &rte_flow_item_ipv4_mask;
		sa->pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
		sa->pattern[1].spec = &sa->ipv4_spec;
		sa->ipv4_spec.hdr.dst_addr = sa->dst.ip.ip4;
		sa->ipv4_spec.hdr.src_addr = sa->src.ip.ip4;
		sa->pattern[2].type = RTE_FLOW_ITEM_TYPE_ESP;
		sa->pattern[2].spec = &sa->esp_spec;
		sa->pattern[2].mask = &rte_flow_item_esp_mask;
		sa->esp_spec.hdr.spi = rte_cpu_to_be_32(sa->spi);
		sa->pattern[3].type = RTE_FLOW_ITEM_TYPE_END;
	}
	sa->action[1].type = RTE_FLOW_ACTION_TYPE_END;

	ret = rte_flow_validate(sa->portid, &sa->attr, sa->pattern, sa->action,
				&err);
	if (ret < 0) {
		RTE_LOG(ERR, IPSEC, "Flow validation failed %s\n", err.message);
		return ret;
	}

	sa->flow = rte_flow_create(sa->portid, &sa->attr, sa->pattern,
					sa->action, &err);
	if (!sa->flow) {
		RTE_LOG(ERR, IPSEC, "Flow creation failed %s\n", err.message);
		return -1;
	}

	return 0;
}

/*
 * queue crypto-ops into PMD queue.
 */
void
enqueue_cop_burst(struct cdev_qp *cqp)
{
	uint32_t i, len, ret;

	len = cqp->len;
	ret = rte_cryptodev_enqueue_burst(cqp->id, cqp->qp, cqp->buf, len);
	if (ret < len) {
		RTE_LOG_DP(DEBUG, IPSEC, "Cryptodev %u queue %u:"
			" enqueued %u crypto ops out of %u\n",
			cqp->id, cqp->qp, ret, len);
			/* drop packets that we fail to enqueue */
			for (i = ret; i < len; i++)
				free_pkts(&cqp->buf[i]->sym->m_src, 1);
	}
	cqp->in_flight += ret;
	cqp->len = 0;
}

static inline void
enqueue_cop(struct cdev_qp *cqp, struct rte_crypto_op *cop)
{
	cqp->buf[cqp->len++] = cop;

	if (cqp->len == MAX_PKT_BURST)
		enqueue_cop_burst(cqp);
}

static inline void
ipsec_enqueue(ipsec_xform_fn xform_func, struct ipsec_ctx *ipsec_ctx,
		struct rte_mbuf *pkts[], void *sas[],
		uint16_t nb_pkts)
{
	int32_t ret = 0, i;
	struct ipsec_mbuf_metadata *priv;
	struct rte_crypto_sym_op *sym_cop;
	struct ipsec_sa *sa;
	struct rte_ipsec_session *ips;

	for (i = 0; i < nb_pkts; i++) {
		if (unlikely(sas[i] == NULL)) {
			free_pkts(&pkts[i], 1);
			continue;
		}

		rte_prefetch0(sas[i]);
		rte_prefetch0(pkts[i]);

		priv = get_priv(pkts[i]);
		sa = ipsec_mask_saptr(sas[i]);
		priv->sa = sa;
		ips = ipsec_get_primary_session(sa);

		switch (ips->type) {
		case RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL:
			priv->cop.type = RTE_CRYPTO_OP_TYPE_SYMMETRIC;
			priv->cop.status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;

			rte_prefetch0(&priv->sym_cop);

			if (unlikely(ips->security.ses == NULL)) {
				free_pkts(&pkts[i], 1);
				continue;
			}

			if (unlikely((pkts[i]->packet_type &
					(RTE_PTYPE_TUNNEL_MASK |
					RTE_PTYPE_L4_MASK)) ==
					MBUF_PTYPE_TUNNEL_ESP_IN_UDP &&
					sa->udp_encap != 1)) {
				free_pkts(&pkts[i], 1);
				continue;
			}

			sym_cop = get_sym_cop(&priv->cop);
			sym_cop->m_src = pkts[i];

			rte_security_attach_session(&priv->cop,
				ips->security.ses);
			break;

		case RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO:
			RTE_LOG(ERR, IPSEC, "CPU crypto is not supported by the"
					" legacy mode.");
			free_pkts(&pkts[i], 1);
			continue;

		case RTE_SECURITY_ACTION_TYPE_NONE:

			priv->cop.type = RTE_CRYPTO_OP_TYPE_SYMMETRIC;
			priv->cop.status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;

			rte_prefetch0(&priv->sym_cop);

			if (unlikely(ips->crypto.ses == NULL)) {
				free_pkts(&pkts[i], 1);
				continue;
			}

			rte_crypto_op_attach_sym_session(&priv->cop,
					ips->crypto.ses);

			ret = xform_func(pkts[i], sa, &priv->cop);
			if (unlikely(ret)) {
				free_pkts(&pkts[i], 1);
				continue;
			}
			break;
		case RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL:
			RTE_ASSERT(ips->security.ses != NULL);
			ipsec_ctx->ol_pkts[ipsec_ctx->ol_pkts_cnt++] = pkts[i];
			if (ips->security.ol_flags &
				RTE_SECURITY_TX_OLOAD_NEED_MDATA)
				rte_security_set_pkt_metadata(
					ips->security.ctx, ips->security.ses,
					pkts[i], NULL);
			continue;
		case RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO:
			RTE_ASSERT(ips->security.ses != NULL);
			priv->cop.type = RTE_CRYPTO_OP_TYPE_SYMMETRIC;
			priv->cop.status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;

			rte_prefetch0(&priv->sym_cop);
			rte_security_attach_session(&priv->cop,
					ips->security.ses);

			ret = xform_func(pkts[i], sa, &priv->cop);
			if (unlikely(ret)) {
				free_pkts(&pkts[i], 1);
				continue;
			}

			ipsec_ctx->ol_pkts[ipsec_ctx->ol_pkts_cnt++] = pkts[i];
			if (ips->security.ol_flags &
				RTE_SECURITY_TX_OLOAD_NEED_MDATA)
				rte_security_set_pkt_metadata(
					ips->security.ctx, ips->security.ses,
					pkts[i], NULL);
			continue;
		}

		enqueue_cop(sa->cqp[ipsec_ctx->lcore_id], &priv->cop);
	}
}

static inline int32_t
ipsec_inline_dequeue(ipsec_xform_fn xform_func, struct ipsec_ctx *ipsec_ctx,
	      struct rte_mbuf *pkts[], uint16_t max_pkts)
{
	int32_t nb_pkts, ret;
	struct ipsec_mbuf_metadata *priv;
	struct ipsec_sa *sa;
	struct rte_mbuf *pkt;

	nb_pkts = 0;
	while (ipsec_ctx->ol_pkts_cnt > 0 && nb_pkts < max_pkts) {
		pkt = ipsec_ctx->ol_pkts[--ipsec_ctx->ol_pkts_cnt];
		rte_prefetch0(pkt);
		priv = get_priv(pkt);
		sa = priv->sa;
		ret = xform_func(pkt, sa, &priv->cop);
		if (unlikely(ret)) {
			free_pkts(&pkt, 1);
			continue;
		}
		pkts[nb_pkts++] = pkt;
	}

	return nb_pkts;
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

			if (ipsec_get_action_type(sa) ==
				RTE_SECURITY_ACTION_TYPE_NONE) {
				ret = xform_func(pkt, sa, cops[j]);
				if (unlikely(ret)) {
					free_pkts(&pkt, 1);
					continue;
				}
			} else if (ipsec_get_action_type(sa) ==
				RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL) {
				if (cops[j]->status) {
					free_pkts(&pkt, 1);
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
	void *sas[nb_pkts];

	inbound_sa_lookup(ctx->sa_ctx, pkts, sas, nb_pkts);

	ipsec_enqueue(esp_inbound, ctx, pkts, sas, nb_pkts);

	return ipsec_inline_dequeue(esp_inbound_post, ctx, pkts, len);
}

uint16_t
ipsec_inbound_cqp_dequeue(struct ipsec_ctx *ctx, struct rte_mbuf *pkts[],
		uint16_t len)
{
	return ipsec_dequeue(esp_inbound_post, ctx, pkts, len);
}

uint16_t
ipsec_outbound(struct ipsec_ctx *ctx, struct rte_mbuf *pkts[],
		uint32_t sa_idx[], uint16_t nb_pkts, uint16_t len)
{
	void *sas[nb_pkts];

	outbound_sa_lookup(ctx->sa_ctx, sa_idx, sas, nb_pkts);

	ipsec_enqueue(esp_outbound, ctx, pkts, sas, nb_pkts);

	return ipsec_inline_dequeue(esp_outbound_post, ctx, pkts, len);
}

uint16_t
ipsec_outbound_cqp_dequeue(struct ipsec_ctx *ctx, struct rte_mbuf *pkts[],
		uint16_t len)
{
	return ipsec_dequeue(esp_outbound_post, ctx, pkts, len);
}
