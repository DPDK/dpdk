/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <stdbool.h>

#include <cryptodev_pmd.h>
#include <rte_crypto.h>

#include "nitrox_sym.h"
#include "nitrox_device.h"
#include "nitrox_sym_capabilities.h"
#include "nitrox_qp.h"
#include "nitrox_sym_reqmgr.h"
#include "nitrox_sym_ctx.h"
#include "nitrox_logs.h"

#define CRYPTODEV_NAME_NITROX_PMD crypto_nitrox_sym
#define MC_MAC_MISMATCH_ERR_CODE 0x4c
#define NPS_PKT_IN_INSTR_SIZE 64
#define IV_FROM_DPTR 1
#define FLEXI_CRYPTO_ENCRYPT_HMAC 0x33
#define FLEXI_CRYPTO_MAX_AAD_LEN 512
#define AES_KEYSIZE_128 16
#define AES_KEYSIZE_192 24
#define AES_KEYSIZE_256 32
#define MAX_IV_LEN 16

struct nitrox_sym_device {
	struct rte_cryptodev *cdev;
	struct nitrox_device *ndev;
};

/* Cipher opcodes */
enum flexi_cipher {
	CIPHER_NULL = 0,
	CIPHER_3DES_CBC,
	CIPHER_3DES_ECB,
	CIPHER_AES_CBC,
	CIPHER_AES_ECB,
	CIPHER_AES_CFB,
	CIPHER_AES_CTR,
	CIPHER_AES_GCM,
	CIPHER_AES_XTS,
	CIPHER_AES_CCM,
	CIPHER_AES_CBC_CTS,
	CIPHER_AES_ECB_CTS,
	CIPHER_INVALID
};

/* Auth opcodes */
enum flexi_auth {
	AUTH_NULL = 0,
	AUTH_MD5,
	AUTH_SHA1,
	AUTH_SHA2_SHA224,
	AUTH_SHA2_SHA256,
	AUTH_SHA2_SHA384,
	AUTH_SHA2_SHA512,
	AUTH_GMAC,
	AUTH_INVALID
};

uint8_t nitrox_sym_drv_id;
static const char nitrox_sym_drv_name[] = RTE_STR(CRYPTODEV_NAME_NITROX_PMD);
static const struct rte_driver nitrox_rte_sym_drv = {
	.name = nitrox_sym_drv_name,
	.alias = nitrox_sym_drv_name
};

static int nitrox_sym_dev_qp_release(struct rte_cryptodev *cdev,
				     uint16_t qp_id);

static int
nitrox_sym_dev_config(struct rte_cryptodev *cdev,
		      struct rte_cryptodev_config *config)
{
	struct nitrox_sym_device *sym_dev = cdev->data->dev_private;
	struct nitrox_device *ndev = sym_dev->ndev;

	if (config->nb_queue_pairs > ndev->nr_queues) {
		NITROX_LOG(ERR, "Invalid queue pairs, max supported %d\n",
			   ndev->nr_queues);
		return -EINVAL;
	}

	return 0;
}

static int
nitrox_sym_dev_start(struct rte_cryptodev *cdev)
{
	/* SE cores initialization is done in PF */
	RTE_SET_USED(cdev);
	return 0;
}

static void
nitrox_sym_dev_stop(struct rte_cryptodev *cdev)
{
	/* SE cores cleanup is done in PF */
	RTE_SET_USED(cdev);
}

static int
nitrox_sym_dev_close(struct rte_cryptodev *cdev)
{
	int i, ret;

	for (i = 0; i < cdev->data->nb_queue_pairs; i++) {
		ret = nitrox_sym_dev_qp_release(cdev, i);
		if (ret)
			return ret;
	}

	return 0;
}

static void
nitrox_sym_dev_info_get(struct rte_cryptodev *cdev,
			struct rte_cryptodev_info *info)
{
	struct nitrox_sym_device *sym_dev = cdev->data->dev_private;
	struct nitrox_device *ndev = sym_dev->ndev;

