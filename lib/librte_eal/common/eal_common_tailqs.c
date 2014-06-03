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

#include <sys/queue.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>

#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_memory.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_log.h>
#include <rte_string_fns.h>

#include "eal_private.h"

/**
 * Name of tailq_head
 */
const char* rte_tailq_names[RTE_MAX_TAILQ] = {
#define rte_tailq_elem(idx, name)     name,
#include <rte_tailq_elem.h>
};

struct rte_tailq_head *
rte_eal_tailq_lookup(const char *name)
{
	unsigned i;
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;

	if (name == NULL)
		return NULL;

	for (i = 0; i < RTE_MAX_TAILQ; i++) {
		if (rte_tailq_names[i] == NULL)
			continue;
		if (!strncmp(name, rte_tailq_names[i], RTE_TAILQ_NAMESIZE-1))
			return &mcfg->tailq_head[i];
	}

	return NULL;
}

inline struct rte_tailq_head *
rte_eal_tailq_lookup_by_idx(const unsigned tailq_idx)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;

	if (tailq_idx >= RTE_MAX_TAILQ) {
		RTE_LOG(ERR, EAL, "%s(): No more room in config\n", __func__);
		return NULL;
	}

	return &mcfg->tailq_head[tailq_idx];
}

struct rte_tailq_head *
rte_eal_tailq_reserve(const char *name)
{
	return rte_eal_tailq_lookup(name);
}

inline struct rte_tailq_head *
rte_eal_tailq_reserve_by_idx(const unsigned tailq_idx)
{
	return rte_eal_tailq_lookup_by_idx(tailq_idx);
}

void
rte_dump_tailq(FILE *f)
{
	struct rte_mem_config *mcfg;
	unsigned i = 0;

	mcfg = rte_eal_get_configuration()->mem_config;

	rte_rwlock_read_lock(&mcfg->qlock);
	for (i=0; i < RTE_MAX_TAILQ; i++) {
		const struct rte_tailq_head *tailq = &mcfg->tailq_head[i];
		const struct rte_dummy_head *head = &tailq->tailq_head;

		fprintf(f, "Tailq %u: qname:<%s>, tqh_first:%p, tqh_last:%p\n", i,
		       (rte_tailq_names[i] != NULL ? rte_tailq_names[i]:"nil"),
		       head->tqh_first, head->tqh_last);
	}
	rte_rwlock_read_unlock(&mcfg->qlock);
}

int
rte_eal_tailqs_init(void)
{
	unsigned i;
	struct rte_mem_config *mcfg = NULL;

	RTE_BUILD_BUG_ON(RTE_MAX_TAILQ < RTE_TAILQ_NUM);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		mcfg = rte_eal_get_configuration()->mem_config;
		for (i = 0; i < RTE_MAX_TAILQ; i++)
			TAILQ_INIT(&mcfg->tailq_head[i].tailq_head);
	}

	return 0;
}

