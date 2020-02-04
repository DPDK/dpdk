/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#include <rte_cryptodev.h>
#include <rte_ethdev.h>
#include <rte_eventdev.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_security.h>
#include <rte_security_driver.h>

#include "otx2_cryptodev_qp.h"
#include "otx2_ethdev.h"
#include "otx2_ethdev_sec.h"
#include "otx2_ipsec_fp.h"
#include "otx2_sec_idev.h"

#define ETH_SEC_MAX_PKT_LEN	1450

struct eth_sec_tag_const {
	RTE_STD_C11
	union {
		struct {
			uint32_t rsvd_11_0  : 12;
			uint32_t port       : 8;
			uint32_t event_type : 4;
			uint32_t rsvd_31_24 : 8;
		};
		uint32_t u32;
	};
};

static struct rte_cryptodev_capabilities otx2_eth_sec_crypto_caps[] = {
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

static const struct rte_security_capability otx2_eth_sec_capabilities[] = {
	{	/* IPsec Inline Protocol ESP Tunnel Ingress */
		.action = RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
			.options = { 0 }
		},
		.crypto_capabilities = otx2_eth_sec_crypto_caps,
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
		.crypto_capabilities = otx2_eth_sec_crypto_caps,
		.ol_flags = RTE_SECURITY_TX_OLOAD_NEED_MDATA
	},
	{
		.action = RTE_SECURITY_ACTION_TYPE_NONE
	}
};

static inline void
in_sa_mz_name_get(char *name, int size, uint16_t port)
{
	snprintf(name, size, "otx2_ipsec_in_sadb_%u", port);
}

static struct otx2_ipsec_fp_in_sa *
in_sa_get(uint16_t port, int sa_index)
{
	char name[RTE_MEMZONE_NAMESIZE];
	struct otx2_ipsec_fp_in_sa *sa;
	const struct rte_memzone *mz;

	in_sa_mz_name_get(name, RTE_MEMZONE_NAMESIZE, port);
	mz = rte_memzone_lookup(name);
	if (mz == NULL) {
		otx2_err("Could not get the memzone reserved for IN SA DB");
		return NULL;
	}

	sa = mz->addr;

	return sa + sa_index;
}

static int
hmac_init(struct otx2_ipsec_fp_sa_ctl *ctl, struct otx2_cpt_qp *qp,
	  const uint8_t *auth_key, int len, uint8_t *hmac_key)
{
	struct inst_data {
		struct otx2_cpt_res cpt_res;
		uint8_t buffer[64];
	} *md;

	volatile struct otx2_cpt_res *res;
	uint64_t timeout, lmt_status;
	struct otx2_cpt_inst_s inst;
	rte_iova_t md_iova;
	int ret;

	memset(&inst, 0, sizeof(struct otx2_cpt_inst_s));

	md = rte_zmalloc(NULL, sizeof(struct inst_data), OTX2_CPT_RES_ALIGN);
	if (md == NULL)
		return -ENOMEM;

	memcpy(md->buffer, auth_key, len);

	md_iova = rte_malloc_virt2iova(md);
	if (md_iova == RTE_BAD_IOVA) {
		ret = -EINVAL;
		goto free_md;
	}

	inst.res_addr = md_iova + offsetof(struct inst_data, cpt_res);
	inst.opcode = OTX2_CPT_OP_WRITE_HMAC_IPAD_OPAD;
	inst.param2 = ctl->auth_type;
	inst.dlen = len;
	inst.dptr = md_iova + offsetof(struct inst_data, buffer);
	inst.rptr = inst.dptr;
	inst.egrp = OTX2_CPT_EGRP_INLINE_IPSEC;

	md->cpt_res.compcode = 0;
	md->cpt_res.uc_compcode = 0xff;

	timeout = rte_get_timer_cycles() + 5 * rte_get_timer_hz();

	rte_cio_wmb();

	do {
		otx2_lmt_mov(qp->lmtline, &inst, 2);
		lmt_status = otx2_lmt_submit(qp->lf_nq_reg);
	} while (lmt_status == 0);

	res = (volatile struct otx2_cpt_res *)&md->cpt_res;

	/* Wait until instruction completes or times out */
	while (res->uc_compcode == 0xff) {
		if (rte_get_timer_cycles() > timeout)
			break;
	}

	if (res->u16[0] != OTX2_SEC_COMP_GOOD) {
		ret = -EIO;
		goto free_md;
	}

	/* Retrieve the ipad and opad from rptr */
	memcpy(hmac_key, md->buffer, 48);

	ret = 0;

free_md:
	rte_free(md);
	return ret;
}

