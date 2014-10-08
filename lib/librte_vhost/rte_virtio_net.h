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

#ifndef _VIRTIO_NET_H_
#define _VIRTIO_NET_H_

/* Used to indicate that the device is running on a data core */
#define VIRTIO_DEV_RUNNING 1

/* Backend value set by guest. */
#define VIRTIO_DEV_STOPPED -1


/* Enum for virtqueue management. */
enum {VIRTIO_RXQ, VIRTIO_TXQ, VIRTIO_QNUM};

#define BUF_VECTOR_MAX 256

/*
 * Structure contains buffer address, length and descriptor index
 * from vring to do scatter RX.
*/
struct buf_vector {
uint64_t buf_addr;
uint32_t buf_len;
uint32_t desc_idx;
};

/*
 * Structure contains variables relevant to TX/RX virtqueues.
 */
struct vhost_virtqueue
{
	struct vring_desc	*desc;				/* Virtqueue descriptor ring. */
	struct vring_avail	*avail;				/* Virtqueue available ring. */
	struct vring_used	*used;				/* Virtqueue used ring. */
	uint32_t			size;				/* Size of descriptor ring. */
	uint32_t			backend;			/* Backend value to determine if device should started/stopped. */
	uint16_t			vhost_hlen;			/* Vhost header length (varies depending on RX merge buffers. */
	volatile uint16_t	last_used_idx;		/* Last index used on the available ring */
	volatile uint16_t	last_used_idx_res;	/* Used for multiple devices reserving buffers. */
	eventfd_t			callfd;				/* Currently unused as polling mode is enabled. */
	eventfd_t			kickfd;				/* Used to notify the guest (trigger interrupt). */
	/* Used for scatter RX. */
	struct buf_vector	buf_vec[BUF_VECTOR_MAX];
} __rte_cache_aligned;

/*
 * Device structure contains all configuration information relating to the device.
 */
struct virtio_net
{
	struct vhost_virtqueue	*virtqueue[VIRTIO_QNUM];	/* Contains all virtqueue information. */
	struct virtio_memory 	*mem;						/* QEMU memory and memory region information. */
	uint64_t 				features;					/* Negotiated feature set. */
	uint64_t 				device_fh;					/* device identifier. */
	uint32_t 				flags;						/* Device flags. Only used to check if device is running on data core. */
} __rte_cache_aligned;

/*
 * Information relating to memory regions including offsets to addresses in QEMUs memory file.
 */
struct virtio_memory_regions {
	uint64_t	guest_phys_address;		/* Base guest physical address of region. */
	uint64_t	guest_phys_address_end;	/* End guest physical address of region. */
	uint64_t	memory_size;			/* Size of region. */
	uint64_t	userspace_address;		/* Base userspace address of region. */
	uint64_t	address_offset;			/* Offset of region for address translation. */
};


/*
 * Memory structure includes region and mapping information.
 */
struct virtio_memory {
	uint64_t			base_address;			/* Base QEMU userspace address of the memory file. */
	uint64_t			mapped_address;			/* Mapped address of memory file base in our applications memory space. */
	uint64_t			mapped_size;			/* Total size of memory file. */
	uint32_t			nregions;				/* Number of memory regions. */
	/* Memory region information. */
	struct virtio_memory_regions      regions[0];
};

/*
 * Device operations to add/remove device.
 */
struct virtio_net_device_ops {
	int (* new_device) 		(struct virtio_net *);	/* Add device. */
	void (* destroy_device)	(volatile struct virtio_net *);	/* Remove device. */
};

int init_virtio_net(struct virtio_net_device_ops const * const);
int deinit_virtio_net(void);

struct vhost_net_device_ops const * get_virtio_net_callbacks(void);

/**
 * Function to convert guest physical addresses to vhost virtual addresses.
 * This is used to convert guest virtio buffer addresses.
 */
static inline uint64_t __attribute__((always_inline))
gpa_to_vva(struct virtio_net *dev, uint64_t guest_pa)
{
	struct virtio_memory_regions *region;
	uint32_t regionidx;
	uint64_t vhost_va = 0;

	for (regionidx = 0; regionidx < dev->mem->nregions; regionidx++) {
		region = &dev->mem->regions[regionidx];
		if ((guest_pa >= region->guest_phys_address) &&
			(guest_pa <= region->guest_phys_address_end)) {
			vhost_va = region->address_offset + guest_pa;
			break;
		}
	}
	return vhost_va;
}

#endif /* _VIRTIO_NET_H_ */
