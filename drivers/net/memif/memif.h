/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 Cisco Systems, Inc.  All rights reserved.
 */

#ifndef _MEMIF_H_
#define _MEMIF_H_

#define MEMIF_COOKIE		0x3E31F20
#define MEMIF_VERSION_MAJOR	2
#define MEMIF_VERSION_MINOR	0
#define MEMIF_VERSION		((MEMIF_VERSION_MAJOR << 8) | MEMIF_VERSION_MINOR)
#define MEMIF_NAME_SZ		32

/*
 * S2M: direction slave -> master
 * M2S: direction master -> slave
 */

/*
 *  Type definitions
 */

typedef enum memif_msg_type {
	MEMIF_MSG_TYPE_NONE,
	MEMIF_MSG_TYPE_ACK,
	MEMIF_MSG_TYPE_HELLO,
	MEMIF_MSG_TYPE_INIT,
	MEMIF_MSG_TYPE_ADD_REGION,
	MEMIF_MSG_TYPE_ADD_RING,
	MEMIF_MSG_TYPE_CONNECT,
	MEMIF_MSG_TYPE_CONNECTED,
	MEMIF_MSG_TYPE_DISCONNECT,
} memif_msg_type_t;

typedef enum {
	MEMIF_RING_S2M, /**< buffer ring in direction slave -> master */
	MEMIF_RING_M2S, /**< buffer ring in direction master -> slave */
} memif_ring_type_t;

typedef enum {
	MEMIF_INTERFACE_MODE_ETHERNET,
	MEMIF_INTERFACE_MODE_IP,
	MEMIF_INTERFACE_MODE_PUNT_INJECT,
} memif_interface_mode_t;

typedef uint16_t memif_region_index_t;
typedef uint32_t memif_region_offset_t;
typedef uint64_t memif_region_size_t;
typedef uint16_t memif_ring_index_t;
typedef uint32_t memif_interface_id_t;
typedef uint16_t memif_version_t;
typedef uint8_t memif_log2_ring_size_t;

/*
 *  Socket messages
 */

 /**
  * M2S
  * Contains master interfaces configuration.
  */
typedef struct __rte_packed {
	uint8_t name[MEMIF_NAME_SZ]; /**< Client app name. In this case DPDK version */
	memif_version_t min_version; /**< lowest supported memif version */
	memif_version_t max_version; /**< highest supported memif version */
	memif_region_index_t max_region; /**< maximum num of regions */
	memif_ring_index_t max_m2s_ring; /**< maximum num of M2S ring */
	memif_ring_index_t max_s2m_ring; /**< maximum num of S2M rings */
	memif_log2_ring_size_t max_log2_ring_size; /**< maximum ring size (as log2) */
} memif_msg_hello_t;

/**
 * S2M
 * Contains information required to identify interface
 * to which the slave wants to connect.
 */
typedef struct __rte_packed {
	memif_version_t version;		/**< memif version */
	memif_interface_id_t id;		/**< interface id */
	memif_interface_mode_t mode:8;		/**< interface mode */
	uint8_t secret[24];			/**< optional security parameter */
	uint8_t name[MEMIF_NAME_SZ]; /**< Client app name. In this case DPDK version */
} memif_msg_init_t;

/**
 * S2M
 * Request master to add new shared memory region to master interface.
 * Shared files file descriptor is passed in cmsghdr.
 */
typedef struct __rte_packed {
	memif_region_index_t index;		/**< shm regions index */
	memif_region_size_t size;		/**< shm region size */
} memif_msg_add_region_t;

/**
 * S2M
 * Request master to add new ring to master interface.
 */
typedef struct __rte_packed {
	uint16_t flags;				/**< flags */
#define MEMIF_MSG_ADD_RING_FLAG_S2M 1		/**< ring is in S2M direction */
	memif_ring_index_t index;		/**< ring index */
	memif_region_index_t region; /**< region index on which this ring is located */
	memif_region_offset_t offset;		/**< buffer start offset */
	memif_log2_ring_size_t log2_ring_size;	/**< ring size (log2) */
	uint16_t private_hdr_size;		/**< used for private metadata */
} memif_msg_add_ring_t;

/**
 * S2M
 * Finalize connection establishment.
 */
typedef struct __rte_packed {
	uint8_t if_name[MEMIF_NAME_SZ];		/**< slave interface name */
} memif_msg_connect_t;

/**
 * M2S
 * Finalize connection establishment.
 */
typedef struct __rte_packed {
	uint8_t if_name[MEMIF_NAME_SZ];		/**< master interface name */
} memif_msg_connected_t;

/**
 * S2M & M2S
 * Disconnect interfaces.
 */
typedef struct __rte_packed {
	uint32_t code;				/**< error code */
	uint8_t string[96];			/**< disconnect reason */
} memif_msg_disconnect_t;

typedef struct __rte_packed __rte_aligned(128)
{
	memif_msg_type_t type:16;
	union {
		memif_msg_hello_t hello;
		memif_msg_init_t init;
		memif_msg_add_region_t add_region;
		memif_msg_add_ring_t add_ring;
		memif_msg_connect_t connect;
		memif_msg_connected_t connected;
		memif_msg_disconnect_t disconnect;
	};
} memif_msg_t;

/*
 *  Ring and Descriptor Layout
 */

/**
 * Buffer descriptor.
 */
typedef struct __rte_packed {
	uint16_t flags;				/**< flags */
#define MEMIF_DESC_FLAG_NEXT 1			/**< is chained buffer */
	memif_region_index_t region; /**< region index on which the buffer is located */
	uint32_t length;			/**< buffer length */
	memif_region_offset_t offset;		/**< buffer offset */
	uint32_t metadata;
} memif_desc_t;

#define MEMIF_CACHELINE_ALIGN_MARK(mark) \
	RTE_MARKER mark __rte_cache_aligned;

typedef struct {
	MEMIF_CACHELINE_ALIGN_MARK(cacheline0);
	uint32_t cookie;			/**< MEMIF_COOKIE */
	uint16_t flags;				/**< flags */
#define MEMIF_RING_FLAG_MASK_INT 1		/**< disable interrupt mode */
	uint16_t head;			/**< pointer to ring buffer head */
	MEMIF_CACHELINE_ALIGN_MARK(cacheline1);
	uint16_t tail;			/**< pointer to ring buffer tail */
	MEMIF_CACHELINE_ALIGN_MARK(cacheline2);
	memif_desc_t desc[0];			/**< buffer descriptors */
} memif_ring_t;

#endif				/* _MEMIF_H_ */
