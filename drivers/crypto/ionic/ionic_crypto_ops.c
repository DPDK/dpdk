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

static struct rte_cryptodev_ops iocpt_ops = {
	.dev_configure = iocpt_op_config,
	.dev_close = iocpt_op_close,
	.dev_infos_get = iocpt_op_info_get,

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

	return 0;
}
