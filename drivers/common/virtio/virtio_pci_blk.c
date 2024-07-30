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
#include "virtio_pci_state.h"

struct virtio_blk_dev_state {
	struct virtio_dev_common_state common_state;
	struct virtio_blk_config blk_dev_cfg;
	struct virtio_dev_queue_info q_info[];
} __rte_packed;

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
	PMD_INIT_LOG(INFO, "Virtio blk nr_vq is %d", hw->num_queues_blk);

	return hw->num_queues_blk;
}

static uint16_t
modern_blk_get_dev_cfg_size(void)
{
	return sizeof(struct virtio_blk_config);
}

static void *
modern_blk_get_queue_offset(void *state)
{
	struct virtio_blk_dev_state *state_blk = state;

	return state_blk->q_info;
}

static uint32_t
modern_blk_get_state_size(uint16_t num_queues)
{
	return sizeof(struct virtio_blk_config) + sizeof(struct virtio_dev_common_state) +
			num_queues * sizeof(struct virtio_dev_queue_info);
}

static void
modern_blk_dev_cfg_dump(void *f_hdr)
{
	struct virtio_field_hdr *tmp_f_hdr= f_hdr;
	const struct virtio_blk_config *dev_cfg;

	if (tmp_f_hdr->size < offsetof(struct virtio_blk_config, num_queues)) {
		PMD_DUMP_LOG(ERR, ">>> blk_config: state is truncated (%d < %lu)\n",
					tmp_f_hdr->size,
					offsetof(struct virtio_blk_config, num_queues));
		return;
	}

	PMD_DUMP_LOG(INFO, ">> virtio_blk_config, size:%d bytes \n", tmp_f_hdr->size);

	dev_cfg = (struct virtio_blk_config *)(tmp_f_hdr + 1);
	PMD_DUMP_LOG(INFO, ">>> virtio_blk_config type, capacity: %" PRIx64 " size_max: %u seg_max: %u blk_size: %u num_queues: %u\n",
		  dev_cfg->capacity, dev_cfg->size_max, dev_cfg->seg_max,
		  dev_cfg->blk_size, dev_cfg->num_queues);
}

const struct virtio_dev_specific_ops virtio_blk_dev_pci_modern_ops = {
	.get_queue_num = modern_blk_get_queue_num,
	.get_dev_cfg_size = modern_blk_get_dev_cfg_size,
	.get_queue_offset = modern_blk_get_queue_offset,
	.get_state_size = modern_blk_get_state_size,
	.dev_cfg_dump = modern_blk_dev_cfg_dump,
	.dev_cfg_compare = NULL,
	.dev_state_init = NULL,
};
