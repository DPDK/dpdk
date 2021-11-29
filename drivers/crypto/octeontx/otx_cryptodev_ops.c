/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#include <rte_alarm.h>
#include <rte_bus_pci.h>
#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include <rte_eventdev.h>
#include <rte_event_crypto_adapter.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_mempool.h>

#include "otx_cryptodev.h"
#include "otx_cryptodev_capabilities.h"
#include "otx_cryptodev_hw_access.h"
#include "otx_cryptodev_mbox.h"
#include "otx_cryptodev_ops.h"

#include "cpt_pmd_logs.h"
#include "cpt_pmd_ops_helper.h"
#include "cpt_ucode.h"
#include "cpt_ucode_asym.h"

#include "ssovf_worker.h"

static uint64_t otx_fpm_iova[CPT_EC_ID_PMAX];

/* Forward declarations */

static int
otx_cpt_que_pair_release(struct rte_cryptodev *dev, uint16_t que_pair_id);

/* Alarm routines */

static void
otx_cpt_alarm_cb(void *arg)
{
	struct cpt_vf *cptvf = arg;
	otx_cpt_poll_misc(cptvf);
	rte_eal_alarm_set(CPT_INTR_POLL_INTERVAL_MS * 1000,
			  otx_cpt_alarm_cb, cptvf);
}

static int
otx_cpt_periodic_alarm_start(void *arg)
{
	return rte_eal_alarm_set(CPT_INTR_POLL_INTERVAL_MS * 1000,
				 otx_cpt_alarm_cb, arg);
}

static int
otx_cpt_periodic_alarm_stop(void *arg)
{
	return rte_eal_alarm_cancel(otx_cpt_alarm_cb, arg);
}

/* PMD ops */

static int
otx_cpt_dev_config(struct rte_cryptodev *dev,
		   struct rte_cryptodev_config *config __rte_unused)
{
	int ret = 0;

	CPT_PMD_INIT_FUNC_TRACE();

	if (dev->feature_flags & RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO)
		/* Initialize shared FPM table */
		ret = cpt_fpm_init(otx_fpm_iova);

	return ret;
}

static int
otx_cpt_dev_start(struct rte_cryptodev *c_dev)
{
	void *cptvf = c_dev->data->dev_private;

	CPT_PMD_INIT_FUNC_TRACE();

	return otx_cpt_start_device(cptvf);
}

static void
otx_cpt_dev_stop(struct rte_cryptodev *c_dev)
{
	void *cptvf = c_dev->data->dev_private;

	CPT_PMD_INIT_FUNC_TRACE();

	if (c_dev->feature_flags & RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO)
		cpt_fpm_clear();

	otx_cpt_stop_device(cptvf);
}

static int
otx_cpt_dev_close(struct rte_cryptodev *c_dev)
{
	void *cptvf = c_dev->data->dev_private;
	int i, ret;

	CPT_PMD_INIT_FUNC_TRACE();

	for (i = 0; i < c_dev->data->nb_queue_pairs; i++) {
		ret = otx_cpt_que_pair_release(c_dev, i);
		if (ret)
			return ret;
	}

	otx_cpt_periodic_alarm_stop(cptvf);
	otx_cpt_deinit_device(cptvf);

	return 0;
}

static void
otx_cpt_dev_info_get(struct rte_cryptodev *dev, struct rte_cryptodev_info *info)
{
	CPT_PMD_INIT_FUNC_TRACE();
	if (info != NULL) {
		info->max_nb_queue_pairs = CPT_NUM_QS_PER_VF;
		info->feature_flags = dev->feature_flags;
		info->capabilities = otx_get_capabilities(info->feature_flags);
		info->sym.max_nb_sessions = 0;
		info->driver_id = otx_cryptodev_driver_id;
		info->min_mbuf_headroom_req = OTX_CPT_MIN_HEADROOM_REQ;
		info->min_mbuf_tailroom_req = OTX_CPT_MIN_TAILROOM_REQ;
	}
}

static int
otx_cpt_que_pair_setup(struct rte_cryptodev *dev,
		       uint16_t que_pair_id,
		       const struct rte_cryptodev_qp_conf *qp_conf,
		       int socket_id __rte_unused)
{
	struct cpt_instance *instance = NULL;
	struct rte_pci_device *pci_dev;
	int ret = -1;

	CPT_PMD_INIT_FUNC_TRACE();

	if (dev->data->queue_pairs[que_pair_id] != NULL) {
		ret = otx_cpt_que_pair_release(dev, que_pair_id);
		if (ret)
			return ret;
	}

	if (qp_conf->nb_descriptors > DEFAULT_CMD_QLEN) {
		CPT_LOG_INFO("Number of descriptors too big %d, using default "
			     "queue length of %d", qp_conf->nb_descriptors,
			     DEFAULT_CMD_QLEN);
	}

	pci_dev = RTE_DEV_TO_PCI(dev->device);

	if (pci_dev->mem_resource[0].addr == NULL) {
		CPT_LOG_ERR("PCI mem address null");
		return -EIO;
	}

	ret = otx_cpt_get_resource(dev, 0, &instance, que_pair_id);
	if (ret != 0 || instance == NULL) {
		CPT_LOG_ERR("Error getting instance handle from device %s : "
			    "ret = %d", dev->data->name, ret);
		return ret;
	}

	instance->queue_id = que_pair_id;
	instance->sess_mp = qp_conf->mp_session;
	instance->sess_mp_priv = qp_conf->mp_session_private;
	dev->data->queue_pairs[que_pair_id] = instance;

	return 0;
}

