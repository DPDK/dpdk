/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_rwlock.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_cycles.h>

#include "test.h"

/*
 * rwlock test
 * ===========
 *
 * - There is a global rwlock and a table of rwlocks (one per lcore).
 *
 * - The test function takes all of these locks and launches the
 *   ``test_rwlock_per_core()`` function on each core (except the master).
 *
 *   - The function takes the global write lock, display something,
 *     then releases the global lock.
 *   - Then, it takes the per-lcore write lock, display something, and
 *     releases the per-core lock.
 *   - Finally, a read lock is taken during 100 ms, then released.
 *
 * - The main function unlocks the per-lcore locks sequentially and
 *   waits between each lock. This triggers the display of a message
 *   for each core, in the correct order.
 *
 *   Then, it tries to take the global write lock and display the last
 *   message. The autotest script checks that the message order is correct.
 */

static rte_rwlock_t sl;
static rte_rwlock_t sl_tab[RTE_MAX_LCORE];

static int
test_rwlock_per_core(__attribute__((unused)) void *arg)
{
	rte_rwlock_write_lock(&sl);
	printf("Global write lock taken on core %u\n", rte_lcore_id());
	rte_rwlock_write_unlock(&sl);

	rte_rwlock_write_lock(&sl_tab[rte_lcore_id()]);
	printf("Hello from core %u !\n", rte_lcore_id());
	rte_rwlock_write_unlock(&sl_tab[rte_lcore_id()]);

	rte_rwlock_read_lock(&sl);
	printf("Global read lock taken on core %u\n", rte_lcore_id());
	rte_delay_ms(100);
	printf("Release global read lock on core %u\n", rte_lcore_id());
	rte_rwlock_read_unlock(&sl);

	return 0;
}

static int
test_rwlock(void)
{
	int i;

	rte_rwlock_init(&sl);
	for (i=0; i<RTE_MAX_LCORE; i++)
		rte_rwlock_init(&sl_tab[i]);

	rte_rwlock_write_lock(&sl);

	RTE_LCORE_FOREACH_SLAVE(i) {
		rte_rwlock_write_lock(&sl_tab[i]);
		rte_eal_remote_launch(test_rwlock_per_core, NULL, i);
	}

	rte_rwlock_write_unlock(&sl);

	RTE_LCORE_FOREACH_SLAVE(i) {
		rte_rwlock_write_unlock(&sl_tab[i]);
		rte_delay_ms(100);
	}

	rte_rwlock_write_lock(&sl);
	/* this message should be the last message of test */
	printf("Global write lock taken on master core %u\n", rte_lcore_id());
	rte_rwlock_write_unlock(&sl);

	rte_eal_mp_wait_lcore();

	return 0;
}

REGISTER_TEST_COMMAND(rwlock_autotest, test_rwlock);
