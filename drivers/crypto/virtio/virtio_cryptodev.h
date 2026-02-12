/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 HUAWEI TECHNOLOGIES CO., LTD.
 */

#ifndef _VIRTIO_CRYPTODEV_H_
#define _VIRTIO_CRYPTODEV_H_

#include "virtio_crypto.h"
#include "virtio_pci.h"
#include "virtio_ring.h"

/* Features desired/implemented by this driver. */
#define VIRTIO_CRYPTO_PMD_GUEST_FEATURES (1ULL << VIRTIO_F_VERSION_1 | \
	1ULL << VIRTIO_F_IN_ORDER                | \
	1ULL << VIRTIO_F_RING_PACKED             | \
	1ULL << VIRTIO_F_NOTIFICATION_DATA       | \
	1ULL << VIRTIO_RING_F_INDIRECT_DESC      | \
	1ULL << VIRTIO_F_ORDER_PLATFORM)

#define CRYPTODEV_NAME_VIRTIO_PMD crypto_virtio

#define NUM_ENTRY_VIRTIO_CRYPTO_OP 7

#define VIRTIO_CRYPTO_MAX_IV_SIZE 16
#define VIRTIO_CRYPTO_MAX_MSG_SIZE 512
#define VIRTIO_CRYPTO_MAX_SIGN_SIZE 1024
#define VIRTIO_CRYPTO_MAX_CIPHER_SIZE 1024

#define VIRTIO_CRYPTO_MAX_KEY_SIZE 256

#define VIRTIO_CRYPTO_MAX_CTRL_DATA 4096

extern uint8_t cryptodev_virtio_driver_id;

enum virtio_crypto_cmd_id {
	VIRTIO_CRYPTO_CMD_CIPHER = 0,
	VIRTIO_CRYPTO_CMD_AUTH = 1,
	VIRTIO_CRYPTO_CMD_CIPHER_HASH = 2,
	VIRTIO_CRYPTO_CMD_HASH_CIPHER = 3
};

struct virtio_crypto_op_cookie {
	struct virtio_crypto_op_data_req data_req;
	struct virtio_crypto_inhdr inhdr;
	struct vring_desc desc[NUM_ENTRY_VIRTIO_CRYPTO_OP];
	uint8_t iv[VIRTIO_CRYPTO_MAX_IV_SIZE];
	uint8_t message[VIRTIO_CRYPTO_MAX_MSG_SIZE];
	uint8_t sign[VIRTIO_CRYPTO_MAX_SIGN_SIZE];
	uint8_t cipher[VIRTIO_CRYPTO_MAX_CIPHER_SIZE];
};

/*
 * Control queue function prototype
 */
void virtio_crypto_ctrlq_start(struct rte_cryptodev *dev);

/*
 * Data queue function prototype
 */
void virtio_crypto_dataq_start(struct rte_cryptodev *dev);

int virtio_crypto_queue_setup(struct rte_cryptodev *dev,
		int queue_type,
		uint16_t vtpci_queue_idx,
		uint16_t nb_desc,
		int socket_id,
		struct virtqueue **pvq);

void virtio_crypto_queue_release(struct virtqueue *vq);

uint16_t virtio_crypto_pkt_tx_burst(void *tx_queue,
		struct rte_crypto_op **tx_pkts,
		uint16_t nb_pkts);

uint16_t virtio_crypto_pkt_rx_burst(void *tx_queue,
		struct rte_crypto_op **tx_pkts,
		uint16_t nb_pkts);

int crypto_virtio_dev_init(struct rte_cryptodev *cryptodev, uint64_t features,
		struct rte_pci_device *pci_dev);

#endif /* _VIRTIO_CRYPTODEV_H_ */
