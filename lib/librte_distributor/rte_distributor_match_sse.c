/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
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

#include <rte_mbuf.h>
#include "rte_distributor_private.h"
#include "rte_distributor.h"
#include "smmintrin.h"
#include "nmmintrin.h"


void
find_match_vec(struct rte_distributor *d,
			uint16_t *data_ptr,
			uint16_t *output_ptr)
{
	/* Setup */
	__m128i incoming_fids;
	__m128i inflight_fids;
	__m128i preflight_fids;
	__m128i wkr;
	__m128i mask1;
	__m128i mask2;
	__m128i output;
	struct rte_distributor_backlog *bl;
	uint16_t i;

	/*
	 * Function overview:
	 * 2. Loop through all worker ID's
	 *  2a. Load the current inflights for that worker into an xmm reg
	 *  2b. Load the current backlog for that worker into an xmm reg
	 *  2c. use cmpestrm to intersect flow_ids with backlog and inflights
	 *  2d. Add any matches to the output
	 * 3. Write the output xmm (matching worker ids).
	 */


	output = _mm_set1_epi16(0);
	incoming_fids = _mm_load_si128((__m128i *)data_ptr);

	for (i = 0; i < d->num_workers; i++) {
		bl = &d->backlog[i];

		inflight_fids =
			_mm_load_si128((__m128i *)&(d->in_flight_tags[i]));
		preflight_fids =
			_mm_load_si128((__m128i *)(bl->tags));

		/*
		 * Any incoming_fid that exists anywhere in inflight_fids will
		 * have 0xffff in same position of the mask as the incoming fid
		 * Example (shortened to bytes for brevity):
		 * incoming_fids   0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08
		 * inflight_fids   0x03 0x05 0x07 0x00 0x00 0x00 0x00 0x00
		 * mask            0x00 0x00 0xff 0x00 0xff 0x00 0xff 0x00
		 */

		mask1 = _mm_cmpestrm(inflight_fids, 8, incoming_fids, 8,
			_SIDD_UWORD_OPS |
			_SIDD_CMP_EQUAL_ANY |
			_SIDD_UNIT_MASK);
		mask2 = _mm_cmpestrm(preflight_fids, 8, incoming_fids, 8,
			_SIDD_UWORD_OPS |
			_SIDD_CMP_EQUAL_ANY |
			_SIDD_UNIT_MASK);

		mask1 = _mm_or_si128(mask1, mask2);
		/*
		 * Now mask contains 0xffff where there's a match.
		 * Next we need to store the worker_id in the relevant position
		 * in the output.
		 */

		wkr = _mm_set1_epi16(i+1);
		mask1 = _mm_and_si128(mask1, wkr);
		output = _mm_or_si128(mask1, output);
	}

	/*
	 * At this stage, the output 128-bit contains 8 16-bit values, with
	 * each non-zero value containing the worker ID on which the
	 * corresponding flow is pinned to.
	 */
	_mm_store_si128((__m128i *)output_ptr, output);
}
