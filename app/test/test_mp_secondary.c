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

#include "test.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <sys/queue.h>
#include <errno.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
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
#include <rte_atomic.h>
#include <rte_ring.h>
#include <rte_debug.h>
#include <rte_log.h>
#include <rte_mempool.h>

#ifdef RTE_LIBRTE_HASH
#include <rte_hash.h>
#include <rte_fbk_hash.h>
#endif /* RTE_LIBRTE_HASH */

#ifdef RTE_LIBRTE_LPM
#include <rte_lpm.h>
#endif /* RTE_LIBRTE_LPM */

#include <rte_string_fns.h>

#include "process.h"

#define launch_proc(ARGV) process_dup(ARGV, \
		sizeof(ARGV)/(sizeof(ARGV[0])), __func__)

#ifdef RTE_EXEC_ENV_LINUXAPP
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
#endif

/*
 * This function is called in the primary i.e. main test, to spawn off secondary
 * processes to run actual mp tests. Uses fork() and exec pair
 */
static int
run_secondary_instances(void)
{
	int ret = 0;
	char coremask[10];

#ifdef RTE_EXEC_ENV_LINUXAPP
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
#ifdef RTE_EXEC_ENV_LINUXAPP
	/* bad case, using invalid file prefix */
	const char *argv4[]  = {
			prgname, "-c", coremask, "--proc-type=secondary",
					"--file-prefix=ERROR"
	};
#endif

	snprintf(coremask, sizeof(coremask), "%x", \
			(1 << rte_get_master_lcore()));

	ret |= launch_proc(argv1);
	ret |= launch_proc(argv2);

	ret |= !(launch_proc(argv3));
#ifdef RTE_EXEC_ENV_LINUXAPP
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

#ifdef RTE_LIBRTE_HASH
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

#ifdef RTE_LIBRTE_LPM
	rte_errno=0;
	if ((rte_lpm_create("test_lpm", size, rte_socket_id(), 0) != NULL) &&
	    (rte_lpm_find_existing("test_lpm") == NULL)){
		printf("Error: unexpected return value from rte_lpm_create()\n");
		return -1;
	}
	printf("# Checked rte_lpm_create() OK\n");
#endif

	/* Run a test_pci call */
	if (test_pci() != 0) {
		printf("PCI scan failed in secondary\n");
		if (getuid() == 0) /* pci scans can fail as non-root */
			return -1;
	} else
		printf("PCI scan succeeded in secondary\n");

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
		if (!test_pci_run) {
			printf("=== Running pre-requisite test of test_pci\n");
			test_pci();
			printf("=== Requisite test done\n");
		}
		return run_secondary_instances();
	}

	printf("IN SECONDARY PROCESS\n");

	return run_object_creation_tests();
}

static struct test_command multiprocess_cmd = {
	.command = "multiprocess_autotest",
	.callback = test_mp_secondary,
};
REGISTER_TEST_COMMAND(multiprocess_cmd);
