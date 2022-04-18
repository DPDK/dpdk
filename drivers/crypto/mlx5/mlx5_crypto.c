/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */

#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_eal_paging.h>
#include <rte_errno.h>
#include <rte_log.h>
#include <rte_bus_pci.h>
#include <rte_memory.h>

#include <mlx5_glue.h>
#include <mlx5_common.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_common_os.h>

#include "mlx5_crypto_utils.h"
#include "mlx5_crypto.h"

#define MLX5_CRYPTO_DRIVER_NAME crypto_mlx5
#define MLX5_CRYPTO_LOG_NAME pmd.crypto.mlx5
#define MLX5_CRYPTO_MAX_QPS 128
#define MLX5_CRYPTO_MAX_SEGS 56

#define MLX5_CRYPTO_FEATURE_FLAGS \
	(RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO | RTE_CRYPTODEV_FF_HW_ACCELERATED | \
	 RTE_CRYPTODEV_FF_IN_PLACE_SGL | RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT | \
	 RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT | \
	 RTE_CRYPTODEV_FF_OOP_LB_IN_SGL_OUT | \
	 RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT | \
	 RTE_CRYPTODEV_FF_CIPHER_WRAPPED_KEY | \
	 RTE_CRYPTODEV_FF_CIPHER_MULTIPLE_DATA_UNITS)

TAILQ_HEAD(mlx5_crypto_privs, mlx5_crypto_priv) mlx5_crypto_priv_list =
				TAILQ_HEAD_INITIALIZER(mlx5_crypto_priv_list);
static pthread_mutex_t priv_list_lock;

int mlx5_crypto_logtype;

uint8_t mlx5_crypto_driver_id;

const struct rte_cryptodev_capabilities mlx5_crypto_caps[] = {
	{		/* AES XTS */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_AES_XTS,
				.block_size = 16,
				.key_size = {
					.min = 32,
					.max = 64,
					.increment = 32
				},
				.iv_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.dataunit_set =
				RTE_CRYPTO_CIPHER_DATA_UNIT_LEN_512_BYTES |
				RTE_CRYPTO_CIPHER_DATA_UNIT_LEN_4096_BYTES |
				RTE_CRYPTO_CIPHER_DATA_UNIT_LEN_1_MEGABYTES,
			}, }
		}, }
	},
};

static const char mlx5_crypto_drv_name[] = RTE_STR(MLX5_CRYPTO_DRIVER_NAME);

static const struct rte_driver mlx5_drv = {
	.name = mlx5_crypto_drv_name,
	.alias = mlx5_crypto_drv_name
};

static struct cryptodev_driver mlx5_cryptodev_driver;

struct mlx5_crypto_session {
	uint32_t bs_bpt_eo_es;
	/**< bsf_size, bsf_p_type, encryption_order and encryption standard,
	 * saved in big endian format.
	 */
	uint32_t bsp_res;
	/**< crypto_block_size_pointer and reserved 24 bits saved in big
	 * endian format.
	 */
	uint32_t iv_offset:16;
	/**< Starting point for Initialisation Vector. */
	struct mlx5_crypto_dek *dek; /**< Pointer to dek struct. */
	uint32_t dek_id; /**< DEK ID */
} __rte_packed;

static void
mlx5_crypto_dev_infos_get(struct rte_cryptodev *dev,
			  struct rte_cryptodev_info *dev_info)
{
	RTE_SET_USED(dev);
	if (dev_info != NULL) {
		dev_info->driver_id = mlx5_crypto_driver_id;
		dev_info->feature_flags = MLX5_CRYPTO_FEATURE_FLAGS;
		dev_info->capabilities = mlx5_crypto_caps;
		dev_info->max_nb_queue_pairs = MLX5_CRYPTO_MAX_QPS;
		dev_info->min_mbuf_headroom_req = 0;
		dev_info->min_mbuf_tailroom_req = 0;
		dev_info->sym.max_nb_sessions = 0;
		/*
		 * If 0, the device does not have any limitation in number of
		 * sessions that can be used.
		 */
	}
}

static int
mlx5_crypto_dev_configure(struct rte_cryptodev *dev,
			  struct rte_cryptodev_config *config)
{
	struct mlx5_crypto_priv *priv = dev->data->dev_private;

	if (config == NULL) {
		DRV_LOG(ERR, "Invalid crypto dev configure parameters.");
		return -EINVAL;
	}
	if ((config->ff_disable & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO) != 0) {
		DRV_LOG(ERR,
			"Disabled symmetric crypto feature is not supported.");
		return -ENOTSUP;
	}
	if (mlx5_crypto_dek_setup(priv) != 0) {
		DRV_LOG(ERR, "Dek hash list creation has failed.");
		return -ENOMEM;
	}
	priv->dev_config = *config;
	DRV_LOG(DEBUG, "Device %u was configured.", dev->driver_id);
	return 0;
}

static void
mlx5_crypto_dev_stop(struct rte_cryptodev *dev)
{
	RTE_SET_USED(dev);
}

static int
mlx5_crypto_dev_start(struct rte_cryptodev *dev)
{
	struct mlx5_crypto_priv *priv = dev->data->dev_private;

	return mlx5_dev_mempool_subscribe(priv->cdev);
}

static int
mlx5_crypto_dev_close(struct rte_cryptodev *dev)
{
	struct mlx5_crypto_priv *priv = dev->data->dev_private;

	mlx5_crypto_dek_unset(priv);
	DRV_LOG(DEBUG, "Device %u was closed.", dev->driver_id);
	return 0;
}