	if (!info)
		return;

	info->max_nb_queue_pairs = ndev->nr_queues;
	info->feature_flags = cdev->feature_flags;
	info->capabilities = nitrox_get_sym_capabilities();
	info->driver_id = nitrox_sym_drv_id;
	info->sym.max_nb_sessions = 0;
}

static void
nitrox_sym_dev_stats_get(struct rte_cryptodev *cdev,
			 struct rte_cryptodev_stats *stats)
{
	int qp_id;

	for (qp_id = 0; qp_id < cdev->data->nb_queue_pairs; qp_id++) {
		struct nitrox_qp *qp = cdev->data->queue_pairs[qp_id];

		if (!qp)
			continue;

		stats->enqueued_count += qp->stats.enqueued_count;
		stats->dequeued_count += qp->stats.dequeued_count;
		stats->enqueue_err_count += qp->stats.enqueue_err_count;
		stats->dequeue_err_count += qp->stats.dequeue_err_count;
	}
}

static void
nitrox_sym_dev_stats_reset(struct rte_cryptodev *cdev)
{
	int qp_id;

	for (qp_id = 0; qp_id < cdev->data->nb_queue_pairs; qp_id++) {
		struct nitrox_qp *qp = cdev->data->queue_pairs[qp_id];

		if (!qp)
			continue;

		memset(&qp->stats, 0, sizeof(qp->stats));
	}
}

static int
nitrox_sym_dev_qp_setup(struct rte_cryptodev *cdev, uint16_t qp_id,
			const struct rte_cryptodev_qp_conf *qp_conf,
			int socket_id)
{
	struct nitrox_sym_device *sym_dev = cdev->data->dev_private;
	struct nitrox_device *ndev = sym_dev->ndev;
	struct nitrox_qp *qp = NULL;
	int err;

	NITROX_LOG(DEBUG, "queue %d\n", qp_id);
	if (qp_id >= ndev->nr_queues) {
		NITROX_LOG(ERR, "queue %u invalid, max queues supported %d\n",
			   qp_id, ndev->nr_queues);
		return -EINVAL;
	}

	if (cdev->data->queue_pairs[qp_id]) {
		err = nitrox_sym_dev_qp_release(cdev, qp_id);
		if (err)
			return err;
	}

	qp = rte_zmalloc_socket("nitrox PMD qp", sizeof(*qp),
				RTE_CACHE_LINE_SIZE,
				socket_id);
	if (!qp) {
		NITROX_LOG(ERR, "Failed to allocate nitrox qp\n");
		return -ENOMEM;
	}

	qp->qno = qp_id;
	err = nitrox_qp_setup(qp, ndev->bar_addr, cdev->data->name,
			      qp_conf->nb_descriptors, NPS_PKT_IN_INSTR_SIZE,
			      socket_id);
	if (unlikely(err))
		goto qp_setup_err;

	qp->sr_mp = nitrox_sym_req_pool_create(cdev, qp->count, qp_id,
					       socket_id);
	if (unlikely(!qp->sr_mp))
		goto req_pool_err;

	cdev->data->queue_pairs[qp_id] = qp;
	NITROX_LOG(DEBUG, "queue %d setup done\n", qp_id);
	return 0;

req_pool_err:
	nitrox_qp_release(qp, ndev->bar_addr);
qp_setup_err:
	rte_free(qp);
	return err;
}

