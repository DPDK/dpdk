/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <cryptodev_pmd.h>
#include <rte_ip.h>
#include <rte_security.h>
#include <rte_security_driver.h>

#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_ops.h"
#include "cnxk_ipsec.h"
#include "cnxk_security.h"
#include "cn9k_ipsec.h"

#include "roc_api.h"

static int
cn9k_ipsec_outb_sa_create(struct cnxk_cpt_qp *qp,
			  struct rte_security_ipsec_xform *ipsec,
			  struct rte_crypto_sym_xform *crypto_xform,
			  struct rte_security_session *sec_sess)
{
	struct roc_cpt *roc_cpt = qp->lf.roc_cpt;
	union roc_on_ipsec_outb_param1 param1;
	struct cnxk_cpt_inst_tmpl *inst_tmpl;
	struct cn9k_sec_session *sess;
	struct cn9k_ipsec_sa *sa;
	union cpt_inst_w4 w4;
	union cpt_inst_w7 w7;
	size_t ctx_len;
	uint8_t egrp;
	int ret;

	sess = (struct cn9k_sec_session *)sec_sess;
	sa = &sess->sa;

	/* Initialize lookaside IPsec private data */

	memset(sa, 0, sizeof(struct cn9k_ipsec_sa));

	sess->is_outbound = 1;

	if (ipsec->esn.value)
		sess->esn = ipsec->esn.value - 1;

	ret = cnxk_ipsec_outb_rlens_get(&sess->rlens, ipsec, crypto_xform);
	if (ret)
		return ret;

	sess->custom_hdr_len =
		sizeof(struct roc_ie_on_outb_hdr) - ROC_IE_ON_MAX_IV_LEN;

#ifdef LA_IPSEC_DEBUG
	/* Use IV from application in debug mode */
	if (ipsec->options.iv_gen_disable == 1) {
		sess->custom_hdr_len = sizeof(struct roc_ie_on_outb_hdr);

		if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
			sess->cipher_iv_off = crypto_xform->aead.iv.offset;
			sess->cipher_iv_len = crypto_xform->aead.iv.length;
		} else if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
			sess->cipher_iv_off = crypto_xform->cipher.iv.offset;
			sess->cipher_iv_len = crypto_xform->cipher.iv.length;
		} else {
			sess->cipher_iv_off = crypto_xform->auth.iv.offset;
			sess->cipher_iv_len = crypto_xform->auth.iv.length;
		}
	}
#else
	if (ipsec->options.iv_gen_disable != 0) {
		plt_err("Application provided IV is not supported");
		return -ENOTSUP;
	}
#endif

	ret = cnxk_on_ipsec_outb_sa_create(ipsec, crypto_xform, &sa->out_sa);

	if (ret < 0)
		return ret;

	ctx_len = ret;
	egrp = roc_cpt->eng_grp[CPT_ENG_TYPE_IE];

	w4.u64 = 0;
	w4.s.opcode_major = ROC_IE_ON_MAJOR_OP_PROCESS_OUTBOUND_IPSEC | ROC_IE_ON_INPLACE_BIT;
	w4.s.opcode_minor = ctx_len >> 3;

	param1.u16 = 0;
	param1.s.ikev2 = 1;

#ifdef LA_IPSEC_DEBUG
	/* Use IV from application in debug mode */
	if (ipsec->options.iv_gen_disable == 1)
		param1.s.per_pkt_iv = ROC_IE_ON_IV_SRC_FROM_DPTR;
#else
	if (ipsec->options.iv_gen_disable != 0) {
		plt_err("Application provided IV is not supported");
		return -ENOTSUP;
	}
#endif

	w4.s.param1 = param1.u16;

	w7.u64 = 0;
	w7.s.egrp = egrp;
	w7.s.cptr = (uintptr_t)&sess->sa;

	inst_tmpl = &sess->inst;
	inst_tmpl->w4 = w4.u64;
	inst_tmpl->w7 = w7.u64;

	return 0;
}

static int
cn9k_ipsec_inb_sa_create(struct cnxk_cpt_qp *qp,
			 struct rte_security_ipsec_xform *ipsec,
			 struct rte_crypto_sym_xform *crypto_xform,
			 struct rte_security_session *sec_sess)
{
	struct roc_cpt *roc_cpt = qp->lf.roc_cpt;
	struct cnxk_cpt_inst_tmpl *inst_tmpl;
	union roc_on_ipsec_inb_param2 param2;
	struct cn9k_sec_session *sess;
	struct cn9k_ipsec_sa *sa;
	union cpt_inst_w4 w4;
	union cpt_inst_w7 w7;
	size_t ctx_len = 0;
	uint8_t egrp;
	int ret = 0;

	sess = (struct cn9k_sec_session *)sec_sess;
	sa = &sess->sa;

	memset(sa, 0, sizeof(struct cn9k_ipsec_sa));

	sess->is_outbound = 0;
	sess->replay_win_sz = ipsec->replay_win_sz;

	if (sess->replay_win_sz) {
		if (sess->replay_win_sz > CNXK_ON_AR_WIN_SIZE_MAX) {
			plt_err("Replay window size:%u is not supported", sess->replay_win_sz);
			return -ENOTSUP;
		}

		/* Set window bottom to 1, base and top to size of window */
		sess->ar.winb = 1;
		sess->ar.wint = sess->replay_win_sz;
		sess->ar.base = sess->replay_win_sz;

		sess->seq_lo = ipsec->esn.low;
		sess->seq_hi = ipsec->esn.hi;

		sess->sa.in_sa.common_sa.seq_t.tl = sess->seq_lo;
		sess->sa.in_sa.common_sa.seq_t.th = sess->seq_hi;
	}