static unsigned int
mlx5_crypto_sym_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct mlx5_crypto_session);
}

static int
mlx5_crypto_sym_session_configure(struct rte_cryptodev *dev,
				  struct rte_crypto_sym_xform *xform,
				  struct rte_cryptodev_sym_session *session,
				  struct rte_mempool *mp)
{
	struct mlx5_crypto_priv *priv = dev->data->dev_private;
	struct mlx5_crypto_session *sess_private_data;
	struct rte_crypto_cipher_xform *cipher;
	uint8_t encryption_order;
	int ret;

	if (unlikely(xform->next != NULL)) {
		DRV_LOG(ERR, "Xform next is not supported.");
		return -ENOTSUP;
	}
	if (unlikely((xform->type != RTE_CRYPTO_SYM_XFORM_CIPHER) ||
		     (xform->cipher.algo != RTE_CRYPTO_CIPHER_AES_XTS))) {
		DRV_LOG(ERR, "Only AES-XTS algorithm is supported.");
		return -ENOTSUP;
	}
	ret = rte_mempool_get(mp, (void *)&sess_private_data);
	if (ret != 0) {
		DRV_LOG(ERR,
			"Failed to get session %p private data from mempool.",
			sess_private_data);
		return -ENOMEM;
	}
	cipher = &xform->cipher;
	sess_private_data->dek = mlx5_crypto_dek_prepare(priv, cipher);
	if (sess_private_data->dek == NULL) {
		rte_mempool_put(mp, sess_private_data);
		DRV_LOG(ERR, "Failed to prepare dek.");
		return -ENOMEM;
	}
	if (cipher->op == RTE_CRYPTO_CIPHER_OP_ENCRYPT)
		encryption_order = MLX5_ENCRYPTION_ORDER_ENCRYPTED_RAW_MEMORY;
	else
		encryption_order = MLX5_ENCRYPTION_ORDER_ENCRYPTED_RAW_WIRE;
	sess_private_data->bs_bpt_eo_es = rte_cpu_to_be_32
			(MLX5_BSF_SIZE_64B << MLX5_BSF_SIZE_OFFSET |
			 MLX5_BSF_P_TYPE_CRYPTO << MLX5_BSF_P_TYPE_OFFSET |
			 encryption_order << MLX5_ENCRYPTION_ORDER_OFFSET |
			 MLX5_ENCRYPTION_STANDARD_AES_XTS);
	switch (xform->cipher.dataunit_len) {
	case 0:
		sess_private_data->bsp_res = 0;
		break;
	case 512:
		sess_private_data->bsp_res = rte_cpu_to_be_32
					     ((uint32_t)MLX5_BLOCK_SIZE_512B <<
					     MLX5_BLOCK_SIZE_OFFSET);
		break;
	case 4096:
		sess_private_data->bsp_res = rte_cpu_to_be_32
					     ((uint32_t)MLX5_BLOCK_SIZE_4096B <<
					     MLX5_BLOCK_SIZE_OFFSET);
		break;
	case 1048576:
		sess_private_data->bsp_res = rte_cpu_to_be_32
					     ((uint32_t)MLX5_BLOCK_SIZE_1MB <<
					     MLX5_BLOCK_SIZE_OFFSET);
		break;
	default:
		DRV_LOG(ERR, "Cipher data unit length is not supported.");
		return -ENOTSUP;
	}
	sess_private_data->iv_offset = cipher->iv.offset;
	sess_private_data->dek_id =
			rte_cpu_to_be_32(sess_private_data->dek->obj->id &
					 0xffffff);
	set_sym_session_private_data(session, dev->driver_id,
				     sess_private_data);
	DRV_LOG(DEBUG, "Session %p was configured.", sess_private_data);
	return 0;
}

static void
mlx5_crypto_sym_session_clear(struct rte_cryptodev *dev,
			      struct rte_cryptodev_sym_session *sess)
{
	struct mlx5_crypto_priv *priv = dev->data->dev_private;
	struct mlx5_crypto_session *spriv = get_sym_session_private_data(sess,
								dev->driver_id);

	if (unlikely(spriv == NULL)) {
		DRV_LOG(ERR, "Failed to get session %p private data.", spriv);
		return;
	}
	mlx5_crypto_dek_destroy(priv, spriv->dek);
	set_sym_session_private_data(sess, dev->driver_id, NULL);
	rte_mempool_put(rte_mempool_from_obj(spriv), spriv);
	DRV_LOG(DEBUG, "Session %p was cleared.", spriv);
}

static void
mlx5_crypto_indirect_mkeys_release(struct mlx5_crypto_qp *qp, uint16_t n)
{
	uint16_t i;

	for (i = 0; i < n; i++)
		if (qp->mkey[i])
			claim_zero(mlx5_devx_cmd_destroy(qp->mkey[i]));
}

static void
mlx5_crypto_qp_release(struct mlx5_crypto_qp *qp)
{
	if (qp == NULL)
		return;
	mlx5_devx_qp_destroy(&qp->qp_obj);
	mlx5_mr_btree_free(&qp->mr_ctrl.cache_bh);
	mlx5_devx_cq_destroy(&qp->cq_obj);
	rte_free(qp);
}

static int
mlx5_crypto_queue_pair_release(struct rte_cryptodev *dev, uint16_t qp_id)
{
	struct mlx5_crypto_qp *qp = dev->data->queue_pairs[qp_id];

	mlx5_crypto_indirect_mkeys_release(qp, qp->entries_n);
	mlx5_crypto_qp_release(qp);
	dev->data->queue_pairs[qp_id] = NULL;
	return 0;
}

