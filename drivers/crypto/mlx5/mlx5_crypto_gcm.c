/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 NVIDIA Corporation & Affiliates
 */

#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_eal_paging.h>
#include <rte_errno.h>
#include <rte_log.h>
#include <bus_pci_driver.h>
#include <rte_memory.h>
#include <rte_io.h>

#include <mlx5_glue.h>
#include <mlx5_common.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_common_os.h>

#include "mlx5_crypto_utils.h"
#include "mlx5_crypto.h"

/*
 * AES-GCM uses indirect KLM mode. The UMR WQE comprises of WQE control +
 * UMR control + mkey context + indirect KLM. The WQE size is aligned to
 * be 3 WQEBBS.
 */
#define MLX5_UMR_GCM_WQE_SIZE \
	(RTE_ALIGN(sizeof(struct mlx5_umr_wqe) + sizeof(struct mlx5_wqe_dseg), \
			MLX5_SEND_WQE_BB))

#define MLX5_UMR_GCM_WQE_SET_SIZE \
	(MLX5_UMR_GCM_WQE_SIZE + \
	 RTE_ALIGN(sizeof(struct mlx5_wqe_send_en_wqe), \
	 MLX5_SEND_WQE_BB))

#define MLX5_UMR_GCM_WQE_STRIDE \
	(MLX5_UMR_GCM_WQE_SIZE / MLX5_SEND_WQE_BB)

#define MLX5_MMO_CRYPTO_OPC (MLX5_OPCODE_MMO | \
	(MLX5_OPC_MOD_MMO_CRYPTO << WQE_CSEG_OPC_MOD_OFFSET))

/*
 * The status default value is RTE_CRYPTO_OP_STATUS_SUCCESS.
 * Copy tag should fill different value to status.
 */
#define MLX5_CRYPTO_OP_STATUS_GCM_TAG_COPY (RTE_CRYPTO_OP_STATUS_SUCCESS + 1)

struct mlx5_crypto_gcm_op_info {
	bool need_umr;
	bool is_oop;
	bool is_enc;
	void *digest;
	void *src_addr;
};

struct mlx5_crypto_gcm_data {
	void *src_addr;
	uint32_t src_bytes;
	void *dst_addr;
	uint32_t dst_bytes;
	uint32_t src_mkey;
	uint32_t dst_mkey;
};

struct mlx5_crypto_gcm_tag_cpy_info {
	void *digest;
	uint8_t tag_len;
} __rte_packed;

static struct rte_cryptodev_capabilities mlx5_crypto_gcm_caps[] = {
	{
		.op = RTE_CRYPTO_OP_TYPE_UNDEFINED,
	},
	{
		.op = RTE_CRYPTO_OP_TYPE_UNDEFINED,
	}
};

int
mlx5_crypto_dek_fill_gcm_attr(struct mlx5_crypto_dek *dek,
			      struct mlx5_devx_dek_attr *dek_attr,
			      void *cb_ctx)
{
	uint32_t offset = 0;
	struct mlx5_crypto_dek_ctx *ctx = cb_ctx;
	struct rte_crypto_aead_xform *aead_ctx = &ctx->xform->aead;

	if (aead_ctx->algo != RTE_CRYPTO_AEAD_AES_GCM) {
		DRV_LOG(ERR, "Only AES-GCM algo supported.");
		return -EINVAL;
	}
	dek_attr->key_purpose = MLX5_CRYPTO_KEY_PURPOSE_GCM;
	switch (aead_ctx->key.length) {
	case 16:
		offset = 16;
		dek->size = 16;
		dek_attr->key_size = MLX5_CRYPTO_KEY_SIZE_128b;
		break;
	case 32:
		dek->size = 32;
		dek_attr->key_size = MLX5_CRYPTO_KEY_SIZE_256b;
		break;
	default:
		DRV_LOG(ERR, "Wrapped key size not supported.");
		return -EINVAL;
	}
	memcpy(&dek_attr->key[offset], aead_ctx->key.data, aead_ctx->key.length);
	memcpy(&dek->data, aead_ctx->key.data, aead_ctx->key.length);
	return 0;
}

static int
mlx5_crypto_generate_gcm_cap(struct mlx5_hca_crypto_mmo_attr *mmo_attr,
			     struct rte_cryptodev_capabilities *cap)
{
	/* Init key size. */
	if (mmo_attr->gcm_128_encrypt && mmo_attr->gcm_128_decrypt &&
		mmo_attr->gcm_256_encrypt && mmo_attr->gcm_256_decrypt) {
		cap->sym.aead.key_size.min = 16;
		cap->sym.aead.key_size.max = 32;
		cap->sym.aead.key_size.increment = 16;
	} else if (mmo_attr->gcm_256_encrypt && mmo_attr->gcm_256_decrypt) {
		cap->sym.aead.key_size.min = 32;
		cap->sym.aead.key_size.max = 32;
		cap->sym.aead.key_size.increment = 0;
	} else if (mmo_attr->gcm_128_encrypt && mmo_attr->gcm_128_decrypt) {
		cap->sym.aead.key_size.min = 16;
		cap->sym.aead.key_size.max = 16;
		cap->sym.aead.key_size.increment = 0;
	} else {
		DRV_LOG(ERR, "No available AES-GCM encryption/decryption supported.");
		return -1;
	}
	/* Init tag size. */
	if (mmo_attr->gcm_auth_tag_128 && mmo_attr->gcm_auth_tag_96) {
		cap->sym.aead.digest_size.min = 12;
		cap->sym.aead.digest_size.max = 16;
		cap->sym.aead.digest_size.increment = 4;
	} else if (mmo_attr->gcm_auth_tag_96) {
		cap->sym.aead.digest_size.min = 12;
		cap->sym.aead.digest_size.max = 12;
		cap->sym.aead.digest_size.increment = 0;
	} else if (mmo_attr->gcm_auth_tag_128) {
		cap->sym.aead.digest_size.min = 16;
		cap->sym.aead.digest_size.max = 16;
		cap->sym.aead.digest_size.increment = 0;
	} else {
		DRV_LOG(ERR, "No available AES-GCM tag size supported.");
		return -1;
	}
	/* Init AAD size. */
	cap->sym.aead.aad_size.min = 0;
	cap->sym.aead.aad_size.max = UINT16_MAX;
	cap->sym.aead.aad_size.increment = 1;
	/* Init IV size. */
	cap->sym.aead.iv_size.min = 12;
	cap->sym.aead.iv_size.max = 12;
	cap->sym.aead.iv_size.increment = 0;
	/* Init left items. */
	cap->op = RTE_CRYPTO_OP_TYPE_SYMMETRIC;
	cap->sym.xform_type = RTE_CRYPTO_SYM_XFORM_AEAD;
	cap->sym.aead.algo = RTE_CRYPTO_AEAD_AES_GCM;
	return 0;
}