static int
nitrox_sym_dev_qp_release(struct rte_cryptodev *cdev, uint16_t qp_id)
{
	struct nitrox_sym_device *sym_dev = cdev->data->dev_private;
	struct nitrox_device *ndev = sym_dev->ndev;
	struct nitrox_qp *qp;
	int err;

	NITROX_LOG(DEBUG, "queue %d\n", qp_id);
	if (qp_id >= ndev->nr_queues) {
		NITROX_LOG(ERR, "queue %u invalid, max queues supported %d\n",
			   qp_id, ndev->nr_queues);
		return -EINVAL;
	}

	qp = cdev->data->queue_pairs[qp_id];
	if (!qp) {
		NITROX_LOG(DEBUG, "queue %u already freed\n", qp_id);
		return 0;
	}

	if (!nitrox_qp_is_empty(qp)) {
		NITROX_LOG(ERR, "queue %d not empty\n", qp_id);
		return -EAGAIN;
	}

	cdev->data->queue_pairs[qp_id] = NULL;
	err = nitrox_qp_release(qp, ndev->bar_addr);
	nitrox_sym_req_pool_free(qp->sr_mp);
	rte_free(qp);
	NITROX_LOG(DEBUG, "queue %d release done\n", qp_id);
	return err;
}

static unsigned int
nitrox_sym_dev_sess_get_size(__rte_unused struct rte_cryptodev *cdev)
{
	return sizeof(struct nitrox_crypto_ctx);
}

static enum nitrox_chain
get_crypto_chain_order(const struct rte_crypto_sym_xform *xform)
{
	enum nitrox_chain res = NITROX_CHAIN_NOT_SUPPORTED;

	if (unlikely(xform == NULL))
		return res;

	switch (xform->type) {
	case RTE_CRYPTO_SYM_XFORM_AUTH:
		if (xform->next == NULL) {
			res = NITROX_CHAIN_NOT_SUPPORTED;
		} else if (xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
			if (xform->auth.op == RTE_CRYPTO_AUTH_OP_VERIFY &&
			    xform->next->cipher.op ==
			    RTE_CRYPTO_CIPHER_OP_DECRYPT) {
				res = NITROX_CHAIN_AUTH_CIPHER;
			} else {
				NITROX_LOG(ERR, "auth op %d, cipher op %d\n",
				    xform->auth.op, xform->next->cipher.op);
			}
		}
		break;
	case RTE_CRYPTO_SYM_XFORM_CIPHER:
		if (xform->next == NULL) {
			res = NITROX_CHAIN_CIPHER_ONLY;
		} else if (xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
			if (xform->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT &&
			    xform->next->auth.op ==
			    RTE_CRYPTO_AUTH_OP_GENERATE) {
				res = NITROX_CHAIN_CIPHER_AUTH;
			} else {
				NITROX_LOG(ERR, "cipher op %d, auth op %d\n",
				    xform->cipher.op, xform->next->auth.op);
			}
		}
		break;
	case RTE_CRYPTO_SYM_XFORM_AEAD:
		res = NITROX_CHAIN_COMBINED;
		break;
	default:
		break;
	}

	return res;
}

static enum flexi_cipher
get_flexi_cipher_type(enum rte_crypto_cipher_algorithm algo, bool *is_aes)
{
	enum flexi_cipher type;

	switch (algo) {
	case RTE_CRYPTO_CIPHER_AES_CBC:
		type = CIPHER_AES_CBC;
		*is_aes = true;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		type = CIPHER_3DES_CBC;
		*is_aes = false;
		break;
	default:
		type = CIPHER_INVALID;
		NITROX_LOG(ERR, "Algorithm not supported %d\n", algo);
		break;
	}

	return type;
}

static int
flexi_aes_keylen(size_t keylen, bool is_aes)
{
	int aes_keylen;

	if (!is_aes)
		return 0;

	switch (keylen) {
	case AES_KEYSIZE_128:
		aes_keylen = 1;
		break;
	case AES_KEYSIZE_192:
		aes_keylen = 2;
		break;
	case AES_KEYSIZE_256:
		aes_keylen = 3;
		break;
	default:
		NITROX_LOG(ERR, "Invalid keylen %zu\n", keylen);
		aes_keylen = -EINVAL;
		break;
	}

	return aes_keylen;
}

