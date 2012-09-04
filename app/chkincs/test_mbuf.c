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
 *  version: DPDK.L.1.2.3-3
 */


#include <rte_mbuf.h>

#include "test.h"

#define MBUF_SIZE 2048
#define NB_MBUF   128

/*
 *      ^
 *     / \
 *    / | \     WARNING: this test program does *not* show how to use the
 *   /  .  \    API. Its only goal is to check dependencies of include files.
 *  /_______\
 */

int
test_mbuf(void)
{
	struct rte_mempool *mbuf_pool;
	struct rte_mbuf *m, *m1;
	char *hdr;
	int x;
	int* ptr;

	mbuf_pool = rte_mempool_create("test_mbuf_pool", NB_MBUF,
				       MBUF_SIZE, 32, 0,
				       (void (*)(struct rte_mempool*, void*)) 0, (void *)0,
				       rte_pktmbuf_init, (void *)0,
				       SOCKET_ID_ANY, 0);
	if (mbuf_pool == NULL) {
		return -1;
	}

	m = rte_pktmbuf_alloc(mbuf_pool);
	if(m == NULL) {
		return -1;
	}

	m1 = RTE_MBUF_FROM_BADDR(RTE_MBUF_TO_BADDR(m));
	(void)m1;

	x = rte_pktmbuf_pkt_len(m);
	x = rte_pktmbuf_data_len(m);
	x = rte_pktmbuf_headroom(m);
	x = rte_pktmbuf_tailroom(m);
	x = rte_pktmbuf_is_contiguous(m);

	m = rte_pktmbuf_lastseg(m);

	hdr = rte_pktmbuf_mtod(m, char *);
	rte_pktmbuf_dump(m, 0);

	hdr = rte_pktmbuf_append(m, 10);
	x = rte_pktmbuf_trim(m, 10);
	hdr = rte_pktmbuf_prepend(m, 10);
	hdr = rte_pktmbuf_adj(m, 10);

	ptr = (int*) rte_ctrlmbuf_data(m);
	*ptr = rte_ctrlmbuf_len(m);
	*ptr = rte_pktmbuf_pkt_len(m);
	*ptr = rte_pktmbuf_data_len(m);

	rte_pktmbuf_free_seg(m);
	rte_pktmbuf_free(m);

	RTE_MBUF_PREFETCH_TO_FREE(m);

	rte_mbuf_sanity_check(m, RTE_MBUF_CTRL, 1);

	(void)x;
	(void)hdr;

	return 0;
}
