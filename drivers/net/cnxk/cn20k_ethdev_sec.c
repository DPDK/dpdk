/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <rte_cryptodev.h>
#include <rte_eventdev.h>
#include <rte_pmd_cnxk.h>
#include <rte_security.h>
#include <rte_security_driver.h>

#include <cn20k_ethdev.h>
#include <cnxk_security.h>
#include <roc_priv.h>

PLT_STATIC_ASSERT(offsetof(struct rte_pmd_cnxk_ipsec_inb_sa, ctx.ar_winbits) ==
		  offsetof(struct roc_ow_ipsec_inb_sa, ctx.ar_winbits));

PLT_STATIC_ASSERT(offsetof(struct rte_pmd_cnxk_ipsec_outb_sa, ctx.mib_pkts) ==
		  offsetof(struct roc_ow_ipsec_outb_sa, ctx.mib_pkts));

PLT_STATIC_ASSERT(RTE_PMD_CNXK_CTX_MAX_CKEY_LEN == ROC_CTX_MAX_CKEY_LEN);
PLT_STATIC_ASSERT(RTE_PMD_CNXK_CTX_MAX_OPAD_IPAD_LEN == RTE_PMD_CNXK_CTX_MAX_OPAD_IPAD_LEN);

PLT_STATIC_ASSERT(RTE_PMD_CNXK_AR_WIN_SIZE_MIN == ROC_AR_WIN_SIZE_MIN);
PLT_STATIC_ASSERT(RTE_PMD_CNXK_AR_WIN_SIZE_MAX == ROC_AR_WIN_SIZE_MAX);
PLT_STATIC_ASSERT(RTE_PMD_CNXK_LOG_MIN_AR_WIN_SIZE_M1 == ROC_LOG_MIN_AR_WIN_SIZE_M1);
PLT_STATIC_ASSERT(RTE_PMD_CNXK_AR_WINBITS_SZ == ROC_AR_WINBITS_SZ);

static struct rte_cryptodev_capabilities cn20k_eth_sec_crypto_caps[] = {
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
	{	/* AES CTR */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_AES_CTR,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.iv_size = {
					.min = 12,
					.max = 16,
					.increment = 4
				}
			}, }
		}, }
	},
	{	/* 3DES CBC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_3DES_CBC,
				.block_size = 8,
				.key_size = {
					.min = 24,
					.max = 24,
					.increment = 0
				},
				.iv_size = {
					.min = 8,
					.max = 16,
					.increment = 8
				}
			}, }
		}, }
	},
	{	/* AES-XCBC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{ .sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_AES_XCBC_MAC,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.digest_size = {
					.min = 12,
					.max = 12,
					.increment = 0,
				},
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
	{	/* SHA256 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA256_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 1,
					.max = 1024,
					.increment = 1
				},
				.digest_size = {
					.min = 16,
					.max = 32,
					.increment = 16
				},
			}, }
		}, }
	},
	{	/* SHA384 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA384_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 1,
					.max = 1024,
					.increment = 1
				},
				.digest_size = {
					.min = 24,
					.max = 48,
					.increment = 24
					},
			}, }
		}, }
	},
	{	/* SHA512 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA512_HMAC,
				.block_size = 128,
				.key_size = {
					.min = 1,
					.max = 1024,
					.increment = 1
				},
				.digest_size = {
					.min = 32,
					.max = 64,
					.increment = 32
				},
			}, }
		}, }
	},
	{	/* AES GMAC (AUTH) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_AES_GMAC,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.digest_size = {
					.min = 8,
					.max = 16,
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
	{	/* AES CCM */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AEAD,
			{.aead = {
				.algo = RTE_CRYPTO_AEAD_AES_CCM,
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
					.min = 11,
					.max = 13,
					.increment = 1
				}
			}, }
		}, }
	},
	{	/* NULL (AUTH) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_NULL,
				.block_size = 1,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.digest_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
			}, },
		}, },
	},
	{	/* NULL (CIPHER) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_NULL,
				.block_size = 1,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.iv_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				}
			}, },
		}, }
	},

	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static const struct rte_security_capability cn20k_eth_sec_ipsec_capabilities[] = {
	{	/* IPsec Inline Protocol ESP Tunnel Ingress */
		.action = RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
			.replay_win_sz_max = ROC_AR_WIN_SIZE_MAX,
			.options = {
				.udp_encap = 1,
				.udp_ports_verify = 1,
				.copy_df = 1,
				.copy_dscp = 1,
				.copy_flabel = 1,
				.tunnel_hdr_verify = RTE_SECURITY_IPSEC_TUNNEL_VERIFY_SRC_DST_ADDR,
				.dec_ttl = 1,
				.ip_csum_enable = 1,
				.l4_csum_enable = 1,
				.stats = 1,
				.esn = 1,
				.ingress_oop = 1,
			},
		},
		.crypto_capabilities = cn20k_eth_sec_crypto_caps,
		.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
	},
	{	/* IPsec Inline Protocol ESP Tunnel Egress */
		.action = RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
			.replay_win_sz_max = ROC_AR_WIN_SIZE_MAX,
			.options = {
				.iv_gen_disable = 1,
				.udp_encap = 1,
				.udp_ports_verify = 1,
				.copy_df = 1,
				.copy_dscp = 1,
				.copy_flabel = 1,
				.dec_ttl = 1,
				.ip_csum_enable = 1,
				.l4_csum_enable = 1,
				.stats = 1,
				.esn = 1,
			},
		},
		.crypto_capabilities = cn20k_eth_sec_crypto_caps,
		.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
	},
	{	/* IPsec Inline Protocol ESP Transport Egress */
		.action = RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
			.replay_win_sz_max = ROC_AR_WIN_SIZE_MAX,
			.options = {
				.iv_gen_disable = 1,
				.udp_encap = 1,
				.udp_ports_verify = 1,
				.copy_df = 1,
				.copy_dscp = 1,
				.dec_ttl = 1,
				.ip_csum_enable = 1,
				.l4_csum_enable = 1,
				.stats = 1,
				.esn = 1,
				.ingress_oop = 1,
			},
		},
		.crypto_capabilities = cn20k_eth_sec_crypto_caps,
		.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
	},
	{	/* IPsec Inline Protocol ESP Transport Ingress */
		.action = RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
			.replay_win_sz_max = ROC_AR_WIN_SIZE_MAX,
			.options = {
				.udp_encap = 1,
				.udp_ports_verify = 1,
				.copy_df = 1,
				.copy_dscp = 1,
				.dec_ttl = 1,
				.ip_csum_enable = 1,
				.l4_csum_enable = 1,
				.stats = 1,
				.esn = 1,
				.ingress_oop = 1,
			},
		},
		.crypto_capabilities = cn20k_eth_sec_crypto_caps,
		.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
	},
};

