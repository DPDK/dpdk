/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021-2024 Advanced Micro Devices, Inc.
 */

#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_mempool.h>

#include "ionic_crypto.h"

static int
iocpt_op_config(struct rte_cryptodev *cdev,
		struct rte_cryptodev_config *config __rte_unused)
{
	struct iocpt_dev *dev = cdev->data->dev_private;

	iocpt_configure(dev);

	return 0;
}

static int
iocpt_op_start(struct rte_cryptodev *cdev)
{
	struct iocpt_dev *dev = cdev->data->dev_private;

	return iocpt_start(dev);
}

static void
iocpt_op_stop(struct rte_cryptodev *cdev)
{
	struct iocpt_dev *dev = cdev->data->dev_private;

	return iocpt_stop(dev);
}

static int
iocpt_op_close(struct rte_cryptodev *cdev)
{
	struct iocpt_dev *dev = cdev->data->dev_private;

	iocpt_deinit(dev);

	return 0;
}

static void
iocpt_op_info_get(struct rte_cryptodev *cdev, struct rte_cryptodev_info *info)
{
	struct iocpt_dev *dev = cdev->data->dev_private;

	if (info == NULL)
		return;

	info->max_nb_queue_pairs = dev->max_qps;
	info->feature_flags = dev->features;
	info->capabilities = iocpt_get_caps(info->feature_flags);
	info->sym.max_nb_sessions = dev->max_sessions;
	info->driver_id = dev->driver_id;
	info->min_mbuf_headroom_req = 0;
	info->min_mbuf_tailroom_req = 0;
}

static int
iocpt_op_queue_release(struct rte_cryptodev *cdev, uint16_t queue_id)
{
	struct iocpt_crypto_q *cptq = cdev->data->queue_pairs[queue_id];

	IOCPT_PRINT(DEBUG, "queue_id %u", queue_id);

	assert(!(cptq->flags & IOCPT_Q_F_INITED));

	iocpt_cryptoq_free(cptq);

	cdev->data->queue_pairs[queue_id] = NULL;

	return 0;
}

static int
iocpt_op_queue_setup(struct rte_cryptodev *cdev, uint16_t queue_id,
		const struct rte_cryptodev_qp_conf *qp_conf,
		int socket_id)
{
	struct iocpt_dev *dev = cdev->data->dev_private;
	int err;

	if (cdev->data->queue_pairs[queue_id] != NULL)
		iocpt_op_queue_release(cdev, queue_id);

	if (qp_conf->nb_descriptors < (1 << IOCPT_QSIZE_MIN_LG2) ||
	    qp_conf->nb_descriptors > (1 << IOCPT_QSIZE_MAX_LG2)) {
		IOCPT_PRINT(ERR, "invalid nb_descriptors %u, use range %u..%u",
			qp_conf->nb_descriptors,
			1 << IOCPT_QSIZE_MIN_LG2, 1 << IOCPT_QSIZE_MAX_LG2);
		return -ERANGE;
	}

	IOCPT_PRINT(DEBUG, "queue_id %u", queue_id);

	err = iocpt_cryptoq_alloc(dev, socket_id, queue_id,
				qp_conf->nb_descriptors);
	if (err != 0)
		return err;

	cdev->data->queue_pairs[queue_id] = dev->cryptoqs[queue_id];

	return 0;
}

static unsigned int
iocpt_op_get_session_size(struct rte_cryptodev *cdev __rte_unused)
{
	return iocpt_session_size();
}

static inline int
iocpt_is_algo_supported(struct rte_crypto_sym_xform *xform)
{
	if (xform->next != NULL) {
		IOCPT_PRINT(ERR, "chaining not supported");
		return -ENOTSUP;
	}

	if (xform->type != RTE_CRYPTO_SYM_XFORM_AEAD) {
		IOCPT_PRINT(ERR, "xform->type %d not supported", xform->type);
		return -ENOTSUP;
	}

	return 0;
}

