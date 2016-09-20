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

#ifdef RTE_EXEC_ENV_LINUXAPP
 #include <dirent.h>
 #include <fcntl.h>
#endif

#include "virtio_pci.h"
#include "virtio_logs.h"
#include "virtqueue.h"

/*
 * Following macros are derived from linux/pci_regs.h, however,
 * we can't simply include that header here, as there is no such
 * file for non-Linux platform.
 */
#define PCI_CAPABILITY_LIST	0x34
#define PCI_CAP_ID_VNDR		0x09

/*
 * The remaining space is defined by each driver as the per-driver
 * configuration space.
 */
#define VIRTIO_PCI_CONFIG(hw) (((hw)->use_msix) ? 24 : 20)

static inline int
check_vq_phys_addr_ok(struct virtqueue *vq)
{
	/* Virtio PCI device VIRTIO_PCI_QUEUE_PF register is 32bit,
	 * and only accepts 32 bit page frame number.
	 * Check if the allocated physical memory exceeds 16TB.
	 */
	if ((vq->vq_ring_mem + vq->vq_ring_size - 1) >>
			(VIRTIO_PCI_QUEUE_ADDR_SHIFT + 32)) {
		PMD_INIT_LOG(ERR, "vring address shouldn't be above 16TB!");
		return 0;
	}

	return 1;
}

/*
 * Since we are in legacy mode:
 * http://ozlabs.org/~rusty/virtio-spec/virtio-0.9.5.pdf
 *
 * "Note that this is possible because while the virtio header is PCI (i.e.
 * little) endian, the device-specific region is encoded in the native endian of
 * the guest (where such distinction is applicable)."
 *
 * For powerpc which supports both, qemu supposes that cpu is big endian and
 * enforces this for the virtio-net stuff.
 */
static void
legacy_read_dev_config(struct virtio_hw *hw, size_t offset,
		       void *dst, int length)
{
#ifdef RTE_ARCH_PPC_64
	int size;

	while (length > 0) {
		if (length >= 4) {
			size = 4;
			rte_eal_pci_ioport_read(&hw->io, dst, size,
				VIRTIO_PCI_CONFIG(hw) + offset);
			*(uint32_t *)dst = rte_be_to_cpu_32(*(uint32_t *)dst);
		} else if (length >= 2) {
			size = 2;
			rte_eal_pci_ioport_read(&hw->io, dst, size,
				VIRTIO_PCI_CONFIG(hw) + offset);
			*(uint16_t *)dst = rte_be_to_cpu_16(*(uint16_t *)dst);
		} else {
			size = 1;
			rte_eal_pci_ioport_read(&hw->io, dst, size,
				VIRTIO_PCI_CONFIG(hw) + offset);
		}

		dst = (char *)dst + size;
		offset += size;
		length -= size;
	}
#else
	rte_eal_pci_ioport_read(&hw->io, dst, length,
				VIRTIO_PCI_CONFIG(hw) + offset);
#endif
}

static void
legacy_write_dev_config(struct virtio_hw *hw, size_t offset,
			const void *src, int length)
{
#ifdef RTE_ARCH_PPC_64
	union {
		uint32_t u32;
		uint16_t u16;
	} tmp;
	int size;

	while (length > 0) {
		if (length >= 4) {
			size = 4;
			tmp.u32 = rte_cpu_to_be_32(*(const uint32_t *)src);
			rte_eal_pci_ioport_write(&hw->io, &tmp.u32, size,
				VIRTIO_PCI_CONFIG(hw) + offset);
		} else if (length >= 2) {
			size = 2;
			tmp.u16 = rte_cpu_to_be_16(*(const uint16_t *)src);
			rte_eal_pci_ioport_write(&hw->io, &tmp.u16, size,
				VIRTIO_PCI_CONFIG(hw) + offset);
		} else {
			size = 1;
			rte_eal_pci_ioport_write(&hw->io, src, size,
				VIRTIO_PCI_CONFIG(hw) + offset);
		}

		src = (const char *)src + size;
		offset += size;
		length -= size;
	}
#else
	rte_eal_pci_ioport_write(&hw->io, src, length,
				 VIRTIO_PCI_CONFIG(hw) + offset);
#endif
}

static uint64_t
legacy_get_features(struct virtio_hw *hw)
{
	uint32_t dst;

	rte_eal_pci_ioport_read(&hw->io, &dst, 4, VIRTIO_PCI_HOST_FEATURES);
	return dst;
}