#define SEC_CAPS_LEN (RTE_DIM(cn20k_eth_sec_ipsec_capabilities) + 1)

static struct rte_security_capability cn20k_eth_sec_capabilities[SEC_CAPS_LEN];

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
cn20k_eth_sec_sso_work_cb(uint64_t *gw, void *args, uint32_t soft_exp_event)
{
	struct rte_eth_event_ipsec_desc desc;
	struct cn20k_sec_sess_priv sess_priv;
	struct cn20k_outb_priv_data *priv;
	struct roc_ow_ipsec_outb_sa *sa;
	struct cpt_cn20k_res_s *res;
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
		if (soft_exp_event & 0x1) {
			sa = (struct roc_ow_ipsec_outb_sa *)args;
			priv = roc_nix_inl_ow_ipsec_outb_sa_sw_rsvd(sa);
			desc.metadata = (uint64_t)priv->userdata;
			if (sa->w2.s.life_unit == ROC_IE_OT_SA_LIFE_UNIT_PKTS)
				desc.subtype = RTE_ETH_EVENT_IPSEC_SA_PKT_EXPIRY;
			else
				desc.subtype = RTE_ETH_EVENT_IPSEC_SA_BYTE_EXPIRY;
			eth_dev = &rte_eth_devices[soft_exp_event >> 8];
			rte_eth_dev_callback_process(eth_dev, RTE_ETH_EVENT_IPSEC, &desc);
		} else {
			plt_err("Unknown event gw[0] = 0x%016lx, gw[1] = 0x%016lx", gw[0], gw[1]);
		}
		return;
	}

	/* Get ethdev port from tag */
	port = gw[0] & 0xFF;
	eth_dev = &rte_eth_devices[port];
	dev = cnxk_eth_pmd_priv(eth_dev);

	sess_priv.u64 = *rte_security_dynfield(mbuf);
	/* Calculate dlen adj */
	dlen_adj = mbuf->pkt_len - mbuf->l2_len;
	rlen = (dlen_adj + sess_priv.roundup_len) + (sess_priv.roundup_byte - 1);
	rlen &= ~(uint64_t)(sess_priv.roundup_byte - 1);
	rlen += sess_priv.partial_len;
	dlen_adj = rlen - dlen_adj;

	/* Find the res area residing on next cacheline after end of data */
	nixtx = rte_pktmbuf_mtod(mbuf, uintptr_t) + mbuf->pkt_len + dlen_adj;
	nixtx += BIT_ULL(7);
	nixtx = (nixtx - 1) & ~(BIT_ULL(7) - 1);
	res = (struct cpt_cn20k_res_s *)nixtx;

	plt_nix_dbg("Outbound error, mbuf %p, sa_index %u, compcode %x uc %x", mbuf,
		    sess_priv.sa_idx, res->compcode, res->uc_compcode);

	sess_priv.u64 = *rte_security_dynfield(mbuf);

	sa_base = dev->outb.sa_base;
	sa = roc_nix_inl_ow_ipsec_outb_sa(sa_base, sess_priv.sa_idx);
	priv = roc_nix_inl_ow_ipsec_outb_sa_sw_rsvd(sa);

	memset(&desc, 0, sizeof(desc));

	switch (res->uc_compcode) {
	case ROC_IE_OT_UCC_ERR_SA_OVERFLOW:
		desc.subtype = RTE_ETH_EVENT_IPSEC_ESN_OVERFLOW;
		break;
	case ROC_IE_OT_UCC_ERR_SA_EXPIRED:
		if (sa->w2.s.life_unit == ROC_IE_OT_SA_LIFE_UNIT_PKTS)
			desc.subtype = RTE_ETH_EVENT_IPSEC_SA_PKT_HARD_EXPIRY;
		else
			desc.subtype = RTE_ETH_EVENT_IPSEC_SA_BYTE_HARD_EXPIRY;
		break;
	case ROC_IE_OT_UCC_ERR_PKT_IP:
		warn_cnt++;
		if (warn_cnt % 10000 == 0)
			plt_warn("Outbound error, bad ip pkt, mbuf %p,"
				 " sa_index %u (total warnings %" PRIu64 ")",
				 mbuf, sess_priv.sa_idx, warn_cnt);
		desc.subtype = -res->uc_compcode;
		break;
	default:
		warn_cnt++;
		if (warn_cnt % 10000 == 0)
			plt_warn("Outbound error, mbuf %p, sa_index %u,"
				 " compcode %x uc %x,"
				 " (total warnings %" PRIu64 ")",
				 mbuf, sess_priv.sa_idx, res->compcode, res->uc_compcode, warn_cnt);
		desc.subtype = -res->uc_compcode;
		break;
	}

	desc.metadata = (uint64_t)priv->userdata;
	rte_eth_dev_callback_process(eth_dev, RTE_ETH_EVENT_IPSEC, &desc);
	cnxk_pktmbuf_free_no_cache(mbuf);
}

