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

#define PAGE_SIZE   4096

/* Enum for virtqueue management. */
enum {VIRTIO_RXQ, VIRTIO_TXQ, VIRTIO_QNUM};

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
} __rte_cache_aligned;

/*
 * Device structure contains all configuration information relating to the device.
 */
struct virtio_net
{
	struct vhost_virtqueue	*virtqueue[VIRTIO_QNUM];	/* Contains all virtqueue information. */
	struct virtio_memory 	*mem;						/* QEMU memory and memory region information. */
	struct ether_addr 		mac_address;				/* Device MAC address (Obtained on first TX packet). */
	uint64_t 				features;					/* Negotiated feature set. */
	uint64_t 				device_fh;					/* device identifier. */
	uint32_t 				vmdq_rx_q;					/* RX VMDQ queue number. */
	uint32_t 				flags;						/* Device flags. Only used to check if device is running on data core. */
	uint32_t 				vlan_tag;					/* Vlan tag for device. Currently set to device_id (0-63). */
	uint16_t 				coreid;						/* Data core that the device is added to. */
	volatile uint8_t 		ready;						/* A device is set as ready if the MAC address has been set. */
	volatile uint8_t		remove;						/* Device is marked for removal from the data core. */
} __rte_cache_aligned;

/*
 * Device linked list structure for configuration.
 */
struct virtio_net_config_ll
{
	struct virtio_net		dev;	/* Virtio device. */
	struct virtio_net_config_ll	*next; /* Next entry on linked list. */
};

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
 * Information relating to memory regions including offsets to
 * addresses in host physical space.
 */
struct virtio_memory_regions_hpa {
	/* Base guest physical address of region. */
	uint64_t	guest_phys_address;
	/* End guest physical address of region. */
	uint64_t	guest_phys_address_end;
	/* Size of region. */
	uint64_t	memory_size;
	/* Offset of region for gpa to hpa translation. */
	uint64_t	host_phys_addr_offset;
};

/*
 * Memory structure includes region and mapping information.
 */
struct virtio_memory {
	uint64_t			base_address;			/* Base QEMU userspace address of the memory file. */
	uint64_t			mapped_address;			/* Mapped address of memory file base in our applications memory space. */
	uint64_t			mapped_size;			/* Total size of memory file. */
	uint32_t			nregions;				/* Number of memory regions. */
	 /* Number of memory regions for gpa to hpa translation. */
	uint32_t			nregions_hpa;
	/* Memory region information for gpa to hpa translation. */
	struct virtio_memory_regions_hpa  *regions_hpa;
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

#endif /* _VIRTIO_NET_H_ */