static void
legacy_set_features(struct virtio_hw *hw, uint64_t features)
{
	if ((features >> 32) != 0) {
		PMD_DRV_LOG(ERR,
			"only 32 bit features are allowed for legacy virtio!");
		return;
	}
	rte_eal_pci_ioport_write(&hw->io, &features, 4,
				 VIRTIO_PCI_GUEST_FEATURES);
}

static uint8_t
legacy_get_status(struct virtio_hw *hw)
{
	uint8_t dst;

	rte_eal_pci_ioport_read(&hw->io, &dst, 1, VIRTIO_PCI_STATUS);
	return dst;
}

static void
legacy_set_status(struct virtio_hw *hw, uint8_t status)
{
	rte_eal_pci_ioport_write(&hw->io, &status, 1, VIRTIO_PCI_STATUS);
}

static void
legacy_reset(struct virtio_hw *hw)
{
	legacy_set_status(hw, VIRTIO_CONFIG_STATUS_RESET);
}

static uint8_t
legacy_get_isr(struct virtio_hw *hw)
{
	uint8_t dst;

	rte_eal_pci_ioport_read(&hw->io, &dst, 1, VIRTIO_PCI_ISR);
	return dst;
}

/* Enable one vector (0) for Link State Intrerrupt */
static uint16_t
legacy_set_config_irq(struct virtio_hw *hw, uint16_t vec)
{
	uint16_t dst;

	rte_eal_pci_ioport_write(&hw->io, &vec, 2, VIRTIO_MSI_CONFIG_VECTOR);
	rte_eal_pci_ioport_read(&hw->io, &dst, 2, VIRTIO_MSI_CONFIG_VECTOR);
	return dst;
}

static uint16_t
legacy_get_queue_num(struct virtio_hw *hw, uint16_t queue_id)
{
	uint16_t dst;

	rte_eal_pci_ioport_write(&hw->io, &queue_id, 2, VIRTIO_PCI_QUEUE_SEL);
	rte_eal_pci_ioport_read(&hw->io, &dst, 2, VIRTIO_PCI_QUEUE_NUM);
	return dst;
}

static int
legacy_setup_queue(struct virtio_hw *hw, struct virtqueue *vq)
{
	uint32_t src;

	if (!check_vq_phys_addr_ok(vq))
		return -1;

	rte_eal_pci_ioport_write(&hw->io, &vq->vq_queue_index, 2,
			 VIRTIO_PCI_QUEUE_SEL);
	src = vq->vq_ring_mem >> VIRTIO_PCI_QUEUE_ADDR_SHIFT;
	rte_eal_pci_ioport_write(&hw->io, &src, 4, VIRTIO_PCI_QUEUE_PFN);

	return 0;
}

static void
legacy_del_queue(struct virtio_hw *hw, struct virtqueue *vq)
{
	uint32_t src = 0;

	rte_eal_pci_ioport_write(&hw->io, &vq->vq_queue_index, 2,
			 VIRTIO_PCI_QUEUE_SEL);
	rte_eal_pci_ioport_write(&hw->io, &src, 4, VIRTIO_PCI_QUEUE_PFN);
}

static void
legacy_notify_queue(struct virtio_hw *hw, struct virtqueue *vq)
{
	rte_eal_pci_ioport_write(&hw->io, &vq->vq_queue_index, 2,
			 VIRTIO_PCI_QUEUE_NOTIFY);
}

#ifdef RTE_EXEC_ENV_LINUXAPP
static int
legacy_virtio_has_msix(const struct rte_pci_addr *loc)
{
	DIR *d;
	char dirname[PATH_MAX];

	snprintf(dirname, sizeof(dirname),
		     "%s/" PCI_PRI_FMT "/msi_irqs", pci_get_sysfs_path(),
		     loc->domain, loc->bus, loc->devid, loc->function);

	d = opendir(dirname);
	if (d)
		closedir(d);

	return d != NULL;
}
#else
static int
legacy_virtio_has_msix(const struct rte_pci_addr *loc __rte_unused)
{
	/* nic_uio does not enable interrupts, return 0 (false). */
	return 0;
}
#endif

static int
legacy_virtio_resource_init(struct rte_pci_device *pci_dev,
			    struct virtio_hw *hw, uint32_t *dev_flags)
{
	if (rte_eal_pci_ioport_map(pci_dev, 0, &hw->io) < 0)
		return -1;

	if (pci_dev->intr_handle.type != RTE_INTR_HANDLE_UNKNOWN)
		*dev_flags |= RTE_ETH_DEV_INTR_LSC;
	else
		*dev_flags &= ~RTE_ETH_DEV_INTR_LSC;

	return 0;
}

