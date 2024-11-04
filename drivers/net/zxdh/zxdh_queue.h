/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef ZXDH_QUEUE_H
#define ZXDH_QUEUE_H

#include <stdint.h>

#include <rte_common.h>

#include "zxdh_ethdev.h"
#include "zxdh_rxtx.h"
#include "zxdh_pci.h"

enum { ZXDH_VTNET_RQ = 0, ZXDH_VTNET_TQ = 1 };

#define ZXDH_VIRTQUEUE_MAX_NAME_SZ        32
#define ZXDH_RQ_QUEUE_IDX                 0
#define ZXDH_TQ_QUEUE_IDX                 1
#define ZXDH_MAX_TX_INDIRECT              8

/* This marks a buffer as write-only (otherwise read-only). */
#define ZXDH_VRING_DESC_F_WRITE           2
/* This flag means the descriptor was made available by the driver */
#define ZXDH_VRING_PACKED_DESC_F_AVAIL   (1 << (7))

#define ZXDH_RING_EVENT_FLAGS_ENABLE      0x0
#define ZXDH_RING_EVENT_FLAGS_DISABLE     0x1
#define ZXDH_RING_EVENT_FLAGS_DESC        0x2

#define ZXDH_VQ_RING_DESC_CHAIN_END       32768

/*
 * ring descriptors: 16 bytes.
 * These can chain together via "next".
 */
struct zxdh_vring_desc {
	uint64_t addr;  /*  Address (guest-physical). */
	uint32_t len;   /* Length. */
	uint16_t flags; /* The flags as indicated above. */
	uint16_t next;  /* We chain unused descriptors via this. */
} __rte_packed;

struct zxdh_vring_used_elem {
	/* Index of start of used descriptor chain. */
	uint32_t id;
	/* Total length of the descriptor chain which was written to. */
	uint32_t len;
};

struct zxdh_vring_used {
	uint16_t flags;
	uint16_t idx;
	struct zxdh_vring_used_elem ring[];
} __rte_packed;

struct zxdh_vring_avail {
	uint16_t flags;
	uint16_t idx;
	uint16_t ring[];
} __rte_packed;

struct zxdh_vring_packed_desc {
	uint64_t addr;
	uint32_t len;
	uint16_t id;
	uint16_t flags;
} __rte_packed;

struct zxdh_vring_packed_desc_event {
	uint16_t desc_event_off_wrap;
	uint16_t desc_event_flags;
} __rte_packed;

struct zxdh_vring_packed {
	uint32_t num;
	struct zxdh_vring_packed_desc *desc;
	struct zxdh_vring_packed_desc_event *driver;
	struct zxdh_vring_packed_desc_event *device;
} __rte_packed;

struct zxdh_vq_desc_extra {
	void *cookie;
	uint16_t ndescs;
	uint16_t next;
} __rte_packed;

struct zxdh_virtqueue {
	struct zxdh_hw  *hw; /* < zxdh_hw structure pointer. */
	struct {
		/* vring keeping descs and events */
		struct zxdh_vring_packed ring;
		uint8_t used_wrap_counter;
		uint8_t rsv;
		uint16_t cached_flags; /* < cached flags for descs */
		uint16_t event_flags_shadow;
		uint16_t rsv1;
	} __rte_packed vq_packed;
	uint16_t vq_used_cons_idx; /* < last consumed descriptor */
	uint16_t vq_nentries;  /* < vring desc numbers */
	uint16_t vq_free_cnt;  /* < num of desc available */
	uint16_t vq_avail_idx; /* < sync until needed */
	uint16_t vq_free_thresh; /* < free threshold */
	uint16_t rsv2;

	void *vq_ring_virt_mem;  /* < linear address of vring */
	uint32_t vq_ring_size;

	union {
		struct zxdh_virtnet_rx rxq;
		struct zxdh_virtnet_tx txq;
	};

	/*
	 * physical address of vring, or virtual address
	 */
	rte_iova_t vq_ring_mem;

	/*
	 * Head of the free chain in the descriptor table. If
	 * there are no free descriptors, this will be set to
	 * VQ_RING_DESC_CHAIN_END.
	 */
	uint16_t  vq_desc_head_idx;
	uint16_t  vq_desc_tail_idx;
	uint16_t  vq_queue_index;   /* < PCI queue index */
	uint16_t  offset; /* < relative offset to obtain addr in mbuf */
	uint16_t *notify_addr;
	struct rte_mbuf **sw_ring;  /* < RX software ring. */
	struct zxdh_vq_desc_extra vq_descx[];
} __rte_packed;

struct zxdh_type_hdr {
	uint8_t port;  /* bit[0:1] 00-np 01-DRS 10-DTP */
	uint8_t pd_len;
	uint8_t num_buffers;
	uint8_t reserved;
} __rte_packed; /* 4B */

