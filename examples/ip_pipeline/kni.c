/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdlib.h>
#include <string.h>

#include <rte_ethdev.h>
#include <rte_bus_pci.h>
#include <rte_string_fns.h>

#include "kni.h"
#include "mempool.h"
#include "link.h"

static struct kni_list kni_list;

#ifndef KNI_MAX
#define KNI_MAX                                            16
#endif

int
kni_init(void)
{
	TAILQ_INIT(&kni_list);

#ifdef RTE_LIBRTE_KNI
	rte_kni_init(KNI_MAX);
#endif

	return 0;
}

struct kni *
kni_find(const char *name)
{
	struct kni *kni;

	if (name == NULL)
		return NULL;

	TAILQ_FOREACH(kni, &kni_list, node)
		if (strcmp(kni->name, name) == 0)
			return kni;

	return NULL;
}

#ifndef RTE_LIBRTE_KNI

struct kni *
kni_create(const char *name __rte_unused,
	struct kni_params *params __rte_unused)
{
	return NULL;
}

void
kni_handle_request(void)
{
	return;
}

#else

static int
kni_config_network_interface(uint16_t port_id, uint8_t if_up)
{
	int ret = 0;

	if (!rte_eth_dev_is_valid_port(port_id))
		return -EINVAL;

	ret = (if_up) ?
		rte_eth_dev_set_link_up(port_id) :
		rte_eth_dev_set_link_down(port_id);

	return ret;
}

static int
kni_change_mtu(uint16_t port_id, unsigned int new_mtu)
{
	int ret;

	if (!rte_eth_dev_is_valid_port(port_id))
		return -EINVAL;

	if (new_mtu > ETHER_MAX_LEN)
		return -EINVAL;

	/* Set new MTU */
	ret = rte_eth_dev_set_mtu(port_id, new_mtu);
	if (ret < 0)
		return ret;

	return 0;
}

struct kni *
kni_create(const char *name, struct kni_params *params)
{
	struct rte_eth_dev_info dev_info;
	struct rte_kni_conf kni_conf;
	struct rte_kni_ops kni_ops;
	struct kni *kni;
	struct mempool *mempool;
	struct link *link;
	struct rte_kni *k;
	const struct rte_pci_device *pci_dev;
	const struct rte_bus *bus = NULL;

	/* Check input params */
	if ((name == NULL) ||
		kni_find(name) ||
		(params == NULL))
		return NULL;

	mempool = mempool_find(params->mempool_name);
	link = link_find(params->link_name);
	if ((mempool == NULL) ||
		(link == NULL))
		return NULL;

	/* Resource create */
	rte_eth_dev_info_get(link->port_id, &dev_info);

	memset(&kni_conf, 0, sizeof(kni_conf));
	strlcpy(kni_conf.name, name, RTE_KNI_NAMESIZE);
	kni_conf.force_bind = params->force_bind;
	kni_conf.core_id = params->thread_id;
	kni_conf.group_id = link->port_id;
	kni_conf.mbuf_size = mempool->buffer_size;
	if (dev_info.device)
		bus = rte_bus_find_by_device(dev_info.device);
	if (bus && !strcmp(bus->name, "pci")) {
		pci_dev = RTE_DEV_TO_PCI(dev_info.device);
		kni_conf.addr = pci_dev->addr;
		kni_conf.id = pci_dev->id;
	}

	memset(&kni_ops, 0, sizeof(kni_ops));
	kni_ops.port_id = link->port_id;
	kni_ops.config_network_if = kni_config_network_interface;
	kni_ops.change_mtu = kni_change_mtu;

	k = rte_kni_alloc(mempool->m, &kni_conf, &kni_ops);
	if (k == NULL)
		return NULL;

	/* Node allocation */
	kni = calloc(1, sizeof(struct kni));
	if (kni == NULL)
		return NULL;

	/* Node fill in */
	strlcpy(kni->name, name, sizeof(kni->name));
	kni->k = k;

	/* Node add to list */
	TAILQ_INSERT_TAIL(&kni_list, kni, node);

	return kni;
}

void
kni_handle_request(void)
{
	struct kni *kni;

	TAILQ_FOREACH(kni, &kni_list, node)
		rte_kni_handle_request(kni->k);
}

#endif
