/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <rte_cryptodev.h>
#include <rte_eventdev.h>
#include <rte_security.h>
#include <rte_security_driver.h>

#include <cn10k_ethdev.h>
#include <cnxk_security.h>

static struct rte_cryptodev_capabilities cn10k_eth_sec_crypto_caps[] = {
	{	/* AES GCM */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AEAD,
			{.aead = {
				.algo = RTE_CRYPTO_AEAD_AES_GCM,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.digest_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.aad_size = {
					.min = 8,
					.max = 12,
					.increment = 4
				},
				.iv_size = {
					.min = 12,
					.max = 12,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* AES CBC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_AES_CBC,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.iv_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* SHA1 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 20,
					.max = 64,
					.increment = 1
				},
				.digest_size = {
					.min = 12,
					.max = 12,
					.increment = 0
				},
			}, }
		}, }
	},
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static const struct rte_security_capability cn10k_eth_sec_capabilities[] = {
	{	/* IPsec Inline Protocol ESP Tunnel Ingress */
		.action = RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
			.options = { 0 }
		},
		.crypto_capabilities = cn10k_eth_sec_crypto_caps,
		.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
	},
	{	/* IPsec Inline Protocol ESP Tunnel Egress */
		.action = RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
			.options = { 0 }
		},
		.crypto_capabilities = cn10k_eth_sec_crypto_caps,
		.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
	},
	{	/* IPsec Inline Protocol ESP Transport Egress */
		.action = RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
			.options = { 0 }
		},
		.crypto_capabilities = cn10k_eth_sec_crypto_caps,
		.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
	},
	{	/* IPsec Inline Protocol ESP Transport Ingress */
		.action = RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
			.options = { 0 }
		},
		.crypto_capabilities = cn10k_eth_sec_crypto_caps,
		.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
	},
	{
		.action = RTE_SECURITY_ACTION_TYPE_NONE
	}
};

static inline void
cnxk_pktmbuf_free_no_cache(struct rte_mbuf *mbuf)
{
	struct rte_mbuf *next;

	if (!mbuf)
		return;
	do {
		next = mbuf->next;
		roc_npa_aura_op_free(mbuf->pool->pool_id, 1, (rte_iova_t)mbuf);
		mbuf = next;
	} while (mbuf != NULL);
}

