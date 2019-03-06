/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 HUAWEI TECHNOLOGIES CO., LTD.
 */

#include <stdint.h>

#ifdef RTE_EXEC_ENV_LINUX
 #include <dirent.h>
 #include <fcntl.h>
#endif

#include <rte_io.h>
#include <rte_bus.h>

#include "virtio_pci.h"
#include "virtqueue.h"

/*
 * Following macros are derived from linux/pci_regs.h, however,
 * we can't simply include that header here, as there is no such
 * file for non-Linux platform.
 */
#define PCI_CAPABILITY_LIST	0x34
#define PCI_CAP_ID_VNDR		0x09
#define PCI_CAP_ID_MSIX		0x11

/*
 * The remaining space is defined by each driver as the per-driver
 * configuration space.
 */
#define VIRTIO_PCI_CONFIG(hw) \
		(((hw)->use_msix == VIRTIO_MSIX_ENABLED) ? 24 : 20)

struct virtio_hw_internal virtio_hw_internal[RTE_MAX_VIRTIO_CRYPTO];

static inline int
check_vq_phys_addr_ok(struct virtqueue *vq)
{
	/* Virtio PCI device VIRTIO_PCI_QUEUE_PF register is 32bit,
	 * and only accepts 32 bit page frame number.
	 * Check if the allocated physical memory exceeds 16TB.
	 */
	if ((vq->vq_ring_mem + vq->vq_ring_size - 1) >>
			(VIRTIO_PCI_QUEUE_ADDR_SHIFT + 32)) {
		VIRTIO_CRYPTO_INIT_LOG_ERR("vring address shouldn't be above 16TB!");
		return 0;
	}

	return 1;
}

static inline void
io_write64_twopart(uint64_t val, uint32_t *lo, uint32_t *hi)
{
	rte_write32(val & ((1ULL << 32) - 1), lo);
	rte_write32(val >> 32,		     hi);
}

static void
modern_read_dev_config(struct virtio_crypto_hw *hw, size_t offset,
		       void *dst, int length)
{
	int i;
	uint8_t *p;
	uint8_t old_gen, new_gen;

	do {
		old_gen = rte_read8(&hw->common_cfg->config_generation);

		p = dst;
		for (i = 0;  i < length; i++)
			*p++ = rte_read8((uint8_t *)hw->dev_cfg + offset + i);

		new_gen = rte_read8(&hw->common_cfg->config_generation);
	} while (old_gen != new_gen);
}

static void
modern_write_dev_config(struct virtio_crypto_hw *hw, size_t offset,
			const void *src, int length)
{
	int i;
	const uint8_t *p = src;

	for (i = 0;  i < length; i++)
		rte_write8((*p++), (((uint8_t *)hw->dev_cfg) + offset + i));
}

static uint64_t
modern_get_features(struct virtio_crypto_hw *hw)
{
	uint32_t features_lo, features_hi;

	rte_write32(0, &hw->common_cfg->device_feature_select);
	features_lo = rte_read32(&hw->common_cfg->device_feature);

	rte_write32(1, &hw->common_cfg->device_feature_select);
	features_hi = rte_read32(&hw->common_cfg->device_feature);

	return ((uint64_t)features_hi << 32) | features_lo;
}

static void
modern_set_features(struct virtio_crypto_hw *hw, uint64_t features)
{
	rte_write32(0, &hw->common_cfg->guest_feature_select);
	rte_write32(features & ((1ULL << 32) - 1),
		    &hw->common_cfg->guest_feature);

	rte_write32(1, &hw->common_cfg->guest_feature_select);
	rte_write32(features >> 32,
		    &hw->common_cfg->guest_feature);
}

static uint8_t
modern_get_status(struct virtio_crypto_hw *hw)
{
	return rte_read8(&hw->common_cfg->device_status);
}

static void
modern_set_status(struct virtio_crypto_hw *hw, uint8_t status)
{
	rte_write8(status, &hw->common_cfg->device_status);
}

