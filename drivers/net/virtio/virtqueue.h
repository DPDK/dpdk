/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _VIRTQUEUE_H_
#define _VIRTQUEUE_H_

#include <stdint.h>

#include <rte_atomic.h>
#include <rte_memory.h>
#include <rte_mempool.h>

#include "virtio_pci.h"
#include "virtio_ring.h"
#include "virtio_logs.h"
#include "virtio_rxtx.h"

struct rte_mbuf;

/*
 * Per virtio_config.h in Linux.
 *     For virtio_pci on SMP, we don't need to order with respect to MMIO
 *     accesses through relaxed memory I/O windows, so smp_mb() et al are
 *     sufficient.
 *
 */
#define virtio_mb()	rte_smp_mb()
#define virtio_rmb()	rte_smp_rmb()
#define virtio_wmb()	rte_smp_wmb()

#ifdef RTE_PMD_PACKET_PREFETCH
#define rte_packet_prefetch(p)  rte_prefetch1(p)
#else
#define rte_packet_prefetch(p)  do {} while(0)
#endif

#define VIRTQUEUE_MAX_NAME_SZ 32

#ifdef RTE_VIRTIO_USER
/**
 * Return the physical address (or virtual address in case of
 * virtio-user) of mbuf data buffer.
 *
 * The address is firstly casted to the word size (sizeof(uintptr_t))
 * before casting it to uint64_t. This is to make it work with different
 * combination of word size (64 bit and 32 bit) and virtio device
 * (virtio-pci and virtio-user).
 */
#define VIRTIO_MBUF_ADDR(mb, vq) \
	((uint64_t)(*(uintptr_t *)((uintptr_t)(mb) + (vq)->offset)))
#else
#define VIRTIO_MBUF_ADDR(mb, vq) ((mb)->buf_iova)
#endif

/**
 * Return the physical address (or virtual address in case of
 * virtio-user) of mbuf data buffer, taking care of mbuf data offset
 */
#define VIRTIO_MBUF_DATA_DMA_ADDR(mb, vq) \
	(VIRTIO_MBUF_ADDR(mb, vq) + (mb)->data_off)

#define VTNET_SQ_RQ_QUEUE_IDX 0
#define VTNET_SQ_TQ_QUEUE_IDX 1
#define VTNET_SQ_CQ_QUEUE_IDX 2

enum { VTNET_RQ = 0, VTNET_TQ = 1, VTNET_CQ = 2 };
/**
 * The maximum virtqueue size is 2^15. Use that value as the end of
 * descriptor chain terminator since it will never be a valid index
 * in the descriptor table. This is used to verify we are correctly
 * handling vq_free_cnt.
 */
#define VQ_RING_DESC_CHAIN_END 32768

/**
 * Control the RX mode, ie. promiscuous, allmulti, etc...
 * All commands require an "out" sg entry containing a 1 byte
 * state value, zero = disable, non-zero = enable.  Commands
 * 0 and 1 are supported with the VIRTIO_NET_F_CTRL_RX feature.
 * Commands 2-5 are added with VIRTIO_NET_F_CTRL_RX_EXTRA.
 */
#define VIRTIO_NET_CTRL_RX              0
#define VIRTIO_NET_CTRL_RX_PROMISC      0
#define VIRTIO_NET_CTRL_RX_ALLMULTI     1
#define VIRTIO_NET_CTRL_RX_ALLUNI       2
#define VIRTIO_NET_CTRL_RX_NOMULTI      3
#define VIRTIO_NET_CTRL_RX_NOUNI        4
#define VIRTIO_NET_CTRL_RX_NOBCAST      5

/**
 * Control the MAC
 *
 * The MAC filter table is managed by the hypervisor, the guest should
 * assume the size is infinite.  Filtering should be considered
 * non-perfect, ie. based on hypervisor resources, the guest may
 * received packets from sources not specified in the filter list.
 *
 * In addition to the class/cmd header, the TABLE_SET command requires
 * two out scatterlists.  Each contains a 4 byte count of entries followed
 * by a concatenated byte stream of the ETH_ALEN MAC addresses.  The
 * first sg list contains unicast addresses, the second is for multicast.
 * This functionality is present if the VIRTIO_NET_F_CTRL_RX feature
 * is available.
 *
 * The ADDR_SET command requests one out scatterlist, it contains a
 * 6 bytes MAC address. This functionality is present if the
 * VIRTIO_NET_F_CTRL_MAC_ADDR feature is available.
 */
struct virtio_net_ctrl_mac {
	uint32_t entries;
	uint8_t macs[][ETHER_ADDR_LEN];
} __attribute__((__packed__));

#define VIRTIO_NET_CTRL_MAC    1
#define VIRTIO_NET_CTRL_MAC_TABLE_SET        0
#define VIRTIO_NET_CTRL_MAC_ADDR_SET         1

