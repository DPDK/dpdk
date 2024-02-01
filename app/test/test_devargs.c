/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ethdev_driver.h>
#include <rte_common.h>
#include <rte_devargs.h>
#include <rte_kvargs.h>
#include <bus_driver.h>
#include <rte_class.h>

#include "test.h"

/* Check layer arguments. */
static int
test_args(const char *devargs, const char *layer, const char *args, const int n)
{
	struct rte_kvargs *kvlist;

	if (n == 0) {
		if (args != NULL && strlen(args) > 0) {
			printf("rte_devargs_parse(%s) %s args parsed (not expected)\n",
			       devargs, layer);
			return -1;
		} else {
			return 0;
		}
	}
	if (args == NULL) {
		printf("rte_devargs_parse(%s) %s args not parsed\n",
		       devargs, layer);
		return -1;
	}
	kvlist = rte_kvargs_parse(args, NULL);
	if (kvlist == NULL) {
		printf("rte_devargs_parse(%s) %s_str: %s not parsed\n",
		       devargs, layer, args);
		return -1;
	}
	if ((int)kvlist->count != n) {
		printf("rte_devargs_parse(%s) %s_str: %s kv number %u, not %d\n",
		       devargs, layer, args, kvlist->count, n);
		rte_kvargs_free(kvlist);
		return -1;
	}
	rte_kvargs_free(kvlist);
	return 0;
}

struct devargs_case {
	const char *devargs;
	int bus_kv;
	int class_kv;
	int driver_kv;
	const char *bus;
	const char *name;
	const char *class;
};

static int
test_valid_devargs_cases(const struct devargs_case *list, size_t n)
{
	struct rte_devargs da;
	uint32_t i;
	int ret;
	int fail = TEST_SUCCESS;
	struct rte_bus *pci_bus = rte_bus_find_by_name("pci");
	struct rte_bus *vdev_bus = rte_bus_find_by_name("vdev");
	struct rte_class *eth_class = rte_class_find_by_name("eth");

	for (i = 0; i < n; i++) {
		if (pci_bus == NULL && list[i].bus != NULL &&
		    strcmp(list[i].bus, "pci") == 0)
			continue;
		if (vdev_bus == NULL && list[i].bus != NULL &&
		    strcmp(list[i].bus, "vdev") == 0)
			continue;
		if (eth_class == NULL && list[i].class != NULL &&
		    strcmp(list[i].class, "eth") == 0)
			continue;
		memset(&da, 0, sizeof(da));
		ret = rte_devargs_parse(&da, list[i].devargs);
		if (ret < 0) {
			printf("rte_devargs_parse(%s) returned %d (but should not)\n",
			       list[i].devargs, ret);
			goto fail;
		}
		if ((list[i].bus_kv > 0 || list[i].bus != NULL) &&
		    da.bus == NULL) {
			printf("rte_devargs_parse(%s) bus not parsed\n",
			       list[i].devargs);
			goto fail;
		}
		if (test_args(list[i].devargs, "bus", da.bus_str,
			      list[i].bus_kv) != 0)
			goto fail;
		if (list[i].bus != NULL &&
		    strcmp(rte_bus_name(da.bus), list[i].bus) != 0) {
			printf("rte_devargs_parse(%s) bus name (%s) not expected (%s)\n",
			       list[i].devargs, rte_bus_name(da.bus), list[i].bus);
			goto fail;
		}
		if ((list[i].class_kv > 0 || list[i].class != NULL) &&
		    da.cls == NULL) {
			printf("rte_devargs_parse(%s) class not parsed\n",
			       list[i].devargs);
			goto fail;
		}
		if (test_args(list[i].devargs, "class", da.cls_str,
			      list[i].class_kv) != 0)
			goto fail;
		if (list[i].class != NULL &&
		    strcmp(da.cls->name, list[i].class) != 0) {
			printf("rte_devargs_parse(%s) class name (%s) not expected (%s)\n",
			       list[i].devargs, da.cls->name, list[i].class);
			goto fail;
		}
		if (test_args(list[i].devargs, "driver", da.drv_str,
			      list[i].driver_kv) != 0)
			goto fail;
		if (list[i].name != NULL &&
		    strcmp(da.name, list[i].name) != 0) {
			printf("rte_devargs_parse(%s) device name (%s) not expected (%s)\n",
			       list[i].devargs, da.name, list[i].name);
			goto fail;
		}
		goto cleanup;
fail:
		fail = TEST_FAILED;
cleanup:
		rte_devargs_reset(&da);
	}
	return fail;
}

