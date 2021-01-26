/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _VIRTIO_RING_H_
#define _VIRTIO_RING_H_

#include <stdint.h>

#include <rte_common.h>

/* This marks a buffer as continuing via the next field. */
#define VRING_DESC_F_NEXT       1
/* This marks a buffer as write-only (otherwise read-only). */
#define VRING_DESC_F_WRITE      2
/* This means the buffer contains a list of buffer descriptors. */
#define VRING_DESC_F_INDIRECT   4

/* This flag means the descriptor was made available by the driver */
#define VRING_PACKED_DESC_F_AVAIL	(1 << 7)
/* This flag means the descriptor was used by the device */
#define VRING_PACKED_DESC_F_USED	(1 << 15)

/* Frequently used combinations */
#define VRING_PACKED_DESC_F_AVAIL_USED	(VRING_PACKED_DESC_F_AVAIL | \
					 VRING_PACKED_DESC_F_USED)

/* The Host uses this in used->flags to advise the Guest: don't kick me
 * when you add a buffer.  It's unreliable, so it's simply an
 * optimization.  Guest will still kick if it's out of buffers. */
#define VRING_USED_F_NO_NOTIFY  1
/* The Guest uses this in avail->flags to advise the Host: don't
 * interrupt me when you consume a buffer.  It's unreliable, so it's
 * simply an optimization.  */
#define VRING_AVAIL_F_NO_INTERRUPT  1

/* VirtIO ring descriptors: 16 bytes.
 * These can chain together via "next". */
struct vring_desc {
	uint64_t addr;  /*  Address (guest-physical). */
	uint32_t len;   /* Length. */
	uint16_t flags; /* The flags as indicated above. */
	uint16_t next;  /* We chain unused descriptors via this. */
};

struct vring_avail {
	uint16_t flags;
	uint16_t idx;
	uint16_t ring[0];
};

/* id is a 16bit index. uint32_t is used here for ids for padding reasons. */
struct vring_used_elem {
	/* Index of start of used descriptor chain. */
	uint32_t id;
	/* Total length of the descriptor chain which was written to. */
	uint32_t len;
};

struct vring_used {
	uint16_t flags;
	uint16_t idx;
	struct vring_used_elem ring[0];
};

/* For support of packed virtqueues in Virtio 1.1 the format of descriptors
 * looks like this.
 */
struct vring_packed_desc {
	uint64_t addr;
	uint32_t len;
	uint16_t id;
	uint16_t flags;
};

#define RING_EVENT_FLAGS_ENABLE 0x0
#define RING_EVENT_FLAGS_DISABLE 0x1
#define RING_EVENT_FLAGS_DESC 0x2
struct vring_packed_desc_event {
	uint16_t desc_event_off_wrap;
	uint16_t desc_event_flags;
};

struct vring_packed {
	unsigned int num;
	struct vring_packed_desc *desc;
	struct vring_packed_desc_event *driver;
	struct vring_packed_desc_event *device;
};

struct vring {
	unsigned int num;
	struct vring_desc  *desc;
	struct vring_avail *avail;
	struct vring_used  *used;
};

/* The standard layout for the ring is a continuous chunk of memory which
 * looks like this.  We assume num is a power of 2.
 *
 * struct vring {
 *      // The actual descriptors (16 bytes each)
 *      struct vring_desc desc[num];
 *
 *      // A ring of available descriptor heads with free-running index.
 *      __u16 avail_flags;
 *      __u16 avail_idx;
 *      __u16 available[num];
 *      __u16 used_event_idx;
 *
 *      // Padding to the next align boundary.
 *      char pad[];
 *
 *      // A ring of used descriptor heads with free-running index.
 *      __u16 used_flags;
 *      __u16 used_idx;
 *      struct vring_used_elem used[num];
 *      __u16 avail_event_idx;
 * };
 *
 * NOTE: for VirtIO PCI, align is 4096.
 */

/*
 * We publish the used event index at the end of the available ring, and vice
 * versa. They are at the end for backwards compatibility.
 */
#define vring_used_event(vr)  ((vr)->avail->ring[(vr)->num])
#define vring_avail_event(vr) (*(uint16_t *)&(vr)->used->ring[(vr)->num])

static inline size_t
vring_size(struct virtio_hw *hw, unsigned int num, unsigned long align)
{
	size_t size;

	if (virtio_with_packed_queue(hw)) {
		size = num * sizeof(struct vring_packed_desc);
		size += sizeof(struct vring_packed_desc_event);
		size = RTE_ALIGN_CEIL(size, align);
		size += sizeof(struct vring_packed_desc_event);
		return size;
	}

	size = num * sizeof(struct vring_desc);
	size += sizeof(struct vring_avail) + (num * sizeof(uint16_t));
	size = RTE_ALIGN_CEIL(size, align);
	size += sizeof(struct vring_used) +
		(num * sizeof(struct vring_used_elem));
	return size;
}
static inline void
vring_init_split(struct vring *vr, uint8_t *p, unsigned long align,
	 unsigned int num)
{
	vr->num = num;
	vr->desc = (struct vring_desc *) p;
	vr->avail = (struct vring_avail *) (p +
		num * sizeof(struct vring_desc));
	vr->used = (void *)
		RTE_ALIGN_CEIL((uintptr_t)(&vr->avail->ring[num]), align);
}

static inline void
vring_init_packed(struct vring_packed *vr, uint8_t *p, unsigned long align,
		 unsigned int num)
{
	vr->num = num;
	vr->desc = (struct vring_packed_desc *)p;
	vr->driver = (struct vring_packed_desc_event *)(p +
			vr->num * sizeof(struct vring_packed_desc));
	vr->device = (struct vring_packed_desc_event *)
		RTE_ALIGN_CEIL(((uintptr_t)vr->driver +
				sizeof(struct vring_packed_desc_event)), align);
}

/*
 * The following is used with VIRTIO_RING_F_EVENT_IDX.
 * Assuming a given event_idx value from the other size, if we have
 * just incremented index from old to new_idx, should we trigger an
 * event?
 */
static inline int
vring_need_event(uint16_t event_idx, uint16_t new_idx, uint16_t old)
{
	return (uint16_t)(new_idx - event_idx - 1) < (uint16_t)(new_idx - old);
}

#endif /* _VIRTIO_RING_H_ */