static const struct virtio_pci_ops legacy_ops = {
	.read_dev_cfg	= legacy_read_dev_config,
	.write_dev_cfg	= legacy_write_dev_config,
	.reset		= legacy_reset,
	.get_status	= legacy_get_status,
	.set_status	= legacy_set_status,
	.get_features	= legacy_get_features,
	.set_features	= legacy_set_features,
	.get_isr	= legacy_get_isr,
	.set_config_irq	= legacy_set_config_irq,
	.get_queue_num	= legacy_get_queue_num,
	.setup_queue	= legacy_setup_queue,
	.del_queue	= legacy_del_queue,
	.notify_queue	= legacy_notify_queue,
};


static inline uint8_t
io_read8(uint8_t *addr)
{
	return *(volatile uint8_t *)addr;
}

static inline void
io_write8(uint8_t val, uint8_t *addr)
{
	*(volatile uint8_t *)addr = val;
}

static inline uint16_t
io_read16(uint16_t *addr)
{
	return *(volatile uint16_t *)addr;
}

static inline void
io_write16(uint16_t val, uint16_t *addr)
{
	*(volatile uint16_t *)addr = val;
}

static inline uint32_t
io_read32(uint32_t *addr)
{
	return *(volatile uint32_t *)addr;
}

static inline void
io_write32(uint32_t val, uint32_t *addr)
{
	*(volatile uint32_t *)addr = val;
}

static inline void
io_write64_twopart(uint64_t val, uint32_t *lo, uint32_t *hi)
{
	io_write32(val & ((1ULL << 32) - 1), lo);
	io_write32(val >> 32,		     hi);
}

static void
modern_read_dev_config(struct virtio_hw *hw, size_t offset,
		       void *dst, int length)
{
	int i;
	uint8_t *p;
	uint8_t old_gen, new_gen;

	do {
		old_gen = io_read8(&hw->common_cfg->config_generation);

		p = dst;
		for (i = 0;  i < length; i++)
			*p++ = io_read8((uint8_t *)hw->dev_cfg + offset + i);

		new_gen = io_read8(&hw->common_cfg->config_generation);
	} while (old_gen != new_gen);
}

static void
modern_write_dev_config(struct virtio_hw *hw, size_t offset,
			const void *src, int length)
{
	int i;
	const uint8_t *p = src;

	for (i = 0;  i < length; i++)
		io_write8(*p++, (uint8_t *)hw->dev_cfg + offset + i);
}

static uint64_t
modern_get_features(struct virtio_hw *hw)
{
	uint32_t features_lo, features_hi;

	io_write32(0, &hw->common_cfg->device_feature_select);
	features_lo = io_read32(&hw->common_cfg->device_feature);

	io_write32(1, &hw->common_cfg->device_feature_select);
	features_hi = io_read32(&hw->common_cfg->device_feature);

	return ((uint64_t)features_hi << 32) | features_lo;
}

static void
modern_set_features(struct virtio_hw *hw, uint64_t features)
{
	io_write32(0, &hw->common_cfg->guest_feature_select);
	io_write32(features & ((1ULL << 32) - 1),
		&hw->common_cfg->guest_feature);

	io_write32(1, &hw->common_cfg->guest_feature_select);
	io_write32(features >> 32,
		&hw->common_cfg->guest_feature);
}

static uint8_t
modern_get_status(struct virtio_hw *hw)
{
	return io_read8(&hw->common_cfg->device_status);
}

static void
modern_set_status(struct virtio_hw *hw, uint8_t status)
{
	io_write8(status, &hw->common_cfg->device_status);
}

static void
modern_reset(struct virtio_hw *hw)
{
	modern_set_status(hw, VIRTIO_CONFIG_STATUS_RESET);
	modern_get_status(hw);
}

static uint8_t
modern_get_isr(struct virtio_hw *hw)
{
	return io_read8(hw->isr);
}

static uint16_t
modern_set_config_irq(struct virtio_hw *hw, uint16_t vec)
{
	io_write16(vec, &hw->common_cfg->msix_config);
	return io_read16(&hw->common_cfg->msix_config);
}

static uint16_t
modern_get_queue_num(struct virtio_hw *hw, uint16_t queue_id)
{
	io_write16(queue_id, &hw->common_cfg->queue_select);
	return io_read16(&hw->common_cfg->queue_size);
}