static void
modern_reset(struct virtio_crypto_hw *hw)
{
	modern_set_status(hw, VIRTIO_CONFIG_STATUS_RESET);
	modern_get_status(hw);
}

static uint8_t
modern_get_isr(struct virtio_crypto_hw *hw)
{
	return rte_read8(hw->isr);
}

static uint16_t
modern_set_config_irq(struct virtio_crypto_hw *hw, uint16_t vec)
{
	rte_write16(vec, &hw->common_cfg->msix_config);
	return rte_read16(&hw->common_cfg->msix_config);
}

static uint16_t
modern_set_queue_irq(struct virtio_crypto_hw *hw, struct virtqueue *vq,
		uint16_t vec)
{
	rte_write16(vq->vq_queue_index, &hw->common_cfg->queue_select);
	rte_write16(vec, &hw->common_cfg->queue_msix_vector);
	return rte_read16(&hw->common_cfg->queue_msix_vector);
}

static uint16_t
modern_get_queue_num(struct virtio_crypto_hw *hw, uint16_t queue_id)
{
	rte_write16(queue_id, &hw->common_cfg->queue_select);
	return rte_read16(&hw->common_cfg->queue_size);
}

static int
modern_setup_queue(struct virtio_crypto_hw *hw, struct virtqueue *vq)
{
	uint64_t desc_addr, avail_addr, used_addr;
	uint16_t notify_off;

	if (!check_vq_phys_addr_ok(vq))
		return -1;

	desc_addr = vq->vq_ring_mem;
	avail_addr = desc_addr + vq->vq_nentries * sizeof(struct vring_desc);
	used_addr = RTE_ALIGN_CEIL(avail_addr + offsetof(struct vring_avail,
							 ring[vq->vq_nentries]),
				   VIRTIO_PCI_VRING_ALIGN);

	rte_write16(vq->vq_queue_index, &hw->common_cfg->queue_select);

	io_write64_twopart(desc_addr, &hw->common_cfg->queue_desc_lo,
				      &hw->common_cfg->queue_desc_hi);
	io_write64_twopart(avail_addr, &hw->common_cfg->queue_avail_lo,
				       &hw->common_cfg->queue_avail_hi);
	io_write64_twopart(used_addr, &hw->common_cfg->queue_used_lo,
				      &hw->common_cfg->queue_used_hi);

	notify_off = rte_read16(&hw->common_cfg->queue_notify_off);
	vq->notify_addr = (void *)((uint8_t *)hw->notify_base +
				notify_off * hw->notify_off_multiplier);

	rte_write16(1, &hw->common_cfg->queue_enable);

	VIRTIO_CRYPTO_INIT_LOG_DBG("queue %u addresses:", vq->vq_queue_index);
	VIRTIO_CRYPTO_INIT_LOG_DBG("\t desc_addr: %" PRIx64, desc_addr);
	VIRTIO_CRYPTO_INIT_LOG_DBG("\t aval_addr: %" PRIx64, avail_addr);
	VIRTIO_CRYPTO_INIT_LOG_DBG("\t used_addr: %" PRIx64, used_addr);
	VIRTIO_CRYPTO_INIT_LOG_DBG("\t notify addr: %p (notify offset: %u)",
		vq->notify_addr, notify_off);

	return 0;
}

static void
modern_del_queue(struct virtio_crypto_hw *hw, struct virtqueue *vq)
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
modern_notify_queue(struct virtio_crypto_hw *hw __rte_unused,
		struct virtqueue *vq)
{
	rte_write16(vq->vq_queue_index, vq->notify_addr);
}

const struct virtio_pci_ops virtio_crypto_modern_ops = {
	.read_dev_cfg	= modern_read_dev_config,
	.write_dev_cfg	= modern_write_dev_config,
	.reset		= modern_reset,
	.get_status	= modern_get_status,
	.set_status	= modern_set_status,
	.get_features	= modern_get_features,
	.set_features	= modern_set_features,
	.get_isr	= modern_get_isr,
	.set_config_irq	= modern_set_config_irq,
	.set_queue_irq  = modern_set_queue_irq,
	.get_queue_num	= modern_get_queue_num,
	.setup_queue	= modern_setup_queue,
	.del_queue	= modern_del_queue,
	.notify_queue	= modern_notify_queue,
};

