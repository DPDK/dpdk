/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _VHOST_NET_USER_H
#define _VHOST_NET_USER_H

#include <stdint.h>
#include <linux/vhost.h>

#include "rte_vhost.h"

/* refer to hw/virtio/vhost-user.c */

#define VHOST_MEMORY_MAX_NREGIONS 8

#define VHOST_USER_PROTOCOL_FEATURES	((1ULL << VHOST_USER_PROTOCOL_F_MQ) | \
					 (1ULL << VHOST_USER_PROTOCOL_F_LOG_SHMFD) |\
					 (1ULL << VHOST_USER_PROTOCOL_F_RARP) | \
					 (1ULL << VHOST_USER_PROTOCOL_F_REPLY_ACK) | \
					 (1ULL << VHOST_USER_PROTOCOL_F_NET_MTU) | \
					 (1ULL << VHOST_USER_PROTOCOL_F_SLAVE_REQ))

typedef enum VhostUserRequest {
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
	VHOST_USER_SEND_RARP = 19,
	VHOST_USER_NET_SET_MTU = 20,
	VHOST_USER_SET_SLAVE_REQ_FD = 21,
	VHOST_USER_IOTLB_MSG = 22,
	VHOST_USER_MAX
} VhostUserRequest;

typedef enum VhostUserSlaveRequest {
	VHOST_USER_SLAVE_NONE = 0,
	VHOST_USER_SLAVE_IOTLB_MSG = 1,
	VHOST_USER_SLAVE_MAX
} VhostUserSlaveRequest;

typedef struct VhostUserMemoryRegion {
	uint64_t guest_phys_addr;
	uint64_t memory_size;
	uint64_t userspace_addr;
	uint64_t mmap_offset;
} VhostUserMemoryRegion;

typedef struct VhostUserMemory {
	uint32_t nregions;
	uint32_t padding;
	VhostUserMemoryRegion regions[VHOST_MEMORY_MAX_NREGIONS];
} VhostUserMemory;

typedef struct VhostUserLog {
	uint64_t mmap_size;
	uint64_t mmap_offset;
} VhostUserLog;

typedef struct VhostUserMsg {
	union {
		uint32_t master; /* a VhostUserRequest value */
		uint32_t slave;  /* a VhostUserSlaveRequest value*/
	} request;

#define VHOST_USER_VERSION_MASK     0x3
#define VHOST_USER_REPLY_MASK       (0x1 << 2)
#define VHOST_USER_NEED_REPLY		(0x1 << 3)
	uint32_t flags;
	uint32_t size; /* the following payload size */
	union {
#define VHOST_USER_VRING_IDX_MASK   0xff
#define VHOST_USER_VRING_NOFD_MASK  (0x1<<8)
		uint64_t u64;
		struct vhost_vring_state state;
		struct vhost_vring_addr addr;
		VhostUserMemory memory;
		VhostUserLog    log;
		struct vhost_iotlb_msg iotlb;
	} payload;
	int fds[VHOST_MEMORY_MAX_NREGIONS];
} __attribute((packed)) VhostUserMsg;

#define VHOST_USER_HDR_SIZE offsetof(VhostUserMsg, payload.u64)

/* The version of the protocol we support */
#define VHOST_USER_VERSION    0x1


/* vhost_user.c */
int vhost_user_msg_handler(int vid, int fd);
int vhost_user_iotlb_miss(struct virtio_net *dev, uint64_t iova, uint8_t perm);

/* socket.c */
int read_fd_message(int sockfd, char *buf, int buflen, int *fds, int fd_num);
int send_fd_message(int sockfd, char *buf, int buflen, int *fds, int fd_num);

#endif
