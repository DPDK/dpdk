/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 RehiveTech. All rights reserved.
 */

#include <stdio.h>
#include <string.h>

#include "test.h"
#include "resource.h"

const char test_resource_dpdk_blob[] = {
	'\x44', '\x50', '\x44', '\x4b', '\x00'
};

REGISTER_RESOURCE(test_resource_dpdk,
		test_resource_dpdk_blob, test_resource_dpdk_blob + 4);

static int test_resource_dpdk(void)
{
	const struct resource *r;

	r = resource_find("test_resource_dpdk");
	TEST_ASSERT_NOT_NULL(r, "Could not find test_resource_dpdk");
	TEST_ASSERT(!strcmp(r->name, "test_resource_dpdk"),
			"Found resource %s, expected test_resource_dpdk",
			r->name);

	TEST_ASSERT(!strncmp("DPDK", r->begin, 4),
			"Unexpected payload: %.4s...", r->begin);

	return 0;
}

REGISTER_LINKED_RESOURCE(test_resource_c);

static int test_resource_c(void)
{
	const struct resource *r;
	FILE *f;

	r = resource_find("test_resource_c");
	TEST_ASSERT_NOT_NULL(r, "No test_resource_c found");
	TEST_ASSERT(!strcmp(r->name, "test_resource_c"),
			"Found resource %s, expected test_resource_c",
			r->name);

	TEST_ASSERT_SUCCESS(resource_fwrite_file(r, "test_resource.c"),
			"Failed to write file %s", r->name);

	f = fopen("test_resource.c", "r");
	TEST_ASSERT_NOT_NULL(f,
			"Missing extracted file resource.c");
	fclose(f);
	remove("test_resource.c");

	return 0;
}

#ifdef RTE_APP_TEST_RESOURCE_TAR
REGISTER_LINKED_RESOURCE(test_resource_tar);

static int test_resource_tar(void)
{
	const struct resource *r;
	FILE *f;

	r = resource_find("test_resource_tar");
	TEST_ASSERT_NOT_NULL(r, "No test_resource_tar found");
	TEST_ASSERT(!strcmp(r->name, "test_resource_tar"),
			"Found resource %s, expected test_resource_tar",
			r->name);

	TEST_ASSERT_SUCCESS(resource_untar(r),
			"Failed to to untar %s", r->name);

	f = fopen("test_resource.c", "r");
	TEST_ASSERT_NOT_NULL(f,
			"Missing extracted file test_resource.c");
	fclose(f);

	TEST_ASSERT_SUCCESS(resource_rm_by_tar(r),
			"Failed to remove extracted contents of %s", r->name);
	return 0;
}

#endif /* RTE_APP_TEST_RESOURCE_TAR */

static int test_resource(void)
{
	if (test_resource_dpdk())
		return -1;

	if (test_resource_c())
		return -1;

#ifdef RTE_APP_TEST_RESOURCE_TAR
	if (test_resource_tar())
		return -1;
#endif /* RTE_APP_TEST_RESOURCE_TAR */

	return 0;
}

REGISTER_TEST_COMMAND(resource_autotest, test_resource);
