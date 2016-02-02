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

#define VIRTIO_PCI_REG_ADDR(hw, reg) \
	(unsigned short)((hw)->io_base + (reg))

#define VIRTIO_READ_REG_1(hw, reg) \
	inb((VIRTIO_PCI_REG_ADDR((hw), (reg))))
#define VIRTIO_WRITE_REG_1(hw, reg, value) \
	outb_p((unsigned char)(value), (VIRTIO_PCI_REG_ADDR((hw), (reg))))

#define VIRTIO_READ_REG_2(hw, reg) \
	inw((VIRTIO_PCI_REG_ADDR((hw), (reg))))
#define VIRTIO_WRITE_REG_2(hw, reg, value) \
	outw_p((unsigned short)(value), (VIRTIO_PCI_REG_ADDR((hw), (reg))))

#define VIRTIO_READ_REG_4(hw, reg) \
	inl((VIRTIO_PCI_REG_ADDR((hw), (reg))))
#define VIRTIO_WRITE_REG_4(hw, reg, value) \
	outl_p((unsigned int)(value), (VIRTIO_PCI_REG_ADDR((hw), (reg))))

static void
legacy_read_dev_config(struct virtio_hw *hw, size_t offset,
		       void *dst, int length)
{
	uint64_t off;
	uint8_t *d;
	int size;

	off = VIRTIO_PCI_CONFIG(hw) + offset;
	for (d = dst; length > 0; d += size, off += size, length -= size) {
		if (length >= 4) {
			size = 4;
			*(uint32_t *)d = VIRTIO_READ_REG_4(hw, off);
		} else if (length >= 2) {
			size = 2;
			*(uint16_t *)d = VIRTIO_READ_REG_2(hw, off);
		} else {
			size = 1;
			*d = VIRTIO_READ_REG_1(hw, off);
		}
	}
}

static void
legacy_write_dev_config(struct virtio_hw *hw, size_t offset,
			const void *src, int length)
{
	uint64_t off;
	const uint8_t *s;
	int size;

	off = VIRTIO_PCI_CONFIG(hw) + offset;
	for (s = src; length > 0; s += size, off += size, length -= size) {
		if (length >= 4) {
			size = 4;
			VIRTIO_WRITE_REG_4(hw, off, *(const uint32_t *)s);
		} else if (length >= 2) {
			size = 2;
			VIRTIO_WRITE_REG_2(hw, off, *(const uint16_t *)s);
		} else {
			size = 1;
			VIRTIO_WRITE_REG_1(hw, off, *s);
		}
	}
}

static uint64_t
legacy_get_features(struct virtio_hw *hw)
{
	return VIRTIO_READ_REG_4(hw, VIRTIO_PCI_HOST_FEATURES);
}

static void
legacy_set_features(struct virtio_hw *hw, uint64_t features)
{
	if ((features >> 32) != 0) {
		PMD_DRV_LOG(ERR,
			"only 32 bit features are allowed for legacy virtio!");
		return;
	}
	VIRTIO_WRITE_REG_4(hw, VIRTIO_PCI_GUEST_FEATURES, features);
}

static uint8_t
legacy_get_status(struct virtio_hw *hw)
{
	return VIRTIO_READ_REG_1(hw, VIRTIO_PCI_STATUS);
}

static void
legacy_set_status(struct virtio_hw *hw, uint8_t status)
{
	VIRTIO_WRITE_REG_1(hw, VIRTIO_PCI_STATUS, status);
}

static void
legacy_reset(struct virtio_hw *hw)
{
	legacy_set_status(hw, VIRTIO_CONFIG_STATUS_RESET);
}

static uint8_t
legacy_get_isr(struct virtio_hw *hw)
{
	return VIRTIO_READ_REG_1(hw, VIRTIO_PCI_ISR);
}

/* Enable one vector (0) for Link State Intrerrupt */
static uint16_t
legacy_set_config_irq(struct virtio_hw *hw, uint16_t vec)
{
	VIRTIO_WRITE_REG_2(hw, VIRTIO_MSI_CONFIG_VECTOR, vec);
	return VIRTIO_READ_REG_2(hw, VIRTIO_MSI_CONFIG_VECTOR);
}

static uint16_t
legacy_get_queue_num(struct virtio_hw *hw, uint16_t queue_id)
{
	VIRTIO_WRITE_REG_2(hw, VIRTIO_PCI_QUEUE_SEL, queue_id);
	return VIRTIO_READ_REG_2(hw, VIRTIO_PCI_QUEUE_NUM);
}

static void
legacy_setup_queue(struct virtio_hw *hw, struct virtqueue *vq)
{
	VIRTIO_WRITE_REG_2(hw, VIRTIO_PCI_QUEUE_SEL, vq->vq_queue_index);

	VIRTIO_WRITE_REG_4(hw, VIRTIO_PCI_QUEUE_PFN,
		vq->mz->phys_addr >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);
}

