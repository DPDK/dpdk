/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include <stdint.h>
#include <unistd.h>

#include <rte_io.h>
#include <rte_bus.h>
#include <rte_pci.h>
#include <rte_common.h>
#include <rte_cycles.h>

#include "zxdh_ethdev.h"
#include "zxdh_pci.h"
#include "zxdh_logs.h"
#include "zxdh_queue.h"

#define ZXDH_PMD_DEFAULT_GUEST_FEATURES   \
		(1ULL << ZXDH_NET_F_MRG_RXBUF | \
		1ULL << ZXDH_NET_F_STATUS    | \
		1ULL << ZXDH_NET_F_MQ        | \
		1ULL << ZXDH_F_ANY_LAYOUT    | \
		1ULL << ZXDH_F_VERSION_1     | \
		1ULL << ZXDH_F_RING_PACKED   | \
		1ULL << ZXDH_F_IN_ORDER      | \
		1ULL << ZXDH_F_NOTIFICATION_DATA | \
		1ULL << ZXDH_NET_F_MAC)

#define ZXDH_PMD_DEFAULT_HOST_FEATURES   \
	(1ULL << ZXDH_NET_F_MRG_RXBUF | \
	 1ULL << ZXDH_NET_F_STATUS    | \
	 1ULL << ZXDH_NET_F_MQ        | \
	 1ULL << ZXDH_F_ANY_LAYOUT    | \
	 1ULL << ZXDH_F_VERSION_1   | \
	 1ULL << ZXDH_F_RING_PACKED | \
	 1ULL << ZXDH_F_IN_ORDER    | \
	 1ULL << ZXDH_F_NOTIFICATION_DATA |\
	 1ULL << ZXDH_NET_F_MAC | \
	 1ULL << ZXDH_NET_F_CSUM |\
	 1ULL << ZXDH_NET_F_GUEST_CSUM |\
	 1ULL << ZXDH_NET_F_GUEST_TSO4 |\
	 1ULL << ZXDH_NET_F_GUEST_TSO6 |\
	 1ULL << ZXDH_NET_F_HOST_TSO4 |\
	 1ULL << ZXDH_NET_F_HOST_TSO6)

static void
zxdh_read_dev_config(struct zxdh_hw *hw, size_t offset,
		void *dst, int32_t length)
{
	int32_t i       = 0;
	uint8_t *p      = NULL;
	uint8_t old_gen = 0;
	uint8_t new_gen = 0;

	do {
		old_gen = rte_read8(&hw->common_cfg->config_generation);

		p = dst;
		for (i = 0;  i < length; i++)
			*p++ = rte_read8((uint8_t *)hw->dev_cfg + offset + i);

		new_gen = rte_read8(&hw->common_cfg->config_generation);
	} while (old_gen != new_gen);
}

static void
zxdh_write_dev_config(struct zxdh_hw *hw, size_t offset,
		const void *src, int32_t length)
{
	int32_t i = 0;
	const uint8_t *p = src;

	for (i = 0;  i < length; i++)
		rte_write8((*p++), (((uint8_t *)hw->dev_cfg) + offset + i));
}

static uint8_t
zxdh_get_status(struct zxdh_hw *hw)
{
	return rte_read8(&hw->common_cfg->device_status);
}

static void
zxdh_set_status(struct zxdh_hw *hw, uint8_t status)
{
	rte_write8(status, &hw->common_cfg->device_status);
}

static uint64_t
zxdh_get_features(struct zxdh_hw *hw)
{
	uint32_t features_lo = 0;
	uint32_t features_hi = 0;

	rte_write32(0, &hw->common_cfg->device_feature_select);
	features_lo = rte_read32(&hw->common_cfg->device_feature);

	rte_write32(1, &hw->common_cfg->device_feature_select);
	features_hi = rte_read32(&hw->common_cfg->device_feature);

	return ((uint64_t)features_hi << 32) | features_lo;
}

static void
zxdh_set_features(struct zxdh_hw *hw, uint64_t features)
{
	rte_write32(0, &hw->common_cfg->guest_feature_select);
	rte_write32(features & ((1ULL << 32) - 1), &hw->common_cfg->guest_feature);
	rte_write32(1, &hw->common_cfg->guest_feature_select);
	rte_write32(features >> 32, &hw->common_cfg->guest_feature);
}

static uint16_t
zxdh_set_config_irq(struct zxdh_hw *hw, uint16_t vec)
{
	rte_write16(vec, &hw->common_cfg->msix_config);
	return rte_read16(&hw->common_cfg->msix_config);
}

