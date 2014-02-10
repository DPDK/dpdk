/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#include <cmdline_parse.h>

#include <rte_interrupts.h>
#include <rte_pci.h>

#include "test.h"


#define	TEST_BLACKLIST_NUM	0x100

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
static unsigned driver_registered = 0;
static struct rte_pci_addr blacklist[TEST_BLACKLIST_NUM];

static int my_driver_init(struct rte_pci_driver *dr,
			  struct rte_pci_device *dev);

/*
 * To test cases where RTE_PCI_DRV_NEED_IGB_UIO is set, and isn't set, two
 * drivers are created (one with IGB devices, the other with IXGBE devices).
 */

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
#define RTE_PCI_DEV_USE_82575EB_COPPER
#include <rte_pci_dev_ids.h>

{ .vendor_id = 0, /* sentinel */ },
};

struct rte_pci_driver my_driver = {
	.name = "test_driver",
	.devinit = my_driver_init,
	.id_table = my_driver_id,
#ifdef RTE_EAL_UNBIND_PORTS
	.drv_flags = RTE_PCI_DRV_NEED_IGB_UIO,
#endif
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
blacklist_clear(void)
{
	rte_eal_pci_set_blacklist(NULL, 0);
}



static void
blacklist_all_devices(void)
{
	struct rte_pci_device *dev = NULL;
	unsigned idx = 0;

	memset(blacklist, 0, sizeof (blacklist));

	TAILQ_FOREACH(dev, &device_list, next) {
		if (idx >= sizeof (blacklist) / sizeof (blacklist[0])) {
			printf("Error: too many devices to blacklist");
			break;
		}
		blacklist[idx] = dev->addr;
		++idx;
	}

	rte_eal_pci_set_blacklist(blacklist, idx);
	printf("%u devices blacklisted\n", idx);
}

int
test_pci(void)
{

	printf("Dump all devices\n");
	rte_eal_pci_dump();
	if (driver_registered == 0) {
		rte_eal_pci_register(&my_driver);
		rte_eal_pci_register(&my_driver2);
		driver_registered = 1;
	}

	pci_dev_count = 0;
	printf("Scan bus\n");
	rte_eal_pci_probe();

	if (pci_dev_count == 0) {
		printf("no device detected\n");
		return -1;
	}

	blacklist_all_devices();

	pci_dev_count = 0;
	printf("Scan bus with all devices blacklisted\n");
	rte_eal_pci_probe();

	blacklist_clear();

	if (pci_dev_count != 0) {
		printf("not all devices are blacklisted\n");
		return -1;
	}

	test_pci_run = 1;
	if (driver_registered == 1) {
		rte_eal_pci_unregister(&my_driver);
		rte_eal_pci_unregister(&my_driver2);
		driver_registered = 0;
	}

	return 0;
}