/**
 * Control VLAN filtering
 *
 * The VLAN filter table is controlled via a simple ADD/DEL interface.
 * VLAN IDs not added may be filtered by the hypervisor.  Del is the
 * opposite of add.  Both commands expect an out entry containing a 2
 * byte VLAN ID.  VLAN filtering is available with the
 * VIRTIO_NET_F_CTRL_VLAN feature bit.
 */
#define VIRTIO_NET_CTRL_VLAN     2
#define VIRTIO_NET_CTRL_VLAN_ADD 0
#define VIRTIO_NET_CTRL_VLAN_DEL 1

/*
 * Control link announce acknowledgement
 *
 * The command VIRTIO_NET_CTRL_ANNOUNCE_ACK is used to indicate that
 * driver has recevied the notification; device would clear the
 * VIRTIO_NET_S_ANNOUNCE bit in the status field after it receives
 * this command.
 */
#define VIRTIO_NET_CTRL_ANNOUNCE     3
#define VIRTIO_NET_CTRL_ANNOUNCE_ACK 0

struct virtio_net_ctrl_hdr {
	uint8_t class;
	uint8_t cmd;
} __attribute__((packed));

typedef uint8_t virtio_net_ctrl_ack;

#define VIRTIO_NET_OK     0
#define VIRTIO_NET_ERR    1

#define VIRTIO_MAX_CTRL_DATA 2048

struct virtio_pmd_ctrl {
	struct virtio_net_ctrl_hdr hdr;
	virtio_net_ctrl_ack status;
	uint8_t data[VIRTIO_MAX_CTRL_DATA];
};

struct vq_desc_extra {
	void *cookie;
	uint16_t ndescs;
};

struct virtqueue {
	struct virtio_hw  *hw; /**< virtio_hw structure pointer. */
	struct vring vq_ring;  /**< vring keeping desc, used and avail */
	/**
	 * Last consumed descriptor in the used table,
	 * trails vq_ring.used->idx.
	 */
	uint16_t vq_used_cons_idx;
	uint16_t vq_nentries;  /**< vring desc numbers */
	uint16_t vq_free_cnt;  /**< num of desc available */
	uint16_t vq_avail_idx; /**< sync until needed */
	uint16_t vq_free_thresh; /**< free threshold */

	void *vq_ring_virt_mem;  /**< linear address of vring*/
	unsigned int vq_ring_size;

	union {
		struct virtnet_rx rxq;
		struct virtnet_tx txq;
		struct virtnet_ctl cq;
	};

	rte_iova_t vq_ring_mem; /**< physical address of vring,
	                         * or virtual address for virtio_user. */

	/**
	 * Head of the free chain in the descriptor table. If
	 * there are no free descriptors, this will be set to
	 * VQ_RING_DESC_CHAIN_END.
	 */
	uint16_t  vq_desc_head_idx;
	uint16_t  vq_desc_tail_idx;
	uint16_t  vq_queue_index;   /**< PCI queue index */
	uint16_t offset; /**< relative offset to obtain addr in mbuf */
	uint16_t  *notify_addr;
	struct rte_mbuf **sw_ring;  /**< RX software ring. */
	struct vq_desc_extra vq_descx[0];
};

/* If multiqueue is provided by host, then we suppport it. */
#define VIRTIO_NET_CTRL_MQ   4
#define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET        0
#define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MIN        1
#define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MAX        0x8000

/**
 * This is the first element of the scatter-gather list.  If you don't
 * specify GSO or CSUM features, you can simply ignore the header.
 */
struct virtio_net_hdr {
#define VIRTIO_NET_HDR_F_NEEDS_CSUM 1    /**< Use csum_start,csum_offset*/
#define VIRTIO_NET_HDR_F_DATA_VALID 2    /**< Checksum is valid */
	uint8_t flags;
#define VIRTIO_NET_HDR_GSO_NONE     0    /**< Not a GSO frame */
#define VIRTIO_NET_HDR_GSO_TCPV4    1    /**< GSO frame, IPv4 TCP (TSO) */
#define VIRTIO_NET_HDR_GSO_UDP      3    /**< GSO frame, IPv4 UDP (UFO) */
#define VIRTIO_NET_HDR_GSO_TCPV6    4    /**< GSO frame, IPv6 TCP */
#define VIRTIO_NET_HDR_GSO_ECN      0x80 /**< TCP has ECN set */
	uint8_t gso_type;
	uint16_t hdr_len;     /**< Ethernet + IP + tcp/udp hdrs */
	uint16_t gso_size;    /**< Bytes to append to hdr_len per frame */
	uint16_t csum_start;  /**< Position to start checksumming from */
	uint16_t csum_offset; /**< Offset after that to place checksum */
};

/**
 * This is the version of the header to use when the MRG_RXBUF
 * feature has been negotiated.
 */
struct virtio_net_hdr_mrg_rxbuf {
	struct   virtio_net_hdr hdr;
	uint16_t num_buffers; /**< Number of merged rx buffers */
};