static int
modern_setup_queue(struct virtio_hw *hw, struct virtqueue *vq)
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

	io_write16(vq->vq_queue_index, &hw->common_cfg->queue_select);

	io_write64_twopart(desc_addr, &hw->common_cfg->queue_desc_lo,
				      &hw->common_cfg->queue_desc_hi);
	io_write64_twopart(avail_addr, &hw->common_cfg->queue_avail_lo,
				       &hw->common_cfg->queue_avail_hi);
	io_write64_twopart(used_addr, &hw->common_cfg->queue_used_lo,
				      &hw->common_cfg->queue_used_hi);

	notify_off = io_read16(&hw->common_cfg->queue_notify_off);
	vq->notify_addr = (void *)((uint8_t *)hw->notify_base +
				notify_off * hw->notify_off_multiplier);

	io_write16(1, &hw->common_cfg->queue_enable);

	PMD_INIT_LOG(DEBUG, "queue %u addresses:", vq->vq_queue_index);
	PMD_INIT_LOG(DEBUG, "\t desc_addr: %" PRIx64, desc_addr);
	PMD_INIT_LOG(DEBUG, "\t aval_addr: %" PRIx64, avail_addr);
	PMD_INIT_LOG(DEBUG, "\t used_addr: %" PRIx64, used_addr);
	PMD_INIT_LOG(DEBUG, "\t notify addr: %p (notify offset: %u)",
		vq->notify_addr, notify_off);

	return 0;
}

static void
modern_del_queue(struct virtio_hw *hw, struct virtqueue *vq)
{
	io_write16(vq->vq_queue_index, &hw->common_cfg->queue_select);

	io_write64_twopart(0, &hw->common_cfg->queue_desc_lo,
				  &hw->common_cfg->queue_desc_hi);
	io_write64_twopart(0, &hw->common_cfg->queue_avail_lo,
				  &hw->common_cfg->queue_avail_hi);
	io_write64_twopart(0, &hw->common_cfg->queue_used_lo,
				  &hw->common_cfg->queue_used_hi);

	io_write16(0, &hw->common_cfg->queue_enable);
}

static void
modern_notify_queue(struct virtio_hw *hw __rte_unused, struct virtqueue *vq)
{
	io_write16(1, vq->notify_addr);
}

static const struct virtio_pci_ops modern_ops = {
	.read_dev_cfg	= modern_read_dev_config,
	.write_dev_cfg	= modern_write_dev_config,
	.reset		= modern_reset,
	.get_status	= modern_get_status,
	.set_status	= modern_set_status,
	.get_features	= modern_get_features,
	.set_features	= modern_set_features,
	.get_isr	= modern_get_isr,
	.set_config_irq	= modern_set_config_irq,
	.get_queue_num	= modern_get_queue_num,
	.setup_queue	= modern_setup_queue,
	.del_queue	= modern_del_queue,
	.notify_queue	= modern_notify_queue,
};


void
vtpci_read_dev_config(struct virtio_hw *hw, size_t offset,
		      void *dst, int length)
{
	hw->vtpci_ops->read_dev_cfg(hw, offset, dst, length);
}

void
vtpci_write_dev_config(struct virtio_hw *hw, size_t offset,
		       const void *src, int length)
{
	hw->vtpci_ops->write_dev_cfg(hw, offset, src, length);
}

uint64_t
vtpci_negotiate_features(struct virtio_hw *hw, uint64_t host_features)
{
	uint64_t features;

	/*
	 * Limit negotiated features to what the driver, virtqueue, and
	 * host all support.
	 */
	features = host_features & hw->guest_features;
	hw->vtpci_ops->set_features(hw, features);

	return features;
}

void
vtpci_reset(struct virtio_hw *hw)
{
	hw->vtpci_ops->set_status(hw, VIRTIO_CONFIG_STATUS_RESET);
	/* flush status write */
	hw->vtpci_ops->get_status(hw);
}

void
vtpci_reinit_complete(struct virtio_hw *hw)
{
	vtpci_set_status(hw, VIRTIO_CONFIG_STATUS_DRIVER_OK);
}

void
vtpci_set_status(struct virtio_hw *hw, uint8_t status)
{
	if (status != VIRTIO_CONFIG_STATUS_RESET)
		status |= hw->vtpci_ops->get_status(hw);

	hw->vtpci_ops->set_status(hw, status);
}

uint8_t
vtpci_get_status(struct virtio_hw *hw)
{
	return hw->vtpci_ops->get_status(hw);
}

uint8_t
vtpci_isr(struct virtio_hw *hw)
{
	return hw->vtpci_ops->get_isr(hw);
}


/* Enable one vector (0) for Link State Intrerrupt */
uint16_t
vtpci_irq_config(struct virtio_hw *hw, uint16_t vec)
{
	return hw->vtpci_ops->set_config_irq(hw, vec);
}

