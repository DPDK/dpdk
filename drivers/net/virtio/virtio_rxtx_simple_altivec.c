/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 * Copyright(C) 2019 IBM Corporation
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <rte_altivec.h>
#include <rte_byteorder.h>
#include <rte_branch_prediction.h>
#include <rte_cycles.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_errno.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_prefetch.h>
#include <rte_string_fns.h>

#include "virtio_rxtx_simple.h"

#define RTE_VIRTIO_DESC_PER_LOOP 8

/* virtio vPMD receive routine, only accept(nb_pkts >= RTE_VIRTIO_DESC_PER_LOOP)
 *
 * This routine is for non-mergeable RX, one desc for each guest buffer.
 * This routine is based on the RX ring layout optimization. Each entry in the
 * avail ring points to the desc with the same index in the desc ring and this
 * will never be changed in the driver.
 *
 * - nb_pkts < RTE_VIRTIO_DESC_PER_LOOP, just return no packet
 */
uint16_t
virtio_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
	uint16_t nb_pkts)
{
	struct virtnet_rx *rxvq = rx_queue;
	struct virtqueue *vq = rxvq->vq;
	struct virtio_hw *hw = vq->hw;
	uint16_t nb_used, nb_total;
	uint16_t desc_idx;
	struct vring_used_elem *rused;
	struct rte_mbuf **sw_ring;
	struct rte_mbuf **sw_ring_end;
	struct rte_mbuf **ref_rx_pkts;
	uint16_t nb_pkts_received = 0;
	const vector unsigned char zero = {0};

	const vector unsigned char shuf_msk1 = {
		0xFF, 0xFF, 0xFF, 0xFF,	/* packet type */
		4, 5, 0xFF, 0xFF, /* vlan tci */
		4, 5,			/* dat len */
		0xFF, 0xFF,		/* vlan tci */
		0xFF, 0xFF, 0xFF, 0xFF
	};

	const vector unsigned char shuf_msk2 = {
		0xFF, 0xFF, 0xFF, 0xFF,	/* packet type */
		12, 13, 0xFF, 0xFF,	/* pkt len */
		12, 13,			/* dat len */
		0xFF, 0xFF,		/* vlan tci */
		0xFF, 0xFF, 0xFF, 0xFF
	};

	/*
	 * Subtract the header length.
	 *  In which case do we need the header length in used->len ?
	 */
	const vector unsigned short len_adjust = {
		0, 0,
		(uint16_t)-vq->hw->vtnet_hdr_size, 0,
		(uint16_t)-vq->hw->vtnet_hdr_size, 0,
		0, 0
	};

	if (unlikely(hw->started == 0))
		return nb_pkts_received;

	if (unlikely(nb_pkts < RTE_VIRTIO_DESC_PER_LOOP))
		return 0;

	nb_used = virtqueue_nused(vq);

	rte_compiler_barrier();

	if (unlikely(nb_used == 0))
		return 0;

	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, RTE_VIRTIO_DESC_PER_LOOP);
	nb_used = RTE_MIN(nb_used, nb_pkts);

	desc_idx = (uint16_t)(vq->vq_used_cons_idx & (vq->vq_nentries - 1));
	rused = &vq->vq_split.ring.used->ring[desc_idx];
	sw_ring  = &vq->sw_ring[desc_idx];
	sw_ring_end = &vq->sw_ring[vq->vq_nentries];

	rte_prefetch0(rused);

	if (vq->vq_free_cnt >= RTE_VIRTIO_VPMD_RX_REARM_THRESH) {
		virtio_rxq_rearm_vec(rxvq);
		if (unlikely(virtqueue_kick_prepare(vq)))
			virtqueue_notify(vq);
	}

	nb_total = nb_used;
	ref_rx_pkts = rx_pkts;
	for (nb_pkts_received = 0;
		nb_pkts_received < nb_total;) {
		vector unsigned char desc[RTE_VIRTIO_DESC_PER_LOOP / 2];
		vector unsigned char mbp[RTE_VIRTIO_DESC_PER_LOOP / 2];
		vector unsigned char pkt_mb[RTE_VIRTIO_DESC_PER_LOOP];

		mbp[0] = vec_vsx_ld(0, (unsigned char const *)(sw_ring + 0));
		desc[0] = vec_vsx_ld(0, (unsigned char const *)(rused + 0));
		*(vector unsigned char *)&rx_pkts[0] = mbp[0];

		mbp[1] = vec_vsx_ld(0, (unsigned char const *)(sw_ring + 2));
		desc[1] = vec_vsx_ld(0, (unsigned char const *)(rused + 2));
		*(vector unsigned char *)&rx_pkts[2] = mbp[1];

		mbp[2] = vec_vsx_ld(0, (unsigned char const *)(sw_ring + 4));
		desc[2] = vec_vsx_ld(0, (unsigned char const *)(rused + 4));
		*(vector unsigned char *)&rx_pkts[4] = mbp[2];

		mbp[3] = vec_vsx_ld(0, (unsigned char const *)(sw_ring + 6));
		desc[3] = vec_vsx_ld(0, (unsigned char const *)(rused + 6));
		*(vector unsigned char *)&rx_pkts[6] = mbp[3];

		pkt_mb[0] = vec_perm(desc[0], zero, shuf_msk1);
		pkt_mb[1] = vec_perm(desc[0], zero, shuf_msk2);
		pkt_mb[0] = (vector unsigned char)
			((vector unsigned short)pkt_mb[0] + len_adjust);
		pkt_mb[1] = (vector unsigned char)
			((vector unsigned short)pkt_mb[1] + len_adjust);
		*(vector unsigned char *)&rx_pkts[0]->rx_descriptor_fields1 =
			pkt_mb[0];
		*(vector unsigned char *)&rx_pkts[1]->rx_descriptor_fields1 =
			pkt_mb[1];

		pkt_mb[2] = vec_perm(desc[1], zero, shuf_msk1);
		pkt_mb[3] = vec_perm(desc[1], zero, shuf_msk2);
		pkt_mb[2] = (vector unsigned char)
			((vector unsigned short)pkt_mb[2] + len_adjust);
		pkt_mb[3] = (vector unsigned char)
			((vector unsigned short)pkt_mb[3] + len_adjust);
		*(vector unsigned char *)&rx_pkts[2]->rx_descriptor_fields1 =
			pkt_mb[2];
		*(vector unsigned char *)&rx_pkts[3]->rx_descriptor_fields1 =
			pkt_mb[3];

		pkt_mb[4] = vec_perm(desc[2], zero, shuf_msk1);
		pkt_mb[5] = vec_perm(desc[2], zero, shuf_msk2);
		pkt_mb[4] = (vector unsigned char)
			((vector unsigned short)pkt_mb[4] + len_adjust);
		pkt_mb[5] = (vector unsigned char)
			((vector unsigned short)pkt_mb[5] + len_adjust);
		*(vector unsigned char *)&rx_pkts[4]->rx_descriptor_fields1 =
			pkt_mb[4];
		*(vector unsigned char *)&rx_pkts[5]->rx_descriptor_fields1 =
			pkt_mb[5];

		pkt_mb[6] = vec_perm(desc[3], zero, shuf_msk1);
		pkt_mb[7] = vec_perm(desc[3], zero, shuf_msk2);
		pkt_mb[6] = (vector unsigned char)
			((vector unsigned short)pkt_mb[6] + len_adjust);
		pkt_mb[7] = (vector unsigned char)
			((vector unsigned short)pkt_mb[7] + len_adjust);
		*(vector unsigned char *)&rx_pkts[6]->rx_descriptor_fields1 =
			pkt_mb[6];
		*(vector unsigned char *)&rx_pkts[7]->rx_descriptor_fields1 =
			pkt_mb[7];

		if (unlikely(nb_used <= RTE_VIRTIO_DESC_PER_LOOP)) {
			if (sw_ring + nb_used <= sw_ring_end)
				nb_pkts_received += nb_used;
			else
				nb_pkts_received += sw_ring_end - sw_ring;
			break;
		} else {
			if (unlikely(sw_ring + RTE_VIRTIO_DESC_PER_LOOP >=
				sw_ring_end)) {
				nb_pkts_received += sw_ring_end - sw_ring;
				break;
			} else {
				nb_pkts_received += RTE_VIRTIO_DESC_PER_LOOP;

				rx_pkts += RTE_VIRTIO_DESC_PER_LOOP;
				sw_ring += RTE_VIRTIO_DESC_PER_LOOP;
				rused   += RTE_VIRTIO_DESC_PER_LOOP;
				nb_used -= RTE_VIRTIO_DESC_PER_LOOP;
			}
		}
	}

	vq->vq_used_cons_idx += nb_pkts_received;
	vq->vq_free_cnt += nb_pkts_received;
	rxvq->stats.packets += nb_pkts_received;
	for (nb_used = 0; nb_used < nb_pkts_received; nb_used++)
		virtio_update_packet_stats(&rxvq->stats, ref_rx_pkts[nb_used]);

	return nb_pkts_received;
}
