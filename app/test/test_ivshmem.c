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

#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <stdio.h>

#include <cmdline_parse.h>

#include "test.h"

#include <rte_common.h>
#include <rte_ivshmem.h>
#include <rte_string_fns.h>
#include "process.h"

#define DUPLICATE_METADATA "duplicate"
#define METADATA_NAME "metadata"
#define NONEXISTENT_METADATA "nonexistent"
#define FIRST_TEST 'a'

#define launch_proc(ARGV) process_dup(ARGV, \
		sizeof(ARGV)/(sizeof(ARGV[0])), "test_ivshmem")

#define ASSERT(cond,msg) do {						\
		if (!(cond)) {								\
			printf("**** TEST %s() failed: %s\n",	\
				__func__, msg);						\
			return -1;								\
		}											\
} while(0)

static char*
get_current_prefix(char * prefix, int size)
{
	char path[PATH_MAX] = {0};
	char buf[PATH_MAX] = {0};

	/* get file for config (fd is always 3) */
	snprintf(path, sizeof(path), "/proc/self/fd/%d", 3);

	/* return NULL on error */
	if (readlink(path, buf, sizeof(buf)) == -1)
		return NULL;

	/* get the basename */
	snprintf(buf, sizeof(buf), "%s", basename(buf));

	/* copy string all the way from second char up to start of _config */
	snprintf(prefix, size, "%.*s",
			(int)(strnlen(buf, sizeof(buf)) - sizeof("_config")),
			&buf[1]);

	return prefix;
}

static struct rte_ivshmem_metadata*
mmap_metadata(const char *name)
{
	int fd;
	char pathname[PATH_MAX];
	struct rte_ivshmem_metadata *metadata;

	snprintf(pathname, sizeof(pathname),
			"/var/run/.dpdk_ivshmem_metadata_%s", name);

	fd = open(pathname, O_RDWR, 0660);
	if (fd < 0)
		return NULL;

	metadata = (struct rte_ivshmem_metadata*) mmap(NULL,
			sizeof(struct rte_ivshmem_metadata), PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, 0);

	if (metadata == MAP_FAILED)
		return NULL;

	close(fd);

	return metadata;
}

static int
create_duplicate(void)
{
	/* create a metadata that another process will then try to overwrite */
	ASSERT (rte_ivshmem_metadata_create(DUPLICATE_METADATA) == 0,
			"Creating metadata failed");
	return 0;
}

static int
test_ivshmem_create_lots_of_memzones(void)
{
	int i;
	char name[IVSHMEM_NAME_LEN];
	const struct rte_memzone *mz;

	ASSERT(rte_ivshmem_metadata_create(METADATA_NAME) == 0,
			"Failed to create metadata");

	for (i = 0; i < RTE_LIBRTE_IVSHMEM_MAX_ENTRIES; i++) {
		snprintf(name, sizeof(name), "mz_%i", i);

		mz = rte_memzone_reserve(name, RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY, 0);
		ASSERT(mz != NULL, "Failed to reserve memzone");

		ASSERT(rte_ivshmem_metadata_add_memzone(mz, METADATA_NAME) == 0,
				"Failed to add memzone");
	}
	mz = rte_memzone_reserve("one too many", RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY, 0);
	ASSERT(mz != NULL, "Failed to reserve memzone");

	ASSERT(rte_ivshmem_metadata_add_memzone(mz, METADATA_NAME) < 0,
		"Metadata should have been full");

	return 0;
}

static int
test_ivshmem_create_duplicate_memzone(void)
{
	const struct rte_memzone *mz;

	ASSERT(rte_ivshmem_metadata_create(METADATA_NAME) == 0,
			"Failed to create metadata");

	mz = rte_memzone_reserve("mz", RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY, 0);
	ASSERT(mz != NULL, "Failed to reserve memzone");

	ASSERT(rte_ivshmem_metadata_add_memzone(mz, METADATA_NAME) == 0,
			"Failed to add memzone");

	ASSERT(rte_ivshmem_metadata_add_memzone(mz, METADATA_NAME) < 0,
			"Added the same memzone twice");

	return 0;
}