static __rte_noinline uint32_t
mlx5_crypto_get_block_size(struct rte_crypto_op *op)
{
	uint32_t bl = op->sym->cipher.data.length;

	switch (bl) {
	case (1 << 20):
		return RTE_BE32(MLX5_BLOCK_SIZE_1MB << MLX5_BLOCK_SIZE_OFFSET);
	case (1 << 12):
		return RTE_BE32(MLX5_BLOCK_SIZE_4096B <<
				MLX5_BLOCK_SIZE_OFFSET);
	case (1 << 9):
		return RTE_BE32(MLX5_BLOCK_SIZE_512B << MLX5_BLOCK_SIZE_OFFSET);
	default:
		DRV_LOG(ERR, "Unknown block size: %u.", bl);
		return UINT32_MAX;
	}
}

static __rte_always_inline uint32_t
mlx5_crypto_klm_set(struct mlx5_crypto_qp *qp, struct rte_mbuf *mbuf,
		    struct mlx5_wqe_dseg *klm, uint32_t offset,
		    uint32_t *remain)
{
	uint32_t data_len = (rte_pktmbuf_data_len(mbuf) - offset);
	uintptr_t addr = rte_pktmbuf_mtod_offset(mbuf, uintptr_t, offset);

	if (data_len > *remain)
		data_len = *remain;
	*remain -= data_len;
	klm->bcount = rte_cpu_to_be_32(data_len);
	klm->pbuf = rte_cpu_to_be_64(addr);
	klm->lkey = mlx5_mr_mb2mr(&qp->mr_ctrl, mbuf);
	return klm->lkey;

}

static __rte_always_inline uint32_t
mlx5_crypto_klms_set(struct mlx5_crypto_qp *qp, struct rte_crypto_op *op,
		     struct rte_mbuf *mbuf, struct mlx5_wqe_dseg *klm)
{
	uint32_t remain_len = op->sym->cipher.data.length;
	uint32_t nb_segs = mbuf->nb_segs;
	uint32_t klm_n = 1u;

	/* First mbuf needs to take the cipher offset. */
	if (unlikely(mlx5_crypto_klm_set(qp, mbuf, klm,
		     op->sym->cipher.data.offset, &remain_len) == UINT32_MAX)) {
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		return 0;
	}
	while (remain_len) {
		nb_segs--;
		mbuf = mbuf->next;
		if (unlikely(mbuf == NULL || nb_segs == 0)) {
			op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			return 0;
		}
		if (unlikely(mlx5_crypto_klm_set(qp, mbuf, ++klm, 0,
						 &remain_len) == UINT32_MAX)) {
			op->status = RTE_CRYPTO_OP_STATUS_ERROR;
			return 0;
		}
		klm_n++;
	}
	return klm_n;
}

static __rte_always_inline int
mlx5_crypto_wqe_set(struct mlx5_crypto_priv *priv,
			 struct mlx5_crypto_qp *qp,
			 struct rte_crypto_op *op,
			 struct mlx5_umr_wqe *umr)
{
	struct mlx5_crypto_session *sess = get_sym_session_private_data
				(op->sym->session, mlx5_crypto_driver_id);
	struct mlx5_wqe_cseg *cseg = &umr->ctr;
	struct mlx5_wqe_mkey_cseg *mkc = &umr->mkc;
	struct mlx5_wqe_dseg *klms = &umr->kseg[0];
	struct mlx5_wqe_umr_bsf_seg *bsf = ((struct mlx5_wqe_umr_bsf_seg *)
				      RTE_PTR_ADD(umr, priv->umr_wqe_size)) - 1;
	uint32_t ds;
	bool ipl = op->sym->m_dst == NULL || op->sym->m_dst == op->sym->m_src;
	/* Set UMR WQE. */
	uint32_t klm_n = mlx5_crypto_klms_set(qp, op,
				   ipl ? op->sym->m_src : op->sym->m_dst, klms);

	if (unlikely(klm_n == 0))
		return 0;
	bsf->bs_bpt_eo_es = sess->bs_bpt_eo_es;
	if (unlikely(!sess->bsp_res)) {
		bsf->bsp_res = mlx5_crypto_get_block_size(op);
		if (unlikely(bsf->bsp_res == UINT32_MAX)) {
			op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			return 0;
		}
	} else {
		bsf->bsp_res = sess->bsp_res;
	}
	bsf->raw_data_size = rte_cpu_to_be_32(op->sym->cipher.data.length);
	memcpy(bsf->xts_initial_tweak,
	       rte_crypto_op_ctod_offset(op, uint8_t *, sess->iv_offset), 16);
	bsf->res_dp = sess->dek_id;
	mkc->len = rte_cpu_to_be_64(op->sym->cipher.data.length);
	cseg->opcode = rte_cpu_to_be_32((qp->db_pi << 8) | MLX5_OPCODE_UMR);
	qp->db_pi += priv->umr_wqe_stride;
	/* Set RDMA_WRITE WQE. */
	cseg = RTE_PTR_ADD(cseg, priv->umr_wqe_size);
	klms = RTE_PTR_ADD(cseg, sizeof(struct mlx5_rdma_write_wqe));
	if (!ipl) {
		klm_n = mlx5_crypto_klms_set(qp, op, op->sym->m_src, klms);
		if (unlikely(klm_n == 0))
			return 0;
	} else {
		memcpy(klms, &umr->kseg[0], sizeof(*klms) * klm_n);
	}
	ds = 2 + klm_n;
	cseg->sq_ds = rte_cpu_to_be_32((qp->qp_obj.qp->id << 8) | ds);
	cseg->opcode = rte_cpu_to_be_32((qp->db_pi << 8) |
							MLX5_OPCODE_RDMA_WRITE);
	ds = RTE_ALIGN(ds, 4);
	qp->db_pi += ds >> 2;
	/* Set NOP WQE if needed. */
	if (priv->max_rdmar_ds > ds) {
		cseg += ds;
		ds = priv->max_rdmar_ds - ds;
		cseg->sq_ds = rte_cpu_to_be_32((qp->qp_obj.qp->id << 8) | ds);
		cseg->opcode = rte_cpu_to_be_32((qp->db_pi << 8) |
							       MLX5_OPCODE_NOP);
		qp->db_pi += ds >> 2; /* Here, DS is 4 aligned for sure. */
	}
	qp->wqe = (uint8_t *)cseg;
	return 1;
}