void
cn10k_eth_sec_sso_work_cb(uint64_t *gw, void *args)
{
	struct rte_eth_event_ipsec_desc desc;
	struct cn10k_sec_sess_priv sess_priv;
	struct cn10k_outb_priv_data *priv;
	struct roc_ot_ipsec_outb_sa *sa;
	struct cpt_cn10k_res_s *res;
	struct rte_eth_dev *eth_dev;
	struct cnxk_eth_dev *dev;
	static uint64_t warn_cnt;
	uint16_t dlen_adj, rlen;
	struct rte_mbuf *mbuf;
	uintptr_t sa_base;
	uintptr_t nixtx;
	uint8_t port;

	RTE_SET_USED(args);

	switch ((gw[0] >> 28) & 0xF) {
	case RTE_EVENT_TYPE_ETHDEV:
		/* Event from inbound inline dev due to IPSEC packet bad L4 */
		mbuf = (struct rte_mbuf *)(gw[1] - sizeof(struct rte_mbuf));
		plt_nix_dbg("Received mbuf %p from inline dev inbound", mbuf);
		cnxk_pktmbuf_free_no_cache(mbuf);
		return;
	case RTE_EVENT_TYPE_CPU:
		/* Check for subtype */
		if (((gw[0] >> 20) & 0xFF) == CNXK_ETHDEV_SEC_OUTB_EV_SUB) {
			/* Event from outbound inline error */
			mbuf = (struct rte_mbuf *)gw[1];
			break;
		}
		/* Fall through */
	default:
		plt_err("Unknown event gw[0] = 0x%016lx, gw[1] = 0x%016lx",
			gw[0], gw[1]);
		return;
	}

	/* Get ethdev port from tag */
	port = gw[0] & 0xFF;
	eth_dev = &rte_eth_devices[port];
	dev = cnxk_eth_pmd_priv(eth_dev);

	sess_priv.u64 = *rte_security_dynfield(mbuf);
	/* Calculate dlen adj */
	dlen_adj = mbuf->pkt_len - mbuf->l2_len;
	rlen = (dlen_adj + sess_priv.roundup_len) +
	       (sess_priv.roundup_byte - 1);
	rlen &= ~(uint64_t)(sess_priv.roundup_byte - 1);
	rlen += sess_priv.partial_len;
	dlen_adj = rlen - dlen_adj;

	/* Find the res area residing on next cacheline after end of data */
	nixtx = rte_pktmbuf_mtod(mbuf, uintptr_t) + mbuf->pkt_len + dlen_adj;
	nixtx += BIT_ULL(7);
	nixtx = (nixtx - 1) & ~(BIT_ULL(7) - 1);
	res = (struct cpt_cn10k_res_s *)nixtx;

	plt_nix_dbg("Outbound error, mbuf %p, sa_index %u, compcode %x uc %x",
		    mbuf, sess_priv.sa_idx, res->compcode, res->uc_compcode);

	sess_priv.u64 = *rte_security_dynfield(mbuf);

	sa_base = dev->outb.sa_base;
	sa = roc_nix_inl_ot_ipsec_outb_sa(sa_base, sess_priv.sa_idx);
	priv = roc_nix_inl_ot_ipsec_outb_sa_sw_rsvd(sa);

	memset(&desc, 0, sizeof(desc));

	switch (res->uc_compcode) {
	case ROC_IE_OT_UCC_ERR_SA_OVERFLOW:
		desc.subtype = RTE_ETH_EVENT_IPSEC_ESN_OVERFLOW;
		break;
	case ROC_IE_OT_UCC_ERR_PKT_IP:
		warn_cnt++;
		if (warn_cnt % 10000 == 0)
			plt_warn("Outbound error, bad ip pkt, mbuf %p,"
				 " sa_index %u (total warnings %" PRIu64 ")",
				 mbuf, sess_priv.sa_idx, warn_cnt);
		desc.subtype = RTE_ETH_EVENT_IPSEC_UNKNOWN;
		break;
	default:
		warn_cnt++;
		if (warn_cnt % 10000 == 0)
			plt_warn("Outbound error, mbuf %p, sa_index %u,"
				 " compcode %x uc %x,"
				 " (total warnings %" PRIu64 ")",
				 mbuf, sess_priv.sa_idx, res->compcode,
				 res->uc_compcode, warn_cnt);
		desc.subtype = RTE_ETH_EVENT_IPSEC_UNKNOWN;
		break;
	}

	desc.metadata = (uint64_t)priv->userdata;
	rte_eth_dev_callback_process(eth_dev, RTE_ETH_EVENT_IPSEC, &desc);
	cnxk_pktmbuf_free_no_cache(mbuf);
}