static uint16_t
zxdh_set_queue_irq(struct zxdh_hw *hw, struct zxdh_virtqueue *vq, uint16_t vec)
{
	rte_write16(vq->vq_queue_index, &hw->common_cfg->queue_select);
	rte_write16(vec, &hw->common_cfg->queue_msix_vector);
	return rte_read16(&hw->common_cfg->queue_msix_vector);
}

static uint8_t
zxdh_get_isr(struct zxdh_hw *hw)
{
	return rte_read8(hw->isr);
}

static uint16_t
zxdh_get_queue_num(struct zxdh_hw *hw, uint16_t queue_id)
{
	rte_write16(queue_id, &hw->common_cfg->queue_select);
	return rte_read16(&hw->common_cfg->queue_size);
}

static void
zxdh_set_queue_num(struct zxdh_hw *hw, uint16_t queue_id, uint16_t vq_size)
{
	rte_write16(queue_id, &hw->common_cfg->queue_select);
	rte_write16(vq_size, &hw->common_cfg->queue_size);
}

static int32_t
check_vq_phys_addr_ok(struct zxdh_virtqueue *vq)
{
	if ((vq->vq_ring_mem + vq->vq_ring_size - 1) >> (ZXDH_PCI_QUEUE_ADDR_SHIFT + 32)) {
		PMD_DRV_LOG(ERR, "vring address shouldn't be above 16TB!");
		return 0;
	}
	return 1;
}

static inline void
io_write64_twopart(uint64_t val, uint32_t *lo, uint32_t *hi)
{
	rte_write32(val & ((1ULL << 32) - 1), lo);
	rte_write32(val >> 32, hi);
}

static int32_t
zxdh_setup_queue(struct zxdh_hw *hw, struct zxdh_virtqueue *vq)
{
	uint64_t desc_addr  = 0;
	uint64_t avail_addr = 0;
	uint64_t used_addr  = 0;
	uint16_t notify_off = 0;

	if (!check_vq_phys_addr_ok(vq))
		return -1;

	desc_addr = vq->vq_ring_mem;
	avail_addr = desc_addr + vq->vq_nentries * sizeof(struct zxdh_vring_desc);
	if (zxdh_pci_packed_queue(vq->hw)) {
		used_addr = RTE_ALIGN_CEIL((avail_addr +
				sizeof(struct zxdh_vring_packed_desc_event)),
				ZXDH_PCI_VRING_ALIGN);
	} else {
		used_addr = RTE_ALIGN_CEIL(avail_addr + offsetof(struct zxdh_vring_avail,
						ring[vq->vq_nentries]), ZXDH_PCI_VRING_ALIGN);
	}

	rte_write16(vq->vq_queue_index, &hw->common_cfg->queue_select);

	io_write64_twopart(desc_addr, &hw->common_cfg->queue_desc_lo,
					   &hw->common_cfg->queue_desc_hi);
	io_write64_twopart(avail_addr, &hw->common_cfg->queue_avail_lo,
					   &hw->common_cfg->queue_avail_hi);
	io_write64_twopart(used_addr, &hw->common_cfg->queue_used_lo,
					   &hw->common_cfg->queue_used_hi);

	notify_off = rte_read16(&hw->common_cfg->queue_notify_off); /* default 0 */
	notify_off = 0;
	vq->notify_addr = (void *)((uint8_t *)hw->notify_base +
			notify_off * hw->notify_off_multiplier);

	rte_write16(1, &hw->common_cfg->queue_enable);

	return 0;
}

static void
zxdh_del_queue(struct zxdh_hw *hw, struct zxdh_virtqueue *vq)
{
	rte_write16(vq->vq_queue_index, &hw->common_cfg->queue_select);

	io_write64_twopart(0, &hw->common_cfg->queue_desc_lo,
					   &hw->common_cfg->queue_desc_hi);
	io_write64_twopart(0, &hw->common_cfg->queue_avail_lo,
					   &hw->common_cfg->queue_avail_hi);
	io_write64_twopart(0, &hw->common_cfg->queue_used_lo,
					   &hw->common_cfg->queue_used_hi);

	rte_write16(0, &hw->common_cfg->queue_enable);
}