static int
mlx5_crypto_sym_gcm_session_configure(struct rte_cryptodev *dev,
				  struct rte_crypto_sym_xform *xform,
				  struct rte_cryptodev_sym_session *session)
{
	struct mlx5_crypto_priv *priv = dev->data->dev_private;
	struct mlx5_crypto_session *sess_private_data = CRYPTODEV_GET_SYM_SESS_PRIV(session);
	struct rte_crypto_aead_xform *aead = &xform->aead;
	uint32_t op_type;

	if (unlikely(xform->next != NULL)) {
		DRV_LOG(ERR, "Xform next is not supported.");
		return -ENOTSUP;
	}
	if (aead->algo != RTE_CRYPTO_AEAD_AES_GCM) {
		DRV_LOG(ERR, "Only AES-GCM algorithm is supported.");
		return -ENOTSUP;
	}
	if (aead->op == RTE_CRYPTO_AEAD_OP_ENCRYPT)
		op_type = MLX5_CRYPTO_OP_TYPE_ENCRYPTION;
	else
		op_type = MLX5_CRYPTO_OP_TYPE_DECRYPTION;
	sess_private_data->op_type = op_type;
	sess_private_data->mmo_ctrl = rte_cpu_to_be_32
			(op_type << MLX5_CRYPTO_MMO_OP_OFFSET |
			 MLX5_ENCRYPTION_TYPE_AES_GCM << MLX5_CRYPTO_MMO_TYPE_OFFSET);
	sess_private_data->wqe_aad_len = rte_cpu_to_be_32((uint32_t)aead->aad_length);
	sess_private_data->wqe_tag_len = rte_cpu_to_be_32((uint32_t)aead->digest_length);
	sess_private_data->aad_len = aead->aad_length;
	sess_private_data->tag_len = aead->digest_length;
	sess_private_data->iv_offset = aead->iv.offset;
	sess_private_data->iv_len = aead->iv.length;
	sess_private_data->dek = mlx5_crypto_dek_prepare(priv, xform);
	if (sess_private_data->dek == NULL) {
		DRV_LOG(ERR, "Failed to prepare dek.");
		return -ENOMEM;
	}
	sess_private_data->dek_id =
			rte_cpu_to_be_32(sess_private_data->dek->obj->id &
					 0xffffff);
	DRV_LOG(DEBUG, "Session %p was configured.", sess_private_data);
	return 0;
}

static void *
mlx5_crypto_gcm_mkey_klm_update(struct mlx5_crypto_priv *priv,
				struct mlx5_crypto_qp *qp __rte_unused,
				uint32_t idx)
{
	return &qp->klm_array[idx * priv->max_klm_num];
}

static int
mlx5_crypto_gcm_qp_release(struct rte_cryptodev *dev, uint16_t qp_id)
{
	struct mlx5_crypto_priv *priv = dev->data->dev_private;
	struct mlx5_crypto_qp *qp = dev->data->queue_pairs[qp_id];

	if (qp->umr_qp_obj.qp != NULL)
		mlx5_devx_qp_destroy(&qp->umr_qp_obj);
	if (qp->qp_obj.qp != NULL)
		mlx5_devx_qp_destroy(&qp->qp_obj);
	if (qp->cq_obj.cq != NULL)
		mlx5_devx_cq_destroy(&qp->cq_obj);
	if (qp->mr.obj != NULL) {
		void *opaq = qp->mr.addr;

		priv->dereg_mr_cb(&qp->mr);
		rte_free(opaq);
	}
	mlx5_crypto_indirect_mkeys_release(qp, qp->entries_n);
	mlx5_mr_btree_free(&qp->mr_ctrl.cache_bh);
	rte_free(qp);
	dev->data->queue_pairs[qp_id] = NULL;
	return 0;
}

static void
mlx5_crypto_gcm_init_qp(struct mlx5_crypto_qp *qp)
{
	volatile struct mlx5_gga_wqe *restrict wqe =
				    (volatile struct mlx5_gga_wqe *)qp->qp_obj.wqes;
	volatile union mlx5_gga_crypto_opaque *opaq = qp->opaque_addr;
	const uint32_t sq_ds = rte_cpu_to_be_32((qp->qp_obj.qp->id << 8) | 4u);
	const uint32_t flags = RTE_BE32(MLX5_COMP_ALWAYS <<
					MLX5_COMP_MODE_OFFSET);
	const uint32_t opaq_lkey = rte_cpu_to_be_32(qp->mr.lkey);
	int i;

	/* All the next fields state should stay constant. */
	for (i = 0; i < qp->entries_n; ++i, ++wqe) {
		wqe->sq_ds = sq_ds;
		wqe->flags = flags;
		wqe->opaque_lkey = opaq_lkey;
		wqe->opaque_vaddr = rte_cpu_to_be_64((uint64_t)(uintptr_t)&opaq[i]);
	}
}

