/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 Cisco Systems, Inc.  All rights reserved.
 */

#ifndef _MEMIF_SOCKET_H_
#define _MEMIF_SOCKET_H_

#include <sys/queue.h>
#include <sys/un.h>

/**
 * Remove device from socket device list. If no device is left on the socket,
 * remove the socket as well.
 *
 * @param dev
 *   memif device
 */
void memif_socket_remove_device(struct rte_eth_dev *dev);

/**
 * Enqueue disconnect message to control channel message queue.
 *
 * @param cc
 *   control channel
 * @param reason
 *   const string stating disconnect reason (96 characters)
 * @param err_code
 *   error code
 */
void memif_msg_enq_disconnect(struct memif_control_channel *cc, const char *reason,
			      int err_code);

/**
 * Initialize memif socket for specified device. If socket doesn't exist, create socket.
 *
 * @param dev
 *   memif device
 * @param socket_filename
 *   socket filename
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
int memif_socket_init(struct rte_eth_dev *dev, const char *socket_filename);

/**
 * Disconnect memif device. Close control channel and shared memory.
 *
 * @param dev
 *   memif device
 */
void memif_disconnect(struct rte_eth_dev *dev);

/**
 * If device is properly configured, enable connection establishment.
 *
 * @param dev
 *   memif device
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
int memif_connect_server(struct rte_eth_dev *dev);


/**
 * If device is properly configured, send connection request.
 *
 * @param dev
 *   memif device
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
int memif_connect_client(struct rte_eth_dev *dev);

struct memif_socket_dev_list_elt {
	TAILQ_ENTRY(memif_socket_dev_list_elt) next;
	struct rte_eth_dev *dev;		/**< pointer to device internals */
	char dev_name[RTE_ETH_NAME_MAX_LEN];
};

#define MEMIF_SOCKET_HASH_NAME			"memif-sh"
#define MEMIF_SOCKET_UN_SIZE	\
	(sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path))

struct memif_socket {
	struct rte_intr_handle intr_handle;	/**< interrupt handle */
	char filename[MEMIF_SOCKET_UN_SIZE];	/**< socket filename */

	TAILQ_HEAD(, memif_socket_dev_list_elt) dev_queue;
	/**< Queue of devices using this socket */
	uint8_t listener;			/**< if not zero socket is listener */
};

/* Control message queue. */
struct memif_msg_queue_elt {
	memif_msg_t msg;			/**< control message */
	TAILQ_ENTRY(memif_msg_queue_elt) next;
	int fd;					/**< fd to be sent to peer */
};

struct memif_control_channel {
	struct rte_intr_handle intr_handle;	/**< interrupt handle */
	TAILQ_HEAD(, memif_msg_queue_elt) msg_queue; /**< control message queue */
	struct memif_socket *socket;		/**< pointer to socket */
	struct rte_eth_dev *dev;		/**< pointer to device */
};

#endif				/* MEMIF_SOCKET_H */
