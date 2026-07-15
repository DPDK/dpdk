/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 6WIND S.A.
 */

#include <stdio.h>
#include <string.h>

#include <rte_dev.h>
#include <rte_bus.h>
#include <rte_bus_vdev.h>

#include "test.h"

static struct rte_device *
find_vdev_by_name(const char *name)
{
	struct rte_dev_iterator it = { 0 };
	struct rte_device *dev;

	RTE_DEV_FOREACH(dev, "bus=vdev", &it) {
		if (strcmp(rte_dev_name(dev), name) == 0)
			return dev;
	}
	return NULL;
}

static int
test_vdev_bus(void)
{
	struct rte_bus *vdev_bus = rte_bus_find_by_name("vdev");
	struct rte_dev_iterator dev_iter = { 0 };
	struct rte_device *dev, *dev0, *dev1;

	/* not supported */
	if (vdev_bus == NULL)
		return 0;

	/* create first vdev */
	if (rte_vdev_init("net_null_test0", "") < 0) {
		printf("Failed to create vdev net_null_test0\n");
		goto fail;
	}
	dev0 = find_vdev_by_name("net_null_test0");
	if (dev0 == NULL) {
		printf("Cannot find net_null_test0 vdev\n");
		goto fail;
	}

	/* create second vdev */
	if (rte_vdev_init("net_null_test1", "") < 0) {
		printf("Failed to create vdev net_null_test1\n");
		goto fail;
	}
	dev1 = find_vdev_by_name("net_null_test1");
	if (dev1 == NULL) {
		printf("Cannot find net_null_test1 vdev\n");
		goto fail;
	}

	/* try to find vdevs */
	dev = find_vdev_by_name("net_null_test0");
	if (dev != dev0) {
		printf("Cannot match net_null_test0 vdev\n");
		goto fail;
	}

	dev = find_vdev_by_name("net_null_test1");
	if (dev != dev1) {
		printf("Cannot match net_null_test1 vdev\n");
		goto fail;
	}

	dev = find_vdev_by_name("nonexistent");
	if (dev != NULL) {
		printf("Nonexistent vdev should not match\n");
		goto fail;
	}

	/* iterate all vdevs, and ensure we find dev0 and dev1 */
	RTE_DEV_FOREACH(dev, "bus=vdev", &dev_iter) {
		if (dev == dev0)
			dev0 = NULL;
		else if (dev == dev1)
			dev1 = NULL;
	}
	if (dev0 != NULL) {
		printf("dev0 was not iterated\n");
		goto fail;
	}
	if (dev1 != NULL) {
		printf("dev1 was not iterated\n");
		goto fail;
	}

	rte_vdev_uninit("net_null_test0");
	rte_vdev_uninit("net_null_test1");

	return 0;

fail:
	rte_vdev_uninit("net_null_test0");
	rte_vdev_uninit("net_null_test1");
	return -1;
}

static int
test_vdev(void)
{
	printf("== test vdev bus ==\n");
	if (test_vdev_bus() < 0)
		return -1;
	return 0;
}

REGISTER_FAST_TEST(vdev_autotest, NOHUGE_OK, ASAN_OK, test_vdev);