static int
cn10k_eth_sec_session_create(void *device,
			     struct rte_security_session_conf *conf,
			     struct rte_security_session *sess,
			     struct rte_mempool *mempool)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)device;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct rte_security_ipsec_xform *ipsec;
	struct cn10k_sec_sess_priv sess_priv;
	struct rte_crypto_sym_xform *crypto;
	struct cnxk_eth_sec_sess *eth_sec;
	bool inbound, inl_dev;
	int rc = 0;

	if (conf->action_type != RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL)
		return -ENOTSUP;

	if (conf->protocol != RTE_SECURITY_PROTOCOL_IPSEC)
		return -ENOTSUP;

	if (rte_security_dynfield_register() < 0)
		return -ENOTSUP;

	ipsec = &conf->ipsec;
	crypto = conf->crypto_xform;
	inbound = !!(ipsec->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS);
	inl_dev = !!dev->inb.inl_dev;

	/* Search if a session already exits */
	if (cnxk_eth_sec_sess_get_by_spi(dev, ipsec->spi, inbound)) {
		plt_err("%s SA with SPI %u already in use",
			inbound ? "Inbound" : "Outbound", ipsec->spi);
		return -EEXIST;
	}

	if (rte_mempool_get(mempool, (void **)&eth_sec)) {
		plt_err("Could not allocate security session private data");
		return -ENOMEM;
	}

	memset(eth_sec, 0, sizeof(struct cnxk_eth_sec_sess));
	sess_priv.u64 = 0;

	/* Acquire lock on inline dev for inbound */
	if (inbound && inl_dev)
		roc_nix_inl_dev_lock();

	if (inbound) {
		struct roc_ot_ipsec_inb_sa *inb_sa, *inb_sa_dptr;
		struct cn10k_inb_priv_data *inb_priv;
		uintptr_t sa;

		PLT_STATIC_ASSERT(sizeof(struct cn10k_inb_priv_data) <
				  ROC_NIX_INL_OT_IPSEC_INB_SW_RSVD);

		/* Get Inbound SA from NIX_RX_IPSEC_SA_BASE */
		sa = roc_nix_inl_inb_sa_get(&dev->nix, inl_dev, ipsec->spi);
		if (!sa && dev->inb.inl_dev) {
			plt_err("Failed to create ingress sa, inline dev "
				"not found or spi not in range");
			rc = -ENOTSUP;
			goto mempool_put;
		} else if (!sa) {
			plt_err("Failed to create ingress sa");
			rc = -EFAULT;
			goto mempool_put;
		}

		inb_sa = (struct roc_ot_ipsec_inb_sa *)sa;

		/* Check if SA is already in use */
		if (inb_sa->w2.s.valid) {
			plt_err("Inbound SA with SPI %u already in use",
				ipsec->spi);
			rc = -EBUSY;
			goto mempool_put;
		}

		inb_sa_dptr = (struct roc_ot_ipsec_inb_sa *)dev->inb.sa_dptr;
		memset(inb_sa_dptr, 0, sizeof(struct roc_ot_ipsec_inb_sa));

		/* Fill inbound sa params */
		rc = cnxk_ot_ipsec_inb_sa_fill(inb_sa_dptr, ipsec, crypto);
		if (rc) {
			plt_err("Failed to init inbound sa, rc=%d", rc);
			goto mempool_put;
		}

		inb_priv = roc_nix_inl_ot_ipsec_inb_sa_sw_rsvd(inb_sa);
		/* Back pointer to get eth_sec */
		inb_priv->eth_sec = eth_sec;
		/* Save userdata in inb private area */
		inb_priv->userdata = conf->userdata;

		/* Save SA index/SPI in cookie for now */
		inb_sa_dptr->w1.s.cookie = rte_cpu_to_be_32(ipsec->spi);

		/* Prepare session priv */
		sess_priv.inb_sa = 1;
		sess_priv.sa_idx = ipsec->spi;

		/* Pointer from eth_sec -> inb_sa */
		eth_sec->sa = inb_sa;
		eth_sec->sess = sess;
		eth_sec->sa_idx = ipsec->spi;
		eth_sec->spi = ipsec->spi;
		eth_sec->inl_dev = !!dev->inb.inl_dev;
		eth_sec->inb = true;

		TAILQ_INSERT_TAIL(&dev->inb.list, eth_sec, entry);
		dev->inb.nb_sess++;
		/* Sync session in context cache */
		rc = roc_nix_inl_ctx_write(&dev->nix, inb_sa_dptr, eth_sec->sa,
					   eth_sec->inb,
					   sizeof(struct roc_ot_ipsec_inb_sa));
		if (rc)
			goto mempool_put;
	} else {
		struct roc_ot_ipsec_outb_sa *outb_sa, *outb_sa_dptr;
		struct cn10k_outb_priv_data *outb_priv;
		struct cnxk_ipsec_outb_rlens *rlens;
		uint64_t sa_base = dev->outb.sa_base;
		uint32_t sa_idx;

		PLT_STATIC_ASSERT(sizeof(struct cn10k_outb_priv_data) <
				  ROC_NIX_INL_OT_IPSEC_OUTB_SW_RSVD);

		/* Alloc an sa index */
		rc = cnxk_eth_outb_sa_idx_get(dev, &sa_idx);
		if (rc)
			goto mempool_put;

		outb_sa = roc_nix_inl_ot_ipsec_outb_sa(sa_base, sa_idx);
		outb_priv = roc_nix_inl_ot_ipsec_outb_sa_sw_rsvd(outb_sa);
		rlens = &outb_priv->rlens;

		outb_sa_dptr = (struct roc_ot_ipsec_outb_sa *)dev->outb.sa_dptr;
		memset(outb_sa_dptr, 0, sizeof(struct roc_ot_ipsec_outb_sa));

		/* Fill outbound sa params */
		rc = cnxk_ot_ipsec_outb_sa_fill(outb_sa_dptr, ipsec, crypto);
		if (rc) {
			plt_err("Failed to init outbound sa, rc=%d", rc);
			rc |= cnxk_eth_outb_sa_idx_put(dev, sa_idx);
			goto mempool_put;
		}

		/* Save userdata */
		outb_priv->userdata = conf->userdata;
		outb_priv->sa_idx = sa_idx;
		outb_priv->eth_sec = eth_sec;

		/* Save rlen info */
		cnxk_ipsec_outb_rlens_get(rlens, ipsec, crypto);

		/* Prepare session priv */
		sess_priv.sa_idx = outb_priv->sa_idx;
		sess_priv.roundup_byte = rlens->roundup_byte;
		sess_priv.roundup_len = rlens->roundup_len;
		sess_priv.partial_len = rlens->partial_len;
		sess_priv.mode = outb_sa_dptr->w2.s.ipsec_mode;
		sess_priv.outer_ip_ver = outb_sa_dptr->w2.s.outer_ip_ver;

		/* Pointer from eth_sec -> outb_sa */
		eth_sec->sa = outb_sa;
		eth_sec->sess = sess;
		eth_sec->sa_idx = sa_idx;
		eth_sec->spi = ipsec->spi;

		TAILQ_INSERT_TAIL(&dev->outb.list, eth_sec, entry);
		dev->outb.nb_sess++;
		/* Sync session in context cache */
		rc = roc_nix_inl_ctx_write(&dev->nix, outb_sa_dptr, eth_sec->sa,
					   eth_sec->inb,
					   sizeof(struct roc_ot_ipsec_outb_sa));
		if (rc)
			goto mempool_put;
	}
	if (inbound && inl_dev)
		roc_nix_inl_dev_unlock();

	plt_nix_dbg("Created %s session with spi=%u, sa_idx=%u inl_dev=%u",
		    inbound ? "inbound" : "outbound", eth_sec->spi,
		    eth_sec->sa_idx, eth_sec->inl_dev);
	/*
	 * Update fast path info in priv area.
	 */
	set_sec_session_private_data(sess, (void *)sess_priv.u64);

	return 0;
