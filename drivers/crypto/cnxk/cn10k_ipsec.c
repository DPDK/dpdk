/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <rte_malloc.h>
#include <cryptodev_pmd.h>
#include <rte_esp.h>
#include <rte_ip.h>
#include <rte_security.h>
#include <rte_security_driver.h>
#include <rte_udp.h>

#include "cnxk_cryptodev.h"
#include "cnxk_ipsec.h"
#include "cnxk_security.h"
#include "cn10k_ipsec.h"

#include "roc_api.h"

static uint64_t
ipsec_cpt_inst_w7_get(struct roc_cpt *roc_cpt, void *sa)
{
	union cpt_inst_w7 w7;

	w7.u64 = 0;
	w7.s.egrp = roc_cpt->eng_grp[CPT_ENG_TYPE_IE];
	w7.s.ctx_val = 1;
	w7.s.cptr = (uint64_t)sa;
	rte_mb();

	return w7.u64;
}

static int
cn10k_ipsec_outb_sa_create(struct roc_cpt *roc_cpt,
			   struct rte_security_ipsec_xform *ipsec_xfrm,
			   struct rte_crypto_sym_xform *crypto_xfrm,
			   struct rte_security_session *sec_sess)
{
	union roc_ot_ipsec_outb_param1 param1;
	struct roc_ot_ipsec_outb_sa *out_sa;
	struct cnxk_ipsec_outb_rlens rlens;
	struct cn10k_sec_session *sess;
	struct cn10k_ipsec_sa *sa;
	union cpt_inst_w4 inst_w4;
	int ret;

	sess = get_sec_session_private_data(sec_sess);
	sa = &sess->sa;
	out_sa = &sa->out_sa;

	memset(out_sa, 0, sizeof(struct roc_ot_ipsec_outb_sa));

	/* Translate security parameters to SA */
	ret = cnxk_ot_ipsec_outb_sa_fill(out_sa, ipsec_xfrm, crypto_xfrm);
	if (ret)
		return ret;

	sa->inst.w7 = ipsec_cpt_inst_w7_get(roc_cpt, sa);

#ifdef LA_IPSEC_DEBUG
	/* Use IV from application in debug mode */
	if (ipsec_xfrm->options.iv_gen_disable == 1) {
		out_sa->w2.s.iv_src = ROC_IE_OT_SA_IV_SRC_FROM_SA;
		if (crypto_xfrm->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
			sa->iv_offset = crypto_xfrm->aead.iv.offset;
			sa->iv_length = crypto_xfrm->aead.iv.length;
		}
	}
#else
	if (ipsec_xfrm->options.iv_gen_disable != 0) {
		plt_err("Application provided IV not supported");
		return -ENOTSUP;
	}
#endif

	/* Get Rlen calculation data */
	ret = cnxk_ipsec_outb_rlens_get(&rlens, ipsec_xfrm, crypto_xfrm);
	if (ret)
		return ret;

	sa->max_extended_len = rlens.max_extended_len;

	/* pre-populate CPT INST word 4 */
	inst_w4.u64 = 0;
	inst_w4.s.opcode_major = ROC_IE_OT_MAJOR_OP_PROCESS_OUTBOUND_IPSEC;

	param1.u16 = 0;

	/* Disable IP checksum computation by default */
	param1.s.ip_csum_disable = ROC_IE_OT_SA_INNER_PKT_IP_CSUM_DISABLE;

	if (ipsec_xfrm->options.ip_csum_enable) {
		param1.s.ip_csum_disable =
			ROC_IE_OT_SA_INNER_PKT_IP_CSUM_ENABLE;
	}

	/* Disable L4 checksum computation by default */
	param1.s.l4_csum_disable = ROC_IE_OT_SA_INNER_PKT_L4_CSUM_DISABLE;

	if (ipsec_xfrm->options.l4_csum_enable) {
		param1.s.l4_csum_disable =
			ROC_IE_OT_SA_INNER_PKT_L4_CSUM_ENABLE;
	}

	inst_w4.s.param1 = param1.u16;

	sa->inst.w4 = inst_w4.u64;

	return 0;
}

static int
cn10k_ipsec_inb_sa_create(struct roc_cpt *roc_cpt,
			  struct rte_security_ipsec_xform *ipsec_xfrm,
			  struct rte_crypto_sym_xform *crypto_xfrm,
			  struct rte_security_session *sec_sess)
{
	union roc_ot_ipsec_inb_param1 param1;
	struct roc_ot_ipsec_inb_sa *in_sa;
	struct cn10k_sec_session *sess;
	struct cn10k_ipsec_sa *sa;
	union cpt_inst_w4 inst_w4;
	int ret;

	sess = get_sec_session_private_data(sec_sess);
	sa = &sess->sa;
	in_sa = &sa->in_sa;

