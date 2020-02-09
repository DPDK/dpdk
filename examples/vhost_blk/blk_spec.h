/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _BLK_SPEC_H
#define _BLK_SPEC_H

#include <stdint.h>

#ifndef VHOST_USER_MEMORY_MAX_NREGIONS
#define VHOST_USER_MEMORY_MAX_NREGIONS		8
#endif

#ifndef VHOST_USER_MAX_CONFIG_SIZE
#define VHOST_USER_MAX_CONFIG_SIZE		256
#endif

#ifndef VHOST_USER_PROTOCOL_F_CONFIG
#define VHOST_USER_PROTOCOL_F_CONFIG		9
#endif

#ifndef VHOST_USER_PROTOCOL_F_INFLIGHT_SHMFD
#define VHOST_USER_PROTOCOL_F_INFLIGHT_SHMFD	12
#endif

#define VIRTIO_BLK_ID_BYTES	20	/* ID string length */

#define VIRTIO_BLK_T_IN			0
#define VIRTIO_BLK_T_OUT		1
#define VIRTIO_BLK_T_FLUSH		4
#define VIRTIO_BLK_T_GET_ID		8
#define VIRTIO_BLK_T_DISCARD		11
#define VIRTIO_BLK_T_WRITE_ZEROES	13

#define VIRTIO_BLK_S_OK			0
#define VIRTIO_BLK_S_IOERR		1
#define VIRTIO_BLK_S_UNSUPP		2

enum vhost_user_request {
	VHOST_USER_NONE = 0,
	VHOST_USER_GET_FEATURES = 1,
	VHOST_USER_SET_FEATURES = 2,
	VHOST_USER_SET_OWNER = 3,
	VHOST_USER_RESET_OWNER = 4,
	VHOST_USER_SET_MEM_TABLE = 5,
	VHOST_USER_SET_LOG_BASE = 6,
	VHOST_USER_SET_LOG_FD = 7,
	VHOST_USER_SET_VRING_NUM = 8,
	VHOST_USER_SET_VRING_ADDR = 9,
	VHOST_USER_SET_VRING_BASE = 10,
	VHOST_USER_GET_VRING_BASE = 11,
	VHOST_USER_SET_VRING_KICK = 12,
	VHOST_USER_SET_VRING_CALL = 13,
	VHOST_USER_SET_VRING_ERR = 14,
	VHOST_USER_GET_PROTOCOL_FEATURES = 15,
	VHOST_USER_SET_PROTOCOL_FEATURES = 16,
	VHOST_USER_GET_QUEUE_NUM = 17,
	VHOST_USER_SET_VRING_ENABLE = 18,
	VHOST_USER_MAX
};

/** Get/set config msg payload */
struct vhost_user_config {
	uint32_t offset;
	uint32_t size;
	uint32_t flags;
	uint8_t region[VHOST_USER_MAX_CONFIG_SIZE];
};

/** Fixed-size vhost_memory struct */
struct vhost_memory_padded {
	uint32_t nregions;
	uint32_t padding;
	struct vhost_memory_region regions[VHOST_USER_MEMORY_MAX_NREGIONS];
};

struct vhost_user_msg {
	enum vhost_user_request request;

#define VHOST_USER_VERSION_MASK     0x3
#define VHOST_USER_REPLY_MASK       (0x1 << 2)
	uint32_t flags;
	uint32_t size; /**< the following payload size */
	union {
#define VHOST_USER_VRING_IDX_MASK   0xff
#define VHOST_USER_VRING_NOFD_MASK  (0x1 << 8)
		uint64_t u64;
		struct vhost_vring_state state;
		struct vhost_vring_addr addr;
		struct vhost_memory_padded memory;
		struct vhost_user_config cfg;
	} payload;
} __rte_packed;

#endif
