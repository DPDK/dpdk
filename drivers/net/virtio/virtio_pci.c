/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
#include <stdint.h>

#include "virtio_pci.h"
#include "virtio_logs.h"
#include "virtqueue.h"

static void
legacy_read_dev_config(struct virtio_hw *hw, size_t offset,
		       void *dst, int length)
{
	uint64_t off;
	uint8_t *d;
	int size;

	off = VIRTIO_PCI_CONFIG(hw) + offset;
	for (d = dst; length > 0; d += size, off += size, length -= size) {
		if (length >= 4) {
			size = 4;
			*(uint32_t *)d = VIRTIO_READ_REG_4(hw, off);
		} else if (length >= 2) {
			size = 2;
			*(uint16_t *)d = VIRTIO_READ_REG_2(hw, off);
		} else {
			size = 1;
			*d = VIRTIO_READ_REG_1(hw, off);
		}
	}
}

static void
legacy_write_dev_config(struct virtio_hw *hw, size_t offset,
			const void *src, int length)
{
	uint64_t off;
	const uint8_t *s;
	int size;

	off = VIRTIO_PCI_CONFIG(hw) + offset;
	for (s = src; length > 0; s += size, off += size, length -= size) {
		if (length >= 4) {
			size = 4;
			VIRTIO_WRITE_REG_4(hw, off, *(const uint32_t *)s);
		} else if (length >= 2) {
			size = 2;
			VIRTIO_WRITE_REG_2(hw, off, *(const uint16_t *)s);
		} else {
			size = 1;
			VIRTIO_WRITE_REG_1(hw, off, *s);
		}
	}
}

static uint32_t
legacy_get_features(struct virtio_hw *hw)
{
	return VIRTIO_READ_REG_4(hw, VIRTIO_PCI_HOST_FEATURES);
}

static void
legacy_set_features(struct virtio_hw *hw, uint32_t features)
{
	VIRTIO_WRITE_REG_4(hw, VIRTIO_PCI_GUEST_FEATURES, features);
}

static uint8_t
legacy_get_status(struct virtio_hw *hw)
{
	return VIRTIO_READ_REG_1(hw, VIRTIO_PCI_STATUS);
}

static void
legacy_set_status(struct virtio_hw *hw, uint8_t status)
{
	VIRTIO_WRITE_REG_1(hw, VIRTIO_PCI_STATUS, status);
}

static void
legacy_reset(struct virtio_hw *hw)
{
	legacy_set_status(hw, VIRTIO_CONFIG_STATUS_RESET);
}

static uint8_t
legacy_get_isr(struct virtio_hw *hw)
{
	return VIRTIO_READ_REG_1(hw, VIRTIO_PCI_ISR);
}

/* Enable one vector (0) for Link State Intrerrupt */
static uint16_t
legacy_set_config_irq(struct virtio_hw *hw, uint16_t vec)
{
	VIRTIO_WRITE_REG_2(hw, VIRTIO_MSI_CONFIG_VECTOR, vec);
	return VIRTIO_READ_REG_2(hw, VIRTIO_MSI_CONFIG_VECTOR);
}

static uint16_t
legacy_get_queue_num(struct virtio_hw *hw, uint16_t queue_id)
{
	VIRTIO_WRITE_REG_2(hw, VIRTIO_PCI_QUEUE_SEL, queue_id);
	return VIRTIO_READ_REG_2(hw, VIRTIO_PCI_QUEUE_NUM);
}

static void
legacy_setup_queue(struct virtio_hw *hw, struct virtqueue *vq)
{
	VIRTIO_WRITE_REG_2(hw, VIRTIO_PCI_QUEUE_SEL, vq->vq_queue_index);

	VIRTIO_WRITE_REG_4(hw, VIRTIO_PCI_QUEUE_PFN,
		vq->mz->phys_addr >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);
}

static void
legacy_del_queue(struct virtio_hw *hw, struct virtqueue *vq)
{
	VIRTIO_WRITE_REG_2(hw, VIRTIO_PCI_QUEUE_SEL, vq->vq_queue_index);

	VIRTIO_WRITE_REG_4(hw, VIRTIO_PCI_QUEUE_PFN, 0);
}

static void
legacy_notify_queue(struct virtio_hw *hw, struct virtqueue *vq)
{
	VIRTIO_WRITE_REG_2(hw, VIRTIO_PCI_QUEUE_NOTIFY, vq->vq_queue_index);
}


static const struct virtio_pci_ops legacy_ops = {
	.read_dev_cfg	= legacy_read_dev_config,
	.write_dev_cfg	= legacy_write_dev_config,
	.reset		= legacy_reset,
	.get_status	= legacy_get_status,
	.set_status	= legacy_set_status,
	.get_features	= legacy_get_features,
	.set_features	= legacy_set_features,
	.get_isr	= legacy_get_isr,
	.set_config_irq	= legacy_set_config_irq,
	.get_queue_num	= legacy_get_queue_num,
	.setup_queue	= legacy_setup_queue,
	.del_queue	= legacy_del_queue,
	.notify_queue	= legacy_notify_queue,
};


void
vtpci_read_dev_config(struct virtio_hw *hw, size_t offset,
		      void *dst, int length)
{
	hw->vtpci_ops->read_dev_cfg(hw, offset, dst, length);
}

void
vtpci_write_dev_config(struct virtio_hw *hw, size_t offset,
		       const void *src, int length)
{
	hw->vtpci_ops->write_dev_cfg(hw, offset, src, length);
}

uint32_t
vtpci_negotiate_features(struct virtio_hw *hw, uint32_t host_features)
{
	uint32_t features;

	/*
	 * Limit negotiated features to what the driver, virtqueue, and
	 * host all support.
	 */
	features = host_features & hw->guest_features;
	hw->vtpci_ops->set_features(hw, features);

	return features;
}

void
vtpci_reset(struct virtio_hw *hw)
{
	hw->vtpci_ops->set_status(hw, VIRTIO_CONFIG_STATUS_RESET);
	/* flush status write */
	hw->vtpci_ops->get_status(hw);
}

void
vtpci_reinit_complete(struct virtio_hw *hw)
{
	vtpci_set_status(hw, VIRTIO_CONFIG_STATUS_DRIVER_OK);
}

void
vtpci_set_status(struct virtio_hw *hw, uint8_t status)
{
	if (status != VIRTIO_CONFIG_STATUS_RESET)
		status |= hw->vtpci_ops->get_status(hw);

	hw->vtpci_ops->set_status(hw, status);
}

uint8_t
vtpci_isr(struct virtio_hw *hw)
{
	return hw->vtpci_ops->get_isr(hw);
}


/* Enable one vector (0) for Link State Intrerrupt */
uint16_t
vtpci_irq_config(struct virtio_hw *hw, uint16_t vec)
{
	return hw->vtpci_ops->set_config_irq(hw, vec);
}

int
vtpci_init(struct rte_pci_device *dev __rte_unused, struct virtio_hw *hw)
{
	hw->vtpci_ops = &legacy_ops;

	return 0;
}
