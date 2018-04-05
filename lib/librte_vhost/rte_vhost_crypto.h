/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#ifndef _VHOST_CRYPTO_H_
#define _VHOST_CRYPTO_H_

#define VHOST_CRYPTO_MBUF_POOL_SIZE		(8192)
#define VHOST_CRYPTO_MAX_BURST_SIZE		(64)
#define VHOST_CRYPTO_SESSION_MAP_ENTRIES	(1024) /**< Max nb sessions */
/** max nb virtual queues in a burst for finalizing*/
#define VIRTIO_CRYPTO_MAX_NUM_BURST_VQS		(64)

enum rte_vhost_crypto_zero_copy {
	RTE_VHOST_CRYPTO_ZERO_COPY_DISABLE = 0,
	RTE_VHOST_CRYPTO_ZERO_COPY_ENABLE = 1,
	RTE_VHOST_CRYPTO_MAX_ZERO_COPY_OPTIONS
};

/**
 *  Create Vhost-crypto instance
 *
 * @param vid
 *  The identifier of the vhost device.
 * @param cryptodev_id
 *  The identifier of DPDK Cryptodev, the same cryptodev_id can be assigned to
 *  multiple Vhost-crypto devices.
 * @param sess_pool
 *  The pointer to the created cryptodev session pool with the private data size
 *  matches the target DPDK Cryptodev.
 * @param socket_id
 *  NUMA Socket ID to allocate resources on. *
 * @return
 *  0 if the Vhost Crypto Instance is created successfully.
 *  Negative integer if otherwise
 */
int __rte_experimental
rte_vhost_crypto_create(int vid, uint8_t cryptodev_id,
		struct rte_mempool *sess_pool, int socket_id);

/**
 *  Free the Vhost-crypto instance
 *
 * @param vid
 *  The identifier of the vhost device.
 * @return
 *  0 if the Vhost Crypto Instance is created successfully.
 *  Negative integer if otherwise.
 */
int __rte_experimental
rte_vhost_crypto_free(int vid);

/**
 *  Enable or disable zero copy feature
 *
 * @param vid
 *  The identifier of the vhost device.
 * @param option
 *  Flag of zero copy feature.
 * @return
 *  0 if completed successfully.
 *  Negative integer if otherwise.
 */
int __rte_experimental
rte_vhost_crypto_set_zero_copy(int vid, enum rte_vhost_crypto_zero_copy option);

/**
 * Fetch a number of vring descriptors from virt-queue and translate to DPDK
 * crypto operations. After this function is executed, the user can enqueue
 * the processed ops to the target cryptodev.
 *
 * @param vid
 *  The identifier of the vhost device.
 * @param qid
 *  Virtio queue index.
 * @param ops
 *  The address of an array of pointers to *rte_crypto_op* structures that must
 *  be large enough to store *nb_ops* pointers in it.
 * @param nb_ops
 *  The maximum number of operations to be fetched and translated.
 * @return
 *  The number of fetched and processed vhost crypto request operations.
 */
uint16_t __rte_experimental
rte_vhost_crypto_fetch_requests(int vid, uint32_t qid,
		struct rte_crypto_op **ops, uint16_t nb_ops);
/**
 * Finalize the dequeued crypto ops. After the translated crypto ops are
 * dequeued from the cryptodev, this function shall be called to write the
 * processed data back to the vring descriptor (if no-copy is turned off).
 *
 * @param ops
 *  The address of an array of *rte_crypto_op* structure that was dequeued
 *  from cryptodev.
 * @param nb_ops
 *  The number of operations contained in the array.
 * @callfds
 *  The callfd number(s) contained in this burst, this shall be an array with
 *  no less than VIRTIO_CRYPTO_MAX_NUM_BURST_VQS elements.
 * @nb_callfds
 *  The number of call_fd numbers exist in the callfds.
 * @return
 *  The number of ops processed.
 */
uint16_t __rte_experimental
rte_vhost_crypto_finalize_requests(struct rte_crypto_op **ops,
		uint16_t nb_ops, int *callfds, uint16_t *nb_callfds);

#endif /**< _VHOST_CRYPTO_H_ */