	/* Translate security parameters to SA */
	ret = cnxk_ot_ipsec_inb_sa_fill(in_sa, ipsec_xfrm, crypto_xfrm);
	if (ret)
		return ret;

	/* TODO add support for antireplay */
	sa->in_sa.w0.s.ar_win = 0;

	/* TODO add support for udp encap */

	sa->inst.w7 = ipsec_cpt_inst_w7_get(roc_cpt, sa);

	/* pre-populate CPT INST word 4 */
	inst_w4.u64 = 0;
	inst_w4.s.opcode_major = ROC_IE_OT_MAJOR_OP_PROCESS_INBOUND_IPSEC;

	param1.u16 = 0;

	/* Disable IP checksum verification by default */
	param1.s.ip_csum_disable = ROC_IE_OT_SA_INNER_PKT_IP_CSUM_DISABLE;

	if (ipsec_xfrm->options.ip_csum_enable) {
		param1.s.ip_csum_disable =
			ROC_IE_OT_SA_INNER_PKT_IP_CSUM_ENABLE;
		sa->ip_csum_enable = true;
	}

	/* Disable L4 checksum verification by default */
	param1.s.l4_csum_disable = ROC_IE_OT_SA_INNER_PKT_L4_CSUM_DISABLE;

	if (ipsec_xfrm->options.l4_csum_enable) {
		param1.s.l4_csum_disable =
			ROC_IE_OT_SA_INNER_PKT_L4_CSUM_ENABLE;
	}

	param1.s.esp_trailer_disable = 1;

	inst_w4.s.param1 = param1.u16;

	sa->inst.w4 = inst_w4.u64;

	return 0;
}

static int
cn10k_ipsec_session_create(void *dev,
			   struct rte_security_ipsec_xform *ipsec_xfrm,
			   struct rte_crypto_sym_xform *crypto_xfrm,
			   struct rte_security_session *sess)
{
	struct rte_cryptodev *crypto_dev = dev;
	struct roc_cpt *roc_cpt;
	struct cnxk_cpt_vf *vf;
	int ret;

	vf = crypto_dev->data->dev_private;
	roc_cpt = &vf->cpt;

	if (crypto_dev->data->queue_pairs[0] == NULL) {
		plt_err("Setup cpt queue pair before creating security session");
		return -EPERM;
	}

	ret = cnxk_ipsec_xform_verify(ipsec_xfrm, crypto_xfrm);
	if (ret)
		return ret;

	if (ipsec_xfrm->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS)
		return cn10k_ipsec_inb_sa_create(roc_cpt, ipsec_xfrm,
						 crypto_xfrm, sess);
	else
		return cn10k_ipsec_outb_sa_create(roc_cpt, ipsec_xfrm,
						  crypto_xfrm, sess);
}

static int
cn10k_sec_session_create(void *device, struct rte_security_session_conf *conf,
			 struct rte_security_session *sess,
			 struct rte_mempool *mempool)
{
	struct cn10k_sec_session *priv;
	int ret;

	if (conf->action_type != RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL)
		return -EINVAL;

	if (rte_mempool_get(mempool, (void **)&priv)) {
		plt_err("Could not allocate security session private data");
		return -ENOMEM;
	}

	set_sec_session_private_data(sess, priv);

	if (conf->protocol != RTE_SECURITY_PROTOCOL_IPSEC) {
		ret = -ENOTSUP;
		goto mempool_put;
	}
	ret = cn10k_ipsec_session_create(device, &conf->ipsec,
					 conf->crypto_xform, sess);
	if (ret)
		goto mempool_put;

	return 0;

mempool_put:
	rte_mempool_put(mempool, priv);
	set_sec_session_private_data(sess, NULL);
	return ret;
}

static int
cn10k_sec_session_destroy(void *device __rte_unused,
			  struct rte_security_session *sess)
{
	struct cn10k_sec_session *priv;
	struct rte_mempool *sess_mp;

	priv = get_sec_session_private_data(sess);

	if (priv == NULL)
		return 0;

	sess_mp = rte_mempool_from_obj(priv);

	set_sec_session_private_data(sess, NULL);
	rte_mempool_put(sess_mp, priv);

	return 0;
}

static unsigned int
cn10k_sec_session_get_size(void *device __rte_unused)
{
	return sizeof(struct cn10k_sec_session);
}

/* Update platform specific security ops */
void
cn10k_sec_ops_override(void)
{
	/* Update platform specific ops */
	cnxk_sec_ops.session_create = cn10k_sec_session_create;
	cnxk_sec_ops.session_destroy = cn10k_sec_session_destroy;
	cnxk_sec_ops.session_get_size = cn10k_sec_session_get_size;
}
