/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
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

#include <rte_bpf.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_PDUMP_ALL_QUEUES UINT16_MAX

enum {
	RTE_PDUMP_FLAG_RX = 1,  /* receive direction */
	RTE_PDUMP_FLAG_TX = 2,  /* transmit direction */
	/* both receive and transmit directions */
	RTE_PDUMP_FLAG_RXTX = (RTE_PDUMP_FLAG_RX|RTE_PDUMP_FLAG_TX),

	RTE_PDUMP_FLAG_PCAPNG = 4, /* format for pcapng */
};

/**
 * Initialize packet capturing handling
 *
 * Register the IPC action for communication with target (primary) process.
 *
 * @return
 *    0 on success, -1 on error
 */
int
rte_pdump_init(void);

/**
 * Un initialize packet capturing handling
 *
 * Unregister the IPC action for communication with target (primary) process.
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
 *  Unused should be NULL.
 *
 * @return
 *    0 on success, -1 on error, rte_errno is set accordingly.
 */

int
rte_pdump_enable(uint16_t port, uint16_t queue, uint32_t flags,
		struct rte_ring *ring,
		struct rte_mempool *mp,
		void *filter);

/**
 * Enables packet capturing on given port and queue with filtering.
 *
 * @param port_id
 *  The Ethernet port on which packet capturing should be enabled.
 * @param queue
 *  The queue on the Ethernet port which packet capturing
 *  should be enabled. Pass UINT16_MAX to enable packet capturing on all
 *  queues of a given port.
 * @param flags
 *  Pdump library flags that specify direction and packet format.
 * @param snaplen
 *  The upper limit on bytes to copy.
 *  Passing UINT32_MAX means capture all the possible data.
 * @param ring
 *  The ring on which captured packets will be enqueued for user.
 * @param mp
 *  The mempool on to which original packets will be mirrored or duplicated.
 * @param prm
 *  Use BPF program to run to filter packes (can be NULL)
 *
 * @return
 *    0 on success, -1 on error, rte_errno is set accordingly.
 */
int
rte_pdump_enable_bpf(uint16_t port_id, uint16_t queue,
		     uint32_t flags, uint32_t snaplen,
		     struct rte_ring *ring,
		     struct rte_mempool *mp,
		     const struct rte_bpf_prm *prm);

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
rte_pdump_disable(uint16_t port, uint16_t queue, uint32_t flags);

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
 *  unused should be NULL
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
 * Enables packet capturing on given device id and queue with filtering.
 * device_id can be name or pci address of device.
 *
 * @param device_id
 *  device id on which packet capturing should be enabled.
 * @param queue
 *  The queue on the Ethernet port which packet capturing
 *  should be enabled. Pass UINT16_MAX to enable packet capturing on all
 *  queues of a given port.
 * @param flags
 *  Pdump library flags that specify direction and packet format.
 * @param snaplen
 *  The upper limit on bytes to copy.
 *  Passing UINT32_MAX means capture all the possible data.
 * @param ring
 *  The ring on which captured packets will be enqueued for user.
 * @param mp
 *  The mempool on to which original packets will be mirrored or duplicated.
 * @param filter
 *  Use BPF program to run to filter packes (can be NULL)
 *
 * @return
 *    0 on success, -1 on error, rte_errno is set accordingly.
 */
int
rte_pdump_enable_bpf_by_deviceid(const char *device_id, uint16_t queue,
				 uint32_t flags, uint32_t snaplen,
				 struct rte_ring *ring,
				 struct rte_mempool *mp,
				 const struct rte_bpf_prm *filter);


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
 * A structure used to retrieve statistics from packet capture.
 * The statistics are sum of both receive and transmit queues.
 */
struct rte_pdump_stats {
	RTE_ATOMIC(uint64_t) accepted; /**< Number of packets accepted by filter. */
	RTE_ATOMIC(uint64_t) filtered; /**< Number of packets rejected by filter. */
	RTE_ATOMIC(uint64_t) nombuf;   /**< Number of mbuf allocation failures. */
	RTE_ATOMIC(uint64_t) ringfull; /**< Number of missed packets due to ring full. */

	uint64_t reserved[4]; /**< Reserved and pad to cache line */
};

/**
 * Retrieve the packet capture statistics for a queue.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param stats
 *   A pointer to structure of type *rte_pdump_stats* to be filled in.
 * @return
 *   Zero if successful. -1 on error and rte_errno is set.
 */
int
rte_pdump_stats(uint16_t port_id, struct rte_pdump_stats *stats);


#ifdef __cplusplus
}
#endif

#endif /* _RTE_PDUMP_H_ */
