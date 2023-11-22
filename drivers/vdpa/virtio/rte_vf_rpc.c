/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021, NVIDIA CORPORATION & AFFILIATES.
 */

/**
 * @file
 *
 * Device specific rpc lib
 */
#include <unistd.h>
#include "rte_vf_rpc.h"
#include "virtio_api.h"
#include "virtio_lm.h"
#include "virtio_vdpa.h"
#include "virtio_util.h"
#include <rte_vhost.h>

extern int virtio_vdpa_logtype;
#define RPC_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, virtio_vdpa_logtype, \
		"VIRTIO VDPA RPC %s(): " fmt "\n", __func__, ##args)

#ifndef PAGE_SIZE
#define PAGE_SIZE   (sysconf(_SC_PAGESIZE))
#endif

int
rte_vdpa_vf_dev_add(const char *vf_name, struct vdpa_vf_params *vf_params __rte_unused)
{
	char args[RTE_DEV_NAME_MAX_LEN];
	snprintf(args, RTE_DEV_NAME_MAX_LEN, "%s=%s", VIRTIO_ARG_VDPA, VIRTIO_ARG_VDPA_VALUE_VF);

	if (!vf_name)
		return -VFE_VDPA_ERR_NO_VF_NAME;

	if(virtio_vdpa_find_priv_resource_by_name(vf_name))
		return -VFE_VDPA_ERR_ADD_VF_ALREADY_ADD;

	return rte_eal_hotplug_add("pci", vf_name, args);
}

int
rte_vdpa_vf_dev_remove(const char *vf_name)
{
	if (!vf_name)
		return -VFE_VDPA_ERR_NO_VF_NAME;

	if(!virtio_vdpa_find_priv_resource_by_name(vf_name))
		return -VFE_VDPA_ERR_NO_VF_DEVICE;

	return rte_eal_hotplug_remove("pci", vf_name);
}

int
rte_vdpa_get_vf_list(const char *pf_name, struct vdpa_vf_params *vf_info,
		int max_vf_num)
{
	struct virtio_vdpa_pf_priv *priv;

	if (!pf_name)
		return -VFE_VDPA_ERR_NO_PF_NAME;

	if (!vf_info || max_vf_num <=0)
		return -ENOMEM;

	priv = rte_vdpa_get_mi_by_bdf(pf_name);
	if (!priv)
		return -VFE_VDPA_ERR_NO_PF_DEVICE;

	return virtio_vdpa_dev_pf_filter_dump(vf_info, max_vf_num, priv);
}

int
rte_vdpa_get_vf_info(const char *vf_name, struct vdpa_vf_params *vf_info)
{
	return virtio_vdpa_dev_vf_filter_dump(vf_name, vf_info);
}

static inline unsigned int
log2above(unsigned int v)
{
	unsigned int l;
	unsigned int r;

	for (l = 0, r = 0; (v >> 1); ++l, v >>= 1)
		r |= (v & 1);
	return l + r;
}

#define ONES32(size) \
	((size) ? (0xffffffff >> (32 - (size))) : 0xffffffff)

#define ROUND_DOWN_BITS(source, num_bits) \
	((source >> num_bits) << num_bits)

#define ROUND_UP_BITS(source, num_bits) \
	(ROUND_DOWN_BITS((source + ((1 << num_bits) - 1)), num_bits))

#define DIV_ROUND_UP_BITS(source, num_bits) \
	(ROUND_UP_BITS(source, num_bits) >> num_bits)