static __rte_always_inline int
iocpt_fill_sess_aead(struct rte_crypto_sym_xform *xform,
		struct iocpt_session_priv *priv)
{
	struct rte_crypto_aead_xform *aead_form = &xform->aead;

	if (aead_form->algo != RTE_CRYPTO_AEAD_AES_GCM) {
		IOCPT_PRINT(ERR, "Unknown algo");
		return -EINVAL;
	}
	if (aead_form->op == RTE_CRYPTO_AEAD_OP_ENCRYPT) {
		priv->op = IOCPT_DESC_OPCODE_GCM_AEAD_ENCRYPT;
	} else if (aead_form->op == RTE_CRYPTO_AEAD_OP_DECRYPT) {
		priv->op = IOCPT_DESC_OPCODE_GCM_AEAD_DECRYPT;
	} else {
		IOCPT_PRINT(ERR, "Unknown cipher operations");
		return -1;
	}

	if (aead_form->key.length < IOCPT_SESS_KEY_LEN_MIN ||
	    aead_form->key.length > IOCPT_SESS_KEY_LEN_MAX_SYMM) {
		IOCPT_PRINT(ERR, "Invalid cipher keylen %u",
			aead_form->key.length);
		return -1;
	}
	priv->key_len = aead_form->key.length;
	memcpy(priv->key, aead_form->key.data, priv->key_len);

	priv->type = IOCPT_SESS_AEAD_AES_GCM;
	priv->iv_offset = aead_form->iv.offset;
	priv->iv_length = aead_form->iv.length;
	priv->digest_length = aead_form->digest_length;
	priv->aad_length = aead_form->aad_length;

	return 0;
}

static int
iocpt_session_cfg(struct iocpt_dev *dev,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *sess)
{
	struct rte_crypto_sym_xform *chain;
	struct iocpt_session_priv *priv = NULL;

	if (iocpt_is_algo_supported(xform) < 0)
		return -ENOTSUP;

	if (unlikely(sess == NULL)) {
		IOCPT_PRINT(ERR, "invalid session");
		return -EINVAL;
	}

	priv = CRYPTODEV_GET_SYM_SESS_PRIV(sess);
	priv->dev = dev;

	chain = xform;
	while (chain) {
		switch (chain->type) {
		case RTE_CRYPTO_SYM_XFORM_AEAD:
			if (iocpt_fill_sess_aead(chain, priv))
				return -EIO;
			break;
		default:
			IOCPT_PRINT(ERR, "invalid crypto xform type %d",
				chain->type);
			return -ENOTSUP;
		}
		chain = chain->next;
	}

	return iocpt_session_init(priv);
}

static int
iocpt_op_session_cfg(struct rte_cryptodev *cdev,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *sess)
{
	struct iocpt_dev *dev = cdev->data->dev_private;

	return iocpt_session_cfg(dev, xform, sess);
}

static void
iocpt_session_clear(struct rte_cryptodev_sym_session *sess)
{
	iocpt_session_deinit(CRYPTODEV_GET_SYM_SESS_PRIV(sess));
}

static void
iocpt_op_session_clear(struct rte_cryptodev *cdev __rte_unused,
		struct rte_cryptodev_sym_session *sess)
{
	iocpt_session_clear(sess);
}

static inline void
iocpt_fill_sge(struct iocpt_crypto_sg_elem *arr, uint8_t idx,
		uint64_t addr, uint16_t len)
{
	arr[idx].addr = rte_cpu_to_le_64(addr);
	arr[idx].len = rte_cpu_to_le_16(len);
}

static __rte_always_inline int
iocpt_enq_one_aead(struct iocpt_crypto_q *cptq,
		struct iocpt_session_priv *priv, struct rte_crypto_op *op)
{
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct iocpt_queue *q = &cptq->q;
	struct iocpt_crypto_desc *desc, *desc_base = q->base;
	struct iocpt_crypto_sg_desc *sg_desc, *sg_desc_base = q->sg_base;
	struct iocpt_crypto_sg_elem *src, *dst;
	rte_iova_t aad_addr, digest_addr, iv_addr, seg_addr;
	uint32_t data_len, data_offset, seg_len;
	uint8_t nsge_src = 0, nsge_dst = 0, flags = 0;
	struct rte_mbuf *m;

