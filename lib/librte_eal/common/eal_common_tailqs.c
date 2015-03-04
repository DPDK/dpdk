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
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_memory.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_log.h>
#include <rte_string_fns.h>
#include <rte_debug.h>

#include "eal_private.h"

TAILQ_HEAD(rte_tailq_elem_head, rte_tailq_elem);
/* local tailq list */
static struct rte_tailq_elem_head rte_tailq_elem_head =
	TAILQ_HEAD_INITIALIZER(rte_tailq_elem_head);

/* number of tailqs registered, -1 before call to rte_eal_tailqs_init */
static int rte_tailqs_count = -1;

struct rte_tailq_head *
rte_eal_tailq_lookup(const char *name)
{
	unsigned i;
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;

	if (name == NULL)
		return NULL;

	for (i = 0; i < RTE_MAX_TAILQ; i++) {
		if (!strncmp(name, mcfg->tailq_head[i].name,
			     RTE_TAILQ_NAMESIZE-1))
			return &mcfg->tailq_head[i];
	}

	return NULL;
}

void
rte_dump_tailq(FILE *f)
{
	struct rte_mem_config *mcfg;
	unsigned i = 0;

	mcfg = rte_eal_get_configuration()->mem_config;

	rte_rwlock_read_lock(&mcfg->qlock);
	for (i = 0; i < RTE_MAX_TAILQ; i++) {
		const struct rte_tailq_head *tailq = &mcfg->tailq_head[i];
		const struct rte_tailq_entry_head *head = &tailq->tailq_head;

		fprintf(f, "Tailq %u: qname:<%s>, tqh_first:%p, tqh_last:%p\n",
			i, tailq->name, head->tqh_first, head->tqh_last);
	}
	rte_rwlock_read_unlock(&mcfg->qlock);
}

static struct rte_tailq_head *
rte_eal_tailq_create(const char *name)
{
	struct rte_tailq_head *head = NULL;

	if (!rte_eal_tailq_lookup(name) &&
	    (rte_tailqs_count + 1 < RTE_MAX_TAILQ)) {
		struct rte_mem_config *mcfg;

		mcfg = rte_eal_get_configuration()->mem_config;
		head = &mcfg->tailq_head[rte_tailqs_count];
		snprintf(head->name, sizeof(head->name) - 1, "%s", name);
		TAILQ_INIT(&head->tailq_head);
		rte_tailqs_count++;
	}

	return head;
}

/* local register, used to store "early" tailqs before rte_eal_init() and to
 * ensure secondary process only registers tailqs once. */
static int
rte_eal_tailq_local_register(struct rte_tailq_elem *t)
{
	struct rte_tailq_elem *temp;

	TAILQ_FOREACH(temp, &rte_tailq_elem_head, next) {
		if (!strncmp(t->name, temp->name, sizeof(temp->name)))
			return -1;
	}

	TAILQ_INSERT_TAIL(&rte_tailq_elem_head, t, next);
	return 0;
}

static void
rte_eal_tailq_update(struct rte_tailq_elem *t)
{
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		/* primary process is the only one that creates */
		t->head = rte_eal_tailq_create(t->name);
	} else {
		t->head = rte_eal_tailq_lookup(t->name);
	}
}

int
rte_eal_tailq_register(struct rte_tailq_elem *t)
{
	if (rte_eal_tailq_local_register(t) < 0) {
		rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL,
			"%s tailq is already registered\n", t->name);
		goto error;
	}

	/* if a register happens after rte_eal_tailqs_init(), then we can update
	 * tailq head */
	if (rte_tailqs_count >= 0) {
		rte_eal_tailq_update(t);
		if (t->head == NULL) {
			rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL,
				"Cannot initialize tailq: %s\n", t->name);
			TAILQ_REMOVE(&rte_tailq_elem_head, t, next);
			goto error;
		}
	}

	return 0;

error:
	t->head = NULL;
	return -1;
}

int
rte_eal_tailqs_init(void)
{
	struct rte_tailq_elem *t;

	rte_tailqs_count = 0;

	TAILQ_FOREACH(t, &rte_tailq_elem_head, next) {
		/* second part of register job for "early" tailqs, see
		 * rte_eal_tailq_register and EAL_REGISTER_TAILQ */
		rte_eal_tailq_update(t);
		if (t->head == NULL) {
			rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL,
				"Cannot initialize tailq: %s\n", t->name);
			/* no need to TAILQ_REMOVE, we are going to panic in
			 * rte_eal_init() */
			goto fail;
		}
	}

	return 0;

fail:
	rte_dump_tailq(stderr);
	return -1;
}
