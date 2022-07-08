/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include "ifcvf.h"
#include "ifcvf_osdep.h"

STATIC void *
get_cap_addr(struct ifcvf_hw *hw, struct ifcvf_pci_cap *cap)
{
	u8 bar = cap->bar;
	u32 length = cap->length;
	u32 offset = cap->offset;

	if (bar > IFCVF_PCI_MAX_RESOURCE - 1) {
		DEBUGOUT("invalid bar: %u\n", bar);
		return NULL;
	}

	if (offset + length < offset) {
		DEBUGOUT("offset(%u) + length(%u) overflows\n",
			offset, length);
		return NULL;
	}

	if (offset + length > hw->mem_resource[cap->bar].len) {
		DEBUGOUT("offset(%u) + length(%u) overflows bar length(%u)",
			offset, length, (u32)hw->mem_resource[cap->bar].len);
		return NULL;
	}

	return hw->mem_resource[bar].addr + offset;
}

int
ifcvf_init_hw(struct ifcvf_hw *hw, PCI_DEV *dev)
{
	int ret;
	u8 pos;
	struct ifcvf_pci_cap cap;

	ret = PCI_READ_CONFIG_BYTE(dev, &pos, PCI_CAPABILITY_LIST);
	if (ret < 0) {
		DEBUGOUT("failed to read pci capability list\n");
		return -1;
	}

	while (pos) {
		ret = PCI_READ_CONFIG_RANGE(dev, (u32 *)&cap,
				sizeof(cap), pos);
		if (ret < 0) {
			DEBUGOUT("failed to read cap at pos: %x", pos);
			break;
		}

		if (cap.cap_vndr != PCI_CAP_ID_VNDR)
			goto next;

		DEBUGOUT("cfg type: %u, bar: %u, offset: %u, "
				"len: %u\n", cap.cfg_type, cap.bar,
				cap.offset, cap.length);

		switch (cap.cfg_type) {
		case IFCVF_PCI_CAP_COMMON_CFG:
			hw->common_cfg = get_cap_addr(hw, &cap);
			break;
		case IFCVF_PCI_CAP_NOTIFY_CFG:
			ret = PCI_READ_CONFIG_DWORD(dev,
					&hw->notify_off_multiplier,
					pos + sizeof(cap));
			if (ret < 0) {
				DEBUGOUT("failed to read notify_off_multiplier\n");
				return -1;
			}
			hw->notify_base = get_cap_addr(hw, &cap);
			hw->notify_region = cap.bar;
			break;
		case IFCVF_PCI_CAP_ISR_CFG:
			hw->isr = get_cap_addr(hw, &cap);
			break;
		case IFCVF_PCI_CAP_DEVICE_CFG:
			hw->dev_cfg = get_cap_addr(hw, &cap);
			break;
		}
next:
		pos = cap.cap_next;
	}

	hw->lm_cfg = hw->mem_resource[4].addr;
	if (!hw->lm_cfg)
		WARNINGOUT("HW support live migration not support!\n");

	if (hw->common_cfg == NULL || hw->notify_base == NULL ||
			hw->isr == NULL || hw->dev_cfg == NULL) {
		DEBUGOUT("capability incomplete\n");
		return -1;
	}

	DEBUGOUT("capability mapping:\n"
		 "common cfg: %p\n"
		 "notify base: %p\n"
		 "isr cfg: %p\n"
		 "device cfg: %p\n"
		 "multiplier: %u\n",
		 hw->common_cfg, hw->notify_base, hw->isr, hw->dev_cfg,
		 hw->notify_off_multiplier);

	return 0;
}

STATIC u8
ifcvf_get_status(struct ifcvf_hw *hw)
{
	return IFCVF_READ_REG8(&hw->common_cfg->device_status);
}

STATIC void
ifcvf_set_status(struct ifcvf_hw *hw, u8 status)
{
	IFCVF_WRITE_REG8(status, &hw->common_cfg->device_status);
}

STATIC void
ifcvf_reset(struct ifcvf_hw *hw)
{
	ifcvf_set_status(hw, 0);

	/* flush status write */
	while (ifcvf_get_status(hw))
		msec_delay(1);
}