static void
outb_dbg_iv_update(struct roc_ow_ipsec_outb_sa *outb_sa, const char *__iv_str)
{
	uint8_t *iv_dbg = outb_sa->iv.iv_dbg;
	char *iv_str = strdup(__iv_str);
	char *iv_b = NULL, len = 16;
	char *save;
	int i;

	if (!iv_str)
		return;

	if (outb_sa->w2.s.enc_type == ROC_IE_SA_ENC_AES_GCM ||
	    outb_sa->w2.s.enc_type == ROC_IE_SA_ENC_AES_CTR ||
	    outb_sa->w2.s.enc_type == ROC_IE_SA_ENC_AES_CCM ||
	    outb_sa->w2.s.auth_type == ROC_IE_SA_AUTH_AES_GMAC) {
		memset(outb_sa->iv.s.iv_dbg1, 0, sizeof(outb_sa->iv.s.iv_dbg1));
		memset(outb_sa->iv.s.iv_dbg2, 0, sizeof(outb_sa->iv.s.iv_dbg2));

		iv_dbg = outb_sa->iv.s.iv_dbg1;
		for (i = 0; i < 4; i++) {
			iv_b = strtok_r(i ? NULL : iv_str, ",", &save);
			if (!iv_b)
				break;
			iv_dbg[i] = strtoul(iv_b, NULL, 0);
		}
		*(uint32_t *)iv_dbg = rte_be_to_cpu_32(*(uint32_t *)iv_dbg);

		iv_dbg = outb_sa->iv.s.iv_dbg2;
		for (i = 0; i < 4; i++) {
			iv_b = strtok_r(NULL, ",", &save);
			if (!iv_b)
				break;
			iv_dbg[i] = strtoul(iv_b, NULL, 0);
		}
		*(uint32_t *)iv_dbg = rte_be_to_cpu_32(*(uint32_t *)iv_dbg);

	} else {
		iv_dbg = outb_sa->iv.iv_dbg;
		memset(iv_dbg, 0, sizeof(outb_sa->iv.iv_dbg));

		for (i = 0; i < len; i++) {
			iv_b = strtok_r(i ? NULL : iv_str, ",", &save);
			if (!iv_b)
				break;
			iv_dbg[i] = strtoul(iv_b, NULL, 0);
		}
		*(uint64_t *)iv_dbg = rte_be_to_cpu_64(*(uint64_t *)iv_dbg);
		*(uint64_t *)&iv_dbg[8] = rte_be_to_cpu_64(*(uint64_t *)&iv_dbg[8]);
	}

	/* Update source of IV */
	outb_sa->w2.s.iv_src = ROC_IE_OT_SA_IV_SRC_FROM_SA;
	free(iv_str);
}