static int
otx_cpt_que_pair_release(struct rte_cryptodev *dev, uint16_t que_pair_id)
{
	struct cpt_instance *instance = dev->data->queue_pairs[que_pair_id];
	int ret;

	CPT_PMD_INIT_FUNC_TRACE();

	ret = otx_cpt_put_resource(instance);
	if (ret != 0) {
		CPT_LOG_ERR("Error putting instance handle of device %s : "
			    "ret = %d", dev->data->name, ret);
		return ret;
	}

	dev->data->queue_pairs[que_pair_id] = NULL;

	return 0;
}

static unsigned int
otx_cpt_get_session_size(struct rte_cryptodev *dev __rte_unused)
{
	return cpt_get_session_size();
}

static int
sym_xform_verify(struct rte_crypto_sym_xform *xform)
{
	if (xform->next) {
		if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
		    xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
		    xform->next->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT &&
		    (xform->auth.algo != RTE_CRYPTO_AUTH_SHA1_HMAC ||
		     xform->next->cipher.algo != RTE_CRYPTO_CIPHER_AES_CBC))
			return -ENOTSUP;

		if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
		    xform->cipher.op == RTE_CRYPTO_CIPHER_OP_DECRYPT &&
		    xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
		    (xform->cipher.algo != RTE_CRYPTO_CIPHER_AES_CBC ||
		     xform->next->auth.algo != RTE_CRYPTO_AUTH_SHA1_HMAC))
			return -ENOTSUP;

		if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
		    xform->cipher.algo == RTE_CRYPTO_CIPHER_3DES_CBC &&
		    xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
		    xform->next->auth.algo == RTE_CRYPTO_AUTH_SHA1)
			return -ENOTSUP;

		if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
		    xform->auth.algo == RTE_CRYPTO_AUTH_SHA1 &&
		    xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
		    xform->next->cipher.algo == RTE_CRYPTO_CIPHER_3DES_CBC)
			return -ENOTSUP;

	} else {
		if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
		    xform->auth.algo == RTE_CRYPTO_AUTH_NULL &&
		    xform->auth.op == RTE_CRYPTO_AUTH_OP_VERIFY)
			return -ENOTSUP;
	}
	return 0;
}

static int
sym_session_configure(int driver_id, struct rte_crypto_sym_xform *xform,
		      struct rte_cryptodev_sym_session *sess,
		      struct rte_mempool *pool)
{
	struct rte_crypto_sym_xform *temp_xform = xform;
	struct cpt_sess_misc *misc;
	vq_cmd_word3_t vq_cmd_w3;
	void *priv;
	int ret;

	ret = sym_xform_verify(xform);
	if (unlikely(ret))
		return ret;

	if (unlikely(rte_mempool_get(pool, &priv))) {
		CPT_LOG_ERR("Could not allocate session private data");
		return -ENOMEM;
	}

	memset(priv, 0, sizeof(struct cpt_sess_misc) +
			offsetof(struct cpt_ctx, mc_ctx));

	misc = priv;

	for ( ; xform != NULL; xform = xform->next) {
		switch (xform->type) {
		case RTE_CRYPTO_SYM_XFORM_AEAD:
			ret = fill_sess_aead(xform, misc);
			break;
		case RTE_CRYPTO_SYM_XFORM_CIPHER:
			ret = fill_sess_cipher(xform, misc);
			break;
		case RTE_CRYPTO_SYM_XFORM_AUTH:
			if (xform->auth.algo == RTE_CRYPTO_AUTH_AES_GMAC)
				ret = fill_sess_gmac(xform, misc);
			else
				ret = fill_sess_auth(xform, misc);
			break;
		default:
			ret = -1;
		}

		if (ret)
			goto priv_put;
	}

	if ((GET_SESS_FC_TYPE(misc) == HASH_HMAC) &&
			cpt_mac_len_verify(&temp_xform->auth)) {
		CPT_LOG_ERR("MAC length is not supported");
		struct cpt_ctx *ctx = SESS_PRIV(misc);
		if (ctx->auth_key != NULL) {
			rte_free(ctx->auth_key);
			ctx->auth_key = NULL;
		}
		ret = -ENOTSUP;
		goto priv_put;
	}

