/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _VHOST_NET_USER_H
#define _VHOST_NET_USER_H

#include <stdint.h>

#include "rte_vhost.h"

/* refer to hw/virtio/vhost-user.c */

#define VHOST_MEMORY_MAX_NREGIONS 8

#define VHOST_USER_PROTOCOL_FEATURES	((1ULL << VHOST_USER_PROTOCOL_F_MQ) | \
					 (1ULL << VHOST_USER_PROTOCOL_F_LOG_SHMFD) |\
					 (1ULL << VHOST_USER_PROTOCOL_F_RARP) | \
					 (1ULL << VHOST_USER_PROTOCOL_F_REPLY_ACK) | \
					 (1ULL << VHOST_USER_PROTOCOL_F_NET_MTU) | \
					 (1ULL << VHOST_USER_PROTOCOL_F_BACKEND_REQ) | \
					 (1ULL << VHOST_USER_PROTOCOL_F_CRYPTO_SESSION) | \
					 (1ULL << VHOST_USER_PROTOCOL_F_BACKEND_SEND_FD) | \
					 (1ULL << VHOST_USER_PROTOCOL_F_HOST_NOTIFIER) | \
					 (1ULL << VHOST_USER_PROTOCOL_F_PAGEFAULT) | \
					 (1ULL << VHOST_USER_PROTOCOL_F_STATUS))

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
	VHOST_USER_SET_BACKEND_REQ_FD = 21,
	VHOST_USER_IOTLB_MSG = 22,
	VHOST_USER_GET_CONFIG = 24,
	VHOST_USER_SET_CONFIG = 25,
	VHOST_USER_CRYPTO_CREATE_SESS = 26,
	VHOST_USER_CRYPTO_CLOSE_SESS = 27,
	VHOST_USER_POSTCOPY_ADVISE = 28,
	VHOST_USER_POSTCOPY_LISTEN = 29,
	VHOST_USER_POSTCOPY_END = 30,
	VHOST_USER_GET_INFLIGHT_FD = 31,
	VHOST_USER_SET_INFLIGHT_FD = 32,
	VHOST_USER_SET_STATUS = 39,
	VHOST_USER_GET_STATUS = 40,
} VhostUserRequest;

typedef enum VhostUserBackendRequest {
	VHOST_USER_BACKEND_NONE = 0,
	VHOST_USER_BACKEND_IOTLB_MSG = 1,
	VHOST_USER_BACKEND_CONFIG_CHANGE_MSG = 2,
	VHOST_USER_BACKEND_VRING_HOST_NOTIFIER_MSG = 3,
} VhostUserBackendRequest;

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

/* Comply with Cryptodev-Linux */
#define VHOST_USER_CRYPTO_MAX_HMAC_KEY_LENGTH	512
#define VHOST_USER_CRYPTO_MAX_CIPHER_KEY_LENGTH	64

/* Same structure as vhost-user backend session info */
typedef struct VhostUserCryptoSessionParam {
	int64_t session_id;
	uint32_t op_code;
	uint32_t cipher_algo;
	uint32_t cipher_key_len;
	uint32_t hash_algo;
	uint32_t digest_len;
	uint32_t auth_key_len;
	uint32_t aad_len;
	uint8_t op_type;
	uint8_t dir;
	uint8_t hash_mode;
	uint8_t chaining_dir;
	uint8_t *ciphe_key;
	uint8_t *auth_key;
	uint8_t cipher_key_buf[VHOST_USER_CRYPTO_MAX_CIPHER_KEY_LENGTH];
	uint8_t auth_key_buf[VHOST_USER_CRYPTO_MAX_HMAC_KEY_LENGTH];
} VhostUserCryptoSessionParam;

typedef struct VhostUserVringArea {
	uint64_t u64;
	uint64_t size;
	uint64_t offset;
} VhostUserVringArea;

typedef struct VhostUserInflight {
	uint64_t mmap_size;
	uint64_t mmap_offset;
	uint16_t num_queues;
	uint16_t queue_size;
} VhostUserInflight;

#define VHOST_USER_MAX_CONFIG_SIZE		256

/** Get/set config msg payload */
struct vhost_user_config {
	uint32_t offset;
	uint32_t size;
	uint32_t flags;
	uint8_t region[VHOST_USER_MAX_CONFIG_SIZE];
};

typedef struct VhostUserMsg {
	union {
		uint32_t frontend; /* a VhostUserRequest value */
		uint32_t backend;  /* a VhostUserBackendRequest value*/
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
		VhostUserCryptoSessionParam crypto_session;
		VhostUserVringArea area;
		VhostUserInflight inflight;
		struct vhost_user_config cfg;
	} payload;
	/* Nothing should be added after the payload */
} __rte_packed VhostUserMsg;

/* Note: this structure and VhostUserMsg can't be changed carelessly as
 * external message handlers rely on them.
 */
struct __rte_packed vhu_msg_context {
	VhostUserMsg msg;
	int fds[VHOST_MEMORY_MAX_NREGIONS];
	int fd_num;
};

#define VHOST_USER_HDR_SIZE offsetof(VhostUserMsg, payload.u64)

/* The version of the protocol we support */
#define VHOST_USER_VERSION    0x1


/* vhost_user.c */
int vhost_user_msg_handler(int vid, int fd);
int vhost_user_iotlb_miss(struct virtio_net *dev, uint64_t iova, uint8_t perm);

/* socket.c */
int read_fd_message(char *ifname, int sockfd, char *buf, int buflen, int *fds, int max_fds,
		int *fd_num);
int send_fd_message(char *ifname, int sockfd, char *buf, int buflen, int *fds, int fd_num);

#endif