/* Test several valid cases */
static int
test_valid_devargs(void)
{
	static const struct devargs_case list[] = {
		/* Global devargs syntax: */
		{ "bus=pci",
		  1, 0, 0, "pci", NULL, NULL},
		{ "class=eth",
		  0, 1, 0, NULL, NULL, "eth" },
		{ "bus=pci,addr=1:2.3/class=eth/driver=abc,k0=v0",
		  2, 1, 2, "pci", "0000:01:02.3", "eth" },
		{ "bus=vdev,name=/dev/file/name/class=eth",
		  2, 1, 0, "vdev", "/dev/file/name", "eth" },
		{ "bus=vdev,name=/class/bus/path/class=eth",
		  2, 1, 0, "vdev", "/class/bus/path", "eth" },
		{ "bus=vdev,name=///dblslsh/class=eth",
		  2, 1, 0, "vdev", "///dblslsh", "eth" },
		/* Legacy devargs syntax: */
		{ "1:2.3", 0, 0, 0,
		  "pci", "1:2.3", NULL },
		{ "pci:1:2.3,k0=v0",
		  0, 0, 1, "pci", "1:2.3", NULL },
	};
	static const struct devargs_case legacy_ring_list[] = {
		{ "net_ring0",
		  0, 0, 0, "vdev", "net_ring0", NULL },
		{ "net_ring0,iface=test,path=/class/bus/,queues=1",
		  0, 0, 3, "vdev", "net_ring0", NULL },
	};
	struct rte_bus *vdev_bus = rte_bus_find_by_name("vdev");
	int ret;

	ret = test_valid_devargs_cases(list, RTE_DIM(list));
	if (vdev_bus != NULL && vdev_bus->parse("net_ring0", NULL) == 0)
		/* Ring vdev driver enabled. */
		ret |= test_valid_devargs_cases(legacy_ring_list,
						RTE_DIM(legacy_ring_list));
	return ret;
}

/* Test several invalid cases */
static int
test_invalid_devargs(void)
{
	static const char * const list[] = {
		"bus=wrong-bus",
		"class=wrong-class"};
	struct rte_devargs da;
	uint32_t i;
	int ret;
	int fail = 0;

	for (i = 0; i < RTE_DIM(list); i++) {
		ret = rte_devargs_parse(&da, list[i]);
		if (ret >= 0) {
			printf("rte_devargs_parse(%s) returned %d (but should not)\n",
			       list[i], ret);
			fail = ret;
		}
		rte_devargs_reset(&da);
	}
	return fail;
}

struct devargs_parse_case {
	const char *devargs;
	uint8_t devargs_count;
};