static bool
crypto_key_is_valid(struct rte_crypto_cipher_xform *xform,
		    struct flexi_crypto_context *fctx)
{
	if (unlikely(xform->key.length > sizeof(fctx->crypto.key))) {
		NITROX_LOG(ERR, "Invalid crypto key length %d\n",
			   xform->key.length);
		return false;
	}

	return true;
}

static int
configure_cipher_ctx(struct rte_crypto_cipher_xform *xform,
		     struct nitrox_crypto_ctx *ctx)
{
	enum flexi_cipher type;
	bool cipher_is_aes = false;
	int aes_keylen;
	struct flexi_crypto_context *fctx = &ctx->fctx;

	type = get_flexi_cipher_type(xform->algo, &cipher_is_aes);
	if (unlikely(type == CIPHER_INVALID))
		return -ENOTSUP;

	aes_keylen = flexi_aes_keylen(xform->key.length, cipher_is_aes);
	if (unlikely(aes_keylen < 0))
		return -EINVAL;

	if (unlikely(!cipher_is_aes && !crypto_key_is_valid(xform, fctx)))
		return -EINVAL;

	if (unlikely(xform->iv.length > MAX_IV_LEN))
		return -EINVAL;

	fctx->flags = rte_be_to_cpu_64(fctx->flags);
	fctx->w0.cipher_type = type;
	fctx->w0.aes_keylen = aes_keylen;
	fctx->w0.iv_source = IV_FROM_DPTR;
	fctx->flags = rte_cpu_to_be_64(fctx->flags);
	memset(fctx->crypto.key, 0, sizeof(fctx->crypto.key));
	memcpy(fctx->crypto.key, xform->key.data, xform->key.length);

	ctx->opcode = FLEXI_CRYPTO_ENCRYPT_HMAC;
	ctx->req_op = (xform->op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) ?
			NITROX_OP_ENCRYPT : NITROX_OP_DECRYPT;
	ctx->iv.offset = xform->iv.offset;
	ctx->iv.length = xform->iv.length;
	return 0;
}

static enum flexi_auth
get_flexi_auth_type(enum rte_crypto_auth_algorithm algo)
{
	enum flexi_auth type;

	switch (algo) {
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		type = AUTH_SHA1;
		break;
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
		type = AUTH_SHA2_SHA224;
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		type = AUTH_SHA2_SHA256;
		break;
	default:
		NITROX_LOG(ERR, "Algorithm not supported %d\n", algo);
		type = AUTH_INVALID;
		break;
	}

	return type;
}

static bool
auth_key_is_valid(const uint8_t *data, uint16_t length,
		  struct flexi_crypto_context *fctx)
{
	if (unlikely(!data && length)) {
		NITROX_LOG(ERR, "Invalid auth key\n");
		return false;
	}

	if (unlikely(length > sizeof(fctx->auth.opad))) {
		NITROX_LOG(ERR, "Invalid auth key length %d\n",
			   length);
		return false;
	}

	return true;
}

static int
configure_auth_ctx(struct rte_crypto_auth_xform *xform,
		   struct nitrox_crypto_ctx *ctx)
{
	enum flexi_auth type;
	struct flexi_crypto_context *fctx = &ctx->fctx;

	type = get_flexi_auth_type(xform->algo);
	if (unlikely(type == AUTH_INVALID))
		return -ENOTSUP;

	if (unlikely(!auth_key_is_valid(xform->key.data, xform->key.length,
					fctx)))
		return -EINVAL;

	ctx->digest_length = xform->digest_length;

	fctx->flags = rte_be_to_cpu_64(fctx->flags);
	fctx->w0.hash_type = type;
	fctx->w0.auth_input_type = 1;
	fctx->w0.mac_len = xform->digest_length;
	fctx->flags = rte_cpu_to_be_64(fctx->flags);
	memset(&fctx->auth, 0, sizeof(fctx->auth));
	memcpy(fctx->auth.opad, xform->key.data, xform->key.length);
	return 0;
}

