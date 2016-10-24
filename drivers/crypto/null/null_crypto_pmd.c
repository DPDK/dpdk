/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_common.h>
#include <rte_config.h>
#include <rte_cryptodev_pmd.h>
#include <rte_vdev.h>
#include <rte_malloc.h>

#include "null_crypto_pmd_private.h"

/**
 * Global static parameter used to create a unique name for each crypto device.
 */
static unsigned unique_name_id;

static inline int
create_unique_device_name(char *name, size_t size)
{
	int ret;

	if (name == NULL)
		return -EINVAL;

	ret = snprintf(name, size, "%s_%u", RTE_STR(CRYPTODEV_NAME_NULL_PMD),
			unique_name_id++);
	if (ret < 0)
		return ret;
	return 0;
}


/** verify and set session parameters */
int
null_crypto_set_session_parameters(
		struct null_crypto_session *sess __rte_unused,
		const struct rte_crypto_sym_xform *xform)
{
	if (xform == NULL) {
		return -1;
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

	return -1;
}

/** Process crypto operation for mbuf */
static int
process_op(const struct null_crypto_qp *qp, struct rte_crypto_op *op,
		struct null_crypto_session *sess __rte_unused)
{
	/* set status as successful by default */
	op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

	/*
	 * if crypto session and operation are valid just enqueue the packet
	 * in the processed ring
	 */
	return rte_ring_enqueue(qp->processed_pkts, (void *)op);
}

static struct null_crypto_session *
get_session(struct null_crypto_qp *qp, struct rte_crypto_sym_op *op)
{
	struct null_crypto_session *sess;

	if (op->sess_type == RTE_CRYPTO_SYM_OP_WITH_SESSION) {
		if (unlikely(op->session == NULL ||
			     op->session->dev_type != RTE_CRYPTODEV_NULL_PMD))
			return NULL;

		sess = (struct null_crypto_session *)op->session->_private;
	} else  {
		struct rte_cryptodev_session *c_sess = NULL;

		if (rte_mempool_get(qp->sess_mp, (void **)&c_sess))
			return NULL;

		sess = (struct null_crypto_session *)c_sess->_private;

		if (null_crypto_set_session_parameters(sess, op->xform)	!= 0)
			return NULL;
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
		sess = get_session(qp, ops[i]->sym);
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
			(void **)ops, nb_ops);
	qp->qp_stats.dequeued_count += nb_dequeued;

	return nb_dequeued;
}

static int cryptodev_null_remove(const char *name);

/** Create crypto device */
static int
cryptodev_null_create(const char *name,
		struct rte_crypto_vdev_init_params *init_params)
{
	struct rte_cryptodev *dev;
	char crypto_dev_name[RTE_CRYPTODEV_NAME_MAX_LEN];
	struct null_crypto_private *internals;

	/* create a unique device name */
	if (create_unique_device_name(crypto_dev_name,
			RTE_CRYPTODEV_NAME_MAX_LEN) != 0) {
		NULL_CRYPTO_LOG_ERR("failed to create unique cryptodev name");
		return -EINVAL;
	}

	dev = rte_cryptodev_pmd_virtual_dev_init(crypto_dev_name,
			sizeof(struct null_crypto_private),
			init_params->socket_id);
	if (dev == NULL) {
		NULL_CRYPTO_LOG_ERR("failed to create cryptodev vdev");
		goto init_error;
	}

	dev->dev_type = RTE_CRYPTODEV_NULL_PMD;
	dev->dev_ops = null_crypto_pmd_ops;

	/* register rx/tx burst functions for data path */
	dev->dequeue_burst = null_crypto_pmd_dequeue_burst;
	dev->enqueue_burst = null_crypto_pmd_enqueue_burst;

	dev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING;

	internals = dev->data->dev_private;

	internals->max_nb_qpairs = init_params->max_nb_queue_pairs;
	internals->max_nb_sessions = init_params->max_nb_sessions;

	return 0;

init_error:
	NULL_CRYPTO_LOG_ERR("driver %s: cryptodev_null_create failed", name);
	cryptodev_null_remove(crypto_dev_name);

	return -EFAULT;
}

/** Initialise null crypto device */
static int
cryptodev_null_probe(const char *name,
		const char *input_args)
{
	struct rte_crypto_vdev_init_params init_params = {
		RTE_CRYPTODEV_VDEV_DEFAULT_MAX_NB_QUEUE_PAIRS,
		RTE_CRYPTODEV_VDEV_DEFAULT_MAX_NB_SESSIONS,
		rte_socket_id()
	};

	rte_cryptodev_parse_vdev_init_params(&init_params, input_args);

	RTE_LOG(INFO, PMD, "Initialising %s on NUMA node %d\n", name,
			init_params.socket_id);
	RTE_LOG(INFO, PMD, "  Max number of queue pairs = %d\n",
			init_params.max_nb_queue_pairs);
	RTE_LOG(INFO, PMD, "  Max number of sessions = %d\n",
			init_params.max_nb_sessions);

	return cryptodev_null_create(name, &init_params);
}

/** Uninitialise null crypto device */
static int
cryptodev_null_remove(const char *name)
{
	if (name == NULL)
		return -EINVAL;

	RTE_LOG(INFO, PMD, "Closing null crypto device %s on numa socket %u\n",
			name, rte_socket_id());

	return 0;
}

static struct rte_vdev_driver cryptodev_null_pmd_drv = {
	.probe = cryptodev_null_probe,
	.remove = cryptodev_null_remove
};

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_NULL_PMD, cryptodev_null_pmd_drv);
RTE_PMD_REGISTER_ALIAS(CRYPTODEV_NAME_NULL_PMD, cryptodev_null_pmd);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_NULL_PMD,
	"max_nb_queue_pairs=<int> "
	"max_nb_sessions=<int> "
	"socket_id=<int>");
