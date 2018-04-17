/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 HUAWEI TECHNOLOGIES CO., LTD.
 */
#include "virtqueue.h"
#include "virtio_cryptodev.h"

static int
virtio_crypto_vring_start(struct virtqueue *vq)
{
	struct virtio_crypto_hw *hw = vq->hw;
	int i, size = vq->vq_nentries;
	struct vring *vr = &vq->vq_ring;
	uint8_t *ring_mem = vq->vq_ring_virt_mem;

	PMD_INIT_FUNC_TRACE();

	vring_init(vr, size, ring_mem, VIRTIO_PCI_VRING_ALIGN);
	vq->vq_desc_tail_idx = (uint16_t)(vq->vq_nentries - 1);
	vq->vq_free_cnt = vq->vq_nentries;

	/* Chain all the descriptors in the ring with an END */
	for (i = 0; i < size - 1; i++)
		vr->desc[i].next = (uint16_t)(i + 1);
	vr->desc[i].next = VQ_RING_DESC_CHAIN_END;

	/*
	 * Disable device(host) interrupting guest
	 */
	virtqueue_disable_intr(vq);

	/*
	 * Set guest physical address of the virtqueue
	 * in VIRTIO_PCI_QUEUE_PFN config register of device
	 * to share with the backend
	 */
	if (VTPCI_OPS(hw)->setup_queue(hw, vq) < 0) {
		VIRTIO_CRYPTO_INIT_LOG_ERR("setup_queue failed");
		return -EINVAL;
	}

	return 0;
}

void
virtio_crypto_ctrlq_start(struct rte_cryptodev *dev)
{
	struct virtio_crypto_hw *hw = dev->data->dev_private;

	if (hw->cvq) {
		virtio_crypto_vring_start(hw->cvq);
		VIRTQUEUE_DUMP((struct virtqueue *)hw->cvq);
	}
}

void
virtio_crypto_dataq_start(struct rte_cryptodev *dev)
{
	/*
	 * Start data vrings
	 * -	Setup vring structure for data queues
	 */
	uint16_t i;
	struct virtio_crypto_hw *hw = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	/* Start data vring. */
	for (i = 0; i < hw->max_dataqueues; i++) {
		virtio_crypto_vring_start(dev->data->queue_pairs[i]);
		VIRTQUEUE_DUMP((struct virtqueue *)dev->data->queue_pairs[i]);
	}
}

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