static int
configure_aead_ctx(struct rte_crypto_aead_xform *xform,
		   struct nitrox_crypto_ctx *ctx)
{
	int aes_keylen;
	struct flexi_crypto_context *fctx = &ctx->fctx;

	if (unlikely(xform->aad_length > FLEXI_CRYPTO_MAX_AAD_LEN)) {
		NITROX_LOG(ERR, "AAD length %d not supported\n",
			   xform->aad_length);
		return -ENOTSUP;
	}

	if (unlikely(xform->algo != RTE_CRYPTO_AEAD_AES_GCM))
		return -ENOTSUP;

	aes_keylen = flexi_aes_keylen(xform->key.length, true);
	if (unlikely(aes_keylen < 0))
		return -EINVAL;

	if (unlikely(!auth_key_is_valid(xform->key.data, xform->key.length,
					fctx)))
		return -EINVAL;

	if (unlikely(xform->iv.length > MAX_IV_LEN))
		return -EINVAL;

	fctx->flags = rte_be_to_cpu_64(fctx->flags);
	fctx->w0.cipher_type = CIPHER_AES_GCM;
	fctx->w0.aes_keylen = aes_keylen;
	fctx->w0.iv_source = IV_FROM_DPTR;
	fctx->w0.hash_type = AUTH_NULL;
	fctx->w0.auth_input_type = 1;
	fctx->w0.mac_len = xform->digest_length;
	fctx->flags = rte_cpu_to_be_64(fctx->flags);
	memset(fctx->crypto.key, 0, sizeof(fctx->crypto.key));
	memcpy(fctx->crypto.key, xform->key.data, xform->key.length);
	memset(&fctx->auth, 0, sizeof(fctx->auth));
	memcpy(fctx->auth.opad, xform->key.data, xform->key.length);

	ctx->opcode = FLEXI_CRYPTO_ENCRYPT_HMAC;
	ctx->req_op = (xform->op == RTE_CRYPTO_AEAD_OP_ENCRYPT) ?
			NITROX_OP_ENCRYPT : NITROX_OP_DECRYPT;
	ctx->iv.offset = xform->iv.offset;
	ctx->iv.length = xform->iv.length;
	ctx->digest_length = xform->digest_length;
	ctx->aad_length = xform->aad_length;
	return 0;
}

static int
nitrox_sym_dev_sess_configure(struct rte_cryptodev *cdev,
			      struct rte_crypto_sym_xform *xform,
			      struct rte_cryptodev_sym_session *sess,
			      struct rte_mempool *mempool)
{
	void *mp_obj;
	struct nitrox_crypto_ctx *ctx;
	struct rte_crypto_cipher_xform *cipher_xform = NULL;
	struct rte_crypto_auth_xform *auth_xform = NULL;
	struct rte_crypto_aead_xform *aead_xform = NULL;
	int ret = -EINVAL;

	if (rte_mempool_get(mempool, &mp_obj)) {
		NITROX_LOG(ERR, "Couldn't allocate context\n");
		return -ENOMEM;
	}

	ctx = mp_obj;
	ctx->nitrox_chain = get_crypto_chain_order(xform);
	switch (ctx->nitrox_chain) {
	case NITROX_CHAIN_CIPHER_ONLY:
		cipher_xform = &xform->cipher;
		break;
	case NITROX_CHAIN_CIPHER_AUTH:
		cipher_xform = &xform->cipher;
		auth_xform = &xform->next->auth;
		break;
	case NITROX_CHAIN_AUTH_CIPHER:
		auth_xform = &xform->auth;
		cipher_xform = &xform->next->cipher;
		break;
	case NITROX_CHAIN_COMBINED:
		aead_xform = &xform->aead;
		break;
	default:
		NITROX_LOG(ERR, "Crypto chain not supported\n");
		ret = -ENOTSUP;
		goto err;
	}

	if (cipher_xform && unlikely(configure_cipher_ctx(cipher_xform, ctx))) {
		NITROX_LOG(ERR, "Failed to configure cipher ctx\n");
		goto err;
	}