void
vtpci_read_cryptodev_config(struct virtio_crypto_hw *hw, size_t offset,
		void *dst, int length)
{
	VTPCI_OPS(hw)->read_dev_cfg(hw, offset, dst, length);
}

void
vtpci_write_cryptodev_config(struct virtio_crypto_hw *hw, size_t offset,
		const void *src, int length)
{
	VTPCI_OPS(hw)->write_dev_cfg(hw, offset, src, length);
}

uint64_t
vtpci_cryptodev_negotiate_features(struct virtio_crypto_hw *hw,
		uint64_t host_features)
{
	uint64_t features;

	/*
	 * Limit negotiated features to what the driver, virtqueue, and
	 * host all support.
	 */
	features = host_features & hw->guest_features;
	VTPCI_OPS(hw)->set_features(hw, features);

	return features;
}

void
vtpci_cryptodev_reset(struct virtio_crypto_hw *hw)
{
	VTPCI_OPS(hw)->set_status(hw, VIRTIO_CONFIG_STATUS_RESET);
	/* flush status write */
	VTPCI_OPS(hw)->get_status(hw);
}

void
vtpci_cryptodev_reinit_complete(struct virtio_crypto_hw *hw)
{
	vtpci_cryptodev_set_status(hw, VIRTIO_CONFIG_STATUS_DRIVER_OK);
}

void
vtpci_cryptodev_set_status(struct virtio_crypto_hw *hw, uint8_t status)
{
	if (status != VIRTIO_CONFIG_STATUS_RESET)
		status |= VTPCI_OPS(hw)->get_status(hw);

	VTPCI_OPS(hw)->set_status(hw, status);
}

uint8_t
vtpci_cryptodev_get_status(struct virtio_crypto_hw *hw)
{
	return VTPCI_OPS(hw)->get_status(hw);
}

uint8_t
vtpci_cryptodev_isr(struct virtio_crypto_hw *hw)
{
	return VTPCI_OPS(hw)->get_isr(hw);
}

static void *
get_cfg_addr(struct rte_pci_device *dev, struct virtio_pci_cap *cap)
{
	uint8_t  bar    = cap->bar;
	uint32_t length = cap->length;
	uint32_t offset = cap->offset;
	uint8_t *base;

	if (bar >= PCI_MAX_RESOURCE) {
		VIRTIO_CRYPTO_INIT_LOG_ERR("invalid bar: %u", bar);
		return NULL;
	}

	if (offset + length < offset) {
		VIRTIO_CRYPTO_INIT_LOG_ERR("offset(%u) + length(%u) overflows",
			offset, length);
		return NULL;
	}

	if (offset + length > dev->mem_resource[bar].len) {
		VIRTIO_CRYPTO_INIT_LOG_ERR(
			"invalid cap: overflows bar space: %u > %" PRIu64,
			offset + length, dev->mem_resource[bar].len);
		return NULL;
	}

	base = dev->mem_resource[bar].addr;
	if (base == NULL) {
		VIRTIO_CRYPTO_INIT_LOG_ERR("bar %u base addr is NULL", bar);
		return NULL;
	}

	return base + offset;
}

#define PCI_MSIX_ENABLE 0x8000