static int
test_ivshmem_api_test(void)
{
	const struct rte_memzone * mz;
	struct rte_mempool * mp;
	struct rte_ring * r;
	char buf[BUFSIZ];

	memset(buf, 0, sizeof(buf));

	r = rte_ring_create("ring", 1, SOCKET_ID_ANY, 0);
	mp = rte_mempool_create("mempool", 1, 1, 1, 1, NULL, NULL, NULL, NULL,
			SOCKET_ID_ANY, 0);
	mz = rte_memzone_reserve("memzone", 64, SOCKET_ID_ANY, 0);

	ASSERT(r != NULL, "Failed to create ring");
	ASSERT(mp != NULL, "Failed to create mempool");
	ASSERT(mz != NULL, "Failed to reserve memzone");

	/* try to create NULL metadata */
	ASSERT(rte_ivshmem_metadata_create(NULL) < 0,
			"Created metadata with NULL name");

	/* create valid metadata to do tests on */
	ASSERT(rte_ivshmem_metadata_create(METADATA_NAME) == 0,
			"Failed to create metadata");

	/* test adding memzone */
	ASSERT(rte_ivshmem_metadata_add_memzone(NULL, NULL) < 0,
			"Added NULL memzone to NULL metadata");
	ASSERT(rte_ivshmem_metadata_add_memzone(NULL, METADATA_NAME) < 0,
			"Added NULL memzone");
	ASSERT(rte_ivshmem_metadata_add_memzone(mz, NULL) < 0,
			"Added memzone to NULL metadata");
	ASSERT(rte_ivshmem_metadata_add_memzone(mz, NONEXISTENT_METADATA) < 0,
			"Added memzone to nonexistent metadata");

	/* test adding ring */
	ASSERT(rte_ivshmem_metadata_add_ring(NULL, NULL) < 0,
			"Added NULL ring to NULL metadata");
	ASSERT(rte_ivshmem_metadata_add_ring(NULL, METADATA_NAME) < 0,
			"Added NULL ring");
	ASSERT(rte_ivshmem_metadata_add_ring(r, NULL) < 0,
			"Added ring to NULL metadata");
	ASSERT(rte_ivshmem_metadata_add_ring(r, NONEXISTENT_METADATA) < 0,
			"Added ring to nonexistent metadata");

	/* test adding mempool */
	ASSERT(rte_ivshmem_metadata_add_mempool(NULL, NULL) < 0,
			"Added NULL mempool to NULL metadata");
	ASSERT(rte_ivshmem_metadata_add_mempool(NULL, METADATA_NAME) < 0,
			"Added NULL mempool");
	ASSERT(rte_ivshmem_metadata_add_mempool(mp, NULL) < 0,
			"Added mempool to NULL metadata");
	ASSERT(rte_ivshmem_metadata_add_mempool(mp, NONEXISTENT_METADATA) < 0,
			"Added mempool to nonexistent metadata");

	/* test creating command line */
	ASSERT(rte_ivshmem_metadata_cmdline_generate(NULL, sizeof(buf), METADATA_NAME) < 0,
			"Written command line into NULL buffer");
	ASSERT(strnlen(buf, sizeof(buf)) == 0, "Buffer is not empty");

	ASSERT(rte_ivshmem_metadata_cmdline_generate(buf, 0, METADATA_NAME) < 0,
			"Written command line into small buffer");
	ASSERT(strnlen(buf, sizeof(buf)) == 0, "Buffer is not empty");

	ASSERT(rte_ivshmem_metadata_cmdline_generate(buf, sizeof(buf), NULL) < 0,
			"Written command line for NULL metadata");
	ASSERT(strnlen(buf, sizeof(buf)) == 0, "Buffer is not empty");

	ASSERT(rte_ivshmem_metadata_cmdline_generate(buf, sizeof(buf),
			NONEXISTENT_METADATA) < 0,
			"Writen command line for nonexistent metadata");
	ASSERT(strnlen(buf, sizeof(buf)) == 0, "Buffer is not empty");

	/* add stuff to config */
	ASSERT(rte_ivshmem_metadata_add_memzone(mz, METADATA_NAME) == 0,
			"Failed to add memzone to valid config");
	ASSERT(rte_ivshmem_metadata_add_ring(r, METADATA_NAME) == 0,
			"Failed to add ring to valid config");
	ASSERT(rte_ivshmem_metadata_add_mempool(mp, METADATA_NAME) == 0,
			"Failed to add mempool to valid config");

	/* create config */
	ASSERT(rte_ivshmem_metadata_cmdline_generate(buf, sizeof(buf),
			METADATA_NAME) == 0, "Failed to write command-line");

	/* check if something was written */
	ASSERT(strnlen(buf, sizeof(buf)) != 0, "Buffer is empty");

	/* make sure we don't segfault */
	rte_ivshmem_metadata_dump(stdout, NULL);

	/* dump our metadata */
	rte_ivshmem_metadata_dump(stdout, METADATA_NAME);

	return 0;
}

static int
test_ivshmem_create_duplicate_metadata(void)
{
	ASSERT(rte_ivshmem_metadata_create(DUPLICATE_METADATA) < 0,
			"Creating duplicate metadata should have failed");

	return 0;
}