static int
eth_sec_ipsec_out_sess_create(struct rte_eth_dev *eth_dev,
			      struct rte_security_ipsec_xform *ipsec,
			      struct rte_crypto_sym_xform *crypto_xform,
			      struct rte_security_session *sec_sess)
{
	struct rte_crypto_sym_xform *auth_xform, *cipher_xform;
	struct otx2_sec_session_ipsec_ip *sess;
	uint16_t port = eth_dev->data->port_id;
	int cipher_key_len, auth_key_len, ret;
	const uint8_t *cipher_key, *auth_key;
	struct otx2_ipsec_fp_sa_ctl *ctl;
	struct otx2_ipsec_fp_out_sa *sa;
	struct otx2_sec_session *priv;
	struct otx2_cpt_qp *qp;

	priv = get_sec_session_private_data(sec_sess);
	sess = &priv->ipsec.ip;

	sa = &sess->out_sa;
	ctl = &sa->ctl;
	if (ctl->valid) {
		otx2_err("SA already registered");
		return -EINVAL;
	}

	memset(sess, 0, sizeof(struct otx2_sec_session_ipsec_ip));

	if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AEAD)
		memcpy(sa->nonce, &ipsec->salt, 4);

	if (ipsec->options.udp_encap == 1) {
		sa->udp_src = 4500;
		sa->udp_dst = 4500;
	}

	if (ipsec->mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL) {
		if (ipsec->tunnel.type == RTE_SECURITY_IPSEC_TUNNEL_IPV4) {
			memcpy(&sa->ip_src, &ipsec->tunnel.ipv4.src_ip,
			       sizeof(struct in_addr));
			memcpy(&sa->ip_dst, &ipsec->tunnel.ipv4.dst_ip,
			       sizeof(struct in_addr));
		} else {
			return -EINVAL;
		}
	} else {
		return -EINVAL;
	}

	cipher_xform = crypto_xform;
	auth_xform = crypto_xform->next;

	cipher_key_len = 0;
	auth_key_len = 0;
	auth_key = NULL;

	if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		cipher_key = crypto_xform->aead.key.data;
		cipher_key_len = crypto_xform->aead.key.length;
	} else {
		cipher_key = cipher_xform->cipher.key.data;
		cipher_key_len = cipher_xform->cipher.key.length;
		auth_key = auth_xform->auth.key.data;
		auth_key_len = auth_xform->auth.key.length;
	}

	if (cipher_key_len != 0)
		memcpy(sa->cipher_key, cipher_key, cipher_key_len);
	else
		return -EINVAL;

	/* Get CPT QP to be used for this SA */
	ret = otx2_sec_idev_tx_cpt_qp_get(port, &qp);
	if (ret)
		return ret;

	sess->qp = qp;

	sess->cpt_lmtline = qp->lmtline;
	sess->cpt_nq_reg = qp->lf_nq_reg;

	/* Populate control word */
	ret = ipsec_fp_sa_ctl_set(ipsec, crypto_xform, ctl);
	if (ret)
		goto cpt_put;

	if (auth_key_len && auth_key) {
		ret = hmac_init(ctl, qp, auth_key, auth_key_len, sa->hmac_key);
		if (ret)
			goto cpt_put;
	}

	return 0;
cpt_put:
	otx2_sec_idev_tx_cpt_qp_put(sess->qp);
	return ret;
}

static int
eth_sec_ipsec_in_sess_create(struct rte_eth_dev *eth_dev,
			     struct rte_security_ipsec_xform *ipsec,
			     struct rte_crypto_sym_xform *crypto_xform,
			     struct rte_security_session *sec_sess)
{
	struct rte_crypto_sym_xform *auth_xform, *cipher_xform;
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_sec_session_ipsec_ip *sess;
	uint16_t port = eth_dev->data->port_id;
	int cipher_key_len, auth_key_len, ret;
	const uint8_t *cipher_key, *auth_key;
	struct otx2_ipsec_fp_sa_ctl *ctl;
	struct otx2_ipsec_fp_in_sa *sa;
	struct otx2_sec_session *priv;
	struct otx2_cpt_qp *qp;

	if (ipsec->spi >= dev->ipsec_in_max_spi) {
		otx2_err("SPI exceeds max supported");
		return -EINVAL;
	}

	sa = in_sa_get(port, ipsec->spi);
	ctl = &sa->ctl;

	priv = get_sec_session_private_data(sec_sess);
	sess = &priv->ipsec.ip;

	if (ctl->valid) {
		otx2_err("SA already registered");
		return -EINVAL;
	}

	memset(sa, 0, sizeof(struct otx2_ipsec_fp_in_sa));

	auth_xform = crypto_xform;
	cipher_xform = crypto_xform->next;

	cipher_key_len = 0;
	auth_key_len = 0;
	auth_key = NULL;