static inline int
mlx5_crypto_gcm_umr_qp_setup(struct rte_cryptodev *dev, struct mlx5_crypto_qp *qp,
			     int socket_id)
{
	struct mlx5_crypto_priv *priv = dev->data->dev_private;
	struct mlx5_devx_qp_attr attr = {0};
	uint32_t ret;
	uint32_t log_wqbb_n;

	/* Set UMR + SEND_EN WQE as maximum same with crypto. */
	log_wqbb_n = rte_log2_u32(qp->entries_n *
			(MLX5_UMR_GCM_WQE_SET_SIZE / MLX5_SEND_WQE_BB));
	attr.pd = priv->cdev->pdn;
	attr.uar_index = mlx5_os_get_devx_uar_page_id(priv->uar.obj);
	attr.cqn = qp->cq_obj.cq->id;
	attr.num_of_receive_wqes = 0;
	attr.num_of_send_wqbbs = RTE_BIT32(log_wqbb_n);
	attr.ts_format =
		mlx5_ts_format_conv(priv->cdev->config.hca_attr.qp_ts_format);
	attr.cd_master = 1;
	ret = mlx5_devx_qp_create(priv->cdev->ctx, &qp->umr_qp_obj,
				  attr.num_of_send_wqbbs * MLX5_SEND_WQE_BB,
				  &attr, socket_id);
	if (ret) {
		DRV_LOG(ERR, "Failed to create UMR QP.");
		return -1;
	}
	if (mlx5_devx_qp2rts(&qp->umr_qp_obj, qp->umr_qp_obj.qp->id)) {
		DRV_LOG(ERR, "Failed to change UMR QP state to RTS.");
		return -1;
	}
	/* Save the UMR WQEBBS for checking the WQE boundary. */
	qp->umr_wqbbs = attr.num_of_send_wqbbs;
	return 0;
}

static int
mlx5_crypto_gcm_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
			 const struct rte_cryptodev_qp_conf *qp_conf,
			 int socket_id)
{
	struct mlx5_crypto_priv *priv = dev->data->dev_private;
	struct mlx5_hca_attr *attr = &priv->cdev->config.hca_attr;
	struct mlx5_crypto_qp *qp;
	struct mlx5_devx_cq_attr cq_attr = {
		.uar_page_id = mlx5_os_get_devx_uar_page_id(priv->uar.obj),
	};
	struct mlx5_devx_qp_attr qp_attr = {
		.pd = priv->cdev->pdn,
		.uar_index = mlx5_os_get_devx_uar_page_id(priv->uar.obj),
		.user_index = qp_id,
	};
	struct mlx5_devx_mkey_attr mkey_attr = {
		.pd = priv->cdev->pdn,
		.umr_en = 1,
		.klm_num = priv->max_klm_num,
	};
	uint32_t log_ops_n = rte_log2_u32(qp_conf->nb_descriptors);
	uint32_t entries = RTE_BIT32(log_ops_n);
	uint32_t alloc_size = sizeof(*qp);
	size_t mr_size, opaq_size;
	void *mr_buf;
	int ret;

	alloc_size = RTE_ALIGN(alloc_size, RTE_CACHE_LINE_SIZE);
	alloc_size += (sizeof(struct rte_crypto_op *) +
		       sizeof(struct mlx5_devx_obj *)) * entries;
	qp = rte_zmalloc_socket(__func__, alloc_size, RTE_CACHE_LINE_SIZE,
				socket_id);
	if (qp == NULL) {
		DRV_LOG(ERR, "Failed to allocate qp memory.");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	qp->priv = priv;
	qp->entries_n = entries;
	if (mlx5_mr_ctrl_init(&qp->mr_ctrl, &priv->cdev->mr_scache.dev_gen,
				  priv->dev_config.socket_id)) {
		DRV_LOG(ERR, "Cannot allocate MR Btree for qp %u.",
			(uint32_t)qp_id);
		rte_errno = ENOMEM;
		goto err;
	}
	/*
	 * The following KLM pointer must be aligned with
	 * MLX5_UMR_KLM_PTR_ALIGN. Aligned opaq_size here
	 * to make the KLM pointer with offset be aligned.
	 */
	opaq_size = RTE_ALIGN(sizeof(union mlx5_gga_crypto_opaque) * entries,
			      MLX5_UMR_KLM_PTR_ALIGN);
	mr_size = (priv->max_klm_num * sizeof(struct mlx5_klm) * entries) + opaq_size;
	mr_buf = rte_calloc(__func__, (size_t)1, mr_size, MLX5_UMR_KLM_PTR_ALIGN);
	if (mr_buf == NULL) {
		DRV_LOG(ERR, "Failed to allocate mr memory.");
		rte_errno = ENOMEM;
		goto err;
	}
	if (priv->reg_mr_cb(priv->cdev->pd, mr_buf, mr_size, &qp->mr) != 0) {
		rte_free(mr_buf);
		DRV_LOG(ERR, "Failed to register opaque MR.");
		rte_errno = ENOMEM;
		goto err;
	}
	qp->opaque_addr = qp->mr.addr;
	qp->klm_array = RTE_PTR_ADD(qp->opaque_addr, opaq_size);
	/*
	 * Triple the CQ size as UMR QP which contains UMR and SEND_EN WQE
	 * will share this CQ .
	 */
	qp->cq_entries_n = rte_align32pow2(entries * 3);
	ret = mlx5_devx_cq_create(priv->cdev->ctx, &qp->cq_obj,
				  rte_log2_u32(qp->cq_entries_n),
				  &cq_attr, socket_id);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to create CQ.");
		goto err;
	}
	qp_attr.cqn = qp->cq_obj.cq->id;
	qp_attr.ts_format = mlx5_ts_format_conv(attr->qp_ts_format);
	qp_attr.num_of_receive_wqes = 0;
	qp_attr.num_of_send_wqbbs = entries;
	qp_attr.mmo = attr->crypto_mmo.crypto_mmo_qp;
	/* Set MMO QP as follower as the input data may depend on UMR. */
	qp_attr.cd_slave_send = 1;
	ret = mlx5_devx_qp_create(priv->cdev->ctx, &qp->qp_obj,
				  qp_attr.num_of_send_wqbbs * MLX5_WQE_SIZE,
				  &qp_attr, socket_id);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to create QP.");
		goto err;
	}
	mlx5_crypto_gcm_init_qp(qp);
	ret = mlx5_devx_qp2rts(&qp->qp_obj, 0);
	if (ret)
		goto err;
	qp->ops = (struct rte_crypto_op **)(qp + 1);
	qp->mkey = (struct mlx5_devx_obj **)(qp->ops + entries);
	if (mlx5_crypto_gcm_umr_qp_setup(dev, qp, socket_id)) {
		DRV_LOG(ERR, "Failed to setup UMR QP.");
		goto err;
	}
	DRV_LOG(INFO, "QP %u: SQN=0x%X CQN=0x%X entries num = %u",
		(uint32_t)qp_id, qp->qp_obj.qp->id, qp->cq_obj.cq->id, entries);
	if (mlx5_crypto_indirect_mkeys_prepare(priv, qp, &mkey_attr,
					       mlx5_crypto_gcm_mkey_klm_update)) {
		DRV_LOG(ERR, "Cannot allocate indirect memory regions.");
		rte_errno = ENOMEM;
		goto err;
	}
	dev->data->queue_pairs[qp_id] = qp;
	return 0;