struct zxdh_pi_hdr {
	uint8_t  pi_len;
	uint8_t  pkt_type;
	uint16_t vlan_id;
	uint32_t ipv6_extend;
	uint16_t l3_offset;
	uint16_t l4_offset;
	uint8_t  phy_port;
	uint8_t  pkt_flag_hi8;
	uint16_t pkt_flag_lw16;
	union {
		struct {
			uint64_t sa_idx;
			uint8_t  reserved_8[8];
		} dl;
		struct {
			uint32_t lro_flag;
			uint32_t lro_mss;
			uint16_t err_code;
			uint16_t pm_id;
			uint16_t pkt_len;
			uint8_t  reserved[2];
		} ul;
	};
} __rte_packed; /* 32B */

struct zxdh_pd_hdr_dl {
	uint32_t ol_flag;
	uint8_t tag_idx;
	uint8_t tag_data;
	uint16_t dst_vfid;
	uint32_t svlan_insert;
	uint32_t cvlan_insert;
} __rte_packed; /* 16B */

struct zxdh_net_hdr_dl {
	struct zxdh_type_hdr  type_hdr; /* 4B */
	struct zxdh_pi_hdr    pi_hdr; /* 32B */
	struct zxdh_pd_hdr_dl pd_hdr; /* 16B */
} __rte_packed;

struct zxdh_pd_hdr_ul {
	uint32_t pkt_flag;
	uint32_t rss_hash;
	uint32_t fd;
	uint32_t striped_vlan_tci;
	uint8_t tag_idx;
	uint8_t tag_data;
	uint16_t src_vfid;
	uint16_t pkt_type_out;
	uint16_t pkt_type_in;
} __rte_packed; /* 24B */

struct zxdh_net_hdr_ul {
	struct zxdh_type_hdr  type_hdr; /* 4B */
	struct zxdh_pi_hdr    pi_hdr; /* 32B */
	struct zxdh_pd_hdr_ul pd_hdr; /* 24B */
} __rte_packed; /* 60B */

struct zxdh_tx_region {
	struct zxdh_net_hdr_dl tx_hdr;
	union {
		struct zxdh_vring_desc tx_indir[ZXDH_MAX_TX_INDIRECT];
		struct zxdh_vring_packed_desc tx_packed_indir[ZXDH_MAX_TX_INDIRECT];
	} __rte_packed;
};

static inline size_t
vring_size(struct zxdh_hw *hw, uint32_t num, unsigned long align)
{
	size_t size;

	if (vtpci_packed_queue(hw)) {
		size = num * sizeof(struct zxdh_vring_packed_desc);
		size += sizeof(struct zxdh_vring_packed_desc_event);
		size = RTE_ALIGN_CEIL(size, align);
		size += sizeof(struct zxdh_vring_packed_desc_event);
		return size;
	}

	size = num * sizeof(struct zxdh_vring_desc);
	size += sizeof(struct zxdh_vring_avail) + (num * sizeof(uint16_t));
	size = RTE_ALIGN_CEIL(size, align);
	size += sizeof(struct zxdh_vring_used) + (num * sizeof(struct zxdh_vring_used_elem));
	return size;
}

static inline void
vring_init_packed(struct zxdh_vring_packed *vr, uint8_t *p,
		unsigned long align, uint32_t num)
{
	vr->num    = num;
	vr->desc   = (struct zxdh_vring_packed_desc *)p;
	vr->driver = (struct zxdh_vring_packed_desc_event *)(p +
				 vr->num * sizeof(struct zxdh_vring_packed_desc));
	vr->device = (struct zxdh_vring_packed_desc_event *)RTE_ALIGN_CEIL(((uintptr_t)vr->driver +
				 sizeof(struct zxdh_vring_packed_desc_event)), align);
}

static inline void
vring_desc_init_packed(struct zxdh_virtqueue *vq, int32_t n)
{
	int32_t i = 0;

	for (i = 0; i < n - 1; i++) {
		vq->vq_packed.ring.desc[i].id = i;
		vq->vq_descx[i].next = i + 1;
	}
	vq->vq_packed.ring.desc[i].id = i;
	vq->vq_descx[i].next = ZXDH_VQ_RING_DESC_CHAIN_END;
}

static inline void
vring_desc_init_indirect_packed(struct zxdh_vring_packed_desc *dp, int32_t n)
{
	int32_t i = 0;

	for (i = 0; i < n; i++) {
		dp[i].id = (uint16_t)i;
		dp[i].flags = ZXDH_VRING_DESC_F_WRITE;
	}
}

static inline void
virtqueue_disable_intr(struct zxdh_virtqueue *vq)
{
	if (vq->vq_packed.event_flags_shadow != ZXDH_RING_EVENT_FLAGS_DISABLE) {
		vq->vq_packed.event_flags_shadow = ZXDH_RING_EVENT_FLAGS_DISABLE;
		vq->vq_packed.ring.driver->desc_event_flags = vq->vq_packed.event_flags_shadow;
	}
}

struct rte_mbuf *zxdh_virtqueue_detach_unused(struct zxdh_virtqueue *vq);
int32_t zxdh_free_queues(struct rte_eth_dev *dev);
int32_t zxdh_get_queue_type(uint16_t vtpci_queue_idx);

#endif /* ZXDH_QUEUE_H */