	if (auth_xform && unlikely(configure_auth_ctx(auth_xform, ctx))) {
		NITROX_LOG(ERR, "Failed to configure auth ctx\n");
		goto err;
	}

	if (aead_xform && unlikely(configure_aead_ctx(aead_xform, ctx))) {
		NITROX_LOG(ERR, "Failed to configure aead ctx\n");
		goto err;
	}

	ctx->iova = rte_mempool_virt2iova(ctx);
	set_sym_session_private_data(sess, cdev->driver_id, ctx);
	return 0;
err:
	rte_mempool_put(mempool, mp_obj);
	return ret;
}

static void
nitrox_sym_dev_sess_clear(struct rte_cryptodev *cdev,
			  struct rte_cryptodev_sym_session *sess)
{
	struct nitrox_crypto_ctx *ctx = get_sym_session_private_data(sess,
							cdev->driver_id);
	struct rte_mempool *sess_mp;

	if (!ctx)
		return;

	memset(ctx, 0, sizeof(*ctx));
	sess_mp = rte_mempool_from_obj(ctx);
	set_sym_session_private_data(sess, cdev->driver_id, NULL);
	rte_mempool_put(sess_mp, ctx);
}

static struct nitrox_crypto_ctx *
get_crypto_ctx(struct rte_crypto_op *op)
{
	if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
		if (likely(op->sym->session))
			return get_sym_session_private_data(op->sym->session,
							   nitrox_sym_drv_id);
	}

	return NULL;
}

static int
nitrox_enq_single_op(struct nitrox_qp *qp, struct rte_crypto_op *op)
{
	struct nitrox_crypto_ctx *ctx;
	struct nitrox_softreq *sr;
	int err;

	op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
	ctx = get_crypto_ctx(op);
	if (unlikely(!ctx)) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
		return -EINVAL;
	}

	if (unlikely(rte_mempool_get(qp->sr_mp, (void **)&sr)))
		return -ENOMEM;

	err = nitrox_process_se_req(qp->qno, op, ctx, sr);
	if (unlikely(err)) {
		rte_mempool_put(qp->sr_mp, sr);
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		return err;
	}

	nitrox_qp_enqueue(qp, nitrox_sym_instr_addr(sr), sr);
	return 0;
}

static uint16_t
nitrox_sym_dev_enq_burst(void *queue_pair, struct rte_crypto_op **ops,
			 uint16_t nb_ops)
{
	struct nitrox_qp *qp = queue_pair;
	uint16_t free_slots = 0;
	uint16_t cnt = 0;
	bool err = false;

	free_slots = nitrox_qp_free_count(qp);
	if (nb_ops > free_slots)
		nb_ops = free_slots;

	for (cnt = 0; cnt < nb_ops; cnt++) {
		if (unlikely(nitrox_enq_single_op(qp, ops[cnt]))) {
			err = true;
			break;
		}
	}

	nitrox_ring_dbell(qp, cnt);
	qp->stats.enqueued_count += cnt;
	if (unlikely(err))
		qp->stats.enqueue_err_count++;

	return cnt;
}

static int
nitrox_deq_single_op(struct nitrox_qp *qp, struct rte_crypto_op **op_ptr)
{
	struct nitrox_softreq *sr;
	int ret;
	struct rte_crypto_op *op;

	sr = nitrox_qp_get_softreq(qp);
	ret = nitrox_check_se_req(sr, op_ptr);
	if (ret < 0)
		return -EAGAIN;

	op = *op_ptr;
	nitrox_qp_dequeue(qp);
	rte_mempool_put(qp->sr_mp, sr);
	if (!ret) {
		op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
		qp->stats.dequeued_count++;

		return 0;
	}

	if (ret == MC_MAC_MISMATCH_ERR_CODE)
		op->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
	else
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;

	qp->stats.dequeue_err_count++;
	return 0;
}