err:
	mlx5_crypto_gcm_qp_release(dev, qp_id);
	return -1;
}

static __rte_always_inline void
mlx5_crypto_gcm_get_op_info(struct mlx5_crypto_qp *qp,
			    struct rte_crypto_op *op,
			    struct mlx5_crypto_gcm_op_info *op_info)
{
	struct mlx5_crypto_session *sess = CRYPTODEV_GET_SYM_SESS_PRIV(op->sym->session);
	struct rte_mbuf *m_src = op->sym->m_src;
	void *aad_addr = op->sym->aead.aad.data;
	void *tag_addr = op->sym->aead.digest.data;
	void *src_addr = rte_pktmbuf_mtod_offset(m_src, void *, op->sym->aead.data.offset);
	struct rte_mbuf *m_dst = m_src;
	void *dst_addr = src_addr;
	void *expected_aad = NULL;
	void *expected_tag = NULL;
	bool is_enc = sess->op_type == MLX5_CRYPTO_OP_TYPE_ENCRYPTION;
	bool cp_aad = false;
	bool cp_tag = false;

	op_info->is_oop = false;
	op_info->need_umr = false;
	op_info->is_enc = is_enc;
	op_info->digest = NULL;
	op_info->src_addr = aad_addr;
	if (op->sym->m_dst && op->sym->m_dst != m_src) {
		op_info->is_oop = true;
		m_dst = op->sym->m_dst;
		dst_addr = rte_pktmbuf_mtod_offset(m_dst, void *, op->sym->aead.data.offset);
		if (m_dst->nb_segs > 1) {
			op_info->need_umr = true;
			return;
		}
		/*
		 * If the op's mbuf has extra data offset, don't copy AAD to
		 * this area.
		 */
		if (rte_pktmbuf_headroom(m_dst) < sess->aad_len ||
		    op->sym->aead.data.offset) {
			op_info->need_umr = true;
			return;
		}
	}
	if (m_src->nb_segs > 1) {
		op_info->need_umr = true;
		return;
	}
	expected_aad = RTE_PTR_SUB(src_addr, sess->aad_len);
	if (expected_aad != aad_addr) {
		/*
		 * If the op's mbuf has extra data offset, don't copy AAD to
		 * this area.
		 */
		if (sess->aad_len > MLX5_CRYPTO_GCM_MAX_AAD ||
		    sess->aad_len > rte_pktmbuf_headroom(m_src) ||
		    op->sym->aead.data.offset) {
			op_info->need_umr = true;
			return;
		}
		cp_aad = true;
		op_info->src_addr = expected_aad;
	}
	expected_tag = RTE_PTR_ADD(is_enc ? dst_addr : src_addr, op->sym->aead.data.length);
	if (expected_tag != tag_addr) {
		struct rte_mbuf *mbuf = is_enc ? m_dst : m_src;

		/*
		 * If op's mbuf is not fully set as payload, don't copy digest to
		 * the left area.
		 */
		if (rte_pktmbuf_tailroom(mbuf) < sess->tag_len ||
		    rte_pktmbuf_data_len(mbuf) != op->sym->aead.data.length) {
			op_info->need_umr = true;
			return;
		}
		if (is_enc) {
			op_info->digest = expected_tag;
			qp->cpy_tag_op++;
		} else {
			cp_tag = true;
		}
	}
	if (cp_aad)
		memcpy(expected_aad, aad_addr, sess->aad_len);
	if (cp_tag)
		memcpy(expected_tag, tag_addr, sess->tag_len);
}

static __rte_always_inline uint32_t
_mlx5_crypto_gcm_umr_build_mbuf_klm(struct mlx5_crypto_qp *qp,
				    struct rte_mbuf *mbuf,
				    struct mlx5_klm *klm,
				    uint32_t offset,
				    uint32_t *remain)
{
	uint32_t data_len = (rte_pktmbuf_data_len(mbuf) - offset);
	uintptr_t addr = rte_pktmbuf_mtod_offset(mbuf, uintptr_t, offset);

	if (data_len > *remain)
		data_len = *remain;
	*remain -= data_len;
	klm->byte_count = rte_cpu_to_be_32(data_len);
	klm->address = rte_cpu_to_be_64(addr);
	klm->mkey = mlx5_mr_mb2mr(&qp->mr_ctrl, mbuf);
	return klm->mkey;
}