STATIC void
ifcvf_add_status(struct ifcvf_hw *hw, u8 status)
{
	if (status != 0)
		status |= ifcvf_get_status(hw);

	ifcvf_set_status(hw, status);
	ifcvf_get_status(hw);
}

u64
ifcvf_get_features(struct ifcvf_hw *hw)
{
	u32 features_lo, features_hi;
	struct ifcvf_pci_common_cfg *cfg = hw->common_cfg;

	IFCVF_WRITE_REG32(0, &cfg->device_feature_select);
	features_lo = IFCVF_READ_REG32(&cfg->device_feature);

	IFCVF_WRITE_REG32(1, &cfg->device_feature_select);
	features_hi = IFCVF_READ_REG32(&cfg->device_feature);

	return ((u64)features_hi << 32) | features_lo;
}

STATIC void
ifcvf_set_features(struct ifcvf_hw *hw, u64 features)
{
	struct ifcvf_pci_common_cfg *cfg = hw->common_cfg;

	IFCVF_WRITE_REG32(0, &cfg->guest_feature_select);
	IFCVF_WRITE_REG32(features & ((1ULL << 32) - 1), &cfg->guest_feature);

	IFCVF_WRITE_REG32(1, &cfg->guest_feature_select);
	IFCVF_WRITE_REG32(features >> 32, &cfg->guest_feature);
}

STATIC int
ifcvf_config_features(struct ifcvf_hw *hw)
{
	u64 host_features;

	host_features = ifcvf_get_features(hw);
	hw->req_features &= host_features;

	ifcvf_set_features(hw, hw->req_features);
	ifcvf_add_status(hw, IFCVF_CONFIG_STATUS_FEATURES_OK);

	if (!(ifcvf_get_status(hw) & IFCVF_CONFIG_STATUS_FEATURES_OK)) {
		DEBUGOUT("failed to set FEATURES_OK status\n");
		return -1;
	}

	return 0;
}

STATIC void
io_write64_twopart(u64 val, u32 *lo, u32 *hi)
{
	IFCVF_WRITE_REG32(val & ((1ULL << 32) - 1), lo);
	IFCVF_WRITE_REG32(val >> 32, hi);
}

STATIC int
ifcvf_hw_enable(struct ifcvf_hw *hw)
{
	struct ifcvf_pci_common_cfg *cfg;
	u8 *lm_cfg;
	u32 i;
	u16 notify_off;

	cfg = hw->common_cfg;
	lm_cfg = hw->lm_cfg;

	IFCVF_WRITE_REG16(0, &cfg->msix_config);
	if (IFCVF_READ_REG16(&cfg->msix_config) == IFCVF_MSI_NO_VECTOR) {
		DEBUGOUT("msix vec alloc failed for device config\n");
		return -1;
	}

	for (i = 0; i < hw->nr_vring; i++) {
		IFCVF_WRITE_REG16(i, &cfg->queue_select);
		io_write64_twopart(hw->vring[i].desc, &cfg->queue_desc_lo,
				&cfg->queue_desc_hi);
		io_write64_twopart(hw->vring[i].avail, &cfg->queue_avail_lo,
				&cfg->queue_avail_hi);
		io_write64_twopart(hw->vring[i].used, &cfg->queue_used_lo,
				&cfg->queue_used_hi);
		IFCVF_WRITE_REG16(hw->vring[i].size, &cfg->queue_size);

		if (lm_cfg) {
			*(u32 *)(lm_cfg + IFCVF_LM_RING_STATE_OFFSET +
					(i / 2) * IFCVF_LM_CFG_SIZE + (i % 2) * 4) =
				(u32)hw->vring[i].last_avail_idx |
				((u32)hw->vring[i].last_used_idx << 16);
		}

		IFCVF_WRITE_REG16(i + 1, &cfg->queue_msix_vector);
		if (IFCVF_READ_REG16(&cfg->queue_msix_vector) ==
				IFCVF_MSI_NO_VECTOR) {
			DEBUGOUT("queue %u, msix vec alloc failed\n",
					i);
			return -1;
		}

		notify_off = IFCVF_READ_REG16(&cfg->queue_notify_off);
		hw->notify_addr[i] = (void *)((u8 *)hw->notify_base +
				notify_off * hw->notify_off_multiplier);
		IFCVF_WRITE_REG16(1, &cfg->queue_enable);
	}

	return 0;
}