	set_sym_session_private_data(sess, driver_id, priv);

	misc->ctx_dma_addr = rte_mempool_virt2iova(misc) +
			     sizeof(struct cpt_sess_misc);

	vq_cmd_w3.u64 = 0;
	vq_cmd_w3.s.grp = 0;
	vq_cmd_w3.s.cptr = misc->ctx_dma_addr + offsetof(struct cpt_ctx,
							 mc_ctx);

	misc->cpt_inst_w7 = vq_cmd_w3.u64;

	return 0;

priv_put:
	if (priv)
		rte_mempool_put(pool, priv);
	return -ENOTSUP;
}

static void
sym_session_clear(int driver_id, struct rte_cryptodev_sym_session *sess)
{
	void *priv = get_sym_session_private_data(sess, driver_id);
	struct cpt_sess_misc *misc;
	struct rte_mempool *pool;
	struct cpt_ctx *ctx;

	if (priv == NULL)
		return;

	misc = priv;
	ctx = SESS_PRIV(misc);

	if (ctx->auth_key != NULL)
		rte_free(ctx->auth_key);

	memset(priv, 0, cpt_get_session_size());

	pool = rte_mempool_from_obj(priv);

	set_sym_session_private_data(sess, driver_id, NULL);

	rte_mempool_put(pool, priv);
}

static int
otx_cpt_session_cfg(struct rte_cryptodev *dev,
		    struct rte_crypto_sym_xform *xform,
		    struct rte_cryptodev_sym_session *sess,
		    struct rte_mempool *pool)
{
	CPT_PMD_INIT_FUNC_TRACE();

	return sym_session_configure(dev->driver_id, xform, sess, pool);
}


static void
otx_cpt_session_clear(struct rte_cryptodev *dev,
		  struct rte_cryptodev_sym_session *sess)
{
	CPT_PMD_INIT_FUNC_TRACE();

	return sym_session_clear(dev->driver_id, sess);
}

static unsigned int
otx_cpt_asym_session_size_get(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct cpt_asym_sess_misc);
}

static int
otx_cpt_asym_session_cfg(struct rte_cryptodev *dev,
			 struct rte_crypto_asym_xform *xform __rte_unused,
			 struct rte_cryptodev_asym_session *sess,
			 struct rte_mempool *pool)
{
	struct cpt_asym_sess_misc *priv;
	int ret;

	CPT_PMD_INIT_FUNC_TRACE();

	if (rte_mempool_get(pool, (void **)&priv)) {
		CPT_LOG_ERR("Could not allocate session private data");
		return -ENOMEM;
	}

	memset(priv, 0, sizeof(struct cpt_asym_sess_misc));

	ret = cpt_fill_asym_session_parameters(priv, xform);
	if (ret) {
		CPT_LOG_ERR("Could not configure session parameters");

		/* Return session to mempool */
		rte_mempool_put(pool, priv);
		return ret;
	}

	priv->cpt_inst_w7 = 0;

	set_asym_session_private_data(sess, dev->driver_id, priv);
	return 0;
}

static void
otx_cpt_asym_session_clear(struct rte_cryptodev *dev,
			   struct rte_cryptodev_asym_session *sess)
{
	struct cpt_asym_sess_misc *priv;
	struct rte_mempool *sess_mp;

	CPT_PMD_INIT_FUNC_TRACE();

	priv = get_asym_session_private_data(sess, dev->driver_id);

	if (priv == NULL)
		return;

	/* Free resources allocated during session configure */
	cpt_free_asym_session_parameters(priv);
	memset(priv, 0, otx_cpt_asym_session_size_get(dev));
	sess_mp = rte_mempool_from_obj(priv);
	set_asym_session_private_data(sess, dev->driver_id, NULL);
	rte_mempool_put(sess_mp, priv);
}

static __rte_always_inline void * __rte_hot
otx_cpt_request_enqueue(struct cpt_instance *instance,
			void *req, uint64_t cpt_inst_w7)
{
	struct cpt_request_info *user_req = (struct cpt_request_info *)req;

	fill_cpt_inst(instance, req, cpt_inst_w7);

	CPT_LOG_DP_DEBUG("req: %p op: %p ", req, user_req->op);

	/* Fill time_out cycles */
	user_req->time_out = rte_get_timer_cycles() +
			DEFAULT_COMMAND_TIMEOUT * rte_get_timer_hz();
	user_req->extra_time = 0;

	/* Default mode of software queue */
	mark_cpt_inst(instance);

	CPT_LOG_DP_DEBUG("Submitted NB cmd with request: %p "
			 "op: %p", user_req, user_req->op);
	return req;
}