	ret = cnxk_on_ipsec_inb_sa_create(ipsec, crypto_xform, &sa->in_sa);
	if (ret < 0)
		return ret;

	if (sa->in_sa.common_sa.ctl.esn_en)
		sess->esn_en = 1;

	ctx_len = ret;
	egrp = roc_cpt->eng_grp[CPT_ENG_TYPE_IE];

	w4.u64 = 0;
	w4.s.opcode_major = ROC_IE_ON_MAJOR_OP_PROCESS_INBOUND_IPSEC | ROC_IE_ON_INPLACE_BIT;
	w4.s.opcode_minor = ctx_len >> 3;

	param2.u16 = 0;
	param2.s.ikev2 = 1;
	w4.s.param2 = param2.u16;

	w7.s.egrp = egrp;
	w7.s.cptr = (uintptr_t)&sess->sa;

	inst_tmpl = &sess->inst;
	inst_tmpl->w4 = w4.u64;
	inst_tmpl->w7 = w7.u64;

	return 0;
}

static inline int
cn9k_ipsec_xform_verify(struct rte_security_ipsec_xform *ipsec,
			struct rte_crypto_sym_xform *crypto)
{
	if (ipsec->life.bytes_hard_limit != 0 ||
	    ipsec->life.bytes_soft_limit != 0 ||
	    ipsec->life.packets_hard_limit != 0 ||
	    ipsec->life.packets_soft_limit != 0)
		return -ENOTSUP;

	if (ipsec->mode == RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT &&
	    ipsec->proto != RTE_SECURITY_IPSEC_SA_PROTO_AH) {
		enum rte_crypto_sym_xform_type type = crypto->type;

		if (type == RTE_CRYPTO_SYM_XFORM_AEAD) {
			if ((crypto->aead.algo == RTE_CRYPTO_AEAD_AES_GCM) &&
			    (crypto->aead.key.length == 32)) {
				plt_err("Transport mode AES-256-GCM is not supported");
				return -ENOTSUP;
			}
		}
	}
	return 0;
}

static int
cn9k_ipsec_session_create(void *dev,
			  struct rte_security_ipsec_xform *ipsec_xform,
			  struct rte_crypto_sym_xform *crypto_xform,
			  struct rte_security_session *sess)
{
	struct rte_cryptodev *crypto_dev = dev;
	struct cnxk_cpt_qp *qp;
	int ret;

	qp = crypto_dev->data->queue_pairs[0];
	if (qp == NULL) {
		plt_err("CPT queue pairs need to be setup for creating security"
			" session");
		return -EPERM;
	}

	ret = cnxk_ipsec_xform_verify(ipsec_xform, crypto_xform);
	if (ret)
		return ret;

	ret = cn9k_ipsec_xform_verify(ipsec_xform, crypto_xform);
	if (ret)
		return ret;

	if (ipsec_xform->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS)
		return cn9k_ipsec_inb_sa_create(qp, ipsec_xform, crypto_xform,
						sess);
	else
		return cn9k_ipsec_outb_sa_create(qp, ipsec_xform, crypto_xform,
						 sess);
}

static int
cn9k_sec_session_create(void *device, struct rte_security_session_conf *conf,
			struct rte_security_session *sess)
{
	struct cn9k_sec_session *priv = SECURITY_GET_SESS_PRIV(sess);

	if (conf->action_type != RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL)
		return -EINVAL;

	memset(priv, 0, sizeof(*priv));

	if (conf->protocol != RTE_SECURITY_PROTOCOL_IPSEC)
		return -ENOTSUP;

	return cn9k_ipsec_session_create(device, &conf->ipsec,
					conf->crypto_xform, sess);
}

static int
cn9k_sec_session_destroy(void *device __rte_unused,
			 struct rte_security_session *sess)
{
	struct roc_ie_on_outb_sa *out_sa;
	struct cn9k_sec_session *priv;
	struct roc_ie_on_sa_ctl *ctl;
	struct cn9k_ipsec_sa *sa;

	priv = SECURITY_GET_SESS_PRIV(sess);
	if (priv == NULL)
		return 0;

	sa = &priv->sa;
	out_sa = &sa->out_sa;

	ctl = &out_sa->common_sa.ctl;
	ctl->valid = 0;

	rte_io_wmb();

	memset(priv, 0, sizeof(*priv));

	return 0;
}

static unsigned int
cn9k_sec_session_get_size(void *device __rte_unused)
{
	return sizeof(struct cn9k_sec_session);
}

static int
cn9k_sec_session_update(void *device, struct rte_security_session *sec_sess,
			struct rte_security_session_conf *conf)
{
	struct rte_cryptodev *crypto_dev = device;
	struct cnxk_cpt_qp *qp;
	int ret;

	qp = crypto_dev->data->queue_pairs[0];
	if (qp == NULL) {
		plt_err("CPT queue pairs need to be setup for updating security"
			" session");
		return -EPERM;
	}

	if (conf->ipsec.direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS)
		return -ENOTSUP;

	ret = cnxk_ipsec_xform_verify(&conf->ipsec, conf->crypto_xform);
	if (ret)
		return ret;

	ret = cn9k_ipsec_xform_verify(&conf->ipsec, conf->crypto_xform);
	if (ret)
		return ret;

	return cn9k_ipsec_outb_sa_create(qp, &conf->ipsec, conf->crypto_xform,
					 sec_sess);
}

/* Update platform specific security ops */
void
cn9k_sec_ops_override(void)
{
	/* Update platform specific ops */
	cnxk_sec_ops.session_create = cn9k_sec_session_create;
	cnxk_sec_ops.session_destroy = cn9k_sec_session_destroy;
	cnxk_sec_ops.session_get_size = cn9k_sec_session_get_size;
	cnxk_sec_ops.session_update = cn9k_sec_session_update;
}
