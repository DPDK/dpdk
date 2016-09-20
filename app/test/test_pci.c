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
#include "resource.h"

/* Generic maximum number of drivers to have room to allocate all drivers */
#define NUM_MAX_DRIVERS 256

/*
 * PCI test
 * ========
 *
 * - Register a driver with a ``probe()`` function.
 *
 * - Dump all PCI devices.
 *
 * - Check that the ``probe()`` function is called at least once.
 */

int test_pci_run = 0; /* value checked by the multiprocess test */
static unsigned pci_dev_count;

static int my_driver_init(struct rte_pci_driver *dr,
			  struct rte_pci_device *dev);

/* IXGBE NICS */
struct rte_pci_id my_driver_id[] = {
	{RTE_PCI_DEVICE(0x0001, 0x1234)},
	{ .vendor_id = 0, /* sentinel */ },
};

struct rte_pci_id my_driver_id2[] = {
	{RTE_PCI_DEVICE(0x0001, 0x4444)},
	{RTE_PCI_DEVICE(0x0002, 0xabcd)},
	{ .vendor_id = 0, /* sentinel */ },
};

struct rte_pci_driver my_driver = {
	.driver = {
		.name = "test_driver"
	},
	.probe = my_driver_init,
	.id_table = my_driver_id,
	.drv_flags = 0,
};

struct rte_pci_driver my_driver2 = {
	.driver = {
		.name = "test_driver2"
	},
	.probe = my_driver_init,
	.id_table = my_driver_id2,
	.drv_flags = 0,
};

static int
my_driver_init(__attribute__((unused)) struct rte_pci_driver *dr,
	       struct rte_pci_device *dev)
{
	printf("My driver init called in %s\n", dr->driver.name);
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
		free(devargs->args);
		free(devargs);
	}
}

/* backup real devices & drivers (not used for testing) */
struct pci_driver_list real_pci_driver_list =
	TAILQ_HEAD_INITIALIZER(real_pci_driver_list);
struct pci_device_list real_pci_device_list =
	TAILQ_HEAD_INITIALIZER(real_pci_device_list);

REGISTER_LINKED_RESOURCE(test_pci_sysfs);

static int
test_pci_setup(void)
{
	struct rte_pci_device *dev;
	struct rte_pci_driver *dr;
	const struct resource *r;
	int ret;

	r = resource_find("test_pci_sysfs");
	TEST_ASSERT_NOT_NULL(r, "missing resource test_pci_sysfs");

	ret = resource_untar(r);
	TEST_ASSERT_SUCCESS(ret, "failed to untar %s", r->name);

	ret = setenv("SYSFS_PCI_DEVICES", "test_pci_sysfs/bus/pci/devices", 1);
	TEST_ASSERT_SUCCESS(ret, "failed to setenv");

	/* Unregister original devices & drivers lists */
	while (!TAILQ_EMPTY(&pci_driver_list)) {
		dr = TAILQ_FIRST(&pci_driver_list);
		rte_eal_pci_unregister(dr);
		TAILQ_INSERT_TAIL(&real_pci_driver_list, dr, next);
	}

	while (!TAILQ_EMPTY(&pci_device_list)) {
		dev = TAILQ_FIRST(&pci_device_list);
		TAILQ_REMOVE(&pci_device_list, dev, next);
		TAILQ_INSERT_TAIL(&real_pci_device_list, dev, next);
	}

	ret = rte_eal_pci_scan();
	TEST_ASSERT_SUCCESS(ret, "failed to scan PCI bus");
	rte_eal_pci_dump(stdout);

	return 0;
}

static int
test_pci_cleanup(void)
{
	struct rte_pci_device *dev;
	struct rte_pci_driver *dr;
	const struct resource *r;
	int ret;

	unsetenv("SYSFS_PCI_DEVICES");

	r = resource_find("test_pci_sysfs");
	TEST_ASSERT_NOT_NULL(r, "missing resource test_pci_sysfs");

	ret = resource_rm_by_tar(r);
	TEST_ASSERT_SUCCESS(ret, "Failed to delete resource %s", r->name);

	/*
	 * FIXME: there is no API in DPDK to free a rte_pci_device so we
	 * cannot free the devices in the right way. Let's assume that we
	 * don't care for tests.
	 */
	while (!TAILQ_EMPTY(&pci_device_list)) {
		dev = TAILQ_FIRST(&pci_device_list);
		TAILQ_REMOVE(&pci_device_list, dev, next);
	}

	/* Restore original devices & drivers lists */
	while (!TAILQ_EMPTY(&real_pci_driver_list)) {
		dr = TAILQ_FIRST(&real_pci_driver_list);
		TAILQ_REMOVE(&real_pci_driver_list, dr, next);
		rte_eal_pci_register(dr);
	}

	while (!TAILQ_EMPTY(&real_pci_device_list)) {
		dev = TAILQ_FIRST(&real_pci_device_list);
		TAILQ_REMOVE(&real_pci_device_list, dev, next);
		TAILQ_INSERT_TAIL(&pci_device_list, dev, next);
	}

	return 0;
}

static int
test_pci_blacklist(void)
{
	struct rte_devargs_list save_devargs_list;

	printf("Dump all devices\n");
	TEST_ASSERT(TAILQ_EMPTY(&pci_driver_list),
			"pci_driver_list not empty");

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

	return 0;
}

static int test_pci_sysfs(void)
{
	const char *orig;
	const char *newpath;
	int ret;

	orig = pci_get_sysfs_path();
	ret = setenv("SYSFS_PCI_DEVICES", "My Documents", 1);
	TEST_ASSERT_SUCCESS(ret, "Failed setenv to My Documents");

	newpath = pci_get_sysfs_path();
	TEST_ASSERT(!strcmp(newpath, "My Documents"),
			"pci_get_sysfs_path() should return 'My Documents' "
			"but gives %s", newpath);

	ret = setenv("SYSFS_PCI_DEVICES", orig, 1);
	TEST_ASSERT_SUCCESS(ret, "Failed setenv back to '%s'", orig);

	newpath = pci_get_sysfs_path();
	TEST_ASSERT(!strcmp(orig, newpath),
			"pci_get_sysfs_path returned unexpected path: "
			"%s (expected: %s)", newpath, orig);
	return 0;
}

int
test_pci(void)
{
	if (test_pci_sysfs())
		return -1;

	if (test_pci_setup())
		return -1;

	if (test_pci_blacklist())
		return -1;

	if (test_pci_cleanup())
		return -1;

	return 0;
}

REGISTER_TEST_COMMAND(pci_autotest, test_pci);
