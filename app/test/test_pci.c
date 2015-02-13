/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   Copyright(c) 2014 6WIND S.A.
 *   All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/queue.h>

#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ethdev.h>
#include <rte_devargs.h>

#include "test.h"

/* Generic maximum number of drivers to have room to allocate all drivers */
#define NUM_MAX_DRIVERS 256

/*
 * PCI test
 * ========
 *
 * - Register a driver with a ``devinit()`` function.
 *
 * - Dump all PCI devices.
 *
 * - Check that the ``devinit()`` function is called at least once.
 */

int test_pci_run = 0; /* value checked by the multiprocess test */
static unsigned pci_dev_count;

static int my_driver_init(struct rte_pci_driver *dr,
			  struct rte_pci_device *dev);

/* IXGBE NICS */
struct rte_pci_id my_driver_id[] = {

#define RTE_PCI_DEV_ID_DECL_IXGBE(vend, dev) {RTE_PCI_DEVICE(vend, dev)},
#include <rte_pci_dev_ids.h>

{ .vendor_id = 0, /* sentinel */ },
};

struct rte_pci_id my_driver_id2[] = {

/* IGB & EM NICS */
#define RTE_PCI_DEV_ID_DECL_EM(vend, dev) {RTE_PCI_DEVICE(vend, dev)},
#define RTE_PCI_DEV_ID_DECL_IGB(vend, dev) {RTE_PCI_DEVICE(vend, dev)},
#include <rte_pci_dev_ids.h>

{ .vendor_id = 0, /* sentinel */ },
};

struct rte_pci_driver my_driver = {
	.name = "test_driver",
	.devinit = my_driver_init,
	.id_table = my_driver_id,
	.drv_flags = 0,
};

struct rte_pci_driver my_driver2 = {
	.name = "test_driver2",
	.devinit = my_driver_init,
	.id_table = my_driver_id2,
	.drv_flags = 0,
};

static int
my_driver_init(__attribute__((unused)) struct rte_pci_driver *dr,
	       struct rte_pci_device *dev)
{
	printf("My driver init called in %s\n", dr->name);
	printf("%x:%x:%x.%d", dev->addr.domain, dev->addr.bus,
	       dev->addr.devid, dev->addr.function);
	printf(" - vendor:%x device:%x\n", dev->id.vendor_id, dev->id.device_id);

	pci_dev_count ++;
	return 0;
}

static void
blacklist_all_devices(void)
{
	struct rte_pci_device *dev = NULL;
	unsigned i = 0;
	char pci_addr_str[16];

	TAILQ_FOREACH(dev, &pci_device_list, next) {
		snprintf(pci_addr_str, sizeof(pci_addr_str), PCI_PRI_FMT,
			dev->addr.domain, dev->addr.bus, dev->addr.devid,
			dev->addr.function);
		if (rte_eal_devargs_add(RTE_DEVTYPE_BLACKLISTED_PCI,
				pci_addr_str) < 0) {
			printf("Error: cannot blacklist <%s>", pci_addr_str);
			break;
		}
		i++;
	}
	printf("%u devices blacklisted\n", i);
}

/* clear devargs list that was modified by the test */
static void free_devargs_list(void)
{
	struct rte_devargs *devargs;

	while (!TAILQ_EMPTY(&devargs_list)) {
		devargs = TAILQ_FIRST(&devargs_list);
		TAILQ_REMOVE(&devargs_list, devargs, next);
		if (devargs->args)
			free(devargs->args);
		free(devargs);
	}
}

int
test_pci(void)
{
	struct rte_devargs_list save_devargs_list;
	struct rte_pci_driver *dr = NULL;
	struct rte_pci_driver *save_pci_driver_list[NUM_MAX_DRIVERS];
	unsigned i, num_drivers = 0;

	printf("Dump all devices\n");
	rte_eal_pci_dump(stdout);

	/* Unregister all previous drivers */
	TAILQ_FOREACH(dr, &pci_driver_list, next) {
		rte_eal_pci_unregister(dr);
		save_pci_driver_list[num_drivers++] = dr;
	}

	rte_eal_pci_register(&my_driver);
	rte_eal_pci_register(&my_driver2);

	pci_dev_count = 0;
	printf("Scan bus\n");
	rte_eal_pci_probe();

	if (pci_dev_count == 0) {
		printf("no device detected\n");
		return -1;
	}

	/* save the real devargs_list */
	save_devargs_list = devargs_list;
	TAILQ_INIT(&devargs_list);

	blacklist_all_devices();

	pci_dev_count = 0;
	printf("Scan bus with all devices blacklisted\n");
	rte_eal_pci_probe();

	free_devargs_list();
	devargs_list = save_devargs_list;

	if (pci_dev_count != 0) {
		printf("not all devices are blacklisted\n");
		return -1;
	}

	test_pci_run = 1;

	rte_eal_pci_unregister(&my_driver);
	rte_eal_pci_unregister(&my_driver2);

	/* Restore original driver list */
	for (i = 0; i < num_drivers; i++)
		rte_eal_pci_register(save_pci_driver_list[i]);

	return 0;
}

static struct test_command pci_cmd = {
	.command = "pci_autotest",
	.callback = test_pci,
};
REGISTER_TEST_COMMAND(pci_cmd);
