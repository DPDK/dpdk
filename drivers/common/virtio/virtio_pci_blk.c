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
#include "virtio_blk.h"

static uint16_t
modern_blk_get_queue_num(struct virtio_hw *hw)
{
	if (virtio_dev_with_feature(hw, VIRTIO_BLK_F_MQ)) {
			VIRTIO_OPS(hw)->read_dev_cfg(hw,
					offsetof(struct virtio_blk_config, num_queues),
					&hw->num_queues_blk,
					sizeof(hw->num_queues_blk));
	} else {
			hw->num_queues_blk = 1;
	}
	PMD_INIT_LOG(DEBUG, "Virtio blk nr_vq is %d", hw->num_queues_blk);

	return hw->num_queues_blk;
}

const struct virtio_dev_specific_ops virtio_blk_dev_pci_modern_ops = {
	.get_queue_num = modern_blk_get_queue_num,
};