	if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		if (crypto_xform->aead.algo == RTE_CRYPTO_AEAD_AES_GCM)
			memcpy(sa->nonce, &ipsec->salt, 4);
		cipher_key = crypto_xform->aead.key.data;
		cipher_key_len = crypto_xform->aead.key.length;
	} else {
		cipher_key = cipher_xform->cipher.key.data;
		cipher_key_len = cipher_xform->cipher.key.length;
		auth_key = auth_xform->auth.key.data;
		auth_key_len = auth_xform->auth.key.length;
	}

	if (cipher_key_len != 0)
		memcpy(sa->cipher_key, cipher_key, cipher_key_len);
	else
		return -EINVAL;

	sess->in_sa = sa;

	sa->userdata = priv->userdata;

	ret = ipsec_fp_sa_ctl_set(ipsec, crypto_xform, ctl);
	if (ret)
		return ret;

	if (auth_key_len && auth_key) {
		/* Get a queue pair for HMAC init */
		ret = otx2_sec_idev_tx_cpt_qp_get(port, &qp);
		if (ret)
			return ret;
		ret = hmac_init(ctl, qp, auth_key, auth_key_len, sa->hmac_key);
		otx2_sec_idev_tx_cpt_qp_put(qp);
	}
	return ret;
}

static int
eth_sec_ipsec_sess_create(struct rte_eth_dev *eth_dev,
			  struct rte_security_ipsec_xform *ipsec,
			  struct rte_crypto_sym_xform *crypto_xform,
			  struct rte_security_session *sess)
{
	int ret;

	ret = ipsec_fp_xform_verify(ipsec, crypto_xform);
	if (ret)
		return ret;

	if (ipsec->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS)
		return eth_sec_ipsec_in_sess_create(eth_dev, ipsec,
						    crypto_xform, sess);
	else
		return eth_sec_ipsec_out_sess_create(eth_dev, ipsec,
						     crypto_xform, sess);
}

static int
otx2_eth_sec_session_create(void *device,
			    struct rte_security_session_conf *conf,
			    struct rte_security_session *sess,
			    struct rte_mempool *mempool)
{
	struct otx2_sec_session *priv;
	int ret;

	if (conf->action_type != RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL)
		return -ENOTSUP;

	if (rte_mempool_get(mempool, (void **)&priv)) {
		otx2_err("Could not allocate security session private data");
		return -ENOMEM;
	}

	set_sec_session_private_data(sess, priv);

	/*
	 * Save userdata provided by the application. For ingress packets, this
	 * could be used to identify the SA.
	 */
	priv->userdata = conf->userdata;

	if (conf->protocol == RTE_SECURITY_PROTOCOL_IPSEC)
		ret = eth_sec_ipsec_sess_create(device, &conf->ipsec,
						conf->crypto_xform,
						sess);
	else
		ret = -ENOTSUP;

	if (ret)
		goto mempool_put;

	return 0;

mempool_put:
	rte_mempool_put(mempool, priv);
	set_sec_session_private_data(sess, NULL);
	return ret;
}

static int
otx2_eth_sec_session_destroy(void *device __rte_unused,
			     struct rte_security_session *sess)
{
	struct otx2_sec_session_ipsec_ip *sess_ip;
	struct otx2_sec_session *priv;
	struct rte_mempool *sess_mp;
	int ret;

	priv = get_sec_session_private_data(sess);
	if (priv == NULL)
		return -EINVAL;

	sess_ip = &priv->ipsec.ip;

	/* Release CPT LF used for this session */
	if (sess_ip->qp != NULL) {
		ret = otx2_sec_idev_tx_cpt_qp_put(sess_ip->qp);
		if (ret)
			return ret;
	}

	sess_mp = rte_mempool_from_obj(priv);

	set_sec_session_private_data(sess, NULL);
	rte_mempool_put(sess_mp, priv);

	return 0;
}

static unsigned int
otx2_eth_sec_session_get_size(void *device __rte_unused)
{
	return sizeof(struct otx2_sec_session);
}

static int
otx2_eth_sec_set_pkt_mdata(void *device __rte_unused,
			    struct rte_security_session *session,
			    struct rte_mbuf *m, void *params __rte_unused)
{
	/* Set security session as the pkt metadata */
	m->udata64 = (uint64_t)session;

	return 0;
}

static int
otx2_eth_sec_get_userdata(void *device __rte_unused, uint64_t md,
			   void **userdata)
{
	/* Retrieve userdata  */
	*userdata = (void *)md;

	return 0;
}

static const struct rte_security_capability *
otx2_eth_sec_capabilities_get(void *device __rte_unused)
{
	return otx2_eth_sec_capabilities;
}