static int
cn20k_eth_sec_outb_sa_misc_fill(struct roc_nix *roc_nix, struct roc_ow_ipsec_outb_sa *sa,
				void *sa_cptr, struct rte_security_ipsec_xform *ipsec_xfrm,
				uint32_t sa_idx)
{
	uint64_t *ring_base, ring_addr;

	if (ipsec_xfrm->life.bytes_soft_limit | ipsec_xfrm->life.packets_soft_limit) {
		ring_base = roc_nix_inl_outb_ring_base_get(roc_nix);
		if (ring_base == NULL)
			return -ENOTSUP;

		ring_addr = ring_base[sa_idx >> ROC_NIX_SOFT_EXP_ERR_RING_MAX_ENTRY_LOG2];
		sa->ctx.err_ctl.s.mode = ROC_IE_OT_ERR_CTL_MODE_RING;
		sa->ctx.err_ctl.s.address = ring_addr >> 3;
		sa->w0.s.ctx_id = ((uintptr_t)sa_cptr >> 51) & 0x1ff;
	}

	return 0;
}

static int
cn20k_eth_sec_session_create(void *device, struct rte_security_session_conf *conf,
			     struct rte_security_session *sess)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)device;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct rte_security_ipsec_xform *ipsec;
	struct cn20k_sec_sess_priv sess_priv;
	struct rte_crypto_sym_xform *crypto;
	struct cnxk_eth_sec_sess *eth_sec = SECURITY_GET_SESS_PRIV(sess);
	struct roc_nix *nix = &dev->nix;
	bool inbound, inl_dev;
	rte_spinlock_t *lock;
	char tbuf[128] = {0};
	int rc = 0;

	if (conf->action_type != RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL)
		return -ENOTSUP;

	if (conf->protocol != RTE_SECURITY_PROTOCOL_IPSEC)
		return -ENOTSUP;

	if (nix->custom_inb_sa)
		return -ENOTSUP;

	if (rte_security_dynfield_register() < 0)
		return -ENOTSUP;

	if (conf->ipsec.options.ip_reassembly_en && dev->reass_dynfield_off < 0) {
		if (rte_eth_ip_reassembly_dynfield_register(&dev->reass_dynfield_off,
							    &dev->reass_dynflag_bit) < 0)
			return -rte_errno;
	}

	if (conf->ipsec.options.ingress_oop && rte_security_oop_dynfield_offset < 0) {
		/* Register for security OOP dynfield if required */
		if (rte_security_oop_dynfield_register() < 0)
			return -rte_errno;
	}

	/* We cannot support inbound reassembly and OOP together */
	if (conf->ipsec.options.ip_reassembly_en && conf->ipsec.options.ingress_oop) {
		plt_err("Cannot support Inbound reassembly and OOP together");
		return -ENOTSUP;
	}

	ipsec = &conf->ipsec;
	crypto = conf->crypto_xform;
	inbound = !!(ipsec->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS);
	inl_dev = !!dev->inb.inl_dev;

	memset(eth_sec, 0, sizeof(struct cnxk_eth_sec_sess));
	sess_priv.u64 = 0;

	lock = inbound ? &dev->inb.lock : &dev->outb.lock;
	rte_spinlock_lock(lock);

	/* Acquire lock on inline dev for inbound */
	if (inbound && inl_dev)
		roc_nix_inl_dev_lock();

	if (inbound) {
		struct roc_ow_ipsec_inb_sa *inb_sa, *inb_sa_dptr;
		struct cn20k_inb_priv_data *inb_priv;
		uint32_t spi_mask;
		uintptr_t sa;

		PLT_STATIC_ASSERT(sizeof(struct cn20k_inb_priv_data) <
				  ROC_NIX_INL_OT_IPSEC_INB_SW_RSVD);

		spi_mask = roc_nix_inl_inb_spi_range(nix, inl_dev, NULL, NULL);

		/* Search if a session already exits */
		if (cnxk_eth_sec_sess_get_by_sa_idx(dev, ipsec->spi & spi_mask, true)) {
			plt_err("Inbound SA with SPI/SA index %u already in use", ipsec->spi);
			rc = -EEXIST;
			goto err;
		}

		/* Get Inbound SA from NIX_RX_IPSEC_SA_BASE */
		sa = roc_nix_inl_inb_sa_get(nix, inl_dev, ipsec->spi);
		if (!sa && dev->inb.inl_dev) {
			snprintf(tbuf, sizeof(tbuf),
				 "Failed to create ingress sa, inline dev "
				 "not found or spi not in range");
			rc = -ENOTSUP;
			goto err;
		} else if (!sa) {
			snprintf(tbuf, sizeof(tbuf), "Failed to create ingress sa");
			rc = -EFAULT;
			goto err;
		}

		inb_sa = (struct roc_ow_ipsec_inb_sa *)sa;

		/* Check if SA is already in use */
		if (inb_sa->w2.s.valid) {
			snprintf(tbuf, sizeof(tbuf), "Inbound SA with SPI %u already in use",
				 ipsec->spi);
			rc = -EBUSY;
			goto err;
		}

		inb_sa_dptr = (struct roc_ow_ipsec_inb_sa *)dev->inb.sa_dptr;
		memset(inb_sa_dptr, 0, sizeof(struct roc_ow_ipsec_inb_sa));

		/* Fill inbound sa params */
		rc = cnxk_ow_ipsec_inb_sa_fill(inb_sa_dptr, ipsec, crypto);
		if (rc) {
			snprintf(tbuf, sizeof(tbuf), "Failed to init inbound sa, rc=%d", rc);
			goto err;
		}

		inb_priv = roc_nix_inl_ow_ipsec_inb_sa_sw_rsvd(inb_sa);
		/* Back pointer to get eth_sec */
		inb_priv->eth_sec = eth_sec;
		/* Save userdata in inb private area */
		inb_priv->userdata = conf->userdata;

		/* Save SA index/SPI in cookie for now */
		inb_sa_dptr->w1.s.cookie = ipsec->spi & spi_mask;

		if (ipsec->options.stats == 1) {
			/* Enable mib counters */
			inb_sa_dptr->w0.s.count_mib_bytes = 1;
			inb_sa_dptr->w0.s.count_mib_pkts = 1;
		}

		/* Enable out-of-place processing */
		if (ipsec->options.ingress_oop)
			inb_sa_dptr->w0.s.pkt_format = ROC_IE_OT_SA_PKT_FMT_FULL;

		/* Prepare session priv */
		sess_priv.inb_sa = 1;
		sess_priv.sa_idx = ipsec->spi & spi_mask;

		/* Pointer from eth_sec -> inb_sa */
		eth_sec->sa = inb_sa;
		eth_sec->sess = sess;
		eth_sec->sa_idx = ipsec->spi & spi_mask;
		eth_sec->spi = ipsec->spi;
		eth_sec->inl_dev = !!dev->inb.inl_dev;
		eth_sec->inb = true;
		eth_sec->inb_oop = !!ipsec->options.ingress_oop;

		TAILQ_INSERT_TAIL(&dev->inb.list, eth_sec, entry);
		dev->inb.nb_sess++;
		/* Sync session in context cache */
		rc = roc_nix_inl_ctx_write(&dev->nix, inb_sa_dptr, eth_sec->sa, eth_sec->inb,
					   sizeof(struct roc_ow_ipsec_inb_sa));
		if (rc)
			goto err;

		if (conf->ipsec.options.ip_reassembly_en) {
			inb_priv->reass_dynfield_off = dev->reass_dynfield_off;
			inb_priv->reass_dynflag_bit = dev->reass_dynflag_bit;
		}

		if (ipsec->options.ingress_oop)
			dev->inb.nb_oop++;

	} else {
		struct roc_ow_ipsec_outb_sa *outb_sa, *outb_sa_dptr;
		struct cn20k_outb_priv_data *outb_priv;
		struct cnxk_ipsec_outb_rlens *rlens;
		uint64_t sa_base = dev->outb.sa_base;
		const char *iv_str;
		uint32_t sa_idx;

		PLT_STATIC_ASSERT(sizeof(struct cn20k_outb_priv_data) <
				  ROC_NIX_INL_OT_IPSEC_OUTB_SW_RSVD);

		/* Alloc an sa index */
		rc = cnxk_eth_outb_sa_idx_get(dev, &sa_idx, ipsec->spi);
		if (rc)
			goto err;

		outb_sa = roc_nix_inl_ow_ipsec_outb_sa(sa_base, sa_idx);
		outb_priv = roc_nix_inl_ow_ipsec_outb_sa_sw_rsvd(outb_sa);
		rlens = &outb_priv->rlens;

		outb_sa_dptr = (struct roc_ow_ipsec_outb_sa *)dev->outb.sa_dptr;
		memset(outb_sa_dptr, 0, sizeof(struct roc_ow_ipsec_outb_sa));

		/* Fill outbound sa params */
		rc = cnxk_ow_ipsec_outb_sa_fill(outb_sa_dptr, ipsec, crypto);
		if (rc) {
			snprintf(tbuf, sizeof(tbuf), "Failed to init outbound sa, rc=%d", rc);
			rc |= cnxk_eth_outb_sa_idx_put(dev, sa_idx);
			goto err;
		}

		if (conf->ipsec.options.iv_gen_disable == 1) {
			iv_str = getenv("ETH_SEC_IV_OVR");
			if (iv_str)
				outb_dbg_iv_update(outb_sa_dptr, iv_str);
		}
		/* Fill outbound sa misc params */
		rc = cn20k_eth_sec_outb_sa_misc_fill(&dev->nix, outb_sa_dptr, outb_sa, ipsec,
						     sa_idx);
		if (rc) {
			snprintf(tbuf, sizeof(tbuf), "Failed to init outb sa misc params, rc=%d",
				 rc);
			rc |= cnxk_eth_outb_sa_idx_put(dev, sa_idx);
			goto err;
		}

		/* Save userdata */
		outb_priv->userdata = conf->userdata;
		outb_priv->sa_idx = sa_idx;
		outb_priv->eth_sec = eth_sec;

		/* Save rlen info */
		cnxk_ipsec_outb_rlens_get(rlens, ipsec, crypto);

		if (ipsec->options.stats == 1) {
			/* Enable mib counters */
			outb_sa_dptr->w0.s.count_mib_bytes = 1;
			outb_sa_dptr->w0.s.count_mib_pkts = 1;
		}

		/* Prepare session priv */
		sess_priv.sa_idx = outb_priv->sa_idx;
		sess_priv.roundup_byte = rlens->roundup_byte;
		sess_priv.roundup_len = rlens->roundup_len;
		sess_priv.partial_len = rlens->partial_len;
		sess_priv.mode = outb_sa_dptr->w2.s.ipsec_mode;
		sess_priv.outer_ip_ver = outb_sa_dptr->w2.s.outer_ip_ver;
		/* Propagate inner checksum enable from SA to fast path */
		sess_priv.chksum =
			(!ipsec->options.ip_csum_enable << 1 | !ipsec->options.l4_csum_enable);
		sess_priv.dec_ttl = ipsec->options.dec_ttl;
		if (roc_feature_nix_has_inl_ipsec_mseg() && dev->outb.cpt_eng_caps & BIT_ULL(35))
			sess_priv.nixtx_off = 1;

		/* Pointer from eth_sec -> outb_sa */
		eth_sec->sa = outb_sa;
		eth_sec->sess = sess;
		eth_sec->sa_idx = sa_idx;
		eth_sec->spi = ipsec->spi;

		TAILQ_INSERT_TAIL(&dev->outb.list, eth_sec, entry);
		dev->outb.nb_sess++;
		/* Sync session in context cache */
		rc = roc_nix_inl_ctx_write(&dev->nix, outb_sa_dptr, eth_sec->sa, eth_sec->inb,
					   sizeof(struct roc_ow_ipsec_outb_sa));
		if (rc)
			goto err;
	}
	if (inbound && inl_dev)
		roc_nix_inl_dev_unlock();
	rte_spinlock_unlock(lock);

	plt_nix_dbg("Created %s session with spi=%u, sa_idx=%u inl_dev=%u",
		    inbound ? "inbound" : "outbound", eth_sec->spi, eth_sec->sa_idx,
		    eth_sec->inl_dev);
	/*
	 * Update fast path info in priv area.
	 */
	sess->fast_mdata = sess_priv.u64;

	return 0;
