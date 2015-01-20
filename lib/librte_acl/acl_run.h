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

#ifndef	_ACL_RUN_H_
#define	_ACL_RUN_H_

#include <rte_acl.h>
#include "acl.h"

#define MAX_SEARCHES_AVX16	16
#define MAX_SEARCHES_SSE8	8
#define MAX_SEARCHES_SSE4	4
#define MAX_SEARCHES_SCALAR	2

#define GET_NEXT_4BYTES(prm, idx)	\
	(*((const int32_t *)((prm)[(idx)].data + *(prm)[idx].data_index++)))


#define RTE_ACL_NODE_INDEX	((uint32_t)~RTE_ACL_NODE_TYPE)

#define	SCALAR_QRANGE_MULT	0x01010101
#define	SCALAR_QRANGE_MASK	0x7f7f7f7f
#define	SCALAR_QRANGE_MIN	0x80808080

/*
 * Structure to manage N parallel trie traversals.
 * The runtime trie traversal routines can process 8, 4, or 2 tries
 * in parallel. Each packet may require multiple trie traversals (up to 4).
 * This structure is used to fill the slots (0 to n-1) for parallel processing
 * with the trie traversals needed for each packet.
 */
struct acl_flow_data {
	uint32_t            num_packets;
	/* number of packets processed */
	uint32_t            started;
	/* number of trie traversals in progress */
	uint32_t            trie;
	/* current trie index (0 to N-1) */
	uint32_t            cmplt_size;
	uint32_t            total_packets;
	uint32_t            categories;
	/* number of result categories per packet. */
	/* maximum number of packets to process */
	const uint64_t     *trans;
	const uint8_t     **data;
	uint32_t           *results;
	struct completion  *last_cmplt;
	struct completion  *cmplt_array;
};

/*
 * Structure to maintain running results for
 * a single packet (up to 4 tries).
 */
struct completion {
	uint32_t *results;                          /* running results. */
	int32_t   priority[RTE_ACL_MAX_CATEGORIES]; /* running priorities. */
	uint32_t  count;                            /* num of remaining tries */
	/* true for allocated struct */
} __attribute__((aligned(XMM_SIZE)));

/*
 * One parms structure for each slot in the search engine.
 */
struct parms {
	const uint8_t              *data;
	/* input data for this packet */
	const uint32_t             *data_index;
	/* data indirection for this trie */
	struct completion          *cmplt;
	/* completion data for this packet */
};

/*
 * Define an global idle node for unused engine slots
 */
static const uint32_t idle[UINT8_MAX + 1];

/*
 * Allocate a completion structure to manage the tries for a packet.
 */
static inline struct completion *
alloc_completion(struct completion *p, uint32_t size, uint32_t tries,
	uint32_t *results)
{
	uint32_t n;

	for (n = 0; n < size; n++) {

		if (p[n].count == 0) {

			/* mark as allocated and set number of tries. */
			p[n].count = tries;
			p[n].results = results;
			return &(p[n]);
		}
	}

	/* should never get here */
	return NULL;
}

/*
 * Resolve priority for a single result trie.
 */
static inline void
resolve_single_priority(uint64_t transition, int n,
	const struct rte_acl_ctx *ctx, struct parms *parms,
	const struct rte_acl_match_results *p)
{
	if (parms[n].cmplt->count == ctx->num_tries ||
			parms[n].cmplt->priority[0] <=
			p[transition].priority[0]) {

		parms[n].cmplt->priority[0] = p[transition].priority[0];
		parms[n].cmplt->results[0] = p[transition].results[0];
	}
}

/*
 * Routine to fill a slot in the parallel trie traversal array (parms) from
 * the list of packets (flows).
 */
static inline uint64_t
acl_start_next_trie(struct acl_flow_data *flows, struct parms *parms, int n,
	const struct rte_acl_ctx *ctx)
{
	uint64_t transition;

	/* if there are any more packets to process */
	if (flows->num_packets < flows->total_packets) {
		parms[n].data = flows->data[flows->num_packets];
		parms[n].data_index = ctx->trie[flows->trie].data_index;

		/* if this is the first trie for this packet */
		if (flows->trie == 0) {
			flows->last_cmplt = alloc_completion(flows->cmplt_array,
				flows->cmplt_size, ctx->num_tries,
				flows->results +
				flows->num_packets * flows->categories);
		}

		/* set completion parameters and starting index for this slot */
		parms[n].cmplt = flows->last_cmplt;
		transition =
			flows->trans[parms[n].data[*parms[n].data_index++] +
			ctx->trie[flows->trie].root_index];

		/*
		 * if this is the last trie for this packet,
		 * then setup next packet.
		 */
		flows->trie++;
		if (flows->trie >= ctx->num_tries) {
			flows->trie = 0;
			flows->num_packets++;
		}

		/* keep track of number of active trie traversals */
		flows->started++;

	/* no more tries to process, set slot to an idle position */
	} else {
		transition = ctx->idle;
		parms[n].data = (const uint8_t *)idle;
		parms[n].data_index = idle;
	}
	return transition;
}

static inline void
acl_set_flow(struct acl_flow_data *flows, struct completion *cmplt,
	uint32_t cmplt_size, const uint8_t **data, uint32_t *results,
	uint32_t data_num, uint32_t categories, const uint64_t *trans)
{
	flows->num_packets = 0;
	flows->started = 0;
	flows->trie = 0;
	flows->last_cmplt = NULL;
	flows->cmplt_array = cmplt;
	flows->total_packets = data_num;
	flows->categories = categories;
	flows->cmplt_size = cmplt_size;
	flows->data = data;
	flows->results = results;
	flows->trans = trans;
}

typedef void (*resolve_priority_t)
(uint64_t transition, int n, const struct rte_acl_ctx *ctx,
	struct parms *parms, const struct rte_acl_match_results *p,
	uint32_t categories);

/*
 * Detect matches. If a match node transition is found, then this trie
 * traversal is complete and fill the slot with the next trie
 * to be processed.
 */
static inline uint64_t
acl_match_check(uint64_t transition, int slot,
	const struct rte_acl_ctx *ctx, struct parms *parms,
	struct acl_flow_data *flows, resolve_priority_t resolve_priority)
{
	const struct rte_acl_match_results *p;

	p = (const struct rte_acl_match_results *)
		(flows->trans + ctx->match_index);

	if (transition & RTE_ACL_NODE_MATCH) {

		/* Remove flags from index and decrement active traversals */
		transition &= RTE_ACL_NODE_INDEX;
		flows->started--;

		/* Resolve priorities for this trie and running results */
		if (flows->categories == 1)
			resolve_single_priority(transition, slot, ctx,
				parms, p);
		else
			resolve_priority(transition, slot, ctx, parms,
				p, flows->categories);

		/* Count down completed tries for this search request */
		parms[slot].cmplt->count--;

		/* Fill the slot with the next trie or idle trie */
		transition = acl_start_next_trie(flows, parms, slot, ctx);
	}

	return transition;
}

#endif /* _ACL_RUN_H_ */
