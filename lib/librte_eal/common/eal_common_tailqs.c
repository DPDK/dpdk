/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
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
 * 
 */

#include <sys/queue.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_memory.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_log.h>
#include <rte_string_fns.h>
#include "eal_private.h"

static unsigned tailq_idx = 0;

struct rte_tailq_head *
rte_eal_tailq_lookup(const char *name)
{
	unsigned i;
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;

	/*
	 * the algorithm is not optimal (linear), but there are few
	 * tailq's and this function should be called at init only
	 */
	for (i = 0; i < RTE_MAX_TAILQ; i++) {
		if (!strncmp(name, mcfg->tailq_head[i].qname, RTE_TAILQ_NAMESIZE-1))
			return &mcfg->tailq_head[i];
	}
	return NULL;
}

struct rte_tailq_head *
rte_eal_tailq_reserve(const char *name)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;

	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		return rte_eal_tailq_lookup(name);

	if (tailq_idx == RTE_MAX_TAILQ){
		RTE_LOG(ERR, EAL, "%s(): No more room in config\n", __func__);
		return NULL;
	}

	/* zone already exist */
	if (rte_eal_tailq_lookup(name) != NULL) {
		RTE_LOG(DEBUG, EAL, "%s(): tailq <%s> already exists\n",
			__func__, name);
		return NULL;
	}

	rte_snprintf(mcfg->tailq_head[tailq_idx].qname, RTE_TAILQ_NAMESIZE,
			"%.*s", (int)(RTE_TAILQ_NAMESIZE - 1), name);

	return &mcfg->tailq_head[tailq_idx++];
}

int
rte_eal_tailqs_init(void)
{
	unsigned i;
	struct rte_config *cfg = rte_eal_get_configuration();

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		for (i = 0; i < RTE_MAX_TAILQ; i++)
			TAILQ_INIT(&cfg->mem_config->tailq_head[i].tailq_head);

	return 0;
}