static __rte_always_inline void * __rte_hot
otx_cpt_enq_single_asym(struct cpt_instance *instance,
			struct rte_crypto_op *op)
{
	struct cpt_qp_meta_info *minfo = &instance->meta_info;
	struct rte_crypto_asym_op *asym_op = op->asym;
	struct asym_op_params params = {0};
	struct cpt_asym_sess_misc *sess;
	uintptr_t *cop;
	void *mdata;
	void *req;
	int ret;

	if (unlikely(rte_mempool_get(minfo->pool, &mdata) < 0)) {
		CPT_LOG_DP_ERR("Could not allocate meta buffer for request");
		rte_errno = ENOMEM;
		return NULL;
	}

	sess = get_asym_session_private_data(asym_op->session,
					     otx_cryptodev_driver_id);

	/* Store phys_addr of the mdata to meta_buf */
	params.meta_buf = rte_mempool_virt2iova(mdata);

	cop = mdata;
	cop[0] = (uintptr_t)mdata;
	cop[1] = (uintptr_t)op;
	cop[2] = cop[3] = 0ULL;

	params.req = RTE_PTR_ADD(cop, 4 * sizeof(uintptr_t));
	params.req->op = cop;

	/* Adjust meta_buf by crypto_op data  and request_info struct */
	params.meta_buf += (4 * sizeof(uintptr_t)) +
			   sizeof(struct cpt_request_info);

	switch (sess->xfrm_type) {
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		ret = cpt_modex_prep(&params, &sess->mod_ctx);
		if (unlikely(ret))
			goto req_fail;
		break;
	case RTE_CRYPTO_ASYM_XFORM_RSA:
		ret = cpt_enqueue_rsa_op(op, &params, sess);
		if (unlikely(ret))
			goto req_fail;
		break;
	case RTE_CRYPTO_ASYM_XFORM_ECDSA:
		ret = cpt_enqueue_ecdsa_op(op, &params, sess, otx_fpm_iova);
		if (unlikely(ret))
			goto req_fail;
		break;
	case RTE_CRYPTO_ASYM_XFORM_ECPM:
		ret = cpt_ecpm_prep(&asym_op->ecpm, &params,
				    sess->ec_ctx.curveid);
		if (unlikely(ret))
			goto req_fail;
		break;

	default:
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		rte_errno = EINVAL;
		goto req_fail;
	}

	req = otx_cpt_request_enqueue(instance, params.req, sess->cpt_inst_w7);
	if (unlikely(req == NULL)) {
		CPT_LOG_DP_ERR("Could not enqueue crypto req");
		goto req_fail;
	}

	return req;

req_fail:
	free_op_meta(mdata, minfo->pool);

	return NULL;
}

static __rte_always_inline void * __rte_hot
otx_cpt_enq_single_sym(struct cpt_instance *instance,
		       struct rte_crypto_op *op)
{
	struct cpt_sess_misc *sess;
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct cpt_request_info *prep_req;
	void *mdata = NULL;
	int ret = 0;
	void *req;
	uint64_t cpt_op;

	sess = (struct cpt_sess_misc *)
			get_sym_session_private_data(sym_op->session,
						     otx_cryptodev_driver_id);

	cpt_op = sess->cpt_op;

	if (likely(cpt_op & CPT_OP_CIPHER_MASK))
		ret = fill_fc_params(op, sess, &instance->meta_info, &mdata,
				     (void **)&prep_req);
	else
		ret = fill_digest_params(op, sess, &instance->meta_info,
					 &mdata, (void **)&prep_req);

	if (unlikely(ret)) {
		CPT_LOG_DP_ERR("prep crypto req : op %p, cpt_op 0x%x "
			       "ret 0x%x", op, (unsigned int)cpt_op, ret);
		return NULL;
	}

	/* Enqueue prepared instruction to h/w */
	req = otx_cpt_request_enqueue(instance, prep_req, sess->cpt_inst_w7);
	if (unlikely(req == NULL))
		/* Buffer allocated for request preparation need to be freed */
		free_op_meta(mdata, instance->meta_info.pool);

	return req;
}

static __rte_always_inline void * __rte_hot
otx_cpt_enq_single_sym_sessless(struct cpt_instance *instance,
				struct rte_crypto_op *op)
{
	const int driver_id = otx_cryptodev_driver_id;
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct rte_cryptodev_sym_session *sess;
	void *req;
	int ret;

	/* Create temporary session */
	sess = rte_cryptodev_sym_session_create(instance->sess_mp);
	if (sess == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}

	ret = sym_session_configure(driver_id, sym_op->xform, sess,
				    instance->sess_mp_priv);
	if (ret)
		goto sess_put;

	sym_op->session = sess;

	/* Enqueue op with the tmp session set */
	req = otx_cpt_enq_single_sym(instance, op);
	if (unlikely(req == NULL))
		goto priv_put;

