/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 HUAWEI TECHNOLOGIES CO., LTD.
 */
#include "virtio_cryptodev.h"

uint16_t
virtio_crypto_pkt_rx_burst(
	void *tx_queue __rte_unused,
	struct rte_crypto_op **rx_pkts __rte_unused,
	uint16_t nb_pkts __rte_unused)
{
	uint16_t nb_rx = 0;

	return nb_rx;
}

uint16_t
virtio_crypto_pkt_tx_burst(
	void *tx_queue __rte_unused,
	struct rte_crypto_op **tx_pkts __rte_unused,
	uint16_t nb_pkts __rte_unused)
{
	uint16_t nb_tx = 0;

	return nb_tx;
}