static void
zxdh_notify_queue(struct zxdh_hw *hw, struct zxdh_virtqueue *vq)
{
	uint32_t notify_data = 0;

	if (!zxdh_pci_with_feature(hw, ZXDH_F_NOTIFICATION_DATA)) {
		rte_write16(vq->vq_queue_index, vq->notify_addr);
		return;
	}

	notify_data = ((uint32_t)vq->vq_avail_idx << 16) | vq->vq_queue_index;
	if (zxdh_pci_with_feature(hw, ZXDH_F_RING_PACKED) &&
			(vq->vq_packed.cached_flags & ZXDH_VRING_PACKED_DESC_F_AVAIL))
		notify_data |= RTE_BIT32(31);

	PMD_DRV_LOG(DEBUG, "queue:%d notify_data 0x%x notify_addr 0x%p",
				 vq->vq_queue_index, notify_data, vq->notify_addr);
	rte_write32(notify_data, vq->notify_addr);
}

const struct zxdh_pci_ops zxdh_dev_pci_ops = {
	.read_dev_cfg   = zxdh_read_dev_config,
	.write_dev_cfg  = zxdh_write_dev_config,
	.get_status     = zxdh_get_status,
	.set_status     = zxdh_set_status,
	.get_features   = zxdh_get_features,
	.set_features   = zxdh_set_features,
	.set_queue_irq  = zxdh_set_queue_irq,
	.set_config_irq = zxdh_set_config_irq,
	.get_isr        = zxdh_get_isr,
	.get_queue_num  = zxdh_get_queue_num,
	.set_queue_num  = zxdh_set_queue_num,
	.setup_queue    = zxdh_setup_queue,
	.del_queue      = zxdh_del_queue,
	.notify_queue   = zxdh_notify_queue,
};

uint8_t
zxdh_pci_isr(struct zxdh_hw *hw)
{
	return ZXDH_VTPCI_OPS(hw)->get_isr(hw);
}

uint16_t
zxdh_pci_get_features(struct zxdh_hw *hw)
{
	return ZXDH_VTPCI_OPS(hw)->get_features(hw);
}

void
zxdh_pci_reset(struct zxdh_hw *hw)
{
	PMD_DRV_LOG(INFO, "port %u device start reset, just wait", hw->port_id);
	uint32_t retry = 0;

	ZXDH_VTPCI_OPS(hw)->set_status(hw, ZXDH_CONFIG_STATUS_RESET);
	/* Flush status write and wait device ready max 3 seconds. */
	while (ZXDH_VTPCI_OPS(hw)->get_status(hw) != ZXDH_CONFIG_STATUS_RESET) {
		++retry;
		rte_delay_ms(1);
	}
	PMD_DRV_LOG(INFO, "port %u device reset %u ms done", hw->port_id, retry);
}

void
zxdh_pci_reinit_complete(struct zxdh_hw *hw)
{
	zxdh_pci_set_status(hw, ZXDH_CONFIG_STATUS_DRIVER_OK);
}

void
zxdh_pci_set_status(struct zxdh_hw *hw, uint8_t status)
{
	if (status != ZXDH_CONFIG_STATUS_RESET)
		status |= ZXDH_VTPCI_OPS(hw)->get_status(hw);

	ZXDH_VTPCI_OPS(hw)->set_status(hw, status);
}

static void
*get_cfg_addr(struct rte_pci_device *dev, struct zxdh_pci_cap *cap)
{
	uint8_t  bar    = cap->bar;
	uint32_t length = cap->length;
	uint32_t offset = cap->offset;

	if (bar >= PCI_MAX_RESOURCE) {
		PMD_DRV_LOG(ERR, "invalid bar: %u", bar);
		return NULL;
	}
	if (offset + length < offset) {
		PMD_DRV_LOG(ERR, "offset(%u) + length(%u) overflows", offset, length);
		return NULL;
	}
	if (offset + length > dev->mem_resource[bar].len) {
		PMD_DRV_LOG(ERR, "invalid cap: overflows bar space");
		return NULL;
	}
	uint8_t *base = dev->mem_resource[bar].addr;

	if (base == NULL) {
		PMD_DRV_LOG(ERR, "bar %u base addr is NULL", bar);
		return NULL;
	}
	return base + offset;
}

