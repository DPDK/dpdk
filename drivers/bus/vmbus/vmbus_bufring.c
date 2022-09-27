/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2009-2012,2016 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2012 Citrix Inc.
 * All rights reserved.
 */

#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/uio.h>

#include <rte_eal.h>
#include <rte_tailq.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_atomic.h>
#include <rte_memory.h>
#include <rte_pause.h>
#include <rte_bus_vmbus.h>

#include "private.h"

/* Increase bufring index by inc with wraparound */
static inline uint32_t vmbus_br_idxinc(uint32_t idx, uint32_t inc, uint32_t sz)
{
	idx += inc;
	if (idx >= sz)
		idx -= sz;

	return idx;
}

void vmbus_br_setup(struct vmbus_br *br, void *buf, unsigned int blen)
{
	br->vbr = buf;
	br->windex = br->vbr->windex;
	br->dsize = blen - sizeof(struct vmbus_bufring);
}

/*
 * When we write to the ring buffer, check if the host needs to be
 * signaled.
 *
 * The contract:
 * - The host guarantees that while it is draining the TX bufring,
 *   it will set the br_imask to indicate it does not need to be
 *   interrupted when new data are added.
 * - The host guarantees that it will completely drain the TX bufring
 *   before exiting the read loop.  Further, once the TX bufring is
 *   empty, it will clear the br_imask and re-check to see if new
 *   data have arrived.
 */
static inline bool
vmbus_txbr_need_signal(const struct vmbus_bufring *vbr, uint32_t old_windex)
{
	rte_smp_mb();
	if (vbr->imask)
		return false;

	rte_smp_rmb();

	/*
	 * This is the only case we need to signal when the
	 * ring transitions from being empty to non-empty.
	 */
	return old_windex == vbr->rindex;
}

static inline uint32_t
vmbus_txbr_copyto(const struct vmbus_br *tbr, uint32_t windex,
		  const void *src0, uint32_t cplen)
{
	uint8_t *br_data = tbr->vbr->data;
	uint32_t br_dsize = tbr->dsize;
	const uint8_t *src = src0;

	/* XXX use double mapping like Linux kernel? */
	if (cplen > br_dsize - windex) {
		uint32_t fraglen = br_dsize - windex;

		/* Wrap-around detected */
		memcpy(br_data + windex, src, fraglen);
		memcpy(br_data, src + fraglen, cplen - fraglen);
	} else {
		memcpy(br_data + windex, src, cplen);
	}

	return vmbus_br_idxinc(windex, cplen, br_dsize);
}

/*
 * Write scattered channel packet to TX bufring.
 *
 * The offset of this channel packet is written as a 64bits value
 * immediately after this channel packet.
 *
 * The write goes through three stages:
 *  1. Reserve space in ring buffer for the new data.
 *     Writer atomically moves priv_write_index.
 *  2. Copy the new data into the ring.
 *  3. Update the tail of the ring (visible to host) that indicates
 *     next read location. Writer updates write_index
 */
int
vmbus_txbr_write(struct vmbus_br *tbr, const struct iovec iov[], int iovlen,
		 bool *need_sig)
{
	struct vmbus_bufring *vbr = tbr->vbr;
	uint32_t ring_size = tbr->dsize;
	uint32_t old_windex, next_windex, windex, total;
	uint64_t save_windex;
	int i;

	total = 0;
	for (i = 0; i < iovlen; i++)
		total += iov[i].iov_len;
	total += sizeof(save_windex);

	/* Reserve space in ring */
	do {
		uint32_t avail;

		/* Get current free location */
		old_windex = tbr->windex;

		/* Prevent compiler reordering this with calculation */
		rte_compiler_barrier();

		avail = vmbus_br_availwrite(tbr, old_windex);

		/* If not enough space in ring, then tell caller. */
		if (avail <= total)
			return -EAGAIN;

		next_windex = vmbus_br_idxinc(old_windex, total, ring_size);

		/* Atomic update of next write_index for other threads */
	} while (!rte_atomic32_cmpset(&tbr->windex, old_windex, next_windex));

	/* Space from old..new is now reserved */
	windex = old_windex;
	for (i = 0; i < iovlen; i++) {
		windex = vmbus_txbr_copyto(tbr, windex,
					   iov[i].iov_base, iov[i].iov_len);
	}

	/* Set the offset of the current channel packet. */
	save_windex = ((uint64_t)old_windex) << 32;
	windex = vmbus_txbr_copyto(tbr, windex, &save_windex,
				   sizeof(save_windex));

	/* The region reserved should match region used */
	RTE_ASSERT(windex == next_windex);

	/* Ensure that data is available before updating host index */
	rte_smp_wmb();

	/* Checkin for our reservation. wait for our turn to update host */
	while (!rte_atomic32_cmpset(&vbr->windex, old_windex, next_windex))
		rte_pause();

	/* If host had read all data before this, then need to signal */
	*need_sig |= vmbus_txbr_need_signal(vbr, old_windex);
	return 0;
}

static inline uint32_t
vmbus_rxbr_copyfrom(const struct vmbus_br *rbr, uint32_t rindex,
		    void *dst0, size_t cplen)
{
	const uint8_t *br_data = rbr->vbr->data;
	uint32_t br_dsize = rbr->dsize;
	uint8_t *dst = dst0;

	if (cplen > br_dsize - rindex) {
		uint32_t fraglen = br_dsize - rindex;

		/* Wrap-around detected. */
		memcpy(dst, br_data + rindex, fraglen);
		memcpy(dst + fraglen, br_data, cplen - fraglen);
	} else {
		memcpy(dst, br_data + rindex, cplen);
	}

	return vmbus_br_idxinc(rindex, cplen, br_dsize);
}

/* Copy data from receive ring but don't change index */
int
vmbus_rxbr_peek(const struct vmbus_br *rbr, void *data, size_t dlen)
{
	uint32_t avail;

	/*
	 * The requested data and the 64bits channel packet
	 * offset should be there at least.
	 */
	avail = vmbus_br_availread(rbr);
	if (avail < dlen + sizeof(uint64_t))
		return -EAGAIN;

	vmbus_rxbr_copyfrom(rbr, rbr->vbr->rindex, data, dlen);
	return 0;
}

/*
 * Copy data from receive ring and change index
 * NOTE:
 * We assume (dlen + skip) == sizeof(channel packet).
 */
int
vmbus_rxbr_read(struct vmbus_br *rbr, void *data, size_t dlen, size_t skip)
{
	struct vmbus_bufring *vbr = rbr->vbr;
	uint32_t br_dsize = rbr->dsize;
	uint32_t rindex;

	if (vmbus_br_availread(rbr) < dlen + skip + sizeof(uint64_t))
		return -EAGAIN;

	/* Record where host was when we started read (for debug) */
	rbr->windex = rbr->vbr->windex;

	/*
	 * Copy channel packet from RX bufring.
	 */
	rindex = vmbus_br_idxinc(rbr->vbr->rindex, skip, br_dsize);
	rindex = vmbus_rxbr_copyfrom(rbr, rindex, data, dlen);

	/*
	 * Discard this channel packet's 64bits offset, which is useless to us.
	 */
	rindex = vmbus_br_idxinc(rindex, sizeof(uint64_t), br_dsize);

	/* Update the read index _after_ the channel packet is fetched.	 */
	rte_compiler_barrier();

	vbr->rindex = rindex;

	return 0;
}
