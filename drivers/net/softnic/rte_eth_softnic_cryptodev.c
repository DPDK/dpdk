/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdlib.h>
#include <stdio.h>

#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include <rte_string_fns.h>

#include "rte_eth_softnic_internals.h"

#define SOFTNIC_CRYPTO_SESSION_CACHE_SIZE 128

int
softnic_cryptodev_init(struct pmd_internals *p)
{
	TAILQ_INIT(&p->cryptodev_list);

	return 0;
}

void
softnic_cryptodev_free(struct pmd_internals *p)
{
	for ( ; ; ) {
		struct softnic_cryptodev *cryptodev;

		cryptodev = TAILQ_FIRST(&p->cryptodev_list);
		if (cryptodev == NULL)
			break;

		TAILQ_REMOVE(&p->cryptodev_list, cryptodev, node);
		free(cryptodev);
	}
}

struct softnic_cryptodev *
softnic_cryptodev_find(struct pmd_internals *p,
	const char *name)
{
	struct softnic_cryptodev *cryptodev;

	if (name == NULL)
		return NULL;

	TAILQ_FOREACH(cryptodev, &p->cryptodev_list, node)
		if (strcmp(cryptodev->name, name) == 0)
			return cryptodev;

	return NULL;
}

struct softnic_cryptodev *
softnic_cryptodev_create(struct pmd_internals *p,
	const char *name,
	struct softnic_cryptodev_params *params)
{
	struct rte_cryptodev_info dev_info;
	struct rte_cryptodev_config dev_conf;
	struct rte_cryptodev_qp_conf queue_conf;
	struct softnic_cryptodev *cryptodev;
	uint32_t dev_id, i;
	uint32_t socket_id;
	uint32_t cache_size;
	char mp_name[NAME_SIZE];
	int status;

	/* Check input params */
	if ((name == NULL) ||
		softnic_cryptodev_find(p, name) ||
		(params->n_queues == 0) ||
		(params->queue_size == 0) ||
		(params->session_pool_size == 0))
		return NULL;

	if (params->dev_name) {
		status = rte_cryptodev_get_dev_id(params->dev_name);
		if (status == -1)
			return NULL;

		dev_id = (uint32_t)status;
	} else {
		if (rte_cryptodev_is_valid_dev(params->dev_id) == 0)
			return NULL;

		dev_id = params->dev_id;
	}

	cache_size = (params->session_pool_size / 2 <
			SOFTNIC_CRYPTO_SESSION_CACHE_SIZE) ?
					(params->session_pool_size / 2) :
					SOFTNIC_CRYPTO_SESSION_CACHE_SIZE;

	socket_id = rte_cryptodev_socket_id(dev_id);
	rte_cryptodev_info_get(dev_id, &dev_info);

	if (dev_info.max_nb_queue_pairs < params->n_queues)
		return NULL;
	if (dev_info.feature_flags & RTE_CRYPTODEV_FF_HW_ACCELERATED)
		return NULL;

	dev_conf.socket_id = socket_id;
	dev_conf.nb_queue_pairs = params->n_queues;

	status = rte_cryptodev_configure(dev_id, &dev_conf);
	if (status < 0)
		return NULL;

	queue_conf.nb_descriptors = params->queue_size;
	for (i = 0; i < params->n_queues; i++) {
		status = rte_cryptodev_queue_pair_setup(dev_id, i,
				&queue_conf, socket_id);
		if (status < 0)
			return NULL;
	}

	if (rte_cryptodev_start(dev_id) < 0)
		return NULL;

	cryptodev = calloc(1, sizeof(struct softnic_cryptodev));
	if (cryptodev == NULL) {
		rte_cryptodev_stop(dev_id);
		return NULL;
	}

	strlcpy(cryptodev->name, name, sizeof(cryptodev->name));
	cryptodev->dev_id = dev_id;
	cryptodev->n_queues = params->n_queues;

	snprintf(mp_name, NAME_SIZE, "%s_mp%u", name, dev_id);
	cryptodev->mp_create = rte_cryptodev_sym_session_pool_create(mp_name,
			params->session_pool_size,
			0,
			cache_size,
			0,
			socket_id);
	if (!cryptodev->mp_create)
		goto error_exit;

	snprintf(mp_name, NAME_SIZE, "%s_priv_mp%u", name, dev_id);
	cryptodev->mp_init = rte_mempool_create(mp_name,
			params->session_pool_size,
			rte_cryptodev_sym_get_private_session_size(dev_id),
			cache_size,
			0,
			NULL,
			NULL,
			NULL,
			NULL,
			socket_id,
			0);
	if (!cryptodev->mp_init)
		goto error_exit;

	TAILQ_INSERT_TAIL(&p->cryptodev_list, cryptodev, node);

	return cryptodev;

error_exit:
	rte_mempool_free(cryptodev->mp_create);
	rte_mempool_free(cryptodev->mp_init);

	free(cryptodev);

	return NULL;
}
