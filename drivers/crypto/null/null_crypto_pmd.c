/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <rte_common.h>
#include <rte_cryptodev_pmd.h>
#include <rte_bus_vdev.h>
#include <rte_malloc.h>

#include "null_crypto_pmd_private.h"

static uint8_t cryptodev_driver_id;

/** verify and set session parameters */
int
null_crypto_set_session_parameters(
		struct null_crypto_session *sess __rte_unused,
		const struct rte_crypto_sym_xform *xform)
{
	if (xform == NULL) {
		return -EINVAL;
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
			xform->next == NULL) {
		/* Authentication Only */
		if (xform->auth.algo == RTE_CRYPTO_AUTH_NULL)
			return 0;
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
			xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		/* Authentication then Cipher */
		if (xform->auth.algo == RTE_CRYPTO_AUTH_NULL &&
			xform->next->cipher.algo == RTE_CRYPTO_CIPHER_NULL)
			return 0;
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
			xform->next == NULL) {
		/* Cipher Only */
		if (xform->cipher.algo == RTE_CRYPTO_CIPHER_NULL)
			return 0;
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
			xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		/* Cipher then Authentication */
		if (xform->cipher.algo == RTE_CRYPTO_CIPHER_NULL &&
			xform->next->auth.algo == RTE_CRYPTO_AUTH_NULL)
			return 0;
	}

	return -ENOTSUP;
}

/** Process crypto operation for mbuf */
static int
process_op(const struct null_crypto_qp *qp, struct rte_crypto_op *op,
		struct null_crypto_session *sess __rte_unused)
{
	/* set status as successful by default */
	op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

	/* Free session if a session-less crypto op. */
	if (op->sess_type == RTE_CRYPTO_OP_SESSIONLESS) {
		memset(op->sym->session, 0,
				sizeof(struct null_crypto_session));
		rte_cryptodev_sym_session_free(op->sym->session);
		op->sym->session = NULL;
	}

	/*
	 * if crypto session and operation are valid just enqueue the packet
	 * in the processed ring
	 */
	return rte_ring_enqueue(qp->processed_pkts, (void *)op);
}

static struct null_crypto_session *
get_session(struct null_crypto_qp *qp, struct rte_crypto_op *op)
{
	struct null_crypto_session *sess = NULL;
	struct rte_crypto_sym_op *sym_op = op->sym;

	if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
		if (likely(sym_op->session != NULL))
			sess = (struct null_crypto_session *)
					get_sym_session_private_data(
					sym_op->session, cryptodev_driver_id);
	} else {
		void *_sess = NULL;
		void *_sess_private_data = NULL;

		if (rte_mempool_get(qp->sess_mp, (void **)&_sess))
			return NULL;

		if (rte_mempool_get(qp->sess_mp_priv,
				(void **)&_sess_private_data))
			return NULL;

		sess = (struct null_crypto_session *)_sess_private_data;

		if (unlikely(null_crypto_set_session_parameters(sess,
				sym_op->xform) != 0)) {
			rte_mempool_put(qp->sess_mp, _sess);
			rte_mempool_put(qp->sess_mp_priv, _sess_private_data);
			sess = NULL;
		}
		sym_op->session = (struct rte_cryptodev_sym_session *)_sess;
		set_sym_session_private_data(op->sym->session,
				cryptodev_driver_id, _sess_private_data);
	}

	return sess;
}

/** Enqueue burst */
static uint16_t
null_crypto_pmd_enqueue_burst(void *queue_pair, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	struct null_crypto_session *sess;
	struct null_crypto_qp *qp = queue_pair;

	int i, retval;

	for (i = 0; i < nb_ops; i++) {
		sess = get_session(qp, ops[i]);
		if (unlikely(sess == NULL))
			goto enqueue_err;

		retval = process_op(qp, ops[i], sess);
		if (unlikely(retval < 0))
			goto enqueue_err;
	}

	qp->qp_stats.enqueued_count += i;
	return i;

enqueue_err:
	if (ops[i])
		ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;

	qp->qp_stats.enqueue_err_count++;
	return i;
}

/** Dequeue burst */
static uint16_t
null_crypto_pmd_dequeue_burst(void *queue_pair, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	struct null_crypto_qp *qp = queue_pair;

	unsigned nb_dequeued;

	nb_dequeued = rte_ring_dequeue_burst(qp->processed_pkts,
			(void **)ops, nb_ops, NULL);
	qp->qp_stats.dequeued_count += nb_dequeued;

	return nb_dequeued;
}

/** Create crypto device */
static int
cryptodev_null_create(const char *name,
		struct rte_vdev_device *vdev,
		struct rte_cryptodev_pmd_init_params *init_params)
{
	struct rte_cryptodev *dev;
	struct null_crypto_private *internals;
	dev = rte_cryptodev_pmd_create(name, &vdev->device, init_params);
	if (dev == NULL) {
		NULL_LOG(ERR, "failed to create cryptodev vdev");
		return -EFAULT;
	}

	dev->driver_id = cryptodev_driver_id;
	dev->dev_ops = null_crypto_pmd_ops;

	/* register rx/tx burst functions for data path */
	dev->dequeue_burst = null_crypto_pmd_dequeue_burst;
	dev->enqueue_burst = null_crypto_pmd_enqueue_burst;

	dev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			RTE_CRYPTODEV_FF_IN_PLACE_SGL;

	internals = dev->data->dev_private;

	internals->max_nb_qpairs = init_params->max_nb_queue_pairs;

	return 0;
}

/** Initialise null crypto device */
static int
cryptodev_null_probe(struct rte_vdev_device *dev)
{
	struct rte_cryptodev_pmd_init_params init_params = {
		"",
		sizeof(struct null_crypto_private),
		rte_socket_id(),
		RTE_CRYPTODEV_PMD_DEFAULT_MAX_NB_QUEUE_PAIRS
	};
	const char *name, *args;
	int retval;

	name = rte_vdev_device_name(dev);
	if (name == NULL)
		return -EINVAL;

	args = rte_vdev_device_args(dev);

	retval = rte_cryptodev_pmd_parse_input_args(&init_params, args);
	if (retval) {
		NULL_LOG(ERR,
				"Failed to parse initialisation arguments[%s]",
				args);
		return -EINVAL;
	}

	return cryptodev_null_create(name, dev, &init_params);
}

static int
cryptodev_null_remove_dev(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev *cryptodev;
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	cryptodev = rte_cryptodev_pmd_get_named_dev(name);
	if (cryptodev == NULL)
		return -ENODEV;

	return rte_cryptodev_pmd_destroy(cryptodev);
}

static struct rte_vdev_driver cryptodev_null_pmd_drv = {
	.probe = cryptodev_null_probe,
	.remove = cryptodev_null_remove_dev,
};

static struct cryptodev_driver null_crypto_drv;

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_NULL_PMD, cryptodev_null_pmd_drv);
RTE_PMD_REGISTER_ALIAS(CRYPTODEV_NAME_NULL_PMD, cryptodev_null_pmd);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_NULL_PMD,
	"max_nb_queue_pairs=<int> "
	"socket_id=<int>");
RTE_PMD_REGISTER_CRYPTO_DRIVER(null_crypto_drv, cryptodev_null_pmd_drv.driver,
		cryptodev_driver_id);

RTE_INIT(null_init_log)
{
	null_logtype_driver = rte_log_register("pmd.crypto.null");
}
