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
#include "virtio_pci_state.h"

struct virtio_net_dev_state {
	struct virtio_dev_common_state common_state;
	struct virtio_net_config net_dev_cfg;
	struct virtio_field_hdr rx_mod_hdr;
	union virtnet_rx_mode rxomd;
	struct virtio_dev_queue_info q_info[];
} __rte_packed;

static uint16_t
modern_net_get_queue_num(struct virtio_hw *hw)
{
	uint16_t nr_vq;

	if (virtio_dev_with_feature(hw, VIRTIO_NET_F_MQ) ||
			virtio_dev_with_feature(hw, VIRTIO_NET_F_RSS)) {
		VIRTIO_OPS(hw)->read_dev_cfg(hw,
			offsetof(struct virtio_net_config, max_virtqueue_pairs),
			&hw->max_queue_pairs,
			sizeof(hw->max_queue_pairs));
	} else {
		PMD_INIT_LOG(WARNING,
				 "Neither VIRTIO_NET_F_MQ nor VIRTIO_NET_F_RSS are supported");
		hw->max_queue_pairs = 1;
	}

	nr_vq = hw->max_queue_pairs * 2;

	PMD_INIT_LOG(INFO, "Virtio net nr_vq is %d", nr_vq);
	return nr_vq;

}

static uint16_t
modern_net_get_dev_cfg_size(void)
{
	return sizeof(struct virtio_net_config);
}

static void *
modern_net_get_queue_offset(void *state)
{
	struct virtio_net_dev_state *state_net = state;

	return state_net->q_info;
}

static uint32_t
modern_net_get_state_size(uint16_t num_queues)
{
	return sizeof(struct virtio_net_config) + sizeof(struct virtio_dev_common_state) +
			num_queues * sizeof(struct virtio_dev_queue_info);
}

static void
modern_net_dev_cfg_dump(void *f_hdr)
{
	struct virtio_field_hdr *tmp_f_hdr= f_hdr;
	const struct virtio_net_config *dev_cfg;

	if (tmp_f_hdr->size < sizeof(struct virtio_net_config)) {
		PMD_DUMP_LOG(ERR, ">> net_config: state is truncated (%d < %lu)\n",
					tmp_f_hdr->size,
					sizeof(struct virtio_net_config));
		return;
	}

	dev_cfg = (struct virtio_net_config *)(tmp_f_hdr + 1);
	PMD_DUMP_LOG(INFO, ">> virtio_net_config, size:%d bytes \n", tmp_f_hdr->size);

	PMD_DUMP_LOG(INFO, ">>> mac: %02X:%02X:%02X:%02X:%02X:%02X status: 0x%x max_virtqueue_pairs: 0x%x mtu: 0x%x\n",
		  dev_cfg->mac[5], dev_cfg->mac[4], dev_cfg->mac[3], dev_cfg->mac[2], dev_cfg->mac[1], dev_cfg->mac[0],
		  dev_cfg->status, dev_cfg->max_virtqueue_pairs, dev_cfg->mtu);
}

static bool
modern_net_dev_cfg_compare(struct virtio_hw *hw, void *f_hdr, void *f_hdr_r)
{
	struct virtio_field_hdr *tmp_f_hdr= f_hdr, *tmp_f_hdr_r= f_hdr_r;
	const struct virtio_net_config *dev_cfg, *dev_cfg_r;
	int i;

	dev_cfg = (struct virtio_net_config *)(tmp_f_hdr + 1);
	dev_cfg_r = (struct virtio_net_config *)(tmp_f_hdr_r + 1);

	for(i = 0; i < RTE_ETHER_ADDR_LEN; i++) {
		if (dev_cfg->mac[i] != dev_cfg_r->mac[i]) {
			PMD_DUMP_LOG(INFO, ">> virtio_net_config, mac diff:%d -%d i:%d bytes\n",
				dev_cfg->mac[i], dev_cfg_r->mac[i], i);
			return false;
		}
	}

	if ((dev_cfg->status != dev_cfg_r->status) ||
	    (dev_cfg->max_virtqueue_pairs != dev_cfg_r->max_virtqueue_pairs) ||
	    (dev_cfg->mtu != dev_cfg_r->mtu)) {
		PMD_DUMP_LOG(INFO, ">> virtio_net_config, diff:%d-%d %d-%d %d-%d\n",
			dev_cfg->status, dev_cfg_r->status,
			dev_cfg->max_virtqueue_pairs, dev_cfg_r->max_virtqueue_pairs, dev_cfg->mtu, dev_cfg_r->mtu);
		return false;
	}
	if (virtio_with_feature(hw, VIRTIO_NET_F_SPEED_DUPLEX)) {
		if ((dev_cfg->speed != dev_cfg_r->speed) ||
		    (dev_cfg->duplex != dev_cfg_r->duplex)) {
			PMD_DUMP_LOG(INFO, ">> virtio_net_config, speed diff:%d-%d %d-%d\n",
				dev_cfg->speed, dev_cfg_r->speed, dev_cfg->duplex, dev_cfg_r->duplex);
			return false;
		}
	}

	return true;
}

static void
modern_net_dev_state_init(void *state)
{
	struct virtio_net_dev_state *state_net = state;

	/*Set to promisc mod, this is WA to fix ipv6 issue RM:3229948*/
	state_net->rx_mod_hdr.type = rte_cpu_to_le_32(VIRTIO_NET_RX_MODE_CFG);
	state_net->rx_mod_hdr.size = rte_cpu_to_le_32(sizeof(union virtnet_rx_mode));
	state_net->rxomd.promisc = 1;
	state_net->rxomd.val = rte_cpu_to_le_32(state_net->rxomd.val);
}

const struct virtio_dev_specific_ops virtio_net_dev_pci_modern_ops = {
	.get_queue_num = modern_net_get_queue_num,
	.get_dev_cfg_size = modern_net_get_dev_cfg_size,
	.get_queue_offset = modern_net_get_queue_offset,
	.get_state_size = modern_net_get_state_size,
	.dev_cfg_dump = modern_net_dev_cfg_dump,
	.dev_cfg_compare = modern_net_dev_cfg_compare,
	.dev_state_init = modern_net_dev_state_init,
};