static __rte_always_inline int
mlx5_crypto_gcm_build_mbuf_chain_klms(struct mlx5_crypto_qp *qp,
				      struct rte_crypto_op *op,
				      struct rte_mbuf *mbuf,
				      struct mlx5_klm *klm)
{
	uint32_t remain_len = op->sym->aead.data.length;
	__rte_unused uint32_t nb_segs = mbuf->nb_segs;
	uint32_t klm_n = 0;

	/* mbuf seg num should be less than max_segs_num. */
	MLX5_ASSERT(nb_segs <= qp->priv->max_segs_num);
	/* First mbuf needs to take the data offset. */
	if (unlikely(_mlx5_crypto_gcm_umr_build_mbuf_klm(qp, mbuf, klm,
		     op->sym->aead.data.offset, &remain_len) == UINT32_MAX)) {
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		return 0;
	}
	klm++;
	klm_n++;
	while (remain_len) {
		nb_segs--;
		mbuf = mbuf->next;
		MLX5_ASSERT(mbuf && nb_segs);
		if (unlikely(_mlx5_crypto_gcm_umr_build_mbuf_klm(qp, mbuf, klm,
						0, &remain_len) == UINT32_MAX)) {
			op->status = RTE_CRYPTO_OP_STATUS_ERROR;
			return 0;
		}
		klm++;
		klm_n++;
	}
	return klm_n;
}

static __rte_always_inline int
mlx5_crypto_gcm_build_klm_by_addr(struct mlx5_crypto_qp *qp,
				  struct mlx5_klm *klm,
				  void *addr,
				  uint32_t len)
{
	klm->byte_count = rte_cpu_to_be_32(len);
	klm->address = rte_cpu_to_be_64((uintptr_t)addr);
	klm->mkey = mlx5_mr_addr2mr_bh(&qp->mr_ctrl, (uintptr_t)addr);
	if (klm->mkey == UINT32_MAX)
		return 0;
	return 1;
}

static __rte_always_inline int
mlx5_crypto_gcm_build_op_klm(struct mlx5_crypto_qp *qp,
			     struct rte_crypto_op *op,
			     struct mlx5_crypto_gcm_op_info *op_info,
			     struct mlx5_klm *klm,
			     uint32_t *len)
{
	struct mlx5_crypto_session *sess = CRYPTODEV_GET_SYM_SESS_PRIV(op->sym->session);
	struct mlx5_klm *digest = NULL, *aad = NULL;
	uint32_t total_len = op->sym->aead.data.length + sess->aad_len + sess->tag_len;
	uint32_t klm_n = 0, klm_src = 0, klm_dst = 0;

	/* Build AAD KLM. */
	aad = klm;
	if (!mlx5_crypto_gcm_build_klm_by_addr(qp, aad, op->sym->aead.aad.data, sess->aad_len))
		return 0;
	klm_n++;
	/* Build src mubf KLM. */
	klm_src = mlx5_crypto_gcm_build_mbuf_chain_klms(qp, op, op->sym->m_src, &klm[klm_n]);
	if (!klm_src)
		return 0;
	klm_n += klm_src;
	/* Reserve digest KLM if needed. */
	if (!op_info->is_oop ||
	    sess->op_type == MLX5_CRYPTO_OP_TYPE_DECRYPTION) {
		digest = &klm[klm_n];
		klm_n++;
	}
	/* Build dst mbuf KLM. */
	if (op_info->is_oop) {
		klm[klm_n] = *aad;
		klm_n++;
		klm_dst = mlx5_crypto_gcm_build_mbuf_chain_klms(qp, op, op->sym->m_dst,
								&klm[klm_n]);
		if (!klm_dst)
			return 0;
		klm_n += klm_dst;
		total_len += (op->sym->aead.data.length + sess->aad_len);
	}
	/* Update digest at the end if it is not set. */
	if (!digest) {
		digest = &klm[klm_n];
		klm_n++;
	}
	/* Build digest KLM. */
	if (!mlx5_crypto_gcm_build_klm_by_addr(qp, digest, op->sym->aead.digest.data,
					       sess->tag_len))
		return 0;
	*len = total_len;
	return klm_n;
}

static __rte_always_inline struct mlx5_wqe_cseg *
mlx5_crypto_gcm_get_umr_wqe(struct mlx5_crypto_qp *qp)
{
	uint32_t wqe_offset = qp->umr_pi & (qp->umr_wqbbs - 1);
	uint32_t left_wqbbs = qp->umr_wqbbs - wqe_offset;
	struct mlx5_wqe_cseg *wqe;

	/* If UMR WQE is near the boundary. */
	if (left_wqbbs < MLX5_UMR_GCM_WQE_STRIDE) {
		/* Append NOP WQE as the left WQEBBS is not enough for UMR. */
		wqe = RTE_PTR_ADD(qp->umr_qp_obj.umem_buf, wqe_offset * MLX5_SEND_WQE_BB);
		wqe->opcode = rte_cpu_to_be_32(MLX5_OPCODE_NOP | ((uint32_t)qp->umr_pi << 8));
		wqe->sq_ds = rte_cpu_to_be_32((qp->umr_qp_obj.qp->id << 8) | (left_wqbbs << 2));
		wqe->flags = RTE_BE32(0);
		wqe->misc = RTE_BE32(0);
		qp->umr_pi += left_wqbbs;
		wqe_offset = qp->umr_pi & (qp->umr_wqbbs - 1);
	}
	wqe_offset *= MLX5_SEND_WQE_BB;
	return RTE_PTR_ADD(qp->umr_qp_obj.umem_buf, wqe_offset);
}

static __rte_always_inline int
mlx5_crypto_gcm_build_umr(struct mlx5_crypto_qp *qp,
			  struct rte_crypto_op *op,
			  uint32_t idx,
			  struct mlx5_crypto_gcm_op_info *op_info,
			  struct mlx5_crypto_gcm_data *data)
{
	struct mlx5_crypto_priv *priv = qp->priv;
	struct mlx5_crypto_session *sess = CRYPTODEV_GET_SYM_SESS_PRIV(op->sym->session);
	struct mlx5_wqe_cseg *wqe;
	struct mlx5_wqe_umr_cseg *ucseg;
	struct mlx5_wqe_mkey_cseg *mkc;
	struct mlx5_klm *iklm;
	struct mlx5_klm *klm = &qp->klm_array[idx * priv->max_klm_num];
	uint16_t klm_size, klm_align;
	uint32_t total_len;

