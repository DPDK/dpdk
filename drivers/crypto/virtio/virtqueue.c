/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 HUAWEI TECHNOLOGIES CO., LTD.
 */

#include <stdint.h>

#include <rte_mbuf.h>
#include <rte_crypto.h>
#include <rte_malloc.h>

#include "virtqueue.h"

void
virtqueue_disable_intr(struct virtqueue *vq)
{
	/*
	 * Set VRING_AVAIL_F_NO_INTERRUPT to hint host
	 * not to interrupt when it consumes packets
	 * Note: this is only considered a hint to the host
	 */
	vq->vq_ring.avail->flags |= VRING_AVAIL_F_NO_INTERRUPT;
}

void
virtqueue_detatch_unused(struct virtqueue *vq)
{
	struct rte_crypto_op *cop = NULL;

	int idx;

	if (vq != NULL)
		for (idx = 0; idx < vq->vq_nentries; idx++) {
			cop = vq->vq_descx[idx].crypto_op;
			if (cop) {
				if (cop->sym->m_src)
					rte_pktmbuf_free(cop->sym->m_src);
				if (cop->sym->m_dst)
					rte_pktmbuf_free(cop->sym->m_dst);
				rte_crypto_op_free(cop);
				vq->vq_descx[idx].crypto_op = NULL;
			}
		}
}