int32_t
zxdh_read_pci_caps(struct rte_pci_device *dev, struct zxdh_hw *hw)
{
	struct zxdh_pci_cap cap;
	uint8_t pos = 0;
	int32_t ret = 0;

	if (dev->mem_resource[0].addr == NULL) {
		PMD_DRV_LOG(ERR, "bar0 base addr is NULL");
		return -1;
	}

	hw->use_msix = zxdh_pci_msix_detect(dev);

	pos = rte_pci_find_capability(dev, RTE_PCI_CAP_ID_VNDR);
	while (pos) {
		ret = rte_pci_read_config(dev, &cap, sizeof(cap), pos);
		if (ret != sizeof(cap)) {
			PMD_DRV_LOG(ERR, "failed to read pci cap at pos: %x ret %d", pos, ret);
			break;
		}
		if (cap.cap_vndr != RTE_PCI_CAP_ID_VNDR) {
			PMD_DRV_LOG(DEBUG, "[%2x] skipping non VNDR cap id: %02x",
				pos, cap.cap_vndr);
			goto next;
		}
		PMD_DRV_LOG(DEBUG, "[%2x] cfg type: %u, bar: %u, offset: %04x, len: %u",
			pos, cap.cfg_type, cap.bar, cap.offset, cap.length);

		switch (cap.cfg_type) {
		case ZXDH_PCI_CAP_COMMON_CFG:
			hw->common_cfg = get_cfg_addr(dev, &cap);
			break;
		case ZXDH_PCI_CAP_NOTIFY_CFG: {
			ret = rte_pci_read_config(dev, &hw->notify_off_multiplier,
						4, pos + sizeof(cap));
			if (ret != 4)
				PMD_DRV_LOG(ERR,
					"failed to read notify_off_multiplier, ret %d", ret);
			else
				hw->notify_base = get_cfg_addr(dev, &cap);
			break;
		}
		case ZXDH_PCI_CAP_DEVICE_CFG:
			hw->dev_cfg = get_cfg_addr(dev, &cap);
			break;
		case ZXDH_PCI_CAP_ISR_CFG:
			hw->isr = get_cfg_addr(dev, &cap);
			break;
		case ZXDH_PCI_CAP_PCI_CFG: {
			hw->pcie_id = *(uint16_t *)&cap.padding[1];
			PMD_DRV_LOG(DEBUG, "get pcie id 0x%x", hw->pcie_id);

			if ((hw->pcie_id >> 11) & 0x1) /* PF */ {
				PMD_DRV_LOG(DEBUG, "EP %u PF %u",
					hw->pcie_id >> 12, (hw->pcie_id >> 8) & 0x7);
			} else { /* VF */
				PMD_DRV_LOG(DEBUG, "EP %u PF %u VF %u",
					hw->pcie_id >> 12,
					(hw->pcie_id >> 8) & 0x7,
					hw->pcie_id & 0xff);
			}
			break;
		}
		}
next:
	pos = cap.cap_next;
	}
	if (hw->common_cfg == NULL || hw->notify_base == NULL ||
		hw->dev_cfg == NULL || hw->isr == NULL) {
		PMD_DRV_LOG(ERR, "no zxdh pci device found");
		return -1;
	}
	return 0;
}

void
zxdh_pci_read_dev_config(struct zxdh_hw *hw, size_t offset, void *dst, int32_t length)
{
	ZXDH_VTPCI_OPS(hw)->read_dev_cfg(hw, offset, dst, length);
}

void zxdh_pci_write_dev_config(struct zxdh_hw *hw, size_t offset, const void *src, int32_t length)
{
	ZXDH_VTPCI_OPS(hw)->write_dev_cfg(hw, offset, src, length);
}

void
zxdh_get_pci_dev_config(struct zxdh_hw *hw)
{
	uint64_t guest_features = 0;
	uint64_t nego_features = 0;

	hw->host_features = ZXDH_PMD_DEFAULT_HOST_FEATURES;

	guest_features = (uint64_t)ZXDH_PMD_DEFAULT_GUEST_FEATURES;
	nego_features = guest_features & hw->host_features;

	hw->guest_features = nego_features;

	if (hw->guest_features & (1ULL << ZXDH_NET_F_MAC)) {
		zxdh_pci_read_dev_config(hw, offsetof(struct zxdh_net_config, mac),
				&hw->mac_addr, RTE_ETHER_ADDR_LEN);
	} else {
		rte_eth_random_addr(&hw->mac_addr[0]);
	}
}

enum zxdh_msix_status zxdh_pci_msix_detect(struct rte_pci_device *dev)
{
	uint16_t flags = 0;
	uint8_t pos = 0;
	int16_t ret = 0;

	pos = rte_pci_find_capability(dev, RTE_PCI_CAP_ID_MSIX);

	if (pos > 0) {
		ret = rte_pci_read_config(dev, &flags, 2, pos + RTE_PCI_MSIX_FLAGS);
		if (ret == 2 && flags & RTE_PCI_MSIX_FLAGS_ENABLE)
			return ZXDH_MSIX_ENABLED;
		else
			return ZXDH_MSIX_DISABLED;
	}
	return ZXDH_MSIX_NONE;
}