err:
	if (inbound && inl_dev)
		roc_nix_inl_dev_unlock();
	rte_spinlock_unlock(lock);

	if (rc)
		plt_err("%s", tbuf);
	return rc;
}

static int
cn20k_eth_sec_session_destroy(void *device, struct rte_security_session *sess)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)device;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_eth_sec_sess *eth_sec;
	rte_spinlock_t *lock;
	void *sa_dptr;

	eth_sec = cnxk_eth_sec_sess_get_by_sess(dev, sess);
	if (!eth_sec)
		return -ENOENT;
	if (dev->nix.custom_inb_sa)
		return -ENOTSUP;

	lock = eth_sec->inb ? &dev->inb.lock : &dev->outb.lock;
	rte_spinlock_lock(lock);

	if (eth_sec->inl_dev)
		roc_nix_inl_dev_lock();

	if (eth_sec->inb) {
		/* Disable SA */
		sa_dptr = dev->inb.sa_dptr;
		roc_ow_ipsec_inb_sa_init(sa_dptr);

		roc_nix_inl_ctx_write(&dev->nix, sa_dptr, eth_sec->sa, eth_sec->inb,
				      sizeof(struct roc_ow_ipsec_inb_sa));
		TAILQ_REMOVE(&dev->inb.list, eth_sec, entry);
		dev->inb.nb_sess--;
		if (eth_sec->inb_oop)
			dev->inb.nb_oop--;

	} else {
		/* Disable SA */
		sa_dptr = dev->outb.sa_dptr;
		roc_ow_ipsec_outb_sa_init(sa_dptr);

		roc_nix_inl_ctx_write(&dev->nix, sa_dptr, eth_sec->sa, eth_sec->inb,
				      sizeof(struct roc_ow_ipsec_outb_sa));
		/* Release Outbound SA index */
		cnxk_eth_outb_sa_idx_put(dev, eth_sec->sa_idx);
		TAILQ_REMOVE(&dev->outb.list, eth_sec, entry);
		dev->outb.nb_sess--;
	}
	if (eth_sec->inl_dev)
		roc_nix_inl_dev_unlock();

	rte_spinlock_unlock(lock);

	plt_nix_dbg("Destroyed %s session with spi=%u, sa_idx=%u, inl_dev=%u",
		    eth_sec->inb ? "inbound" : "outbound", eth_sec->spi, eth_sec->sa_idx,
		    eth_sec->inl_dev);

	return 0;
}