static int
test_ivshmem_create_metadata_config(void)
{
	struct rte_ivshmem_metadata *metadata;

	rte_ivshmem_metadata_create(METADATA_NAME);

	metadata = mmap_metadata(METADATA_NAME);

	ASSERT(metadata != MAP_FAILED, "Metadata mmaping failed");

	ASSERT(metadata->magic_number == IVSHMEM_MAGIC,
			"Magic number is not that magic");

	ASSERT(strncmp(metadata->name, METADATA_NAME, sizeof(metadata->name)) == 0,
			"Name has not been set up");

	ASSERT(metadata->entry[0].offset == 0, "Offest is not initialized");
	ASSERT(metadata->entry[0].mz.addr == 0, "mz.addr is not initialized");
	ASSERT(metadata->entry[0].mz.len == 0, "mz.len is not initialized");

	return 0;
}

static int
test_ivshmem_create_multiple_metadata_configs(void)
{
	int i;
	char name[IVSHMEM_NAME_LEN];
	struct rte_ivshmem_metadata *metadata;

	for (i = 0; i < RTE_LIBRTE_IVSHMEM_MAX_METADATA_FILES / 2; i++) {
		snprintf(name, sizeof(name), "test_%d", i);
		rte_ivshmem_metadata_create(name);
		metadata = mmap_metadata(name);

		ASSERT(metadata->magic_number == IVSHMEM_MAGIC,
				"Magic number is not that magic");

		ASSERT(strncmp(metadata->name, name, sizeof(metadata->name)) == 0,
				"Name has not been set up");
	}

	return 0;
}

static int
test_ivshmem_create_too_many_metadata_configs(void)
{
	int i;
	char name[IVSHMEM_NAME_LEN];

	for (i = 0; i < RTE_LIBRTE_IVSHMEM_MAX_METADATA_FILES; i++) {
		snprintf(name, sizeof(name), "test_%d", i);
		ASSERT(rte_ivshmem_metadata_create(name) == 0,
				"Create config file failed");
	}

	ASSERT(rte_ivshmem_metadata_create(name) < 0,
			"Create config file didn't fail");

	return 0;
}

enum rte_ivshmem_tests {
	_test_ivshmem_api_test = 0,
	_test_ivshmem_create_metadata_config,
	_test_ivshmem_create_multiple_metadata_configs,
	_test_ivshmem_create_too_many_metadata_configs,
	_test_ivshmem_create_duplicate_metadata,
	_test_ivshmem_create_lots_of_memzones,
	_test_ivshmem_create_duplicate_memzone,
	_last_test,
};

#define RTE_IVSHMEM_TEST_ID "RTE_IVSHMEM_TEST_ID"

static int
launch_all_tests_on_secondary_processes(void)
{
	int ret = 0;
	char id;
	char testid;
	char tmp[PATH_MAX] = {0};
	char prefix[PATH_MAX] = {0};

	get_current_prefix(tmp, sizeof(tmp));

	snprintf(prefix, sizeof(prefix), "--file-prefix=%s", tmp);

	const char *argv[] = { prgname, "-c", "1", "-n", "3",
			"--proc-type=secondary", prefix };

	for (id = 0; id < _last_test; id++) {
		testid = (char)(FIRST_TEST + id);
		setenv(RTE_IVSHMEM_TEST_ID, &testid, 1);
		if (launch_proc(argv) != 0)
			return -1;
	}
	return ret;
}

int
test_ivshmem(void)
{
	int testid;

	/* We want to have a clean execution for every test without exposing
	 * private global data structures in rte_ivshmem so we launch each test
	 * on a different secondary process. */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {

		/* first, create metadata */
		ASSERT(create_duplicate() == 0, "Creating metadata failed");

		return launch_all_tests_on_secondary_processes();
	}

	testid = *(getenv(RTE_IVSHMEM_TEST_ID)) - FIRST_TEST;

	printf("Secondary process running test %d \n", testid);

	switch (testid) {
	case _test_ivshmem_api_test:
		return test_ivshmem_api_test();

	case _test_ivshmem_create_metadata_config:
		return test_ivshmem_create_metadata_config();

	case _test_ivshmem_create_multiple_metadata_configs:
		return test_ivshmem_create_multiple_metadata_configs();

	case _test_ivshmem_create_too_many_metadata_configs:
		return test_ivshmem_create_too_many_metadata_configs();

	case _test_ivshmem_create_duplicate_metadata:
		return test_ivshmem_create_duplicate_metadata();

	case _test_ivshmem_create_lots_of_memzones:
		return test_ivshmem_create_lots_of_memzones();

	case _test_ivshmem_create_duplicate_memzone:
		return test_ivshmem_create_duplicate_memzone();

	default:
		break;
	}

	return -1;
}

static struct test_command ivshmem_cmd = {
	.command = "ivshmem_autotest",
	.callback = test_ivshmem,
};
REGISTER_TEST_COMMAND(ivshmem_cmd);
