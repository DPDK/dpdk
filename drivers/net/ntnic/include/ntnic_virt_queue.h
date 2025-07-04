/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTOSS_VIRT_QUEUE_H__
#define __NTOSS_VIRT_QUEUE_H__

#include <stdint.h>
#include <stdalign.h>

#include <rte_memory.h>

struct nthw_virt_queue;

#define SPLIT_RING        0
#define PACKED_RING       1
#define IN_ORDER          1

/*
 * SPLIT : This marks a buffer as continuing via the next field.
 * PACKED: This marks a buffer as continuing. (packed does not have a next field, so must be
 * contiguous) In Used descriptors it must be ignored
 */
#define VIRTQ_DESC_F_NEXT 1
/*
 * SPLIT : This marks a buffer as device write-only (otherwise device read-only).
 * PACKED: This marks a descriptor as device write-only (otherwise device read-only).
 * PACKED: In a used descriptor, this bit is used to specify whether any data has been written by
 * the device into any parts of the buffer.
 */
#define VIRTQ_DESC_F_WRITE 2

/*
 * Split Ring virtq Descriptor
 */
struct __rte_packed_begin virtq_desc {
	/* Address (guest-physical). */
	uint64_t addr;
	/* Length. */
	uint32_t len;
	/* The flags as indicated above. */
	uint16_t flags;
	/* Next field if flags & NEXT */
	uint16_t next;
} __rte_packed_end;


/*
 * Packed Ring special structures and defines
 */

/* additional packed ring flags */
#define VIRTQ_DESC_F_AVAIL     (1 << 7)
#define VIRTQ_DESC_F_USED      (1 << 15)

/* descr phys address must be 16 byte aligned */
struct __rte_aligned(16) pvirtq_desc {
	/* Buffer Address. */
	uint64_t addr;
	/* Buffer Length. */
	uint32_t len;
	/* Buffer ID. */
	uint16_t id;
	/* The flags depending on descriptor type. */
	uint16_t flags;
};

/* Disable events */
#define RING_EVENT_FLAGS_DISABLE 0x1

struct __rte_aligned(16) pvirtq_event_suppress {
	union {
		struct {
			/* Descriptor Ring Change Event Offset */
			uint16_t desc_event_off : 15;
			/* Descriptor Ring Change Event Wrap Counter */
			uint16_t desc_event_wrap : 1;
		};
		/* If desc_event_flags set to RING_EVENT_FLAGS_DESC */
		uint16_t desc;
	};

	union {
		struct {
			uint16_t desc_event_flags : 2;	/* Descriptor Ring Change Event Flags */
			uint16_t reserved : 14;	/* Reserved, set to 0 */
		};
		uint16_t flags;
	};
};

/*
 * Common virtq descr
 */
#define vq_set_next(vq, index, nxt) \
do { \
	struct nthw_cvirtq_desc *temp_vq = (vq); \
	if (temp_vq->vq_type == SPLIT_RING) \
		temp_vq->s[index].next = nxt; \
} while (0)

#define vq_add_flags(vq, index, flgs) \
do { \
	struct nthw_cvirtq_desc *temp_vq = (vq); \
	uint16_t tmp_index = (index); \
	typeof(flgs) tmp_flgs = (flgs); \
	if (temp_vq->vq_type == SPLIT_RING) \
		temp_vq->s[tmp_index].flags |= tmp_flgs; \
	else if (temp_vq->vq_type == PACKED_RING) \
		temp_vq->p[tmp_index].flags |= tmp_flgs; \
} while (0)

#define vq_set_flags(vq, index, flgs) \
do { \
	struct nthw_cvirtq_desc *temp_vq = (vq); \
	uint32_t temp_flags = (flgs); \
	uint32_t temp_index = (index); \
	if ((temp_vq)->vq_type == SPLIT_RING) \
		(temp_vq)->s[temp_index].flags = temp_flags; \
	else if ((temp_vq)->vq_type == PACKED_RING) \
		(temp_vq)->p[temp_index].flags = temp_flags; \
} while (0)

struct nthw_virtq_desc_buf {
	/* Address (guest-physical). */
	alignas(16) uint64_t addr;
	/* Length. */
	uint32_t len;
};

struct nthw_cvirtq_desc {
	union {
		struct nthw_virtq_desc_buf *b;  /* buffer part as is common */
		struct virtq_desc     *s;  /* SPLIT */
		struct pvirtq_desc    *p;  /* PACKED */
	};
	uint16_t vq_type;
};

struct nthw_received_packets {
	void *addr;
	uint32_t len;
};

#endif /* __NTOSS_VIRT_QUEUE_H__ */