static const struct rte_security_capability *
cn20k_eth_sec_capabilities_get(void *device __rte_unused)
{
	return cn20k_eth_sec_capabilities;
}

static int
cn20k_eth_sec_session_update(void *device, struct rte_security_session *sess,
			     struct rte_security_session_conf *conf)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)device;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct rte_security_ipsec_xform *ipsec;
	struct cn20k_sec_sess_priv sess_priv;
	struct rte_crypto_sym_xform *crypto;
	struct cnxk_eth_sec_sess *eth_sec;
	bool inbound;
	int rc;

	if (conf->action_type != RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL ||
	    conf->protocol != RTE_SECURITY_PROTOCOL_IPSEC)
		return -ENOENT;

	ipsec = &conf->ipsec;
	crypto = conf->crypto_xform;
	inbound = !!(ipsec->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS);

	eth_sec = cnxk_eth_sec_sess_get_by_sess(dev, sess);
	if (!eth_sec)
		return -ENOENT;

	eth_sec->spi = conf->ipsec.spi;

	if (inbound) {
		struct roc_ow_ipsec_inb_sa *inb_sa_dptr, *inb_sa;
		struct cn20k_inb_priv_data *inb_priv;

		inb_sa = eth_sec->sa;
		inb_priv = roc_nix_inl_ow_ipsec_inb_sa_sw_rsvd(inb_sa);
		inb_sa_dptr = (struct roc_ow_ipsec_inb_sa *)dev->inb.sa_dptr;
		memset(inb_sa_dptr, 0, sizeof(struct roc_ow_ipsec_inb_sa));

		rc = cnxk_ow_ipsec_inb_sa_fill(inb_sa_dptr, ipsec, crypto);
		if (rc)
			return -EINVAL;
		/* Use cookie for original data */
		inb_sa_dptr->w1.s.cookie = inb_sa->w1.s.cookie;

		if (ipsec->options.stats == 1) {
			/* Enable mib counters */
			inb_sa_dptr->w0.s.count_mib_bytes = 1;
			inb_sa_dptr->w0.s.count_mib_pkts = 1;
		}

		/* Enable out-of-place processing */
		if (ipsec->options.ingress_oop)
			inb_sa_dptr->w0.s.pkt_format = ROC_IE_OT_SA_PKT_FMT_FULL;

		rc = roc_nix_inl_ctx_write(&dev->nix, inb_sa_dptr, eth_sec->sa, eth_sec->inb,
					   sizeof(struct roc_ow_ipsec_inb_sa));
		if (rc)
			return -EINVAL;

		/* Save userdata in inb private area */
		inb_priv->userdata = conf->userdata;
	} else {
		struct roc_ow_ipsec_outb_sa *outb_sa_dptr, *outb_sa;
		struct cn20k_outb_priv_data *outb_priv;
		struct cnxk_ipsec_outb_rlens *rlens;

		outb_sa = eth_sec->sa;
		outb_priv = roc_nix_inl_ow_ipsec_outb_sa_sw_rsvd(outb_sa);
		rlens = &outb_priv->rlens;
		outb_sa_dptr = (struct roc_ow_ipsec_outb_sa *)dev->outb.sa_dptr;
		memset(outb_sa_dptr, 0, sizeof(struct roc_ow_ipsec_outb_sa));

		rc = cnxk_ow_ipsec_outb_sa_fill(outb_sa_dptr, ipsec, crypto);
		if (rc)
			return -EINVAL;

		/* Save rlen info */
		cnxk_ipsec_outb_rlens_get(rlens, ipsec, crypto);

		if (ipsec->options.stats == 1) {
			/* Enable mib counters */
			outb_sa_dptr->w0.s.count_mib_bytes = 1;
			outb_sa_dptr->w0.s.count_mib_pkts = 1;
		}

		sess_priv.u64 = 0;
		sess_priv.sa_idx = outb_priv->sa_idx;
		sess_priv.roundup_byte = rlens->roundup_byte;
		sess_priv.roundup_len = rlens->roundup_len;
		sess_priv.partial_len = rlens->partial_len;
		sess_priv.mode = outb_sa_dptr->w2.s.ipsec_mode;
		sess_priv.outer_ip_ver = outb_sa_dptr->w2.s.outer_ip_ver;
		/* Propagate inner checksum enable from SA to fast path */
		sess_priv.chksum =
			(!ipsec->options.ip_csum_enable << 1 | !ipsec->options.l4_csum_enable);
		sess_priv.dec_ttl = ipsec->options.dec_ttl;
		if (roc_feature_nix_has_inl_ipsec_mseg() && dev->outb.cpt_eng_caps & BIT_ULL(35))
			sess_priv.nixtx_off = 1;

		rc = roc_nix_inl_ctx_write(&dev->nix, outb_sa_dptr, eth_sec->sa, eth_sec->inb,
					   sizeof(struct roc_ow_ipsec_outb_sa));
		if (rc)
			return -EINVAL;

		/* Save userdata */
		outb_priv->userdata = conf->userdata;
		sess->fast_mdata = sess_priv.u64;
	}

	return 0;
}