	return req;

priv_put:
	sym_session_clear(driver_id, sess);
sess_put:
	rte_mempool_put(instance->sess_mp, sess);
	return NULL;
}

#define OP_TYPE_SYM		0
#define OP_TYPE_ASYM		1

static __rte_always_inline void *__rte_hot
otx_cpt_enq_single(struct cpt_instance *inst,
		   struct rte_crypto_op *op,
		   const uint8_t op_type)
{
	/* Check for the type */

	if (op_type == OP_TYPE_SYM) {
		if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION)
			return otx_cpt_enq_single_sym(inst, op);
		else
			return otx_cpt_enq_single_sym_sessless(inst, op);
	}

	if (op_type == OP_TYPE_ASYM) {
		if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION)
			return otx_cpt_enq_single_asym(inst, op);
	}

	/* Should not reach here */
	rte_errno = ENOTSUP;
	return NULL;
}

static  __rte_always_inline uint16_t __rte_hot
otx_cpt_pkt_enqueue(void *qptr, struct rte_crypto_op **ops, uint16_t nb_ops,
		    const uint8_t op_type)
{
	struct cpt_instance *instance = (struct cpt_instance *)qptr;
	uint16_t count, free_slots;
	void *req;
	struct cpt_vf *cptvf = (struct cpt_vf *)instance;
	struct pending_queue *pqueue = &cptvf->pqueue;

	free_slots = pending_queue_free_slots(pqueue, DEFAULT_CMD_QLEN,
				DEFAULT_CMD_QRSVD_SLOTS);
	if (nb_ops > free_slots)
		nb_ops = free_slots;

	count = 0;
	while (likely(count < nb_ops)) {

		/* Enqueue single op */
		req = otx_cpt_enq_single(instance, ops[count], op_type);

		if (unlikely(req == NULL))
			break;

		pending_queue_push(pqueue, req, count, DEFAULT_CMD_QLEN);
		count++;
	}

	if (likely(count)) {
		pending_queue_commit(pqueue, count, DEFAULT_CMD_QLEN);
		otx_cpt_ring_dbell(instance, count);
	}
	return count;
}

static uint16_t
otx_cpt_enqueue_asym(void *qptr, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	return otx_cpt_pkt_enqueue(qptr, ops, nb_ops, OP_TYPE_ASYM);
}

static uint16_t
otx_cpt_enqueue_sym(void *qptr, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	return otx_cpt_pkt_enqueue(qptr, ops, nb_ops, OP_TYPE_SYM);
}

static __rte_always_inline void
submit_request_to_sso(struct ssows *ws, uintptr_t req,
		      struct rte_event *rsp_info)
{
	uint64_t add_work;

	add_work = rsp_info->flow_id | (RTE_EVENT_TYPE_CRYPTODEV << 28) |
		   ((uint64_t)(rsp_info->sched_type) << 32);

	if (!rsp_info->sched_type)
		ssows_head_wait(ws);

	rte_atomic_thread_fence(__ATOMIC_RELEASE);
	ssovf_store_pair(add_work, req, ws->grps[rsp_info->queue_id]);
}

static inline union rte_event_crypto_metadata *
get_event_crypto_mdata(struct rte_crypto_op *op)
{
	union rte_event_crypto_metadata *ec_mdata;

	if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION)
		ec_mdata = rte_cryptodev_sym_session_get_user_data(
							   op->sym->session);
	else if (op->sess_type == RTE_CRYPTO_OP_SESSIONLESS &&
		 op->private_data_offset)
		ec_mdata = (union rte_event_crypto_metadata *)
			((uint8_t *)op + op->private_data_offset);
	else
		return NULL;

	return ec_mdata;
}

uint16_t __rte_hot
otx_crypto_adapter_enqueue(void *port, struct rte_crypto_op *op)
{
	union rte_event_crypto_metadata *ec_mdata;
	struct cpt_instance *instance;
	struct cpt_request_info *req;
	struct rte_event *rsp_info;
	uint8_t op_type, cdev_id;
	uint16_t qp_id;

	ec_mdata = get_event_crypto_mdata(op);
	if (unlikely(ec_mdata == NULL)) {
		rte_errno = EINVAL;
		return 0;
	}

	cdev_id = ec_mdata->request_info.cdev_id;
	qp_id = ec_mdata->request_info.queue_pair_id;
	rsp_info = &ec_mdata->response_info;
	instance = rte_cryptodevs[cdev_id].data->queue_pairs[qp_id];

	if (unlikely(!instance->ca_enabled)) {
		rte_errno = EINVAL;
		return 0;
	}

	op_type = op->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC ? OP_TYPE_SYM :
							     OP_TYPE_ASYM;
	req = otx_cpt_enq_single(instance, op, op_type);
	if (unlikely(req == NULL))
		return 0;

	otx_cpt_ring_dbell(instance, 1);
	req->qp = instance;
	submit_request_to_sso(port, (uintptr_t)req, rsp_info);

	return 1;
}