static int
virtio_read_caps(struct rte_pci_device *dev, struct virtio_crypto_hw *hw)
{
	uint8_t pos;
	struct virtio_pci_cap cap;
	int ret;

	if (rte_pci_map_device(dev)) {
		VIRTIO_CRYPTO_INIT_LOG_DBG("failed to map pci device!");
		return -1;
	}

	ret = rte_pci_read_config(dev, &pos, 1, PCI_CAPABILITY_LIST);
	if (ret < 0) {
		VIRTIO_CRYPTO_INIT_LOG_DBG("failed to read pci capability list");
		return -1;
	}

	while (pos) {
		ret = rte_pci_read_config(dev, &cap, sizeof(cap), pos);
		if (ret < 0) {
			VIRTIO_CRYPTO_INIT_LOG_ERR(
				"failed to read pci cap at pos: %x", pos);
			break;
		}

		if (cap.cap_vndr == PCI_CAP_ID_MSIX) {
			/* Transitional devices would also have this capability,
			 * that's why we also check if msix is enabled.
			 * 1st byte is cap ID; 2nd byte is the position of next
			 * cap; next two bytes are the flags.
			 */
			uint16_t flags = ((uint16_t *)&cap)[1];

			if (flags & PCI_MSIX_ENABLE)
				hw->use_msix = VIRTIO_MSIX_ENABLED;
			else
				hw->use_msix = VIRTIO_MSIX_DISABLED;
		}

		if (cap.cap_vndr != PCI_CAP_ID_VNDR) {
			VIRTIO_CRYPTO_INIT_LOG_DBG(
				"[%2x] skipping non VNDR cap id: %02x",
				pos, cap.cap_vndr);
			goto next;
		}

		VIRTIO_CRYPTO_INIT_LOG_DBG(
			"[%2x] cfg type: %u, bar: %u, offset: %04x, len: %u",
			pos, cap.cfg_type, cap.bar, cap.offset, cap.length);

		switch (cap.cfg_type) {
		case VIRTIO_PCI_CAP_COMMON_CFG:
			hw->common_cfg = get_cfg_addr(dev, &cap);
			break;
		case VIRTIO_PCI_CAP_NOTIFY_CFG:
			rte_pci_read_config(dev, &hw->notify_off_multiplier,
					4, pos + sizeof(cap));
			hw->notify_base = get_cfg_addr(dev, &cap);
			break;
		case VIRTIO_PCI_CAP_DEVICE_CFG:
			hw->dev_cfg = get_cfg_addr(dev, &cap);
			break;
		case VIRTIO_PCI_CAP_ISR_CFG:
			hw->isr = get_cfg_addr(dev, &cap);
			break;
		}

next:
		pos = cap.cap_next;
	}

	if (hw->common_cfg == NULL || hw->notify_base == NULL ||
	    hw->dev_cfg == NULL    || hw->isr == NULL) {
		VIRTIO_CRYPTO_INIT_LOG_INFO("no modern virtio pci device found.");
		return -1;
	}

	VIRTIO_CRYPTO_INIT_LOG_INFO("found modern virtio pci device.");

	VIRTIO_CRYPTO_INIT_LOG_DBG("common cfg mapped at: %p", hw->common_cfg);
	VIRTIO_CRYPTO_INIT_LOG_DBG("device cfg mapped at: %p", hw->dev_cfg);
	VIRTIO_CRYPTO_INIT_LOG_DBG("isr cfg mapped at: %p", hw->isr);
	VIRTIO_CRYPTO_INIT_LOG_DBG("notify base: %p, notify off multiplier: %u",
		hw->notify_base, hw->notify_off_multiplier);

	return 0;
}

/*
 * Return -1:
 *   if there is error mapping with VFIO/UIO.
 *   if port map error when driver type is KDRV_NONE.
 *   if whitelisted but driver type is KDRV_UNKNOWN.
 * Return 1 if kernel driver is managing the device.
 * Return 0 on success.
 */
int
vtpci_cryptodev_init(struct rte_pci_device *dev, struct virtio_crypto_hw *hw)
{
	/*
	 * Try if we can succeed reading virtio pci caps, which exists
	 * only on modern pci device. If failed, we fallback to legacy
	 * virtio handling.
	 */
	if (virtio_read_caps(dev, hw) == 0) {
		VIRTIO_CRYPTO_INIT_LOG_INFO("modern virtio pci detected.");
		virtio_hw_internal[hw->dev_id].vtpci_ops =
					&virtio_crypto_modern_ops;
		hw->modern = 1;
		return 0;
	}

	/*
	 * virtio crypto conforms to virtio 1.0 and doesn't support
	 * legacy mode
	 */
	return -1;
}
