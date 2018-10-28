/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Gaëtan Rivet
 */

#include <string.h>

#include <cmdline_parse_etheraddr.h>
#include <rte_class.h>
#include <rte_compat.h>
#include <rte_errno.h>
#include <rte_kvargs.h>
#include <rte_log.h>

#include "rte_ethdev.h"
#include "rte_ethdev_core.h"
#include "rte_ethdev_driver.h"
#include "ethdev_private.h"

enum eth_params {
	RTE_ETH_PARAM_MAC,
	RTE_ETH_PARAM_REPRESENTOR,
	RTE_ETH_PARAM_MAX,
};

static const char * const eth_params_keys[] = {
	[RTE_ETH_PARAM_MAC] = "mac",
	[RTE_ETH_PARAM_REPRESENTOR] = "representor",
	[RTE_ETH_PARAM_MAX] = NULL,
};

struct eth_dev_match_arg {
	struct rte_device *device;
	struct rte_kvargs *kvlist;
};

#define eth_dev_match_arg(d, k) \
	(&(const struct eth_dev_match_arg) { \
		.device = (d), \
		.kvlist = (k), \
	})

static int
eth_mac_cmp(const char *key __rte_unused,
		const char *value, void *opaque)
{
	int ret;
	struct ether_addr mac;
	const struct rte_eth_dev_data *data = opaque;
	struct rte_eth_dev_info dev_info;
	uint32_t index;

	/* Parse devargs MAC address. */
	/*
	 * cannot use ether_aton_r(value, &mac)
	 * because of include conflict with rte_ether.h
	 */
	ret = cmdline_parse_etheraddr(NULL, value, &mac, sizeof(mac));
	if (ret < 0)
		return -1; /* invalid devargs value */

	/* Return 0 if devargs MAC is matching one of the device MACs. */
	rte_eth_dev_info_get(data->port_id, &dev_info);
	for (index = 0; index < dev_info.max_mac_addrs; index++)
		if (is_same_ether_addr(&mac, &data->mac_addrs[index]))
			return 0;
	return -1; /* no match */
}

static int
eth_representor_cmp(const char *key __rte_unused,
		const char *value, void *opaque)
{
	int ret;
	char *values;
	const struct rte_eth_dev_data *data = opaque;
	struct rte_eth_devargs representors;
	uint16_t index;

	if ((data->dev_flags & RTE_ETH_DEV_REPRESENTOR) == 0)
		return -1; /* not a representor port */

	/* Parse devargs representor values. */
	values = strdup(value);
	if (values == NULL)
		return -1;
	memset(&representors, 0, sizeof(representors));
	ret = rte_eth_devargs_parse_list(values,
			rte_eth_devargs_parse_representor_ports,
			&representors);
	free(values);
	if (ret != 0)
		return -1; /* invalid devargs value */

	/* Return 0 if representor id is matching one of the values. */
	for (index = 0; index < representors.nb_representor_ports; index++)
		if (data->representor_id ==
				representors.representor_ports[index])
			return 0;
	return -1; /* no match */
}

static int
eth_dev_match(const struct rte_eth_dev *edev,
	      const void *_arg)
{
	int ret;
	const struct eth_dev_match_arg *arg = _arg;
	const struct rte_kvargs *kvlist = arg->kvlist;
	unsigned int pair;

	if (edev->state == RTE_ETH_DEV_UNUSED)
		return -1;
	if (arg->device != NULL && arg->device != edev->device)
		return -1;

	ret = rte_kvargs_process(kvlist,
			eth_params_keys[RTE_ETH_PARAM_MAC],
			eth_mac_cmp, edev->data);
	if (ret != 0)
		return -1;

	ret = rte_kvargs_process(kvlist,
			eth_params_keys[RTE_ETH_PARAM_REPRESENTOR],
			eth_representor_cmp, edev->data);
	if (ret != 0)
		return -1;
	/* search for representor key */
	for (pair = 0; pair < kvlist->count; pair++) {
		ret = strcmp(kvlist->pairs[pair].key,
				eth_params_keys[RTE_ETH_PARAM_REPRESENTOR]);
		if (ret == 0)
			break; /* there is a representor key */
	}
	/* if no representor key, default is to not match representor ports */
	if (ret != 0)
		if ((edev->data->dev_flags & RTE_ETH_DEV_REPRESENTOR) != 0)
			return -1; /* do not match any representor */

	return 0;
}

static void *
eth_dev_iterate(const void *start,
		const char *str,
		const struct rte_dev_iterator *it)
{
	struct rte_kvargs *kvargs = NULL;
	struct rte_eth_dev *edev = NULL;
	const char * const *valid_keys = NULL;

	if (str != NULL) {
		if (str[0] == '+') /* no validation of keys */
			str++;
		else
			valid_keys = eth_params_keys;
		kvargs = rte_kvargs_parse(str, valid_keys);
		if (kvargs == NULL) {
			RTE_LOG(ERR, EAL, "cannot parse argument list\n");
			rte_errno = EINVAL;
			return NULL;
		}
	}
	edev = eth_find_device(start, eth_dev_match,
			       eth_dev_match_arg(it->device, kvargs));
	rte_kvargs_free(kvargs);
	return edev;
}

static struct rte_class rte_class_eth = {
	.dev_iterate = eth_dev_iterate,
};

RTE_REGISTER_CLASS(eth, rte_class_eth);