static inline void
otx_cpt_asym_rsa_op(struct rte_crypto_op *cop, struct cpt_request_info *req,
		    struct rte_crypto_rsa_xform *rsa_ctx)

{
	struct rte_crypto_rsa_op_param *rsa = &cop->asym->rsa;

	switch (rsa->op_type) {
	case RTE_CRYPTO_ASYM_OP_ENCRYPT:
		rsa->cipher.length = rsa_ctx->n.length;
		memcpy(rsa->cipher.data, req->rptr, rsa->cipher.length);
		break;
	case RTE_CRYPTO_ASYM_OP_DECRYPT:
		if (rsa->pad == RTE_CRYPTO_RSA_PADDING_NONE)
			rsa->message.length = rsa_ctx->n.length;
		else {
			/* Get length of decrypted output */
			rsa->message.length = rte_cpu_to_be_16
					(*((uint16_t *)req->rptr));

			/* Offset data pointer by length fields */
			req->rptr += 2;
		}
		memcpy(rsa->message.data, req->rptr, rsa->message.length);
		break;
	case RTE_CRYPTO_ASYM_OP_SIGN:
		rsa->sign.length = rsa_ctx->n.length;
		memcpy(rsa->sign.data, req->rptr, rsa->sign.length);
		break;
	case RTE_CRYPTO_ASYM_OP_VERIFY:
		if (rsa->pad == RTE_CRYPTO_RSA_PADDING_NONE)
			rsa->sign.length = rsa_ctx->n.length;
		else {
			/* Get length of decrypted output */
			rsa->sign.length = rte_cpu_to_be_16
					(*((uint16_t *)req->rptr));

			/* Offset data pointer by length fields */
			req->rptr += 2;
		}
		memcpy(rsa->sign.data, req->rptr, rsa->sign.length);

		if (memcmp(rsa->sign.data, rsa->message.data,
			   rsa->message.length)) {
			CPT_LOG_DP_ERR("RSA verification failed");
			cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
		}
		break;
	default:
		CPT_LOG_DP_DEBUG("Invalid RSA operation type");
		cop->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		break;
	}
}

static __rte_always_inline void
otx_cpt_asym_dequeue_ecdsa_op(struct rte_crypto_ecdsa_op_param *ecdsa,
			    struct cpt_request_info *req,
			    struct cpt_asym_ec_ctx *ec)

{
	int prime_len = ec_grp[ec->curveid].prime.length;

	if (ecdsa->op_type == RTE_CRYPTO_ASYM_OP_VERIFY)
		return;

	/* Separate out sign r and s components */
	memcpy(ecdsa->r.data, req->rptr, prime_len);
	memcpy(ecdsa->s.data, req->rptr + RTE_ALIGN_CEIL(prime_len, 8),
	       prime_len);
	ecdsa->r.length = prime_len;
	ecdsa->s.length = prime_len;
}

static __rte_always_inline void
otx_cpt_asym_dequeue_ecpm_op(struct rte_crypto_ecpm_op_param *ecpm,
			     struct cpt_request_info *req,
			     struct cpt_asym_ec_ctx *ec)
{
	int prime_len = ec_grp[ec->curveid].prime.length;

	memcpy(ecpm->r.x.data, req->rptr, prime_len);
	memcpy(ecpm->r.y.data, req->rptr + RTE_ALIGN_CEIL(prime_len, 8),
	       prime_len);
	ecpm->r.x.length = prime_len;
	ecpm->r.y.length = prime_len;
}

static __rte_always_inline void __rte_hot
otx_cpt_asym_post_process(struct rte_crypto_op *cop,
			  struct cpt_request_info *req)
{
	struct rte_crypto_asym_op *op = cop->asym;
	struct cpt_asym_sess_misc *sess;

	sess = get_asym_session_private_data(op->session,
					     otx_cryptodev_driver_id);

	switch (sess->xfrm_type) {
	case RTE_CRYPTO_ASYM_XFORM_RSA:
		otx_cpt_asym_rsa_op(cop, req, &sess->rsa_ctx);
		break;
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		op->modex.result.length = sess->mod_ctx.modulus.length;
		memcpy(op->modex.result.data, req->rptr,
		       op->modex.result.length);
		break;
	case RTE_CRYPTO_ASYM_XFORM_ECDSA:
		otx_cpt_asym_dequeue_ecdsa_op(&op->ecdsa, req, &sess->ec_ctx);
		break;
	case RTE_CRYPTO_ASYM_XFORM_ECPM:
		otx_cpt_asym_dequeue_ecpm_op(&op->ecpm, req, &sess->ec_ctx);
		break;
	default:
		CPT_LOG_DP_DEBUG("Invalid crypto xform type");
		cop->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		break;
	}
}

