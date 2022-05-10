#ifndef _VIRTIO_ADMIN_H
#define _VIRTIO_ADMIN_H
/* This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE. */

/*
 * This feature indicates that the device supports administration virtqueue
 * negotiation.
 */
#define VIRTIO_F_ADMIN_VQ               40

/* We support indirect buffer descriptors for admin queue */
#define VIRTIO_F_ADMIN_VQ_INDIRECT_DESC 41

/* We support in-order buffer consumptions for admin queue */
#define VIRTIO_F_ADMIN_VQ_IN_ORDER      42

/*
 * This feature indicates that the device supports administrating migration
 * operations for its managed functions using the admin queue of the parent
 * device.
 */
#define VIRTIO_F_ADMIN_MIGRATION        43

/*
 * This feature indicates that the device supports internal state tracking
 * mechanism for its managed devices, while managed device internal status is
 * not VIRTIO_S_FREEZED.
 */
#define VIRTIO_F_ADMIN_MIGRATION_DYNAMIC_INTERNAL_STATE_TRACK   44

/*
 * This feature indicates that the device supports dirty page tracking in push
 * map mode with bit granularity for its managed devices using the admin
 * virtq of the parent device.
 */
#define VIRTIO_F_ADMIN_DIRTY_PAGE_PUSH_BITMAP_TRACK 45

/*
 * This feature indicates that the device supports dirty page tracking in push
 * map mode with byte granularity for its managed devices using the admin
 * virtq of the parent device.
 */
#define VIRTIO_F_ADMIN_DIRTY_PAGE_PUSH_BYTEMAP_TRACK 46

/*
 * This feature indicates that the device supports dirty page tracking in pull
 * map mode with bit granularity for its managed devices using the admin
 * virtq of the parent device.
 */
#define VIRTIO_F_ADMIN_DIRTY_PAGE_PULL_BITMAP_TRACK 47

/*
 * This feature indicates that the device supports dirty page tracking in pull
 * map mode with byte granularity for its managed devices using the
 * admin virtq of the parent device.
 */
#define VIRTIO_F_ADMIN_DIRTY_PAGE_PULL_BYTEMAP_TRACK 48

/*
 * Admin virtqueue data structures
 *
 * The Admin virtqueue expects a header in the first sg entry
 * and an ack/status response in the last entry.  Data for the
 * command goes in between.
 */
struct virtio_admin_hdr {
	uint8_t class;
	uint8_t cmd;
} __attribute__((packed));

typedef uint8_t virtio_admin_status;

/* status values that are transport, device and vendor independent */
#define VIRTIO_ADMIN_STATUS_COMMON_START	0
#define VIRTIO_ADMIN_STATUS_COMMON_END		63

/* status values that are transport specific */
#define VIRTIO_ADMIN_STATUS_TRANSPORT_START	64
#define VIRTIO_ADMIN_STATUS_TRANSPORT_END	127

/* status values that are device specific */
#define VIRTIO_ADMIN_STATUS_DEVICE_START	128
#define VIRTIO_ADMIN_STATUS_DEVICE_END		191

/* status values that are reserved */
#define VIRTIO_ADMIN_STATUS_RESERVED_START	192
#define VIRTIO_ADMIN_STATUS_RESERVED_END	255

/* Common status values */
#define VIRTIO_ADMIN_STATUS_COMMON_OK			0
#define VIRTIO_ADMIN_STATUS_COMMON_ERR			1
#define VIRTIO_ADMIN_STATUS_COMMON_INVALID_CLASS	2
#define VIRTIO_ADMIN_STATUS_COMMON_INVALID_COMMAND	3
#define VIRTIO_ADMIN_STATUS_COMMON_DATA_TRANSFER_ERR	4
#define VIRTIO_ADMIN_STATUS_COMMON_DEVICE_INTERNAL_ERR	5

/* class values that are transport, device and vendor independent */
#define VIRTIO_ADMIN_COMMON_CLASS_START		0
#define VIRTIO_ADMIN_COMMON_CLASS_END		63

/* class values that are transport specific */
#define VIRTIO_ADMIN_TRANSPORT_CLASS_START	64
#define VIRTIO_ADMIN_TRANSPORT_CLASS_END	127

/* class values that are device specific */
#define VIRTIO_ADMIN_DEVICE_CLASS_START		128
#define VIRTIO_ADMIN_DEVICE_CLASS_END		191

/* class values that are reserved */
#define VIRTIO_ADMIN_RESERVED_CLASS_START	192
#define VIRTIO_ADMIN_RESERVED_CLASS_END		255


#endif /* _VIRTIO_ADMIN_H */