static int
cn20k_eth_sec_session_stats_get(void *device, struct rte_security_session *sess,
				struct rte_security_stats *stats)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)device;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_eth_sec_sess *eth_sec;
	int rc;

	eth_sec = cnxk_eth_sec_sess_get_by_sess(dev, sess);
	if (eth_sec == NULL)
		return -EINVAL;

	rc = roc_nix_inl_sa_sync(&dev->nix, eth_sec->sa, eth_sec->inb, ROC_NIX_INL_SA_OP_FLUSH);
	if (rc)
		return -EINVAL;
	rte_delay_ms(1);

	stats->protocol = RTE_SECURITY_PROTOCOL_IPSEC;

	if (eth_sec->inb) {
		stats->ipsec.ipackets = ((struct roc_ow_ipsec_inb_sa *)eth_sec->sa)->ctx.mib_pkts;
		stats->ipsec.ibytes = ((struct roc_ow_ipsec_inb_sa *)eth_sec->sa)->ctx.mib_octs;
	} else {
		stats->ipsec.opackets = ((struct roc_ow_ipsec_outb_sa *)eth_sec->sa)->ctx.mib_pkts;
		stats->ipsec.obytes = ((struct roc_ow_ipsec_outb_sa *)eth_sec->sa)->ctx.mib_octs;
	}

	return 0;
}