static void
legacy_del_queue(struct virtio_hw *hw, struct virtqueue *vq)
{
	VIRTIO_WRITE_REG_2(hw, VIRTIO_PCI_QUEUE_SEL, vq->vq_queue_index);

	VIRTIO_WRITE_REG_4(hw, VIRTIO_PCI_QUEUE_PFN, 0);
}

static void
legacy_notify_queue(struct virtio_hw *hw, struct virtqueue *vq)
{
	VIRTIO_WRITE_REG_2(hw, VIRTIO_PCI_QUEUE_NOTIFY, vq->vq_queue_index);
}

#ifdef RTE_EXEC_ENV_LINUXAPP
static int
parse_sysfs_value(const char *filename, unsigned long *val)
{
	FILE *f;
	char buf[BUFSIZ];
	char *end = NULL;

	f = fopen(filename, "r");
	if (f == NULL) {
		PMD_INIT_LOG(ERR, "%s(): cannot open sysfs value %s",
			     __func__, filename);
		return -1;
	}

	if (fgets(buf, sizeof(buf), f) == NULL) {
		PMD_INIT_LOG(ERR, "%s(): cannot read sysfs value %s",
			     __func__, filename);
		fclose(f);
		return -1;
	}
	*val = strtoul(buf, &end, 0);
	if ((buf[0] == '\0') || (end == NULL) || (*end != '\n')) {
		PMD_INIT_LOG(ERR, "%s(): cannot parse sysfs value %s",
			     __func__, filename);
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}

static int
get_uio_dev(struct rte_pci_addr *loc, char *buf, unsigned int buflen,
			unsigned int *uio_num)
{
	struct dirent *e;
	DIR *dir;
	char dirname[PATH_MAX];

	/*
	 * depending on kernel version, uio can be located in uio/uioX
	 * or uio:uioX
	 */
	snprintf(dirname, sizeof(dirname),
		     SYSFS_PCI_DEVICES "/" PCI_PRI_FMT "/uio",
		     loc->domain, loc->bus, loc->devid, loc->function);
	dir = opendir(dirname);
	if (dir == NULL) {
		/* retry with the parent directory */
		snprintf(dirname, sizeof(dirname),
			     SYSFS_PCI_DEVICES "/" PCI_PRI_FMT,
			     loc->domain, loc->bus, loc->devid, loc->function);
		dir = opendir(dirname);

		if (dir == NULL) {
			PMD_INIT_LOG(ERR, "Cannot opendir %s", dirname);
			return -1;
		}
	}

	/* take the first file starting with "uio" */
	while ((e = readdir(dir)) != NULL) {
		/* format could be uio%d ...*/
		int shortprefix_len = sizeof("uio") - 1;
		/* ... or uio:uio%d */
		int longprefix_len = sizeof("uio:uio") - 1;
		char *endptr;

		if (strncmp(e->d_name, "uio", 3) != 0)
			continue;

		/* first try uio%d */
		errno = 0;
		*uio_num = strtoull(e->d_name + shortprefix_len, &endptr, 10);
		if (errno == 0 && endptr != (e->d_name + shortprefix_len)) {
			snprintf(buf, buflen, "%s/uio%u", dirname, *uio_num);
			break;
		}

		/* then try uio:uio%d */
		errno = 0;
		*uio_num = strtoull(e->d_name + longprefix_len, &endptr, 10);
		if (errno == 0 && endptr != (e->d_name + longprefix_len)) {
			snprintf(buf, buflen, "%s/uio:uio%u", dirname,
				     *uio_num);
			break;
		}
	}
	closedir(dir);

	/* No uio resource found */
	if (e == NULL) {
		PMD_INIT_LOG(ERR, "Could not find uio resource");
		return -1;
	}

	return 0;
}

static int
legacy_virtio_has_msix(const struct rte_pci_addr *loc)
{
	DIR *d;
	char dirname[PATH_MAX];

	snprintf(dirname, sizeof(dirname),
		     SYSFS_PCI_DEVICES "/" PCI_PRI_FMT "/msi_irqs",
		     loc->domain, loc->bus, loc->devid, loc->function);

	d = opendir(dirname);
	if (d)
		closedir(d);

	return (d != NULL);
}

/* Extract I/O port numbers from sysfs */
static int
virtio_resource_init_by_uio(struct rte_pci_device *pci_dev)
{
	char dirname[PATH_MAX];
	char filename[PATH_MAX];
	unsigned long start, size;
	unsigned int uio_num;

	if (get_uio_dev(&pci_dev->addr, dirname, sizeof(dirname), &uio_num) < 0)
		return -1;

	/* get portio size */
	snprintf(filename, sizeof(filename),
		     "%s/portio/port0/size", dirname);
	if (parse_sysfs_value(filename, &size) < 0) {
		PMD_INIT_LOG(ERR, "%s(): cannot parse size",
			     __func__);
		return -1;
	}

	/* get portio start */
	snprintf(filename, sizeof(filename),
		 "%s/portio/port0/start", dirname);
	if (parse_sysfs_value(filename, &start) < 0) {
		PMD_INIT_LOG(ERR, "%s(): cannot parse portio start",
			     __func__);
		return -1;
	}
	pci_dev->mem_resource[0].addr = (void *)(uintptr_t)start;
	pci_dev->mem_resource[0].len =  (uint64_t)size;
	PMD_INIT_LOG(DEBUG,
		     "PCI Port IO found start=0x%lx with size=0x%lx",
		     start, size);

	/* save fd */
	memset(dirname, 0, sizeof(dirname));
	snprintf(dirname, sizeof(dirname), "/dev/uio%u", uio_num);
	pci_dev->intr_handle.fd = open(dirname, O_RDWR);
	if (pci_dev->intr_handle.fd < 0) {
		PMD_INIT_LOG(ERR, "Cannot open %s: %s\n",
			dirname, strerror(errno));
		return -1;
	}

	pci_dev->intr_handle.type = RTE_INTR_HANDLE_UIO;
	pci_dev->driver->drv_flags |= RTE_PCI_DRV_INTR_LSC;

	return 0;
}

/* Extract port I/O numbers from proc/ioports */
static int
virtio_resource_init_by_ioports(struct rte_pci_device *pci_dev)
{
	uint16_t start, end;
	int size;
	FILE *fp;
	char *line = NULL;
	char pci_id[16];
	int found = 0;
	size_t linesz;

	snprintf(pci_id, sizeof(pci_id), PCI_PRI_FMT,
		 pci_dev->addr.domain,
		 pci_dev->addr.bus,
		 pci_dev->addr.devid,
		 pci_dev->addr.function);

	fp = fopen("/proc/ioports", "r");
	if (fp == NULL) {
		PMD_INIT_LOG(ERR, "%s(): can't open ioports", __func__);
		return -1;
	}

	while (getdelim(&line, &linesz, '\n', fp) > 0) {
		char *ptr = line;
		char *left;
		int n;

		n = strcspn(ptr, ":");
		ptr[n] = 0;
		left = &ptr[n + 1];

		while (*left && isspace(*left))
			left++;

		if (!strncmp(left, pci_id, strlen(pci_id))) {
			found = 1;

			while (*ptr && isspace(*ptr))
				ptr++;

			sscanf(ptr, "%04hx-%04hx", &start, &end);
			size = end - start + 1;

			break;
		}
	}

	free(line);
	fclose(fp);

	if (!found)
		return -1;

	pci_dev->mem_resource[0].addr = (void *)(uintptr_t)(uint32_t)start;
	pci_dev->mem_resource[0].len =  (uint64_t)size;
	PMD_INIT_LOG(DEBUG,
		"PCI Port IO found start=0x%x with size=0x%x",
		start, size);

	/* can't support lsc interrupt without uio */
	pci_dev->driver->drv_flags &= ~RTE_PCI_DRV_INTR_LSC;

	return 0;
}

/* Extract I/O port numbers from sysfs */
static int
legacy_virtio_resource_init(struct rte_pci_device *pci_dev)
{
	if (virtio_resource_init_by_uio(pci_dev) == 0)
		return 0;
	else
		return virtio_resource_init_by_ioports(pci_dev);
}

#else
static int
legayc_virtio_has_msix(const struct rte_pci_addr *loc __rte_unused)
{
	/* nic_uio does not enable interrupts, return 0 (false). */
	return 0;
}

static int
legacy_virtio_resource_init(struct rte_pci_device *pci_dev __rte_unused)
{
	/* no setup required */
	return 0;
}
#endif

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

static void
modern_setup_queue(struct virtio_hw *hw, struct virtqueue *vq)
{
	uint64_t desc_addr, avail_addr, used_addr;
	uint16_t notify_off;

	desc_addr = vq->mz->phys_addr;
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

	if (rte_eal_pci_map_device(dev) < 0) {
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

int
vtpci_init(struct rte_pci_device *dev, struct virtio_hw *hw)
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
		dev->driver->drv_flags |= RTE_PCI_DRV_INTR_LSC;
		return 0;
	}

	PMD_INIT_LOG(INFO, "trying with legacy virtio pci.");
	if (legacy_virtio_resource_init(dev) < 0)
		return -1;

	hw->vtpci_ops = &legacy_ops;
	hw->use_msix = legacy_virtio_has_msix(&dev->addr);
	hw->io_base  = (uint32_t)(uintptr_t)dev->mem_resource[0].addr;
	hw->modern   = 0;

	return 0;
}