mempool_put:
	if (inbound && inl_dev)
		roc_nix_inl_dev_unlock();
	rte_mempool_put(mempool, eth_sec);
	return rc;
}

static int
cn10k_eth_sec_session_destroy(void *device, struct rte_security_session *sess)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)device;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_eth_sec_sess *eth_sec;
	struct rte_mempool *mp;
	void *sa_dptr;

	eth_sec = cnxk_eth_sec_sess_get_by_sess(dev, sess);
	if (!eth_sec)
		return -ENOENT;

	if (eth_sec->inl_dev)
		roc_nix_inl_dev_lock();

	if (eth_sec->inb) {
		/* Disable SA */
		sa_dptr = dev->inb.sa_dptr;
		roc_nix_inl_inb_sa_init(sa_dptr);

		roc_nix_inl_ctx_write(&dev->nix, sa_dptr, eth_sec->sa,
				      eth_sec->inb,
				      sizeof(struct roc_ot_ipsec_inb_sa));
		TAILQ_REMOVE(&dev->inb.list, eth_sec, entry);
		dev->inb.nb_sess--;
	} else {
		/* Disable SA */
		sa_dptr = dev->outb.sa_dptr;
		roc_nix_inl_outb_sa_init(sa_dptr);

		roc_nix_inl_ctx_write(&dev->nix, sa_dptr, eth_sec->sa,
				      eth_sec->inb,
				      sizeof(struct roc_ot_ipsec_outb_sa));
		/* Release Outbound SA index */
		cnxk_eth_outb_sa_idx_put(dev, eth_sec->sa_idx);
		TAILQ_REMOVE(&dev->outb.list, eth_sec, entry);
		dev->outb.nb_sess--;
	}
	if (eth_sec->inl_dev)
		roc_nix_inl_dev_unlock();

	plt_nix_dbg("Destroyed %s session with spi=%u, sa_idx=%u, inl_dev=%u",
		    eth_sec->inb ? "inbound" : "outbound", eth_sec->spi,
		    eth_sec->sa_idx, eth_sec->inl_dev);

	/* Put eth_sec object back to pool */
	mp = rte_mempool_from_obj(eth_sec);
	set_sec_session_private_data(sess, NULL);
	rte_mempool_put(mp, eth_sec);
	return 0;
}

static const struct rte_security_capability *
cn10k_eth_sec_capabilities_get(void *device __rte_unused)
{
	return cn10k_eth_sec_capabilities;
}

void
cn10k_eth_sec_ops_override(void)
{
	static int init_once;

	if (init_once)
		return;
	init_once = 1;

	/* Update platform specific ops */
	cnxk_eth_sec_ops.session_create = cn10k_eth_sec_session_create;
	cnxk_eth_sec_ops.session_destroy = cn10k_eth_sec_session_destroy;
	cnxk_eth_sec_ops.capabilities_get = cn10k_eth_sec_capabilities_get;
}