	desc = &desc_base[q->head_idx];
	sg_desc = &sg_desc_base[q->head_idx];
	src = sg_desc->src_elems;
	dst = sg_desc->dst_elems;

	/* Fill the first SGE with the IV / Nonce */
	iv_addr = rte_crypto_op_ctophys_offset(op, priv->iv_offset);
	iocpt_fill_sge(src, nsge_src++, iv_addr, priv->iv_length);

	/* Fill the second SGE with the AAD, if applicable */
	if (priv->aad_length > 0) {
		aad_addr = sym_op->aead.aad.phys_addr;
		iocpt_fill_sge(src, nsge_src++, aad_addr, priv->aad_length);
		flags |= IOCPT_DESC_F_AAD_VALID;
	}

	m = sym_op->m_src;
	data_len = sym_op->aead.data.length;

	/* Fast-forward through mbuf chain to account for data offset */
	data_offset = sym_op->aead.data.offset;
	while (m != NULL && data_offset >= m->data_len) {
		data_offset -= m->data_len;
		m = m->next;
	}

	/* Fill the next SGEs with the payload segments */
	while (m != NULL && data_len > 0) {
		seg_addr = rte_mbuf_data_iova(m) + data_offset;
		seg_len = RTE_MIN(m->data_len - data_offset, data_len);
		data_offset = 0;
		data_len -= seg_len;

		/* Use -1 to save room for digest */
		if (nsge_src >= IOCPT_CRYPTO_MAX_SG_ELEMS - 1)
			return -ERANGE;

		iocpt_fill_sge(src, nsge_src++, seg_addr, seg_len);

		m = m->next;
	}

	/* AEAD AES-GCM: digest == authentication tag */
	digest_addr = sym_op->aead.digest.phys_addr;
	iocpt_fill_sge(src, nsge_src++, digest_addr, priv->digest_length);

	/* Process Out-Of-Place destination SGL */
	if (sym_op->m_dst != NULL) {
		/* Put the AAD here, too */
		if (priv->aad_length > 0)
			iocpt_fill_sge(dst, nsge_dst++,
				sym_op->aead.aad.phys_addr, priv->aad_length);

		m = sym_op->m_dst;
		data_len = sym_op->aead.data.length;

		/* Fast-forward through chain to account for data offset */
		data_offset = sym_op->aead.data.offset;
		while (m != NULL && data_offset >= m->data_len) {
			data_offset -= m->data_len;
			m = m->next;
		}

		/* Fill in the SGEs with the payload segments */
		while (m != NULL && data_len > 0) {
			seg_addr = rte_mbuf_data_iova(m) + data_offset;
			seg_len = RTE_MIN(m->data_len - data_offset, data_len);
			data_offset = 0;
			data_len -= seg_len;

			if (nsge_dst >= IOCPT_CRYPTO_MAX_SG_ELEMS)
				return -ERANGE;

			iocpt_fill_sge(dst, nsge_dst++, seg_addr, seg_len);

			m = m->next;
		}
	}

	desc->opcode = priv->op;
	desc->flags = flags;
	desc->num_src_dst_sgs = iocpt_encode_nsge_src_dst(nsge_src, nsge_dst);
	desc->session_tag = rte_cpu_to_le_32(priv->index);

	op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
	q->info[q->head_idx] = op;
	q->head_idx = Q_NEXT_TO_POST(q, 1);

	return 0;
}