static void *
get_cfg_addr(struct rte_pci_device *dev, struct virtio_pci_cap *cap)
{
	uint8_t  bar    = cap->bar;
	uint32_t length = cap->length;
	uint32_t offset = cap->offset;
	uint8_t *base;

	if (bar > 5) {
		PMD_INIT_LOG(ERR, "invalid bar: %u", bar);
		return NULL;
	}

	if (offset + length < offset) {
		PMD_INIT_LOG(ERR, "offset(%u) + length(%u) overflows",
			offset, length);
		return NULL;
	}

	if (offset + length > dev->mem_resource[bar].len) {
		PMD_INIT_LOG(ERR,
			"invalid cap: overflows bar space: %u > %" PRIu64,
			offset + length, dev->mem_resource[bar].len);
		return NULL;
	}

	base = dev->mem_resource[bar].addr;
	if (base == NULL) {
		PMD_INIT_LOG(ERR, "bar %u base addr is NULL", bar);
		return NULL;
	}

	return base + offset;
}

static int
virtio_read_caps(struct rte_pci_device *dev, struct virtio_hw *hw)
{
	uint8_t pos;
	struct virtio_pci_cap cap;
	int ret;

	if (rte_eal_pci_map_device(dev)) {
		PMD_INIT_LOG(DEBUG, "failed to map pci device!");
		return -1;
	}

	ret = rte_eal_pci_read_config(dev, &pos, 1, PCI_CAPABILITY_LIST);
	if (ret < 0) {
		PMD_INIT_LOG(DEBUG, "failed to read pci capability list");
		return -1;
	}

	while (pos) {
		ret = rte_eal_pci_read_config(dev, &cap, sizeof(cap), pos);
		if (ret < 0) {
			PMD_INIT_LOG(ERR,
				"failed to read pci cap at pos: %x", pos);
			break;
		}

		if (cap.cap_vndr != PCI_CAP_ID_VNDR) {
			PMD_INIT_LOG(DEBUG,
				"[%2x] skipping non VNDR cap id: %02x",
				pos, cap.cap_vndr);
			goto next;
		}

		PMD_INIT_LOG(DEBUG,
			"[%2x] cfg type: %u, bar: %u, offset: %04x, len: %u",
			pos, cap.cfg_type, cap.bar, cap.offset, cap.length);

		switch (cap.cfg_type) {
		case VIRTIO_PCI_CAP_COMMON_CFG:
			hw->common_cfg = get_cfg_addr(dev, &cap);
			break;
		case VIRTIO_PCI_CAP_NOTIFY_CFG:
			rte_eal_pci_read_config(dev, &hw->notify_off_multiplier,
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
		PMD_INIT_LOG(INFO, "no modern virtio pci device found.");
		return -1;
	}

	PMD_INIT_LOG(INFO, "found modern virtio pci device.");

	PMD_INIT_LOG(DEBUG, "common cfg mapped at: %p", hw->common_cfg);
	PMD_INIT_LOG(DEBUG, "device cfg mapped at: %p", hw->dev_cfg);
	PMD_INIT_LOG(DEBUG, "isr cfg mapped at: %p", hw->isr);
	PMD_INIT_LOG(DEBUG, "notify base: %p, notify off multiplier: %u",
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
vtpci_init(struct rte_pci_device *dev, struct virtio_hw *hw,
	   uint32_t *dev_flags)
{
	hw->dev = dev;

	/*
	 * Try if we can succeed reading virtio pci caps, which exists
	 * only on modern pci device. If failed, we fallback to legacy
	 * virtio handling.
	 */
	if (virtio_read_caps(dev, hw) == 0) {
		PMD_INIT_LOG(INFO, "modern virtio pci detected.");
		hw->vtpci_ops = &modern_ops;
		hw->modern    = 1;
		*dev_flags |= RTE_ETH_DEV_INTR_LSC;
		return 0;
	}

	PMD_INIT_LOG(INFO, "trying with legacy virtio pci.");
	if (legacy_virtio_resource_init(dev, hw, dev_flags) < 0) {
		if (dev->kdrv == RTE_KDRV_UNKNOWN &&
		    (!dev->device.devargs ||
		     dev->device.devargs->type !=
			RTE_DEVTYPE_WHITELISTED_PCI)) {
			PMD_INIT_LOG(INFO,
				"skip kernel managed virtio device.");
			return 1;
		}
		return -1;
	}

	hw->vtpci_ops = &legacy_ops;
	hw->use_msix = legacy_virtio_has_msix(&dev->addr);
	hw->modern   = 0;

	return 0;
}