static uint16_t
mlx5_crypto_enqueue_burst(void *queue_pair, struct rte_crypto_op **ops,
			  uint16_t nb_ops)
{
	struct mlx5_crypto_qp *qp = queue_pair;
	struct mlx5_crypto_priv *priv = qp->priv;
	struct mlx5_umr_wqe *umr;
	struct rte_crypto_op *op;
	uint16_t mask = qp->entries_n - 1;
	uint16_t remain = qp->entries_n - (qp->pi - qp->ci);
	uint32_t idx;

	if (remain < nb_ops)
		nb_ops = remain;
	else
		remain = nb_ops;
	if (unlikely(remain == 0))
		return 0;
	do {
		idx = qp->pi & mask;
		op = *ops++;
		umr = RTE_PTR_ADD(qp->qp_obj.umem_buf,
			priv->wqe_set_size * idx);
		if (unlikely(mlx5_crypto_wqe_set(priv, qp, op, umr) == 0)) {
			qp->stats.enqueue_err_count++;
			if (remain != nb_ops) {
				qp->stats.enqueued_count -= remain;
				break;
			}
			return 0;
		}
		qp->ops[idx] = op;
		qp->pi++;
	} while (--remain);
	qp->stats.enqueued_count += nb_ops;
	mlx5_doorbell_ring(&priv->uar.bf_db, *(volatile uint64_t *)qp->wqe,
			   qp->db_pi, &qp->qp_obj.db_rec[MLX5_SND_DBR],
			   !priv->uar.dbnc);
	return nb_ops;
}

static __rte_noinline void
mlx5_crypto_cqe_err_handle(struct mlx5_crypto_qp *qp, struct rte_crypto_op *op)
{
	const uint32_t idx = qp->ci & (qp->entries_n - 1);
	volatile struct mlx5_err_cqe *cqe = (volatile struct mlx5_err_cqe *)
							&qp->cq_obj.cqes[idx];

	op->status = RTE_CRYPTO_OP_STATUS_ERROR;
	qp->stats.dequeue_err_count++;
	DRV_LOG(ERR, "CQE ERR:%x.\n", rte_be_to_cpu_32(cqe->syndrome));
}

static uint16_t
mlx5_crypto_dequeue_burst(void *queue_pair, struct rte_crypto_op **ops,
			  uint16_t nb_ops)
{
	struct mlx5_crypto_qp *qp = queue_pair;
	volatile struct mlx5_cqe *restrict cqe;
	struct rte_crypto_op *restrict op;
	const unsigned int cq_size = qp->entries_n;
	const unsigned int mask = cq_size - 1;
	uint32_t idx;
	uint32_t next_idx = qp->ci & mask;
	const uint16_t max = RTE_MIN((uint16_t)(qp->pi - qp->ci), nb_ops);
	uint16_t i = 0;
	int ret;

	if (unlikely(max == 0))
		return 0;
	do {
		idx = next_idx;
		next_idx = (qp->ci + 1) & mask;
		op = qp->ops[idx];
		cqe = &qp->cq_obj.cqes[idx];
		ret = check_cqe(cqe, cq_size, qp->ci);
		rte_io_rmb();
		if (unlikely(ret != MLX5_CQE_STATUS_SW_OWN)) {
			if (unlikely(ret != MLX5_CQE_STATUS_HW_OWN))
				mlx5_crypto_cqe_err_handle(qp, op);
			break;
		}
		op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
		ops[i++] = op;
		qp->ci++;
	} while (i < max);
	if (likely(i != 0)) {
		rte_io_wmb();
		qp->cq_obj.db_rec[0] = rte_cpu_to_be_32(qp->ci);
		qp->stats.dequeued_count += i;
	}
	return i;
}

