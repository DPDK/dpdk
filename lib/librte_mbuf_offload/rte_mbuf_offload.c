/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015 Intel Corporation. All rights reserved.
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

#include <string.h>
#include <rte_common.h>

#include "rte_mbuf_offload.h"

/** Initialize rte_mbuf_offload structure */
static void
rte_pktmbuf_offload_init(struct rte_mempool *mp,
		__rte_unused void *opaque_arg,
		void *_op_data,
		__rte_unused unsigned i)
{
	struct rte_mbuf_offload *ol = _op_data;

	memset(_op_data, 0, mp->elt_size);

	ol->type = RTE_PKTMBUF_OL_NOT_SPECIFIED;
	ol->mp = mp;
}


struct rte_mempool *
rte_pktmbuf_offload_pool_create(const char *name, unsigned size,
		unsigned cache_size, uint16_t priv_size, int socket_id)
{
	struct rte_pktmbuf_offload_pool_private *priv;
	unsigned elt_size = sizeof(struct rte_mbuf_offload) + priv_size;


	/* lookup mempool in case already allocated */
	struct rte_mempool *mp = rte_mempool_lookup(name);

	if (mp != NULL) {
		priv = (struct rte_pktmbuf_offload_pool_private *)
				rte_mempool_get_priv(mp);

		if (priv->offload_priv_size <  priv_size ||
				mp->elt_size != elt_size ||
				mp->cache_size < cache_size ||
				mp->size < size) {
			mp = NULL;
			return NULL;
		}
		return mp;
	}

	mp = rte_mempool_create(
			name,
			size,
			elt_size,
			cache_size,
			sizeof(struct rte_pktmbuf_offload_pool_private),
			NULL,
			NULL,
			rte_pktmbuf_offload_init,
			NULL,
			socket_id,
			0);

	if (mp == NULL)
		return NULL;

	priv = (struct rte_pktmbuf_offload_pool_private *)
			rte_mempool_get_priv(mp);

	priv->offload_priv_size = priv_size;
	return mp;
}
