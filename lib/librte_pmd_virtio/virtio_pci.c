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

void
vtpci_read_dev_config(struct virtio_hw *hw, uint64_t offset,
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

void
vtpci_write_dev_config(struct virtio_hw *hw, uint64_t offset,
		void *src, int length)
{
	uint64_t off;
	uint8_t *s;
	int size;

	off = VIRTIO_PCI_CONFIG(hw) + offset;
	for (s = src; length > 0; s += size, off += size, length -= size) {
		if (length >= 4) {
			size = 4;
			VIRTIO_WRITE_REG_4(hw, off, *(uint32_t *)s);
		} else if (length >= 2) {
			size = 2;
			VIRTIO_WRITE_REG_2(hw, off, *(uint16_t *)s);
		} else {
			size = 1;
			VIRTIO_WRITE_REG_1(hw, off, *s);
		}
	}
}

uint32_t
vtpci_negotiate_features(struct virtio_hw *hw, uint32_t guest_features)
{
	uint32_t features;
	/*
	 * Limit negotiated features to what the driver, virtqueue, and
	 * host all support.
	 */
	features = (hw->host_features) & guest_features;

	VIRTIO_WRITE_REG_4(hw, VIRTIO_PCI_GUEST_FEATURES, features);
	return features;
}


void
vtpci_reset(struct virtio_hw *hw)
{
	/*
	 * Setting the status to RESET sets the host device to
	 * the original, uninitialized state.
	 */
	vtpci_set_status(hw, VIRTIO_CONFIG_STATUS_RESET);
	vtpci_get_status(hw);
}

void
vtpci_reinit_complete(struct virtio_hw *hw)
{
	vtpci_set_status(hw, VIRTIO_CONFIG_STATUS_DRIVER_OK);
}

uint8_t
vtpci_get_status(struct virtio_hw *hw)
{
	return VIRTIO_READ_REG_1(hw, VIRTIO_PCI_STATUS);
}

void
vtpci_set_status(struct virtio_hw *hw, uint8_t status)
{
	if (status != VIRTIO_CONFIG_STATUS_RESET)
		status = (uint8_t)(status | vtpci_get_status(hw));

	VIRTIO_WRITE_REG_1(hw, VIRTIO_PCI_STATUS, status);
}