static void
mlx5_crypto_qp_init(struct mlx5_crypto_priv *priv, struct mlx5_crypto_qp *qp)
{
	uint32_t i;

	for (i = 0 ; i < qp->entries_n; i++) {
		struct mlx5_wqe_cseg *cseg = RTE_PTR_ADD(qp->qp_obj.umem_buf,
			i * priv->wqe_set_size);
		struct mlx5_wqe_umr_cseg *ucseg = (struct mlx5_wqe_umr_cseg *)
								     (cseg + 1);
		struct mlx5_wqe_umr_bsf_seg *bsf =
			(struct mlx5_wqe_umr_bsf_seg *)(RTE_PTR_ADD(cseg,
						       priv->umr_wqe_size)) - 1;
		struct mlx5_wqe_rseg *rseg;

		/* Init UMR WQE. */
		cseg->sq_ds = rte_cpu_to_be_32((qp->qp_obj.qp->id << 8) |
					 (priv->umr_wqe_size / MLX5_WSEG_SIZE));
		cseg->flags = RTE_BE32(MLX5_COMP_ONLY_FIRST_ERR <<
				       MLX5_COMP_MODE_OFFSET);
		cseg->misc = rte_cpu_to_be_32(qp->mkey[i]->id);
		ucseg->if_cf_toe_cq_res = RTE_BE32(1u << MLX5_UMRC_IF_OFFSET);
		ucseg->mkey_mask = RTE_BE64(1u << 0); /* Mkey length bit. */
		ucseg->ko_to_bs = rte_cpu_to_be_32
			((MLX5_CRYPTO_KLM_SEGS_NUM(priv->umr_wqe_size) <<
			 MLX5_UMRC_KO_OFFSET) | (4 << MLX5_UMRC_TO_BS_OFFSET));
		bsf->keytag = priv->keytag;
		/* Init RDMA WRITE WQE. */
		cseg = RTE_PTR_ADD(cseg, priv->umr_wqe_size);
		cseg->flags = RTE_BE32((MLX5_COMP_ALWAYS <<
				      MLX5_COMP_MODE_OFFSET) |
				      MLX5_WQE_CTRL_INITIATOR_SMALL_FENCE);
		rseg = (struct mlx5_wqe_rseg *)(cseg + 1);
		rseg->rkey = rte_cpu_to_be_32(qp->mkey[i]->id);
	}
}

static int
mlx5_crypto_indirect_mkeys_prepare(struct mlx5_crypto_priv *priv,
				  struct mlx5_crypto_qp *qp)
{
	struct mlx5_umr_wqe *umr;
	uint32_t i;
	struct mlx5_devx_mkey_attr attr = {
		.pd = priv->cdev->pdn,
		.umr_en = 1,
		.crypto_en = 1,
		.set_remote_rw = 1,
		.klm_num = MLX5_CRYPTO_KLM_SEGS_NUM(priv->umr_wqe_size),
	};

	for (umr = (struct mlx5_umr_wqe *)qp->qp_obj.umem_buf, i = 0;
	   i < qp->entries_n; i++, umr = RTE_PTR_ADD(umr, priv->wqe_set_size)) {
		attr.klm_array = (struct mlx5_klm *)&umr->kseg[0];
		qp->mkey[i] = mlx5_devx_cmd_mkey_create(priv->cdev->ctx, &attr);
		if (!qp->mkey[i])
			goto error;
	}
	return 0;
error:
	DRV_LOG(ERR, "Failed to allocate indirect mkey.");
	mlx5_crypto_indirect_mkeys_release(qp, i);
	return -1;
}

static int
mlx5_crypto_queue_pair_setup(struct rte_cryptodev *dev, uint16_t qp_id,
			     const struct rte_cryptodev_qp_conf *qp_conf,
			     int socket_id)
{
	struct mlx5_crypto_priv *priv = dev->data->dev_private;
	struct mlx5_devx_qp_attr attr = {0};
	struct mlx5_crypto_qp *qp;
	uint16_t log_nb_desc = rte_log2_u32(qp_conf->nb_descriptors);
	uint32_t ret;
	uint32_t alloc_size = sizeof(*qp);
	uint32_t log_wqbb_n;
	struct mlx5_devx_cq_attr cq_attr = {
		.uar_page_id = mlx5_os_get_devx_uar_page_id(priv->uar.obj),
	};

	if (dev->data->queue_pairs[qp_id] != NULL)
		mlx5_crypto_queue_pair_release(dev, qp_id);
	alloc_size = RTE_ALIGN(alloc_size, RTE_CACHE_LINE_SIZE);
	alloc_size += (sizeof(struct rte_crypto_op *) +
		       sizeof(struct mlx5_devx_obj *)) *
		       RTE_BIT32(log_nb_desc);
	qp = rte_zmalloc_socket(__func__, alloc_size, RTE_CACHE_LINE_SIZE,
				socket_id);
	if (qp == NULL) {
		DRV_LOG(ERR, "Failed to allocate QP memory.");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	if (mlx5_devx_cq_create(priv->cdev->ctx, &qp->cq_obj, log_nb_desc,
				&cq_attr, socket_id) != 0) {
		DRV_LOG(ERR, "Failed to create CQ.");
		goto error;
	}
	log_wqbb_n = rte_log2_u32(RTE_BIT32(log_nb_desc) *
				(priv->wqe_set_size / MLX5_SEND_WQE_BB));
	attr.pd = priv->cdev->pdn;
	attr.uar_index = mlx5_os_get_devx_uar_page_id(priv->uar.obj);
	attr.cqn = qp->cq_obj.cq->id;
	attr.num_of_receive_wqes = 0;
	attr.num_of_send_wqbbs = RTE_BIT32(log_wqbb_n);
	attr.ts_format =
		mlx5_ts_format_conv(priv->cdev->config.hca_attr.qp_ts_format);
	ret = mlx5_devx_qp_create(priv->cdev->ctx, &qp->qp_obj,
					attr.num_of_send_wqbbs * MLX5_WQE_SIZE,
					&attr, socket_id);
	if (ret) {
		DRV_LOG(ERR, "Failed to create QP.");
		goto error;
	}
	if (mlx5_mr_ctrl_init(&qp->mr_ctrl, &priv->cdev->mr_scache.dev_gen,
			      priv->dev_config.socket_id) != 0) {
		DRV_LOG(ERR, "Cannot allocate MR Btree for qp %u.",
			(uint32_t)qp_id);
		rte_errno = ENOMEM;
		goto error;
	}
	/*
	 * In Order to configure self loopback, when calling devx qp2rts the
	 * remote QP id that is used is the id of the same QP.
	 */
	if (mlx5_devx_qp2rts(&qp->qp_obj, qp->qp_obj.qp->id))
		goto error;
	qp->mkey = (struct mlx5_devx_obj **)RTE_ALIGN((uintptr_t)(qp + 1),
							   RTE_CACHE_LINE_SIZE);
	qp->ops = (struct rte_crypto_op **)(qp->mkey + RTE_BIT32(log_nb_desc));
	qp->entries_n = 1 << log_nb_desc;
	if (mlx5_crypto_indirect_mkeys_prepare(priv, qp)) {
		DRV_LOG(ERR, "Cannot allocate indirect memory regions.");
		rte_errno = ENOMEM;
		goto error;
	}
	mlx5_crypto_qp_init(priv, qp);
	qp->priv = priv;
	dev->data->queue_pairs[qp_id] = qp;
	return 0;
error:
	mlx5_crypto_qp_release(qp);
	return -1;
}

static void
mlx5_crypto_stats_get(struct rte_cryptodev *dev,
		      struct rte_cryptodev_stats *stats)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct mlx5_crypto_qp *qp = dev->data->queue_pairs[qp_id];

		stats->enqueued_count += qp->stats.enqueued_count;
		stats->dequeued_count += qp->stats.dequeued_count;
		stats->enqueue_err_count += qp->stats.enqueue_err_count;
		stats->dequeue_err_count += qp->stats.dequeue_err_count;
	}
}

