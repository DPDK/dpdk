/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Gaëtan Rivet
 */

#include <stdlib.h>
#include <string.h>

#include <rte_class.h>
#include <rte_errno.h>
#include <rte_kvargs.h>
#include <rte_log.h>

#include "rte_ethdev.h"
#include "ethdev_driver.h"
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
	struct rte_ether_addr mac;
	const struct rte_eth_dev_data *data = opaque;
	struct rte_eth_dev_info dev_info;
	uint32_t index;

	/* Parse devargs MAC address. */
	if (rte_ether_unformat_addr(value, &mac) < 0)
		return -1; /* invalid devargs value */

	/* Return 0 if devargs MAC is matching one of the device MACs. */
	rte_eth_dev_info_get(data->port_id, &dev_info);
	for (index = 0; index < dev_info.max_mac_addrs; index++)
		if (rte_is_same_ether_addr(&mac, &data->mac_addrs[index]))
			return 0;
	return -1; /* no match */
}

static int
eth_representor_cmp(const char *key __rte_unused,
		const char *value, void *opaque)
{
	int ret;
	char *values;
	const struct rte_eth_dev *edev = opaque;
	const struct rte_eth_dev_data *data = edev->data;
	struct rte_eth_devargs eth_da;
	uint16_t id = 0, nc, np, nf, i, c, p, f;

	if ((data->dev_flags & RTE_ETH_DEV_REPRESENTOR) == 0)
		return -1; /* not a representor port */

	/* Parse devargs representor values. */
	values = strdup(value);
	if (values == NULL)
		return -1;
	memset(&eth_da, 0, sizeof(eth_da));
	ret = rte_eth_devargs_parse_representor_ports(values, &eth_da);
	free(values);
	if (ret != 0)
		return -1; /* invalid devargs value */

	if (eth_da.nb_mh_controllers == 0 && eth_da.nb_ports == 0 &&
	    eth_da.nb_representor_ports == 0)
		return -1;
	nc = eth_da.nb_mh_controllers > 0 ? eth_da.nb_mh_controllers : 1;
	np = eth_da.nb_ports > 0 ? eth_da.nb_ports : 1;
	nf = eth_da.nb_representor_ports > 0 ? eth_da.nb_representor_ports : 1;

	/* Return 0 if representor ID is matching one of the values. */
	for (i = 0; i < nc * np * nf; ++i) {
		c = i / (np * nf);
		p = (i / nf) % np;
		f = i % nf;
		if (rte_eth_representor_id_get(edev->data->backer_port_id,
			eth_da.type,
			eth_da.nb_mh_controllers == 0 ? -1 :
					eth_da.mh_controllers[c],
			eth_da.nb_ports == 0 ? -1 : eth_da.ports[p],
			eth_da.nb_representor_ports == 0 ? -1 :
					eth_da.representor_ports[f],
			&id) < 0)
			continue;
		if (data->representor_id == id)
			return 0;
	}
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
			eth_representor_cmp, (void *)(uintptr_t)edev);
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