static struct rte_security_ops otx2_eth_sec_ops = {
	.session_create		= otx2_eth_sec_session_create,
	.session_destroy	= otx2_eth_sec_session_destroy,
	.session_get_size	= otx2_eth_sec_session_get_size,
	.set_pkt_metadata	= otx2_eth_sec_set_pkt_mdata,
	.get_userdata		= otx2_eth_sec_get_userdata,
	.capabilities_get	= otx2_eth_sec_capabilities_get
};

int
otx2_eth_sec_ctx_create(struct rte_eth_dev *eth_dev)
{
	struct rte_security_ctx *ctx;
	int ret;

	ctx = rte_malloc("otx2_eth_sec_ctx",
			 sizeof(struct rte_security_ctx), 0);
	if (ctx == NULL)
		return -ENOMEM;

	ret = otx2_sec_idev_cfg_init(eth_dev->data->port_id);
	if (ret) {
		rte_free(ctx);
		return ret;
	}

	/* Populate ctx */

	ctx->device = eth_dev;
	ctx->ops = &otx2_eth_sec_ops;
	ctx->sess_cnt = 0;

	eth_dev->security_ctx = ctx;

	return 0;
}

void
otx2_eth_sec_ctx_destroy(struct rte_eth_dev *eth_dev)
{
	rte_free(eth_dev->security_ctx);
}

static int
eth_sec_ipsec_cfg(struct rte_eth_dev *eth_dev, uint8_t tt)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint16_t port = eth_dev->data->port_id;
	struct nix_inline_ipsec_lf_cfg *req;
	struct otx2_mbox *mbox = dev->mbox;
	struct eth_sec_tag_const tag_const;
	char name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;

	in_sa_mz_name_get(name, RTE_MEMZONE_NAMESIZE, port);
	mz = rte_memzone_lookup(name);
	if (mz == NULL)
		return -EINVAL;

	req = otx2_mbox_alloc_msg_nix_inline_ipsec_lf_cfg(mbox);
	req->enable = 1;
	req->sa_base_addr = mz->iova;

	req->ipsec_cfg0.tt = tt;

	tag_const.u32 = 0;
	tag_const.event_type = RTE_EVENT_TYPE_ETHDEV;
	tag_const.port = port;
	req->ipsec_cfg0.tag_const = tag_const.u32;

	req->ipsec_cfg0.sa_pow2_size =
			rte_log2_u32(sizeof(struct otx2_ipsec_fp_in_sa));
	req->ipsec_cfg0.lenm1_max = ETH_SEC_MAX_PKT_LEN - 1;

	req->ipsec_cfg1.sa_idx_w = rte_log2_u32(dev->ipsec_in_max_spi);
	req->ipsec_cfg1.sa_idx_max = dev->ipsec_in_max_spi - 1;

	return otx2_mbox_process(mbox);
}

int
otx2_eth_sec_init(struct rte_eth_dev *eth_dev)
{
	const size_t sa_width = sizeof(struct otx2_ipsec_fp_in_sa);
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint16_t port = eth_dev->data->port_id;
	char name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;
	int mz_sz, ret;
	uint16_t nb_sa;

	RTE_BUILD_BUG_ON(sa_width < 32 || sa_width > 512 ||
			 !RTE_IS_POWER_OF_2(sa_width));

	if (!(dev->tx_offloads & DEV_TX_OFFLOAD_SECURITY) &&
	    !(dev->rx_offloads & DEV_RX_OFFLOAD_SECURITY))
		return 0;

	nb_sa = dev->ipsec_in_max_spi;
	mz_sz = nb_sa * sa_width;
	in_sa_mz_name_get(name, RTE_MEMZONE_NAMESIZE, port);
	mz = rte_memzone_reserve_aligned(name, mz_sz, rte_socket_id(),
					 RTE_MEMZONE_IOVA_CONTIG, OTX2_ALIGN);

	if (mz == NULL) {
		otx2_err("Could not allocate inbound SA DB");
		return -ENOMEM;
	}

	memset(mz->addr, 0, mz_sz);

	ret = eth_sec_ipsec_cfg(eth_dev, SSO_TT_ORDERED);
	if (ret < 0) {
		otx2_err("Could not configure inline IPsec");
		goto sec_fini;
	}

	return 0;

sec_fini:
	otx2_err("Could not configure device for security");
	otx2_eth_sec_fini(eth_dev);
	return ret;
}

void
otx2_eth_sec_fini(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint16_t port = eth_dev->data->port_id;
	char name[RTE_MEMZONE_NAMESIZE];

	if (!(dev->tx_offloads & DEV_TX_OFFLOAD_SECURITY) &&
	    !(dev->rx_offloads & DEV_RX_OFFLOAD_SECURITY))
		return;

	in_sa_mz_name_get(name, RTE_MEMZONE_NAMESIZE, port);
	rte_memzone_free(rte_memzone_lookup(name));
}