static void
mlx5_crypto_stats_reset(struct rte_cryptodev *dev)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct mlx5_crypto_qp *qp = dev->data->queue_pairs[qp_id];

		memset(&qp->stats, 0, sizeof(qp->stats));
	}
}

static struct rte_cryptodev_ops mlx5_crypto_ops = {
	.dev_configure			= mlx5_crypto_dev_configure,
	.dev_start			= mlx5_crypto_dev_start,
	.dev_stop			= mlx5_crypto_dev_stop,
	.dev_close			= mlx5_crypto_dev_close,
	.dev_infos_get			= mlx5_crypto_dev_infos_get,
	.stats_get			= mlx5_crypto_stats_get,
	.stats_reset			= mlx5_crypto_stats_reset,
	.queue_pair_setup		= mlx5_crypto_queue_pair_setup,
	.queue_pair_release		= mlx5_crypto_queue_pair_release,
	.sym_session_get_size		= mlx5_crypto_sym_session_get_size,
	.sym_session_configure		= mlx5_crypto_sym_session_configure,
	.sym_session_clear		= mlx5_crypto_sym_session_clear,
	.sym_get_raw_dp_ctx_size	= NULL,
	.sym_configure_raw_dp_ctx	= NULL,
};

static int
mlx5_crypto_args_check_handler(const char *key, const char *val, void *opaque)
{
	struct mlx5_crypto_devarg_params *devarg_prms = opaque;
	struct mlx5_devx_crypto_login_attr *attr = &devarg_prms->login_attr;
	unsigned long tmp;
	FILE *file;
	int ret;
	int i;

	if (strcmp(key, "class") == 0)
		return 0;
	if (strcmp(key, "wcs_file") == 0) {
		file = fopen(val, "rb");
		if (file == NULL) {
			rte_errno = ENOTSUP;
			return -rte_errno;
		}
		for (i = 0 ; i < MLX5_CRYPTO_CREDENTIAL_SIZE ; i++) {
			ret = fscanf(file, "%02hhX", &attr->credential[i]);
			if (ret <= 0) {
				fclose(file);
				DRV_LOG(ERR,
					"Failed to read credential from file.");
				rte_errno = EINVAL;
				return -rte_errno;
			}
		}
		fclose(file);
		devarg_prms->login_devarg = true;
		return 0;
	}
	errno = 0;
	tmp = strtoul(val, NULL, 0);
	if (errno) {
		DRV_LOG(WARNING, "%s: \"%s\" is an invalid integer.", key, val);
		return -errno;
	}
	if (strcmp(key, "max_segs_num") == 0) {
		if (!tmp) {
			DRV_LOG(ERR, "max_segs_num must be greater than 0.");
			rte_errno = EINVAL;
			return -rte_errno;
		}
		devarg_prms->max_segs_num = (uint32_t)tmp;
	} else if (strcmp(key, "import_kek_id") == 0) {
		attr->session_import_kek_ptr = (uint32_t)tmp;
	} else if (strcmp(key, "credential_id") == 0) {
		attr->credential_pointer = (uint32_t)tmp;
	} else if (strcmp(key, "keytag") == 0) {
		devarg_prms->keytag = tmp;
	} else {
		DRV_LOG(WARNING, "Invalid key %s.", key);
	}
	return 0;
}

static int
mlx5_crypto_parse_devargs(struct rte_devargs *devargs,
			  struct mlx5_crypto_devarg_params *devarg_prms)
{
	struct mlx5_devx_crypto_login_attr *attr = &devarg_prms->login_attr;
	struct rte_kvargs *kvlist;

