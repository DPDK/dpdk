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

#include "rte_cryptodev_vdev.h"
#include "rte_cryptodev_pci.h"
#include "rte_cryptodev_pmd.h"

/**
 * Parse name from argument
 */
static int
rte_cryptodev_vdev_parse_name_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	struct rte_crypto_vdev_init_params *params = extra_args;

	if (strlen(value) >= RTE_CRYPTODEV_NAME_MAX_LEN - 1) {
		CDEV_LOG_ERR("Invalid name %s, should be less than "
				"%u bytes", value,
				RTE_CRYPTODEV_NAME_MAX_LEN - 1);
		return -1;
	}

	strncpy(params->name, value, RTE_CRYPTODEV_NAME_MAX_LEN);

	return 0;
}

/**
 * Parse integer from argument
 */
static int
rte_cryptodev_vdev_parse_integer_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	int *i = extra_args;

	*i = atoi(value);
	if (*i < 0) {
		CDEV_LOG_ERR("Argument has to be positive.");
		return -1;
	}

	return 0;
}

struct rte_cryptodev *
rte_cryptodev_vdev_pmd_init(const char *name, size_t dev_private_size,
		int socket_id, struct rte_vdev_device *vdev)
{
	struct rte_cryptodev *cryptodev;

	/* allocate device structure */
	cryptodev = rte_cryptodev_pmd_allocate(name, socket_id);
	if (cryptodev == NULL)
		return NULL;

	/* allocate private device structure */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		cryptodev->data->dev_private =
				rte_zmalloc_socket("cryptodev device private",
						dev_private_size,
						RTE_CACHE_LINE_SIZE,
						socket_id);

		if (cryptodev->data->dev_private == NULL)
			rte_panic("Cannot allocate memzone for private device"
					" data");
	}

	cryptodev->device = &vdev->device;

	/* initialise user call-back tail queue */
	TAILQ_INIT(&(cryptodev->link_intr_cbs));

	return cryptodev;
}

int
rte_cryptodev_vdev_parse_init_params(struct rte_crypto_vdev_init_params *params,
		const char *input_args)
{
	struct rte_kvargs *kvlist = NULL;
	int ret = 0;

	if (params == NULL)
		return -EINVAL;

	if (input_args) {
		kvlist = rte_kvargs_parse(input_args,
				cryptodev_vdev_valid_params);
		if (kvlist == NULL)
			return -1;

		ret = rte_kvargs_process(kvlist,
					RTE_CRYPTODEV_VDEV_MAX_NB_QP_ARG,
					&rte_cryptodev_vdev_parse_integer_arg,
					&params->max_nb_queue_pairs);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist,
					RTE_CRYPTODEV_VDEV_MAX_NB_SESS_ARG,
					&rte_cryptodev_vdev_parse_integer_arg,
					&params->max_nb_sessions);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist, RTE_CRYPTODEV_VDEV_SOCKET_ID,
					&rte_cryptodev_vdev_parse_integer_arg,
					&params->socket_id);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist, RTE_CRYPTODEV_VDEV_NAME,
					&rte_cryptodev_vdev_parse_name_arg,
					params);
		if (ret < 0)
			goto free_kvlist;
	}

free_kvlist:
	rte_kvargs_free(kvlist);
	return ret;
}

int
rte_cryptodev_pci_generic_probe(struct rte_pci_device *pci_dev,
			size_t private_data_size,
			cryptodev_pci_init_t dev_init)
{
	struct rte_cryptodev *cryptodev;

	char cryptodev_name[RTE_CRYPTODEV_NAME_MAX_LEN];

	int retval;

	rte_pci_device_name(&pci_dev->addr, cryptodev_name,
			sizeof(cryptodev_name));

	cryptodev = rte_cryptodev_pmd_allocate(cryptodev_name, rte_socket_id());
	if (cryptodev == NULL)
		return -ENOMEM;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		cryptodev->data->dev_private =
				rte_zmalloc_socket(
						"cryptodev private structure",
						private_data_size,
						RTE_CACHE_LINE_SIZE,
						rte_socket_id());

		if (cryptodev->data->dev_private == NULL)
			rte_panic("Cannot allocate memzone for private "
					"device data");
	}

	cryptodev->device = &pci_dev->device;

	/* init user callbacks */
	TAILQ_INIT(&(cryptodev->link_intr_cbs));

	/* Invoke PMD device initialization function */
	RTE_FUNC_PTR_OR_ERR_RET(*dev_init, -EINVAL);
	retval = dev_init(cryptodev);
	if (retval == 0)
		return 0;

	CDEV_LOG_ERR("driver %s: crypto_dev_init(vendor_id=0x%x device_id=0x%x)"
			" failed", pci_dev->device.driver->name,
			(unsigned int) pci_dev->id.vendor_id,
			(unsigned int) pci_dev->id.device_id);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(cryptodev->data->dev_private);

	/* free crypto device */
	rte_cryptodev_pmd_release_device(cryptodev);

	return -ENXIO;
}

int
rte_cryptodev_pci_generic_remove(struct rte_pci_device *pci_dev,
		cryptodev_pci_uninit_t dev_uninit)
{
	struct rte_cryptodev *cryptodev;
	char cryptodev_name[RTE_CRYPTODEV_NAME_MAX_LEN];
	int ret;

	if (pci_dev == NULL)
		return -EINVAL;

	rte_pci_device_name(&pci_dev->addr, cryptodev_name,
			sizeof(cryptodev_name));

	cryptodev = rte_cryptodev_pmd_get_named_dev(cryptodev_name);
	if (cryptodev == NULL)
		return -ENODEV;

	/* Invoke PMD device uninit function */
	if (dev_uninit) {
		ret = dev_uninit(cryptodev);
		if (ret)
			return ret;
	}

	/* free crypto device */
	rte_cryptodev_pmd_release_device(cryptodev);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(cryptodev->data->dev_private);

	cryptodev->device = NULL;
	cryptodev->data = NULL;

	return 0;
}
