/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <stdlib.h>
#include <sys/queue.h>

#include <dev_driver.h>
#include <rte_errno.h>
#include <rte_string_fns.h>
#include <rte_malloc.h>

#include "cryptodev_pmd.h"

/**
 * Parse name from argument
 */
static int
rte_cryptodev_pmd_parse_name_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	struct rte_cryptodev_pmd_init_params *params = extra_args;
	int n;

	if (value == NULL || extra_args == NULL)
		return -EINVAL;

	n = strlcpy(params->name, value, RTE_CRYPTODEV_NAME_MAX_LEN);
	if (n >= RTE_CRYPTODEV_NAME_MAX_LEN)
		return -EINVAL;

	return 0;
}

/**
 * Parse unsigned integer from argument
 */
static int
rte_cryptodev_pmd_parse_uint_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	int i;
	char *end;

	if (value == NULL || extra_args == NULL)
		return -EINVAL;

	errno = 0;

	i = strtol(value, &end, 10);
	if (*end != 0 || errno != 0 || i < 0)
		return -EINVAL;

	*((uint32_t *)extra_args) = i;
	return 0;
}

int
rte_cryptodev_pmd_parse_input_args(
		struct rte_cryptodev_pmd_init_params *params,
		const char *args)
{
	struct rte_kvargs *kvlist = NULL;
	int ret = 0;

	if (params == NULL)
		return -EINVAL;

	if (args) {
		kvlist = rte_kvargs_parse(args,	cryptodev_pmd_valid_params);
		if (kvlist == NULL)
			return -EINVAL;

		ret = rte_kvargs_process(kvlist,
				RTE_CRYPTODEV_PMD_MAX_NB_QP_ARG,
				&rte_cryptodev_pmd_parse_uint_arg,
				&params->max_nb_queue_pairs);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist,
				RTE_CRYPTODEV_PMD_SOCKET_ID_ARG,
				&rte_cryptodev_pmd_parse_uint_arg,
				&params->socket_id);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist,
				RTE_CRYPTODEV_PMD_NAME_ARG,
				&rte_cryptodev_pmd_parse_name_arg,
				params);
		if (ret < 0)
			goto free_kvlist;
	}

free_kvlist:
	rte_kvargs_free(kvlist);
	return ret;
}

struct rte_cryptodev *
rte_cryptodev_pmd_create(const char *name,
		struct rte_device *device,
		struct rte_cryptodev_pmd_init_params *params)
{
	struct rte_cryptodev *cryptodev;

	if (params->name[0] != '\0') {
		CDEV_LOG_INFO("User specified device name = %s", params->name);
		name = params->name;
	}

	CDEV_LOG_INFO("Creating cryptodev %s", name);

	CDEV_LOG_INFO("Initialisation parameters - name: %s,"
			"socket id: %d, max queue pairs: %u",
			name, params->socket_id, params->max_nb_queue_pairs);

	/* allocate device structure */
	cryptodev = rte_cryptodev_pmd_allocate(name, params->socket_id);
	if (cryptodev == NULL) {
		CDEV_LOG_ERR("Failed to allocate crypto device for %s", name);
		return NULL;
	}

	/* allocate private device structure */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		cryptodev->data->dev_private =
				rte_zmalloc_socket("cryptodev device private",
						params->private_data_size,
						RTE_CACHE_LINE_SIZE,
						params->socket_id);

		if (cryptodev->data->dev_private == NULL) {
			CDEV_LOG_ERR("Cannot allocate memory for cryptodev %s"
					" private data", name);

			rte_cryptodev_pmd_release_device(cryptodev);
			return NULL;
		}
	}

	cryptodev->device = device;

	/* initialise user call-back tail queue */
	TAILQ_INIT(&(cryptodev->link_intr_cbs));

	return cryptodev;
}

int
rte_cryptodev_pmd_destroy(struct rte_cryptodev *cryptodev)
{
	int retval;
	void *dev_priv = cryptodev->data->dev_private;

	CDEV_LOG_INFO("Closing crypto device %s", cryptodev->device->name);

	/* free crypto device */
	retval = rte_cryptodev_pmd_release_device(cryptodev);
	if (retval)
		return retval;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(dev_priv);


	cryptodev->device = NULL;
	cryptodev->data = NULL;

	return 0;
}

void
rte_cryptodev_pmd_probing_finish(struct rte_cryptodev *cryptodev)
{
	if (cryptodev == NULL)
		return;
	/*
	 * for secondary process, at that point we expect device
	 * to be already 'usable', so shared data and all function
	 * pointers for fast-path devops have to be setup properly
	 * inside rte_cryptodev.
	 */
	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		cryptodev_fp_ops_set(rte_crypto_fp_ops +
				cryptodev->data->dev_id, cryptodev);
}

static uint16_t
dummy_crypto_enqueue_burst(__rte_unused void *qp,
			   __rte_unused struct rte_crypto_op **ops,
			   __rte_unused uint16_t nb_ops)
{
	CDEV_LOG_ERR(
		"crypto enqueue burst requested for unconfigured device");
	rte_errno = ENOTSUP;
	return 0;
}

static uint16_t
dummy_crypto_dequeue_burst(__rte_unused void *qp,
			   __rte_unused struct rte_crypto_op **ops,
			   __rte_unused uint16_t nb_ops)
{
	CDEV_LOG_ERR(
		"crypto dequeue burst requested for unconfigured device");
	rte_errno = ENOTSUP;
	return 0;
}

void
cryptodev_fp_ops_reset(struct rte_crypto_fp_ops *fp_ops)
{
	static struct rte_cryptodev_cb_rcu dummy_cb[RTE_MAX_QUEUES_PER_PORT];
	static void *dummy_data[RTE_MAX_QUEUES_PER_PORT];
	static const struct rte_crypto_fp_ops dummy = {
		.enqueue_burst = dummy_crypto_enqueue_burst,
		.dequeue_burst = dummy_crypto_dequeue_burst,
		.qp = {
			.data = dummy_data,
			.enq_cb = dummy_cb,
			.deq_cb = dummy_cb,
		},
	};

	*fp_ops = dummy;
}

void
cryptodev_fp_ops_set(struct rte_crypto_fp_ops *fp_ops,
		     const struct rte_cryptodev *dev)
{
	fp_ops->enqueue_burst = dev->enqueue_burst;
	fp_ops->dequeue_burst = dev->dequeue_burst;
	fp_ops->qp.data = dev->data->queue_pairs;
	fp_ops->qp.enq_cb = dev->enq_cbs;
	fp_ops->qp.deq_cb = dev->deq_cbs;
}

void *
rte_cryptodev_session_event_mdata_get(struct rte_crypto_op *op)
{
	if (op->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC &&
			op->sess_type == RTE_CRYPTO_OP_WITH_SESSION)
		return rte_cryptodev_sym_session_get_user_data(op->sym->session);
	else if (op->type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC &&
			op->sess_type == RTE_CRYPTO_OP_WITH_SESSION)
		return op->asym->session->event_mdata;
	else if (op->sess_type == RTE_CRYPTO_OP_SESSIONLESS &&
			op->private_data_offset)
		return ((uint8_t *)op + op->private_data_offset);
	else
		return NULL;
}