	/* Default values. */
	attr->credential_pointer = 0;
	attr->session_import_kek_ptr = 0;
	devarg_prms->keytag = 0;
	devarg_prms->max_segs_num = 8;
	if (devargs == NULL) {
		DRV_LOG(ERR,
	"No login devargs in order to enable crypto operations in the device.");
		rte_errno = EINVAL;
		return -1;
	}
	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL) {
		DRV_LOG(ERR, "Failed to parse devargs.");
		rte_errno = EINVAL;
		return -1;
	}
	if (rte_kvargs_process(kvlist, NULL, mlx5_crypto_args_check_handler,
			   devarg_prms) != 0) {
		DRV_LOG(ERR, "Devargs handler function Failed.");
		rte_kvargs_free(kvlist);
		rte_errno = EINVAL;
		return -1;
	}
	rte_kvargs_free(kvlist);
	if (devarg_prms->login_devarg == false) {
		DRV_LOG(ERR,
	"No login credential devarg in order to enable crypto operations "
	"in the device.");
		rte_errno = EINVAL;
		return -1;
	}
	return 0;
}

/*
 * Calculate UMR WQE size and RDMA Write WQE size with the
 * following limitations:
 *	- Each WQE size is multiple of 64.
 *	- The summarize of both UMR WQE and RDMA_W WQE is a power of 2.
 *	- The number of entries in the UMR WQE's KLM list is multiple of 4.
 */
static void
mlx5_crypto_get_wqe_sizes(uint32_t segs_num, uint32_t *umr_size,
			uint32_t *rdmaw_size)
{
	uint32_t diff, wqe_set_size;

	*umr_size = MLX5_CRYPTO_UMR_WQE_STATIC_SIZE +
			RTE_ALIGN(segs_num, 4) *
			sizeof(struct mlx5_wqe_dseg);
	/* Make sure UMR WQE size is multiple of WQBB. */
	*umr_size = RTE_ALIGN(*umr_size, MLX5_SEND_WQE_BB);
	*rdmaw_size = sizeof(struct mlx5_rdma_write_wqe) +
			sizeof(struct mlx5_wqe_dseg) *
			(segs_num <= 2 ? 2 : 2 +
			RTE_ALIGN(segs_num - 2, 4));
	/* Make sure RDMA_WRITE WQE size is multiple of WQBB. */
	*rdmaw_size = RTE_ALIGN(*rdmaw_size, MLX5_SEND_WQE_BB);
	wqe_set_size = *rdmaw_size + *umr_size;
	diff = rte_align32pow2(wqe_set_size) - wqe_set_size;
	/* Make sure wqe_set size is power of 2. */
	if (diff)
		*umr_size += diff;
}

static uint8_t
mlx5_crypto_max_segs_num(uint16_t max_wqe_size)
{
	int klms_sizes = max_wqe_size - MLX5_CRYPTO_UMR_WQE_STATIC_SIZE;
	uint32_t max_segs_cap = RTE_ALIGN_FLOOR(klms_sizes, MLX5_SEND_WQE_BB) /
			sizeof(struct mlx5_wqe_dseg);

	MLX5_ASSERT(klms_sizes >= MLX5_SEND_WQE_BB);
	while (max_segs_cap) {
		uint32_t umr_wqe_size, rdmw_wqe_size;

		mlx5_crypto_get_wqe_sizes(max_segs_cap, &umr_wqe_size,
						&rdmw_wqe_size);
		if (umr_wqe_size <= max_wqe_size &&
				rdmw_wqe_size <= max_wqe_size)
			break;
		max_segs_cap -= 4;
	}
	return max_segs_cap;
}

static int
mlx5_crypto_configure_wqe_size(struct mlx5_crypto_priv *priv,
				uint16_t max_wqe_size, uint32_t max_segs_num)
{
	uint32_t rdmw_wqe_size, umr_wqe_size;

	mlx5_crypto_get_wqe_sizes(max_segs_num, &umr_wqe_size,
					&rdmw_wqe_size);
	priv->wqe_set_size = rdmw_wqe_size + umr_wqe_size;
	if (umr_wqe_size > max_wqe_size ||
				rdmw_wqe_size > max_wqe_size) {
		DRV_LOG(ERR, "Invalid max_segs_num: %u. should be %u or lower.",
			max_segs_num,
			mlx5_crypto_max_segs_num(max_wqe_size));
		rte_errno = EINVAL;
		return -EINVAL;
	}
	priv->umr_wqe_size = (uint16_t)umr_wqe_size;
	priv->umr_wqe_stride = priv->umr_wqe_size / MLX5_SEND_WQE_BB;
	priv->max_rdmar_ds = rdmw_wqe_size / sizeof(struct mlx5_wqe_dseg);
	return 0;
}

