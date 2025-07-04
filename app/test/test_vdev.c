/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 6WIND S.A.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <rte_common.h>
#include <rte_kvargs.h>
#include <bus_driver.h>
#include <rte_bus_vdev.h>

#include "test.h"

#define TEST_VDEV_KEY_NAME "name"

static const char * const valid_keys[] = {
	TEST_VDEV_KEY_NAME,
	NULL,
};

static int
cmp_dev_name(const struct rte_device *dev, const void *name)
{
	return strcmp(rte_dev_name(dev), name);
}

static int
cmp_dev_match(const struct rte_device *dev, const void *_kvlist)
{
	const struct rte_kvargs *kvlist = _kvlist;
	const char *key = TEST_VDEV_KEY_NAME;
	const char *name;

	/* no kvlist arg, all devices match */
	if (kvlist == NULL)
		return 0;

	/* if key is present in kvlist and does not match, filter device */
	name = rte_kvargs_get(kvlist, key);
	if (name != NULL && strcmp(name, rte_dev_name(dev)) != 0)
		return -1;

	return 0;
}

static struct rte_device *
get_matching_vdev(const char *match_str)
{
	struct rte_bus *vdev_bus = rte_bus_find_by_name("vdev");
	struct rte_kvargs *kvargs = NULL;
	struct rte_device *dev;

	if (match_str != NULL) {
		kvargs = rte_kvargs_parse(match_str, valid_keys);
		if (kvargs == NULL) {
			printf("Failed to parse match string\n");
			return NULL;
		}
	}

	dev = vdev_bus->find_device(NULL, cmp_dev_match, kvargs);
	rte_kvargs_free(kvargs);

	return dev;
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
	dev0 = vdev_bus->find_device(NULL, cmp_dev_name, "net_null_test0");
	if (dev0 == NULL) {
		printf("Cannot find net_null_test0 vdev\n");
		goto fail;
	}

	/* create second vdev */
	if (rte_vdev_init("net_null_test1", "") < 0) {
		printf("Failed to create vdev net_null_test1\n");
		goto fail;
	}
	dev1 = vdev_bus->find_device(NULL, cmp_dev_name, "net_null_test1");
	if (dev1 == NULL) {
		printf("Cannot find net_null_test1 vdev\n");
		goto fail;
	}

	/* try to match vdevs */
	dev = get_matching_vdev("name=net_null_test0");
	if (dev != dev0) {
		printf("Cannot match net_null_test0 vdev\n");
		goto fail;
	}

	dev = get_matching_vdev("name=net_null_test1");
	if (dev != dev1) {
		printf("Cannot match net_null_test1 vdev\n");
		goto fail;
	}

	dev = get_matching_vdev("name=unexistant");
	if (dev != NULL) {
		printf("Unexistant vdev should not match\n");
		goto fail;
	}

	dev = get_matching_vdev("");
	if (dev == NULL || dev == dev1) {
		printf("Cannot match any vdev with empty match string\n");
		goto fail;
	}

	dev = get_matching_vdev(NULL);
	if (dev == NULL || dev == dev1) {
		printf("Cannot match any vdev with NULL match string\n");
		goto fail;
	}

	/* iterate all vdevs, and ensure we find vdev0 and vdev1 */
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

REGISTER_FAST_TEST(vdev_autotest, true, true, test_vdev);