	/* Build KLM base on the op. */
	klm_size = mlx5_crypto_gcm_build_op_klm(qp, op, op_info, klm, &total_len);
	if (!klm_size)
		return -EINVAL;
	klm_align = RTE_ALIGN(klm_size, 4);
	/* Get UMR WQE memory. */
	wqe = mlx5_crypto_gcm_get_umr_wqe(qp);
	memset(wqe, 0, MLX5_UMR_GCM_WQE_SIZE);
	/* Set WQE control seg. Non-inline KLM UMR WQE size must be 9 WQE_DS. */
	wqe->opcode = rte_cpu_to_be_32(MLX5_OPCODE_UMR | ((uint32_t)qp->umr_pi << 8));
	wqe->sq_ds = rte_cpu_to_be_32((qp->umr_qp_obj.qp->id << 8) | 9);
	wqe->flags = RTE_BE32(MLX5_COMP_ONLY_FIRST_ERR << MLX5_COMP_MODE_OFFSET);
	wqe->misc = rte_cpu_to_be_32(qp->mkey[idx]->id);
	/* Set UMR WQE control seg. */
	ucseg = (struct mlx5_wqe_umr_cseg *)(wqe + 1);
	ucseg->mkey_mask |= RTE_BE64(1u << 0);
	ucseg->ko_to_bs = rte_cpu_to_be_32(klm_align << MLX5_UMRC_KO_OFFSET);
	/* Set mkey context seg. */
	mkc = (struct mlx5_wqe_mkey_cseg *)(ucseg + 1);
	mkc->len = rte_cpu_to_be_64(total_len);
	mkc->qpn_mkey = rte_cpu_to_be_32(0xffffff00 | (qp->mkey[idx]->id & 0xff));
	/* Set UMR pointer to data seg. */
	iklm = (struct mlx5_klm *)(mkc + 1);
	iklm->address = rte_cpu_to_be_64((uintptr_t)((char *)klm));
	iklm->mkey = rte_cpu_to_be_32(qp->mr.lkey);
	data->src_mkey = rte_cpu_to_be_32(qp->mkey[idx]->id);
	data->dst_mkey = data->src_mkey;
	data->src_addr = 0;
	data->src_bytes = sess->aad_len + op->sym->aead.data.length;
	data->dst_bytes = data->src_bytes;
	if (op_info->is_enc)
		data->dst_bytes += sess->tag_len;
	else
		data->src_bytes += sess->tag_len;
	if (op_info->is_oop)
		data->dst_addr = (void *)(uintptr_t)(data->src_bytes);
	else
		data->dst_addr = 0;
	/* Clear the padding memory. */
	memset(&klm[klm_size], 0, sizeof(struct mlx5_klm) * (klm_align - klm_size));
	/* Update PI and WQE */
	qp->umr_pi += MLX5_UMR_GCM_WQE_STRIDE;
	qp->umr_wqe = (uint8_t *)wqe;
	return 0;
}

static __rte_always_inline void
mlx5_crypto_gcm_build_send_en(struct mlx5_crypto_qp *qp)
{
	uint32_t wqe_offset = (qp->umr_pi & (qp->umr_wqbbs - 1)) * MLX5_SEND_WQE_BB;
	struct mlx5_wqe_cseg *cs = RTE_PTR_ADD(qp->umr_qp_obj.wqes, wqe_offset);
	struct mlx5_wqe_qseg *qs = RTE_PTR_ADD(cs, sizeof(struct mlx5_wqe_cseg));

	cs->opcode = rte_cpu_to_be_32(MLX5_OPCODE_SEND_EN | ((uint32_t)qp->umr_pi << 8));
	cs->sq_ds = rte_cpu_to_be_32((qp->umr_qp_obj.qp->id << 8) | 2);
	/*
	 * No need to generate the SEND_EN CQE as we want only GGA CQE
	 * in the CQ normally. We can compare qp->last_send_gga_pi with
	 * qp->pi to know if all SEND_EN be consumed.
	 */
	cs->flags = RTE_BE32((MLX5_COMP_ONLY_FIRST_ERR << MLX5_COMP_MODE_OFFSET) |
			MLX5_WQE_CTRL_INITIATOR_SMALL_FENCE);
	cs->misc = RTE_BE32(0);
	qs->max_index = rte_cpu_to_be_32(qp->pi);
	qs->qpn_cqn = rte_cpu_to_be_32(qp->qp_obj.qp->id);
	qp->umr_wqe = (uint8_t *)cs;
	qp->umr_pi += 1;
}

static __rte_always_inline void
mlx5_crypto_gcm_wqe_set(struct mlx5_crypto_qp *qp,
			struct rte_crypto_op *op,
			uint32_t idx,
			struct mlx5_crypto_gcm_data *data)
{
	struct mlx5_crypto_session *sess = CRYPTODEV_GET_SYM_SESS_PRIV(op->sym->session);
	struct mlx5_gga_wqe *wqe = &((struct mlx5_gga_wqe *)qp->qp_obj.wqes)[idx];
	union mlx5_gga_crypto_opaque *opaq = qp->opaque_addr;