static __rte_always_inline void __rte_hot
otx_cpt_dequeue_post_process(struct rte_crypto_op *cop, uintptr_t *rsp,
			     const uint8_t op_type)
{
	/* H/w has returned success */
	cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

	/* Perform further post processing */

	if ((op_type == OP_TYPE_SYM) &&
	    (cop->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC)) {
		/* Check if auth verify need to be completed */
		if (unlikely(rsp[2]))
			compl_auth_verify(cop, (uint8_t *)rsp[2], rsp[3]);
		return;
	}

	if ((op_type == OP_TYPE_ASYM) &&
	    (cop->type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC)) {
		rsp = RTE_PTR_ADD(rsp, 4 * sizeof(uintptr_t));
		otx_cpt_asym_post_process(cop, (struct cpt_request_info *)rsp);
	}

	return;
}

static inline void
free_sym_session_data(const struct cpt_instance *instance,
		      struct rte_crypto_op *cop)
{
	void *sess_private_data_t = get_sym_session_private_data(
		cop->sym->session, otx_cryptodev_driver_id);
	memset(sess_private_data_t, 0, cpt_get_session_size());
	memset(cop->sym->session, 0,
	       rte_cryptodev_sym_get_existing_header_session_size(
		       cop->sym->session));
	rte_mempool_put(instance->sess_mp_priv, sess_private_data_t);
	rte_mempool_put(instance->sess_mp, cop->sym->session);
	cop->sym->session = NULL;
}

static __rte_always_inline struct rte_crypto_op *
otx_cpt_process_response(const struct cpt_instance *instance, uintptr_t *rsp,
			 uint8_t cc, const uint8_t op_type)
{
	struct rte_crypto_op *cop;
	void *metabuf;

	metabuf = (void *)rsp[0];
	cop = (void *)rsp[1];

	/* Check completion code */
	if (likely(cc == 0)) {
		/* H/w success pkt. Post process */
		otx_cpt_dequeue_post_process(cop, rsp, op_type);
	} else if (cc == ERR_GC_ICV_MISCOMPARE) {
		/* auth data mismatch */
		cop->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
	} else {
		/* Error */
		cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
	}

	if (unlikely(cop->sess_type == RTE_CRYPTO_OP_SESSIONLESS))
		free_sym_session_data(instance, cop);
	free_op_meta(metabuf, instance->meta_info.pool);

	return cop;
}

static __rte_always_inline uint16_t __rte_hot
otx_cpt_pkt_dequeue(void *qptr, struct rte_crypto_op **ops, uint16_t nb_ops,
		    const uint8_t op_type)
{
	struct cpt_instance *instance = (struct cpt_instance *)qptr;
	struct cpt_request_info *user_req;
	struct cpt_vf *cptvf = (struct cpt_vf *)instance;
	uint8_t cc[nb_ops];
	int i, count, pcount;
	uint8_t ret;
	int nb_completed;
	struct pending_queue *pqueue = &cptvf->pqueue;

	pcount = pending_queue_level(pqueue, DEFAULT_CMD_QLEN);

	/* Ensure pcount isn't read before data lands */
	rte_atomic_thread_fence(__ATOMIC_ACQUIRE);

	count = (nb_ops > pcount) ? pcount : nb_ops;

	for (i = 0; i < count; i++) {
		pending_queue_peek(pqueue, (void **) &user_req,
			DEFAULT_CMD_QLEN, i + 1 < count);

		ret = check_nb_command_id(user_req, instance);

		if (unlikely(ret == ERR_REQ_PENDING)) {
			/* Stop checking for completions */
			break;
		}

		/* Return completion code and op handle */
		cc[i] = ret;
		ops[i] = user_req->op;

		CPT_LOG_DP_DEBUG("Request %p Op %p completed with code %d",
				 user_req, user_req->op, ret);

		pending_queue_pop(pqueue, DEFAULT_CMD_QLEN);
	}

	nb_completed = i;

	for (i = 0; i < nb_completed; i++) {
		if (likely((i + 1) < nb_completed))
			rte_prefetch0(ops[i+1]);

		ops[i] = otx_cpt_process_response(instance, (void *)ops[i],
						  cc[i], op_type);
	}

	return nb_completed;
}

static uint16_t
otx_cpt_dequeue_asym(void *qptr, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	return otx_cpt_pkt_dequeue(qptr, ops, nb_ops, OP_TYPE_ASYM);
}

static uint16_t
otx_cpt_dequeue_sym(void *qptr, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	return otx_cpt_pkt_dequeue(qptr, ops, nb_ops, OP_TYPE_SYM);
}