STATIC void
ifcvf_hw_disable(struct ifcvf_hw *hw)
{
	u32 i;
	struct ifcvf_pci_common_cfg *cfg;
	u32 ring_state;

	cfg = hw->common_cfg;

	IFCVF_WRITE_REG16(IFCVF_MSI_NO_VECTOR, &cfg->msix_config);
	for (i = 0; i < hw->nr_vring; i++) {
		IFCVF_WRITE_REG16(i, &cfg->queue_select);
		IFCVF_WRITE_REG16(0, &cfg->queue_enable);
		IFCVF_WRITE_REG16(IFCVF_MSI_NO_VECTOR, &cfg->queue_msix_vector);
		ring_state = *(u32 *)(hw->lm_cfg + IFCVF_LM_RING_STATE_OFFSET +
				(i / 2) * IFCVF_LM_CFG_SIZE + (i % 2) * 4);
		hw->vring[i].last_avail_idx = (u16)(ring_state >> 16);
		hw->vring[i].last_used_idx = (u16)(ring_state >> 16);
	}
}

int
ifcvf_start_hw(struct ifcvf_hw *hw)
{
	ifcvf_reset(hw);
	ifcvf_add_status(hw, IFCVF_CONFIG_STATUS_ACK);
	ifcvf_add_status(hw, IFCVF_CONFIG_STATUS_DRIVER);

	if (ifcvf_config_features(hw) < 0)
		return -1;

	if (ifcvf_hw_enable(hw) < 0)
		return -1;

	ifcvf_add_status(hw, IFCVF_CONFIG_STATUS_DRIVER_OK);
	return 0;
}

void
ifcvf_stop_hw(struct ifcvf_hw *hw)
{
	ifcvf_hw_disable(hw);
	ifcvf_reset(hw);
}

void
ifcvf_enable_logging(struct ifcvf_hw *hw, u64 log_base, u64 log_size)
{
	u8 *lm_cfg;

	lm_cfg = hw->lm_cfg;
	if (!lm_cfg)
		return;

	*(u32 *)(lm_cfg + IFCVF_LM_BASE_ADDR_LOW) =
		log_base & IFCVF_32_BIT_MASK;

	*(u32 *)(lm_cfg + IFCVF_LM_BASE_ADDR_HIGH) =
		(log_base >> 32) & IFCVF_32_BIT_MASK;

	*(u32 *)(lm_cfg + IFCVF_LM_END_ADDR_LOW) =
		(log_base + log_size) & IFCVF_32_BIT_MASK;

	*(u32 *)(lm_cfg + IFCVF_LM_END_ADDR_HIGH) =
		((log_base + log_size) >> 32) & IFCVF_32_BIT_MASK;

	*(u32 *)(lm_cfg + IFCVF_LM_LOGGING_CTRL) = IFCVF_LM_ENABLE_VF;
}

void
ifcvf_disable_logging(struct ifcvf_hw *hw)
{
	u8 *lm_cfg;

	lm_cfg = hw->lm_cfg;
	if (!lm_cfg)
		return;

	*(u32 *)(lm_cfg + IFCVF_LM_LOGGING_CTRL) = IFCVF_LM_DISABLE;
}

void
ifcvf_notify_queue(struct ifcvf_hw *hw, u16 qid)
{
	IFCVF_WRITE_REG16(qid, hw->notify_addr[qid]);
}

u8
ifcvf_get_notify_region(struct ifcvf_hw *hw)
{
	return hw->notify_region;
}

u64
ifcvf_get_queue_notify_off(struct ifcvf_hw *hw, int qid)
{
	return (u8 *)hw->notify_addr[qid] -
		(u8 *)hw->mem_resource[hw->notify_region].addr;
}