static int
virtio_vdpa_rpc_check_dirty_logging(uint64_t dirty_addr, uint32_t dirty_len,
		uint8_t *log_base, uint32_t log_size, /* (iova, len) in start loging sge[0] */
		uint16_t mode, uint32_t guest_page_size) /*vm used page size*/
{
	uint32_t log_log_page_size = log2above(guest_page_size);
	uint32_t page_offset = dirty_addr & ONES32(log_log_page_size);
	uint64_t start_page = (dirty_addr >> log_log_page_size);
	uint32_t num_pages, num_of_bytes = 0;
	uint8_t  written_data = 0;
	uint32_t byte_offset =	0;
	uint64_t start_byte = 0, i;

	num_pages = DIV_ROUND_UP_BITS(dirty_len + page_offset,
						      log_log_page_size);

	switch (mode) {
	case VIRTIO_M_DIRTY_TRACK_PUSH_BITMAP:
	case VIRTIO_M_DIRTY_TRACK_PULL_BITMAP:
		byte_offset = start_page & ONES32(3);
		num_of_bytes = DIV_ROUND_UP_BITS(num_pages + byte_offset, 3);
		start_byte = start_page >> 3;
		written_data = 0xff;
		break;
	case VIRTIO_M_DIRTY_TRACK_PUSH_BYTEMAP:
	case VIRTIO_M_DIRTY_TRACK_PULL_BYTEMAP:
		num_of_bytes = num_pages;
		start_byte = start_page;
		written_data = 0x1;
		break;
	default:
		RPC_LOG(ERR, "check_dirty_logging failed<<<<Unsurpported map mode>>>>");
		return -EOPNOTSUPP;
	}

	if ((start_byte + num_of_bytes) > log_size) {
		RPC_LOG(ERR, "check_dirty_logging failed<<<<Too many pages>>>>");
		return -EINVAL;
	}
	/*check*/
	for (i = 0; i < num_of_bytes; i++)
		if (log_base[start_byte + i] != written_data) {
			RPC_LOG(ERR, "check_dirty_logging failed<<<<Byte[%" PRIu64 "] should be 0x%x, actual is [%u]>>>>",
					start_byte + i, written_data, log_base[start_byte + i]);
			return -EINVAL;
		}

	return 0;
}

int
rte_vdpa_vf_dev_debug(const char *vf_name,
		struct vdpa_debug_vf_params *vf_debug_info)
{
	const struct rte_memzone *vdpa_dp_mz;
	struct virtio_vdpa_priv *vf_priv;
	uint64_t range_length;
	struct virtio_sge sge;
	uint32_t unit = 1;
	uint16_t num_vr;
	int ret, i;

	if (!vf_name || !vf_debug_info)
		return -EINVAL;

	vf_priv = virtio_vdpa_find_priv_resource_by_name(vf_name);
	if (vf_priv == NULL) {
		RPC_LOG(ERR, "Can't find dev: %s, ", vf_name);
		return -EINVAL;
	}

	RPC_LOG(INFO, "vdev: %s, cmd: %u", vf_name, vf_debug_info->test_type);