uintptr_t __rte_hot
otx_crypto_adapter_dequeue(uintptr_t get_work1)
{
	const struct cpt_instance *instance;
	struct cpt_request_info *req;
	struct rte_crypto_op *cop;
	uint8_t cc, op_type;
	uintptr_t *rsp;

	req = (struct cpt_request_info *)get_work1;
	instance = req->qp;
	rsp = req->op;
	cop = (void *)rsp[1];
	op_type = cop->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC ? OP_TYPE_SYM :
							      OP_TYPE_ASYM;

	do {
		cc = check_nb_command_id(
			req, (struct cpt_instance *)(uintptr_t)instance);
	} while (cc == ERR_REQ_PENDING);

	cop = otx_cpt_process_response(instance, (void *)req->op, cc, op_type);

	return (uintptr_t)(cop);
}

static struct rte_cryptodev_ops cptvf_ops = {
	/* Device related operations */
	.dev_configure = otx_cpt_dev_config,
	.dev_start = otx_cpt_dev_start,
	.dev_stop = otx_cpt_dev_stop,
	.dev_close = otx_cpt_dev_close,
	.dev_infos_get = otx_cpt_dev_info_get,

	.stats_get = NULL,
	.stats_reset = NULL,
	.queue_pair_setup = otx_cpt_que_pair_setup,
	.queue_pair_release = otx_cpt_que_pair_release,

	/* Crypto related operations */
	.sym_session_get_size = otx_cpt_get_session_size,
	.sym_session_configure = otx_cpt_session_cfg,
	.sym_session_clear = otx_cpt_session_clear,

	.asym_session_get_size = otx_cpt_asym_session_size_get,
	.asym_session_configure = otx_cpt_asym_session_cfg,
	.asym_session_clear = otx_cpt_asym_session_clear,
};

int
otx_cpt_dev_create(struct rte_cryptodev *c_dev)
{
	struct rte_pci_device *pdev = RTE_DEV_TO_PCI(c_dev->device);
	struct cpt_vf *cptvf = NULL;
	void *reg_base;
	char dev_name[32];
	int ret;

	if (pdev->mem_resource[0].phys_addr == 0ULL)
		return -EIO;

	/* for secondary processes, we don't initialise any further as primary
	 * has already done this work.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	cptvf = rte_zmalloc_socket("otx_cryptodev_private_mem",
			sizeof(struct cpt_vf), RTE_CACHE_LINE_SIZE,
			rte_socket_id());

	if (cptvf == NULL) {
		CPT_LOG_ERR("Cannot allocate memory for device private data");
		return -ENOMEM;
	}

	snprintf(dev_name, 32, "%02x:%02x.%x",
			pdev->addr.bus, pdev->addr.devid, pdev->addr.function);

	reg_base = pdev->mem_resource[0].addr;
	if (!reg_base) {
		CPT_LOG_ERR("Failed to map BAR0 of %s", dev_name);
		ret = -ENODEV;
		goto fail;
	}

	ret = otx_cpt_hw_init(cptvf, pdev, reg_base, dev_name);
	if (ret) {
		CPT_LOG_ERR("Failed to init cptvf %s", dev_name);
		ret = -EIO;
		goto fail;
	}

	switch (cptvf->vftype) {
	case OTX_CPT_VF_TYPE_AE:
		/* Set asymmetric cpt feature flags */
		c_dev->feature_flags = RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO |
				RTE_CRYPTODEV_FF_HW_ACCELERATED |
				RTE_CRYPTODEV_FF_RSA_PRIV_OP_KEY_QT;
		break;
	case OTX_CPT_VF_TYPE_SE:
		/* Set symmetric cpt feature flags */
		c_dev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
				RTE_CRYPTODEV_FF_HW_ACCELERATED |
				RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
				RTE_CRYPTODEV_FF_IN_PLACE_SGL |
				RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT |
				RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT |
				RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT |
				RTE_CRYPTODEV_FF_SYM_SESSIONLESS |
				RTE_CRYPTODEV_FF_DIGEST_ENCRYPTED;
		break;
	default:
		/* Feature not supported. Abort */
		CPT_LOG_ERR("VF type not supported by %s", dev_name);
		ret = -EIO;
		goto deinit_dev;
	}

	/* Start off timer for mailbox interrupts */
	otx_cpt_periodic_alarm_start(cptvf);

	c_dev->dev_ops = &cptvf_ops;

	if (c_dev->feature_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO) {
		c_dev->enqueue_burst = otx_cpt_enqueue_sym;
		c_dev->dequeue_burst = otx_cpt_dequeue_sym;
	} else {
		c_dev->enqueue_burst = otx_cpt_enqueue_asym;
		c_dev->dequeue_burst = otx_cpt_dequeue_asym;
	}

	/* Save dev private data */
	c_dev->data->dev_private = cptvf;

	return 0;

deinit_dev:
	otx_cpt_deinit_device(cptvf);

fail:
	if (cptvf) {
		/* Free private data allocated */
		rte_free(cptvf);
	}

	return ret;
}