static uint16_t
nitrox_sym_dev_deq_burst(void *queue_pair, struct rte_crypto_op **ops,
			 uint16_t nb_ops)
{
	struct nitrox_qp *qp = queue_pair;
	uint16_t filled_slots = nitrox_qp_used_count(qp);
	int cnt = 0;

	if (nb_ops > filled_slots)
		nb_ops = filled_slots;

	for (cnt = 0; cnt < nb_ops; cnt++)
		if (nitrox_deq_single_op(qp, &ops[cnt]))
			break;

	return cnt;
}

static struct rte_cryptodev_ops nitrox_cryptodev_ops = {
	.dev_configure		= nitrox_sym_dev_config,
	.dev_start		= nitrox_sym_dev_start,
	.dev_stop		= nitrox_sym_dev_stop,
	.dev_close		= nitrox_sym_dev_close,
	.dev_infos_get		= nitrox_sym_dev_info_get,
	.stats_get		= nitrox_sym_dev_stats_get,
	.stats_reset		= nitrox_sym_dev_stats_reset,
	.queue_pair_setup	= nitrox_sym_dev_qp_setup,
	.queue_pair_release     = nitrox_sym_dev_qp_release,
	.sym_session_get_size   = nitrox_sym_dev_sess_get_size,
	.sym_session_configure  = nitrox_sym_dev_sess_configure,
	.sym_session_clear      = nitrox_sym_dev_sess_clear
};

int
nitrox_sym_pmd_create(struct nitrox_device *ndev)
{
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	struct rte_cryptodev_pmd_init_params init_params = {
			.name = "",
			.socket_id = ndev->pdev->device.numa_node,
			.private_data_size = sizeof(struct nitrox_sym_device)
	};
	struct rte_cryptodev *cdev;

	rte_pci_device_name(&ndev->pdev->addr, name, sizeof(name));
	snprintf(name + strlen(name), RTE_CRYPTODEV_NAME_MAX_LEN - strlen(name),
		 "_n5sym");
	ndev->rte_sym_dev.driver = &nitrox_rte_sym_drv;
	ndev->rte_sym_dev.numa_node = ndev->pdev->device.numa_node;
	ndev->rte_sym_dev.devargs = NULL;
	cdev = rte_cryptodev_pmd_create(name, &ndev->rte_sym_dev,
					&init_params);
	if (!cdev) {
		NITROX_LOG(ERR, "Cryptodev '%s' creation failed\n", name);
		return -ENODEV;
	}

	ndev->rte_sym_dev.name = cdev->data->name;
	cdev->driver_id = nitrox_sym_drv_id;
	cdev->dev_ops = &nitrox_cryptodev_ops;
	cdev->enqueue_burst = nitrox_sym_dev_enq_burst;
	cdev->dequeue_burst = nitrox_sym_dev_deq_burst;
	cdev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
		RTE_CRYPTODEV_FF_HW_ACCELERATED |
		RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
		RTE_CRYPTODEV_FF_IN_PLACE_SGL |
		RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT |
		RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT |
		RTE_CRYPTODEV_FF_OOP_LB_IN_SGL_OUT |
		RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT;

	ndev->sym_dev = cdev->data->dev_private;
	ndev->sym_dev->cdev = cdev;
	ndev->sym_dev->ndev = ndev;

	rte_cryptodev_pmd_probing_finish(cdev);

	NITROX_LOG(DEBUG, "Created cryptodev '%s', dev_id %d, drv_id %d\n",
		   cdev->data->name, cdev->data->dev_id, nitrox_sym_drv_id);
	return 0;
}

int
nitrox_sym_pmd_destroy(struct nitrox_device *ndev)
{
	return rte_cryptodev_pmd_destroy(ndev->sym_dev->cdev);
}

static struct cryptodev_driver nitrox_crypto_drv;
RTE_PMD_REGISTER_CRYPTO_DRIVER(nitrox_crypto_drv,
		nitrox_rte_sym_drv,
		nitrox_sym_drv_id);
