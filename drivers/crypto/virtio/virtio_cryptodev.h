/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 HUAWEI TECHNOLOGIES CO., LTD.
 */

#ifndef _VIRTIO_CRYPTODEV_H_
#define _VIRTIO_CRYPTODEV_H_

#include <rte_cryptodev.h>

/* Features desired/implemented by this driver. */
#define VIRTIO_CRYPTO_PMD_GUEST_FEATURES (1ULL << VIRTIO_F_VERSION_1)

#define CRYPTODEV_NAME_VIRTIO_PMD crypto_virtio

uint16_t virtio_crypto_pkt_tx_burst(void *tx_queue,
		struct rte_crypto_op **tx_pkts,
		uint16_t nb_pkts);

uint16_t virtio_crypto_pkt_rx_burst(void *tx_queue,
		struct rte_crypto_op **tx_pkts,
		uint16_t nb_pkts);

#endif /* _VIRTIO_CRYPTODEV_H_ */
