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

#include <rte_mempool.h>

#include "test.h"

#define MAX_BULK 16
#define MEMPOOL_ELT_SIZE 2048
#define MEMPOOL_SIZE 2047

/*
 *      ^
 *     / \
 *    / | \     WARNING: this test program does *not* show how to use the
 *   /  .  \    API. Its only goal is to check dependencies of include files.
 *  /_______\
 */

int
test_mempool(void)
{
	struct rte_mempool *mp;
	void *ptrs[MAX_BULK];
	int x;
	phys_addr_t addr;

	mp = rte_mempool_create("test_nocache", MEMPOOL_SIZE,
				MEMPOOL_ELT_SIZE, 0, 0,
				(void (*)(struct rte_mempool*, void*)) 0,
				(void *)0,
				(void (*)(struct rte_mempool*, void*, void*, unsigned int)) 0,
				(void *)0,
				SOCKET_ID_ANY, 0);

	if (mp == NULL) {
		return -1;
	}

	rte_mempool_set_bulk_count(mp, MAX_BULK);
	rte_mempool_dump(mp);

	rte_mempool_mc_get_bulk(mp, ptrs, 1);
	rte_mempool_mc_get_bulk(mp, ptrs, MAX_BULK);
	rte_mempool_sc_get_bulk(mp, ptrs, 1);
	rte_mempool_sc_get_bulk(mp, ptrs, MAX_BULK);
	rte_mempool_get_bulk(mp, ptrs, 1);
	rte_mempool_get_bulk(mp, ptrs, MAX_BULK);
	rte_mempool_mc_get(mp, ptrs);
	rte_mempool_sc_get(mp, ptrs);
	rte_mempool_get(mp, ptrs);

	rte_mempool_mp_put_bulk(mp, ptrs, 1);
	rte_mempool_mp_put_bulk(mp, ptrs, MAX_BULK);
	rte_mempool_sp_put_bulk(mp, ptrs, 1);
	rte_mempool_sp_put_bulk(mp, ptrs, MAX_BULK);
	rte_mempool_put_bulk(mp, ptrs, 1);
	rte_mempool_put_bulk(mp, ptrs, MAX_BULK);
	rte_mempool_mp_put(mp, ptrs);
	rte_mempool_sp_put(mp, ptrs);
	rte_mempool_put(mp, ptrs);

	__MEMPOOL_STAT_ADD(mp, put, 1);
	__mempool_check_cookies(mp, 0, 0, 0);

	x = rte_mempool_count(mp);
	x = rte_mempool_free_count(mp);
	x = rte_mempool_full(mp);
	x = rte_mempool_empty(mp);

	addr = rte_mempool_virt2phy(mp, ptrs[0]);
	rte_mempool_audit(mp);
	ptrs[0] = rte_mempool_get_priv(mp);

	(void)x;
	(void)addr;

	return 0;
}
