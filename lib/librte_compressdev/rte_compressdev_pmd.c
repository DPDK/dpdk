/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#include <rte_malloc.h>
#include <rte_kvargs.h>
#include <rte_eal.h>

#include "rte_compressdev_internal.h"
#include "rte_compressdev_pmd.h"

int compressdev_logtype;

/**
 * Parse name from argument
 */
static int
rte_compressdev_pmd_parse_name_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	struct rte_compressdev_pmd_init_params *params = extra_args;
	int n;

	n = snprintf(params->name, RTE_COMPRESSDEV_NAME_MAX_LEN, "%s", value);
	if (n >= RTE_COMPRESSDEV_NAME_MAX_LEN)
		return -EINVAL;

	return 0;
}

/**
 * Parse unsigned integer from argument
 */
static int
rte_compressdev_pmd_parse_uint_arg(const char *key __rte_unused,
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

int __rte_experimental
rte_compressdev_pmd_parse_input_args(
		struct rte_compressdev_pmd_init_params *params,
		const char *args)
{
	struct rte_kvargs *kvlist = NULL;
	int ret = 0;

	if (params == NULL)
		return -EINVAL;

	if (args) {
		kvlist = rte_kvargs_parse(args,	compressdev_pmd_valid_params);
		if (kvlist == NULL)
			return -EINVAL;

		ret = rte_kvargs_process(kvlist,
				RTE_COMPRESSDEV_PMD_SOCKET_ID_ARG,
				&rte_compressdev_pmd_parse_uint_arg,
				&params->socket_id);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist,
				RTE_COMPRESSDEV_PMD_NAME_ARG,
				&rte_compressdev_pmd_parse_name_arg,
				params);
		if (ret < 0)
			goto free_kvlist;
	}

free_kvlist:
	rte_kvargs_free(kvlist);
	return ret;
}

struct rte_compressdev * __rte_experimental
rte_compressdev_pmd_create(const char *name,
		struct rte_device *device,
		size_t private_data_size,
		struct rte_compressdev_pmd_init_params *params)
{
	struct rte_compressdev *compressdev;

	if (params->name[0] != '\0') {
		COMPRESSDEV_LOG(INFO, "[%s] User specified device name = %s\n",
				device->driver->name, params->name);
		name = params->name;
	}

	COMPRESSDEV_LOG(INFO, "[%s] - Creating compressdev %s\n",
			device->driver->name, name);

	COMPRESSDEV_LOG(INFO,
	"[%s] - Init parameters - name: %s, socket id: %d",
			device->driver->name, name,
			params->socket_id);

	/* allocate device structure */
	compressdev = rte_compressdev_pmd_allocate(name, params->socket_id);
	if (compressdev == NULL) {
		COMPRESSDEV_LOG(ERR, "[%s] Failed to allocate comp device %s",
				device->driver->name, name);
		return NULL;
	}

	/* allocate private device structure */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		compressdev->data->dev_private =
				rte_zmalloc_socket("compressdev device private",
						private_data_size,
						RTE_CACHE_LINE_SIZE,
						params->socket_id);

		if (compressdev->data->dev_private == NULL) {
			COMPRESSDEV_LOG(ERR,
		"[%s] Cannot allocate memory for compressdev %s private data",
					device->driver->name, name);

			rte_compressdev_pmd_release_device(compressdev);
			return NULL;
		}
	}

	compressdev->device = device;

	return compressdev;
}

int __rte_experimental
rte_compressdev_pmd_destroy(struct rte_compressdev *compressdev)
{
	int retval;

	COMPRESSDEV_LOG(INFO, "[%s] Closing comp device %s",
			compressdev->device->driver->name,
			compressdev->device->name);

	/* free comp device */
	retval = rte_compressdev_pmd_release_device(compressdev);
	if (retval)
		return retval;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(compressdev->data->dev_private);

	compressdev->device = NULL;
	compressdev->data = NULL;

	return 0;
}
