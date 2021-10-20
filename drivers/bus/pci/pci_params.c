/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 Gaëtan Rivet
 */

#include <sys/queue.h>

#include <rte_bus.h>
#include <rte_bus_pci.h>
#include <rte_dev.h>
#include <rte_errno.h>
#include <rte_kvargs.h>
#include <rte_devargs.h>
#include <rte_pci.h>
#include <rte_debug.h>

#include "private.h"

enum pci_params {
	RTE_PCI_PARAM_ADDR,
	RTE_PCI_PARAM_MAX,
};

static const char * const pci_params_keys[] = {
	[RTE_PCI_PARAM_ADDR] = "addr",
	[RTE_PCI_PARAM_MAX] = NULL,
};

static int
pci_addr_kv_cmp(const char *key __rte_unused,
		const char *value,
		void *_addr2)
{
	struct rte_pci_addr _addr1;
	struct rte_pci_addr *addr1 = &_addr1;
	struct rte_pci_addr *addr2 = _addr2;

	if (rte_pci_addr_parse(value, addr1))
		return -1;
	return -abs(rte_pci_addr_cmp(addr1, addr2));
}

static int
pci_dev_match(const struct rte_device *dev,
	      const void *_kvlist)
{
	const struct rte_kvargs *kvlist = _kvlist;
	const struct rte_pci_device *pdev;

	if (kvlist == NULL)
		/* Empty string matches everything. */
		return 0;
	pdev = RTE_DEV_TO_PCI_CONST(dev);
	/* if any field does not match. */
	if (rte_kvargs_process(kvlist, pci_params_keys[RTE_PCI_PARAM_ADDR],
			       &pci_addr_kv_cmp,
			       (void *)(intptr_t)&pdev->addr))
		return 1;
	return 0;
}

void *
rte_pci_dev_iterate(const void *start,
		    const char *str,
		    const struct rte_dev_iterator *it __rte_unused)
{
	rte_bus_find_device_t find_device;
	struct rte_kvargs *kvargs = NULL;
	struct rte_device *dev;

	if (str != NULL) {
		kvargs = rte_kvargs_parse(str, pci_params_keys);
		if (kvargs == NULL) {
			RTE_LOG(ERR, EAL, "cannot parse argument list\n");
			rte_errno = EINVAL;
			return NULL;
		}
	}
	find_device = rte_pci_bus.bus.find_device;
	dev = find_device(start, pci_dev_match, kvargs);
	rte_kvargs_free(kvargs);
	return dev;
}

int
rte_pci_devargs_parse(struct rte_devargs *da)
{
	struct rte_kvargs *kvargs;
	const char *addr_str;
	struct rte_pci_addr addr;
	int ret = 0;

	if (da == NULL || da->bus_str == NULL)
		return 0;

	kvargs = rte_kvargs_parse(da->bus_str, NULL);
	if (kvargs == NULL) {
		RTE_LOG(ERR, EAL, "cannot parse argument list: %s\n",
			da->bus_str);
		ret = -ENODEV;
		goto out;
	}

	addr_str = rte_kvargs_get(kvargs, pci_params_keys[RTE_PCI_PARAM_ADDR]);
	if (addr_str == NULL) {
		RTE_LOG(DEBUG, EAL, "No PCI address specified using '%s=<id>' in: %s\n",
			pci_params_keys[RTE_PCI_PARAM_ADDR], da->bus_str);
		goto out;
	}

	ret = rte_pci_addr_parse(addr_str, &addr);
	if (ret != 0) {
		RTE_LOG(ERR, EAL, "PCI address invalid: %s\n", da->bus_str);
		ret = -EINVAL;
		goto out;
	}

	rte_pci_device_name(&addr, da->name, sizeof(da->name));

out:
	if (kvargs != NULL)
		rte_kvargs_free(kvargs);
	if (ret != 0)
		rte_errno = -ret;
	return ret;
}