	memcpy(opaq[idx].cp.iv,
		rte_crypto_op_ctod_offset(op, uint8_t *, sess->iv_offset), sess->iv_len);
	opaq[idx].cp.tag_size = sess->wqe_tag_len;
	opaq[idx].cp.aad_size = sess->wqe_aad_len;
	/* Update control seg. */
	wqe->opcode = rte_cpu_to_be_32(MLX5_MMO_CRYPTO_OPC + (qp->pi << 8));
	wqe->gga_ctrl1 = sess->mmo_ctrl;
	wqe->gga_ctrl2 = sess->dek_id;
	wqe->flags = RTE_BE32(MLX5_COMP_ONLY_FIRST_ERR << MLX5_COMP_MODE_OFFSET);
	/* Update op_info seg. */
	wqe->gather.bcount = rte_cpu_to_be_32(data->src_bytes);
	wqe->gather.lkey = data->src_mkey;
	wqe->gather.pbuf = rte_cpu_to_be_64((uintptr_t)data->src_addr);
	/* Update output seg. */
	wqe->scatter.bcount = rte_cpu_to_be_32(data->dst_bytes);
	wqe->scatter.lkey = data->dst_mkey;
	wqe->scatter.pbuf = rte_cpu_to_be_64((uintptr_t)data->dst_addr);
	qp->wqe = (uint8_t *)wqe;
}

static uint16_t
mlx5_crypto_gcm_enqueue_burst(void *queue_pair,
			      struct rte_crypto_op **ops,
			      uint16_t nb_ops)
{
	struct mlx5_crypto_qp *qp = queue_pair;
	struct mlx5_crypto_session *sess;
	struct mlx5_crypto_priv *priv = qp->priv;
	struct mlx5_crypto_gcm_tag_cpy_info *tag;
	struct mlx5_crypto_gcm_data gcm_data;
	struct rte_crypto_op *op;
	struct mlx5_crypto_gcm_op_info op_info;
	uint16_t mask = qp->entries_n - 1;
	uint16_t remain = qp->entries_n - (qp->pi - qp->qp_ci);
	uint32_t idx;
	uint16_t umr_cnt = 0;

	if (remain < nb_ops)
		nb_ops = remain;
	else
		remain = nb_ops;
	if (unlikely(remain == 0))
		return 0;
	do {
		op = *ops++;
		sess = CRYPTODEV_GET_SYM_SESS_PRIV(op->sym->session);
		idx = qp->pi & mask;
		mlx5_crypto_gcm_get_op_info(qp, op, &op_info);
		if (!op_info.need_umr) {
			gcm_data.src_addr = op_info.src_addr;
			gcm_data.src_bytes = op->sym->aead.data.length + sess->aad_len;
			gcm_data.src_mkey = mlx5_mr_mb2mr(&qp->mr_ctrl, op->sym->m_src);
			if (op_info.is_oop) {
				gcm_data.dst_addr = RTE_PTR_SUB
					(rte_pktmbuf_mtod_offset(op->sym->m_dst,
					 void *, op->sym->aead.data.offset), sess->aad_len);
				gcm_data.dst_mkey = mlx5_mr_mb2mr(&qp->mr_ctrl, op->sym->m_dst);
			} else {
				gcm_data.dst_addr = gcm_data.src_addr;
				gcm_data.dst_mkey = gcm_data.src_mkey;
			}
			gcm_data.dst_bytes = gcm_data.src_bytes;
			if (op_info.is_enc)
				gcm_data.dst_bytes += sess->tag_len;
			else
				gcm_data.src_bytes += sess->tag_len;
		} else {
			if (unlikely(mlx5_crypto_gcm_build_umr(qp, op, idx,
							&op_info, &gcm_data))) {
				qp->stats.enqueue_err_count++;
				if (remain != nb_ops) {
					qp->stats.enqueued_count -= remain;
					break;
				}
				return 0;
			}
			umr_cnt++;
		}
		mlx5_crypto_gcm_wqe_set(qp, op, idx, &gcm_data);
		if (op_info.digest) {
			tag = (struct mlx5_crypto_gcm_tag_cpy_info *)op->sym->aead.digest.data;
			tag->digest = op_info.digest;
			tag->tag_len = sess->tag_len;
			op->status = MLX5_CRYPTO_OP_STATUS_GCM_TAG_COPY;
		} else {
			op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
		}
		qp->ops[idx] = op;
		qp->pi++;
	} while (--remain);
	qp->stats.enqueued_count += nb_ops;
	/* Update the last GGA cseg with COMP. */
	((struct mlx5_wqe_cseg *)qp->wqe)->flags =
		RTE_BE32(MLX5_COMP_ALWAYS << MLX5_COMP_MODE_OFFSET);
	/* Only when there are no pending SEND_EN WQEs in background. */
	if (!umr_cnt && !qp->has_umr) {
		mlx5_doorbell_ring(&priv->uar.bf_db, *(volatile uint64_t *)qp->wqe,
				   qp->pi, &qp->qp_obj.db_rec[MLX5_SND_DBR],
				   !priv->uar.dbnc);
	} else {
		mlx5_crypto_gcm_build_send_en(qp);
		mlx5_doorbell_ring(&priv->uar.bf_db, *(volatile uint64_t *)qp->umr_wqe,
				   qp->umr_pi, &qp->umr_qp_obj.db_rec[MLX5_SND_DBR],
				   !priv->uar.dbnc);
		qp->last_gga_pi = qp->pi;
		qp->has_umr = true;
	}
	return nb_ops;
}

static __rte_noinline void
mlx5_crypto_gcm_cqe_err_handle(struct mlx5_crypto_qp *qp, struct rte_crypto_op *op)
{
	uint8_t op_code;
	const uint32_t idx = qp->cq_ci & (qp->entries_n - 1);
	volatile struct mlx5_err_cqe *cqe = (volatile struct mlx5_err_cqe *)
							&qp->cq_obj.cqes[idx];

	op_code = rte_be_to_cpu_32(cqe->s_wqe_opcode_qpn) >> MLX5_CQ_INDEX_WIDTH;
	DRV_LOG(ERR, "CQE ERR:0x%x, Vendor_ERR:0x%x, OP:0x%x, QPN:0x%x, WQE_CNT:0x%x",
		cqe->syndrome, cqe->vendor_err_synd, op_code,
		(rte_be_to_cpu_32(cqe->s_wqe_opcode_qpn) & 0xffffff),
		rte_be_to_cpu_16(cqe->wqe_counter));
	if (op && op_code == MLX5_OPCODE_MMO) {
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		qp->stats.dequeue_err_count++;
	}
}