static int
mlx5_crypto_dev_probe(struct mlx5_common_device *cdev)
{
	struct rte_cryptodev *crypto_dev;
	struct mlx5_devx_obj *login;
	struct mlx5_crypto_priv *priv;
	struct mlx5_crypto_devarg_params devarg_prms = { 0 };
	struct rte_cryptodev_pmd_init_params init_params = {
		.name = "",
		.private_data_size = sizeof(struct mlx5_crypto_priv),
		.socket_id = cdev->dev->numa_node,
		.max_nb_queue_pairs =
				RTE_CRYPTODEV_PMD_DEFAULT_MAX_NB_QUEUE_PAIRS,
	};
	const char *ibdev_name = mlx5_os_get_ctx_device_name(cdev->ctx);
	int ret;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		DRV_LOG(ERR, "Non-primary process type is not supported.");
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	if (!cdev->config.hca_attr.crypto || !cdev->config.hca_attr.aes_xts) {
		DRV_LOG(ERR, "Not enough capabilities to support crypto "
			"operations, maybe old FW/OFED version?");
		rte_errno = ENOTSUP;
		return -ENOTSUP;
	}
	ret = mlx5_crypto_parse_devargs(cdev->dev->devargs, &devarg_prms);
	if (ret) {
		DRV_LOG(ERR, "Failed to parse devargs.");
		return -rte_errno;
	}
	crypto_dev = rte_cryptodev_pmd_create(ibdev_name, cdev->dev,
					      &init_params);
	if (crypto_dev == NULL) {
		DRV_LOG(ERR, "Failed to create device \"%s\".", ibdev_name);
		return -ENODEV;
	}
	DRV_LOG(INFO,
		"Crypto device %s was created successfully.", ibdev_name);
	crypto_dev->dev_ops = &mlx5_crypto_ops;
	crypto_dev->dequeue_burst = mlx5_crypto_dequeue_burst;
	crypto_dev->enqueue_burst = mlx5_crypto_enqueue_burst;
	crypto_dev->feature_flags = MLX5_CRYPTO_FEATURE_FLAGS;
	crypto_dev->driver_id = mlx5_crypto_driver_id;
	priv = crypto_dev->data->dev_private;
	priv->cdev = cdev;
	priv->crypto_dev = crypto_dev;
	if (mlx5_devx_uar_prepare(cdev, &priv->uar) != 0) {
		rte_cryptodev_pmd_destroy(priv->crypto_dev);
		return -1;
	}
	login = mlx5_devx_cmd_create_crypto_login_obj(cdev->ctx,
						      &devarg_prms.login_attr);
	if (login == NULL) {
		DRV_LOG(ERR, "Failed to configure login.");
		mlx5_devx_uar_release(&priv->uar);
		rte_cryptodev_pmd_destroy(priv->crypto_dev);
		return -rte_errno;
	}
	priv->login_obj = login;
	priv->keytag = rte_cpu_to_be_64(devarg_prms.keytag);
	ret = mlx5_crypto_configure_wqe_size(priv,
		cdev->config.hca_attr.max_wqe_sz_sq, devarg_prms.max_segs_num);
	if (ret) {
		claim_zero(mlx5_devx_cmd_destroy(priv->login_obj));
		mlx5_devx_uar_release(&priv->uar);
		rte_cryptodev_pmd_destroy(priv->crypto_dev);
		return -1;
	}
	DRV_LOG(INFO, "Max number of segments: %u.",
		(unsigned int)RTE_MIN(
			MLX5_CRYPTO_KLM_SEGS_NUM(priv->umr_wqe_size),
			(uint16_t)(priv->max_rdmar_ds - 2)));
	pthread_mutex_lock(&priv_list_lock);
	TAILQ_INSERT_TAIL(&mlx5_crypto_priv_list, priv, next);
	pthread_mutex_unlock(&priv_list_lock);

	rte_cryptodev_pmd_probing_finish(crypto_dev);

	return 0;
}

static int
mlx5_crypto_dev_remove(struct mlx5_common_device *cdev)
{
	struct mlx5_crypto_priv *priv = NULL;

	pthread_mutex_lock(&priv_list_lock);
	TAILQ_FOREACH(priv, &mlx5_crypto_priv_list, next)
		if (priv->crypto_dev->device == cdev->dev)
			break;
	if (priv)
		TAILQ_REMOVE(&mlx5_crypto_priv_list, priv, next);
	pthread_mutex_unlock(&priv_list_lock);
	if (priv) {
		claim_zero(mlx5_devx_cmd_destroy(priv->login_obj));
		mlx5_devx_uar_release(&priv->uar);
		rte_cryptodev_pmd_destroy(priv->crypto_dev);
	}
	return 0;
}

static const struct rte_pci_id mlx5_crypto_pci_id_map[] = {
		{
			RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
					PCI_DEVICE_ID_MELLANOX_CONNECTX6)
		},
		{
			RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
					PCI_DEVICE_ID_MELLANOX_CONNECTX6DX)
		},
		{
			RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
					PCI_DEVICE_ID_MELLANOX_CONNECTX6DXBF)
		},
		{
			.vendor_id = 0
		}
};

static struct mlx5_class_driver mlx5_crypto_driver = {
	.drv_class = MLX5_CLASS_CRYPTO,
	.name = RTE_STR(MLX5_CRYPTO_DRIVER_NAME),
	.id_table = mlx5_crypto_pci_id_map,
	.probe = mlx5_crypto_dev_probe,
	.remove = mlx5_crypto_dev_remove,
};

RTE_INIT(rte_mlx5_crypto_init)
{
	pthread_mutex_init(&priv_list_lock, NULL);
	mlx5_common_init();
	if (mlx5_glue != NULL)
		mlx5_class_driver_register(&mlx5_crypto_driver);
}

RTE_PMD_REGISTER_CRYPTO_DRIVER(mlx5_cryptodev_driver, mlx5_drv,
			       mlx5_crypto_driver_id);

RTE_LOG_REGISTER_DEFAULT(mlx5_crypto_logtype, NOTICE)
RTE_PMD_EXPORT_NAME(MLX5_CRYPTO_DRIVER_NAME, __COUNTER__);
RTE_PMD_REGISTER_PCI_TABLE(MLX5_CRYPTO_DRIVER_NAME, mlx5_crypto_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(MLX5_CRYPTO_DRIVER_NAME, "* ib_uverbs & mlx5_core & mlx5_ib");
