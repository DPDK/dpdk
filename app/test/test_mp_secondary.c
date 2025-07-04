/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>

#include "test.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <sys/queue.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#ifdef RTE_EXEC_ENV_WINDOWS
int
test_mp_secondary(void)
{
	printf("mp_secondary not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}
#else

#include <sys/wait.h>
#include <libgen.h>
#include <dirent.h>
#include <limits.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_errno.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_debug.h>
#include <rte_log.h>
#include <rte_mempool.h>

#ifdef RTE_LIB_HASH
#include <rte_hash.h>
#include <rte_fbk_hash.h>
#endif /* RTE_LIB_HASH */

#ifdef RTE_LIB_LPM
#include <rte_lpm.h>
#endif /* RTE_LIB_LPM */

#include <rte_string_fns.h>

#include "process.h"

#define launch_proc(ARGV) process_dup(ARGV, RTE_DIM(ARGV), __func__)

/*
 * This function is called in the primary i.e. main test, to spawn off secondary
 * processes to run actual mp tests. Uses fork() and exec pair
 */
static int
run_secondary_instances(void)
{
	int ret = 0;
	char coremask[10];

#ifdef RTE_EXEC_ENV_LINUX
	char tmp[PATH_MAX] = {0};
	char prefix[PATH_MAX] = {0};

	get_current_prefix(tmp, sizeof(tmp));

	snprintf(prefix, sizeof(prefix), "--file-prefix=%s", tmp);
#else
	const char *prefix = "";
#endif

	/* good case, using secondary */
	const char *argv1[] = {
			prgname, "-c", coremask, "--proc-type=secondary",
			prefix
	};
	/* good case, using auto */
	const char *argv2[] = {
			prgname, "-c", coremask, "--proc-type=auto",
			prefix
	};
	/* bad case, using invalid type */
	const char *argv3[] = {
			prgname, "-c", coremask, "--proc-type=ERROR",
			prefix
	};
#ifdef RTE_EXEC_ENV_LINUX
	/* bad case, using invalid file prefix */
	const char *argv4[]  = {
			prgname, "-c", coremask, "--proc-type=secondary",
					"--file-prefix=ERROR"
	};
#endif

	snprintf(coremask, sizeof(coremask), "%x", \
			(1 << rte_get_main_lcore()));

	ret |= launch_proc(argv1);
	printf("### Testing rte_mp_disable() reject:\n");
	if (rte_mp_disable()) {
		printf("Error: rte_mp_disable() has been accepted\n");
		ret |= -1;
	} else {
		printf("# Checked rte_mp_disable() is refused\n");
	}
	ret |= launch_proc(argv2);

	ret |= !(launch_proc(argv3));
#ifdef RTE_EXEC_ENV_LINUX
	ret |= !(launch_proc(argv4));
#endif

	return ret;
}

/*
 * This function is run in the secondary instance to test that creation of
 * objects fails in a secondary
 */
static int
run_object_creation_tests(void)
{
	const unsigned flags = 0;
	const unsigned size = 1024;
	const unsigned elt_size = 64;
	const unsigned cache_size = 64;
	const unsigned priv_data_size = 32;

	printf("### Testing object creation - expect lots of mz reserve errors!\n");

	rte_errno = 0;
	if ((rte_memzone_reserve("test_mz", size, rte_socket_id(),
				 flags) == NULL) &&
	    (rte_memzone_lookup("test_mz") == NULL)) {
		printf("Error: unexpected return value from rte_memzone_reserve\n");
		return -1;
	}
	printf("# Checked rte_memzone_reserve() OK\n");

	rte_errno = 0;
	if ((rte_ring_create(
		     "test_ring", size, rte_socket_id(), flags) == NULL) &&
		    (rte_ring_lookup("test_ring") == NULL)){
		printf("Error: unexpected return value from rte_ring_create()\n");
		return -1;
	}
	printf("# Checked rte_ring_create() OK\n");

	rte_errno = 0;
	if ((rte_mempool_create("test_mp", size, elt_size, cache_size,
				priv_data_size, NULL, NULL, NULL, NULL,
				rte_socket_id(), flags) == NULL) &&
	     (rte_mempool_lookup("test_mp") == NULL)){
		printf("Error: unexpected return value from rte_mempool_create()\n");
		return -1;
	}
	printf("# Checked rte_mempool_create() OK\n");

#ifdef RTE_LIB_HASH
	const struct rte_hash_parameters hash_params = { .name = "test_mp_hash" };
	rte_errno=0;
	if ((rte_hash_create(&hash_params) != NULL) &&
	    (rte_hash_find_existing(hash_params.name) == NULL)){
		printf("Error: unexpected return value from rte_hash_create()\n");
		return -1;
	}
	printf("# Checked rte_hash_create() OK\n");

	const struct rte_fbk_hash_params fbk_params = { .name = "test_fbk_mp_hash" };
	rte_errno=0;
	if ((rte_fbk_hash_create(&fbk_params) != NULL) &&
	    (rte_fbk_hash_find_existing(fbk_params.name) == NULL)){
		printf("Error: unexpected return value from rte_fbk_hash_create()\n");
		return -1;
	}
	printf("# Checked rte_fbk_hash_create() OK\n");
#endif

#ifdef RTE_LIB_LPM
	rte_errno=0;
	struct rte_lpm_config config;

	config.max_rules = rte_socket_id();
	config.number_tbl8s = 256;
	config.flags = 0;
	if ((rte_lpm_create("test_lpm", size, &config) != NULL) &&
	    (rte_lpm_find_existing("test_lpm") == NULL)){
		printf("Error: unexpected return value from rte_lpm_create()\n");
		return -1;
	}
	printf("# Checked rte_lpm_create() OK\n");
#endif

	return 0;
}

/* if called in a primary process, just spawns off a secondary process to
 * run validation tests - which brings us right back here again...
 * if called in a secondary process, this runs a series of API tests to check
 * how things run in a secondary instance.
 */
int
test_mp_secondary(void)
{
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		return run_secondary_instances();
	}

	printf("IN SECONDARY PROCESS\n");

	return run_object_creation_tests();
}

#endif /* !RTE_EXEC_ENV_WINDOWS */

REGISTER_FAST_TEST(multiprocess_autotest, false, false, test_mp_secondary);