static int
test_valid_devargs_parsing(void)
{
	static const struct devargs_parse_case list[] = {
		/* Single representors patterns */
		{"representor=1", 1},
		{"representor=[1,2,3]", 1},
		{"representor=[0-3,5,7]", 1},
		{"representor=vf[0-1]", 1},
		{"representor=sf[0-1]", 1},
		{"representor=pf1vf[0-1]", 1},
		{"representor=pf[0-1]vf[1-2]", 1},
		{"representor=pf[0-1]sf[1-2]", 1},
		{"representor=c0pf[0-1]", 1},
		{"representor=sf[1,2]vf[2]", 1},  /* Only SF ports will be represented */
		{"representor=vf2sf[1-2]", 1},    /* Only VF ports will be represented */
		{"representor=c[0-1]pf[0-1]vf[1-2]", 1},
		/* Single devarg inside multiple representor pattern */
		{"representor=[c[0-1]pf[0-1]vf[1-2]]", 1},
		/* Multiple representors patterns */
		{"representor=[vf0,3]", 2},
		{"representor=[vf0,vf1]", 2},
		{"representor=[vf[0],vf[1]]", 2},
		{"representor=[vf[0,1],3]", 2},
		{"representor=[vf[0,1],[3]]", 2},
		{"representor=[pf1vf[0-1],3]", 2},
		{"representor=[pf1vf[0-1],pf[0-1]]", 2},
		{"representor=[pf1,pf3,pf1vf[0-1],vf[0],vf[1],vf0,vf1,vf2]", 8},
		{"representor=[1,3,5,pf1,pf2,pf3,pf1vf[0-1],vf[0],vf[1],vf0,vf1,vf2]", 12},
		{"representor=[[1,3,5],pf1,pf2,pf3,pf1vf[0-1],vf[0],vf[1],vf0,vf1,vf2]", 10},
		{"representor=[c[0,2-4]pf[1,6]vf[0-1],vf[0],vf4,[3-5,7],pf1vf[0-1,6],pf[2,4,6]]", 6}
	};
	struct rte_eth_devargs eth_da[RTE_MAX_ETHPORTS];
	uint32_t i;
	int ret;
	int fail = 0;

	for (i = 0; i < RTE_DIM(list); i++) {
		memset(eth_da, 0, RTE_MAX_ETHPORTS * sizeof(*eth_da));
		ret = rte_eth_devargs_parse(list[i].devargs, eth_da, RTE_MAX_ETHPORTS);
		if (ret <= 0) {
			printf("rte_devargs_parse(%s) returned %d (but should not)\n",
			       list[i].devargs, ret);
			fail = ret;
			break;
		}

		/* Check the return value, count should be equivalent to no of devargs. */
		if (ret != list[i].devargs_count) {
			printf("Devargs returned count %d != expected count %d\n", ret,
			       list[i].devargs_count);
			fail = -1;
			break;
		}
	}
	return fail;
}

static int
test_invalid_devargs_parsing(void)
{
	static const char * const list[] = {
		"representor=1,2,3,4",			/* Out [] missing */
		"representor=[1 2 3]",			/* , missing */
		"representor=c[1,2]",			/* Only controller no valid */
		"representor=c0vf[0-1]",		/* Controller with vf alone not valid */
		"representor=c0sf[0-1]",		/* Controller with sf alone not valid */
		"representor=vf[0-1],vf3",		/* Out [] missing */
		"representor=[[vf0,3]]",		/* Double [] is invalid */
		"representor=pfvf[1-2]",		/* No PF index provided */
		"representor=pf1[vf[1-2]]",		/* Additional [] across vf */
		"representor=c0[pf1vf[1-2]]",		/* Additional [] across pfvf */
		"representor=c0[pf1[vf[1-2]]]",		/* Additional [] across pfvf */
		"representor=[pf1vf[0-1], pf[0-1]]",	/* Space between two devargs is invalid */
		"representor=[pf1vf[0-1], 3]",		/* Space between two devargs is invalid */
		"representor=pf1vf[1-2],representor=1", /* Multiple representor keys */
	};
	struct rte_eth_devargs eth_da[RTE_MAX_ETHPORTS];
	uint32_t i;
	int ret;
	int fail = 0;

	for (i = 0; i < RTE_DIM(list); i++) {
		memset(eth_da, 0, RTE_MAX_ETHPORTS * sizeof(*eth_da));
		ret = rte_eth_devargs_parse(list[i], eth_da, RTE_MAX_ETHPORTS);
		if (ret > 0) {
			printf("rte_devargs_parse(%s) returned %d (but should not)\n",
			       list[i], ret);
			fail = ret;
			break;
		}
	}
	return fail;
}

static int
test_devargs(void)
{
	printf("== test valid case ==\n");
	if (test_valid_devargs() < 0)
		return -1;
	printf("== test invalid case ==\n");
	if (test_invalid_devargs() < 0)
		return -1;
	printf("== test devargs parsing valid case ==\n");
	if (test_valid_devargs_parsing() < 0)
		return -1;
	printf("== test devargs parsing invalid case ==\n");
	if (test_invalid_devargs_parsing() < 0)
		return -1;
	return 0;
}

REGISTER_FAST_TEST(devargs_autotest, true, true, test_devargs);
