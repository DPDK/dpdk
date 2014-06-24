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
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_atomic.h>
#include <rte_launch.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_errno.h>
#include <rte_string_fns.h>
#include <rte_spinlock.h>

#include "rte_mempool.h"

static void
get_phys_map(void *va, phys_addr_t pa[], uint32_t pg_num,
            uint32_t pg_sz, uint32_t memseg_id)
{
    uint32_t i;
    uint64_t virt_addr, mfn_id;
    struct rte_mem_config *mcfg;
    uint32_t page_size = getpagesize();

    /* get pointer to global configuration */
    mcfg = rte_eal_get_configuration()->mem_config;
    virt_addr =(uintptr_t) mcfg->memseg[memseg_id].addr;

    for (i = 0; i != pg_num; i++) {
        mfn_id = ((uintptr_t)va + i * pg_sz - virt_addr) / RTE_PGSIZE_2M;
        pa[i] = mcfg->memseg[memseg_id].mfn[mfn_id] * page_size;
    }
}

/* create the mempool for supporting Dom0 */
struct rte_mempool *
rte_dom0_mempool_create(const char *name, unsigned elt_num, unsigned elt_size,
           unsigned cache_size, unsigned private_data_size,
           rte_mempool_ctor_t *mp_init, void *mp_init_arg,
           rte_mempool_obj_ctor_t *obj_init, void *obj_init_arg,
           int socket_id, unsigned flags)
{
	struct rte_mempool *mp = NULL;
	phys_addr_t *pa;
	char *va;
	size_t sz;
	uint32_t pg_num, pg_shift, pg_sz, total_size;
	const struct rte_memzone *mz;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	int mz_flags = RTE_MEMZONE_1GB|RTE_MEMZONE_SIZE_HINT_ONLY;

	pg_sz = RTE_PGSIZE_2M;

	pg_shift = rte_bsf32(pg_sz);
	total_size = rte_mempool_calc_obj_size(elt_size, flags, NULL);

	/* calc max memory size and max number of pages needed. */
	sz = rte_mempool_xmem_size(elt_num, total_size, pg_shift) +
		RTE_PGSIZE_2M;
	pg_num = sz >> pg_shift;

	/* extract physical mappings of the allocated memory. */
	pa = calloc(pg_num, sizeof (*pa));
	if (pa == NULL)
		return mp;

	snprintf(mz_name, sizeof(mz_name), RTE_MEMPOOL_OBJ_NAME, name);
	mz = rte_memzone_reserve(mz_name, sz, socket_id, mz_flags);
	if (mz == NULL) {
		free(pa);
		return mp;
	}

	va = (char *)RTE_ALIGN_CEIL((uintptr_t)mz->addr, RTE_PGSIZE_2M);
	/* extract physical mappings of the allocated memory. */
	get_phys_map(va, pa, pg_num, pg_sz, mz->memseg_id);

	mp = rte_mempool_xmem_create(name, elt_num, elt_size,
		cache_size, private_data_size,
		mp_init, mp_init_arg,
		obj_init, obj_init_arg,
		socket_id, flags, va, pa, pg_num, pg_shift);

	free(pa);

	return (mp);
}
