/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2026 NXP
 */

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bus_platform_driver.h>
#include <rte_bitops.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_dmadev_pmd.h>
#include <rte_malloc.h>

#include "imx_edma5_dmadev.h"

#include "imx_edma5_logs.h"

RTE_LOG_REGISTER_DEFAULT(imx_edma5_logtype, INFO);

/* Compatible string for i.MX95 eDMA5 device tree nodes. */
#define IMX_EDMA5_COMPAT		"fsl,imx95-edma5"

/* sysfs base path for platform devices' device-tree nodes. */
#define IMX_EDMA5_SYSFS_DEVICES		"/sys/bus/platform/devices"

/*
 * Read the device-tree "dma-channel-mask" property and return the combined
 * 64-bit mask of reserved hardware channels. The property is a raw binary file
 * of big-endian 32-bit cells: cell[0] covers channels 0-31, cell[1] channels
 * 32-63. A set bit marks a channel owned by another bus master (e.g. SCMI
 * firmware) that must not be accessed. Returns 0 if the property is absent.
 */
static uint64_t
imx_edma5_read_channel_mask(const char *dev_name)
{
	char path[PATH_MAX];
	uint32_t cells[2] = { 0, 0 };
	uint64_t mask = 0;
	size_t n;
	FILE *f;

	snprintf(path, sizeof(path), IMX_EDMA5_SYSFS_DEVICES "/%s/of_node/dma-channel-mask",
		dev_name);

	f = fopen(path, "rb");
	if (f == NULL)
		return 0;

	n = fread(cells, 1, sizeof(cells), f);
	fclose(f);

	/* Device-tree cells are big-endian regardless of CPU endianness. */
	if (n >= sizeof(uint32_t))
		mask |= rte_be_to_cpu_32(cells[0]);
	if (n >= 2 * sizeof(uint32_t))
		mask |= (uint64_t)rte_be_to_cpu_32(cells[1]) << 32;

	return mask;
}

static int
imx_edma5_info_get(const struct rte_dma_dev *dev, struct rte_dma_info *dev_info, uint32_t info_sz)
{
	const struct imx_edma5_dev *ed = dev->data->dev_private;

	RTE_SET_USED(info_sz);

	dev_info->dev_capa = RTE_DMA_CAPA_MEM_TO_MEM |
		RTE_DMA_CAPA_OPS_COPY |
		RTE_DMA_CAPA_OPS_COPY_SG;
	dev_info->max_vchans = ed->max_vchans;
	dev_info->max_desc = IMX_EDMA5_MAX_DESC;
	dev_info->min_desc = IMX_EDMA5_MIN_DESC;
	dev_info->max_sges = IMX_EDMA5_MAX_SGES;

	return 0;
}

static const struct rte_dma_dev_ops imx_edma5_ops = {
	.dev_info_get	= imx_edma5_info_get,
};

static int
imx_edma5_probe(struct rte_platform_device *pdev)
{
	struct rte_platform_resource *res;
	struct imx_edma5_dev *ed;
	struct rte_dma_dev *dev;
	const char *name;
	uint32_t mp_csr;
	uint16_t hw;

	name = pdev->name;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		IMX_EDMA5_LOG(ERR, "Secondary process not supported for %s", name);
		return -ENOTSUP;
	}

	if (pdev->num_resource < 1 || pdev->resource == NULL) {
		IMX_EDMA5_LOG(ERR, "No MMIO resource for %s", name);
		return -EINVAL;
	}
	res = &pdev->resource[0];
	if (res->mem.addr == NULL) {
		IMX_EDMA5_LOG(ERR, "MMIO resource not mapped for %s", name);
		return -EINVAL;
	}

	dev = rte_dma_pmd_allocate(name, rte_socket_id(), sizeof(struct imx_edma5_dev));
	if (dev == NULL) {
		IMX_EDMA5_LOG(ERR, "Failed to allocate dmadev for %s", name);
		return -ENOMEM;
	}

	dev->device = &pdev->device;
	dev->dev_ops = &imx_edma5_ops;

	ed = dev->data->dev_private;
	ed->reg_base = res->mem.addr;
	ed->reg_size = res->mem.len;
	ed->dev_id = dev->data->dev_id;

	/*
	 * Build a map from usable vchan index to hardware channel index,
	 * skipping channels reserved for other bus masters by the device-tree
	 * "dma-channel-mask" (accessing them external-aborts).
	 */
	ed->masked_channels = imx_edma5_read_channel_mask(name);
	ed->nb_channels = 0;
	for (hw = 0; hw < IMX_EDMA5_MAX_CHANNELS; hw++) {
		if (ed->masked_channels & (RTE_BIT64(hw)))
			continue;
		ed->chan_map[ed->nb_channels++] = hw;
	}
	ed->max_vchans = ed->nb_channels;

	if (ed->nb_channels == 0) {
		IMX_EDMA5_LOG(ERR, "No usable eDMA5 channels for %s (mask 0x%" PRIx64 ")",
			name, ed->masked_channels);
		rte_dma_pmd_release(name);
		return -ENODEV;
	}

	dev->state = RTE_DMA_DEV_READY;

	/*
	 * Set MP_CSR.GCLC (Global Clock Control) before any per-channel register
	 * is touched: the per-channel windows are individually clock-gated and
	 * external-abort when accessed with GCLC clear. Also enable round-robin
	 * arbitration (ERCA). Use read-modify-write to preserve reset defaults.
	 */
	mp_csr = imx_edma5_read32(ed->reg_base, IMX_EDMA5_MP_CSR);
	mp_csr |= IMX_EDMA5_MP_CSR_GCLC | IMX_EDMA5_MP_CSR_ERCA;
	imx_edma5_write32(ed->reg_base, IMX_EDMA5_MP_CSR, mp_csr);

	IMX_EDMA5_LOG(INFO, "Probed i.MX95 eDMA5 dmadev %s (%u channels)", name, ed->nb_channels);

	return 0;
}

static int
imx_edma5_remove(struct rte_platform_device *pdev)
{
	const char *name = pdev->name;

	return rte_dma_pmd_release(name);
}

static struct rte_platform_driver imx_edma5_pmd_drv = {
	/*
	 * Set the DT compatible string as the driver alias. The platform bus
	 * match logic compares this against the device-tree "compatible" sysfs
	 * strings of each platform device when the kernel driver name (always
	 * "vfio-platform") does not uniquely identify the device.
	 */
	.driver = {
		.alias = IMX_EDMA5_COMPAT,
	},
	.probe = imx_edma5_probe,
	.remove = imx_edma5_remove,
	/*
	 * The eDMA5 is programmed with the IOVA of the buffers, so it works in
	 * both IOVA=VA and IOVA=PA modes; no IOVA-as-VA requirement is forced.
	 */
	.drv_flags = 0,
};

RTE_PMD_REGISTER_PLATFORM(dma_imx_edma5, imx_edma5_pmd_drv);
