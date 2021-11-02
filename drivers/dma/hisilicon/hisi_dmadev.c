/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 HiSilicon Limited
 */

#include <inttypes.h>
#include <string.h>

#include <rte_bus_pci.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_io.h>
#include <rte_log.h>
#include <rte_pci.h>
#include <rte_dmadev_pmd.h>

#include "hisi_dmadev.h"

RTE_LOG_REGISTER_DEFAULT(hisi_dma_logtype, INFO);
#define HISI_DMA_LOG(level, fmt, args...) \
		rte_log(RTE_LOG_ ## level, hisi_dma_logtype, \
		"%s(): " fmt "\n", __func__, ##args)
#define HISI_DMA_LOG_RAW(hw, level, fmt, args...) \
		rte_log(RTE_LOG_ ## level, hisi_dma_logtype, \
		"%s %s(): " fmt "\n", (hw)->data->dev_name, \
		__func__, ##args)
#define HISI_DMA_DEBUG(hw, fmt, args...) \
		HISI_DMA_LOG_RAW(hw, DEBUG, fmt, ## args)
#define HISI_DMA_INFO(hw, fmt, args...) \
		HISI_DMA_LOG_RAW(hw, INFO, fmt, ## args)
#define HISI_DMA_WARN(hw, fmt, args...) \
		HISI_DMA_LOG_RAW(hw, WARNING, fmt, ## args)
#define HISI_DMA_ERR(hw, fmt, args...) \
		HISI_DMA_LOG_RAW(hw, ERR, fmt, ## args)

static uint32_t
hisi_dma_queue_base(struct hisi_dma_dev *hw)
{
	if (hw->reg_layout == HISI_DMA_REG_LAYOUT_HIP08)
		return HISI_DMA_HIP08_QUEUE_BASE;
	else
		return 0;
}

static void
hisi_dma_write_reg(void *base, uint32_t off, uint32_t val)
{
	rte_write32(rte_cpu_to_le_32(val),
		    (volatile void *)((char *)base + off));
}

static void
hisi_dma_write_dev(struct hisi_dma_dev *hw, uint32_t off, uint32_t val)
{
	hisi_dma_write_reg(hw->io_base, off, val);
}

static void
hisi_dma_write_queue(struct hisi_dma_dev *hw, uint32_t qoff, uint32_t val)
{
	uint32_t off = hisi_dma_queue_base(hw) +
			hw->queue_id * HISI_DMA_QUEUE_REGION_SIZE + qoff;
	hisi_dma_write_dev(hw, off, val);
}

static uint32_t
hisi_dma_read_reg(void *base, uint32_t off)
{
	uint32_t val = rte_read32((volatile void *)((char *)base + off));
	return rte_le_to_cpu_32(val);
}

static uint32_t
hisi_dma_read_dev(struct hisi_dma_dev *hw, uint32_t off)
{
	return hisi_dma_read_reg(hw->io_base, off);
}

static uint32_t
hisi_dma_read_queue(struct hisi_dma_dev *hw, uint32_t qoff)
{
	uint32_t off = hisi_dma_queue_base(hw) +
			hw->queue_id * HISI_DMA_QUEUE_REGION_SIZE + qoff;
	return hisi_dma_read_dev(hw, off);
}

static void
hisi_dma_update_bit(struct hisi_dma_dev *hw, uint32_t off, uint32_t pos,
		    bool set)
{
	uint32_t tmp = hisi_dma_read_dev(hw, off);
	uint32_t mask = 1u << pos;
	tmp = set ? tmp | mask : tmp & ~mask;
	hisi_dma_write_dev(hw, off, tmp);
}

static void
hisi_dma_update_queue_bit(struct hisi_dma_dev *hw, uint32_t qoff, uint32_t pos,
			  bool set)
{
	uint32_t tmp = hisi_dma_read_queue(hw, qoff);
	uint32_t mask = 1u << pos;
	tmp = set ? tmp | mask : tmp & ~mask;
	hisi_dma_write_queue(hw, qoff, tmp);
}

#define hisi_dma_poll_hw_state(hw, val, cond, sleep_us, timeout_us) ({ \
	uint32_t timeout = 0; \
	while (timeout++ <= (timeout_us)) { \
		(val) = hisi_dma_read_queue(hw, HISI_DMA_QUEUE_FSM_REG); \
		if (cond) \
			break; \
		rte_delay_us(sleep_us); \
	} \
	(cond) ? 0 : -ETIME; \
})

static int
hisi_dma_reset_hw(struct hisi_dma_dev *hw)
{
#define POLL_SLEEP_US	100
#define POLL_TIMEOUT_US	10000

	uint32_t tmp;
	int ret;

	hisi_dma_update_queue_bit(hw, HISI_DMA_QUEUE_CTRL0_REG,
				  HISI_DMA_QUEUE_CTRL0_PAUSE_B, true);
	hisi_dma_update_queue_bit(hw, HISI_DMA_QUEUE_CTRL0_REG,
				  HISI_DMA_QUEUE_CTRL0_EN_B, false);

	ret = hisi_dma_poll_hw_state(hw, tmp,
		FIELD_GET(HISI_DMA_QUEUE_FSM_STS_M, tmp) != HISI_DMA_STATE_RUN,
		POLL_SLEEP_US, POLL_TIMEOUT_US);
	if (ret) {
		HISI_DMA_ERR(hw, "disable dma timeout!");
		return ret;
	}

	hisi_dma_update_queue_bit(hw, HISI_DMA_QUEUE_CTRL1_REG,
				  HISI_DMA_QUEUE_CTRL1_RESET_B, true);
	hisi_dma_write_queue(hw, HISI_DMA_QUEUE_SQ_TAIL_REG, 0);
	hisi_dma_write_queue(hw, HISI_DMA_QUEUE_CQ_HEAD_REG, 0);
	hisi_dma_update_queue_bit(hw, HISI_DMA_QUEUE_CTRL0_REG,
				  HISI_DMA_QUEUE_CTRL0_PAUSE_B, false);

	ret = hisi_dma_poll_hw_state(hw, tmp,
		FIELD_GET(HISI_DMA_QUEUE_FSM_STS_M, tmp) == HISI_DMA_STATE_IDLE,
		POLL_SLEEP_US, POLL_TIMEOUT_US);
	if (ret) {
		HISI_DMA_ERR(hw, "reset dma timeout!");
		return ret;
	}

	return 0;
}

static void
hisi_dma_init_gbl(void *pci_bar, uint8_t revision)
{
	struct hisi_dma_dev hw;

	memset(&hw, 0, sizeof(hw));
	hw.io_base = pci_bar;

	if (revision == HISI_DMA_REVISION_HIP08B)
		hisi_dma_update_bit(&hw, HISI_DMA_HIP08_MODE_REG,
				    HISI_DMA_HIP08_MODE_SEL_B, true);
}

static uint8_t
hisi_dma_reg_layout(uint8_t revision)
{
	if (revision == HISI_DMA_REVISION_HIP08B)
		return HISI_DMA_REG_LAYOUT_HIP08;
	else
		return HISI_DMA_REG_LAYOUT_INVALID;
}

static void
hisi_dma_gen_pci_device_name(const struct rte_pci_device *pci_dev,
			     char *name, size_t size)
{
	memset(name, 0, size);
	(void)snprintf(name, size, "%x:%x.%x",
		 pci_dev->addr.bus, pci_dev->addr.devid,
		 pci_dev->addr.function);
}

static void
hisi_dma_gen_dev_name(const struct rte_pci_device *pci_dev,
		      uint8_t queue_id, char *name, size_t size)
{
	memset(name, 0, size);
	(void)snprintf(name, size, "%x:%x.%x-ch%u",
		 pci_dev->addr.bus, pci_dev->addr.devid,
		 pci_dev->addr.function, queue_id);
}

static int
hisi_dma_create(struct rte_pci_device *pci_dev, uint8_t queue_id,
		uint8_t revision)
{
#define REG_PCI_BAR_INDEX	2

	char name[RTE_DEV_NAME_MAX_LEN];
	struct rte_dma_dev *dev;
	struct hisi_dma_dev *hw;
	int ret;

	hisi_dma_gen_dev_name(pci_dev, queue_id, name, sizeof(name));
	dev = rte_dma_pmd_allocate(name, pci_dev->device.numa_node,
				   sizeof(*hw));
	if (dev == NULL) {
		HISI_DMA_LOG(ERR, "%s allocate dmadev fail!", name);
		return -EINVAL;
	}

	dev->device = &pci_dev->device;

	hw = dev->data->dev_private;
	hw->data = dev->data;
	hw->revision = revision;
	hw->reg_layout = hisi_dma_reg_layout(revision);
	hw->io_base = pci_dev->mem_resource[REG_PCI_BAR_INDEX].addr;
	hw->queue_id = queue_id;

	ret = hisi_dma_reset_hw(hw);
	if (ret) {
		HISI_DMA_LOG(ERR, "%s init device fail!", name);
		(void)rte_dma_pmd_release(name);
		return -EIO;
	}

	dev->state = RTE_DMA_DEV_READY;
	HISI_DMA_LOG(DEBUG, "%s create dmadev success!", name);

	return 0;
}

static int
hisi_dma_check_revision(struct rte_pci_device *pci_dev, const char *name,
			uint8_t *out_revision)
{
	uint8_t revision;
	int ret;

	ret = rte_pci_read_config(pci_dev, &revision, 1,
				  HISI_DMA_PCI_REVISION_ID_REG);
	if (ret != 1) {
		HISI_DMA_LOG(ERR, "%s read PCI revision failed!", name);
		return -EINVAL;
	}
	if (hisi_dma_reg_layout(revision) == HISI_DMA_REG_LAYOUT_INVALID) {
		HISI_DMA_LOG(ERR, "%s revision: 0x%x not supported!",
			     name, revision);
		return -EINVAL;
	}

	*out_revision = revision;
	return 0;
}

static int
hisi_dma_probe(struct rte_pci_driver *pci_drv __rte_unused,
	       struct rte_pci_device *pci_dev)
{
	char name[RTE_DEV_NAME_MAX_LEN] = { 0 };
	uint8_t revision;
	uint8_t i;
	int ret;

	hisi_dma_gen_pci_device_name(pci_dev, name, sizeof(name));

	if (pci_dev->mem_resource[2].addr == NULL) {
		HISI_DMA_LOG(ERR, "%s BAR2 is NULL!\n", name);
		return -ENODEV;
	}

	ret = hisi_dma_check_revision(pci_dev, name, &revision);
	if (ret)
		return ret;
	HISI_DMA_LOG(DEBUG, "%s read PCI revision: 0x%x", name, revision);

	hisi_dma_init_gbl(pci_dev->mem_resource[2].addr, revision);

	for (i = 0; i < HISI_DMA_MAX_HW_QUEUES; i++) {
		ret = hisi_dma_create(pci_dev, i, revision);
		if (ret) {
			HISI_DMA_LOG(ERR, "%s create dmadev %u failed!",
				     name, i);
			break;
		}
	}

	return ret;
}

static int
hisi_dma_remove(struct rte_pci_device *pci_dev)
{
	char name[RTE_DEV_NAME_MAX_LEN];
	uint8_t i;
	int ret;

	for (i = 0; i < HISI_DMA_MAX_HW_QUEUES; i++) {
		hisi_dma_gen_dev_name(pci_dev, i, name, sizeof(name));
		ret = rte_dma_pmd_release(name);
		if (ret)
			return ret;
	}

	return 0;
}

static const struct rte_pci_id pci_id_hisi_dma_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HISI_DMA_DEVICE_ID) },
	{ .vendor_id = 0, }, /* sentinel */
};

static struct rte_pci_driver hisi_dma_pmd_drv = {
	.id_table  = pci_id_hisi_dma_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe     = hisi_dma_probe,
	.remove    = hisi_dma_remove,
};

RTE_PMD_REGISTER_PCI(dma_hisilicon, hisi_dma_pmd_drv);
RTE_PMD_REGISTER_PCI_TABLE(dma_hisilicon, pci_id_hisi_dma_map);
RTE_PMD_REGISTER_KMOD_DEP(dma_hisilicon, "vfio-pci");
