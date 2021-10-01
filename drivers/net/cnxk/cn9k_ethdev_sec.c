/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <rte_cryptodev.h>
#include <rte_security.h>
#include <rte_security_driver.h>

#include <cn9k_ethdev.h>
#include <cnxk_security.h>

static struct rte_cryptodev_capabilities cn9k_eth_sec_crypto_caps[] = {
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

static const struct rte_security_capability cn9k_eth_sec_capabilities[] = {
	{	/* IPsec Inline Protocol ESP Tunnel Ingress */
		.action = RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
			.options = { 0 }
		},
		.crypto_capabilities = cn9k_eth_sec_crypto_caps,
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
		.crypto_capabilities = cn9k_eth_sec_crypto_caps,
		.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
	},
	{
		.action = RTE_SECURITY_ACTION_TYPE_NONE
	}
};

static inline int
ar_window_init(struct cn9k_inb_priv_data *inb_priv)
{
	if (inb_priv->replay_win_sz > CNXK_ON_AR_WIN_SIZE_MAX) {
		plt_err("Replay window size:%u is not supported",
			inb_priv->replay_win_sz);
		return -ENOTSUP;
	}

	rte_spinlock_init(&inb_priv->ar.lock);
	/*
	 * Set window bottom to 1, base and top to size of
	 * window
	 */
	inb_priv->ar.winb = 1;
	inb_priv->ar.wint = inb_priv->replay_win_sz;
	inb_priv->ar.base = inb_priv->replay_win_sz;

	return 0;
}

static int
cn9k_eth_sec_session_create(void *device,
			    struct rte_security_session_conf *conf,
			    struct rte_security_session *sess,
			    struct rte_mempool *mempool)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)device;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct rte_security_ipsec_xform *ipsec;
	struct cn9k_sec_sess_priv sess_priv;
	struct rte_crypto_sym_xform *crypto;
	struct cnxk_eth_sec_sess *eth_sec;
	bool inbound;
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

	/* Search if a session already exists */
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

	if (inbound) {
		struct cn9k_inb_priv_data *inb_priv;
		struct roc_onf_ipsec_inb_sa *inb_sa;

		PLT_STATIC_ASSERT(sizeof(struct cn9k_inb_priv_data) <
				  ROC_NIX_INL_ONF_IPSEC_INB_SW_RSVD);

		/* Get Inbound SA from NIX_RX_IPSEC_SA_BASE. Assume no inline
		 * device always for CN9K.
		 */
		inb_sa = (struct roc_onf_ipsec_inb_sa *)
			roc_nix_inl_inb_sa_get(&dev->nix, false, ipsec->spi);
		if (!inb_sa) {
			plt_err("Failed to create ingress sa");
			rc = -EFAULT;
			goto mempool_put;
		}

		/* Check if SA is already in use */
		if (inb_sa->ctl.valid) {
			plt_err("Inbound SA with SPI %u already in use",
				ipsec->spi);
			rc = -EBUSY;
			goto mempool_put;
		}

		memset(inb_sa, 0, sizeof(struct roc_onf_ipsec_inb_sa));

		/* Fill inbound sa params */
		rc = cnxk_onf_ipsec_inb_sa_fill(inb_sa, ipsec, crypto);
		if (rc) {
			plt_err("Failed to init inbound sa, rc=%d", rc);
			goto mempool_put;
		}

		inb_priv = roc_nix_inl_onf_ipsec_inb_sa_sw_rsvd(inb_sa);
		/* Back pointer to get eth_sec */
		inb_priv->eth_sec = eth_sec;

		/* Save userdata in inb private area */
		inb_priv->userdata = conf->userdata;

		inb_priv->replay_win_sz = ipsec->replay_win_sz;
		if (inb_priv->replay_win_sz) {
			rc = ar_window_init(inb_priv);
			if (rc)
				goto mempool_put;
		}

		/* Prepare session priv */
		sess_priv.inb_sa = 1;
		sess_priv.sa_idx = ipsec->spi;

		/* Pointer from eth_sec -> inb_sa */
		eth_sec->sa = inb_sa;
		eth_sec->sess = sess;
		eth_sec->sa_idx = ipsec->spi;
		eth_sec->spi = ipsec->spi;
		eth_sec->inb = true;

		TAILQ_INSERT_TAIL(&dev->inb.list, eth_sec, entry);
		dev->inb.nb_sess++;
	} else {
		struct cn9k_outb_priv_data *outb_priv;
		struct roc_onf_ipsec_outb_sa *outb_sa;
		uintptr_t sa_base = dev->outb.sa_base;
		struct cnxk_ipsec_outb_rlens *rlens;
		uint32_t sa_idx;

		PLT_STATIC_ASSERT(sizeof(struct cn9k_outb_priv_data) <
				  ROC_NIX_INL_ONF_IPSEC_OUTB_SW_RSVD);

		/* Alloc an sa index */
		rc = cnxk_eth_outb_sa_idx_get(dev, &sa_idx);
		if (rc)
			goto mempool_put;

		outb_sa = roc_nix_inl_onf_ipsec_outb_sa(sa_base, sa_idx);
		outb_priv = roc_nix_inl_onf_ipsec_outb_sa_sw_rsvd(outb_sa);
		rlens = &outb_priv->rlens;

		memset(outb_sa, 0, sizeof(struct roc_onf_ipsec_outb_sa));

		/* Fill outbound sa params */
		rc = cnxk_onf_ipsec_outb_sa_fill(outb_sa, ipsec, crypto);
		if (rc) {
			plt_err("Failed to init outbound sa, rc=%d", rc);
			rc |= cnxk_eth_outb_sa_idx_put(dev, sa_idx);
			goto mempool_put;
		}

		/* Save userdata */
		outb_priv->userdata = conf->userdata;
		outb_priv->sa_idx = sa_idx;
		outb_priv->eth_sec = eth_sec;
		/* Start sequence number with 1 */
		outb_priv->seq = 1;

		memcpy(&outb_priv->nonce, outb_sa->nonce, 4);
		if (outb_sa->ctl.enc_type == ROC_IE_ON_SA_ENC_AES_GCM)
			outb_priv->copy_salt = 1;

		/* Save rlen info */
		cnxk_ipsec_outb_rlens_get(rlens, ipsec, crypto);

		sess_priv.sa_idx = outb_priv->sa_idx;
		sess_priv.roundup_byte = rlens->roundup_byte;
		sess_priv.roundup_len = rlens->roundup_len;
		sess_priv.partial_len = rlens->partial_len;

		/* Pointer from eth_sec -> outb_sa */
		eth_sec->sa = outb_sa;
		eth_sec->sess = sess;
		eth_sec->sa_idx = sa_idx;
		eth_sec->spi = ipsec->spi;

		TAILQ_INSERT_TAIL(&dev->outb.list, eth_sec, entry);
		dev->outb.nb_sess++;
	}

	/* Sync SA content */
	plt_atomic_thread_fence(__ATOMIC_ACQ_REL);

	plt_nix_dbg("Created %s session with spi=%u, sa_idx=%u",
		    inbound ? "inbound" : "outbound", eth_sec->spi,
		    eth_sec->sa_idx);
	/*
	 * Update fast path info in priv area.
	 */
	set_sec_session_private_data(sess, (void *)sess_priv.u64);

	return 0;
