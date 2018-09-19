/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 Gaëtan Rivet
 */

#include <rte_bus.h>
#include <rte_dev.h>
#include <rte_errno.h>
#include <rte_kvargs.h>
#include <rte_pci.h>

#include "private.h"

enum pci_params {
	RTE_PCI_PARAMS_MAX,
};

static const char * const pci_params_keys[] = {
	[RTE_PCI_PARAMS_MAX] = NULL,
};

static int
pci_dev_match(const struct rte_device *dev,
	      const void *_kvlist)
{
	const struct rte_kvargs *kvlist = _kvlist;

	(void) dev;
	(void) kvlist;
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
