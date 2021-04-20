/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#include <rte_string_fns.h>
#include <rte_malloc.h>
#include <rte_kvargs.h>
#include <rte_eal.h>

#include "rte_compressdev_internal.h"
#include "rte_compressdev_pmd.h"

/**
 * Parse name from argument
 */
static int
rte_compressdev_pmd_parse_name_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	struct rte_compressdev_pmd_init_params *params = extra_args;
	int n;

	n = strlcpy(params->name, value, RTE_COMPRESSDEV_NAME_MAX_LEN);
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

int
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

struct rte_compressdev *
rte_compressdev_pmd_create(const char *name,
		struct rte_device *device,
		size_t private_data_size,
		struct rte_compressdev_pmd_init_params *params)
{
	struct rte_compressdev *compressdev;

	if (params->name[0] != '\0') {
		COMPRESSDEV_LOG(INFO, "User specified device name = %s\n",
				params->name);
		name = params->name;
	}

	COMPRESSDEV_LOG(INFO, "Creating compressdev %s\n", name);

	COMPRESSDEV_LOG(INFO, "Init parameters - name: %s, socket id: %d",
			name, params->socket_id);

	/* allocate device structure */
	compressdev = rte_compressdev_pmd_allocate(name, params->socket_id);
	if (compressdev == NULL) {
		COMPRESSDEV_LOG(ERR, "Failed to allocate comp device %s", name);
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
					"Cannot allocate memory for compressdev"
					" %s private data", name);

			rte_compressdev_pmd_release_device(compressdev);
			return NULL;
		}
	}

	compressdev->device = device;

	return compressdev;
}

int
rte_compressdev_pmd_destroy(struct rte_compressdev *compressdev)
{
	int retval;

	COMPRESSDEV_LOG(INFO, "Closing comp device %s",
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
