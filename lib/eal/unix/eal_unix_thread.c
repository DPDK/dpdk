/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Red Hat, Inc.
 */

#include <errno.h>
#include <unistd.h>

#include <rte_debug.h>

#include "eal_private.h"

int
eal_thread_wake_worker(unsigned int worker_id)
{
	int m2w = lcore_config[worker_id].pipe_main2worker[1];
	int w2m = lcore_config[worker_id].pipe_worker2main[0];
	char c = 0;
	int n;

	do {
		n = write(m2w, &c, 1);
	} while (n == 0 || (n < 0 && errno == EINTR));
	if (n < 0)
		return -EPIPE;

	do {
		n = read(w2m, &c, 1);
	} while (n < 0 && errno == EINTR);
	if (n <= 0)
		return -EPIPE;
	return 0;
}

void
eal_thread_wait_command(void)
{
	unsigned int lcore_id = rte_lcore_id();
	int m2w;
	char c;
	int n;

	m2w = lcore_config[lcore_id].pipe_main2worker[0];
	do {
		n = read(m2w, &c, 1);
	} while (n < 0 && errno == EINTR);
	if (n <= 0)
		rte_panic("cannot read on configuration pipe\n");
}

void
eal_thread_ack_command(void)
{
	unsigned int lcore_id = rte_lcore_id();
	char c = 0;
	int w2m;
	int n;

	w2m = lcore_config[lcore_id].pipe_worker2main[1];
	do {
		n = write(w2m, &c, 1);
	} while (n == 0 || (n < 0 && errno == EINTR));
	if (n < 0)
		rte_panic("cannot write on configuration pipe\n");
}
