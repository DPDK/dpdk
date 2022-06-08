/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */
#ifdef RTE_EXEC_ENV_LINUX
 #include <dirent.h>
 #include <fcntl.h>
#endif

#include <rte_io.h>
#include <rte_bus.h>

#include "virtio_pci.h"
#include "virtio_logs.h"
#include "virtqueue.h"

static uint16_t
modern_net_get_queue_num(struct virtio_hw *hw)
{
	uint16_t nr_vq;

	if (virtio_with_feature(hw, VIRTIO_NET_F_MQ) ||
			virtio_with_feature(hw, VIRTIO_NET_F_RSS)) {
		VIRTIO_OPS(hw)->read_dev_cfg(hw,
			offsetof(struct virtio_net_config, max_virtqueue_pairs),
			&hw->max_queue_pairs,
			sizeof(hw->max_queue_pairs));
	} else {
		PMD_INIT_LOG(DEBUG,
				 "Neither VIRTIO_NET_F_MQ nor VIRTIO_NET_F_RSS are supported");
		hw->max_queue_pairs = 1;
	}

	nr_vq = hw->max_queue_pairs * 2;

	PMD_INIT_LOG(DEBUG, "Virtio net nr_vq is %d", nr_vq);
	return nr_vq;

}

const struct virtio_dev_specific_ops virtio_net_dev_pci_modern_ops = {
	.get_queue_num = modern_net_get_queue_num,
};