mempool_put:
	rte_mempool_put(mempool, eth_sec);
	return rc;
}

static int
cn9k_eth_sec_session_destroy(void *device, struct rte_security_session *sess)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)device;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_onf_ipsec_outb_sa *outb_sa;
	struct roc_onf_ipsec_inb_sa *inb_sa;
	struct cnxk_eth_sec_sess *eth_sec;
	struct rte_mempool *mp;

	eth_sec = cnxk_eth_sec_sess_get_by_sess(dev, sess);
	if (!eth_sec)
		return -ENOENT;

	if (eth_sec->inb) {
		inb_sa = eth_sec->sa;
		/* Disable SA */
		inb_sa->ctl.valid = 0;

		TAILQ_REMOVE(&dev->inb.list, eth_sec, entry);
		dev->inb.nb_sess--;
	} else {
		outb_sa = eth_sec->sa;
		/* Disable SA */
		outb_sa->ctl.valid = 0;

		/* Release Outbound SA index */
		cnxk_eth_outb_sa_idx_put(dev, eth_sec->sa_idx);
		TAILQ_REMOVE(&dev->outb.list, eth_sec, entry);
		dev->outb.nb_sess--;
	}

	/* Sync SA content */
	plt_atomic_thread_fence(__ATOMIC_ACQ_REL);

	plt_nix_dbg("Destroyed %s session with spi=%u, sa_idx=%u",
		    eth_sec->inb ? "inbound" : "outbound", eth_sec->spi,
		    eth_sec->sa_idx);

	/* Put eth_sec object back to pool */
	mp = rte_mempool_from_obj(eth_sec);
	set_sec_session_private_data(sess, NULL);
	rte_mempool_put(mp, eth_sec);
	return 0;
}

static const struct rte_security_capability *
cn9k_eth_sec_capabilities_get(void *device __rte_unused)
{
	return cn9k_eth_sec_capabilities;
}

void
cn9k_eth_sec_ops_override(void)
{
	static int init_once;

	if (init_once)
		return;
	init_once = 1;

	/* Update platform specific ops */
	cnxk_eth_sec_ops.session_create = cn9k_eth_sec_session_create;
	cnxk_eth_sec_ops.session_destroy = cn9k_eth_sec_session_destroy;
	cnxk_eth_sec_ops.capabilities_get = cn9k_eth_sec_capabilities_get;
}