/* Region reserved to allow for transmit header and indirect ring */
#define VIRTIO_MAX_TX_INDIRECT 8
struct virtio_tx_region {
	struct virtio_net_hdr_mrg_rxbuf tx_hdr;
	struct vring_desc tx_indir[VIRTIO_MAX_TX_INDIRECT]
			   __attribute__((__aligned__(16)));
};

/* Chain all the descriptors in the ring with an END */
static inline void
vring_desc_init(struct vring_desc *dp, uint16_t n)
{
	uint16_t i;

	for (i = 0; i < n - 1; i++)
		dp[i].next = (uint16_t)(i + 1);
	dp[i].next = VQ_RING_DESC_CHAIN_END;
}

/**
 * Tell the backend not to interrupt us.
 */
static inline void
virtqueue_disable_intr(struct virtqueue *vq)
{
	vq->vq_ring.avail->flags |= VRING_AVAIL_F_NO_INTERRUPT;
}

/**
 * Tell the backend to interrupt us.
 */
static inline void
virtqueue_enable_intr(struct virtqueue *vq)
{
	vq->vq_ring.avail->flags &= (~VRING_AVAIL_F_NO_INTERRUPT);
}

/**
 *  Dump virtqueue internal structures, for debug purpose only.
 */
void virtqueue_dump(struct virtqueue *vq);
/**
 *  Get all mbufs to be freed.
 */
struct rte_mbuf *virtqueue_detach_unused(struct virtqueue *vq);

/* Flush the elements in the used ring. */
void virtqueue_rxvq_flush(struct virtqueue *vq);

static inline int
virtqueue_full(const struct virtqueue *vq)
{
	return vq->vq_free_cnt == 0;
}

static inline int
virtio_get_queue_type(struct virtio_hw *hw, uint16_t vtpci_queue_idx)
{
	if (vtpci_queue_idx == hw->max_queue_pairs * 2)
		return VTNET_CQ;
	else if (vtpci_queue_idx % 2 == 0)
		return VTNET_RQ;
	else
		return VTNET_TQ;
}

#define VIRTQUEUE_NUSED(vq) ((uint16_t)((vq)->vq_ring.used->idx - (vq)->vq_used_cons_idx))

void vq_ring_free_chain(struct virtqueue *vq, uint16_t desc_idx);

static inline void
vq_update_avail_idx(struct virtqueue *vq)
{
	virtio_wmb();
	vq->vq_ring.avail->idx = vq->vq_avail_idx;
}

static inline void
vq_update_avail_ring(struct virtqueue *vq, uint16_t desc_idx)
{
	uint16_t avail_idx;
	/*
	 * Place the head of the descriptor chain into the next slot and make
	 * it usable to the host. The chain is made available now rather than
	 * deferring to virtqueue_notify() in the hopes that if the host is
	 * currently running on another CPU, we can keep it processing the new
	 * descriptor.
	 */
	avail_idx = (uint16_t)(vq->vq_avail_idx & (vq->vq_nentries - 1));
	if (unlikely(vq->vq_ring.avail->ring[avail_idx] != desc_idx))
		vq->vq_ring.avail->ring[avail_idx] = desc_idx;
	vq->vq_avail_idx++;
}

static inline int
virtqueue_kick_prepare(struct virtqueue *vq)
{
	return !(vq->vq_ring.used->flags & VRING_USED_F_NO_NOTIFY);
}

static inline void
virtqueue_notify(struct virtqueue *vq)
{
	/*
	 * Ensure updated avail->idx is visible to host.
	 * For virtio on IA, the notificaiton is through io port operation
	 * which is a serialization instruction itself.
	 */
	VTPCI_OPS(vq->hw)->notify_queue(vq->hw, vq);
}

#ifdef RTE_LIBRTE_VIRTIO_DEBUG_DUMP
#define VIRTQUEUE_DUMP(vq) do { \
	uint16_t used_idx, nused; \
	used_idx = (vq)->vq_ring.used->idx; \
	nused = (uint16_t)(used_idx - (vq)->vq_used_cons_idx); \
	PMD_INIT_LOG(DEBUG, \
	  "VQ: - size=%d; free=%d; used=%d; desc_head_idx=%d;" \
	  " avail.idx=%d; used_cons_idx=%d; used.idx=%d;" \
	  " avail.flags=0x%x; used.flags=0x%x", \
	  (vq)->vq_nentries, (vq)->vq_free_cnt, nused, \
	  (vq)->vq_desc_head_idx, (vq)->vq_ring.avail->idx, \
	  (vq)->vq_used_cons_idx, (vq)->vq_ring.used->idx, \
	  (vq)->vq_ring.avail->flags, (vq)->vq_ring.used->flags); \
} while (0)
#else
#define VIRTQUEUE_DUMP(vq) do { } while (0)
#endif

#endif /* _VIRTQUEUE_H_ */
