/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 RehiveTech. All rights reserved.
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
 *     * Neither the name of RehiveTech nor the names of its
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
			"Failed to to write file %s", r->name);

	f = fopen("test_resource.c", "r");
	TEST_ASSERT_NOT_NULL(f,
			"Missing extracted file resource.c");
	fclose(f);
	remove("test_resource.c");

	return 0;
}

static int test_resource(void)
{
	if (test_resource_dpdk())
		return -1;

	if (test_resource_c())
		return -1;

	return 0;
}

static struct test_command resource_cmd = {
	.command = "resource_autotest",
	.callback = test_resource,
};
REGISTER_TEST_COMMAND(resource_cmd);
