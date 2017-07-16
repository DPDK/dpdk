/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
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
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>

#include <rte_keepalive.h>

#include <shm.h>

#define MAX_TIMEOUTS 4
#define SEM_TIMEOUT_SECS 2

static struct rte_keepalive_shm *ka_shm_create(void)
{
	int fd = shm_open(RTE_KEEPALIVE_SHM_NAME, O_RDWR, 0666);
	size_t size = sizeof(struct rte_keepalive_shm);
	struct rte_keepalive_shm *shm;

	if (fd < 0)
		printf("Failed to open %s as SHM:%s\n",
			RTE_KEEPALIVE_SHM_NAME,
		strerror(errno));
	else {
		shm = (struct rte_keepalive_shm *) mmap(
			0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		close(fd);
		if (shm == MAP_FAILED)
			printf("Failed to mmap SHM:%s\n", strerror(errno));
		else
			return shm;
	}

	/* Reset to zero, as it was set to MAP_FAILED aka: (void *)-1 */
	shm = 0;
	return NULL;
}

int main(void)
{
	struct rte_keepalive_shm *shm = ka_shm_create();
	struct timespec timeout = { .tv_nsec = 0 };
	int idx_core;
	int cnt_cores;
	uint64_t last_seen_alive_time = 0;
	uint64_t most_recent_alive_time;
	int cnt_timeouts = 0;
	int sem_errno;

	if (shm == NULL) {
		printf("Unable to access shared core state\n");
		return 1;
	}
	while (1) {
		most_recent_alive_time = 0;
		for (idx_core = 0; idx_core < RTE_KEEPALIVE_MAXCORES;
				idx_core++)
			if (shm->core_last_seen_times[idx_core] >
					most_recent_alive_time)
				most_recent_alive_time =
					shm->core_last_seen_times[idx_core];

		timeout.tv_sec = time(NULL) + SEM_TIMEOUT_SECS;
		if (sem_timedwait(&shm->core_died, &timeout) == -1) {
			/* Assume no core death signals and no change in any
			 * last-seen times is the keepalive monitor itself
			 * failing.
			 */
			sem_errno = errno;
			last_seen_alive_time = most_recent_alive_time;
			if (sem_errno == ETIMEDOUT) {
				if (last_seen_alive_time ==
						most_recent_alive_time &&
						cnt_timeouts++ >
						MAX_TIMEOUTS) {
					printf("No updates. Exiting..\n");
					break;
					}
			} else
				printf("sem_timedwait() error (%s)\n",
					strerror(sem_errno));
			continue;
		}
		cnt_timeouts = 0;

		cnt_cores = 0;
		for (idx_core = 0; idx_core < RTE_KEEPALIVE_MAXCORES;
				idx_core++)
			if (shm->core_state[idx_core] == RTE_KA_STATE_DEAD)
				cnt_cores++;
		if (cnt_cores == 0) {
			/* Can happen if core was restarted since Semaphore
			 * was sent, due to agent being offline.
			 */
			printf("Warning: Empty dead core report\n");
			continue;
		}

		printf("%i dead cores: ", cnt_cores);
		for (idx_core = 0;
				idx_core < RTE_KEEPALIVE_MAXCORES;
				idx_core++)
			if (shm->core_state[idx_core] == RTE_KA_STATE_DEAD)
				printf("%d, ", idx_core);
		printf("\b\b\n");
	}
	if (munmap(shm, sizeof(struct rte_keepalive_shm)) != 0)
		printf("Warning: munmap() failed\n");
	return 0;
}
