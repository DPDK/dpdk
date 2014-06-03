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

#ifndef _XEN_VHOST_H_
#define _XEN_VHOST_H_

#include <stdint.h>

#include <rte_tailq.h>
#include <rte_ether.h>

#include "virtio-net.h"

#define RTE_LOGTYPE_XENHOST RTE_LOGTYPE_USER1

#define XEN_VM_ROOTNODE_FMT  "/local/domain/%d/control/dpdk"
#define XEN_VM_NODE_FMT      "/local/domain/%d/control/dpdk/%s"
#define XEN_MEMPOOL_SUFFIX   "mempool_gref"
#define XEN_RXVRING_SUFFIX   "rx_vring_gref"
#define XEN_TXVRING_SUFFIX   "tx_vring_gref"
#define XEN_GVA_SUFFIX       "mempool_va"
#define XEN_VRINGFLAG_SUFFIX "vring_flag"
#define XEN_ADDR_SUFFIX      "ether_addr"
#define VIRTIO_START         "event_type_start_"

#define XEN_GREF_SPLITTOKEN  ','

#define MAX_XENVIRT_MEMPOOL 16
#define MAX_VIRTIO  32
#define MAX_GREF_PER_NODE 64  /* 128 MB memory */

#define PAGE_SIZE   4096
#define PAGE_PFNNUM (PAGE_SIZE / sizeof(uint32_t))

#define XEN_GNTDEV_FNAME "/dev/xen/gntdev"

/* xen grant reference info in one grant node */
struct xen_gnt {
	uint32_t gref;	/* grant reference for this node */
	union {
		int gref;		/* grant reference */
		uint32_t pfn_num;	/* guest pfn number of grant reference */
	} gref_pfn[PAGE_PFNNUM];
}__attribute__((__packed__));


/* structure for mempool or vring node list */
struct xen_gntnode {
	uint32_t gnt_num;           /* grant reference number */
	struct xen_gnt *gnt_info;   /* grant reference info */
};


struct xen_vring {
	uint32_t dom_id;
	uint32_t virtio_idx;    /* index of virtio device */
	void *rxvring_addr;     /* mapped virtual address of rxvring */
	void *txvring_addr;     /* mapped virtual address of txvring */
	uint32_t rxpfn_num;     /* number of gpfn for rxvring */
	uint32_t txpfn_num;	/* number of gpfn for txvring */
	uint32_t *rxpfn_tbl;    /* array of rxvring gpfn */
	uint32_t *txpfn_tbl;	/* array of txvring gpfn */
	uint64_t *rx_pindex;    /* index used to release rx grefs */
	uint64_t *tx_pindex;    /* index used to release tx grefs */
	uint64_t  flag_index;
	uint8_t  *flag; 	/* cleared to zero on guest unmap */
	struct ether_addr addr; /* ethernet address of virtio device */
	uint8_t   removed;

};

struct xen_mempool {
	uint32_t dom_id;      /* guest domain id */
	uint32_t pool_idx;    /* index of memory pool */
	void *gva;            /* guest virtual address of mbuf pool */
	void *hva;            /* host virtual address of mbuf pool */
	uint32_t mempfn_num;  /* number of gpfn for mbuf pool */
	uint32_t *mempfn_tbl; /* array of mbuf pool gpfn */
	uint64_t *pindex;     /* index used to release grefs */
};

struct xen_guest {
	TAILQ_ENTRY(xen_guest) next;
	int32_t dom_id;       /* guest domain id */
	uint32_t pool_num;    /* number of mbuf pool of the guest */
	uint32_t vring_num;   /* number of virtio ports of the guest */
	/* array contain the guest mbuf pool info */
	struct xen_mempool mempool[MAX_XENVIRT_MEMPOOL];
	/* array contain the guest rx/tx vring info */
	struct xen_vring vring[MAX_VIRTIO];
};

TAILQ_HEAD(xen_guestlist, xen_guest);

int
parse_mempoolnode(struct xen_guest *guest);

int
xenhost_init(void);

int
parse_vringnode(struct xen_guest *guest, uint32_t virtio_idx);

int
parse_mempoolnode(struct xen_guest *guest);

void
cleanup_mempool(struct xen_mempool *mempool);

void
cleanup_vring(struct xen_vring *vring);

void
virtio_monitor_loop(void);

int
init_virtio_xen(struct virtio_net_device_ops const * const);

#endif