static uint16_t
iocpt_enqueue_sym(void *qp, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct iocpt_crypto_q *cptq = qp;
	struct rte_crypto_op *op;
	struct iocpt_session_priv *priv;
	uint16_t avail, count;
	int err;

	avail = iocpt_q_space_avail(&cptq->q);
	if (unlikely(nb_ops > avail))
		nb_ops = avail;

	count = 0;
	while (likely(count < nb_ops)) {
		op = ops[count];

		if (unlikely(op->sess_type != RTE_CRYPTO_OP_WITH_SESSION)) {
			op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			break;
		}

		priv = CRYPTODEV_GET_SYM_SESS_PRIV(op->sym->session);
		if (unlikely(priv == NULL)) {
			op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
			break;
		}

		err = iocpt_enq_one_aead(cptq, priv, op);
		if (unlikely(err != 0)) {
			op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			break;
		}

		count++;
	}

	if (likely(count > 0))
		iocpt_q_flush(&cptq->q);

	return count;
}

static uint16_t
iocpt_dequeue_sym(void *qp, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct iocpt_crypto_q *cptq = qp;
	struct iocpt_queue *q = &cptq->q;
	struct iocpt_cq *cq = &cptq->cq;
	struct rte_crypto_op *op;
	struct iocpt_crypto_comp *cq_desc_base = cq->base;
	volatile struct iocpt_crypto_comp *cq_desc;
	uint16_t count = 0;

	cq_desc = &cq_desc_base[cq->tail_idx];

	/* First walk the CQ to update any completed op's status
	 * NB: These can arrive out of order!
	 */
	while ((cq_desc->color & 0x1) == cq->done_color) {
		cq->tail_idx = Q_NEXT_TO_SRVC(cq, 1);
		if (unlikely(cq->tail_idx == 0))
			cq->done_color = !cq->done_color;

		op = q->info[rte_le_to_cpu_16(cq_desc->comp_index)];

		/* Process returned CQ descriptor status */
		if (unlikely(cq_desc->status)) {
			switch (cq_desc->status) {
			case IOCPT_COMP_SYMM_AUTH_VERIFY_ERROR:
				op->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
				break;
			case IOCPT_COMP_INVAL_OPCODE_ERROR:
			case IOCPT_COMP_UNSUPP_OPCODE_ERROR:
			case IOCPT_COMP_SYMM_SRC_SG_ERROR:
			case IOCPT_COMP_SYMM_DST_SG_ERROR:
			case IOCPT_COMP_SYMM_SRC_DST_LEN_MISMATCH:
			case IOCPT_COMP_SYMM_KEY_IDX_ERROR:
				op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
				break;
			default:
				op->status = RTE_CRYPTO_OP_STATUS_ERROR;
				break;
			}
		} else
			op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

		cq_desc = &cq_desc_base[cq->tail_idx];
	}

	/* Next walk the SQ to pop off completed ops in-order */
	while (count < nb_ops) {
		op = q->info[q->tail_idx];

		/* No more completions */
		if (op == NULL ||
		    op->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED)
			break;

		ops[count] = op;
		q->info[q->tail_idx] = NULL;

		q->tail_idx = Q_NEXT_TO_SRVC(q, 1);
		count++;
	}

	return count;
}

static struct rte_cryptodev_ops iocpt_ops = {
	.dev_configure = iocpt_op_config,
	.dev_start = iocpt_op_start,
	.dev_stop = iocpt_op_stop,
	.dev_close = iocpt_op_close,
	.dev_infos_get = iocpt_op_info_get,

	.queue_pair_setup = iocpt_op_queue_setup,
	.queue_pair_release = iocpt_op_queue_release,

	.sym_session_get_size = iocpt_op_get_session_size,
	.sym_session_configure = iocpt_op_session_cfg,
	.sym_session_clear = iocpt_op_session_clear,
};

int
iocpt_assign_ops(struct rte_cryptodev *cdev)
{
	struct iocpt_dev *dev = cdev->data->dev_private;

	cdev->dev_ops = &iocpt_ops;
	cdev->feature_flags = dev->features;

	if (dev->features & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO) {
		cdev->enqueue_burst = iocpt_enqueue_sym;
		cdev->dequeue_burst = iocpt_dequeue_sym;
	}

	return 0;
}
