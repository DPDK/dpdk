/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
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
 *     * Neither the name of the copyright holder nor the names of its
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

#include <rte_malloc.h>

#include "rte_cryptodev_pmd.h"

/**
 * Parse name from argument
 */
static int
rte_cryptodev_pmd_parse_name_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	struct rte_cryptodev_pmd_init_params *params = extra_args;
	int n;

	n = snprintf(params->name, RTE_CRYPTODEV_NAME_MAX_LEN, "%s", value);
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
				RTE_CRYPTODEV_PMD_MAX_NB_SESS_ARG,
				&rte_cryptodev_pmd_parse_uint_arg,
				&params->max_nb_sessions);
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
		CDEV_LOG_INFO("[%s] User specified device name = %s\n",
				device->driver->name, params->name);
		name = params->name;
	}

	CDEV_LOG_INFO("[%s] - Creating cryptodev %s\n",
			device->driver->name, name);

	CDEV_LOG_INFO("[%s] - Initialisation parameters - name: %s,"
			"socket id: %d, max queue pairs: %u, max sessions: %u",
			device->driver->name, name,
			params->socket_id, params->max_nb_queue_pairs,
			params->max_nb_sessions);

	/* allocate device structure */
	cryptodev = rte_cryptodev_pmd_allocate(name, params->socket_id);
	if (cryptodev == NULL) {
		CDEV_LOG_ERR("[%s] Failed to allocate crypto device for %s",
				device->driver->name, name);
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
			CDEV_LOG_ERR("[%s] Cannot allocate memory for "
					"cryptodev %s private data",
					device->driver->name, name);

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

	CDEV_LOG_INFO("[%s] Closing crypto device %s",
			cryptodev->device->driver->name,
			cryptodev->device->name);

	/* free crypto device */
	retval = rte_cryptodev_pmd_release_device(cryptodev);
	if (retval)
		return retval;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(cryptodev->data->dev_private);


	cryptodev->device = NULL;
	cryptodev->data = NULL;

	return 0;
}
