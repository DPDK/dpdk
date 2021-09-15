/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 * Copyright(c) 2020 Red Hat, Inc.
 */

#include <unistd.h>

#include "virtio.h"
#include "virtio_logs.h"

uint64_t
virtio_negotiate_features(struct virtio_hw *hw, uint64_t host_features)
{
	uint64_t features;

	/*
	 * Limit negotiated features to what the driver, virtqueue, and
	 * host all support.
	 */
	features = host_features & hw->guest_features;
	VIRTIO_OPS(hw)->set_features(hw, features);

	return features;
}


void
virtio_read_dev_config(struct virtio_hw *hw, size_t offset,
		      void *dst, int length)
{
	VIRTIO_OPS(hw)->read_dev_cfg(hw, offset, dst, length);
}

void
virtio_write_dev_config(struct virtio_hw *hw, size_t offset,
		       const void *src, int length)
{
	VIRTIO_OPS(hw)->write_dev_cfg(hw, offset, src, length);
}

void
virtio_reset(struct virtio_hw *hw)
{
	uint32_t retry = 0;

	VIRTIO_OPS(hw)->set_status(hw, VIRTIO_CONFIG_STATUS_RESET);
	/* Flush status write and wait device ready max 3 seconds. */
	while (VIRTIO_OPS(hw)->get_status(hw) != VIRTIO_CONFIG_STATUS_RESET) {
		if (retry++ > 3000) {
			PMD_INIT_LOG(WARNING, "port %u device reset timeout", hw->port_id);
			break;
		}
		usleep(1000L);
	}
}

void
virtio_reinit_complete(struct virtio_hw *hw)
{
	virtio_set_status(hw, VIRTIO_CONFIG_STATUS_DRIVER_OK);
}

void
virtio_set_status(struct virtio_hw *hw, uint8_t status)
{
	if (status != VIRTIO_CONFIG_STATUS_RESET)
		status |= VIRTIO_OPS(hw)->get_status(hw);

	VIRTIO_OPS(hw)->set_status(hw, status);
}

uint8_t
virtio_get_status(struct virtio_hw *hw)
{
	return VIRTIO_OPS(hw)->get_status(hw);
}

uint8_t
virtio_get_isr(struct virtio_hw *hw)
{
	return VIRTIO_OPS(hw)->get_isr(hw);
}
