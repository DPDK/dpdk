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

#ifndef _VHOST_NET_USER_H
#define _VHOST_NET_USER_H

#include <stdint.h>
#include <linux/vhost.h>

#include "rte_vhost.h"

/* refer to hw/virtio/vhost-user.c */

#define VHOST_MEMORY_MAX_NREGIONS 8

#define VHOST_USER_PROTOCOL_F_MQ	0
#define VHOST_USER_PROTOCOL_F_LOG_SHMFD	1
#define VHOST_USER_PROTOCOL_F_RARP	2
#define VHOST_USER_PROTOCOL_F_REPLY_ACK	3
#define VHOST_USER_PROTOCOL_F_NET_MTU 4

/*
 * disable REPLY_ACK feature to workaround the buggy QEMU implementation.
 * Proved buggy QEMU includes v2.7 - v2.9.
 */
#define VHOST_USER_PROTOCOL_FEATURES	((1ULL << VHOST_USER_PROTOCOL_F_MQ) | \
					 (1ULL << VHOST_USER_PROTOCOL_F_LOG_SHMFD) |\
					 (1ULL << VHOST_USER_PROTOCOL_F_RARP) | \
					 (0ULL << VHOST_USER_PROTOCOL_F_REPLY_ACK) | \
					 (1ULL << VHOST_USER_PROTOCOL_F_NET_MTU))

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
	VHOST_USER_MAX
} VhostUserRequest;

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
	VhostUserRequest request;

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
	} payload;
	int fds[VHOST_MEMORY_MAX_NREGIONS];
} __attribute((packed)) VhostUserMsg;

#define VHOST_USER_HDR_SIZE offsetof(VhostUserMsg, payload.u64)

/* The version of the protocol we support */
#define VHOST_USER_VERSION    0x1


/* vhost_user.c */
int vhost_user_msg_handler(int vid, int fd);

/* socket.c */
int read_fd_message(int sockfd, char *buf, int buflen, int *fds, int fd_num);
int send_fd_message(int sockfd, char *buf, int buflen, int *fds, int fd_num);

#endif