	RPC_LOG(DEBUG, ">>>>");
	switch(vf_debug_info->test_type) {
		case VDPA_DEBUG_CMD_GET_STATUS:
			ret = virtio_vdpa_cmd_get_status(vf_priv->pf_priv, vf_priv->vf_id, &vf_debug_info->lm_status);
			RPC_LOG(INFO, "vf debug: get lm status %d ret: %d", vf_debug_info->lm_status, ret);
			break;
		case VDPA_DEBUG_CMD_RUNNING:
			ret = virtio_vdpa_cmd_set_status(vf_priv->pf_priv, vf_priv->vf_id, VIRTIO_S_RUNNING);
			RPC_LOG(INFO, "vf debug: cmd_resume: VIRTIO_S_RUNNING, ret: %d", ret);
			break;
		case VDPA_DEBUG_CMD_QUIESCED:
			ret = virtio_vdpa_cmd_set_status(vf_priv->pf_priv, vf_priv->vf_id, VIRTIO_S_QUIESCED);
			RPC_LOG(INFO, "vf debug: cmd_suspend: VIRTIO_S_QUIESCED, ret: %d", ret);
			break;
		case VDPA_DEBUG_CMD_FREEZED:
			ret = virtio_vdpa_cmd_set_status(vf_priv->pf_priv, vf_priv->vf_id, VIRTIO_S_FREEZED);
			RPC_LOG(INFO, "vf debug: cmd_suspend: VIRTIO_S_FREEZED, ret: %d", ret);
			break;
		case VDPA_DEBUG_CMD_START_LOGGING:
			if (virtio_vdpa_max_phy_addr_get(vf_priv, &range_length)) {
				RPC_LOG(ERR, "<<<<Invalid VM memory size>>>>");
				return -EINVAL;
			}
			if ((vf_debug_info->test_mode == VIRTIO_M_DIRTY_TRACK_PUSH_BITMAP ||
					vf_debug_info->test_mode == VIRTIO_M_DIRTY_TRACK_PULL_BITMAP))
				unit = 8;
			sge.len = range_length/(PAGE_SIZE * unit);
			vdpa_dp_mz = virtio_vdpa_dev_dp_map_get(vf_priv, sge.len);
			sge.addr = (uint64_t)vdpa_dp_mz->addr;
			RPC_LOG(DEBUG, "range_length[%" PRIu64 "]/(page_size[%" PRIu64 "] * unit[%u]) = %u",
					range_length, PAGE_SIZE, unit, sge.len);
			if(!sge.len) {
				RPC_LOG(ERR, "<<<<Invalid map len>>>>");
				return -EINVAL;
			}
			ret = virtio_vdpa_cmd_dirty_page_start_track(vf_priv->pf_priv, vf_priv->vf_id,
					vf_debug_info->test_mode,
					PAGE_SIZE,
					0,
					range_length,
					1,
					&sge);
			RPC_LOG(INFO, "vf debug: START_LOGGING, ret: %d", ret);
			break;
		case VDPA_DEBUG_CMD_STOP_LOGGING:
			ret = virtio_vdpa_cmd_dirty_page_stop_track(vf_priv->pf_priv, vf_priv->vf_id, 0);
			RPC_LOG(INFO, "vf debug: STOP_LOGGING, ret: %d", ret);
			vdpa_dp_mz = virtio_vdpa_dev_dp_map_get(vf_priv, 0);

			if (vdpa_dp_mz == NULL) {
				RPC_LOG(ERR, "<<<<Dity map get fail, should start log first>>>>");
				return -EINVAL;
			}

			num_vr = rte_vhost_get_vring_num(vf_priv->vid);
			for (i = 0; i < num_vr; i++) {
				uint64_t dirty_addr;
				uint32_t dirty_len;
				ret = 0;
				/* Only check rx queue for net device */
				if ((vf_priv->pdev->id.device_id != VIRTIO_PCI_MODERN_DEVICEID_NET) || ((i % 2) == 0)) {
					if (!virtio_vdpa_dirty_desc_get(vf_priv, i, &dirty_addr, &dirty_len)) {
						ret = virtio_vdpa_rpc_check_dirty_logging(dirty_addr, dirty_len,
								vdpa_dp_mz->addr, vdpa_dp_mz->len,
								vf_debug_info->test_mode, PAGE_SIZE);
						if (ret)
							RPC_LOG(ERR, "Check queue %d last desc fail dirty_addr %" PRIx64
									" dirty_len %d ret: %d", i, dirty_addr, dirty_len, ret);
						else
							RPC_LOG(INFO, "Check queue %d last desc pass dirty_addr %" PRIx64
									" dirty_len %d ret: %d", i, dirty_addr, dirty_len, ret);

					}
				}

				if (!virtio_vdpa_used_vring_addr_get(vf_priv, i, &dirty_addr, &dirty_len)) {
					ret = virtio_vdpa_rpc_check_dirty_logging(dirty_addr, dirty_len,
							vdpa_dp_mz->addr, vdpa_dp_mz->len,
							vf_debug_info->test_mode, PAGE_SIZE);
					if (ret)
						RPC_LOG(ERR, "Check queue %d used ring fail dirty_addr %" PRIx64
								" dirty_len %d ret: %d", i, dirty_addr, dirty_len, ret);
					else
						RPC_LOG(INFO, "Check queue %d used ring pass dirty_addr %" PRIx64
								" dirty_len %d ret: %d", i, dirty_addr, dirty_len, ret);
				}
			}
			break;
		default:
			RPC_LOG(ERR, "Unsupported command %d", vf_debug_info->test_type);
			return -EINVAL;
	}

	return ret;
}