static __rte_always_inline void
mlx5_crypto_gcm_fill_op(struct mlx5_crypto_qp *qp,
			struct rte_crypto_op **ops,
			uint16_t orci,
			uint16_t rci,
			uint16_t op_mask)
{
	uint16_t n;

	orci &= op_mask;
	rci &= op_mask;
	if (unlikely(orci > rci)) {
		n = op_mask - orci + 1;
		memcpy(ops, &qp->ops[orci], n * sizeof(*ops));
		orci = 0;
	} else {
		n = 0;
	}
	/* rci can be 0 here, memcpy will skip that. */
	memcpy(&ops[n], &qp->ops[orci], (rci - orci) * sizeof(*ops));
}

static __rte_always_inline void
mlx5_crypto_gcm_cpy_tag(struct mlx5_crypto_qp *qp,
			uint16_t orci,
			uint16_t rci,
			uint16_t op_mask)
{
	struct rte_crypto_op *op;
	struct mlx5_crypto_gcm_tag_cpy_info *tag;

	while (qp->cpy_tag_op && orci != rci) {
		op = qp->ops[orci & op_mask];
		if (op->status == MLX5_CRYPTO_OP_STATUS_GCM_TAG_COPY) {
			tag = (struct mlx5_crypto_gcm_tag_cpy_info *)op->sym->aead.digest.data;
			memcpy(op->sym->aead.digest.data, tag->digest, tag->tag_len);
			op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
			qp->cpy_tag_op--;
		}
		orci++;
	}
}

static uint16_t
mlx5_crypto_gcm_dequeue_burst(void *queue_pair,
			      struct rte_crypto_op **ops,
			      uint16_t nb_ops)
{
	struct mlx5_crypto_qp *qp = queue_pair;
	volatile struct mlx5_cqe *restrict cqe;
	const unsigned int cq_size = qp->cq_entries_n;
	const unsigned int mask = cq_size - 1;
	const unsigned int op_mask = qp->entries_n - 1;
	uint32_t idx;
	uint32_t next_idx = qp->cq_ci & mask;
	uint16_t reported_ci = qp->reported_ci;
	uint16_t qp_ci = qp->qp_ci;
	const uint16_t max = RTE_MIN((uint16_t)(qp->pi - reported_ci), nb_ops);
	uint16_t op_num = 0;
	int ret;

	if (unlikely(max == 0))
		return 0;
	while (qp_ci - reported_ci < max) {
		idx = next_idx;
		next_idx = (qp->cq_ci + 1) & mask;
		cqe = &qp->cq_obj.cqes[idx];
		ret = check_cqe(cqe, cq_size, qp->cq_ci);
		if (unlikely(ret != MLX5_CQE_STATUS_SW_OWN)) {
			if (unlikely(ret != MLX5_CQE_STATUS_HW_OWN))
				mlx5_crypto_gcm_cqe_err_handle(qp,
						qp->ops[reported_ci & op_mask]);
			break;
		}
		qp_ci = rte_be_to_cpu_16(cqe->wqe_counter) + 1;
		if (qp->has_umr &&
		    (qp->last_gga_pi + 1) == qp_ci)
			qp->has_umr = false;
		qp->cq_ci++;
	}
	/* If wqe_counter changed, means CQE handled. */
	if (likely(qp->qp_ci != qp_ci)) {
		qp->qp_ci = qp_ci;
		rte_io_wmb();
		qp->cq_obj.db_rec[0] = rte_cpu_to_be_32(qp->cq_ci);
	}
	/* If reported_ci is not same with qp_ci, means op retrieved. */
	if (qp_ci != reported_ci) {
		op_num = RTE_MIN((uint16_t)(qp_ci - reported_ci), max);
		reported_ci += op_num;
		mlx5_crypto_gcm_cpy_tag(qp, qp->reported_ci, reported_ci, op_mask);
		mlx5_crypto_gcm_fill_op(qp, ops, qp->reported_ci, reported_ci, op_mask);
		qp->stats.dequeued_count += op_num;
		qp->reported_ci = reported_ci;
	}
	return op_num;
}

int
mlx5_crypto_gcm_init(struct mlx5_crypto_priv *priv)
{
	struct mlx5_common_device *cdev = priv->cdev;
	struct rte_cryptodev *crypto_dev = priv->crypto_dev;
	struct rte_cryptodev_ops *dev_ops = crypto_dev->dev_ops;
	int ret;

	/* Override AES-GCM specified ops. */
	dev_ops->sym_session_configure = mlx5_crypto_sym_gcm_session_configure;
	mlx5_os_set_reg_mr_cb(&priv->reg_mr_cb, &priv->dereg_mr_cb);
	dev_ops->queue_pair_setup = mlx5_crypto_gcm_qp_setup;
	dev_ops->queue_pair_release = mlx5_crypto_gcm_qp_release;
	crypto_dev->dequeue_burst = mlx5_crypto_gcm_dequeue_burst;
	crypto_dev->enqueue_burst = mlx5_crypto_gcm_enqueue_burst;
	priv->max_klm_num = RTE_ALIGN((priv->max_segs_num + 1) * 2 + 1, MLX5_UMR_KLM_NUM_ALIGN);
	/* Generate GCM capability. */
	ret = mlx5_crypto_generate_gcm_cap(&cdev->config.hca_attr.crypto_mmo,
					   mlx5_crypto_gcm_caps);
	if (ret) {
		DRV_LOG(ERR, "No enough AES-GCM cap.");
		return -1;
	}
	priv->caps = mlx5_crypto_gcm_caps;
	return 0;
}
