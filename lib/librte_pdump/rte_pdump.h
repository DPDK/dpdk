/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
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

#ifndef _RTE_PDUMP_H_
#define _RTE_PDUMP_H_

/**
 * @file
 * RTE pdump
 *
 * packet dump library to provide packet capturing support on dpdk.
 */

#include <stdint.h>
#include <rte_mempool.h>
#include <rte_ring.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_PDUMP_ALL_QUEUES UINT16_MAX

enum {
	RTE_PDUMP_FLAG_RX = 1,  /* receive direction */
	RTE_PDUMP_FLAG_TX = 2,  /* transmit direction */
	/* both receive and transmit directions */
	RTE_PDUMP_FLAG_RXTX = (RTE_PDUMP_FLAG_RX|RTE_PDUMP_FLAG_TX)
};

enum rte_pdump_socktype {
	RTE_PDUMP_SOCKET_SERVER = 1,
	RTE_PDUMP_SOCKET_CLIENT = 2
};

/**
 * Initialize packet capturing handling
 *
 * Creates pthread and server socket for handling clients
 * requests to enable/disable rxtx callbacks.
 *
 * @param path
 * directory path for server socket.
 *
 * @return
 *    0 on success, -1 on error
 */
int
rte_pdump_init(const char *path);

/**
 * Un initialize packet capturing handling
 *
 * Cancels pthread, close server socket, removes server socket address.
 *
 * @return
 *    0 on success, -1 on error
 */
int
rte_pdump_uninit(void);

/**
 * Enables packet capturing on given port and queue.
 *
 * @param port
 *  port on which packet capturing should be enabled.
 * @param queue
 *  queue of a given port on which packet capturing should be enabled.
 *  users should pass on value UINT16_MAX to enable packet capturing on all
 *  queues of a given port.
 * @param flags
 *  flags specifies RTE_PDUMP_FLAG_RX/RTE_PDUMP_FLAG_TX/RTE_PDUMP_FLAG_RXTX
 *  on which packet capturing should be enabled for a given port and queue.
 * @param ring
 *  ring on which captured packets will be enqueued for user.
 * @param mp
 *  mempool on to which original packets will be mirrored or duplicated.
 * @param filter
 *  place holder for packet filtering.
 *
 * @return
 *    0 on success, -1 on error, rte_errno is set accordingly.
 */

int
rte_pdump_enable(uint8_t port, uint16_t queue, uint32_t flags,
		struct rte_ring *ring,
		struct rte_mempool *mp,
		void *filter);

/**
 * Disables packet capturing on given port and queue.
 *
 * @param port
 *  port on which packet capturing should be disabled.
 * @param queue
 *  queue of a given port on which packet capturing should be disabled.
 *  users should pass on value UINT16_MAX to disable packet capturing on all
 *  queues of a given port.
 * @param flags
 *  flags specifies RTE_PDUMP_FLAG_RX/RTE_PDUMP_FLAG_TX/RTE_PDUMP_FLAG_RXTX
 *  on which packet capturing should be enabled for a given port and queue.
 *
 * @return
 *    0 on success, -1 on error, rte_errno is set accordingly.
 */

int
rte_pdump_disable(uint8_t port, uint16_t queue, uint32_t flags);

/**
 * Enables packet capturing on given device id and queue.
 * device_id can be name or pci address of device.
 *
 * @param device_id
 *  device id on which packet capturing should be enabled.
 * @param queue
 *  queue of a given device id on which packet capturing should be enabled.
 *  users should pass on value UINT16_MAX to enable packet capturing on all
 *  queues of a given device id.
 * @param flags
 *  flags specifies RTE_PDUMP_FLAG_RX/RTE_PDUMP_FLAG_TX/RTE_PDUMP_FLAG_RXTX
 *  on which packet capturing should be enabled for a given port and queue.
 * @param ring
 *  ring on which captured packets will be enqueued for user.
 * @param mp
 *  mempool on to which original packets will be mirrored or duplicated.
 * @param filter
 *  place holder for packet filtering.
 *
 * @return
 *    0 on success, -1 on error, rte_errno is set accordingly.
 */

int
rte_pdump_enable_by_deviceid(char *device_id, uint16_t queue,
				uint32_t flags,
				struct rte_ring *ring,
				struct rte_mempool *mp,
				void *filter);

/**
 * Disables packet capturing on given device_id and queue.
 * device_id can be name or pci address of device.
 *
 * @param device_id
 *  pci address or name of the device on which packet capturing
 *  should be disabled.
 * @param queue
 *  queue of a given device on which packet capturing should be disabled.
 *  users should pass on value UINT16_MAX to disable packet capturing on all
 *  queues of a given device id.
 * @param flags
 *  flags specifies RTE_PDUMP_FLAG_RX/RTE_PDUMP_FLAG_TX/RTE_PDUMP_FLAG_RXTX
 *  on which packet capturing should be enabled for a given port and queue.
 *
 * @return
 *    0 on success, -1 on error, rte_errno is set accordingly.
 */
int
rte_pdump_disable_by_deviceid(char *device_id, uint16_t queue,
				uint32_t flags);

/**
 * Allows applications to set server and client socket paths.
 * If specified path is null default path will be selected, i.e.
 *"/var/run/" for root user and "$HOME" for non root user.
 * Clients also need to call this API to set their server path if the
 * server path is different from default path.
 * This API is not thread-safe.
 *
 * @param path
 * directory path for server or client socket.
 * @type
 * specifies RTE_PDUMP_SOCKET_SERVER if socket path is for server.
 * (or)
 * specifies RTE_PDUMP_SOCKET_CLIENT if socket path is for client.
 *
 * @return
 * 0 on success, -EINVAL on error
 *
 */
int
rte_pdump_set_socket_dir(const char *path, enum rte_pdump_socktype type);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PDUMP_H_ */