static void
eth_sec_caps_add(struct rte_security_capability eth_sec_caps[], uint32_t *idx,
		 const struct rte_security_capability *caps, uint32_t nb_caps)
{
	PLT_VERIFY(*idx + nb_caps < SEC_CAPS_LEN);

	rte_memcpy(&eth_sec_caps[*idx], caps, nb_caps * sizeof(caps[0]));
	*idx += nb_caps;
}

#define CPT_LMTST_BURST 32
static uint16_t
cn20k_inl_dev_submit(struct roc_nix_inl_dev_q *q, void *inst, uint16_t nb_inst)
{
	uintptr_t lbase = q->lmt_base;
	uint8_t lnum, shft, loff;
	uint16_t left, burst;
	rte_iova_t io_addr;
	uint16_t lmt_id;

	/* Check the flow control to avoid the queue overflow */
	if (cnxk_nix_inl_fc_check(q->fc_addr, &q->fc_addr_sw, q->nb_desc, nb_inst))
		return 0;

	io_addr = q->io_addr;
	ROC_LMT_CPT_BASE_ID_GET(lbase, lmt_id);

	left = nb_inst;
again:
	burst = left > CPT_LMTST_BURST ? CPT_LMTST_BURST : left;

	lnum = 0;
	loff = 0;
	shft = 16;
	memcpy(PLT_PTR_CAST(lbase), inst, burst * sizeof(struct cpt_inst_s));
	loff = (burst % 2) ? 1 : 0;
	lnum = (burst / 2);
	shft = shft + (lnum * 3);

	left -= burst;
	cn20k_nix_sec_steorl(io_addr, lmt_id, lnum, loff, shft);
	rte_io_wmb();
	if (left) {
		inst = RTE_PTR_ADD(inst, burst * sizeof(struct cpt_inst_s));
		goto again;
	}
	return nb_inst;
}

void
cn20k_eth_sec_ops_override(void)
{
	static int init_once;
	uint32_t idx = 0;

	if (init_once)
		return;
	init_once = 1;

	if (roc_feature_nix_has_inl_ipsec())
		eth_sec_caps_add(cn20k_eth_sec_capabilities, &idx, cn20k_eth_sec_ipsec_capabilities,
				 RTE_DIM(cn20k_eth_sec_ipsec_capabilities));

	cn20k_eth_sec_capabilities[idx].action = RTE_SECURITY_ACTION_TYPE_NONE;

	/* Update platform specific ops */
	cnxk_eth_sec_ops.session_create = cn20k_eth_sec_session_create;
	cnxk_eth_sec_ops.session_destroy = cn20k_eth_sec_session_destroy;
	cnxk_eth_sec_ops.capabilities_get = cn20k_eth_sec_capabilities_get;
	cnxk_eth_sec_ops.session_update = cn20k_eth_sec_session_update;
	cnxk_eth_sec_ops.session_stats_get = cn20k_eth_sec_session_stats_get;

	/* Update platform specific rte_pmd_cnxk ops */
	cnxk_pmd_ops.inl_dev_submit = cn20k_inl_dev_submit;
}
