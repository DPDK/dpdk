/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Cesnet
 */

#include <rte_kvargs.h>
#include <ethdev_vdev.h>

#include "nfb.h"

#define VDEV_NFB_DRIVER net_vdev_nfb
#define VDEV_NFB_ARG_DEV "dev"

static int
vdev_nfb_vdev_probe(struct rte_vdev_device *dev)
{
	unsigned int count;
	int ret = 0;
	size_t len, pos;

	struct nfb_probe_params params;

	const char *vdev_name = rte_vdev_device_name(dev);
	const char *vdev_args = rte_vdev_device_args(dev);
	char *dev_params, *dev_params_mod;
	struct rte_kvargs *kvargs;

	kvargs = rte_kvargs_parse(vdev_args, NULL);
	if (kvargs == NULL) {
		NFB_LOG(ERR, "Failed to parse device arguments %s", vdev_args);
		ret = -EINVAL;
		goto err_parse_args;
	}

	dev_params = strdup(vdev_args);
	if (dev_params == NULL) {
		ret = -ENOMEM;
		goto err_strdup_params;
	}

	params.device = &dev->device;
	params.specific_init = NULL;
	params.specific_device = NULL;
	params.path = nfb_default_dev_path();
	params.args = dev_params;
	params.nfb_id = 0;
	params.ep_index = -1;

	len = strlen(dev_params) + 1;
	pos = 0;
	dev_params[pos] = '\0';

	/* Parse parameters for virtual device */
	for (count = 0; count != kvargs->count; ++count) {
		const struct rte_kvargs_pair *pair = &kvargs->pairs[count];

		if (!strcmp(pair->key, VDEV_NFB_ARG_DEV)) {
			params.path = pair->value;
		} else {
			/* Clone non-vdev arguments, result is shorter or equal length */
			dev_params_mod = dev_params + pos;
			ret = snprintf(dev_params_mod, len, "%s%s=%s",
					pos == 0 ? "" : ",", pair->key, pair->value);
			if (ret < 0 || ret >= (signed int)len)
				goto err_clone_args;
			pos += ret;
			len -= ret;
		}
	}

	strlcpy(params.name, vdev_name, sizeof(params.name));

	ret = nfb_eth_common_probe(&params);
	if (ret)
		goto err_nfb_common_probe;

	free(dev_params);
	rte_kvargs_free(kvargs);

	return ret;

err_nfb_common_probe:
err_clone_args:
	free(dev_params);
err_strdup_params:
	rte_kvargs_free(kvargs);
err_parse_args:
	return ret;
}

static int
vdev_nfb_vdev_remove(struct rte_vdev_device *dev)
{
	return nfb_eth_common_remove(&dev->device);
}

/** Virtual device descriptor. */
static struct rte_vdev_driver vdev_nfb_vdev = {
	.probe = vdev_nfb_vdev_probe,
	.remove = vdev_nfb_vdev_remove,
};

RTE_PMD_REGISTER_VDEV(VDEV_NFB_DRIVER, vdev_nfb_vdev);
RTE_PMD_REGISTER_ALIAS(VDEV_NFB_DRIVER, eth_vdev_nfb);
RTE_PMD_REGISTER_PARAM_STRING(net_vdev_nfb,
		VDEV_NFB_ARG_DEV "=<string>"
		);
