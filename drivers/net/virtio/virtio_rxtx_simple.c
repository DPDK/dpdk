/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_branch_prediction.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <rte_prefetch.h>
#include <rte_string_fns.h>
#include <rte_errno.h>
#include <rte_byteorder.h>

#include "virtio_rxtx_simple.h"

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

int __rte_cold
virtio_rxq_vec_setup(struct virtnet_rx *rxq)
{
	struct virtqueue *vq = virtnet_rxq_to_vq(rxq);
	uintptr_t p;
	struct rte_mbuf mb_def = { .buf_addr = 0 }; /* zeroed mbuf */

	mb_def.nb_segs = 1;
	mb_def.data_off = RTE_PKTMBUF_HEADROOM;
	mb_def.port = vq->hw->port_id;
	rte_mbuf_refcnt_set(&mb_def, 1);

	/* prevent compiler reordering: rearm_data covers previous fields */
	rte_compiler_barrier();
	p = (uintptr_t)&mb_def.rearm_data;
	rxq->mbuf_initializer = *(uint64_t *)p;

	return 0;
}

/* Stub for linkage when arch specific implementation is not available */
__rte_weak uint16_t
virtio_recv_pkts_vec(void *rx_queue __rte_unused,
		     struct rte_mbuf **rx_pkts __rte_unused,
		     uint16_t nb_pkts __rte_unused)
{
	rte_panic("Wrong weak function linked by linker\n");
	return 0;
}
