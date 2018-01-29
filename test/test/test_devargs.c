/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2014 6WIND S.A.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include <rte_debug.h>
#include <rte_devargs.h>

#include "test.h"

/* clear devargs list that was modified by the test */
static void free_devargs_list(void)
{
	struct rte_devargs *devargs;

	while (!TAILQ_EMPTY(&devargs_list)) {
		devargs = TAILQ_FIRST(&devargs_list);
		TAILQ_REMOVE(&devargs_list, devargs, next);
		free(devargs->args);
		free(devargs);
	}
}

static int
test_devargs(void)
{
	struct rte_devargs_list save_devargs_list;
	struct rte_devargs *devargs;

	/* save the real devargs_list, it is restored at the end of the test */
	save_devargs_list = devargs_list;
	TAILQ_INIT(&devargs_list);

	/* test valid cases */
	if (rte_eal_devargs_add(RTE_DEVTYPE_WHITELISTED_PCI, "08:00.1") < 0)
		goto fail;
	if (rte_eal_devargs_add(RTE_DEVTYPE_WHITELISTED_PCI, "0000:5:00.0") < 0)
		goto fail;
	if (rte_eal_devargs_add(RTE_DEVTYPE_BLACKLISTED_PCI, "04:00.0,arg=val") < 0)
		goto fail;
	if (rte_eal_devargs_add(RTE_DEVTYPE_BLACKLISTED_PCI, "0000:01:00.1") < 0)
		goto fail;
	if (rte_eal_devargs_type_count(RTE_DEVTYPE_WHITELISTED_PCI) != 2)
		goto fail;
	if (rte_eal_devargs_type_count(RTE_DEVTYPE_BLACKLISTED_PCI) != 2)
		goto fail;
	if (rte_eal_devargs_type_count(RTE_DEVTYPE_VIRTUAL) != 0)
		goto fail;
	if (rte_eal_devargs_add(RTE_DEVTYPE_VIRTUAL, "net_ring0") < 0)
		goto fail;
	if (rte_eal_devargs_add(RTE_DEVTYPE_VIRTUAL, "net_ring1,key=val,k2=val2") < 0)
		goto fail;
	if (rte_eal_devargs_type_count(RTE_DEVTYPE_VIRTUAL) != 2)
		goto fail;
	free_devargs_list();

	/* check virtual device with argument parsing */
	if (rte_eal_devargs_add(RTE_DEVTYPE_VIRTUAL, "net_ring1,k1=val,k2=val2") < 0)
		goto fail;
	devargs = TAILQ_FIRST(&devargs_list);
	if (strncmp(devargs->name, "net_ring1",
			sizeof(devargs->name)) != 0)
		goto fail;
	if (!devargs->args || strcmp(devargs->args, "k1=val,k2=val2") != 0)
		goto fail;
	free_devargs_list();

	/* check PCI device with empty argument parsing */
	if (rte_eal_devargs_add(RTE_DEVTYPE_WHITELISTED_PCI, "04:00.1") < 0)
		goto fail;
	devargs = TAILQ_FIRST(&devargs_list);
	if (strcmp(devargs->name, "04:00.1") != 0)
		goto fail;
	if (!devargs->args || strcmp(devargs->args, "") != 0)
		goto fail;
	free_devargs_list();

	/* test error case: bad PCI address */
	if (rte_eal_devargs_add(RTE_DEVTYPE_WHITELISTED_PCI, "08:1") == 0)
		goto fail;
	if (rte_eal_devargs_add(RTE_DEVTYPE_WHITELISTED_PCI, "00.1") == 0)
		goto fail;
	if (rte_eal_devargs_add(RTE_DEVTYPE_WHITELISTED_PCI, "foo") == 0)
		goto fail;
	if (rte_eal_devargs_add(RTE_DEVTYPE_WHITELISTED_PCI, ",") == 0)
		goto fail;
	if (rte_eal_devargs_add(RTE_DEVTYPE_WHITELISTED_PCI, "000f:0:0") == 0)
		goto fail;

	devargs_list = save_devargs_list;
	return 0;

 fail:
	free_devargs_list();
	devargs_list = save_devargs_list;
	return -1;
}

REGISTER_TEST_COMMAND(devargs_autotest, test_devargs);
