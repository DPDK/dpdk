/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _OTX2_EP_ENQDEQ_H_
#define _OTX2_EP_ENQDEQ_H_

#include <rte_byteorder.h>
#include "otx2_ep_rawdev.h"

#define SDP_IQ_SEND_FAILED      (-1)
#define SDP_IQ_SEND_SUCCESS     (0)

#define SDP_OQ_RECV_FAILED      (-1)
#define SDP_OQ_RECV_SUCCESS     (0)

static inline uint64_t
sdp_endian_swap_8B(uint64_t _d)
{
	return ((((((uint64_t)(_d)) >>  0) & (uint64_t)0xff) << 56) |
		(((((uint64_t)(_d)) >>  8) & (uint64_t)0xff) << 48) |
		(((((uint64_t)(_d)) >> 16) & (uint64_t)0xff) << 40) |
		(((((uint64_t)(_d)) >> 24) & (uint64_t)0xff) << 32) |
		(((((uint64_t)(_d)) >> 32) & (uint64_t)0xff) << 24) |
		(((((uint64_t)(_d)) >> 40) & (uint64_t)0xff) << 16) |
		(((((uint64_t)(_d)) >> 48) & (uint64_t)0xff) <<  8) |
		(((((uint64_t)(_d)) >> 56) & (uint64_t)0xff) <<  0));
}

static inline void
sdp_swap_8B_data(uint64_t *data, uint32_t blocks)
{
	/* Swap 8B blocks */
	while (blocks) {
		*data = sdp_endian_swap_8B(*data);
		blocks--;
		data++;
	}
}

static inline uint32_t
sdp_incr_index(uint32_t index, uint32_t count, uint32_t max)
{
	if ((index + count) >= max)
		index = index + count - max;
	else
		index += count;

	return index;
}

#endif /* _OTX2_EP_ENQDEQ_H_ */
